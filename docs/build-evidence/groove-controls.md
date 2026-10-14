# Sloop-style Groove controls — 2026-10-14

Implementation source: `aaa4724`.

The owner requested performance controls on GROOVE, keeping sound shaping on
SOUND. [The control guide](../GROOVE_CONTROLS.md) specifies the resulting layout:
shared SWING, selected-part LEVEL, STEPS and PAN. Left encoders, Home navigation,
keyboard playing and recording retain their existing roles. Level preserves
explicit mute; Steps preserves all hidden pattern data. The browser exposes
the actual control names, ranges, units and selected-part/global scope.

The accompanying Bass investigation makes no voice/recording changes. Native
checks confirm default LEGATO's single sounding voice and single-note step
replacement, EDIT > VOICE knob 1 selecting POLY, two simultaneous voices and
two notes stored on a POLY step. Browsing another Bass sound with CHORD off
restores LEGATO. The guide also explains the existing tied-group limitation
for staggered, independently overlapping note lengths.

## Verification

- `sh tests/run_jam_tests.sh`: 435 palette and 540 UI checks passed, plus
  native/WASM chord capture, audio equality, overdub and replay checks.
- `sh tests/run_jam_emu_tests.sh`: 127 native adapter checks and 115 Home
  navigation checks passed. Real Chromium/AudioWorklet checks passed for
  Groove swing/level/steps/pan, Sound macros, accurate accessible labels,
  preserved chord lights, held notes, overdub, desktop/mobile and standalone.
- Physical-size renderer output was inspected: all four controls fit the
  240×240 LCD, with CENTER/L/R pan labels and the existing pattern overview.
- `bash tools/cloud/build_device.sh --with-updater`: app **551,036 / 581,564
  bytes**, ordinary RAM **80,744 / 98,304**, pool **324,580 / 344,064**.
  RAM text remains 884 instructions without calls; register-access checks pass.
- Firmware: **611,255 bytes**, identity `FM-1_700`, SHA-256
  `124927e3c0122a13aff87c3d5c5825c019165cb7f108d7a09ab87547201d60e1`.

- Final installer/shared transport: **44 checks passed** on the actual candidate,
  including simulated factory and normal-JamBox transfers.
- Final publication integration: **15 checks passed** on the exact expanded
  50-file package, including source, receipt, firmware and notices.

Publication is verified below. Device receipt
input hashes bind the actual build snapshot. Persistence remains disabled;
hardware validation/release flags remain false. This build has not been
auditioned on a physical FM-1 in the cloud environment.

## Publication

Pages commit `25a85b670a4c54c287a4294010eaa44c830954aa` was published without
rewriting history. [Workflow 38612596990](https://github.com/anthonysprints/jamboxfm/actions/runs/38612596990)
completed successfully for that exact commit. The 50-file package manifest
SHA-256 is `3b9a87eec0552c5944848fff2b9eba1a1a45e21679f1381621ee48fbdbd64a74`.
All 49 downloadable files matched the tested package byte for byte over HTTPS;
the remaining file is the `.nojekyll` configuration marker.

The live [installer](https://anthonysprints.github.io/jamboxfm/install/) and
[emulator](https://anthonysprints.github.io/jamboxfm/emu/) now serve this build.
This documentation-only publication record follows the implementation commit;
corresponding source archives retain the build-time snapshot. Physical audition
of the new controls on the owner's FM-1 remains outstanding.
