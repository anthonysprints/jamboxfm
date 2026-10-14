# Curated preset expansion — 2026-10-14

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
  network requests blocked. Final integrated runner verification follows
  below alongside the FM-1 candidate and installer checks.

The [audio metrics](preset-bank-audio.json) bind the final native bank test
to its source hashes and index its optional audition WAV. RMS over a held
phrase is not perceived-loudness matching: a short bell and a sustained pad
are expected to differ. The [WASM evidence](preset-bank-wasm.json) binds the
44-preset comparison to the exact emulator hash. Listening feedback on the
physical FM-1 and worst-case target audio deadlines remain separate.

## Device and publication

Pending final FM-1 build, memory gates, installer checks and publication.
Application-data persistence remains disabled.
