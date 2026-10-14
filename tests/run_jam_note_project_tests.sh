#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Existing generated headers only; no device compiler, network, or hardware access.
set -eu
cd "$(dirname "$0")/.."
gen=${JAM_GEN_DIR:-build/gen}
out=build/jam-note-project-tests
if [ ! -f "$gen/felucca_tables.h" ]; then
    echo 'Generate firmware assets before running the project/editor tests.' >&2
    exit 1
fi
mkdir -p "$out"
"${CC:-cc}" -O2 -fwrapv -fno-strict-aliasing -Wno-int-to-pointer-cast \
    -I"$gen" -Ifirmware/src -Ifirmware/hal tests/jam_note_project_test.c -o "$out/native" -lm
"$out/native"
node tests/jam_note_editor_test.mjs
