#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Experimental host tests only. No device compiler, flash hooks, or publication.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-recovery-tests
gen=${JAM_GEN_DIR:-build/gen}
compiler=${CC:-cc}
mkdir -p "$out"
if [ ! -f "$gen/felucca_tables.h" ]; then
    echo 'Generate firmware assets before running the instrument fixture.' >&2
    exit 2
fi
python3 tools/check_jam_recovery_identity.py --gen-dir "$gen"
for unit in codec store; do
    "$compiler" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
        "tests/jr_${unit}_test.c" firmware/experimental/recovery/jr_codec.c \
        firmware/experimental/recovery/jr_store.c -o "$out/$unit"
done
"$compiler" -std=c11 -O2 -fwrapv -fno-strict-aliasing -Wno-int-to-pointer-cast \
    -I"$gen" -Ifirmware/src -Ifirmware/hal \
    tests/jr_instrument_test.c firmware/experimental/recovery/jr_codec.c \
    -o "$out/instrument" -lm
"$out/codec"
"$out/instrument"
"$out/instrument" --emit "$out/session-a.payload" 1
"$out/instrument" --emit "$out/session-b.payload" 2
"$out/store" "$out/session-a.payload" "$out/session-b.payload"

# Every command is a new OS process. Only the file-backed NOR survives.
# Use a fresh path for this run; preserve previous artifacts at other paths.
flash="$out/cold-boot-flash.bin"
rm -f "$flash"
"$out/store" --put "$flash" "$out/session-a.payload" initialize "$out/save-a.txt"
"$out/store" --get "$flash" "$out/recovered-a.payload"
"$out/instrument" --verify "$out/recovered-a.payload" 1
cmp "$out/session-a.payload" "$out/recovered-a.payload"
"$out/store" --put "$flash" "$out/session-b.payload" checkpoint "$out/save-b.txt"
"$out/store" --get "$flash" "$out/recovered-b.payload"
"$out/instrument" --verify "$out/recovered-b.payload" 2
cmp "$out/session-b.payload" "$out/recovered-b.payload"
echo 'Experimental recovery: codec, instrument, NOR interruption, and fresh-process checks passed.'
