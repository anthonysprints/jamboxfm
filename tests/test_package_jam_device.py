# SPDX-License-Identifier: GPL-3.0-only
"""Candidate refusal fixtures: no target compiler, network or physical device."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build
import device_build_receipt as receipt
import fm1pkg_make
import package_jam_device as package


class ConfigurationTests(unittest.TestCase):
    def test_real_preprocessor_preserves_literal_flag_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "firmware/src").mkdir(parents=True)
            output = root / "build"
            output.mkdir()
            (root / "firmware/src/felucca.c").write_text(
                '#define JAMBOX_PROTO 1\n#define FELUCCA_FLASH (!JAMBOX_PROTO)\n'
                '#define FELUCCA_OTA 1\n#define FELUCCA_ID "FM-1_700"\n'
                '#define FELUCCA_VERSION "JamBoxFM" " " "0.1.0-dev"\n')

            def preprocessor(tool, *args):
                self.assertEqual(tool, "cc")
                return subprocess.check_output(["cc", *map(str, args)], cwd=root, text=True)

            result = receipt.configuration(root, output, "app", [], preprocessor)
            self.assertEqual(set(result["flags"]), set(receipt.FLAGS))
            self.assertEqual(result["flags"]["JAMBOX_PROTO"], 1)
            self.assertEqual(result["flags"]["FELUCCA_FLASH"], 0)
            self.assertEqual(result["identity"], "FM-1_700")
            self.assertEqual(result["version"], "JamBoxFM 0.1.0-dev")

    def test_unversioned_export_has_honest_null_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            result = receipt.source_state(Path(directory), set())
            self.assertEqual(result, {"revision": None, "dirty": None, "provenance": "unversioned-source-export"})

    def test_failed_attempt_invalidates_previous_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            path = out / "device-build-receipt.json"
            path.write_text("old successful build")
            with patch.object(build, "OUT", out), patch.object(sys, "argv", ["build.py", "--release", "bad"]), \
                 patch.dict(build.os.environ, {"JAMBOX_PROTO": "0"}):
                with self.assertRaises(SystemExit):
                    build.main()
            self.assertFalse(path.exists())


class PackageRefusalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        (self.build / "loader").mkdir(parents=True)
        (self.build / "gen").mkdir()
        self.sdk = self.root / "external-sdk"
        (self.sdk / "cpu/wl82/tools/cfg").mkdir(parents=True)
        for name in ("uboot.boot", "cfg_tool.bin", "cfg/eq_cfg_hw.bin"):
            (self.sdk / "cpu/wl82/tools" / name).write_bytes(b"licensed SDK fixture")
        self.input = "firmware/src/jam_identity.h"
        (self.root / self.input).parent.mkdir(parents=True)
        (self.root / self.input).write_bytes((ROOT / self.input).read_bytes())
        self.levels = "tools/drumkit_levels.json"
        (self.root / self.levels).parent.mkdir(parents=True)
        (self.root / self.levels).write_text('{"fixture": 1}\n')
        self.assertIn(self.levels, receipt.GENERATOR_INPUTS)
        (self.build / "gen/felucca_tables.h").write_text("fixture generated table")
        app = bytes.fromhex("04818000") + b"FM-1_700\0JamBoxFM 0.1.0-dev\0"
        loader = b"\x01\x02FELUCCA-LOADER-1\0ota-FM-1_700\0" + bytes(32)
        for name, data in {"felucca.bin": app, "felucca.elf": b"mock ELF app",
                           "loader/loader.bin": loader, "loader/loader.elf": b"mock ELF loader",
                           "loader/ota.bin": build.ldr_wrap(loader)}.items():
            (self.build / name).write_bytes(data)
        self.write_package()
        flags = dict.fromkeys(receipt.FLAGS, 0)
        flags.update(receipt.REQUIRED_FLAGS)
        loader_flags = dict.fromkeys(receipt.FLAGS, 0)
        loader_flags.update(JAMBOX_PROTO=1, FELUCCA_OTA=1)
        self.external = {"compiler": {"archive_sha256": "fixture"}, "sdk": {"files_sha256": {}}}
        self.record = {"schema_version": 1, "kind": "jambox-device-build",
            "configuration": {"app": {"flags": flags, "identity": receipt.IDENTITIES["app"],
                                      "version": "JamBoxFM " + receipt.VERSION},
                              "loader": {"flags": loader_flags, "identity": receipt.IDENTITIES["loader"]},
                              "package_identity": receipt.IDENTITIES["package"]},
            "source": {"revision": None, "dirty": None, "provenance": "unversioned-source-export"},
            "inputs_sha256": receipt.hashes(self.root, {self.input, self.levels}),
            "generated_inputs_sha256": receipt.hashes(self.root, {"build/gen/felucca_tables.h"}),
            "toolchain": self.external["compiler"], "sdk": self.external["sdk"], "commands": [{"tool": "fixture compiler"}]}
        self.record["artifacts"] = {name: {"sha256": receipt.file_sha(self.build / name),
            "size_bytes": (self.build / name).stat().st_size} for name in (*receipt.ARTIFACTS, "felucca.fwsc")}
        self.save()
        for item in (patch.object(receipt, "dependency_inputs", return_value={self.input, "build/gen/felucca_tables.h"}),
                     patch.object(receipt, "generator_inputs", return_value={self.input, self.levels}),
                     patch.object(receipt, "external_inputs", return_value=self.external)):
            item.start()
            self.addCleanup(item.stop)
        self.memory = patch.object(package.report_device, "report", return_value={"ok": True, "errors": []}).start()
        self.addCleanup(patch.stopall)

    def write_package(self, key=0x980f, product="FM-1_700"):
        with patch.object(fm1pkg_make, "SDK", self.sdk):
            data = fm1pkg_make.ufw(fm1pkg_make.flash_image((self.build / "felucca.bin").read_bytes(), key),
                                   (self.build / "loader/ota.bin").read_bytes(), product)
        (self.build / "felucca.fwsc").write_bytes(data)

    def save(self):
        (self.build / "device-build-receipt.json").write_bytes(receipt.json_bytes(self.record))

    def refresh_package_hash(self):
        path = self.build / "felucca.fwsc"
        self.record["artifacts"]["felucca.fwsc"] = {"sha256": receipt.file_sha(path), "size_bytes": path.stat().st_size}
        self.save()

    def verify(self):
        return package.verify_build(self.root, self.build, self.root / "compiler", self.sdk)

    def test_matching_candidate_reconstructs_and_checks_memory(self):
        record, firmware, memory = self.verify()
        self.assertEqual(record, self.record)
        self.assertEqual(firmware, (self.build / "felucca.fwsc").read_bytes())
        self.memory.assert_called_once()
        self.assertTrue(memory["ok"])

    def test_diagnostic_and_persistence_flags_are_rejected(self):
        original = copy.deepcopy(self.record)
        for name, good in receipt.REQUIRED_FLAGS.items():
            with self.subTest(flag=name):
                self.record = copy.deepcopy(original)
                self.record["configuration"]["app"]["flags"][name] = 1 - good
                self.save()
                with self.assertRaisesRegex(ValueError, name):
                    self.verify()

    def test_all_identity_mismatches_are_rejected(self):
        original = copy.deepcopy(self.record)
        for unit in ("app", "loader", "package_identity"):
            with self.subTest(unit=unit):
                self.record = copy.deepcopy(original)
                if unit == "package_identity":
                    self.record["configuration"][unit] = "FM-1_900"
                else:
                    self.record["configuration"][unit]["identity"] = "FM-1_900"
                self.save()
                with self.assertRaisesRegex(ValueError, "identity"):
                    self.verify()

    def test_stale_source_and_generated_header_are_rejected(self):
        for name in (self.input, self.levels, "build/gen/felucca_tables.h"):
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_bytes()
                path.write_bytes(original + b"\n/* changed */\n")
                with self.assertRaisesRegex(ValueError, "Stale"):
                    self.verify()
                path.write_bytes(original)

    def test_changed_artifacts_are_rejected_even_if_size_matches(self):
        for name in self.record["artifacts"]:
            with self.subTest(name=name):
                path = self.build / name
                original = path.read_bytes()
                path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
                with self.assertRaisesRegex(ValueError, "Tampered or mixed"):
                    self.verify()
                path.write_bytes(original)

    def test_rehashed_wrong_key_and_product_still_fail_canonical_gate(self):
        for key, product in ((0x1234, "FM-1_700"), (0x980f, "FM-1_900")):
            with self.subTest(key=key, product=product):
                self.write_package(key, product)
                self.refresh_package_hash()
                with self.assertRaisesRegex(ValueError, "canonical"):
                    self.verify()

    def test_out_of_region_report_prevents_packaging(self):
        self.memory.return_value = {"ok": False, "errors": ["app: .text is outside XIP"]}
        with self.assertRaisesRegex(ValueError, "outside XIP"):
            self.verify()

    def test_missing_receipt_and_compiler_provenance_fail(self):
        self.record["toolchain"] = {"archive_sha256": "different compiler"}
        self.save()
        with self.assertRaisesRegex(ValueError, "provenance"):
            self.verify()
        (self.build / "device-build-receipt.json").unlink()
        with self.assertRaisesRegex(ValueError, "Missing build receipt"):
            self.verify()

    def test_source_selection_excludes_reserved_and_binary_artifacts(self):
        selected = {path.relative_to(ROOT).as_posix() for path in package.source_package.source_files()}
        self.assertIn("tools/package_jam_device.py", selected)
        self.assertIn("firmware/src/jam_identity.h", selected)
        self.assertFalse(any(name.startswith(("assets/logo/", "assets/hiphop-pack/", "docs/firmware/")) or
                             name == "assets/icons.png" or name.endswith((".fwsc", ".elf")) for name in selected))


if __name__ == "__main__":
    unittest.main()
