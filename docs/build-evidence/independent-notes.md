# Independent POLY note lengths — 2026-10-14

The owner approved removing the tied-group recording restriction for POLY
parts while keeping Bass's LEGATO default. The [playing/design guide](../INDEPENDENT_NOTES.md)
describes the resulting behavior and boundaries.

## Implementation

Each recorded POLY onset has its own grid-snapped length, 1–64 steps, bounded
to one current loop. Four onsets still fit on a step; earlier notes can sound
under later onsets up to the existing shared eight-voice synthesis budget.
Live recording, overdub and free-take completion use the same durations.
No additional overlap setting or allocation is introduced in the audio path.

The ten-byte legacy step and drum layouts remain intact. A byte-per-note
side table carries lengths; zero selects legacy articulation. Legacy tied
holds are materialized before POLY overdub so adding an onset will not cut
them. Mono/legato recording and drum sequencing retain their prior paths.
Explicit lengths determine their own endings rather than the shared GATE.

Sequence owners have bounded per-pitch endpoints on the part's grid, so
tempo/swing changes preserve note lengths. Repeated pitches renew an owner;
they do not create independent envelopes for the same pitch. Physical input,
legacy groups and new sequence owners cannot release each other prematurely.
Clear/replacement generations, transport restart, preset changes, REC handoff,
sustain, active loop shortening and step-division changes have explicit release
handling. Changing division ends current independent sequence notes; the next
onset uses the new grid. Quantized duplicate free-take onsets retain the longest
duration, and held-capture overflow finishes the oldest capture deterministically.

Undo/redo, transpose, rotation, pattern doubling, note removal, LEN editing and
the note-roll renderer preserve the side table. The new private **JRM1**
RAM/export snapshot is 4,584 bytes; legacy JRM0/FUN6 stays 3,816 bytes. Three
snapshots fit the existing retained region, with the fourth in ordinary BSS.
Project/arranger snapshots and editor protocol 11 round-trip lengths. Ordinary
foreign import and persistent project writes remain disabled; JRM1 is not a
new flash-save format. No linker, flash-sector or stack boundaries changed.

## Verification

- `sh tests/run_jam_tests.sh`: **435 palette checks**, **540 Home UI checks**,
  native/WASM audio equality, capture/overdub/replay and queue checks passed.
- `sh tests/run_jam_emu_tests.sh`: existing **127 native adapter** and **115 Home
  navigation** checks passed, plus **93 independent-recording/playback**, **28
  duration UI**, **34 project/editor**, **17 browser editor protocol** and **52
  duration ownership** checks.
- The actual deliverable WASM passed **16 deterministic recording/playback
  checks**, including C held for six steps with E independently sounding from
  steps two to four. Stored lengths, real voice gates, sample bounds, panic
  and LCD writes were checked through the shared panel/API.
- Actual Chromium/AudioWorklet checks passed for sound, recording, GROOVE/SOUND,
  held notes, chord LEDs, page/layer navigation, desktop/mobile and standalone
  with further network requests blocked. There is no second JS note scheduler.
- The final **93-check** native duration suite also passed GCC AddressSanitizer.
- `sh tests/jam_storage_run.sh` passed on the final firmware candidate: **246
  temporary-session storage checks**, Sloop rejection of JRM1, **8 additional OTA
  isolation checks**, configuration guards, editor restrictions, update transport,
  recovery, MIDI parsing and both loader fault/retry suites.
- Legacy project-format tests passed **29 checks**, preserving the old serialized
  sizes and checksums while the JamBox RAM representation changes.
- Updater-enabled FM-1 build passed: app **557,244 / 581,564 bytes**, ordinary
  RAM **87,944 / 98,304**, pool **325,348 / 344,064**. RAM text remains 884
  instructions without calls; retained-region and register-access checks passed.
- Firmware: **611,255 bytes**, identity `FM-1_700`, SHA-256
  `9c5e78a303996ee91600ce0e7fa9434d667f0212dc4e5db7758f28689c6db148`.

- Final installer/shared transport: **44 checks passed**, including complete
  simulated factory and normal-JamBox transfers of this actual candidate.
- Final publication integration: **15 checks passed** on the exact expanded
  50-file package, binding firmware, corresponding source, receipt and notices.

Publication is pending. The build receipt binds the
actual source inputs, compiler and candidate. Hardware/release flags remain
false; physical FM-1 audio deadlines, USB behavior and audition remain separate
from host validation. Persistence remains disabled.

## Publication

Pending final expanded-package checks and exact live-file verification.
