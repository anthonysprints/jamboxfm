#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Compile the production firmware adapter natively; assets are built separately.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
if [ ! -f build/jam-emu-gen/felucca_tables.h ]; then
    echo 'Build emulator assets first: python3 tools/build_jam_emu.py' >&2
    exit 1
fi
mkdir -p build/jam-poly-duration-tests
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -Ibuild/jam-emu-gen -Ifirmware/src \
    tests/jam_poly_duration_test.c -o build/jam-poly-duration-tests/native -lm
build/jam-poly-duration-tests/native
