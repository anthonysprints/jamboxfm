#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the physical JamBox browser emulator, its notices and source.

The default rebuilds first. Nothing is uploaded or published. The standalone
HTML includes both runtime data and a download of its corresponding source.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import html
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
EMU = BUILD / "jam-emu"
SITE = BUILD / "jambox-web"
RUNTIME_FILES = ("index.html", "style.css", "app.js", "worklet.js", "jam-emu.wasm")
# The device tool remains read-only. A separate experimental installer is
# packaged only with --with-trial, after verifying the device build again.
DEVICE_FILES = {
    "device/index.html": "web/device/index.html",
    "device/style.css": "web/device/style.css",
    "device/app.js": "web/device/app.js",
    "fm1ota.js": "web/fm1ota.js",
    "fm1pkg.js": "web/fm1pkg.js",
    "jam-device-policy.js": "web/jam-device-policy.js",
}
SOURCE_SUFFIXES = {".c", ".h", ".S", ".ld", ".py", ".json", ".sh", ".ps1", ".bat",
                   ".js", ".mjs", ".html", ".css", ".md", ".txt", ".ttf"}
SOURCE_ROOTS = ("firmware", "tools", "tests", "web", "LICENSES")
ROOT_FILES = ("README.md", "BUILDING.md", "LICENSING.md", "LICENSE", "PRODUCT_BRIEF.md",
              "SLOOP.md", "GUIDE.md", "DEMARRAGE-RAPIDE-FR.md", "build.sh", "build-sloop.ps1",
              "INSTALL-SLOOP.bat", "OPEN-EDITOR.bat", ".gitignore", ".gitattributes")
NOTICES = {
    "LICENSE": "LICENSE",
    "LICENSING.md": "LICENSING.md",
    "LICENSES/Apache-2.0-msfa.txt": "licenses/Apache-2.0-msfa.txt",
    "assets/fonts/Terminus-LICENSE.txt": "licenses/Terminus-LICENSE.txt",
    "assets/samples-cc0/ATTRIBUTION.txt": "licenses/CC0-ATTRIBUTION.txt",
    "assets/samples-cc0/CREDITS.txt": "licenses/CC0-CREDITS.txt",
    "web/FUKIAI-LICENSE.txt": "licenses/FUKIAI-LICENSE.txt",
}
BLOCKED_PARTS = {".git", "node_modules", "__pycache__", ".cache", ".venv", "data", "private"}


def source_files() -> list[Path]:
    """Select source explicitly; never scoop up the checkout or deployed docs."""
    files = {ROOT / name for name in ROOT_FILES}
    for directory in SOURCE_ROOTS:
        files.update(path for path in (ROOT / directory).rglob("*")
                     if path.suffix in SOURCE_SUFFIXES)
    for directory in ("assets/fonts", "assets/samples-cc0"):
        files.update((ROOT / directory).rglob("*"))
    files.add(ROOT / "assets/icons.json")
    files.update((ROOT / "docs").glob("*.md"))
    files.update((ROOT / "docs/upstream").glob("*.md"))
    files.update((ROOT / "docs/upstream").glob("*.json"))
    selected = []
    for path in sorted(files):
        relative = path.relative_to(ROOT)
        if set(relative.parts) & BLOCKED_PARTS or path.is_symlink() or not path.is_file():
            continue
        if path.name.startswith(".") and path.name not in (".gitignore", ".gitattributes"):
            continue
        selected.append(path)
    for required in ("firmware/host/jam_emu.c", "tools/build_jam_emu.py", "tools/package_jam_emu.py",
                     "web/emu/app.js", "web/emu/worklet.js", "docs/WEB_EMULATOR.md"):
        if ROOT / required not in selected:
            raise RuntimeError(f"Required corresponding source is missing: {required}")
    return selected


def make_zip(entries: dict[str, bytes]) -> bytes:
    """Stable ordering, timestamp and permissions make repeat builds comparable."""
    result = io.BytesIO()
    with zipfile.ZipFile(result, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (0o100755 if name.endswith(".sh") else 0o100644) << 16
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    return result.getvalue()


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def script_json(value) -> str:
    # HTML raw-text parsing happens before JavaScript string parsing.
    return json.dumps(value, ensure_ascii=True, separators=(",", ":")).replace("<", "\\u003c")


def replace_one(pattern: str, replacement: str, text: str, name: str) -> str:
    updated, count = re.subn(pattern, lambda _: replacement, text, flags=re.IGNORECASE)
    if count != 1:
        raise RuntimeError(f"Expected one {name} reference; found {count}")
    return updated


def add_source_footer(page: str, standalone: bool, notices: dict[str, bytes], with_trial: bool = False) -> str:
    source_link = "#" if standalone else "../jambox-source.zip"
    if standalone:
        licenses = "".join(f"<details><summary>{html.escape(name)}</summary><pre style=\"white-space:pre-wrap\">"
                           f"{html.escape(data.decode('utf-8'))}</pre></details>" for name, data in sorted(notices.items()))
        license_link = f"<details><summary>Licenses and credits</summary>{licenses}</details>"
    else:
        license_link = '<a href="../LICENSES.html">Licenses and credits</a>'
    device_link = '' if standalone else ' <span aria-hidden="true">·</span> <a href="../device/">Check your FM-1</a>'
    trial_link = (' <span aria-hidden="true">·</span> <a href="../install/">Install experimental JamBoxFM</a>'
                  if with_trial and not standalone else '')
    footer = (f'<aside aria-label="Source and licenses" style="max-width:70rem;margin:0 auto;padding:0 1rem 2rem;font:12px/1.5 Arial,sans-serif">'
              f'<a id="source-download" href="{source_link}" download="jambox-source.zip">Download corresponding source</a>'
              f' <span aria-hidden="true">·</span> {license_link}{device_link}{trial_link}</aside>\n')
    return replace_one(r"</body\s*>", footer + "</body>", page, "closing body")


def standalone_html(runtime: dict[str, bytes], source_zip: bytes, notices: dict[str, bytes]) -> bytes:
    page = runtime["index.html"].decode("utf-8")
    css, app, worklet = (runtime[name].decode("utf-8") for name in ("style.css", "app.js", "worklet.js"))
    if re.search(r"</style\b", css, re.IGNORECASE) or re.search(r"</script\b", app, re.IGNORECASE):
        raise RuntimeError("Inline asset contains an HTML closing tag; use escaped source text.")
    for name, source in (("app.js", app), ("worklet.js", worklet)):
        if re.search(r"^\s*(?:import|export)\s", source, re.MULTILINE):
            raise RuntimeError(f"Standalone {name} must not depend on module imports/exports.")
    if "JAMBOX_EMBEDDED" not in app:
        raise RuntimeError("app.js must support globalThis.JAMBOX_EMBEDDED before packaging.")
    page = replace_one(r'<link\b(?=[^>]*\brel=[\"\']stylesheet[\"\'])(?=[^>]*\bhref=[\"\'](?:\./)?style\.css[\"\'])[^>]*>',
                       "<style>\n" + css + "\n</style>", page, "style.css")
    payload = {"wasm": base64.b64encode(runtime["jam-emu.wasm"]).decode("ascii"), "worklet": worklet}
    bootstrap = "<script>globalThis.JAMBOX_EMBEDDED=" + script_json(payload) + ";</script>\n"
    page = replace_one(r'<script\b(?=[^>]*\bsrc=[\"\'](?:\./)?app\.js[\"\'])[^>]*>\s*</script\s*>',
                       bootstrap + '<script type="module">\n' + app + "\n</script>", page, "app.js")
    # Its source travels with the single file, even when it is shared alone.
    page = add_source_footer(page, True, notices)
    download = ('<script type="module">\nconst bytes = Uint8Array.from(atob(' +
                script_json(base64.b64encode(source_zip).decode("ascii")) +
                '), char => char.charCodeAt(0));\ndocument.getElementById("source-download").href = '
                'URL.createObjectURL(new Blob([bytes], {type:"application/zip"}));\n</script>\n')
    page = replace_one(r"</body\s*>", download + "</body>", page, "closing body")
    # A local standalone page must not jump to a containing directory.
    page = page.replace('href="./"', 'href="#page-title"')
    return page.encode("utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true", help="package existing emulator build (for validation only)")
    parser.add_argument("--with-trial", action="store_true", help="include the experimental install/update page and verified device candidate")
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")))
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")))
    args = parser.parse_args()
    if not args.no_build:
        subprocess.run([sys.executable, str(ROOT / "tools/build_jam_emu.py")], cwd=ROOT, check=True)
    runtime = {name: (EMU / name).read_bytes() for name in RUNTIME_FILES}
    if not runtime["jam-emu.wasm"].startswith(b"\x00asm"):
        raise RuntimeError("Emulator WASM has an invalid header.")
    # Prevent packaging an outdated front end beside today's source.
    for name in RUNTIME_FILES[:-1]:
        if runtime[name] != (ROOT / "web/emu" / name).read_bytes():
            raise RuntimeError(f"Built {name} is stale; rebuild without --no-build.")
    trial_entries = {}
    if args.with_trial:
        # This repeats the canonical ELF/package/toolchain checks and exports
        # source from today's checkout. Never trust a stale candidate directory.
        import package_jam_device
        from publish_jam_web import TRIAL_BUNDLE_FILES, TRIAL_RUNTIME_FILES, validate_trial
        candidate = package_jam_device.package(toolchain=args.toolchain.resolve(), sdk=args.sdk.resolve())
        trial_entries = {"firmware/" + name: (package_jam_device.OUTPUT / name).read_bytes()
                         for name in TRIAL_BUNDLE_FILES}
        trial = {"schema_version": 2, "kind": "experimental-install", "hardware_validated": False,
            "transitions": ["factory-v15-to-jambox", "jambox-normal-to-jambox"],
            "manifest_sha256": sha(trial_entries["firmware/manifest.json"]),
            "firmware_sha256": candidate["firmware"]["sha256"]}
        trial_entries["firmware/trial.json"] = (json.dumps(trial, indent=2, sort_keys=True) + "\n").encode()
        trial_entries.update({name: (ROOT / "web" / name).read_bytes() for name in TRIAL_RUNTIME_FILES})
        validate_trial(trial_entries)
    source_entries = {"jamboxfm-source/" + path.relative_to(ROOT).as_posix(): path.read_bytes()
                      for path in source_files()}
    source_hashes = {name.removeprefix("jamboxfm-source/"): sha(data) for name, data in source_entries.items()}
    source_entries["jamboxfm-source/SOURCE-MANIFEST.json"] = (json.dumps(source_hashes, indent=2, sort_keys=True) + "\n").encode()
    source_zip = make_zip(source_entries)
    notices = {target: (ROOT / original).read_bytes() for original, target in NOTICES.items()}
    # Preserve the MIT notices from the DSP source directly alongside the binary.
    for name in ("phys_dsp.c", "phys_symp.c"):
        text = (ROOT / "firmware/src" / name).read_text()
        notices["licenses/" + name + "-notice.txt"] = (text[:text.index("*/") + 2] + "\n").encode()
    standalone = standalone_html(runtime, source_zip, notices)
    site_entries = {"emu/" + name: data for name, data in runtime.items()}
    site_entries["emu/index.html"] = add_source_footer(runtime["index.html"].decode(), False, notices, args.with_trial).encode()
    site_entries.update({target: (ROOT / source).read_bytes() for target, source in DEVICE_FILES.items()})
    site_entries.update(notices)
    site_entries.update(trial_entries)
    site_entries["index.html"] = ("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
                                  "<meta http-equiv=\"refresh\" content=\"0;url=emu/\"><title>JamBoxFM</title>"
                                  "<a href=\"emu/\">Open the JamBoxFM instrument</a></html>\n").encode()
    site_entries[".nojekyll"] = b""
    site_entries["jambox-emulator.html"] = standalone
    site_entries["jambox-source.zip"] = source_zip
    site_entries["WEB_EMULATOR.md"] = (ROOT / "docs/WEB_EMULATOR.md").read_bytes()
    license_page = ('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">'
                    '<title>JamBoxFM licenses</title><body style="max-width:70rem;margin:2rem auto;padding:0 1rem;font:14px/1.6 system-ui">'
                    '<a href="emu/">Back to the instrument</a><h1>Licenses and credits</h1>'
                    '<p>JamBoxFM is GPL-3.0-only software based on Sloop / Felucca. '
                    '<a href="jambox-source.zip" download>Download the corresponding source</a>.</p>' +
                    "".join(f"<h2>{html.escape(name)}</h2><pre style=\"white-space:pre-wrap\">{html.escape(data.decode())}</pre>"
                            for name, data in sorted(notices.items())) + "</body></html>\n")
    if args.with_trial:
        trial_credits = ('<h2>Experimental FM-1 firmware</h2><p>The device firmware also contains '
            'JieLi AC79 SDK components under Apache-2.0. '
            '<a href="firmware/licenses/AC79-SDK-LICENSE.txt">SDK license</a> · '
            '<a href="firmware/licenses/AC79-SDK-README.md">SDK source and component notes</a> · '
            '<a href="firmware/jamboxfm-source.zip" download>Corresponding firmware source</a>.</p>')
        license_page = license_page.replace("</body>", trial_credits + "</body>")
    site_entries["LICENSES.html"] = license_page.encode()
    manifest = {name: sha(data) for name, data in sorted(site_entries.items())}
    site_entries["SHA256SUMS"] = "".join(f"{digest}  {name}\n" for name, digest in manifest.items()).encode()
    # Validate before replacing the previous site. The publisher also performs
    # these checks independently before constructing any deployment commit.
    from publish_jam_web import EXPECTED_FILES, TRIAL_FILES, validate_source
    if set(site_entries) != (TRIAL_FILES if args.with_trial else EXPECTED_FILES):
        raise RuntimeError("Generated site differs from the explicit publication allowlist.")
    validate_source(source_zip, site_entries)
    # This fixed output directory is generated solely by this packaging command.
    if SITE.exists():
        shutil.rmtree(SITE)
    for name, data in site_entries.items():
        destination = SITE / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    for name, data in (("jambox-emulator.html", standalone), ("jambox-source.zip", source_zip),
                       ("jambox-web.zip", make_zip(site_entries))):
        destination = BUILD / name
        destination.write_bytes(data)
        print(f"{destination.relative_to(ROOT)}: {len(data):,} bytes; SHA256 {sha(data)}")
    print(f"Static publish root: {SITE.relative_to(ROOT)} ({len(site_entries)} files)")


if __name__ == "__main__":
    main()
