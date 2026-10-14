#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Deployment integration checks use local bare remotes only; never GitHub."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import io

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("publish_jam_web", ROOT / "tools/publish_jam_web.py")
publish = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = publish
spec.loader.exec_module(publish)
IDENTITY = ("Local deployment fixture", "deployment-test@example.invalid")


def git(directory: Path, *arguments: str) -> str:
    result = subprocess.run(["git", "-c", "core.hooksPath=/dev/null", *arguments], cwd=directory,
                            check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout.strip()


def hashes(files):
    files["SHA256SUMS"] = "".join(f"{publish.sha(data)}  {name}\n" for name, data in sorted(files.items())
                                      if name != "SHA256SUMS").encode()


def write_site(site, files):
    for name, body in files.items():
        path = site / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(body)


class DeploymentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.payload = publish.validate_site(ROOT / "build/jambox-web")

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="jambox-publish-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.remote = self.directory / "remote.git"
        self.remote.mkdir()
        git(self.remote, "init", "--bare", "--quiet", "--template=")

    def prepare(self, files=None):
        return publish.prepare_deployment(files or self.payload, str(self.remote), IDENTITY, self.directory / "staging")

    def tip(self):
        result = subprocess.run(["git", "rev-parse", "--verify", publish.BRANCH], cwd=self.remote,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result.stdout.strip() if result.returncode == 0 else None

    def changed_payload(self, marker):
        files = dict(self.payload)
        files["WEB_EMULATOR.md"] += f"\nLocal fixture: {marker}\n".encode()
        hashes(files)
        return files

    def test_initial_dry_run_publish_update_and_noop(self):
        with self.prepare() as staged:
            report = staged.finish()
            self.assertEqual(report["action"], "dry-run")
            self.assertIsNone(self.tip(), "dry-run must not create a remote branch")
            self.assertEqual(git(staged.directory, "ls-tree", "-r", "--name-only", "HEAD").splitlines(),
                             sorted(publish.EXPECTED_FILES))
            self.assertEqual(staged.finish(publish=True)["action"], "published")
            first = staged.commit
        self.assertEqual(self.tip(), first)
        self.assertEqual(git(self.remote, "rev-list", "--count", publish.BRANCH), "1")
        self.assertEqual(git(self.remote, "for-each-ref", "--format=%(refname)"), publish.BRANCH)

        # Another existing source branch must survive the deployment unchanged.
        git(self.remote, "update-ref", "refs/heads/main", first)
        with self.prepare(self.changed_payload("second")) as staged:
            self.assertEqual(staged.parent, first)
            self.assertEqual(staged.finish()["action"], "dry-run")
            self.assertEqual(self.tip(), first)
            self.assertEqual(staged.finish(publish=True)["action"], "published")
            second = staged.commit
        self.assertEqual(git(self.remote, "rev-parse", f"{second}^"), first)
        self.assertEqual(git(self.remote, "rev-list", "--count", publish.BRANCH), "2")
        self.assertEqual(git(self.remote, "rev-parse", "refs/heads/main"), first)
        with self.prepare(self.changed_payload("second")) as staged:
            self.assertFalse(staged.changed)
            self.assertEqual(staged.finish(publish=True)["action"], "unchanged")
            self.assertEqual(staged.commit, second)
        self.assertEqual(git(self.remote, "rev-list", "--count", publish.BRANCH), "2")
        self.assertEqual(git(self.remote, "ls-tree", "-r", "--name-only", publish.BRANCH).splitlines(),
                         sorted(publish.EXPECTED_FILES))

    def test_concurrent_update_is_rejected_and_preserved(self):
        with self.prepare() as initial:
            initial.finish(publish=True)
            first = initial.commit
        with self.prepare(self.changed_payload("stale")) as stale:
            with self.prepare(self.changed_payload("winner")) as winner:
                winner.finish(publish=True)
                winning_tip = winner.commit
            with self.assertRaisesRegex(publish.DeploymentError, "changed during preparation"):
                stale.finish(publish=True)
            # The actual ordinary Git push also rejects the divergent commit.
            with self.assertRaises(publish.DeploymentError):
                publish.git(stale.directory, "push", "--porcelain", "origin", f"HEAD:{publish.BRANCH}")
        self.assertEqual(self.tip(), winning_tip)
        self.assertEqual(git(self.remote, "rev-parse", f"{winning_tip}^"), first)
        self.assertEqual(git(self.remote, "rev-list", "--count", publish.BRANCH), "2")

    def test_inherited_git_paths_cannot_redirect_staging(self):
        trap = self.directory / "trap"
        trap.mkdir()
        git(trap, "init", "--quiet", "--template=")
        before = git(trap, "status", "--porcelain")
        overrides = {"GIT_DIR": str(trap / ".git"), "GIT_WORK_TREE": str(trap),
                     "GIT_INDEX_FILE": str(trap / "unexpected-index")}
        with patch.dict(os.environ, overrides):
            with self.prepare() as staged:
                self.assertEqual(publish.git(staged.directory, "rev-parse", "--show-toplevel").stdout.strip(),
                                 str(staged.directory))
                self.assertEqual(staged.finish()["action"], "dry-run")
        self.assertFalse((trap / "unexpected-index").exists())
        self.assertEqual(git(trap, "status", "--porcelain"), before)
        self.assertIsNone(self.tip())

    def test_package_validation_rejects_stale_extra_and_symlink(self):
        site = self.directory / "site"
        write_site(site, self.payload)
        self.assertEqual(set(publish.validate_site(site)), publish.EXPECTED_FILES)
        (site / "emu/app.js").write_bytes(self.payload["emu/app.js"] + b"\n// stale\n")
        with self.assertRaisesRegex(publish.DeploymentError, "checksum mismatch"):
            publish.validate_site(site)
        (site / "emu/app.js").write_bytes(self.payload["emu/app.js"])
        (site / "old-installer.html").write_text("not the publish target")
        with self.assertRaisesRegex(publish.DeploymentError, "exactly"):
            publish.validate_site(site)
        (site / "old-installer.html").unlink()
        (site / "emu/app.js").unlink()
        (site / "emu/app.js").symlink_to(ROOT / "web/emu/app.js")
        with self.assertRaisesRegex(publish.DeploymentError, "symlink"):
            publish.validate_site(site)

    def test_source_zip_rejects_binary_and_secret_material(self):
        for name in ("firmware/unsafe.fwsc", "tools/.env.production", "tools/.aws/credentials"):
            with self.subTest(name=name):
                payload = dict(self.payload)
                with zipfile.ZipFile(io.BytesIO(payload["jambox-source.zip"])) as archive:
                    entries = {info.filename: archive.read(info) for info in archive.infolist()}
                entries[publish.SOURCE_PREFIX + name] = b"fixture"
                manifest_name = publish.SOURCE_PREFIX + "SOURCE-MANIFEST.json"
                manifest = json.loads(entries[manifest_name])
                manifest[name] = publish.sha(b"fixture")
                entries[manifest_name] = json.dumps(manifest).encode()
                data = io.BytesIO()
                with zipfile.ZipFile(data, "w", zipfile.ZIP_DEFLATED) as archive:
                    for path, body in entries.items():
                        archive.writestr(path, body)
                payload["jambox-source.zip"] = data.getvalue()
                hashes(payload)
                site = self.directory / name.replace("/", "_")
                write_site(site, payload)
                with self.assertRaisesRegex(publish.DeploymentError, "Private or binary"):
                    publish.validate_site(site)

    def test_device_page_must_match_corresponding_source(self):
        payload = dict(self.payload)
        payload["device/app.js"] += b"\n// changed after source packaging\n"
        hashes(payload)
        site = self.directory / "modified-device-tool"
        write_site(site, payload)
        with self.assertRaisesRegex(publish.DeploymentError, "differs from its corresponding source"):
            publish.validate_site(site)

    def test_candidate_firmware_cannot_enter_public_site(self):
        payload = dict(self.payload)
        payload["device/jambox-candidate.fwsc"] = b"unpublished device build"
        hashes(payload)
        site = self.directory / "candidate-leak"
        write_site(site, payload)
        with self.assertRaisesRegex(publish.DeploymentError, "exactly"):
            publish.validate_site(site)


if __name__ == "__main__":
    unittest.main(verbosity=2)
