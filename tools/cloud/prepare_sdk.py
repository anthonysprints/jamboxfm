#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare the pinned AC79 SDK subset, with original notices and verified hashes.

By default fetch missing files from the exact ChoralRoot GitHub commit over
verified HTTPS. --source accepts a local ChoralRoot checkout or its tools/sdk
directory. Existing destination files must match and are never overwritten.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
from urllib.error import URLError
from urllib.request import urlopen


SOURCE_COMMIT = "aa5875af20e3046ee161f988c5d91d3ae66d0c74"
SOURCE_REPO = "https://github.com/Quixotic7/ChoralRootFM1"
SOURCE_BASE = (
    "https://raw.githubusercontent.com/Quixotic7/ChoralRootFM1/"
    + SOURCE_COMMIT
    + "/tools/sdk/"
)
SDK_HASHES = {
    "cpu/wl82/tools/uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cpu/wl82/tools/cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cpu/wl82/tools/cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}
FILE_HASHES = {
    **SDK_HASHES,
    "LICENSE": "c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4",
    "README.md": "5ed97c2f6605c3d81456db58804543df1c6c30082ffb4007c7052d99f6ac2ceb",
}


def verified(content, relative):
    if hashlib.sha256(content).hexdigest() != FILE_HASHES[relative]:
        raise ValueError(f"SDK checksum mismatch: {relative}; no replacement installed")
    return content


def prepare(source, dest):
    if source is not None and (source / "tools/sdk").is_dir():
        source = source / "tools/sdk"
    pending = {}
    # Validate all existing files and all incoming bytes before adding any files.
    for relative in FILE_HASHES:
        target = dest / relative
        if target.exists():
            verified(target.read_bytes(), relative)
            continue
        if source is not None:
            content = (source / relative).read_bytes()
        else:
            with urlopen(SOURCE_BASE + relative, timeout=45) as response:
                if not response.url.startswith("https://"):
                    raise ValueError("SDK download redirected away from HTTPS")
                content = response.read()
        pending[relative] = verified(content, relative)
    for relative, content in pending.items():
        target = dest / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        # Exclusive creation preserves a file added concurrently by another task.
        try:
            with target.open("xb") as stream:
                stream.write(content)
        except FileExistsError:
            verified(target.read_bytes(), relative)
    manifest = {
        "source_repository": SOURCE_REPO,
        "source_commit": SOURCE_COMMIT,
        "source_path": "tools/sdk",
        "original_vendor_repository": "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK",
        "original_vendor_tag": "AC79NN_SDK_V1.2.1_2023-12-13",
        "license": "Apache-2.0",
        "files_sha256": FILE_HASHES,
    }
    # Add a new immutable receipt; preserve any receipt from earlier environment setup.
    receipt = dest / "JAMBOX-SDK-FILES.json"
    encoded = json.dumps(manifest, indent=2) + "\n"
    if receipt.exists():
        if json.loads(receipt.read_text()) != manifest:
            raise ValueError(f"Existing SDK receipt differs: {receipt}; left unchanged")
    else:
        with receipt.open("x") as stream:
            stream.write(encoded)
    print(f"AC79_SDK={dest.resolve()}: five pinned files verified, including license and provenance")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, help="Local ChoralRoot checkout or SDK subset; otherwise fetch pinned GitHub files")
    parser.add_argument("--dest", type=Path, default=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")))
    args = parser.parse_args()
    try:
        prepare(args.source, args.dest)
    except (OSError, URLError, ValueError) as exc:
        parser.exit(1, f"SDK setup failed: {exc}\n")


if __name__ == "__main__":
    main()
