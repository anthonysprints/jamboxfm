#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Archive acquisition regressions; never execute a compiler or use the network."""

import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "prepare_toolchain", Path(__file__).resolve().parents[1] / "tools/cloud/prepare_toolchain.py"
)
toolchain = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(toolchain)


class AcquisitionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base = self.root / "managed"
        self.source = self.root / "owner.tar.xz"
        with tarfile.open(self.source, "w:xz") as bundle:
            for relative in toolchain.REQUIRED_TOOLS:
                self.add_member(bundle, "jieli-linux-toolchains-test/" + relative, b"mock executable")
        self.digest = hashlib.sha256(self.source.read_bytes()).hexdigest()
        self.version = patch.object(toolchain.subprocess, "check_output", return_value="mock compiler version\n").start()
        self.network = patch.object(toolchain.subprocess, "run", side_effect=AssertionError("Unexpected network call")).start()
        self.addCleanup(patch.stopall)
        self.stdout = contextlib.redirect_stdout(io.StringIO())
        self.stdout.__enter__()
        self.addCleanup(self.stdout.__exit__, None, None, None)

    @staticmethod
    def add_member(bundle, name, data):
        member = tarfile.TarInfo(name)
        member.size = len(data)
        member.mode = 0o755
        bundle.addfile(member, io.BytesIO(data))

    def prepare(self, source=None, expected=None):
        toolchain.prepare(self.base, source, expected)

    def lock(self):
        return json.loads((self.base / "toolchain-lock.json").read_text())

    def test_owner_import_records_honest_provenance_and_reuses_pin(self):
        self.prepare(self.source, self.digest.upper())
        pin = self.lock()
        self.assertEqual(pin["archive_sha256"], self.digest)
        self.assertEqual(pin["source_kind"], "owner_provided_archive")
        self.assertEqual(pin["supplied_archive_name"], "owner.tar.xz")
        self.assertNotIn("resolved_url", pin)
        self.assertIn("not observed", pin["acquisition_trust"])
        self.assertEqual((self.base / "jieli-linux-toolchain.tar.xz").read_bytes(), self.source.read_bytes())
        self.assertTrue((self.base / "toolchain/pi32v2/bin/clang").is_file())
        self.prepare(expected=self.digest)
        self.assertEqual(self.lock(), pin)
        self.network.assert_not_called()

    def test_import_mismatch_rejected_before_extraction_or_lock(self):
        with self.assertRaisesRegex(ValueError, "expected-sha256"):
            self.prepare(self.source, "0" * 64)
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.assertFalse((self.base / "toolchain").exists())
        self.version.assert_not_called()

    def test_existing_lock_cannot_be_replaced_by_different_input_or_request(self):
        self.prepare(self.source)
        before = (self.base / "toolchain-lock.json").read_bytes()
        other = self.root / "other.tar.xz"
        other.write_bytes(b"different bytes")
        with self.assertRaisesRegex(ValueError, "acquisition lock"):
            self.prepare(other)
        with self.assertRaisesRegex(ValueError, "existing acquisition lock"):
            self.prepare(expected="0" * 64)
        self.assertEqual((self.base / "toolchain-lock.json").read_bytes(), before)
        self.assertEqual(toolchain.digest_file(self.base / "jieli-linux-toolchain.tar.xz"), self.digest)

    def test_corrupt_or_unpinned_cache_is_not_adopted(self):
        self.prepare(self.source)
        archive = self.base / "jieli-linux-toolchain.tar.xz"
        archive.write_bytes(b"corrupt cache")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            self.prepare(self.source, self.digest)
        (self.base / "toolchain-lock.json").unlink()
        with self.assertRaisesRegex(ValueError, "no acquisition lock"):
            self.prepare(self.source, self.digest)

    def test_missing_file_and_invalid_expected_digest_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "not a readable file"):
            self.prepare(self.root / "missing.tar.xz")
        for expected in ("", "0" * 63, "x" * 64):
            with self.subTest(expected=expected), self.assertRaisesRegex(ValueError, "64 hexadecimal"):
                self.prepare(self.source, expected)
        self.version.assert_not_called()
        self.network.assert_not_called()

    def test_tar_path_escape_is_blocked_without_compiler_execution(self):
        malicious = self.root / "malicious.tar.xz"
        with tarfile.open(malicious, "w:xz") as bundle:
            self.add_member(bundle, "jieli-linux-toolchains-test/../../../escaped", b"escape")
        with self.assertRaises(tarfile.FilterError):
            self.prepare(malicious)
        self.assertFalse((self.root / "escaped").exists())
        self.assertFalse((self.base / "toolchain").exists())
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz").exists())
        self.version.assert_not_called()

    def test_malformed_first_import_does_not_poison_valid_retry(self):
        invalid = self.root / "invalid.tar.xz"
        invalid.write_bytes(b"not an xz archive")
        with self.assertRaises(tarfile.ReadError):
            self.prepare(invalid)
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz").exists())
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz.partial").exists())
        self.version.assert_not_called()
        self.prepare(self.source, self.digest)
        self.assertEqual(self.lock()["archive_sha256"], self.digest)

    def test_unmanaged_release_directory_is_left_untouched(self):
        target = self.base / "jieli-linux-toolchains-test"
        target.mkdir(parents=True)
        sentinel = target / "owner-file"
        sentinel.write_text("preserve")
        with self.assertRaisesRegex(ValueError, "unmanaged"):
            self.prepare(self.source)
        self.assertEqual(sentinel.read_text(), "preserve")
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz").exists())
        self.version.assert_not_called()

    def test_changed_installed_tool_is_rejected_before_execution(self):
        self.prepare(self.source)
        before = (self.base / "toolchain-lock.json").read_bytes()
        self.version.reset_mock()
        (self.base / "toolchain/pi32v2/bin/clang").write_bytes(b"modified executable")
        with self.assertRaisesRegex(ValueError, "tree differs"):
            self.prepare(expected=self.digest)
        self.assertEqual((self.base / "toolchain-lock.json").read_bytes(), before)
        self.version.assert_not_called()

    def test_unexpected_installed_data_is_rejected_before_execution(self):
        self.prepare(self.source)
        self.version.reset_mock()
        (self.base / "toolchain/unexpected-library.so").write_bytes(b"extra runtime dependency")
        with self.assertRaisesRegex(ValueError, "tree differs"):
            self.prepare(expected=self.digest)
        self.version.assert_not_called()

    def test_installed_release_symlink_is_rejected(self):
        self.prepare(self.source)
        self.version.reset_mock()
        release = self.base / "jieli-linux-toolchains-test"
        relocated = self.root / "elsewhere"
        release.rename(relocated)
        release.symlink_to(relocated, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "not a real directory"):
            self.prepare(expected=self.digest)
        self.assertTrue(release.is_symlink())
        self.version.assert_not_called()

    def test_symlinks_must_stay_within_release_after_relocation(self):
        linked = self.root / "linked.tar.xz"
        with tarfile.open(linked, "w:xz") as bundle:
            for relative in toolchain.REQUIRED_TOOLS:
                self.add_member(bundle, "jieli-linux-toolchains-test/" + relative, b"mock executable")
            member = tarfile.TarInfo("jieli-linux-toolchains-test/unused-link")
            member.type = tarfile.SYMTYPE
            member.linkname = "../staging-sibling"
            bundle.addfile(member)
        with self.assertRaisesRegex(ValueError, "symlink escapes"):
            self.prepare(linked)
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz").exists())
        self.assertFalse((self.base / "jieli-linux-toolchains-test").exists())
        self.version.assert_not_called()

    def test_https_download_enforces_expected_digest_and_preserves_tls_flags(self):
        def download(command, **kwargs):
            self.assertEqual(command[command.index("--proto") + 1], "=https")
            self.assertEqual(command[command.index("--proto-redir") + 1], "=https")
            self.assertNotIn("--insecure", command)
            Path(command[command.index("--output") + 1]).write_bytes(self.source.read_bytes())
            return subprocess.CompletedProcess(command, 0, stdout="https://vendor.example/compiler.tar.xz")

        self.network.side_effect = download
        with self.assertRaisesRegex(ValueError, "expected-sha256"):
            self.prepare(expected="0" * 64)
        self.assertFalse((self.base / "jieli-linux-toolchain.tar.xz").exists())
        self.assertFalse((self.base / "toolchain-lock.json").exists())
        self.prepare(expected=self.digest)
        self.assertEqual(self.lock()["source_kind"], "official_https")
        self.assertEqual(self.lock()["resolved_url"], "https://vendor.example/compiler.tar.xz")


if __name__ == "__main__":
    unittest.main()
