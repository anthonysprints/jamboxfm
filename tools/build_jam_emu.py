#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the real JamBox C UI and audio into build/jam-emu/jam-emu.wasm.

Requires Emscripten and Pillow. The private generated-header directory avoids
changing firmware/preview build outputs. Parameter icon artwork is excluded.
"""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "build/jam-emu-gen"
OUT = ROOT / "build/jam-emu"


def main():
    emcc = os.environ.get("EMCC") or shutil.which("emcc")
    if not emcc:
        sys.exit("Emscripten is required: put emcc on PATH before building the JamBox emulator.")
    GEN.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    # Match the host's disabled optional SLICE engine, regardless of a caller's
    # device-build environment. Its optional demo material is not packaged.
    build_env = dict(os.environ, FELUCCA_SLICE="0")
    for script, header in [("gen_tables.py", "felucca_tables.h"), ("gen_samples.py", "felucca_samples.h"),
                           ("gen_drumkits.py", "felucca_drumkits.h"), ("gen_fm6_patches.py", "felucca_fm6.h"),
                           ("gen_font.py", "felucca_font.h")]:
        subprocess.run([sys.executable, str(ROOT / "tools" / script), str(GEN / header)],
                       cwd=ROOT, env=build_env, check=True)
    # icons.c still needs the identifiers with FELUCCA_ICONS=0. No bitmap is
    # loaded, embedded, generated or copied into this emulator's output.
    metadata = json.loads((ROOT / "assets/icons.json").read_text())
    names = metadata["names"]
    ids = ["ICON_" + re.sub(r"[^A-Z0-9]", "_", name.upper()) for name in names]
    (GEN / "felucca_icons.h").write_text(
        "/* Icon identifiers only; artwork disabled for JamBox emulator. */\n#pragma once\n"
        "#define ICON_CELL 12\n#define FELUCCA_ICONS_N 0\nenum {\n    " +
        ",\n    ".join(ids + ["ICON_COUNT"]) + "\n};\n")
    exports = ["jam_emu_init", "jam_emu_frames", "jam_emu_rate", "jam_emu_render", "jam_emu_frame",
               "jam_emu_button", "jam_emu_encoder", "jam_emu_key", "jam_emu_master", "jam_emu_release",
               "jam_emu_panic", "jam_emu_screen", "jam_emu_screen_version", "jam_emu_leds",
               "jam_emu_value", "jam_emu_sound_name"]
    subprocess.run([emcc, str(ROOT / "firmware/host/jam_emu.c"), "-O2", "-fwrapv", "-fno-strict-aliasing",
                    "-Wno-unused-function", "-Wno-int-to-pointer-cast", "-I" + str(GEN),
                    "-I" + str(ROOT / "firmware/src"), "--no-entry", "-sSTANDALONE_WASM=1", "-sFILESYSTEM=0",
                    "-sINITIAL_MEMORY=16777216", "-sSTACK_SIZE=131072",
                    "-sEXPORTED_FUNCTIONS=" + str(["_" + name for name in exports]),
                    "-o", str(OUT / "jam-emu.wasm")], cwd=ROOT, env=build_env, check=True)
    frontend = ROOT / "web/emu"
    if frontend.is_dir():
        for source in frontend.iterdir():
            if source.is_file() and source.suffix in (".html", ".js", ".css"):
                shutil.copy2(source, OUT / source.name)
    print("JamBox panel emulator built:", OUT)


if __name__ == "__main__":
    main()
