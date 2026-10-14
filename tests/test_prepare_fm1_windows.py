#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Source preparation checks; no network, dependency installation or hardware."""
import hashlib
import importlib.util
import io
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
from unittest import mock
import warnings
import zipfile


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("windows_prepare", ROOT / "tools/prepare_fm1_windows.py")
prep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)


def archive(entries, root="project-revision"):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as output:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            for name, data in entries:
                output.writestr(name, data)
    data = buffer.getvalue()
    return data, {"name": "fixture", "root": root, "revision": "revision",
                  "repository": "fixture/project", "size_bytes": len(data),
                  "sha256": hashlib.sha256(data).hexdigest()}


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.destination = self.root / "private-preparation"
        self.local = self.root / "local-helpers"
        self.local.mkdir()
        for name in prep.HELPERS:
            (self.local / name).write_text("# Local reviewed fixture\n", encoding="utf-8")
        self.archives, self.data = [], {}
        for name in ("unbricker", "jl-uboot-tool"):
            content, descriptor = archive([
                ("project-revision/", b""),
                ("project-revision/LICENSE", b"MIT fixture license\n"),
                ("project-revision/dangerous.py", b"raise RuntimeError('Downloaded code ran')\n"),
            ])
            descriptor["name"] = name
            self.archives.append(descriptor)
            self.data[name] = content
        self.specs = mock.patch.object(prep, "ARCHIVES", tuple(self.archives))
        self.specs.start()
        self.addCleanup(self.specs.stop)
        self.patch_calls = []

    def fetch(self, descriptor):
        return self.data[descriptor["name"]]

    def patch(self, unbricker, jl_dir, *, check=False):
        self.patch_calls.append(check)
        self.assertEqual(jl_dir, unbricker / "jl-uboot-tool")
        self.assertEqual((jl_dir / ".fm1-unbrick-commit").read_text(), "revision\n")
        for path in (unbricker, jl_dir):
            self.assertEqual((path / "LICENSE").read_bytes(), b"MIT fixture license\n")
        return {"schema_version": 1, "prepared": True, "check_only": check, "flash_actions": False}

    def prepare(self, **options):
        return prep.prepare(self.destination, source_dir=self.local,
                            downloader=options.pop("downloader", self.fetch),
                            patcher=options.pop("patcher", self.patch), **options)

    def test_prepares_local_sources_without_running_downloaded_code_or_subprocess(self):
        with mock.patch.object(subprocess, "run", side_effect=AssertionError("Unexpected subprocess")):
            report = self.prepare()
        self.assertTrue(report["prepared"])
        for key in ("hardware_accessed", "dependencies_installed", "downloaded_code_executed"):
            self.assertFalse(report[key])
        self.assertEqual(self.patch_calls, [False, True])
        self.assertTrue((self.destination / "preparation.json").is_file())
        for target in prep.HELPERS.values():
            self.assertEqual((self.destination / target).read_text(), "# Local reviewed fixture\n")
        self.assertEqual(len(list((self.destination / "source-archives").glob("*.zip"))), 2)
        self.assertEqual(list(self.root.glob(".jambox-prepare-*")), [])

    def test_bad_download_hash_or_size_leaves_no_destination(self):
        data = self.data["unbricker"]
        for bad in (data[:-1], data + b"x", bytes([data[0] ^ 1]) + data[1:]):
            with self.subTest(length=len(bad)):
                with self.assertRaisesRegex(prep.PreparationError, "integrity"):
                    self.prepare(downloader=lambda _: bad)
                self.assertFalse(self.destination.exists())
                self.assertEqual(list(self.root.glob(".jambox-prepare-*")), [])
        self.assertEqual(self.patch_calls, [])

    def test_existing_folder_is_not_touched_or_downloaded(self):
        self.destination.mkdir()
        sentinel = self.destination / "keep.txt"
        sentinel.write_text("personal file")
        with self.assertRaisesRegex(prep.PreparationError, "already exists"):
            self.prepare(downloader=mock.Mock(side_effect=AssertionError("downloaded")))
        self.assertEqual(sentinel.read_text(), "personal file")

    def test_existing_symlink_including_dangling_is_refused(self):
        for target in (self.local, self.root / "missing"):
            try:
                self.destination.symlink_to(target, target_is_directory=True)
            except OSError:
                self.skipTest("Symlink creation unavailable")
            with self.assertRaisesRegex(prep.PreparationError, "already exists"):
                self.prepare()
            self.assertTrue(self.destination.is_symlink())
            self.destination.unlink()

    def test_destination_created_during_download_is_preserved(self):
        def patcher(*args, **kwargs):
            if not self.destination.exists():
                self.destination.mkdir()
                (self.destination / "keep.txt").write_text("new user data")
            return self.patch(*args, **kwargs)
        with self.assertRaisesRegex(prep.PreparationError, "appeared"):
            self.prepare(patcher=patcher)
        self.assertEqual([p.name for p in self.destination.iterdir()], ["keep.txt"])
        self.assertEqual((self.destination / "keep.txt").read_text(), "new user data")

    def test_patch_failure_is_not_published(self):
        with self.assertRaisesRegex(RuntimeError, "source drift"):
            self.prepare(patcher=mock.Mock(side_effect=RuntimeError("source drift")))
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.root.glob(".jambox-prepare-*")), [])

    def test_missing_helper_fails_before_download(self):
        (self.local / "fm1_stock_rom.py").unlink()
        with self.assertRaisesRegex(prep.PreparationError, "Missing local"):
            self.prepare(downloader=mock.Mock(side_effect=AssertionError("downloaded")))

    def test_unsafe_paths_refused_before_extraction(self):
        for name in ("/project-revision/file", "another-root/file", "project-revision/../escape",
                     "project-revision/./file", "project-revision//file", "project-revision/a\\file",
                     "project-revision/file:stream", "project-revision/CON.txt", "project-revision/NUL",
                     "project-revision/LPT1.py", "project-revision/trailing. ", "project-revision/control\n"):
            with self.subTest(name=name):
                data, descriptor = archive([(name, b"bad")])
                with self.assertRaises(prep.PreparationError):
                    prep.extract_archive(data, descriptor, self.destination)
                self.assertFalse(self.destination.exists())

    def test_symlink_and_special_entries_refused(self):
        for mode in (stat.S_IFLNK, stat.S_IFCHR, stat.S_IFIFO):
            entry = zipfile.ZipInfo("project-revision/linked")
            entry.create_system = 3
            entry.external_attr = (mode | 0o777) << 16
            data, descriptor = archive([(entry, b"../outside")])
            with self.assertRaisesRegex(prep.PreparationError, "linked, or special"):
                prep.extract_archive(data, descriptor, self.destination)
            self.assertFalse(self.destination.exists())

    def test_duplicates_case_collisions_and_file_parents_refused(self):
        for entries in (
            [("project-revision/file", b"a"), ("project-revision/file", b"b")],
            [("project-revision/File", b"a"), ("project-revision/file", b"b")],
            [("project-revision/parent", b"a"), ("project-revision/parent/file", b"b")],
        ):
            data, descriptor = archive(entries)
            with self.assertRaises(prep.PreparationError):
                prep.extract_archive(data, descriptor, self.destination)
            self.assertFalse(self.destination.exists())

    def test_archive_expansion_and_entry_count_are_bounded(self):
        data, descriptor = archive([("project-revision/a", b"a" * 20), ("project-revision/b", b"b" * 20)])
        for limit, value in (("MAX_FILE_BYTES", 10), ("MAX_EXPANDED_BYTES", 30), ("MAX_ENTRIES", 1)):
            with self.subTest(limit=limit), mock.patch.object(prep, limit, value):
                with self.assertRaises(prep.PreparationError):
                    prep.extract_archive(data, descriptor, self.destination)
                self.assertFalse(self.destination.exists())

    def test_download_rejects_oversized_body_or_content_length(self):
        descriptor = dict(self.archives[0], size_bytes=4)
        class Response(io.BytesIO):
            headers = {}
        with mock.patch.object(prep.urllib.request, "urlopen", return_value=Response(b"12345")):
            with self.assertRaisesRegex(prep.PreparationError, "exceeded"):
                prep.download_archive(descriptor)
        response = Response(b"1234")
        response.headers = {"Content-Length": "9999999999"}
        with mock.patch.object(prep.urllib.request, "urlopen", return_value=response):
            with self.assertRaisesRegex(prep.PreparationError, "size changed"):
                prep.download_archive(descriptor)

    def test_invalid_zip_is_refused_after_valid_integrity(self):
        data = b"not an archive"
        descriptor = dict(self.archives[0], size_bytes=len(data), sha256=prep.sha256(data))
        with self.assertRaisesRegex(prep.PreparationError, "extraction failed"):
            prep.extract_archive(data, descriptor, self.destination)
        self.assertFalse(self.destination.exists())


if __name__ == "__main__":
    unittest.main()
