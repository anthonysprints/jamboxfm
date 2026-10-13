#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the dedicated JamBox preview; never overwrite Sloop's preview assets.

Requires Emscripten on PATH. Output: build/jam-preview/{index.html,*.js,*.css,*.wasm}.
No JieLi SDK, browser package manager, hardware, or firmware flashing required.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "build/gen"
OUT = ROOT / "build/jam-preview"


def main():
    emcc = os.environ.get("EMCC") or shutil.which("emcc")
    if not emcc:
        sys.exit("Emscripten is required: put emcc on PATH before building the JamBox preview.")
    GEN.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    for script, header in [("gen_tables.py", "felucca_tables.h"), ("gen_samples.py", "felucca_samples.h"),
                           ("gen_drumkits.py", "felucca_drumkits.h"), ("gen_fm6_patches.py", "felucca_fm6.h")]:
        subprocess.run([sys.executable, str(ROOT / "tools" / script), str(GEN / header)], cwd=ROOT, check=True)
    exports = ["preview_frames", "preview_rate", "preview_render", "jam_preview_init", "jam_preview_key",
               "jam_preview_release", "jam_preview_panic", "jam_preview_select", "jam_preview_play", "jam_preview_toggle_play", "jam_preview_record",
               "jam_preview_tempo", "jam_preview_swing", "jam_preview_octave", "jam_preview_mute",
               "jam_preview_sound", "jam_preview_setting", "jam_preview_drum_step", "jam_preview_value",
               "jam_preview_pad_name", "jam_preview_lane_name", "jam_role_track", "jam_role_name",
               "jam_sound_name", "jam_sound_count", "jam_sound_index", "jam_macro_set", "jam_macro_get",
               "jam_macro_name"]
    subprocess.run([emcc, str(ROOT / "firmware/host/jam_preview.c"), "-O2", "-fwrapv", "-fno-strict-aliasing",
                    "-Wno-unused-function", "-Wno-int-to-pointer-cast", "-I" + str(GEN),
                    "-I" + str(ROOT / "firmware/src"), "--no-entry", "-sSTANDALONE_WASM=1", "-sFILESYSTEM=0",
                    "-sINITIAL_MEMORY=16777216", "-sSTACK_SIZE=131072",
                    "-sEXPORTED_FUNCTIONS=" + str(["_" + name for name in exports]),
                    "-o", str(OUT / "jam-preview.wasm")], cwd=ROOT, check=True)
    for source in (ROOT / "web/jam").iterdir():
        if source.is_file() and source.suffix in (".html", ".js", ".css"):
            shutil.copy2(source, OUT / source.name)
    print("JamBox browser preview built:", OUT)


if __name__ == "__main__":
    main()
