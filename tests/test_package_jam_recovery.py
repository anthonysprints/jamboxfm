# SPDX-License-Identifier: GPL-3.0-only
"""Recovery trial packaging gates; no target compilation, network or device."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import device_build_receipt as receipt
import package_jam_device as normal
import package_jam_recovery as recovery
import publish_jam_web as publication
import test_package_jam_device as baseline


class RecoveryPackageTests(baseline.PackageRefusalTests):
    """Reuse every source/artifact/container/ELF refusal against recovery policy."""

    def setUp(self):
        super().setUp()
        self.record["configuration"]["app"]["flags"]["JAMBOX_RECOVERY"] = 1
        self.save()

    def verify(self):
        return recovery.verify_build(self.root, self.build, self.root / "compiler", self.sdk)

    def test_diagnostic_and_persistence_flags_are_rejected(self):
        original = copy.deepcopy(self.record)
        for name, good in recovery.REQUIRED_FLAGS.items():
            with self.subTest(flag=name):
                self.record = copy.deepcopy(original)
                self.record["configuration"]["app"]["flags"][name] = 1 - good
                self.save()
                with self.assertRaisesRegex(ValueError, name):
                    self.verify()

    def test_recovery_development_build_cannot_be_packaged_as_stable(self):
        with self.assertRaisesRegex(ValueError, "Candidate requires JAMBOX_RECOVERY=0"):
            normal.verify_build(self.root, self.build, self.root / "compiler", self.sdk)
        self.memory.assert_not_called()

    def test_recovery_flag_boolean_does_not_bypass_integer_check(self):
        self.record["configuration"]["app"]["flags"]["JAMBOX_RECOVERY"] = True
        self.save()
        with self.assertRaisesRegex(ValueError, "JAMBOX_RECOVERY=1"):
            self.verify()

    def test_recovery_loader_is_refused(self):
        self.record["configuration"]["loader"]["flags"]["JAMBOX_RECOVERY"] = 1
        self.save()
        with self.assertRaisesRegex(ValueError, "JAMBOX_RECOVERY"):
            self.verify()

    def fixture_bundle(self):
        memory = {"schema_version": 1, "ok": True, "errors": [], "app": {"ok": True},
            "loader": {"ok": True}, "ota": {"headers_crc_body_crc_and_loader_match": True},
            "sha256": {field: self.record["artifacts"][name]["sha256"] for field, name in {
                "app_bin": "felucca.bin", "app_elf": "felucca.elf", "loader_bin": "loader/loader.bin",
                "loader_elf": "loader/loader.elf", "ota_bin": "loader/ota.bin"}.items()}}
        source = {path.relative_to(ROOT).as_posix(): path.read_bytes() for path in normal.source_package.source_files()}
        source.update({name: (self.root / name).read_bytes() for name in self.record["inputs_sha256"]})
        hashes = {name: receipt.sha(body) for name, body in source.items()}
        source["SOURCE-MANIFEST.json"] = receipt.json_bytes(hashes)
        archive = normal.source_package.make_zip({"jamboxfm-source/" + name: body for name, body in source.items()})
        licenses = {name: b"license fixture\n" for name in recovery.LICENSE_FILES}
        for name in ("LICENSE", "LICENSING.md"):
            licenses[name] = source[name]
        output = self.root / "recovery-bundle"
        # Binary format and ELF consistency run in verify_build above. This
        # fixture exercises all evidence links without a target compiler.
        with patch.object(recovery, "_validate_candidate_format"), \
             patch.object(recovery.normal, "source_archive", return_value=(archive, self.record["inputs_sha256"])), \
             patch.object(recovery.normal, "license_files", return_value=licenses):
            self.memory.return_value = memory
            recovery.package(self.root, self.build, output, self.root / "compiler", self.sdk)
        return {path.relative_to(output).as_posix(): path.read_bytes() for path in output.rglob("*") if path.is_file()}

    def rehash(self, files):
        files["SHA256SUMS"] = "".join(f"{receipt.sha(body)}  {name}\n" for name, body in sorted(files.items())
                                    if name != "SHA256SUMS").encode()

    def rebind_manifest(self, files, manifest):
        files["manifest.json"] = receipt.json_bytes(manifest)
        files["trial.json"] = receipt.json_bytes(recovery.trial_descriptor(files["manifest.json"], files[recovery.FIRMWARE]))
        self.rehash(files)

    def test_bundle_contract_binds_only_explicit_normal_jambox_trial(self):
        files = self.fixture_bundle()
        self.assertEqual(set(files), recovery.BUNDLE_FILES)
        # Exercise the reviewed JavaScript policy and canonical package parser
        # as well as Python's source/build/memory evidence checks.
        manifest = recovery.validate_bundle_files(files)
        self.assertEqual(manifest["kind"], "development-recovery-candidate")
        self.assertEqual(manifest["build"]["flags"]["JAMBOX_RECOVERY"], 1)
        self.assertEqual(manifest["transitions"], {"mode": "development-trial", "approved": [],
                                                 "simulated": ["jambox-normal-to-jambox"]})
        self.assertFalse(manifest["recovery"]["automatic_saving"])
        self.assertIs(manifest["release_enabled"], False)
        self.assertIs(manifest["hardware_validated"], False)
        self.assertEqual(json.loads(files["trial.json"])["transitions"], ["jambox-normal-to-jambox"])

    def test_bundle_rejects_missing_unexpected_and_modified_bytes(self):
        original = self.fixture_bundle()
        for mutation in (lambda files: files.pop("trial.json"),
                         lambda files: files.update({"unexpected.txt": b"extra"}),
                         lambda files: files.update({recovery.FIRMWARE: files[recovery.FIRMWARE] + b"tampered"})):
            with self.subTest(mutation=mutation):
                files = dict(original)
                mutation(files)
                with patch.object(recovery, "_validate_candidate_format"), self.assertRaises((ValueError, publication.DeploymentError)):
                    recovery.validate_bundle_files(files)

    def test_rehashed_trial_cannot_add_factory_transition_or_claim_validation(self):
        original = self.fixture_bundle()
        for change in ({"transitions": ["factory-v15-to-jambox", "jambox-normal-to-jambox"]},
                       {"hardware_validated": True}, {"release_enabled": True}, {"schema_version": True}):
            with self.subTest(change=change):
                files = dict(original)
                trial = json.loads(files["trial.json"])
                trial.update(change)
                files["trial.json"] = receipt.json_bytes(trial)
                self.rehash(files)
                with patch.object(recovery, "_validate_candidate_format"), self.assertRaisesRegex(ValueError, "trial descriptor"):
                    recovery.validate_bundle_files(files)

    def test_rehashed_memory_must_match_receipt_and_pass(self):
        original = self.fixture_bundle()
        for change in ({"ok": False}, {"sha256": {"app_bin": "0" * 64}}, {"ota": {}}):
            with self.subTest(change=change):
                files = dict(original)
                memory = json.loads(files["memory.json"])
                memory.update(change)
                files["memory.json"] = receipt.json_bytes(memory)
                manifest = json.loads(files["manifest.json"])
                manifest["build"]["memory_report_sha256"] = receipt.sha(files["memory.json"])
                self.rebind_manifest(files, manifest)
                with patch.object(recovery, "_validate_candidate_format"), self.assertRaisesRegex(ValueError, "memory"):
                    recovery.validate_bundle_files(files)

    def test_rehashed_receipt_cannot_change_flag_or_source(self):
        original = self.fixture_bundle()
        for field in ("flags", "source", "artifact"):
            with self.subTest(field=field):
                files = dict(original)
                record = json.loads(files["build-receipt.json"])
                if field == "flags":
                    record["configuration"]["app"]["flags"]["JAMBOX_RECOVERY"] = 0
                elif field == "source":
                    record["inputs_sha256"][self.input] = "0" * 64
                else:
                    record["artifacts"]["felucca.fwsc"]["sha256"] = "0" * 64
                files["build-receipt.json"] = receipt.json_bytes(record)
                manifest = json.loads(files["manifest.json"])
                manifest["build"]["receipt_sha256"] = receipt.sha(files["build-receipt.json"])
                self.rebind_manifest(files, manifest)
                with patch.object(recovery, "_validate_candidate_format"), self.assertRaises(ValueError):
                    recovery.validate_bundle_files(files)

    def test_format_validator_failure_preserves_existing_output(self):
        output = self.root / "recovery-output"
        output.mkdir()
        (output / "previous.txt").write_text("previous verified candidate")
        with patch.object(recovery.normal, "source_archive", return_value=(b"zip fixture", self.record["inputs_sha256"])), \
             patch.object(recovery.normal, "license_files", return_value={name: b"notice" for name in recovery.LICENSE_FILES}), \
             patch.object(recovery, "validate_bundle_files", side_effect=ValueError("format failed")):
            with self.assertRaisesRegex(ValueError, "format failed"):
                recovery.package(self.root, self.build, output, self.root / "compiler", self.sdk)
        self.assertEqual((output / "previous.txt").read_text(), "previous verified candidate")
        self.assertEqual({path.name for path in output.iterdir()}, {"previous.txt"})


if __name__ == "__main__":
    unittest.main()
