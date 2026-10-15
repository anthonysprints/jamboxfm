#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Native UI routing/confirmation checks. No device compiler or flash access.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-recovery-tests
gen=${JAM_GEN_DIR:-build/gen}
compiler=${CC:-cc}
mkdir -p "$out"
if [ ! -f "$gen/felucca_tables.h" ] || [ ! -f "$gen/felucca_icons.h" ]; then
    echo 'Generate firmware assets before running the recovery UI fixture.' >&2
    exit 2
fi
"$compiler" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -fwrapv -fno-strict-aliasing -w \
    -I"$gen" -Ifirmware/src -Ifirmware/hal \
    tests/jam_recovery_ui_test.c -o "$out/ui" -lm
"$out/ui"
