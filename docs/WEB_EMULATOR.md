# JamBoxFM browser instrument

The browser instrument uses the FM-1's controls: 27 note keys, 14 buttons,
seven encoders and a master level control. Its 240 × 240 screen is drawn by
the C firmware. One WebAssembly instance runs both the firmware UI and the
audio engine in an AudioWorklet. This is a host port of the instrument,
not an emulation of the AC791 chip or a hardware timing test.

Press **Start audio**, then **PLAY** to hear the Dusty session. Turn
**SELECT** to change part; **PRESETS** changes sound. Drag or scroll a knob,
or focus it and use the arrow keys. The page lists the computer keyboard
shortcuts. **All notes off** releases notes and stops playback. Browser
sessions are temporary: reloading loses edits and recorded notes. This
page does not install firmware or access an attached FM-1.

The Dusty session starts Lead in **SNAP** mode: every key plays a note in
the chosen scale, including the black keys. Neighboring keys can share a
pitch. To change this, select Lead, tap **SEL**, and adjust **QNT** with
the third small knob: **OFF** plays normal keyboard notes, **SNAP** stays
in scale, and **WHITE** uses only white keys. In Chords mode, white keys
play chords and black keys modify a chord while a white key is held.

## Build and package

From the repository root, with Python 3, Pillow and Emscripten on PATH:

```sh
python tools/package_jam_emu.py
```

The package command rebuilds `tools/build_jam_emu.py` first. In this prepared
cloud environment, activate the installed dependencies first:

```sh
. tools/cloud/activate.sh
python tools/package_jam_emu.py
```

The tested compiler is Emscripten 3.1.69. Python package pins are in
`tools/cloud/python-requirements.txt`. To prepare the same cloud toolchain,
follow `docs/CLOUD_SETUP.md`; a local setup can instead install Emscripten
and these Python requirements using its usual package manager. Building the
browser requires neither the JieLi SDK nor its device compiler.

Run the emulator's native contracts, build and real Chromium checks with:

```sh
. tools/cloud/activate.sh
sh tests/run_jam_emu_tests.sh
```

The native suite exercises 27 contracts, including physical key/button
release order, LCD drawing and recording. Chromium exercises actual
AudioWorklet sound, panel controls, the live screen and desktop/mobile
layout. After packaging, `node tests/jam_emu_browser.mjs` also exercises
the standalone HTML while blocking every network request after its initial
document. That check verifies embedded runtime independence; it does not
establish that a browser permits `file://` navigation.

Outputs:

| Path | Purpose |
| --- | --- |
| `build/jam-emu/` | Development page, CSS, scripts and compiled WASM |
| `build/jambox-emulator.html` | Single file with embedded runtime, source ZIP and license notices |
| `build/jambox-source.zip` | Corresponding source and build inputs for this browser build |
| `build/jambox-web/` | Isolated static publication root; its index opens `emu/` |
| `build/jambox-web.zip` | The publication root, including the single file, source and notices |

The single file makes no runtime asset downloads: WASM is embedded as
base64 and the worklet is loaded from a Blob. It still needs a browser
that permits WebAssembly, AudioWorklet and audio started by a user gesture.
Opening a downloaded HTML directly depends on the browser's local-file
policy. The managed Chromium in this cloud environment rejects `file://`
navigation with `ERR_BLOCKED_BY_ADMINISTRATOR`, so direct double-click
operation could not be validated here. Serve the package over HTTPS when
local-file execution is unavailable. AudioWorklet requires a secure context;
ordinary remote HTTP hosting is insufficient.

## Publish the static site

Publish **the contents of `build/jambox-web/`** as a static HTTPS site.
The [GitHub Pages workflow](GITHUB_PAGES.md) uses a dedicated `gh-pages`
branch in the public [anthonysprints/jamboxfm repository](https://github.com/anthonysprints/jamboxfm)
and the existing cloud Git connection for repeatable updates.
All runtime URLs are relative, so a repository subdirectory works. The
target address is `https://anthonysprints.github.io/jamboxfm/emu/`.

Do not select `docs/` as a Pages source. The source branch excludes Sloop's
original generated installer and firmware; older workspaces may retain them.
The isolated publication root contains only the JamBox browser instrument,
its source and notices.
Packaging creates files locally; it does not push, enable Pages or publish
a URL. Confirm the Pages deployment and load the published HTML, JavaScript
and WASM before reporting that the site is live.

Keep the source download and license files beside any published binary.
The standalone HTML carries its own corresponding-source download and
notices so they remain available when the HTML is shared by itself.

## Corresponding source and assets

The source ZIP includes firmware, host adapters, browser code, generators,
tests, build helpers and pinned dependency manifests. It also includes all
CC0 samples needed by the generators, Terminus fonts and their license,
and icon **identifiers**. It omits the unused restricted icon bitmap and
panel photograph, generated binaries/caches, Git metadata and the inherited
deployed installer. The browser build disables parameter icon artwork.

Extract `jambox-source.zip`, enter `jamboxfm-source/`, install the build
dependencies above and run `python tools/build_jam_emu.py` to rebuild the
browser. `SOURCE-MANIFEST.json` records SHA-256 hashes of every included
source file. The publication directory also has `SHA256SUMS` covering its
runtime, standalone HTML, source archive and notices.

The overall code license is GPL-3.0-only. Retained notices cover msfa
(Apache-2.0), physical-model DSP (MIT), Terminus (SIL OFL), the inherited
Fukiai font (MIT), and samples (CC0, with provenance). See `LICENSING.md`
for the upstream attribution and excluded artwork terms.
