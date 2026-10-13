# FM-1 update support with temporary JamBox sessions

October 13, 2026. Milestone 2 implements the software paths for USB updating and application rescue while disabling musical-data persistence. Target builds, isolation/transport simulations and browser checks pass. **No physical FM-1 was connected or flashed. These packages are development outputs, not an owner-installation release.**

Starting source: `b28535c4ef7cd4e8f845e954216bff7241229d74`. The [source input hashes](source-inputs.sha256) identify the working-tree compiler inputs, including the new source files, rather than attributing them to the starting commit. Compiler and SDK provenance remain recorded in [milestone 1](../device-build/README.md). The compiler archive was supplied by the owner and verified against its recorded hash; it is not represented as a locally observed vendor download or signature.

## Behavior and boundaries

- `FELUCCA_PERSIST=0` is independent of `FELUCCA_FLASH=1` and `FELUCCA_OTA=1`. Normal Sloop builds retain their existing persistence default. JamBox rejects persistence, OTA without flash, dry-run updating and RAM-only resume configurations that violate this target's policy.
- Every normal JamBox boot clears retained musical slots and settings, initializes empty user banks and starts Dusty. It never imports Sloop projects, autosave, sounds, samples or musical preferences. Only a CRC-checked settings object's recognized layout and strictly validated 32-byte PAN5 physical button/encoder mapping are read. Unknown layouts, corrupt records, duplicate/out-of-range mappings and unknown flash identity fall back to the default panel.
- Temporary arranger snapshots use private `JRM0`, so Sloop's parser rejects them even with a valid checksum. This is not the future JAM1 disk format. Shared SET3/PAN5 RAM preferences/calibration can still be observed when returning to Sloop if its valid flash settings are absent; the claim is musical-project isolation, not total two-way RAM isolation. Bootguard/crash records are preserved.
- UI and editor save, restore, erase and user-bank writes reject requests before changing slots or staging data. Replies retain each protocol command's shape and explicitly report unavailable storage. The browser editor checks the optional PROJECT status before reporting a successful save. Live sound edits and temporary settings still work; settings requiring a persisted restart are unavailable.
- The application's lowest erase/program guard permits only `[0xE0000, 0xE5000)` update staging. The JamBox loader permits only `[0x4000, 0x93000)` application bytes plus `[0xE4000, 0xE5000)` for the known resume record at `0xE4F00`. Boot/SPL, projects/settings and user sounds/sample regions stay outside those writes. Normal boot may clean stale OTA staging: the guarantee is no app-data writes, not zero flash writes.
- Unknown loader flash identity produces no writes, including cleanup. Record-shaped bytes outside the known JamBox record sector survive cleanup. Read, erase, readback or erased-byte verification failure returns `LDR_ERECORD=-15` and prevents the loader's success reset; current-session retry can finish without rewriting the app. The loader's existing startup RAM-record clearing remains unchanged. This does not guarantee power-loss recovery after a partially failed physical erase.
- An interrupted outgoing SysEx frame is closed before the next reply. Queue waits allow 500 ms; USB reset cancels stale queue publication. Rescue drains unconsumed MIDI notes/clocks so they cannot block incoming updater messages. Rescue still depends on an intact application's startup, flash driver and USB code; it is not an independent firmware bank or proof of ROM recovery.

## Reproduce

From the existing checkout:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/cloud/report_device.py > build/device-memory.json
sh tests/jam_storage_run.sh build/felucca.fwsc
sh tests/run_jam_emu_tests.sh
node web/test_web.mjs
```

The wrapper sets `JAMBOX_PROTO=1`, `FELUCCA_PERSIST=0`, `FELUCCA_FLASH=1`, `FELUCCA_OTA=1`, `FELUCCA_OTA_DRYRUN=0`, `FELUCCA_OTA_RAMONLY=0`, `FELUCCA_ICONS=0`, `FELUCCA_SLICE=0`. Without `--with-updater`, the default still disables FLASH/OTA and is diagnostic only. Both targets reuse `build/felucca.*`; preserve results before switching. Tests use synthetic flash/USB fixtures and never open a device connection.

## Results

| Check | Outcome | Evidence |
| --- | --- | --- |
| Update-enabled pi32v2 build and static memory/package checks | Exit 0; all static gates pass | [Build](build.log), [memory report](memory.json) |
| Clean source build, excluding untracked upstream artwork, sample pack and published firmware/site | Exit 0; app, loader, OTA, package and both ELF files byte-identical | [Build](clean-build.log), [comparison](clean-artifacts.json), [memory](clean-memory.json) |
| Default diagnostic wrapper after isolation changes | Exit 0 | [Build](diagnostic-build.log) |
| Real-code storage harness | 238 checks, zero failures; simulated raw NOR deliberately does not enforce policy itself | [Aggregate](storage-tests.log) |
| Sloop format positive control | Valid FUN6 accepted; valid-checksum JRM0 rejected | [Aggregate](storage-tests.log) |
| OTA protocol and whole-image isolation | 14 inherited assertions plus 8 isolation checks pass | [Aggregate](storage-tests.log) |
| Invalid configurations | Four expected compilation failures with specific policy diagnostics | [Aggregate](storage-tests.log) |
| Editor Save status, USB retry/reset, recovery and MIDI | Pass; eight new USB/recovery scenario groups include ISR-publication injection | [Aggregate](storage-tests.log) |
| Loader | 42 JamBox and 41 legacy checks pass, including cleanup fault/retry and full protected-region comparisons | [Aggregate](storage-tests.log) |
| Storage harness AddressSanitizer | 238 checks, zero failures; no address errors | [Log](storage-asan.log) |
| Browser instrument | 36 native checks and actual Chromium desktop/mobile/standalone tests pass | [Log](browser-tests.log) |
| Existing web editor, package parser and simulated installer | Exit 0 | [Log](web-tests.log) |
| Static target instruction-cost gate | **Exit 1**, IRQ estimate 256 versus inherited budget 174 | [Log](target-budget.log) |

The [first browser run](browser-initial.log) caught an obsolete exact-message assertion (`BROWSER: NO STORAGE`) after the shared UI began returning `STORAGE DISABLED`. The host stubs and exact assertion were aligned with that behavior, and the held-SAVE assertion now also requires an empty slot. The final full emulator run passes; no browser assertion was disabled. The loader cleanup regression also [fails against the old cleanup behavior](loader-cleanup-before.log) and passes after error propagation. [Recorded exit codes and flags](status.json) distinguish these outcomes. The storage ASan result is not a claim that all inherited DSP undefined-behavior checks are clean.

The existing web simulation ran against the interim milestone-2 package; final storage/OTA/loader simulations ran against the final package identified below. The full inherited Sloop runner was not repeated in this batch. Its known IRQ-budget and Chromium PWA installability failures remain documented in [milestone 1](../device-build/README.md). The final JamBox IRQ cost was rechecked and remains failed; no budget/golden was changed. The prior byte comparison found the old IRQ budget stale relative to shipped Sloop code. Static cost estimates cannot replace hardware timing measurements.

| Final update-enabled target region | Used bytes | Capacity bytes | Spare bytes |
| --- | ---: | ---: | ---: |
| Complete app load image | 552,952 | 581,564 | 28,612 |
| Ordinary static RAM | 80,600 | 98,304 | 17,704 |
| Large-buffer pool | 324,580 | 344,064 | 19,484 |
| RAM code | 2,784 | 24,576 | 21,792 |
| Retained NOINIT | 15,464 | 15,696 | 232 |
| Loader RAM including BSS | 19,632 | 81,920 | 62,288 |

The loader image is 8,400 bytes; its compressed OTA file is 7,092 bytes with a 7,060-byte body below the 19,968-byte body limit. Static ordinary-RAM/pool reserves exceed the reporter's 4 KiB/8 KiB minimums. NOINIT remains tight. These are link-time figures, not measured stack high-water marks, audio deadline headroom or hardware flash behavior.

Final app SHA-256: `cccb002ab54f4f8378426e6fe692c65305dd0d406da317c978488d38deb7db8f`. Final `.fwsc` SHA-256: `f1f763945eeb0206600468865f701b97dd86b8d793305acf87b0559bc909dffc`. This is same-day reproducibility; inherited `__DATE__` can change a later day's bytes. Only text logs, source and hashes are committed here. Binaries, compiler and generated sites are not published by this milestone; the live browser site is unchanged.

## Next gate

Milestone 3 must replace inherited identity/splash, define package/source/notices metadata, validate installer transitions and provide read-only device identification. The owner's Sloop version is still unknown. Physical backup/recovery preparation, owner-operated installation, USB/panel/audio testing and return to Sloop remain milestone 4. This simulation does not establish compatibility with every stock/custom firmware or make installation ready. See the [device plan](../../FM1_DEVICE_PLAN.md).
