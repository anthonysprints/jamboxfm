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


def source_zip_with(data, replacements):
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries = {info.filename: archive.read(info) for info in archive.infolist()}
    entries.update({publish.SOURCE_PREFIX + name: body for name, body in replacements.items()})
    manifest_name = publish.SOURCE_PREFIX + "SOURCE-MANIFEST.json"
    entries[manifest_name] = json.dumps({name.removeprefix(publish.SOURCE_PREFIX): publish.sha(body)
        for name, body in entries.items() if name != manifest_name}, sort_keys=True).encode()
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, body in entries.items():
            archive.writestr(name, body)
    return output.getvalue()


class DeploymentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Existing deployment tests exercise the base site even if the current
        # local package explicitly includes the experimental extension.
        cls.packaged = publish.validate_site(ROOT / "build/jambox-web")
        cls.payload = {name: cls.packaged[name] for name in publish.EXPECTED_FILES}
        hashes(cls.payload)

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


class ExperimentalPublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = publish.validate_site(ROOT / "build/jambox-web")
        cls.payload = {name: base[name] for name in publish.EXPECTED_FILES}
        # Use the real candidate's CRCs, build receipt and corresponding source;
        # mocked bytes would miss the checks that protect the first install.
        bundle = ROOT / "build/jambox-device"
        for name in publish.TRIAL_BUNDLE_FILES:
            cls.payload["firmware/" + name] = (bundle / name).read_bytes()
        runtime = {name: (ROOT / "web" / name).read_bytes() if (ROOT / "web" / name).exists()
                   else b'<a href="../firmware/jamboxfm-source.zip">Source</a>'
                   for name in publish.TRIAL_RUNTIME_FILES}
        cls.payload.update(runtime)
        cls.payload["LICENSES.html"] += (b'<a href="firmware/licenses/AC79-SDK-LICENSE.txt">SDK license</a>'
            b'<a href="firmware/licenses/AC79-SDK-README.md">SDK notes</a>'
            b'<a href="firmware/jamboxfm-source.zip">Firmware source</a>')
        cls.payload["jambox-source.zip"] = source_zip_with(cls.payload["jambox-source.zip"],
            {"web/" + name: body for name, body in runtime.items()})
        cls.bind(cls.payload)

    @staticmethod
    def bind(files):
        bundle = {name: files["firmware/" + name] for name in publish.TRIAL_BUNDLE_FILES}
        hashes(bundle)
        files["firmware/SHA256SUMS"] = bundle["SHA256SUMS"]
        files["firmware/trial.json"] = json.dumps({"schema_version": 1,
            "kind": "experimental-first-install", "hardware_validated": False,
            "transition": "factory-v15-to-jambox", "manifest_sha256": publish.sha(bundle["manifest.json"]),
            "firmware_sha256": publish.sha(bundle["jamboxfm-0.1.0-dev.fwsc"])}).encode()
        hashes(files)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="jambox-trial-publish-test-")
        self.addCleanup(self.temporary.cleanup)
        self.site = Path(self.temporary.name) / "site"

    def verify(self, payload):
        write_site(self.site, payload)
        return publish.validate_site(self.site)

    def test_exact_experimental_bundle_validates_without_promoting_candidate(self):
        files = self.verify(self.payload)
        self.assertEqual(set(files), publish.TRIAL_FILES)
        manifest = json.loads(files["firmware/manifest.json"])
        self.assertIs(manifest["hardware_validated"], False)
        self.assertIs(manifest["release_enabled"], False)
        self.assertEqual(manifest["transitions"]["approved"], [])

    def test_descriptor_binding_and_unearned_hardware_claim_are_rejected(self):
        for field, value in (("firmware_sha256", "0" * 64), ("manifest_sha256", "0" * 64),
                             ("hardware_validated", True), ("schema_version", True),
                             ("transition", "sloop-2.6-to-jambox"), ("extra", False)):
            with self.subTest(field=field):
                payload = dict(self.payload)
                trial = json.loads(payload["firmware/trial.json"])
                trial[field] = value
                payload["firmware/trial.json"] = json.dumps(trial).encode()
                hashes(payload)
                with self.assertRaisesRegex(publish.DeploymentError, "descriptor or candidate binding"):
                    self.verify(payload)

    def test_rehashed_firmware_corruption_fails_package_crc(self):
        payload = dict(self.payload)
        filename = "firmware/jamboxfm-0.1.0-dev.fwsc"
        data = bytearray(payload[filename])
        data[-1] ^= 1
        payload[filename] = bytes(data)
        manifest = json.loads(payload["firmware/manifest.json"])
        manifest["firmware"]["sha256"] = publish.sha(payload[filename])
        payload["firmware/manifest.json"] = json.dumps(manifest).encode()
        self.bind(payload)
        with self.assertRaisesRegex(publish.DeploymentError, "Candidate format validation failed"):
            self.verify(payload)

    def test_rehashed_device_source_must_match_receipt(self):
        payload = dict(self.payload)
        filename = "firmware/jamboxfm-source.zip"
        payload[filename] = source_zip_with(payload[filename], {"firmware/src/jam_identity.h": b"wrong source"})
        manifest = json.loads(payload["firmware/manifest.json"])
        manifest["source"].update(sha256=publish.sha(payload[filename]), size_bytes=len(payload[filename]))
        payload["firmware/manifest.json"] = json.dumps(manifest).encode()
        self.bind(payload)
        with self.assertRaisesRegex(publish.DeploymentError, "source differs from the build receipt"):
            self.verify(payload)

    def test_trial_cannot_smuggle_backup_or_manufacturer_package(self):
        for name in ("firmware/full-flash-before.bin", "firmware/FM-1.fwsc", "firmware/retained-ram.bin"):
            with self.subTest(name=name):
                payload = dict(self.payload)
                payload[name] = b"private fixture"
                hashes(payload)
                with self.assertRaisesRegex(publish.DeploymentError, "exactly"):
                    self.verify(payload)
                (self.site / name).unlink()

    def test_trial_controller_must_match_published_source(self):
        payload = dict(self.payload)
        payload["jam-trial-install.js"] += b"\n// changed after source export"
        hashes(payload)
        with self.assertRaisesRegex(publish.DeploymentError, "differs from its corresponding source"):
            self.verify(payload)

    def test_candidate_requires_exact_trial_descriptor_and_license_set(self):
        for name in ("firmware/trial.json", "firmware/licenses/AC79-SDK-LICENSE.txt"):
            with self.subTest(name=name), tempfile.TemporaryDirectory(prefix="jambox-missing-trial-") as directory:
                payload = dict(self.payload)
                del payload[name]
                hashes(payload)
                write_site(Path(directory), payload)
                with self.assertRaisesRegex(publish.DeploymentError, "exactly"):
                    publish.validate_site(Path(directory))


if __name__ == "__main__":
    unittest.main(verbosity=2)
