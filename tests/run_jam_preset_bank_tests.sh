#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Source tools/cloud/activate.sh first. No device connection or shared generation.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-preset-bank-tests
gen=${JAM_GEN_DIR:-build/jam-emu-gen}
[ -f "$gen/felucca_tables.h" ] || gen=build/gen
mkdir -p "$out"
"${CC:-clang}" -O2 -fwrapv -fno-strict-aliasing -Wall -Wno-unused-function -Wno-int-to-pointer-cast \
    -I"$gen" -Ifirmware/src tests/jam_preset_bank_test.c -o "$out/bank" -lm
"$out/bank" "$out/metrics.json" "$out/audition.wav"
