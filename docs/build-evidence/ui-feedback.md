# First device UI fixes — 2026-10-14

Scope and proposed follow-ups: [First device feedback](../FIRST_DEVICE_FEEDBACK.md).
The owner reports successful first installation and general playing. This
update implements Home tempo control and the shared NOTES default; chord
semantics and sequencer layout remain proposals.

## Candidate

- File: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes.
- SHA-256: `1e2c7a956081db0e71e2a455d1532b2c555e26d24bc3794f1d23e0b33255afd8`.
- App: 547,256 / 581,564 bytes.
- Ordinary RAM: 80,648 / 98,304; pool: 324,580 / 344,064.
- Retained region: 15,464 / 15,696. ELF and configured reserve gates pass.
- OTA, real flash writes and JamBox enabled; app persistence, dry-run,
  RAM-only updater, restricted icon artwork and optional slicer disabled.
- The loader/update-writing implementation is unchanged by these UI fixes.

The receipt records the pre-commit dirty source snapshot. Its actual build
input hashes and the corresponding source archive are checked by the
packager. No later source commit is substituted into that receipt.
The manifest's hardware/release flags remain false. Schema-2 `trial.json`
binds the exact manifest/firmware to factory-V15 installation and normal
JamBox updating; the existing simulated transitions are not broadened into
production permissions.

## Completed checks

| Check | Outcome |
| --- | --- |
| Real Home input/draw harness | Passed: BPM detents and bounds, live playback, sound/part/pattern preservation, existing screen/layer ownership |
| Device storage/boot harness | 246 checks, zero failures, including reproduced-and-fixed NOTES default and D#/E directional lights |
| Native emulator | 38 checks passed, including shared default, note flashes, transposition/octave and active Home control hints |
| Actual Chromium emulator | Passed: controls, audible AudioWorklet, recording, desktop/mobile and packaged standalone runtime |
| Device storage/update runner | Passed: legacy persistence, configuration guards, transport, recovery, MIDI and both loader builds |
| Candidate packaging / memory unit tests | 12 + 12 passed |
| Candidate integration | Nine simulated transitions, 17 transfer stages, every logical byte checked |
| Production installer controller / shared transport | 21 + 23 passed |
| Final packaged installer in Chromium | 23 cases passed, including complete factory install and normal JamBox update |
| Publication integration | 15 tests passed; real Git push dry-run passed |

Reproduction: activate `tools/cloud/activate.sh`, build with
`bash tools/cloud/build_device.sh --with-updater`, run
`sh tests/run_jam_emu_tests.sh`, the Home harness, and
`CC=/usr/bin/gcc sh tests/jam_storage_run.sh build/felucca.fwsc`.
The Home harness requires its inherited icon header; this run generated it
into `build/jam-tests/ui-gen` without changing the device's artwork-disabled
headers. Then run candidate/memory unit tests and
`node tests/jam_device_candidate_check.mjs`.

Package with `python3 tools/package_jam_emu.py --with-trial --no-build` after
the emulator build; run
`node --test tests/jam_trial_install_test.mjs tests/jam_web_transport_test.mjs`,
`JAM_INSTALL_SITE_ROOT="$PWD/build/jambox-web" node tests/jam_install_browser.mjs`,
and `python3 tests/test_publish_jam_web.py` against that package.

Software simulation does not establish physical JamBox-to-JamBox handover or
power-loss recovery. The prior static IRQ heuristic and inherited Sloop PWA
failure were not rerun or relabeled as passing by this focused change.
The final identity response verifies family/release, not an independent
readback of the new firmware digest.
