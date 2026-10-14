# Sounding-note lights — 2026-10-14

The owner requested every sounding chord tone on the keyboard, alongside
physical presses. This replaces the preceding chord build's root-only live
display. The [playing guide](../CHORD_BUILDER.md#lights-and-held-keys) describes
the current behavior.

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
  input/modifiers, transposed single notes and selected-part isolation, including the first frame after switching parts.
- Chromium emulator passed using the actual panel controls and AudioWorklet:
  exact chord LED sets on Chords and Lead, initial/sustained samples, clean
  release, single-note SNAP, recording, panic, desktop/mobile and standalone.
- Updater-enabled device build passed: app 547,932 / 581,564 bytes; ordinary
  RAM 80,648 / 98,304; pool 324,580 / 344,064. RAM text contains 884
  instructions without calls; register-access checks passed.
- Firmware: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes; SHA-256
  `e64dbff63594a08e5c0fff99e2170b966aeea892620ba144f986a57c3a6e528c`.

The device receipt preserves the actual build snapshot and input hashes.
Hardware/release flags remain false, persistence remains disabled, and this
software-tested update still needs audition on the owner's FM-1. No physical
device was accessed from the cloud environment.

## Publication

Installer/package checks and live deployment results will be recorded here
after publication completes.
