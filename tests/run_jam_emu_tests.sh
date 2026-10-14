#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Source tools/cloud/activate.sh first. Builds the deliverable, then tests it.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
command -v "$CC" >/dev/null || { echo 'Source tools/cloud/activate.sh first.' >&2; exit 1; }
export CC
python3 tools/package_jam_emu.py
mkdir -p build/jam-emu-evidence
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -Ibuild/jam-emu-gen -Ifirmware/src \
    tests/jam_emu_native.c -o build/jam-emu-evidence/native -lm
build/jam-emu-evidence/native
sh tests/run_jam_home_nav_tests.sh
node tests/jam_emu_browser.mjs
echo 'JamBox emulator package, native controls, and real browser tests passed.'
