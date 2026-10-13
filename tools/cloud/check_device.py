#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only device build prerequisite check; never builds, downloads or flashes."""

import argparse
import hashlib
import os
import sys
from pathlib import Path

# This read-only check must not create __pycache__ while importing shared pins.
sys.dont_write_bytecode = True
from prepare_sdk import SDK_HASHES
from prepare_toolchain import REQUIRED_TOOLS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")))
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")))
    args = parser.parse_args()
    problems = []
    sdk_ok = True
    for relative, expected in SDK_HASHES.items():
        path = args.sdk / relative
        try:
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError:
            problems.append(f"Missing/unreadable SDK file: {path}")
            sdk_ok = False
            continue
        if actual != expected:
            problems.append(f"SDK checksum mismatch: {path}")
            sdk_ok = False
    if sdk_ok:
        print(f"SDK: three pinned binary hashes verified ({args.sdk})")
    missing_tools = [relative for relative in REQUIRED_TOOLS if not os.access(args.toolchain / relative, os.X_OK)]
    if missing_tools:
        problems.append(f"Compiler executables missing at {args.toolchain}: {', '.join(missing_tools)}")
    else:
        print(f"Compiler: four required executables present ({args.toolchain}); compilation has not been tested by this check")
    for problem in problems:
        print(problem, file=sys.stderr)
    if not sdk_ok:
        print("Prepare SDK: python3 tools/cloud/prepare_sdk.py (or --source /path/to/pinned/ChoralRootFM1)", file=sys.stderr)
    if missing_tools:
        print("Prepare compiler: bash tools/cloud/setup.sh --device, or import the pinned owner download with prepare_toolchain.py --archive (see docs/CLOUD_SETUP.md)", file=sys.stderr)
    if problems:
        return 1
    print("Device build prerequisites present; no firmware build or hardware validation was performed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
