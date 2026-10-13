#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only FM-1 ELF/image memory gates; JSON on stdout, nonzero on failure.

Use --build-dir for an unchanged Sloop build or a JamBox build. This reports
linked/static budgets, not measured stack usage or hardware performance. No
compiler, packager, installer, file-writing tool or device connection is run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from fm1pkg_make import crc16  # noqa: E402
from lz4blk import decompress  # noqa: E402

# Existing firmware/app.ld and firmware/loader/loader.ld limits; never enlarged.
REGIONS = {
    "XIP": (0x02000120, 0x8DFBC), "RAMTEXT": (0x01C00000, 0x6000),
    "RAM": (0x01C08000, 96 * 1024), "POOL": (0x01C20000, 0x54000),
    "NOINIT": (0x01C7C000, 0x3D50), "LDR": (0x01C0A800, 0x14000),
}
APP_SECTIONS = {".text": "XIP", ".ram_text": "RAMTEXT", ".data": "RAM",
                ".bss": "RAM", ".pool": "POOL", ".noinit": "NOINIT"}
APP_STACKS = {"_guard0": 0x01C74000, "_ustack_lo": 0x01C74100, "_ustack_top": 0x01C7A000,
              "_guard1": 0x01C7A000, "_sstack_lo": 0x01C7A100, "_sstack_top": 0x01C7C000}
LOADER_STACKS = {"_stack_top": 0x01C70000, "_sstack_top": 0x01C72000}
OTA_BODY_LIMIT = 0x5000 - 0x200
LOADER_NAME = b"usb_hid_ota.bin".ljust(16, b"\0")


class InvalidBuild(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidBuild(message)


def elf_sections(blob):
    """Read allocation flags and bytes, which JieLi objdump -h omits."""
    require(len(blob) >= 52 and blob[:6] == b"\x7fELF\x01\x01", "expected little-endian ELF32")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", blob)
    _, kind, _, _, entry, _, offset, _, _, _, _, size, count, strings = header
    require(kind == 2 and size == 40 and count > 0 and strings < count, "invalid executable section table")
    require(offset + size * count <= len(blob), "truncated ELF section table")
    rows = [struct.unpack_from("<10I", blob, offset + i * size) for i in range(count)]
    names_at, names_size = rows[strings][4:6]
    require(names_at + names_size <= len(blob), "truncated ELF section names")
    names = blob[names_at:names_at + names_size]
    sections = {}
    for row in rows:
        name_at, typ, flags, address, data_at, length = row[:6]
        require(name_at < len(names) and b"\0" in names[name_at:], "invalid ELF section name")
        name = names[name_at:].split(b"\0", 1)[0].decode("ascii")
        if not name:
            continue
        require(name not in sections, f"duplicate ELF section {name}")
        require(typ == 8 or data_at + length <= len(blob), f"truncated ELF section {name}")
        sections[name] = {"address": address, "bytes": length, "allocated": bool(flags & 2),
                          "nobits": typ == 8, "data": b"" if typ == 8 else blob[data_at:data_at + length]}
    return entry, sections


def objdump_sections(text):
    require("file format ELF32-pi32v2" in text, "objdump did not identify ELF32-pi32v2")
    rows = {}
    for match in re.finditer(r"^\s*\d+\s+(\.\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\b", text, re.M):
        rows[match[1]] = (int(match[3], 16), int(match[2], 16))
    require(bool(rows), "objdump returned no sections")
    return rows


def objdump_symbols(text):
    require("SYMBOL TABLE:" in text, "objdump returned no symbol table")
    symbols, undefined, helpers = {}, [], []
    for line in text.splitlines():
        parts = line.split()
        if not parts or not re.fullmatch(r"[0-9a-fA-F]{8,16}", parts[0]):
            continue
        section = next((i for i in range(1, len(parts)) if parts[i].startswith((".", "*"))), None)
        if section is None or len(parts) <= section + 2:
            continue  # ELF's mandatory unnamed null/undefined symbol.
        name = " ".join(parts[section + 2:])
        symbols[name] = int(parts[0], 16)
        if parts[section] == "*UND*":
            undefined.append(name)
        # These bare-metal builds link no compiler runtime. Any __* or unwind
        # symbol needs explicit review before it can become a supported helper.
        if name.startswith(("__", "_Unwind_")):
            helpers.append(name)
    return symbols, sorted(set(undefined)), sorted(set(helpers))


def memory_report(kind, sections, symbols, errors):
    expected = APP_SECTIONS if kind == "app" else {".text": "LDR", ".bss": "LDR"}
    usage = {}
    rows = []
    for name, section in sections.items():
        if name not in expected and section["allocated"]:
            errors.append(f"{kind}: unexpected allocated section {name}")
        if name not in expected:
            continue
        region = expected[name]
        origin, capacity = REGIONS[region]
        address, length = section["address"], section["bytes"]
        if address < origin or address + length > origin + capacity:
            errors.append(f"{kind}: {name} is outside {region}")
        if length and not section["allocated"]:
            errors.append(f"{kind}: nonempty {name} lacks ALLOC")
        if section["nobits"] != (name in (".bss", ".pool", ".noinit")):
            errors.append(f"{kind}: incorrect load/NOBITS type for {name}")
        rows.append({"name": name, "address": address, "bytes": length, "region": region})
        item = usage.setdefault(region, {"origin": origin, "capacity_bytes": capacity, "used_bytes": 0,
                                         "span_bytes": 0, "minimum_spare_bytes": 0})
        item["used_bytes"] += length
        item["span_bytes"] = max(item["span_bytes"], address + length - origin)
    for name in expected:
        if name not in sections:
            errors.append(f"{kind}: missing section {name}")
    for region, item in usage.items():
        ordered = sorted((row for row in rows if row["region"] == region and row["bytes"]), key=lambda row: row["address"])
        for before, after in zip(ordered, ordered[1:]):
            if before["address"] + before["bytes"] > after["address"]:
                errors.append(f"{kind}: overlapping sections in {region}")
        item["spare_bytes"] = item["capacity_bytes"] - item["span_bytes"]
        item["minimum_spare_bytes"] = {"RAM": 4096, "POOL": 8192}.get(region, 0)
        if item["spare_bytes"] < item["minimum_spare_bytes"]:
            errors.append(f"{kind}: {region} spare {item['spare_bytes']} B < {item['minimum_spare_bytes']} B")
    bindings = {"_start": (".text", False), "_bss_start": (".bss", False), "_bss_end": (".bss", True)}
    if kind == "app":
        bindings.update({"_etext": (".text", True), "_rt_start": (".ram_text", False),
                         "_rt_end": (".ram_text", True), "_data_start": (".data", False),
                         "_data_end": (".data", True), "_pool_start": (".pool", False), "_pool_end": (".pool", True)})
    else:
        bindings["_image_end"] = (".text", True)
    for symbol, (name, end) in bindings.items():
        if name in sections and symbols.get(symbol) != sections[name]["address"] + (sections[name]["bytes"] if end else 0):
            errors.append(f"{kind}: {symbol} does not match {name}")
    stacks = APP_STACKS if kind == "app" else LOADER_STACKS
    for name, address in stacks.items():
        if symbols.get(name) != address:
            errors.append(f"{kind}: unexpected or missing stack/guard symbol {name}")
    start = REGIONS["XIP" if kind == "app" else "LDR"][0]
    if symbols.get("_start") != start:
        errors.append(f"{kind}: _start is not at the required entry address")
    if kind == "loader" and "LDR" in usage:
        end = usage["LDR"]["origin"] + usage["LDR"]["span_bytes"]
        usage["LDR"]["stack_gap_bytes"] = min(LOADER_STACKS.values()) - end
        if end >= min(LOADER_STACKS.values()):
            errors.append("loader: allocated image/BSS reaches the stacks")
    return {"regions": usage, "sections": rows}


def check_binary(kind, blob, sections, symbols):
    if kind == "loader":
        require(0 < len(blob) <= REGIONS["LDR"][1], "loader image is empty or exceeds LDR")
        require(blob == sections[".text"]["data"], "loader.bin differs from ELF .text")
        require(b"FELUCCA-LOADER-1" in blob[:32], "loader entry identification is missing")
        return
    origin, limit = REGIONS["XIP"]
    require(0 < len(blob) <= limit, "app image is empty or exceeds the flash slot")
    require(blob[:4] == bytes.fromhex("04818000"), "app entry stub is invalid")
    image = bytearray(sections[".text"]["data"])
    for name, symbol in ((".ram_text", "_rt_load"), (".data", "_data_load")):
        require(symbol in symbols, f"app: missing load-address symbol {symbol}")
        address = symbols[symbol]
        data = sections[name]["data"]
        require(address % 4 == 0 and origin <= address <= origin + limit, f"app: invalid {symbol}")
        if data:
            require(len(image) <= address - origin and address - origin + len(data) <= limit,
                    f"app: overlapping or out-of-slot load image for {name}")
            image += b"\xff" * (address - origin - len(image)) + data
    image += b"\xff" * (-len(image) % 4)
    require(blob == image, "felucca.bin does not match ELF sections and load-address symbols")


def checked_lz4(block, expected):
    """Bounds-check tokens before calling the existing shared block decoder."""
    pos, produced = 0, 0
    while pos < len(block):
        token = block[pos]
        pos += 1
        literals = token >> 4
        if literals == 15:
            while True:
                require(pos < len(block), "truncated LZ4 literal length")
                extra = block[pos]; pos += 1; literals += extra
                if extra != 255:
                    break
        require(pos + literals <= len(block), "truncated LZ4 literals")
        pos += literals
        produced += literals
        require(produced <= expected, "LZ4 output exceeds declared block length")
        if pos == len(block):
            break
        require(pos + 2 <= len(block), "truncated LZ4 offset")
        offset = block[pos] | block[pos + 1] << 8
        pos += 2
        require(0 < offset <= produced, "invalid LZ4 back-reference")
        match = token & 15
        if match == 15:
            while True:
                require(pos < len(block), "truncated LZ4 match length")
                extra = block[pos]; pos += 1; match += extra
                if extra != 255:
                    break
        produced += match + 4
        require(produced <= expected, "LZ4 output exceeds declared block length")
    require(produced == expected, "LZ4 output length mismatch")
    decoded = decompress(block)
    require(len(decoded) == expected, "shared LZ4 decoder length mismatch")
    return decoded


def check_ota(ota, loader):
    require(64 <= len(ota) <= OTA_BODY_LIMIT + 32, "OTA file size is outside the staging budget")
    outer = struct.unpack_from("<HHIIBBH16s", ota)
    inner = struct.unpack_from("<HHIIBBH16s", ota, 32)
    require(outer[0] == crc16(ota[2:32]) and inner[0] == crc16(ota[34:64]), "OTA header CRC mismatch")
    require(outer[2] == 32 and outer[3] == len(ota) - 32 and outer[4:] == (0x41, 0, 0, LOADER_NAME),
            "OTA outer header fields are invalid")
    require(inner[2] == len(loader) and inner[3] == REGIONS["LDR"][0] and inner[4:] == (0, 0, 0, LOADER_NAME),
            "OTA inner header fields are invalid")
    require(outer[1] == crc16(ota[32:]), "OTA body CRC mismatch")
    require(0 < len(loader) <= REGIONS["LDR"][1], "OTA decoded loader size is invalid")
    require(b"FELUCCA-LOADER-1" in ota[0x48:0x48 + 64], "OTA loader signature is outside the firmware recognition window")
    decoded, pos, count = bytearray(), 64, 0
    while pos < len(ota):
        require(pos + 8 <= len(ota), "truncated OTA block header")
        clen, dlen = struct.unpack_from("<II", ota, pos)
        pos += 8
        require(0 < clen <= 4096 and 0 < dlen <= 4096 and
                dlen == min(4096, len(loader) - len(decoded)), "invalid OTA block lengths")
        require(pos + clen <= len(ota), "truncated OTA block")
        decoded += checked_lz4(ota[pos:pos + clen], dlen)
        pos += clen
        count += 1
    require(decoded == loader and inner[1] == crc16(decoded), "OTA decoded bytes/CRC differ from loader.bin")
    return {"bytes": len(ota), "outer_body_bytes": len(ota) - 32, "body_limit_bytes": OTA_BODY_LIMIT,
            "body_spare_bytes": OTA_BODY_LIMIT - len(ota) + 32, "decoded_bytes": len(decoded), "blocks": count,
            "headers_crc_body_crc_and_loader_match": True}


def report(build_dir, toolchain):
    errors = []
    result = {"schema_version": 1, "build_dir": str(build_dir), "ok": False, "sha256": {}, "errors": errors}
    try:
        objdump = toolchain / "common/bin/objdump"
        require(os.access(objdump, os.X_OK), f"missing executable {objdump}")
        paths = {"app_elf": build_dir / "felucca.elf", "app_bin": build_dir / "felucca.bin",
                 "loader_elf": build_dir / "loader/loader.elf", "loader_bin": build_dir / "loader/loader.bin",
                 "ota_bin": build_dir / "loader/ota.bin"}
        blobs = {name: path.read_bytes() for name, path in paths.items()}
        result["sha256"] = {name: hashlib.sha256(blob).hexdigest() for name, blob in blobs.items()}
        for kind in ("app", "loader"):
            dumps = {}
            for flag in ("-h", "-t"):
                run = subprocess.run([str(objdump), flag, str(paths[kind + "_elf"])], capture_output=True,
                                     text=True, check=False, timeout=30)
                require(run.returncode == 0, f"{kind}: objdump {flag} failed: {run.stderr.strip()}")
                dumps[flag] = run.stdout
            entry, sections = elf_sections(blobs[kind + "_elf"])
            listed = objdump_sections(dumps["-h"])
            require(all(listed.get(name) == (section["address"], section["bytes"]) for name, section in sections.items()),
                    f"{kind}: ELF sections disagree with objdump")
            symbols, undefined, helpers = objdump_symbols(dumps["-t"])
            if undefined:
                errors.append(f"{kind}: undefined symbols: {', '.join(undefined)}")
            if helpers:
                errors.append(f"{kind}: unsupported runtime/helper symbols: {', '.join(helpers)}")
            require(entry == symbols.get("_start"), f"{kind}: ELF entry differs from _start")
            result[kind] = memory_report(kind, sections, symbols, errors)
            result[kind].update({"image_bytes": len(blobs[kind + "_bin"]), "undefined_symbols": len(undefined),
                                 "unsupported_runtime_symbols": len(helpers)})
            check_binary(kind, blobs[kind + "_bin"], sections, symbols)
        result["app"]["flash_slot_bytes"] = REGIONS["XIP"][1]
        result["app"]["flash_spare_bytes"] = REGIONS["XIP"][1] - len(blobs["app_bin"])
        # Flash also holds the initial RAMTEXT/data bytes and their padding.
        # Their runtime regions above are separate from this complete XIP use.
        result["app"]["regions"]["XIP"].update({"used_bytes": len(blobs["app_bin"]),
            "span_bytes": len(blobs["app_bin"]), "spare_bytes": result["app"]["flash_spare_bytes"]})
        result["ota"] = check_ota(blobs["ota_bin"], blobs["loader_bin"])
    except (InvalidBuild, OSError, subprocess.TimeoutExpired, UnicodeError, struct.error, KeyError) as exc:
        errors.append(str(exc))
    result["ok"] = not errors
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--toolchain", type=Path,
                        default=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")))
    args = parser.parse_args()
    result = report(args.build_dir.resolve(), args.toolchain.resolve())
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
