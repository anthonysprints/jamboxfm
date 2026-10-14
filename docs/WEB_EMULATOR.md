# JamBoxFM browser instrument

Current follow-up: [independent POLY note lengths](INDEPENDENT_NOTES.md) preserve staggered held notes in grid recording. Native/browser checks and the FM-1 build passed; installer verification and publication are pending. Bass remains LEGATO by default. The preceding [GROOVE controls](GROOVE_CONTROLS.md) update is recorded in its [build record](build-evidence/groove-controls.md).

The browser instrument uses the FM-1's controls: 27 note keys, 14 buttons,
seven encoders and a master level control. Its 240 × 240 screen is drawn by
the C firmware. One WebAssembly instance runs both the firmware UI and the
audio engine in an AudioWorklet. This is a host port of the instrument,
not an emulation of the AC791 chip or a hardware timing test.

Press **Start audio**, then **PLAY** to hear the Dusty session. Turn
**ALGORITHM** to change part; **PRESETS** changes sound. Use the top-right
**SELECT** knob on Home to change tempo (one
BPM per detent, from 40 to 240 BPM). Deeper pages retain their existing controls.
Drag or scroll a knob, or focus it and use the arrow keys. The page lists the computer keyboard
shortcuts. **All notes off** releases notes and stops playback. Browser
sessions are temporary: reloading loses edits and recorded notes. This
page does not install firmware or access an attached FM-1.

The two-view Home update is implemented, with native/browser checks and the
updater-enabled FM-1 build passing. Publication is verified; physical audition remains
pending in the [Home views build record](build-evidence/home-views.md).
It starts in **GROOVE**, with four rows of actual pattern steps. Tap **HOME**
while already on Home to switch to **SOUND**, with larger sound controls; tap
again to return. From a sound, scale or sequencer page, HOME returns to the
view you last used. A short HOME tap closes settings to that remembered view;
holding HOME retains settings access. Recording overlays and held-layer
shortcuts keep their existing priority.

Both Home views use SELECT for BPM, ALGORITHM for part, PRESETS for sound and
knobs 1–4 for GROOVE track controls or SOUND macros. The keyboard keeps playing notes/chords
or drums. GROOVE's rows each show a literal 16-step window with that part's
range and length; playback follows each part's own playhead, and stopping
retains the displayed windows. The sound and macros belong to the selected
part. For step editing, hold SEQ; hold SEQ and tap HOME to lock its editor.
See [Two Home views](HOME_VIEWS_PLAN.md) for the display and navigation rules.

The Dusty session starts Chords and Lead in **SNAP** mode: every key works,
including the black keys. Chords starts with **7TH** chord building; Lead
starts with **CHORD OFF** for single notes. Neighboring keys can share a root.
Select a melodic part and tap **SEL**. If SEL 2 opens, turn **SELECT** one click
left to reach the first SEL page. Use small knobs 1–4 for **ROOT**,
**SCL**, **QNT** and **CHORD**. QNT **OFF** uses the printed notes; **SNAP**
rounds out-of-scale roots down into the scale; **WHITE** maps white keys to
scale degrees and reserves black keys for chord modifiers when CHORD is on.
**MAJOR**, **MINOR** and **DIM** build a chosen shape from the root; **TRIAD**
and **7TH** choose their notes from the scale. Enabling CHORD sets POLY voices
so Lead and Bass can sound the full chord; turning it off leaves POLY selected.
From SEL's first page, turn SELECT one click for SEL 2, then use small knob 2
for STRUM and knob 3 for INVERT (ROOT/AUTO/+1/+2/+3). See the
[chord builder guide](CHORD_BUILDER.md) for examples, lights and the remaining
planned Assist features.

Keyboard lights show the keys you press plus all notes playing on the selected
part, including every sounding chord tone and simultaneous sequencer/MIDI notes.
In single-note SNAP, an out-of-scale key lights both the pressed key and its
corrected note. Sounding lights use printed note positions in the selected
octave; they do not suggest other keys that happen to map to the same note.
Notes outside the visible range are not wrapped. Quick taps leave a brief flash;
effect tails do not keep note lights on. This October 14 lighting follow-up
supersedes the original chord build's root-only display. Software checks and
publication are tracked in the [lighting build record](build-evidence/sounding-note-lights.md).

## Independent POLY recording follow-up

The [current recording increment](INDEPENDENT_NOTES.md) lets each POLY note
keep its own grid length. Hold one note, add another later, then release them
separately: replay should preserve that overlap. It also applies to overdubs
and free-takes. Bass/Lead keep their LEGATO defaults, CHORD remains a separate
choice, and the existing four-onsets-per-step/eight-shared-synth-voice limits
remain. No extra overlap setting is required.

The browser uses the same C recorder and release scheduling as the FM-1;
JavaScript does not schedule a parallel performance. Shared C integration, native checks, actual WASM/AudioWorklet/browser checks
and the FM-1 build passed. Publication remains pending. See the [follow-up build
record](build-evidence/independent-notes.md) for the finalized result before
assuming the public emulator has changed. Browser reload and device power-off
still lose the temporary session; permanent saving is disabled.

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

The native suite exercises physical key/button release order, LCD drawing,
recording, black-key chord roots, chord lights, automatic polyphony and
inversion controls. Build-specific counts belong in the linked build records.
Chromium exercises actual AudioWorklet sound, panel controls, the live screen
and desktop/mobile
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
