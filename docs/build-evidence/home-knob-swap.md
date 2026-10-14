# Home knob swap — 2026-10-14

The owner confirmed a successful retry of the preceding UI update, including
BPM control and directional SNAP lights, then requested this mapping:

- SELECT (top-right of the left cluster): tempo, one BPM per detent, 40–240.
- ALGORITHM (bottom-right): Drums / Bass / Chords / Lead selection.
- PRESETS: the selected part's sound.

Implementation source: `02b0154`. Firmware Home hints, browser control hints,
instructions and existing control tests use the same mapping. The Home guard
and all deeper-page/layer control ownership are unchanged. The earlier failed
handoff remains unexplained; this change does not alter the installer or loader.

## Build and checks

- Updater-enabled device build passed: app 547,256 / 581,564 bytes; ordinary
  RAM 80,648 / 98,304; pool 324,580 / 344,064.
- Firmware: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes; SHA-256
  `fe9761b0bd2dedbb8ecd065bfd6f0dd66e1616d1b99bddf6c2a3500c4fc4ccbf`.
- Existing Home input/draw harness passed, including tempo bounds, playback,
  sound/part/pattern preservation and screen/layer ownership.
- Native emulator: 38 checks passed. Chromium emulator passed for controls,
  playback/recording, directional SNAP lights, desktop/mobile and standalone.
  The Home screenshot was inspected for legible updated footer hints.
- Installer controller and shared transport: 44 checks passed, including
  simulated factory and normal-JamBox transfers of the new candidate.
- Publication integration: 15 checks passed. Expanded 50-file trial package
  validated its firmware, receipt, corresponding source, descriptors and notices.

The build receipt retains the actual pre-commit source snapshot and input
hashes. It is not relabeled with a later commit. Hardware/release flags remain
false: the owner's success report concerns the preceding update; this mapping
still needs confirmation on the instrument. No physical device was accessed
from the cloud environment.

## Publication

Pages commit `3bbf3a9e62a1472a859a2190fcc2becb4a85375a` was published without
rewriting history. [Workflow 38581120138](https://github.com/anthonysprints/jamboxfm/actions/runs/38581120138)
completed successfully for that exact commit. The 50-file package's
`SHA256SUMS` digest is
`f92e37c10f143960df0a75ffef5c02e3d31db7cff93b2d8b056fbe8c2b5f69cb`.
All 49 downloadable files matched the local package byte for byte over HTTPS;
the remaining file is the `.nojekyll` configuration marker.

The live [installer](https://anthonysprints.github.io/jamboxfm/install/) and
[emulator](https://anthonysprints.github.io/jamboxfm/emu/) serve this update.
