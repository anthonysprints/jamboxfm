# Kit sound and effects: trials 03 / 04 and adopted controls

Adoption: the owner approved all drum changes through Trial 04 for `main` and
the normal installer. See [release notes](DRUM_RELEASE.md). The trial instructions
below describe the preceding validation flow.

This is stage 3 of [the drum workflow plan](DRUM_WORKFLOW_PLAN.md), developed on
`next/usability-and-saving` for the isolated `preview/drums/` installer. Trials
01–03 received positive owner feedback; Trial 04 adds the requested drum-bank
shortcut and needs its own FM-1 check. The normal installer, browser instrument
and saving preview stay separate.

The aim is straightforward: SEQ edits the beat, EDIT shapes the kit, and FX
opens effects that actually work on drums. This stage adds no individual-drum
recipe editor, storage format, automatic saving or SYN-kit overwrite operation.

## Ordinary drum controls

| Entry | Result | Four right knobs |
| --- | --- | --- |
| SEQ | GRID: named drum rows and step editing. Another tap switches drums 1–8 / 9–16. | Drum / Step / Strength / Repeat |
| EDIT | KIT SOUND: a named pad map; keys play the kit. Repeated taps keep KIT SOUND open. | Kit / Level / Tone / Pan |
| FX, from another family | KIT FX: effects for the whole drum part. | Filter / Echo / Room / Pan |

KIT SOUND's Tone and KIT FX's Filter control the same whole-kit filter, with
low-pass and high-pass directions around its neutral setting. Echo and Room are
the drum part's sends into the shared effects; they do not affect only the
selected pad. Level is the whole kit's volume, separate from a step's Strength.

In GRID, KIT SOUND and the drum FX pages, the left controls stay consistent:
MASTER controls output, SELECT changes BPM one per detent, PRESETS browses the
eight curated kits, and ALGORITHM selects Drums/Bass/Chords/Lead in Home's order.
Changing to another part returns Home. Held shortcuts and recording overlays
keep their own controls.

KIT SOUND's first knob offers the broader usable kit list: the 37 factory kits,
SYN1–4 and available user sample kits. Empty USR sources are skipped. SYN1–4 keep
their existing editable sounds; choosing a kit does not overwrite them or the
recorded beat. This is kit selection, not individual drum replacement.

Repeated FX taps cycle these pages:

| Page | Scope / purpose |
| --- | --- |
| KIT FX | Filter, echo send, room send and pan for Drums. |
| SLICER | The existing slicer for Drums. |
| INSERT | Choose and adjust the drum part's insert effect. |
| INSERT 2 | Adjust that insert's mix. |
| ALL ECHO | Shared echo settings; changes can affect every part using echo. |
| ALL ROOM | Shared room Size and Damp; changes can affect every part using room. |

The shared room page does not show ignored chorus controls on Drums. Synth parts
retain their own FX pages. Whole-kit controls and shared effects remain distinct
from a future individual-drum sound editor.

## Trial 04: quick drum-bank switching

From another screen, SEQ opens GRID with the current drum still selected. Inside
GRID, another ordinary SEQ tap swaps between drums 1–8 and 9–16, keeping the same
row position: drum 6 becomes drum 14, then back to drum 6 on the next tap. The
step cursor and 16-step window stay put. Switching banks changes no hits, mixer
settings, undo or recording state, and does not audition a sound. Knob 1 still
selects individual drums. Held SEQ and recorder overlays retain their controls.

## Helpful taps, preserved holds

Tapping ENV, LFO, SEL or ARP on Drums keeps the current workspace and gives brief
guidance instead of opening inactive synth controls. These messages do not
activate a new mode or change the sound. SEL's scale/chord page is for the melodic
parts; it has no ordinary drum setting. The synth ENV page is not yet connected
to individual drum editing. EDIT opens whole-kit controls, affecting all drums
together rather than only one kick or snare:


| Tap | Guidance |
| --- | --- |
| ENV | EDIT: WHOLE KIT SOUND |
| LFO | FX: EFFECTS + SLICER |
| SEL | DRUMS: PLAY PADS |
| ARP | HOLD ARP + PAD: ROLL |

Held FX, held EDIT erase/undo, held SEQ step gestures, held SEL's global song-key
controls and held ARP rolls remain available. GRID's selected drum, page memory,
hit editing and recording ownership are retained from Trial 02. HOME still
returns to the remembered GROOVE or SOUND view.

## Device check

Trial destination: https://anthonysprints.github.io/jamboxfm/preview/drums/install/.
Publication and browser checks are recorded separately from physical testing.

1. Use Save now and wait for Saved before updating. Keep the existing recovery
   folder; the installer checks the same local `backup.json` and
   `full-flash-before.bin`. These files do not save the current jam.
2. Connect one running JamBoxFM in desktop Chrome/Edge, close other MIDI apps,
   and use the isolated drum trial installer. Keep power and USB steady.
3. Confirm your saved jam and edited sounds returned. Do not reinitialize saving:
   the manual-saving format is unchanged and autosave is still off.
4. Select Drums and tap EDIT. Play pads, browse kits and try Level, Tone and Pan.
   EDIT should keep KIT SOUND open; SEQ should return directly to GRID. Confirm
   changing kits preserves the beat. While in GRID, tap SEQ to swap drum banks,
   then tap it again to return. Check a later step page stays on the same steps.
5. Tap FX and listen to Filter, Echo, Room and Pan. Cycle through SLICER and the
   INSERT pages. ALL ECHO and ALL ROOM are shared across the jam; try them with
   another part also sending to that effect so their scope is clear.
6. Check SELECT tempo, PRESETS curated kits and ALGORITHM part order in all three
   workspaces. Tap ENV/LFO/SEL/ARP and check the guidance and unchanged workspace.
7. Check the held shortcuts, GRID step editing, live recording, sounding lights,
   HOME memory and a melodic part's sound/FX pages. Use Save now only when you
   intend to replace your saved checkpoint.

The firmware family identity is unchanged. The KIT SOUND and KIT FX routes are
the feature check; the install response alone does not verify these controls.
No deliberate power interruption is part of this trial.

## Maintainer checks and publication

### Existing-save compatibility

The saved runtime identity in `jr_asset_identity.h` is unchanged from Trial 02
and the normal manual-saving release. Do not regenerate it for this UI change:
that would reject the owner's existing checkpoint. The catalog checker hashes
whole source files, so the new page table and kit browser require an explicit
[compatible-catalog review](../firmware/experimental/recovery/jr_catalog_compatibility.json).
It permits exactly this reviewed source digest, requires unchanged generated
audio assets and the original runtime header, and rejects unknown later edits.
The codec, runtime, parameter/kit IDs and project/sidecar layouts are unchanged.

`tests/test_jam_recovery_cross_version.py` compiles the actual Trial 02 writer and
current reader against the same verified assets, then exchanges synthetic NOR
images between independent processes in both directions. Its full-session
checks include A–D sections, edited FM6/SYN sounds, note lengths and preferences.
No user backup or device is involved. Record the actual outcome in build evidence.

### Build and preview

Run `tests/run_jam_drum_navigation_tests.sh`,
`tests/run_jam_drum_grid_tests.sh`, `tests/run_jam_drum_kit_ui_tests.sh` and
`tests/run_jam_drum_kit_audio_tests.sh`, plus Home/recovery UI and runtime
regressions. Rebuild the emulator and check the
actual shared UI/audio paths in Chromium. Inspect native 240×240 screens and
confirm synth navigation and held-layer ownership. Host results do not establish
hardware timing or physical audio behavior.

Run `python3 tests/test_jam_recovery_identity.py` and
`python3 tests/test_jam_recovery_cross_version.py` to check the exact compatibility
review and Trial 02 round trips before packaging.

Commit source before the final device build so its receipt identifies clean
corresponding source. Use the existing constrained publication route:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-recovery
python3 tools/package_jam_drums_preview.py --no-build
python3 tests/test_package_jam_recovery.py
python3 tests/test_publish_jam_drums_preview.py
JAM_RECOVERY_SITE_ROOT=build/jambox-drums-preview \
  JAM_RECOVERY_EVIDENCE_DIR=build/jam-drums-install-evidence \
  node tests/jam_recovery_install_browser.mjs
python3 tools/publish_jam_drums_preview.py
python3 tools/publish_jam_drums_preview.py --publish
```

Record the final candidate hash, source, target resource budgets, test outcomes
and independent publication comparison in build evidence. Only
`preview/drums/` may change; preserve all outside Git paths and modes, including
the normal installer, emulator and `preview/saving/`. Keep private recovery files
out of Git. Normal-installer adoption requires its own decision after testing.

The owner-tested Trial 02 remains in website Git history at
`d246c1d8f70b85ac65be0d35cf3782d8fdfb563c`. Its firmware SHA256 is
`c7115d6f5a8c43fa0778f52562ee2f41d2a12b75fa91695b0749cc29c74b0462`.
Use that exact revision when reviewing or preparing a return to Trial 02;
the moving preview URL is not a version pin. See its
[guide](DRUM_GRID_TRIAL.md) and
[owner feedback](build-evidence/drum-grid-controls/owner-feedback.md).
