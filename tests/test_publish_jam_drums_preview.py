#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Preview deployment checks use local bare Git remotes; never GitHub."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import publish_jam_drums_preview as publish
import package_jam_drums_preview as package

IDENTITY = ("Preview fixture", "preview-fixture@example.invalid")


def git(directory, *arguments):
    result = subprocess.run(["git", "-c", "core.hooksPath=/dev/null", *arguments],
        cwd=directory, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout.decode("utf-8").strip()


def hashes(files):
    files["SHA256SUMS"] = "".join(f"{publish.sha(data)}  {name}\n"
        for name, data in sorted(files.items()) if name != "SHA256SUMS").encode()


def snapshot(marker=b"one"):
    # Synthetic package bytes isolate Git containment tests from the independently
    # tested candidate parser. The CLI always calls the complete validate_site.
    files = {name: marker + b":" + name.encode() for name in publish.EXPECTED_FILES}
    hashes(files)
    return files


def write_site(directory, files):
    for name, body in files.items():
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(body)


class PreviewTreeTests(unittest.TestCase):
    def setUp(self):
        candidate_validation = patch.object(publish, "validate_bundle")
        candidate_validation.start()
        self.addCleanup(candidate_validation.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix="jambox-preview-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.remote = self.directory / "remote.git"
        self.remote.mkdir()
        git(self.remote, "init", "--bare", "--quiet", "--template=")
        self.seed = self.directory / "seed"
        self.seed.mkdir()
        git(self.seed, "init", "--quiet", "--template=", "--initial-branch=gh-pages")
        git(self.seed, "config", "user.name", IDENTITY[0])
        git(self.seed, "config", "user.email", IDENTITY[1])
        stable = {"index.html": b"stable home\r\n", "install/app.js": b"stable installer\x00\xff",
            ".nojekyll": b"", "preview/other/index.html": b"unrelated preview",
            "preview/saving/index.html": b"approved saving preview", "emu/app.js": b"stable emulator",
            "preview/drums/obsolete.html": b"replace only this subtree",
            "sentinel\twith\nodd-name": b"odd path bytes", ".gitattributes": b"* text eol=crlf\n",
            "stable-executable": b"#!/bin/sh\nexit 0\n"}
        write_site(self.seed, stable)
        (self.seed / "stable-executable").chmod(0o755)
        (self.seed / "stable-symlink").symlink_to("../../external-do-not-check-out")
        git(self.seed, "add", ".")
        git(self.seed, "commit", "--quiet", "-m", "Stable site fixture")
        # An unusual but valid empty Git subtree must survive too. Checking out
        # and re-indexing the remote tree would silently remove this sentinel.
        empty_tree = publish.git(self.seed, "hash-object", "-t", "tree", "-w", "--stdin", data=b"").stdout.strip()
        direct = publish.git(self.seed, "ls-tree", "-z", "HEAD").stdout
        sentinel_tree = publish.git(self.seed, "mktree", "-z",
            data=direct + b"040000 tree " + empty_tree + b"\tstable-empty\0").stdout.strip().decode()
        sentinel_commit = publish.git(self.seed, "commit-tree", sentinel_tree, "-p", "HEAD",
            data=b"Empty subtree sentinel\n").stdout.strip().decode()
        git(self.seed, "update-ref", "refs/heads/gh-pages", sentinel_commit)
        git(self.seed, "remote", "add", "origin", str(self.remote))
        git(self.seed, "push", "--quiet", "origin", "HEAD:refs/heads/gh-pages")
        self.original = self.tip()
        git(self.remote, "update-ref", "refs/heads/main", self.original)
        self.protected = publish.outside_preview(publish.tree_entries(self.remote, self.original))

    def tip(self):
        return git(self.remote, "rev-parse", publish.BRANCH)

    def prepare(self, files=None):
        return publish.prepare_deployment(files or snapshot(), str(self.remote), IDENTITY,
            self.directory / "scratch")

    def test_default_dry_run_preserves_stable_bytes_paths_modes_and_other_branches(self):
        with self.prepare() as staged:
            report = staged.finish()
            self.assertEqual(report["action"], "dry-run")
            self.assertEqual(self.tip(), self.original)
            self.assertEqual(publish.outside_preview(publish.tree_entries(staged.directory, staged.tree)),
                self.protected)
            entries = publish.tree_entries(staged.directory, staged.tree)
            self.assertNotIn(b"preview/drums/obsolete.html", entries)
            self.assertEqual({name for name in entries if name.startswith(publish.PREFIX.encode())},
                {publish.PREFIX.encode() + name.encode() for name in
                    publish.EXPECTED_FILES | publish.EXPECTED_DIRECTORIES})
            self.assertTrue(all(metadata.startswith(b"100644 blob ") for name, metadata in entries.items()
                if name.startswith(publish.PREFIX.encode()) and b" tree " not in metadata))
            self.assertEqual(entries[b"stable-empty"], self.protected[b"stable-empty"])
            self.assertEqual(git(staged.directory, "show", "-s", "--format=%an <%ae>", staged.commit),
                f"{IDENTITY[0]} <{IDENTITY[1]}>")
            self.assertEqual(git(staged.directory, "rev-parse", staged.commit + "^"), self.original)
            self.assertFalse((staged.directory / "index.html").exists(), "remote files must never be checked out")
        self.assertEqual(git(self.remote, "rev-parse", "refs/heads/main"), self.original)

    def test_publish_is_explicit_normal_push_and_second_identical_deployment_is_noop(self):
        actual_git = publish.git
        commands = []
        def tracked_git(directory, *arguments, **kwargs):
            commands.append(arguments)
            return actual_git(directory, *arguments, **kwargs)
        with patch.object(publish, "git", side_effect=tracked_git):
            with self.prepare() as staged:
                self.assertEqual(staged.finish(publish=True)["action"], "published")
                first = staged.commit
        pushes = [args for args in commands if args[0] == "push"]
        self.assertEqual(pushes, [("push", "--porcelain", "origin", "HEAD:refs/heads/gh-pages")])
        self.assertEqual(self.tip(), first)
        self.assertEqual(publish.outside_preview(publish.tree_entries(self.remote, first)), self.protected)
        with self.prepare() as staged:
            self.assertEqual(staged.finish(publish=True)["action"], "unchanged")
            self.assertEqual(staged.commit, first)

    def test_remote_advancement_is_refused_without_overwriting_it(self):
        with self.prepare() as stale:
            with self.prepare(snapshot(b"concurrent")) as newer:
                newer.finish(publish=True)
                winner = newer.commit
            with self.assertRaisesRegex(publish.DeploymentError, "changed during preparation"):
                stale.finish(publish=True)
            self.assertEqual(self.tip(), winner)

    def test_push_race_after_precheck_is_rejected_by_nonforced_git_push(self):
        with self.prepare() as stale:
            with self.prepare(snapshot(b"concurrent")) as newer:
                newer.finish(publish=True)
                winner = newer.commit
            with patch.object(publish, "remote_tip", return_value=stale.parent):
                with self.assertRaisesRegex(publish.DeploymentError, "Git push failed"):
                    stale.finish(publish=True)
            self.assertEqual(self.tip(), winner)

    def test_tampered_prepared_head_cannot_push_an_unverified_tree(self):
        with self.prepare() as staged:
            publish.git(staged.directory, "update-ref", publish.BRANCH, staged.parent)
            with self.assertRaisesRegex(publish.DeploymentError, "commit changed"):
                staged.finish(publish=True)
        self.assertEqual(self.tip(), self.original)

    def test_ambient_git_location_does_not_modify_source_index(self):
        sentinel = self.directory / "unrelated-index"
        sentinel.write_bytes(b"leave this unrelated index alone")
        with patch.dict(os.environ, {"GIT_INDEX_FILE": str(sentinel), "GIT_WORK_TREE": str(self.seed),
                "GIT_DIR": str(self.seed / ".git")}):
            with self.prepare() as staged:
                self.assertEqual(staged.finish()["action"], "dry-run")
        self.assertEqual(sentinel.read_bytes(), b"leave this unrelated index alone")
        self.assertEqual(git(self.seed, "status", "--porcelain"), "")

    def test_target_cannot_be_changed_to_root_or_source_branch(self):
        for key, value in (("PREFIX", ""), ("PREFIX", "install/"), ("PREFIX", "../"),
                           ("BRANCH", "refs/heads/main")):
            with self.subTest(key=key, value=value), patch.object(publish, key, value):
                with self.assertRaisesRegex(publish.DeploymentError, "fixed gh-pages"):
                    with self.prepare():
                        pass
        self.assertEqual(self.tip(), self.original)

    def test_missing_pages_branch_is_not_created(self):
        git(self.remote, "update-ref", "-d", publish.BRANCH)
        with self.assertRaisesRegex(publish.DeploymentError, "existing gh-pages"):
            with self.prepare():
                pass
        self.assertEqual(git(self.remote, "for-each-ref", "--format=%(refname)"), "refs/heads/main")

    def test_prefix_parent_file_conflict_is_refused_without_removing_it(self):
        git(self.seed, "rm", "-r", "--quiet", "preview")
        (self.seed / "preview").write_bytes(b"existing stable root path")
        git(self.seed, "add", "preview")
        git(self.seed, "commit", "--quiet", "-m", "Prefix conflict fixture")
        git(self.seed, "push", "--quiet", "origin", "HEAD:refs/heads/gh-pages")
        expected = self.tip()
        with self.assertRaisesRegex(publish.DeploymentError, "conflicts"):
            with self.prepare():
                pass
        self.assertEqual(self.tip(), expected)


    def test_rejected_candidate_is_checked_before_any_git_mutation(self):
        with patch.object(publish, "validate_bundle", side_effect=publish.DeploymentError("bad candidate")), \
                patch.object(publish, "git") as mutate:
            with self.assertRaisesRegex(publish.DeploymentError, "bad candidate"):
                with self.prepare():
                    pass
        mutate.assert_not_called()
        self.assertEqual(self.tip(), self.original)


class PackageContainmentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="jambox-preview-package-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.site = self.directory / "site"
        self.payload = snapshot()
        write_site(self.site, self.payload)

    def test_complete_snapshot_passes_to_reviewed_candidate_validator(self):
        with patch.object(package, "validate_bundle") as validate:
            result = publish.validate_site(self.site)
        self.assertEqual(result, self.payload)
        validate.assert_called_once_with(result)

    def test_bundle_uses_reviewed_candidate_checks_and_matches_runtime_to_source(self):
        files = dict(self.payload)
        files["index.html"] = package.LANDING
        files[".nojekyll"] = b""
        files["install/index.html"] = b'<html data-trial="drums"><a href="../firmware/jamboxfm-source.zip">Source</a>'
        source = {original: files[published] for published, original in package.RUNTIME_SOURCE.items()}
        with patch.object(package.recovery, "validate_bundle_files") as validate, \
                patch.object(package, "source_contents", return_value=source):
            package.validate_bundle(files)
        validate.assert_called_once_with({name: files["firmware/" + name] for name in package.recovery.BUNDLE_FILES})
        source["web/recovery/app.js"] += b" changed"
        with patch.object(package.recovery, "validate_bundle_files"), \
                patch.object(package, "source_contents", return_value=source):
            with self.assertRaisesRegex(publish.DeploymentError, "differs from its corresponding source"):
                package.validate_bundle(files)

    def test_rejected_candidate_never_becomes_a_publishable_snapshot(self):
        with patch.object(package.recovery, "validate_bundle_files",
                side_effect=ValueError("firmware descriptor hash mismatch")):
            with self.assertRaisesRegex(publish.DeploymentError, "firmware descriptor hash mismatch"):
                publish.validate_site(self.site)

    def test_symlinked_root_file_and_parent_directory_are_rejected(self):
        with self.subTest("root"):
            linked = self.directory / "linked"
            linked.symlink_to(self.site, target_is_directory=True)
            with self.assertRaisesRegex(publish.DeploymentError, "symlink"):
                publish.validate_site(linked)
        with self.subTest("file"):
            target = self.site / "install/app.js"
            target.unlink()
            target.symlink_to(self.directory / "outside-private")
            with self.assertRaisesRegex(publish.DeploymentError, "symlink"):
                publish.validate_site(self.site)
            target.unlink()
            target.write_bytes(self.payload["install/app.js"])
        with self.subTest("parent"):
            (self.site / "firmware").rename(self.directory / "outside-firmware")
            (self.site / "firmware").symlink_to(self.directory / "outside-firmware", target_is_directory=True)
            with self.assertRaisesRegex(publish.DeploymentError, "symlink"):
                publish.validate_site(self.site)

    def test_missing_unlisted_and_escaping_snapshot_paths_are_rejected(self):
        for name in ("../index.html", "/index.html", "install/../../index.html", "index.html\x00x", "other.js"):
            with self.subTest(name=name):
                files = dict(self.payload)
                files[name] = b"must not escape preview"
                hashes(files)
                with self.assertRaisesRegex(publish.DeploymentError, "exactly"):
                    publish.validate_snapshot(files)
        (self.site / "extra").mkdir()
        with self.assertRaisesRegex(publish.DeploymentError, "Unexpected drums preview directory"):
            publish.validate_site(self.site)

    def test_modified_file_checksum_is_rejected_before_candidate_validation(self):
        (self.site / "install/app.js").write_bytes(b"changed after packaging")
        with patch.object(package, "validate_bundle") as validate:
            with self.assertRaisesRegex(publish.DeploymentError, "checksum mismatch"):
                publish.validate_site(self.site)
        validate.assert_not_called()

    def test_cli_defaults_to_no_publish_and_has_no_destination_override(self):
        class Prepared:
            def __enter__(self): return self
            def __exit__(self, *args): return False
            def finish(self, value=False):
                self.publish = value
                return {"action": "published" if value else "dry-run"}
        prepared = Prepared()
        with patch.object(publish, "validate_site", return_value=self.payload), \
                patch.object(publish, "author_identity", return_value=IDENTITY), \
                patch.object(publish, "prepare_deployment", return_value=prepared), \
                patch.object(sys, "argv", ["publish_jam_drums_preview.py"]), \
                patch("builtins.print"):
            publish.main()
        self.assertIs(prepared.publish, False)
        for option in ("--branch", "--prefix", "--remote", "--site"):
            with self.subTest(option=option), patch.object(sys, "argv", ["publisher", option, "unsafe"]), \
                    patch("sys.stderr"):
                with self.assertRaises(SystemExit) as error:
                    publish.main()
                self.assertEqual(error.exception.code, 2)


class AssemblyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="jambox-drums-assembly-test-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.bundle = self.directory / "candidate"
        self.output = self.directory / "drums"
        write_site(self.bundle, {name: b"candidate:" + name.encode()
            for name in package.recovery.BUNDLE_FILES})
        self.source = {name: (ROOT / name).read_bytes()
            for name in package.RUNTIME_SOURCE.values()}

    def test_assembly_validates_staged_bytes_and_changes_only_its_output(self):
        normal = self.directory / "normal" / "install.html"
        normal.parent.mkdir()
        normal.write_bytes(b"leave normal installer alone")
        with patch.object(package.recovery, "validate_bundle_files") as candidate, \
                patch.object(package, "source_contents", return_value=self.source):
            result = package.assemble(self.bundle, self.output)
            files = package.validate_site(self.output)
        self.assertGreaterEqual(candidate.call_count, 3)
        self.assertEqual(set(files), package.EXPECTED_FILES)
        self.assertEqual(result["sha256sums_sha256"], package.sha(files["SHA256SUMS"]))
        self.assertEqual(files["install/index.html"], self.source["web/drums/index.html"])
        self.assertEqual(normal.read_bytes(), b"leave normal installer alone")

    def test_runtime_source_mismatch_cannot_replace_existing_package(self):
        self.output.mkdir()
        sentinel = self.output / "sentinel"
        sentinel.write_bytes(b"existing package")
        self.source["web/drums/index.html"] += b" altered archive"
        with patch.object(package.recovery, "validate_bundle_files"), \
                patch.object(package, "source_contents", return_value=self.source):
            with self.assertRaisesRegex(package.DeploymentError, "corresponding source"):
                package.assemble(self.bundle, self.output)
        self.assertEqual(sentinel.read_bytes(), b"existing package")

    def test_candidate_symlinks_and_unlisted_files_are_rejected(self):
        for name in ("extra-private.bin", "manifest.json"):
            with self.subTest(name=name):
                path = self.bundle / name
                previous = path.read_bytes() if path.exists() else None
                if previous is not None:
                    path.unlink()
                    path.symlink_to(self.directory / "outside")
                else:
                    path.write_bytes(b"unlisted")
                with self.assertRaises(package.DeploymentError):
                    package.assemble(self.bundle, self.output)
                path.unlink()
                if previous is not None:
                    path.write_bytes(previous)
                self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
