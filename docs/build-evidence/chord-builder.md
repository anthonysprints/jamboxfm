# Chord builder — 2026-10-14

The owner requested playable black-key chord roots, optional WHITE-mode
modifiers, configurable chord shapes, and full chords whenever the setting
is enabled on Lead or Bass. The [playing guide](../CHORD_BUILDER.md) describes
the controls and beginner defaults.

## Result

- QNT OFF/SNAP now uses every key as a chord root. SNAP adjusts the root
  before building the chord; it retains the existing downward rounding.
- WHITE alone reserves black keys for the existing live modifiers.
- MAJOR, MINOR and DIM append to the existing chord choices. INVERT offers
  ROOT, AUTO and three manual inversions without changing the project layout.
  Manual and automatic voicings keep distinct pitches, including POWER chords
  whose octave root previously caused duplicate notes when inverted.
- Enabling chords selects polyphonic voices on melodic parts. Bass and Lead
  still start with chords OFF. Selecting a sound preserves enabled chords.
- Live OFF/SNAP chord lights show the pressed key and corrected root, without
  lighting chord tones or other inputs mapped to that root.
- Local keys sharing a note retain it until the final owner releases it.
  Recording, arpeggiator metadata, strum and panic use the held-key ledger.
  Complete independent external-MIDI/sequencer ownership remains future work.

## Build and checks

- Chord regression harness: 704 checks plus exhaustive scale mapping passed.
  Coverage includes black roots, fixed/scale shapes, WHITE modifiers, manual
  and automatic inversions, both alias-release orders, overlapping chords,
  recording, arpeggiation, delayed strum, held-setting changes and panic.
- Existing sequencer, project and preset compatibility checks passed. UI pages
  and 20,000-frame fuzz passed, including inverted SNAP chord step entry.
- JamBox palette: 435 checks passed; Home, preview recording/release,
  two-chord/bass overdub and audio checks passed. Native/WASM audio matched
  with maximum sample delta zero.
- Final native emulator: 78 checks passed, including three actual gated
  voices on Bass/Lead. Final Chromium emulator passed for physical controls,
  D# major chords, E-to-D# SNAP lights, chromatic roots, recording, release,
  desktop/mobile and standalone operation.
- Final updater-enabled device build passed: app 547,964 / 581,564 bytes;
  ordinary RAM 80,648 / 98,304; pool 324,580 / 344,064. RAM text has 884
  instructions without calls; register-access checks passed.
- Firmware: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes; SHA-256
  `feed5f35c47d36520c9f81090d4a46a6d077ea6542587412cfccc34cb92836b3`.

The device receipt preserves the actual pre-commit snapshot and input hashes;
it is not relabeled as a later commit. This is a software-tested development
candidate, not a claim of physical validation. The updater/installer protocol
is unchanged, persistence remains disabled, and hardware/release flags remain
false. No physical FM-1 was accessed from the cloud environment.

## Publication

Packaging, installer regression results and live publication verification
will be recorded here after deployment completes.
