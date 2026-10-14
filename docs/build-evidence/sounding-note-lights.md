# Sounding-note lights — 2026-10-14

The owner requested every sounding chord tone on the keyboard, alongside
physical presses. This replaces the preceding chord build's root-only live
display. The [playing guide](../CHORD_BUILDER.md#lights-and-held-keys) describes
the current behavior. Implementation source: `a575192`.

## Change

JamBox maps sounding melodic notes to printed pitches in the selected octave
for OFF, SNAP and WHITE. Pressed keys remain lit separately. Chord tones,
strums and simultaneous sequencer/MIDI voices on the selected part use the
same display path; live chords no longer hide other playing notes. Brief
note-on flashes make quick taps visible. Notes outside the visible register
are not wrapped, and effect tails do not extend note lights. Drum pads and
control layers retain their existing indications and NOTES remains optional.

Only shared UI lighting code changed; synthesis, chord building, recording,
the installer protocol and loader were not modified in this increment.

## Build and checks

- Native emulator: 88 checks passed. The lighting cases exercise D# major,
  E-to-D# snapping without aliases, fixed chromatic chords, simultaneous
  playback, short taps and expiry, transpose, selected octave, off-screen
  roots with visible upper tones, inversion, progressive strum, WHITE
  input/modifiers, transposed single notes and selected-part isolation,
  including the first frame after switching parts.
- Chromium emulator passed using the actual panel controls and AudioWorklet:
  exact chord LED sets on Chords and Lead, initial/sustained samples, clean
  release, single-note SNAP, recording, panic, desktop/mobile and standalone.
- Updater-enabled device build passed: app 547,932 / 581,564 bytes; ordinary
  RAM 80,648 / 98,304; pool 324,580 / 344,064. RAM text contains 884
  instructions without calls; register-access checks passed.
- Firmware: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes; SHA-256
  `e64dbff63594a08e5c0fff99e2170b966aeea892620ba144f986a57c3a6e528c`.
- Installer controller/shared transport: 44 checks passed, including simulated
  factory and normal-JamBox transfers of this candidate. The read-only device
  page passed 18 Chromium cases and its outgoing-packet allowlist checks.
- Publication integration: 15 checks passed. The expanded 50-file package
  validated firmware, corresponding source, receipt, descriptors and notices.

The device receipt preserves the actual build snapshot and input hashes.
Hardware/release flags remain false, persistence remains disabled, and this
software-tested update still needs audition on the owner's FM-1. No physical
device was accessed from the cloud environment.

## Publication

Pages commit `f424471019c6e9db0a0d8238522892022dcd75be` was published without
rewriting history. [Workflow 38596864139](https://github.com/anthonysprints/jamboxfm/actions/runs/38596864139)
completed successfully for that exact commit. The 50-file package's
`SHA256SUMS` digest is
`adc8a5325f86bb6d70df6d8f8c58239497e4deb543c07a04778f256936c077a9`.
All 49 downloadable files matched the local package byte for byte over HTTPS;
the remaining file is the `.nojekyll` configuration marker.

The live [installer](https://anthonysprints.github.io/jamboxfm/install/) and
[emulator](https://anthonysprints.github.io/jamboxfm/emu/) serve this update.
The source archives retain the implementation snapshot; this publication
record is completed in a later documentation-only commit.
