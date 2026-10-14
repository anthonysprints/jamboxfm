#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Identity, query bounds and splash without a package, device or target compiler.
set -eu
cd "$(dirname "$0")/.."
out=${JAM_IDENTITY_OUT:-build/jam-identity}
gen=${JAM_GEN_DIR:-build/gen}
mkdir -p "$out" "$gen"
if [ ! -f "$gen/felucca_font.h" ] || [ tools/gen_font.py -nt "$gen/felucca_font.h" ]; then
    python3 tools/gen_font.py "$gen/felucca_font.h"
fi
for mode in 0 1 2 3; do
    for unit in jam_identity_test jam_identity_usb_test; do
        "${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-int-to-pointer-cast \
            -fsanitize=address,undefined -fno-sanitize-recover=all -DTEST_MODE="$mode" \
            -o "$out/$unit-$mode" "tests/$unit.c"
        "$out/$unit-$mode"
    done
done
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function \
    -fsanitize=address,undefined -fno-sanitize-recover=all -I"$gen" \
    -o "$out/jam_splash_test" tests/jam_splash_test.c
"$out/jam_splash_test" "$out/jam-splash.ppm"
