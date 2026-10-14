#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Generate only private UI headers; do not rebuild shared browser/device assets.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-clang}"
mkdir -p build/jam-duration-ui-tests/gen
python3 tools/gen_font.py build/jam-duration-ui-tests/gen/felucca_font.h
python3 tools/gen_icons.py build/jam-duration-ui-tests/gen/felucca_icons.h
python3 tools/gen_logo.py build/jam-duration-ui-tests/gen/sloop_logo.h
"$CC" -O1 -fwrapv -fno-strict-aliasing -w -Ibuild/jam-duration-ui-tests/gen -Ibuild/gen -Ifirmware/src \
    tests/jam_duration_ui_test.c -o build/jam-duration-ui-tests/native -lm
build/jam-duration-ui-tests/native
