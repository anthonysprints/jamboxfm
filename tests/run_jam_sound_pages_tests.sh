#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Real melodic page routing with private UI assets; no flash writer/device.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-sound-pages-tests
gen=${JAM_GEN_DIR:-build/gen}
compiler=${CC:-cc}
mkdir -p "$out/gen" "$out/screens"
if [ ! -f "$gen/felucca_tables.h" ]; then
    echo 'Generate firmware tables before running the sound-page fixture.' >&2
    exit 2
fi
python3 tools/gen_font.py "$out/gen/felucca_font.h"
python3 tools/gen_icons.py "$out/gen/felucca_icons.h"
python3 tools/gen_logo.py "$out/gen/sloop_logo.h"
"$compiler" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -fwrapv -fno-strict-aliasing -w \
    ${JAM_TEST_CFLAGS:-} -I"$out/gen" -I"$gen" -Ifirmware/src -Ifirmware/hal \
    tests/jam_sound_pages_test.c -o "$out/native" -lm
"$out/native" "$out/screens"
