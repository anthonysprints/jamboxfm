# SPDX-License-Identifier: GPL-3.0-only
"""Targeted static-gate corruption/boundary fixtures; no compiler or device."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("report_device", ROOT / "tools/cloud/report_device.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
from build import ldr_wrap  # noqa: E402; the existing package writer, no build actions on import


def section(address, length, nobits=False, data=None):
    return {"address": address, "bytes": length, "allocated": length != 0,
            "nobits": nobits, "data": b"" if nobits else data if data is not None else bytes(length)}


def app_fixture(ram_spare=4096, pool_spare=8192):
    r = report.REGIONS
    sections = {
        ".text": section(r["XIP"][0], 16, data=bytes.fromhex("04818000") + bytes(12)),
        ".ram_text": section(r["RAMTEXT"][0], 0),
        ".data": section(r["RAM"][0], 16),
        ".bss": section(r["RAM"][0] + 16, r["RAM"][1] - 16 - ram_spare, True),
        ".pool": section(r["POOL"][0], r["POOL"][1] - pool_spare, True),
        ".noinit": section(r["NOINIT"][0], r["NOINIT"][1], True),
    }
    symbols = dict(report.APP_STACKS, _start=r["XIP"][0], _etext=r["XIP"][0] + 16,
                   _rt_load=r["XIP"][0] + 16, _data_load=r["XIP"][0] + 16)
    for name, prefix in ((".data", "_data"), (".bss", "_bss"), (".pool", "_pool"), (".ram_text", "_rt")):
        symbols[prefix + "_start"] = sections[name]["address"]
        symbols[prefix + "_end"] = sections[name]["address"] + sections[name]["bytes"]
    return sections, symbols


def repair_ota_headers(blob):
    struct.pack_into("<H", blob, 32, report.crc16(blob[34:64]))
    struct.pack_into("<H", blob, 2, report.crc16(blob[32:]))
    struct.pack_into("<H", blob, 0, report.crc16(blob[2:32]))
    return bytes(blob)


class DeviceReportTests(unittest.TestCase):
    def test_exact_reserves_and_empty_ramtext_are_valid(self):
        sections, symbols = app_fixture()
        errors = []
        result = report.memory_report("app", sections, symbols, errors)
        self.assertEqual(errors, [])
        self.assertEqual(result["regions"]["RAM"]["spare_bytes"], 4096)
        self.assertEqual(result["regions"]["POOL"]["spare_bytes"], 8192)

    def test_each_reserve_one_byte_short_fails(self):
        for ram, pool, region in ((4095, 8192, "RAM"), (4096, 8191, "POOL")):
            with self.subTest(region=region):
                errors = []
                report.memory_report("app", *app_fixture(ram, pool), errors)
                self.assertTrue(any(region + " spare" in error for error in errors))

    def test_wrong_region_and_unknown_allocated_section_fail(self):
        for name in (".noinit", ".ram_text"):
            with self.subTest(name=name):
                sections, symbols = app_fixture()
                sections[name]["bytes"] += 1 if name == ".noinit" else report.REGIONS["RAMTEXT"][1] + 1
                errors = []
                report.memory_report("app", sections, symbols, errors)
                self.assertTrue(any("outside" in error for error in errors))
        sections, symbols = app_fixture()
        sections[".orphan"] = section(0x02000130, 4)
        errors = []
        report.memory_report("app", sections, symbols, errors)
        self.assertTrue(any("unexpected allocated" in error for error in errors))

    def test_loader_budget_is_combined_span_not_independent_sections(self):
        origin, capacity = report.REGIONS["LDR"]
        sections = {".text": section(origin, capacity - 4), ".bss": section(origin + capacity - 4, 8, True)}
        symbols = dict(report.LOADER_STACKS, _start=origin, _image_end=origin + capacity - 4,
                       _bss_start=origin + capacity - 4, _bss_end=origin + capacity + 4)
        errors = []
        result = report.memory_report("loader", sections, symbols, errors)
        self.assertEqual(result["regions"]["LDR"]["span_bytes"], capacity + 4)
        self.assertTrue(any("outside LDR" in error for error in errors))

    def test_stack_reservations_must_match_linker(self):
        sections, symbols = app_fixture()
        symbols["_ustack_lo"] -= 256
        errors = []
        report.memory_report("app", sections, symbols, errors)
        self.assertTrue(any("_ustack_lo" in error for error in errors))

    def test_named_undefined_and_runtime_helpers_are_detected(self):
        symbols, undefined, helpers = report.objdump_symbols("""SYMBOL TABLE:
00000000         *UND* 00000000
00000000         *UND* 00000000 __divdi3
02000120 g F .text 00000020 ordinary_function
02000140 g F .text 00000020 __adddf3
""")
        self.assertEqual(undefined, ["__divdi3"])
        self.assertEqual(helpers, ["__adddf3", "__divdi3"])
        self.assertEqual(symbols["ordinary_function"], 0x02000120)

    def test_binary_must_match_elf_load_bytes(self):
        sections, symbols = app_fixture()
        image = sections[".text"]["data"] + sections[".data"]["data"]
        report.check_binary("app", image, sections, symbols)
        with self.assertRaisesRegex(report.InvalidBuild, "does not match"):
            report.check_binary("app", image[:-1] + b"x", sections, symbols)
        symbols["_data_load"] = symbols["_start"] + 4
        with self.assertRaisesRegex(report.InvalidBuild, "overlapping"):
            report.check_binary("app", image, sections, symbols)

    def test_ota_written_by_existing_builder_round_trips(self):
        loader = b"\x01\x02FELUCCA-LOADER-1" + bytes(8122)
        result = report.check_ota(ldr_wrap(loader), loader)
        self.assertEqual(result["decoded_bytes"], len(loader))
        self.assertEqual(result["blocks"], 2)

    def test_ota_header_body_and_size_corruptions_fail(self):
        loader = b"\x01\x02FELUCCA-LOADER-1" + bytes(8122)
        ota = ldr_wrap(loader)
        for offset in (0, 32, len(ota) - 1):
            with self.subTest(offset=offset):
                bad = bytearray(ota); bad[offset] ^= 1
                with self.assertRaises(report.InvalidBuild):
                    report.check_ota(bytes(bad), loader)
        with self.assertRaisesRegex(report.InvalidBuild, "staging budget"):
            report.check_ota(ota + bytes(report.OTA_BODY_LIMIT), loader)

    def test_rechecks_fields_after_valid_crc(self):
        loader = b"\x01\x02FELUCCA-LOADER-1" + bytes(8122)
        bad = bytearray(ldr_wrap(loader))
        struct.pack_into("<I", bad, 40, report.REGIONS["LDR"][0] + 4)
        with self.assertRaisesRegex(report.InvalidBuild, "inner header fields"):
            report.check_ota(repair_ota_headers(bad), loader)
        bad = bytearray(ldr_wrap(loader)); struct.pack_into("<I", bad, 64, 0)
        with self.assertRaisesRegex(report.InvalidBuild, "block lengths"):
            report.check_ota(repair_ota_headers(bad), loader)

    def test_lz4_rejects_zero_offset_and_truncated_literals(self):
        with self.assertRaisesRegex(report.InvalidBuild, "back-reference"):
            report.checked_lz4(b"\x10A\x00\x00", 5)
        with self.assertRaisesRegex(report.InvalidBuild, "truncated LZ4 literals"):
            report.checked_lz4(b"\xf0\x20A", 47)

    def test_ota_rejects_extra_zero_length_block_even_with_valid_crcs(self):
        loader = b"\x01\x02FELUCCA-LOADER-1" + bytes(8122)
        bad = bytearray(ldr_wrap(loader)) + struct.pack("<II", 1, 0) + b"\0"
        struct.pack_into("<I", bad, 8, len(bad) - 32)
        with self.assertRaisesRegex(report.InvalidBuild, "block lengths"):
            report.check_ota(repair_ota_headers(bad), loader)


if __name__ == "__main__":
    unittest.main()
