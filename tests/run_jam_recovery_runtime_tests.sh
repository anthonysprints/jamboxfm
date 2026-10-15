#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Actual recovery integration with a NOR double. No physical device access.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-recovery-tests
gen=${JAM_GEN_DIR:-build/gen}
compiler=${CC:-cc}
mkdir -p "$out"
python3 tools/check_jam_recovery_identity.py --gen-dir "$gen"
"$compiler" -std=c11 -O2 -fwrapv -fno-strict-aliasing -Wno-int-to-pointer-cast \
    -I"$gen" -Ifirmware/src -Ifirmware/hal \
    tests/jam_recovery_runtime_test.c -o "$out/runtime" -lm
"$out/runtime"
