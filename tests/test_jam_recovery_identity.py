#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact compatibility review checks; generated assets are supplied as digests.

These tests never regenerate the repository's persisted runtime identity.
Actual old/new writer and cold-boot coverage lives in the runtime fixture.
"""
from __future__ import annotations

import contextlib
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "jam_recovery_identity", ROOT / "tools/check_jam_recovery_identity.py")
assert SPEC and SPEC.loader
identity = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(identity)


class RecoveryIdentityTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="jambox-identity-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.header = self.directory / "jr_asset_identity.h"
        self.review_path = self.directory / "jr_catalog_compatibility.json"
        self.canonical = bytes(range(64))
        self.alias = hashlib.sha256(b"reviewed UI-only catalog").digest()
        self.future = hashlib.sha256(b"unreviewed future catalog").digest()
        self.actual = self.canonical[:32] + self.alias
        self.header_bytes = identity.render_identity(self.canonical).encode("utf-8")
        self.header.write_bytes(self.header_bytes)
        self.review = {
            "schema_version": 1,
            "runtime_identity": self.canonical.hex(),
            "baseline_source": "2ef4338d611b61d49efc4d2249183df976eddc83",
            "compatible_catalogs": [
                {"sha256": self.alias.hex(), "review": "Reviewed UI-only changes; stored IDs and audio unchanged."}
            ],
        }
        self.write_review(self.review)
        self.addCleanup(patch.stopall)
        patch.object(identity, "HEADER", self.header).start()
        patch.object(identity, "COMPATIBILITY", self.review_path).start()
        self.render = patch.object(identity, "render", side_effect=self.render_actual).start()

    def render_actual(self, gen_dir: Path) -> tuple[str, bytes]:
        self.assertEqual(gen_dir, self.directory)
        return identity.render_identity(self.actual), self.actual

    def write_review(self, review: object) -> None:
        self.review_path.write_text(json.dumps(review), encoding="utf-8")

    def rejected(self, message: str) -> None:
        with self.assertRaisesRegex(ValueError, message):
            identity.verify(self.directory)
        self.assertEqual(self.header.read_bytes(), self.header_bytes,
                         "refusing a review must never rewrite the runtime identity")

    def test_canonical_baseline_accepts_exact_runtime_identity(self) -> None:
        self.actual = self.canonical
        runtime, actual = identity.verify(self.directory)
        self.assertEqual((runtime, actual), (self.canonical, self.canonical))
        self.assertEqual(self.header.read_bytes(), self.header_bytes)

    def test_approved_alias_preserves_all_64_persisted_identity_bytes(self) -> None:
        runtime, actual = identity.verify(self.directory)
        self.assertEqual(runtime, self.canonical)
        self.assertEqual(actual, self.actual)
        self.assertNotEqual(actual[32:], runtime[32:])
        self.assertEqual(self.header.read_bytes(), self.header_bytes)

    def test_unknown_future_catalog_cannot_inherit_prior_ui_review(self) -> None:
        self.actual = self.canonical[:32] + self.future
        self.rejected("unreviewed recovery source catalog")

    def test_approved_catalog_cannot_approve_different_generated_audio_assets(self) -> None:
        self.actual = hashlib.sha256(b"different generated audio assets").digest() + self.alias
        self.rejected("generated recovery assets changed")

    def test_different_header_bytes_cannot_inherit_the_review(self) -> None:
        for modified in (
            identity.render_identity(self.canonical[:63] + b"\xff").encode(),
            self.header_bytes.replace(b"0x00", b"0x01", 1),
            self.header_bytes + b"\n",
        ):
            with self.subTest(header=modified[-40:]):
                self.header.write_bytes(modified)
                with self.assertRaisesRegex(ValueError, "runtime recovery identity differs"):
                    identity.verify(self.directory)
                self.assertEqual(self.header.read_bytes(), modified)
        self.header.write_bytes(self.header_bytes)

    def test_absent_review_accepts_only_the_exact_original_binding(self) -> None:
        self.review_path.unlink()
        self.actual = self.canonical
        self.assertEqual(identity.verify(self.directory), (self.canonical, self.canonical))
        self.actual = self.canonical[:32] + self.alias
        self.rejected("recovery identity differs from current assets/catalog")

    def test_malformed_top_level_reviews_fail_closed(self) -> None:
        cases: list[object] = [None, True, 1, "*", [], [self.review]]
        for field, values in {
            "schema_version": [True, False, 1.0, "1", 0, 2, None],
            "runtime_identity": [None, True, [], "*", self.canonical.hex()[:-1],
                                 "G" * 128, "A" * 128],
            "baseline_source": [None, True, [], "*", "f" * 39, "A" * 40],
        }.items():
            for value in values:
                review = copy.deepcopy(self.review)
                review[field] = value
                cases.append(review)
        review = copy.deepcopy(self.review)
        review["allow_future_catalogs"] = True
        cases.append(review)
        for field in self.review:
            review = copy.deepcopy(self.review)
            del review[field]
            cases.append(review)
        for case in cases:
            with self.subTest(review=case):
                self.write_review(case)
                self.rejected("invalid recovery compatibility review")

    def test_malformed_catalog_lists_cannot_become_wildcard_reviews(self) -> None:
        record = copy.deepcopy(self.review["compatible_catalogs"][0])
        cases = [None, True, "*", {}, [], [record] * 17]
        for case in cases:
            with self.subTest(catalogs=case):
                review = copy.deepcopy(self.review)
                review["compatible_catalogs"] = case
                self.write_review(review)
                self.rejected("invalid recovery catalog review list")

    def test_malformed_catalog_records_require_exact_hash_and_nonempty_review(self) -> None:
        cases: list[object] = [None, True, "*", [], {}, {"sha256": self.alias.hex()}]
        for field, values in {
            "sha256": [None, True, [], "*", self.alias.hex()[:-1], "a" * 65, "G" * 64, "A" * 64],
            "review": [None, True, [], "", " \n\t"],
        }.items():
            for value in values:
                record = copy.deepcopy(self.review["compatible_catalogs"][0])
                record[field] = value
                cases.append(record)
        record = copy.deepcopy(self.review["compatible_catalogs"][0])
        record["allow_all"] = True
        cases.append(record)
        for case in cases:
            with self.subTest(record=case):
                review = copy.deepcopy(self.review)
                review["compatible_catalogs"] = [case]
                self.write_review(review)
                self.rejected("invalid reviewed recovery catalog")

    def test_malformed_json_is_refused_without_replacing_header(self) -> None:
        self.review_path.write_text('{"schema_version": 1,', encoding="utf-8")
        self.rejected("Expecting")

    def test_cli_reports_actual_catalog_separately_from_persisted_identity(self) -> None:
        output = io.StringIO()
        with patch("sys.argv", ["identity-check", "--gen-dir", str(self.directory)]), \
                contextlib.redirect_stdout(output):
            identity.main()
        self.assertIn("Recovery runtime identity verified: " + self.canonical.hex(), output.getvalue())
        self.assertIn("Current source catalog verified: " + self.alias.hex(), output.getvalue())
        self.assertIn("saved identity unchanged", output.getvalue())
        self.assertEqual(self.header.read_bytes(), self.header_bytes)

    def test_write_cannot_silently_replace_an_active_compatible_identity(self) -> None:
        stderr = io.StringIO()
        with patch("sys.argv", ["identity-check", "--gen-dir", str(self.directory), "--write"]), \
                contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as refusal:
            identity.main()
        self.assertEqual(refusal.exception.code, 1)
        self.assertTrue(stderr.getvalue().strip(), "refusal should explain the compatibility block")
        self.assertEqual(self.header.read_bytes(), self.header_bytes)


if __name__ == "__main__":
    unittest.main()
