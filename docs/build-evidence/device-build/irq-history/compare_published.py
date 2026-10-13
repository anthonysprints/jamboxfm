#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline comparison of a Sloop build with its tracked, published 2.6 package.

Reads files and validates CRCs. No MIDI, hardware, package writing or network.
The only output is a JSON evidence file; extracted firmware remains in memory.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("sloop_packer", args.repo / "tools/fm1pkg_make.py")
    pack = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pack)
    raw = (args.repo / "docs/firmware/sloop-2.6.fwsc").read_bytes()
    logical = bytearray(b"".join(raw[i * 48:i * 48 + 47] for i in range(20)) + raw[960:])
    header = bytearray(logical[:64])
    pack.enc(header, 0, 64)
    assert pack.crc16(header[2:]) == struct.unpack_from("<H", header)[0], "UFW header CRC"
    count = struct.unpack_from("<H", header, 8)[0]
    assert pack.crc16(logical[64:64 + count * 80]) == struct.unpack_from("<H", header, 2)[0], "UFW list CRC"
    flash = None
    for i in range(count):
        entry = bytearray(logical[64 + i * 80:64 + (i + 1) * 80])
        pack.enc(entry, 0, 80)
        kind, _, crc, _, offset, size, _ = struct.unpack_from("<HHHHIII", entry)
        if kind == 0:
            flash = bytearray(logical[offset:offset + size])
            assert len(flash) == size and pack.crc16(flash) == crc, "flash.bin CRC"
    assert flash is not None, "missing flash.bin"
    isd = bytearray(flash[64:96])
    pack.enc(isd, 0, 32)
    assert pack.crc16(isd[2:]) == struct.unpack_from("<H", isd)[0], "isd entry CRC"
    isd_offset = struct.unpack_from("<I", isd, 4)[0]
    key = pack.chipkey_decode(flash[isd_offset:isd_offset + 32])
    assert key == pack.KEY
    area = bytearray(flash[0x4000:])
    pack.sfc(area, 0, len(area), 0, key)
    entry = area[32:64]
    assert pack.crc16(entry[2:]) == struct.unpack_from("<H", entry)[0], "app entry CRC"
    crc, offset, size = struct.unpack_from("<HII", entry, 2)
    published = bytes(area[offset:offset + size])
    assert len(published) == size and pack.crc16(published) == crc, "app bytes CRC"
    rebuilt = (args.repo / "build/felucca.bin").read_bytes()
    assert len(rebuilt) <= len(published)
    differences = [i for i, (a, b) in enumerate(zip(published, rebuilt)) if a != b]
    match = re.search(r"^([0-9a-f]+)\s+g\s+F\s+\.text\s+([0-9a-f]+)\s+fm1_alnk0_irq$",
                      args.symbols.read_text(), re.M)
    assert match, "missing actual IRQ symbol address/size"
    address, irq_size = int(match[1], 16), int(match[2], 16)
    irq_offset = address - 0x02000120
    old_irq = published[irq_offset:irq_offset + irq_size]
    new_irq = rebuilt[irq_offset:irq_offset + irq_size]
    report = {
        "reference_source_commit": "8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e",
        "reference_package": "docs/firmware/sloop-2.6.fwsc",
        "reference_package_sha256": sha(raw),
        "reference_app_slot_sha256": sha(published),
        "reference_app_slot_size": len(published),
        "rebuilt_app_sha256": sha(rebuilt),
        "rebuilt_app_size": len(rebuilt),
        "validated_crcs": ["UFW header", "UFW entry list", "flash.bin", "isd entry", "app entry", "app bytes"],
        "differences": [{"offset_hex": hex(i), "published": published[i], "rebuilt": rebuilt[i],
                         "published_context": repr(published[max(0, i - 16):i + 17]),
                         "rebuilt_context": repr(rebuilt[max(0, i - 16):i + 17])} for i in differences],
        "remaining_published_padding_all_ff": all(x == 255 for x in published[len(rebuilt):]),
        "irq": {"address_hex": hex(address), "size": irq_size, "byte_identical": old_irq == new_irq,
                "published_sha256": sha(old_irq), "rebuilt_sha256": sha(new_irq)},
        "hardware_tested": False,
    }
    args.out.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    assert old_irq == new_irq, "published IRQ differs"


if __name__ == "__main__":
    main()
