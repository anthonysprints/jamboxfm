# JamBoxFM: first physical FM-1 build

Research and plan: October 13, 2026. Starting source: `5b24d1e82efa785bb79a820b7eba2c9df8bb26ec`.
Status: milestone 1 target builds passed; milestone 2 implements update/rescue support with app-data persistence disabled; milestone 3 implements JamBox identity, local candidate packaging, transition simulations and read-only inventory. See [baseline build outcomes](build-evidence/device-build/README.md), [update-isolation evidence](build-evidence/update-isolation/README.md) and [candidate evidence](build-evidence/candidate/README.md). The next milestone is physical-trial preparation. The outputs remain software bring-up artifacts without approval for owner installation. No physical device has been flashed or tested.

## Decision

Keep the existing JamBox/Sloop instrument and hardware foundation. Use Dinghy as a reference for separating the instrument from boot, update and recovery services. Use FM-1-RE to cross-check the update protocol and its failure cases. Replacing the whole instrument with Dinghy would mean rebuilding the sequencer, drums, chord handling, effects and UI we already have.

The first owner-device candidate will run the current Dusty session: drums, bass, chords, Lead with SNAP, recording and sound controls. Musical sessions may remain temporary for this first test. **USB updating and application rescue must work before that candidate is offered for installation.** Saving a song and installing firmware are different flash operations; disabling song saving must not disable the way back to Sloop.

“Temporary session” means the song lives in RAM. The firmware itself still installs into flash and executes from it. Neither the current JamBox wrapper nor Dinghy provides a browser-style, flash-free trial of the complete instrument on the FM-1.

## What the two repositories contribute

| Source inspected | Useful to JamBox | Boundary |
| --- | --- | --- |
| [Dinghy](https://github.com/zvenson/dinghy/tree/d44df7db09b98ccad2f1233b8f83f5f09c04829e), commit `d44df7d` | Small, readable example of boot, physical controls, USB audio/MIDI, update staging and early rescue around a tiny synth. Its platform is from the same Felucca/Sloop family. | Its README reports physical testing of Dinghy 0.3; that is upstream evidence, not a test of JamBox. Its four-voice sine app and 256-byte settings format do not replace our instrument or project storage. |
| [FM-1-RE main](https://github.com/AL-255/FM-1-RE/tree/ec832f281ca165a7d5e0722985f3acb1465046d5), commit `ec832f2` | Hardware/stock-firmware analysis, update packet framing, an observed V15 reflash record and offline protocol tests. | Main is a research/tooling repository, not a replacement firmware builder. Its historical custom-firmware branch explicitly says it is not flash-ready. A successful reflash is not proof of recovery from a broken app. |
| [Our pinned Sloop baseline](https://github.com/isod89/sloop-fm1/tree/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e) | Existing DSP, sequencer, panel, HAL, package builder, update loader, installer transport and tests. | JamBox now separates app persistence from update support and adds software isolation tests. Neither inherited code nor those simulations establishes physical recovery. |

Dinghy's code is GPL-3.0-only, matching our existing code licensing. Preserve file headers, credits and corresponding source for distributed firmware. Its logo is separately reserved; use JamBox artwork. FM-1-RE's root license is WTFPL v2, but that does not relicense manufacturer firmware or separately licensed submodules. Use its findings and appropriate test cases; do not ship its stock firmware dumps as JamBox assets. Keep the existing SDK, fonts, sample and third-party notices.

## Findings that change the order of work

1. **The compiler blocker is resolved in this instance.** The owner uploaded the 20260730.1 archive from the documented vendor download. Its recorded hash and explicit owner-supplied provenance are in [the build evidence](build-evidence/device-build/README.md). All SDK binary hashes pass and both target builds compile. Fresh online acquisition still depends on access to the vendor redirect host; no alternative compiler distribution was substituted.
2. **The wrapper now has two software targets.** Its default retains `JAMBOX_PROTO=1`, `FELUCCA_FLASH=0`, `FELUCCA_OTA=0`; `--with-updater` enables flash and OTA while keeping `FELUCCA_PERSIST=0`. With OTA off, repeated boot failures reach ROM UBOOT rather than the small application USB rescue. ROM entry is not a normal Web MIDI installer. Neither target is an installation release.
3. **Update support is separated from app-data persistence.** `flash_boot()` initializes hardware capability independently. JamBox's `persist_boot()` clears retained musical state and imports only validated panel calibration; inherited project/settings/bank imports and app-data writers are gated by `FELUCCA_PERSIST`. This closes the behavior that merely enabling the old flash flag would have exposed.
4. **Dinghy demonstrates the useful separation.** Its independent updater informed this change in our existing platform. We did not adopt Dinghy's app-settings format or its sector at `0xFC000`, which Sloop already uses for settings/calibration.
5. **Recovery has limits.** OCT-minus rescue runs code in the installed application. It is not a second, protected firmware bank. A failure before that code can run may need a separate ROM recovery tool and physical adapter. We must establish the actual return path before an owner-device trial.

## Milestone 1: reproducible target builds, with no device writes — completed build work

Owner: development environment / agent. No connected FM-1 required.

### Resolve and record the compiler

Preferred path: apply the environment's required access to `pkgman.jieliapp.com`, then download the official Linux x86-64 pi32v2 toolchain over verified HTTPS. If the response redirects, record the actual destination and add only the required access. Retain the resolved URL, archive SHA-256 and `clang --version` through our existing preparation script. A locally recorded checksum pins the downloaded bytes; it is not an independent vendor signature.

The completed path used `prepare_toolchain.py --archive` for the owner's upload, with an explicit SHA-256 check. See [cloud setup](CLOUD_SETUP.md) for the exact command and provenance. The reusable installer now pins those bytes and verifies an existing extraction before reuse.

If this environment cannot reach the vendor, use a **manual GitHub Actions artifact build** on an explicitly selected Linux x86-64 runner. This is a proposed fallback, not an existing or tested workflow. It should use the same preparation/check scripts, pinned action revisions, minimum repository permissions and a compiler cache keyed by the recorded archive hash. Cache hits must still verify that hash. It must build and upload artifacts only; it must not flash a device, publish a release or deploy an installer automatically. Test vendor access on that runner before promising this route works. Windows/WSL or Linux on a local computer remains a further fallback, not a requirement for the owner to learn firmware development.

Existing preparation commands, to run after access is available:

```sh
cd /workspace/jamboxfm
. tools/cloud/activate.sh
python3 tools/cloud/prepare_sdk.py
python3 tools/cloud/prepare_toolchain.py
python3 tools/cloud/check_device.py
```

The SDK and tools stay outside the source checkout. Do not disable TLS, substitute an ordinary ARM compiler or invent a toolchain checksum/version from another project's notes.

### Establish a baseline and measure JamBox

Build the unchanged Sloop commit in its existing research checkout after checking its commit and local changes. Preserve that build's outputs before building JamBox; the inherited output names overlap.

```sh
cd /workspace/scratch/fm1-research/sloop
. /workspace/jamboxfm/tools/cloud/activate.sh
git rev-parse HEAD   # must be 8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e
bash build.sh
sh tests/run_tests.sh

cd /workspace/jamboxfm
bash tools/cloud/build_device.sh
sh tests/run_jam_emu_tests.sh
```

The milestone 1 target builds passed; the evidence record lists the full test outcome and clean-source comparison. The full inherited suite still has the documented stale IRQ budget and Chromium PWA assertion failures; neither was relaxed. The default `build_device.sh` output remains diagnostic with OTA disabled. Milestone 2 adds the separate `--with-updater` target below. Do not distribute either as a release. An unchanged Dinghy target build remains an optional diagnostic if a future compiler/platform failure needs isolation; installing Dinghy on the owner's FM-1 is not required.

Record the source SHA, compiler/SDK hashes, flags, generated-asset inputs, app and loader ELF files, disassembly, section sizes, `.fwsc` checksum and test logs. Add explicit checks for all linker regions, loader BSS/stack separation and unsupported arithmetic helpers; a desktop syntax check cannot establish these. Keep the existing device memory map and pool margin:

| Region / constraint | Candidate gate |
| --- | --- |
| App XIP slot | At most `0x8DFBC` bytes (581,564 bytes), including its load images |
| Ordinary RAM | 96 KiB region; proposed minimum 4 KiB remaining after static allocation |
| Large-buffer pool | 336 KiB region; retain the existing minimum 8 KiB spare |
| RAM code / retained state / stacks | Measure their own regions and guards; do not count them as spare ordinary RAM |
| Loader | Must fit its RAM region, including BSS, and not execute code from flash while rewriting it |

Do not relax the linker map or audio/CPU baselines to make a failing build pass. Rebuild from a clean source export without the checkout's untracked upstream artwork. Force `FELUCCA_SLICE=0` and `FELUCCA_ICONS=0` in the JamBox target, as in the browser, and verify those assets are absent from the deliverable. Milestone 3 supplies the JamBox splash and identity; preserve those checks when preparing later candidates.

Deliverable: archived baseline and JamBox build reports. No hardware writes.

## Milestone 2: updateable JamBox with temporary musical sessions

Owner: agent. Software implementation is present; final target sizes, hashes and exact regression outcomes are recorded in [update-isolation evidence](build-evidence/update-isolation/README.md). This does not complete the owner-installation or physical recovery gates.

The current hardware foundation and linker layout are retained. `app_policy.h` adds `FELUCCA_PERSIST`: it defaults to `FELUCCA_FLASH` for Sloop and is forced off for JamBox. `flash_boot()` probes hardware independently of app-data loading. The builder passes all relevant flags to the target compiler and selects the JamBox-specific loader policy.

```sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/cloud/report_device.py > build/device-memory.json
sh tests/jam_storage_run.sh build/felucca.fwsc
sh tests/run_jam_emu_tests.sh
```

| Area | Implemented policy / validation boundary |
| --- | --- |
| `felucca.c`, `project.c`, `main.c` | Independent flash probe, fresh Dusty initialization, retained project/settings clearing on every normal JamBox boot, no foreign project/bank import. Internal snapshots use private `JRM0` magic rather than Sloop `FUN6`. |
| Save/autosave/settings/presets/samples/FM6 banks/editor restore/reset hooks | Explicit save/erase/restore/upload operations reject with storage-disabled status before bank mutation. Autosave and pre-reset app-data writes are excluded. Live settings and calibration edits are labeled RAM-only. |
| Panel calibration | Full storage-record CRC and exact recognized schema length are checked; only a PAN5 mapping with unique in-range button/encoder IDs and directions ±1 is imported. Unknown/damaged records use defaults. No settings object is rewritten. |
| Update staging | Separate hooks and the app's lowest flash-driver range allow only `[0xE0000, 0xE5000)`, including its resume record. App storage hooks reject writes even if accidentally called. Normal boot can clean stale OTA staging. |
| Package loader | JamBox writes the app at `[0x4000, 0x93000)` and cleans only the known record sector `[0xE4000, 0xE5000)`. Unknown JEDEC identity refuses writes, including cleanup. Read/erase/verification failures during cleanup prevent a success reset and leave the current loader session available for retry; they do not prove power-loss recovery after a partial erase. |
| Early rescue / boot guard | The update-enabled build retains OCT-minus and boot-failure entry to the polled updater before synth startup, complete-message draining, and fallback ROM entry. Actual physical enumeration/recovery remains untested. |
| SysEx transport | Timeout/retry closes abandoned outbound framing, bus reset clears stale state, and new transfers do not inherit an old tail. Software regressions exercise real USB/parser and rescue paths, including bounded stalls. |

The important property is **app-data writes disabled and firmware update writes bounded**. Normal-boot OTA cleanup is also a permitted staging write. Software tests do not prove on-device flash or USB behavior.

The retained-state policy protects musical projects in both directions: JamBox clears inherited slots and Sloop rejects `JRM0`. Settings and panel mappings still have shared `SET3`/`PAN5` layouts. If Sloop has no valid flash settings, a warm return may retain JamBox's temporary UI preferences or calibration. Preserve the owner's backup and validate the actual return transition; do not call every retained field family-private.

The wrapper explicitly sets `FELUCCA_OTA_DRYRUN=0` and `FELUCCA_OTA_RAMONLY=0`; JamBox's policy rejects either mode with OTA enabled. Record the compiled values in the future manifest. A dry-run updater cannot complete installation. Despite its name, RAMONLY changes the updater's resume record, not where songs are stored; it removes the flash record needed for power-loss resume.

`tests/jam_storage_run.sh` uses synthetic foreign settings/projects/banks/samples and the real instrument/UI/editor code with simulated NOR. Coverage includes normal and warm boot, disabled SAVE/autosave/editor requests, protected-byte comparisons, valid calibration and corruption, Sloop's positive `FUN6` control versus rejected `JRM0`, staging writes and refusal outside bounds. Its companion OTA/loader/USB/parser regressions cover malformed transfers and failure handling. Use the final evidence record for the complete command list and results; do not report an early subset as the finished suite. Browser regressions remain a separate command.

Deliverable: software bring-up builds and isolation evidence, with no persistent song saving. Candidate identity/packaging and installer compatibility are milestone 3. No installer publication or hardware writing belongs to this milestone.

## Milestone 3: package identity and installer simulations — software implemented

Owner: agent. Identity/splash, local candidate packaging, simulated transitions and read-only inventory are implemented. [Candidate instructions](FM1_CANDIDATE.md) describe the workflow and limits; [candidate evidence](build-evidence/candidate/README.md) records exact commands, outcomes and artifact hashes. The site package includes a read-only `/device/` page, with intended URL `https://anthonysprints.github.io/jamboxfm/device/`; packaging alone does not publish it. The page has no Install control or firmware download.

The implementation reuses the inherited `.fwsc` builder and host transport. The local candidate packager builds firmware in the cloud; browser identification sends USB MIDI SysEx queries on the owner's computer. This cloud environment cannot directly access that computer's USB port. Actual transfer orchestration for milestone 3 is confined to test helpers requiring simulated MIDI access.

Implemented software boundaries:

- Normal app/package `FM-1_700`, app rescue `FM-1_000`, and loader `ota-FM-1_700` have a documented JamBox version/name mapping. An exact read-only `JAMBOXFM` family response binds schema, mode, release, capabilities and current transport identity. The numeric choice is not a registered allocation and is insufficient by itself to recognize JamBox. The splash and USB/INFO labels identify JamBox without inherited artwork.
- Host parsers check exact framing, canonical packed bytes, declared body length and checksum. No exception accepts FM-1-RE's deliberately modified experimental `FM-1_016` checksum. Normal JamBox, rescue, loader and unknown firmware remain separate cases; Sloop recognition requires the exact audited 2.6 INFO profile as well as its transport identity.
- Local packaging binds current source/generator inputs, compiled app/loader flags, pinned toolchain/SDK identities, artifact hashes, ELF/memory checks and canonical package reconstruction. The bundle includes corresponding source, notices, manifest, build receipt, memory report and checksums. Hashes are reproducibility records, not digital signatures. The manifest disables release/hardware approval and lists no approved physical transitions.
- Software policy preserves AC791N/WL82, the supported 1 MiB flash JEDEC `0x856014`, and package key `0x980F`; the loader retains its device chip-key check before erasing. The printed FM-1 model and software inventory do not establish the owner's hardware revision or actual flash behavior.
- Simulations cover JamBox normal/rescue/loader to JamBox, exact Sloop 2.6 to JamBox, and JamBox normal/rescue back to the pinned Sloop 2.6 package. They exercise both transfer stages, selected pairs after re-enumeration, partial final blocks including 481 bytes, invalid lengths, cancellation, stalls, disconnects and incorrect completion identity. The synthetic package fixture checks host behavior; the actual built candidate has separate validation.
- The device page requires explicit input/output selection, permits only read-only identity/capability queries, handles cancellation and stale replies, and exports an identification report without port names, port IDs or serial metadata. It provides backup guidance and leaves unknown firmware unrecognized. A report is not a musical-data backup.
- Candidate firmware stays in ignored `build/jambox-device/`, outside the public `build/jambox-web/` site. The inherited Sloop package is not published as JamBox. The Python CLI rejects the unmodified JamBox candidate product before MIDI access, including with `--force`; its inherited generic Sloop installer remains separate from the JamBox simulation policy.

No hardware installation or firmware release follows from these checks. Any future operational installer must enforce matching package/manifest, exact supported transition, backup and recovery gates before writing. Firmware publication and physical installation remain separate steps; do not claim firmware-hub inclusion.

## Milestone 4: prepare and perform the first physical trial

Owner: agent prepares build and test sheet; owner operates their FM-1 and local browser. This is the next milestone, and its physical gates are pending. Current installed Sloop version is still unknown; it is needed to select the supported transition and backup tools, not to continue software work. The milestone 3 device check can collect read-only inventory, but it does not authorize a write.

Before the first write, record the actual firmware identity/version and computer/browser. The earlier screenshot suggests Windows; confirm before writing local instructions. Use the installed version's compatible editor to export the complete available backup: settings/calibration, working project and slots, presets, FM6 banks, user kits, MIDI mapping and user samples where supported. Validate lengths/checksums offline and retain the exact known-good Sloop package. An editor backup is not automatically a complete raw-flash image; inventory what it contains and what it omits. Keep personal backups off the public repository.

Establish two recovery levels explicitly:

- **Normal app / app-rescue return:** a prepared, compatible Sloop package and tested installer path from our candidate, including OCT-minus rescue before synth startup.
- **Failure before app rescue:** a concrete ROM recovery route. The previously researched [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter/tree/a632d923203170e3a08565f4056dbb3fd3d65ca5) uses a separate XIAO RP2040 connection and loader; it is not just the normal USB cable or a browser button. Its documented setup and host support must be checked for the owner's machine. Neither this session nor FM-1-RE's reflash record demonstrates recovery on the owner's unit. If that equipment/path is unavailable, keep early bring-up on a development/spare device rather than calling the owner's only unit covered.

If Transporter is the chosen fallback, obtain and verify a device-specific full raw flash dump before the first firmware write and retain it privately. Its writer requires that reference as `--ref backup.bin`; an editor project export cannot substitute for it. Preparing that dump itself requires the appropriate adapter/setup, so it is part of establishing recovery, not a step the cloud environment can perform remotely.

With the candidate package, source/checksum, compatible backup and actual recovery setup available, conduct a supervised installation using a stable USB data connection and sufficient power. The owner performs the physical Install action. Do not use a stock-V15 reset as a universal prerequisite; decide only from the verified current firmware and transition.

First-session checklist:

1. Read the running Sloop identity, preserve the backup and confirm its package; test the current device as a baseline before changing it.
2. Install the JamBox candidate; verify its actual post-restart identity, boot screen and fresh Dusty session.
3. Check all 27 keys, 14 buttons, seven encoders, MASTER, octave controls, display and LED behavior. Verify D-sharp major/SNAP: D-sharp does not light E; E shows its correction to D-sharp.
4. Start drums, record two chords, add bass, play Lead and turn every exposed sound/effect control. Check release, stop/start, mute and sustained/repeated notes.
5. Check speaker/headphone audio, USB MIDI, TRS MIDI, USB audio and cable reconnect. Verify only the advertised USB sample rates.
6. Exercise application rescue normally, reinstall the same candidate, then return to the exact known-good Sloop package. Check backed-up data and calibration; restore only through the matching supported tools if needed.

No deliberate power-cut test on the owner's only instrument. Test transfer-failure cases in simulation first; real destructive/interruption testing belongs on recoverable development hardware.

## Milestone 5: performance and persistence before a regular release

Measure real device behavior under at least ten minutes of bass, rapid four-note chords, Lead, six drum voices, maximum supported effects, active screen/knobs and USB recording. Retain the existing 256-frame audio halves: about 5.805 ms at 44.1 kHz. Measure total audio-interrupt duration including nested USB/timer work; the UI's render-only CPU percentage is insufficient.

Provisional acceptance targets from the implementation plan: p99 under 70% of that deadline, maximum under 85%, zero late DMA halves and no CPU-overload shedding in the supported workload. Record stack guards, actual remaining RAM, audible glitches and end-to-end note latency. Host timings cannot pass this gate.

After the temporary-session build and return path work, implement versioned JamBox saving separately, using the existing [persistence design](IMPLEMENTATION_PLAN.md) and [validation gates](VALIDATION_PLAN.md). Check the proposed 3,840-byte JAM1 object and retained-state layout against the actual target build before committing to them. Test A/B torn-write behavior, corrupted/foreign data, explicit initialization/import, export/restore dependencies and save/reload across power cycles. Do not silently treat a Sloop project as a JamBox project.

Only then label a version as a regular device release, with an explicit supported-firmware transition list and known limitations. Broader musical features such as new palettes, expanded Assist and complete undo can proceed independently; they need not all delay the first controlled device test.

## Ordered implementation batches

1. **Toolchain/build evidence:** target builds and clean-source checks completed; retain the inherited IRQ/PWA failures and compiler provenance.
2. **Platform isolation:** software implemented; final build/test evidence covers separate update flash, disabled app persistence, write boundaries, retained state, calibration and rescue. Hardware remains untested.
3. **Candidate packaging:** software implemented: JamBox identity/splash, manifest/source/notices and build receipts, strict host framing, simulated transition matrix and read-only identification page. Exact integration outcomes are retained in candidate evidence; installation remains disabled.
4. **Hardware trial — next:** inventory/backup/recovery setup, baseline observations, separately prepared candidate installation, functional/performance checks and return to Sloop. All physical validation remains pending.
5. **Persistence/release:** schema, power-cycle checks, supported transitions and normal release packaging.

The first three batches are work the agent can prepare without asking the owner to code. The physical connection and observations in batch 4 require the owner or a development device. There is no hardware completion date implied by this plan; compilation and measurements determine whether optimization or driver fixes are needed.

## Source references and evidence limits

The original research pass ran FM-1-RE's inspected offline test entrypoints: **27 tests passed**. A pure serialization comparison produced **1,028 byte-identical replies** between its packet builder and our then-current Python installer, covering lengths 0–512 with two address/flag pairs and the two completion replies. That established the short-block serialization contract. Later timeout, framing and identity changes have their own [candidate evidence](build-evidence/candidate/README.md). These checks opened no MIDI connection and do not validate a hardware installation. [Original logs and reproduction details](build-evidence/device-plan/README.md).

Detailed repository audits: [Dinghy](research/dinghy-device.md) and [FM-1-RE](research/fm1-re-device.md).

- Dinghy [README/status](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/README.md), [platform separation](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/firmware/src/platform.c), [rescue implementation](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/firmware/src/recovery.c), [USB transport](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/firmware/src/usb.c).
- Dinghy [identity guidance](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/docs/MAKE-IT-YOURS.md), [chip rules](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/docs/RULES.md), [licensing](https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/LICENSING.md).
- FM-1-RE [scope and recovery limits](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/README.md), [V15 reflash record](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md), [protocol tests](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/tests/test_fm1_ota.py), [historical experiment](https://github.com/AL-255/FM-1-RE/tree/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5).
- Local starting implementation: [prototype flags and OTA wiring](../firmware/src/felucca.c), [boot/rescue selection](../firmware/src/main.c), [inherited persistence](../firmware/src/project.c), [loader write/cleanup paths](../firmware/loader/ldr_core.c), [build checks](../tools/build.py), [device wrapper](../tools/cloud/build_device.sh), [compiler setup](../tools/cloud/prepare_toolchain.py).

Research reads actual source at the pinned commits. Upstream claims of successful hardware tests remain attributed to those projects. The subsequent target builds and memory checks have their own evidence record; they do not establish physical-device behavior. No device was connected, flashed or recovered.
