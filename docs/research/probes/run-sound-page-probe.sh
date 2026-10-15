#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Research only. Activate tools/cloud/activate.sh first. No target/device access.
set -eu
cd "$(dirname "$0")/../../.."
out=build/sound-page-research
gen=${JAM_GEN_DIR:-build/gen}
mkdir -p "$out/gen" "$out/screens"
python3 tools/gen_font.py "$out/gen/felucca_font.h"
python3 tools/gen_icons.py "$out/gen/felucca_icons.h"
python3 tools/gen_logo.py "$out/gen/sloop_logo.h"
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -fwrapv -fno-strict-aliasing -w \
 -I"$out/gen" -I"$gen" -Ifirmware/src -Ifirmware/hal \
 docs/research/probes/sound-page-probe.c -o "$out/probe" -lm
"$out/probe" > "$out/inventory.tsv"
"$out/probe" --audio > "$out/audio.tsv"
"$out/probe" --routes > "$out/routes.txt"
