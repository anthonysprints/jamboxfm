#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Activate tools/cloud/activate.sh first. Device compiler and hardware not required.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
command -v "$CC" >/dev/null || { echo 'Native Clang is required; source tools/cloud/activate.sh' >&2; exit 1; }
export CC
python3 tools/build_jam_preview.py
sh tests/run_jam_palette_tests.sh
python3 tools/gen_font.py build/gen/felucca_font.h
python3 tools/gen_icons.py build/gen/felucca_icons.h
python3 tools/gen_logo.py build/gen/sloop_logo.h
mkdir -p build/jam-tests/ui build/host
"$CC" -O1 -fwrapv -fno-strict-aliasing -w -Ibuild/gen -Ifirmware/src \
    tests/jam_ui_test.c -o build/jam-tests/jam_ui_test -lm
build/jam-tests/jam_ui_test build/jam-tests/ui
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -Ibuild/gen -Ifirmware/src \
    tests/jam_preview_native.c -o build/host/jam_preview_native
build/host/jam_preview_native
node tests/jam_preview_test.mjs
echo 'JamBox palette, Home, and native/WASM contracts passed.'
