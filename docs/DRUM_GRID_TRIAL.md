# Drum grid: development trial 02

This implements the first two stages of [the drum workflow plan](DRUM_WORKFLOW_PLAN.md)
and [issue #5](https://github.com/anthonysprints/jamboxfm/issues/5), on
`next/usability-and-saving`. The normal installer and emulator remain separate.

The owner reported that the first entry/recording trial worked. That report is
recorded in [its evidence](build-evidence/drum-grid-entry/owner-feedback.md).
The owner subsequently confirmed that Trial 02 worked through their testing and
looked good; see [Trial 02 feedback](build-evidence/drum-grid-controls/owner-feedback.md).
This records owner testing of the offered build, not independent hardware
validation. The normal installer has not been promoted.

## What changes

- Select Drums and tap SEQ from either Home view or an ordinary parameter page:
  the dedicated GRID opens, regardless of remembered STEP/PATTERN/SONG pages.
- Keyboard ownership updates in the same input pass: the first key after SEQ
  edits a step; the first key after HOME plays a drum. Already-held keys keep
  their original release ownership.
- HOME restores the prior GROOVE/SOUND view. Synth SEQ navigation is unchanged.
- GRID shows eight named drums at a time, the selected drum and kit, its printed
  pad pitch, and a literal step range/length. It keeps the page you are editing;
  an arrow shows when the playhead is elsewhere.
- GRID, live pads and held SEQ use the same selected drum. Held SEQ starts on the
  editing page and returns its chosen page to GRID. Save now preserves the
  transient drum choice without adding a new saved-format field.
- Starting REC from GRID returns to playable Home before arming/recording.
  During arm/count-in/free take, SEQ says FINISH TAKE FIRST and leaves the
  recorder in control. During an ordinary playing overdub, explicit SEQ entry
  preserves recording while GRID keys intentionally edit steps.
- SAVE retains its modal priority and manual checkpoint. Storage format,
  sounds and flash writers are unchanged. Autosave stays off.

## GRID controls

| Control | Action |
| --- | --- |
| Knob 1 | Select/audition a drum. |
| Knob 2 | Select a step; preview its hits only while stopped. |
| Knob 3 | Selected hit strength: ghost / soft / normal / hard. |
| Knob 4 | Selected hit repeats: 1–4. |
| Sixteen white keys | Toggle the selected drum's steps, without REC. |
| First four black keys | Select a 16-step page, within the pattern length. |
| SEQ | Open or keep GRID. |
| EDIT | Switch GRID/KIT. |
| SELECT | Tempo, one BPM per detent; also on KIT. |
| PRESETS | The same eight curated kits as Home; also on KIT. |
| ALGORITHM | Home's Drums/Bass/Chords/Lead order; another part returns Home. |

Strength/Repeat only edit an existing hit. Empty cells explain how to add a hit
and preserve the beat and previous undo. Clamped knob turns also preserve undo.
Kit browsing preserves recorded notes, timing, mixer settings and undo.

KIT retains named pads and kit/level/reverb/pan; its knob 1 still offers the full
inherited kit list, including empty USR entries. Cleaning up those sources and
individual drum sound editing remain later work. Held layers retain their own
control ownership; ordinary knob assignments do not leak into them.

## Device trial

Separate destination: https://anthonysprints.github.io/jamboxfm/preview/drums/install/.
Record actual publication/HTTP outcomes in build evidence before calling it live.

1. Use Save now on the current saving build and wait for Saved before updating.
   Keep the existing recovery folder. The page checks the same local
   backup.json/full-flash-before.bin pair; nothing is uploaded. These files are
   a recovery baseline, not a backup of a new jam.
2. In desktop Chrome/Edge, connect one running JamBoxFM. This preview accepts
   JamBox-to-JamBox updates only. Keep power/USB steady through the update.
3. Confirm the saved jam and edited sounds returned. Do not reinitialize saving:
   the storage format is unchanged.
4. Select Drums → SEQ. Choose drums in both halves of the list, toggle white-key
   steps, change Strength/Repeat and listen with PLAY. Check SELECT tempo,
   PRESETS kit and ALGORITHM part order. Check later/partial step pages.
5. SEQ should keep GRID; EDIT switches GRID/KIT. Check held SEQ returns the same
   drum and its chosen page. Check HOME memory.
6. Start REC from GRID: Home should return with playable drum pads. Record two
   hits at different times. Check held SEQ, held EDIT erase/undo, synth SEQ,
   sounding lights and manual saving.

The family identity is unchanged; direct Drums → SEQ is the feature check.
No deliberate power interruption is part of this trial. Host/browser checks
cannot verify physical button/audio or power-loss behavior.

## Maintainer workflow

After activating `tools/cloud/activate.sh`, run the focused native fixtures
`tests/run_jam_drum_navigation_tests.sh` and `tests/run_jam_drum_grid_tests.sh`, Home/recovery UI regressions, rebuilt
emulator and `tests/jam_drum_navigation_browser.mjs`. Commit source changes
before building so the receipt accurately identifies clean corresponding source.

```sh
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

The package reuses the strict recovery profile, including its internal manifest
and README names. It adds no new storage behavior. The shared controller keeps
saving labels by default; source-bound drum HTML selects the drum trial labels.
Transport, backup/file checks and device guards remain unchanged.

The new publisher targets only `preview/drums/`, validates source/checksums and
receipts, rejects races and preserves every outside Git object/mode, including
the normal installer, emulator and `preview/saving/`. Do not use production
publishers for development. Record target budgets and firmware hash, verify
published artifacts independently, and keep private recovery files out of Git.
