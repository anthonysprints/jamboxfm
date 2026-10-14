# Curated preset expansion — 2026-10-14

Implementation source: `962a8f7`.

The owner requested more presets now and deferred extra tracks. The
[player guide and catalog](../PRESET_LIBRARY.md) lists all 44 choices:
12 Bass, 12 Chords, 12 Lead and eight drum kits. The first two choices per
role and the default Dusty session retain their existing sounds and settings.

## Implementation

The curated catalog resolves existing factory recipes by engine and name.
It uses ANALOG, DIGITAL, PHASE, SAMPLE, WHEEL and FM6. No external recordings,
engine code, track allocations or project-format changes are introduced.
The smaller GRAIN/PHYS polyphony caps and baked-in TRIO chord intervals are
outside this bank. Every melodic choice can play four distinct chord notes
with the existing eight shared synth voices; Bass/Lead retain LEGATO when
chords are off.

Factory sound parameters, FX and measured gain trims are adapted without
loading their arpeggiators, patterns or UNISON mode. Per-entry gain offsets
give several new chord patches headroom and compensate NEON SAW for its
single-voice role. The inherited trim table and the six original melodic
recipes are unchanged. Settings for notes, independent lengths, key/scale,
chords/inversions, timing, locks and mix stay with the track.

SOUND's BRIGHT/MOVE controls now follow each engine. GRIT and SPACE retain
the existing role-specific effect ranges. WHEEL MOVE advances one rotary
option per detent. GROOVE keeps its SWING/LEVEL/STEPS/PAN controls. Both Home
views display the selected sound's position; a sound from outside the
curated bank displays an unknown position rather than a false first entry.

## Completed software checks

- `sh tests/run_jam_tests.sh`: 435 original palette checks, 867 Home UI
  checks, capture/overdub/replay, macro bounds and native/WASM default-chord
  audio equality passed.
- `sh tests/run_jam_preset_bank_tests.sh`: **9,134 checks passed** across
  all 44 choices. Tests cover pattern/independent-length preservation,
  playing/mix settings, stable reloads, valid engine parameters, useful
  macro endpoints, active-lock restoration, invalid requests, actual
  four-voice chords from black roots, and production DSP audio/release.
- Every melodic baseline audition required zero limiter gain reduction
  after curation (the regression threshold permits at most 1 dB). All
  four macros together at both extremes produced bounded output and
  released cleanly; drum LEVEL at zero intentionally mutes it.
- `sh tests/run_jam_presets_wasm_tests.sh`: **1,165 checks passed**, playing
  all 44 sounds through the actual panel ABI. Every preset's native and
  deliverable-WASM samples matched exactly. Physical browsing/wrapping,
  engine requests, sound macro endpoints and LCD bounds passed.
- The shared emulator/browser regression passed real Chromium/AudioWorklet
  checks for Home views, controls, chords, recording, LEDs, release/panic,
  pause/resume, desktop/mobile and the standalone document with further
  network requests blocked. The final integrated runner exited successfully,
  including the new all-preset WASM suite and the existing independent-note,
  project/editor, ownership and Home-navigation regressions. The earlier
  four-part AudioWorklet workbench also passed its Chromium regression.

The [audio metrics](preset-bank-audio.json) bind the final native bank test
to its source hashes and index its optional audition WAV. RMS over a held
phrase is not perceived-loudness matching: a short bell and a sustained pad
are expected to differ. The [WASM evidence](preset-bank-wasm.json) binds the
44-preset comparison to the exact emulator hash. Listening feedback on the
physical FM-1 and worst-case target audio deadlines remain separate.

## Device and publication

The updater-enabled FM-1 build passed with a clean input receipt at the
implementation commit. App: **558,804 / 581,564 bytes**; ordinary RAM:
**87,944 / 98,304**; pool: **325,348 / 344,064**. RAM and pool allocation
are unchanged from the preceding build. Retained state occupies
13,952 / 15,696 bytes. Region reserves, register-access, unresolved-symbol,
stack-boundary and RAM-text checks passed.

Firmware: **611,255 bytes**, identity `FM-1_700`, SHA-256
`0989dff18b3217f0ccbb233812aa4ecb4222a465632c93d3ecec5323871ddebe`.
`sh tests/jam_storage_run.sh build/felucca.fwsc` passed session/storage
policy, JRM1 isolation, update transport, recovery and loader retry/fault
checks on this candidate. Application-data persistence remains disabled.

The final packaged candidate passed **44 installer/transport tests**,
including simulated complete factory and normal-JamBox transfers. The
packaged installer passed **23 real Chromium cases**, including both
transfers of this exact firmware, interruption/error outcomes, identity
checks, reports and desktop/mobile layout. The publisher's **15 integration
tests** passed on the exact 50-file expanded package, including source,
receipt, notices and digest bindings. Cloud checks do not establish physical
FM-1 timing or audition of this new bank.

## Publication

Pages commit `c4896b796312e8b26a9c01206d25bdc2d8b93589` was pushed without
rewriting history. Package `SHA256SUMS` digest:
`a9b6444da1984d30b899cc949ae7a82d6f4a17ff98516c221daa51c479b522e6`.
[Workflow 38624688112](https://github.com/anthonysprints/jamboxfm/actions/runs/38624688112)
completed successfully for that exact commit. All **49 downloadable public
files** matched the tested local package byte for byte over HTTPS, including
the emulator WASM, firmware, source archives and installer. The fiftieth
package file is the `.nojekyll` configuration marker. The
[publication receipt](preset-bank-publication.json) records each public file's
size and digest.

The live [installer](https://anthonysprints.github.io/jamboxfm/install/) and
[browser instrument](https://anthonysprints.github.io/jamboxfm/emu/) serve
this build. This documentation-only verification record follows the
implementation commit and changes no firmware or web runtime bytes.
