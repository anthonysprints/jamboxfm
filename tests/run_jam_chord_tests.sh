#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the emulator or device first for the generated DSP headers.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
GEN=build/jam-emu-gen
[ -f "$GEN/felucca_tables.h" ] || GEN=build/gen
mkdir -p build/jam-chord-tests
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -I"$GEN" -Ifirmware/src \
    tests/jam_chord_test.c -o build/jam-chord-tests/chords -lm
build/jam-chord-tests/chords
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -I"$GEN" -Ifirmware/src \
    tests/scale_test.c -o build/jam-chord-tests/scales -lm
build/jam-chord-tests/scales
