#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Actual drum DSP with isolated native headers; never rebuild shared build/gen.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-drum-kit-audio-tests
gen=${JAM_GEN_DIR:-build/gen}
compiler=${CC:-cc}
mkdir -p "$out/gen"
for header in felucca_tables.h felucca_samples.h felucca_fm6.h felucca_drumkits.h; do
    if [ ! -f "$gen/$header" ]; then
        echo "Generate firmware tables before running the drum audio fixture: $header" >&2
        exit 2
    fi
done
python3 tools/gen_font.py "$out/gen/felucca_font.h"
python3 tools/gen_icons.py "$out/gen/felucca_icons.h"
python3 tools/gen_logo.py "$out/gen/sloop_logo.h"
"$compiler" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -fwrapv -fno-strict-aliasing -w \
    -I"$out/gen" -I"$gen" -Ifirmware/src -Ifirmware/hal \
    tests/jam_drum_kit_audio_test.c -o "$out/native" -lm
"$out/native"
