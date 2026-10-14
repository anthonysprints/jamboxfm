#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Use the already-built deliverable and its private generated asset headers.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
if [ ! -f build/jam-emu-gen/felucca_tables.h ] || [ ! -f "${JAM_EMU_WASM:-build/jam-emu/jam-emu.wasm}" ]; then
    echo 'Build the deliverable first: python3 tools/build_jam_emu.py' >&2
    exit 1
fi
mkdir -p build/jam-emu-evidence
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -Ibuild/jam-emu-gen -Ifirmware/src \
    tests/jam_presets_native.c -o build/jam-emu-evidence/presets-native -lm
JAM_PRESETS_NATIVE=build/jam-emu-evidence/presets-native node tests/jam_presets_wasm.mjs
