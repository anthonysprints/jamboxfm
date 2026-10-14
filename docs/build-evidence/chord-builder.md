# Chord builder — 2026-10-14

The owner requested playable black-key chord roots, optional WHITE-mode
modifiers, configurable chord shapes, and full chords whenever the setting
is enabled on Lead or Bass. The [playing guide](../CHORD_BUILDER.md) describes
the controls and beginner defaults. Implementation source: `0184003`.

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
- Installer controller and shared transport: 44 checks passed, including
  simulated factory and normal-JamBox transfers of this candidate.
- Read-only device page: 18 Chromium cases passed; every outgoing MIDI
  packet matched the read-only allowlist.
- Publication integration: 15 checks passed. The expanded 50-file package
  validated firmware, build receipt, corresponding source, descriptors and
  notices. The final package includes the corrected SEL page instructions.

The device receipt preserves the actual pre-commit snapshot and input hashes;
it is not relabeled as a later commit. This is a software-tested development
candidate, not a claim of physical validation. The updater/installer protocol
is unchanged, persistence remains disabled, and hardware/release flags remain
false. No physical FM-1 was accessed from the cloud environment.

## Publication

Pages commit `c7bba5675207accdca74b354ecdcd0e7848b19c0` was published without
rewriting history. [Workflow 38587624027](https://github.com/anthonysprints/jamboxfm/actions/runs/38587624027)
completed successfully for that exact commit. The 50-file package's
`SHA256SUMS` digest is
`b6d0a9692a5e30fe6e9f6c08899061246ed278ace23abab9c3666f45973f6192`.
All 49 downloadable files matched the local package byte for byte over HTTPS;
the remaining file is the `.nojekyll` configuration marker.

The live [installer](https://anthonysprints.github.io/jamboxfm/install/) and
[emulator](https://anthonysprints.github.io/jamboxfm/emu/) serve this update.
The attached source archives preserve the implementation snapshot; this
publication-results addition is a later documentation-only commit.
