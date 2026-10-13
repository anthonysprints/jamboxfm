#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
out=build/jam-tests
gen=${JAM_GEN_DIR:-build/gen}
mkdir -p "$out" "$gen"
for entry in tables samples fm6_patches drumkits; do
    case "$entry" in
        tables) header=felucca_tables.h ;;
        samples) header=felucca_samples.h ;;
        fm6_patches) header=felucca_fm6.h ;;
        drumkits) header=felucca_drumkits.h ;;
    esac
    if [ ! -f "$gen/$header" ] || [ "tools/gen_$entry.py" -nt "$gen/$header" ]; then
        python3 "tools/gen_$entry.py" "$gen/$header"
    fi
done
"${CC:-cc}" -O2 -fwrapv -fno-strict-aliasing -Wall -Wno-unused-function \
    -I"$gen" -Ifirmware/src -o "$out/jam_palette_test" tests/jam_palette_test.c -lm
"$out/jam_palette_test" "$out/dusty-demo.wav"
