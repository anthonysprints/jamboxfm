#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Native regression checks against the real firmware adapter; no device access.
set -eu
cd "$(dirname "$0")/.."
gen=${JAM_GEN_DIR:-build/jam-emu-gen}
out=build/jam-duration-ownership-tests
if [ ! -f "$gen/felucca_tables.h" ]; then
    echo 'Build emulator assets first: python3 tools/build_jam_emu.py' >&2
    exit 1
fi
mkdir -p "$out"
"${CC:-cc}" -O2 -fwrapv -fno-strict-aliasing -w -I"$gen" -Ifirmware/src \
    tests/jam_duration_ownership_test.c -o "$out/native" -lm
"$out/native"
