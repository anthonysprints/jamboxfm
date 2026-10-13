# FM-1-RE audit for JamBoxFM — 2026-10-13

Inspected `AL-255/FM-1-RE` main at **`ec832f281ca165a7d5e0722985f3acb1465046d5`** in `/workspace/scratch/fm1-device-plan/fm1-re`. Also fetched, but did not check out, historical `with-custom-firmware` at **`628fcaf8945f7353ed86ea4fe2c723ddd704c8a5`**. Main JamBoxFM repository was read only. No firmware/toolchain downloads, hardware tools, MIDI ports, builds, upstream setup scripts, or flashing were used. The expressly authorized offline unittest suite and pure packet comparison were run after inspecting their entrypoints.

## Decision

Use FM-1-RE as additional protocol evidence, stock boot/flash research, and offline regression fixtures. **Do not switch JamBoxFM's working Sloop-derived hardware foundation to this repository.** Current main deliberately has no replacement firmware or custom image builder; the historical firmware branch explicitly says its custom firmware is not flash-ready. It supplies neither a new independently proven hard-brick recovery route nor an alternative verified compiler download that solves the cloud access problem.

- [Current main scope](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/README.md#L3-L12).
- [Historical firmware safety status](https://github.com/AL-255/FM-1-RE/blob/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5/README.md#L63-L69).

## Strong useful findings, with limits

### Correct target, distinct stock image and SDK

Package product `AC791N_STORY`, config `AC791N-v0.01-cfg_tool-v0.10`, and byte-matched SPL identify **JieLi AC791N/WL82, pi32v2**. `JL-BR22` inside libraries does not identify the board's SoC. Legacy BR22/AC693N flash scripts should not be adopted. Stock V13/V14 SPL matches SDK `AC79NN_SDK_V1.1.9_2023-08-01`, SHA-256 `730e54f0a439f58d147be4364ad21e19566945ada9d3a7bbc8371dce5068d3ef`, first present at SDK commit `3df0315c330de6e9b78bebf0ca395df9c093fb34`.

[Exact provenance and legacy-tool caution](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/TODO_aug2.md#L13-L35). This is stock lineage evidence, not a reason to replace Sloop's separately pinned SDK 1.2.1 blobs.

### One completed V15 return path; no hard-brick proof

The author reports a September 4 hardware run from a **locally modified stock-V15-derived package identifying as `FM-1_016`** back to stock `FM-1_015`. This is not an official V16 release. The application still ran. Although its USB configuration descriptor was broken, an already-active Windows descriptor filter still exposed the MIDI port. The live adapter was Windows.Devices.Midi; the author explicitly does not claim an ALSA transport hardware test.

Observed sequence in the upstream record: normal `4C4A:C755` V16 test identity → 48 staging data requests and `E0000000` → OTA `4D4A:4155`, `ota-FM-1_015` → 1,167 flash data requests and `F0000000` → normal `4C4A:C755`, separately queried V15 identity → valid 293-byte USB configuration descriptor and Windows Audio/MIDI enumeration. No audio playback test, flash readback, independent whole-chip wipe, or ROM recovery was performed.

Stock V15 package in that record is 699,956 bytes / 699,936 logical bytes, SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`. This hash identifies the precise upstream experiment; no stock firmware was downloaded or independently hashed in this audit.

- [Run conditions and limited conclusion](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md#L1-L15).
- [Recorded sequence, sizes and hashes](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md#L39-L67).
- [Offline-check and hardware-proof limits](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md#L95-L107).

This supports the statement that version restrictions are at least partly host policy for that path. It does not establish arbitrary official-version downgrades, Sloop→JamBoxFM→stock compatibility, interrupted-update recovery, or rescue when neither application nor loader runs. Root README's broad wording that rollback is not established predates/coexists with this narrower, newer successful path; cite the detailed report rather than flattening these into a universal claim either way.

### OTA partial-block framing: exact, useful regression coverage

Construct the entire decoded response before applying one continuous 8→7-bit packing operation:

```text
00 59 30 | body_length:u24le (= payload_length + 8)
flash_type:u8 | address:u32le | payload_length:u24le | payload
ones_complement(sum of bytes from flash_type through payload)
```

Surround the packed result with MIDI SysEx `F0`/`F7`. Payload must match requested length exactly, at most 512 bytes. The upstream failure was a final 481-byte request whose response wrongly declared 488 body bytes rather than **489**, causing the device to reject it. Fixed prefixes based on full blocks are insufficient.

- [Actual bounded packet builder](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/fm1_ota.py#L84-L100).
- [Independent contract checks for every supported length and 481-byte regression](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/tests/test_fm1_ota.py#L86-L113).
- [Captured cause and correction](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md#L7-L9).

**Our current packet builder already does this correctly.** An offline comparison of JamBoxFM `tools/fm1_install.py:response` against the pinned FM-1-RE `build_response` produced 1,028 byte-identical replies: lengths 0..512 for two address/flag pairs (`0x000AAB20/0`, `0xFFFFFFFF/0xFF`), plus `E0000000` and `F0000000` success replies. This validates pure response serialization, not transport behavior or device install readiness.

### Identity parsing and completion semantics: release checklist additions

FM-1-RE validates identity SysEx framing, decoded length 34, command `00 59 11`, declared body length 27, and one's-complement checksum; accepts stock zero-padded plain identities and the older encoded-field format. It contains one deliberately narrow exception for the author's exact modified `FM-1_016` frame that retained V15's old checksum. **Do not port that experimental exception into our general installer.** Our current Python `parse_identity` checks framing/decoded length/command, but does not check declared body length or checksum; this is a concrete release-hardening gap to audit along with the browser parser. Supporting older encoded identities should follow identified supported transition paths, not guesswork.

- [Identity parser and narrow exception](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/fm1_ota.py#L119-L162).
- [Malformed identity regression tests](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/tests/test_fm1_ota.py#L176-L201).

`E0000000` means verification complete and must not be reported as completed installation. `F0000000` occurs after the traced stock loader's flash write/finalize path. Reply with `success\0`, wait for normal-mode enumeration, and query model/version again. A terminal alone does not prove the intended image booted. The low-level loader's completion helper can return zero after retries even if the host did not provide the expected payload; its C return value alone is also insufficient.

- [Flash state machine, terminal handling and post-boot identity](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/fm1_ota.py#L454-L502).
- [Loader finish-gate disassembly interpretation](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/analysis/device/ota-loader/finish-gates.md#L34-L148).

Do not wholesale replace our installer with this client: `fwsc_info` is explicitly a best-effort metadata parse, not complete structural/CRC validation, and its stage-two model check strips `ota-` if present without unconditionally requiring the OTA prefix. Our package preflight, `FELUCCA-LOADER-1` marker and known-loader restrictions remain important.

[Metadata parser](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/fm1_ota.py#L183-L199); [stage-two identity handling](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/tools/fm1_ota.py#L433-L453).

## Flash map, RAM and recovery boundaries

The mapped V13 stock app is `0x8E59C` bytes, app data at flash `0x4120`, app area at `0x4000`, VM at `0x94000` (size `0x55000`), BTIF at `0xE9000`, USR at `0xEA000` (size `0x12000`), `key_mac` at `0xFF000`. Stock RAM `.data` begins `0x01C00000`, size `0x9E7C`; `.bss` begins `0x01C09E7C`, size `0x17380`; boot hardware info is `0x01C7FD50`.

These are V13 image-specific observations, **not replacement linker constants for Sloop/JamBoxFM**. Our inherited app slot/data boundary/entry differ (`0x8DFBC` app slot, `0x93000` data boundary, XIP entry `0x02000120`). Keep Sloop's linker, packer and storage map aligned until an explicit migration is designed and verified. Use linked ELF/map sizes and the actual packager bounds, not apparent spare space in a reverse-engineered stock image.

- [V13 scope/confidence](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/architecture.md#L3-L10).
- [V13 memory and flash map](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/architecture.md#L59-L100).

The SDK/SPL contains generic support for multiple directory heads, but the stock manifest/config has one application and no configured double bank. **A/B project storage is not A/B firmware rollback.** `RESET=PB01_08_0` means an active-low eight-second PB01 reset; `UPDATE_JUMP=0` selects reset, not a proved MaskROM recovery mode. Do not call the reset input a recovery button. The referenced `jl-uboot-tool` support table lists WL82/AC791N as unknown, and the stock debug-surface audit did not find a hidden factory/console route.

- [Single-bank finding and reset interpretation](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/TODO_aug2.md#L67-L104).
- [UBOOT tool support limit](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/README.md#L113-L116).

The author reports a **19,456-byte (`0x4C00`) forced loader-replacement staging limit** in historical modified-package probes. That is not proof that ordinary stock updates with a larger packaged compressed loader fail: stock V13/V14 reuse an identical loader. Do not copy the historical experiment's trimmed loader. Separately verify every supported transition's loader payload size, identity and known compatible loader marker.

[Experiment's stated scope and size boundary](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/11-ota-protocol.md#L149-L183).

## Controls and CPU: do not promote reverse-engineered guesses to board facts

Some documentation is internally inconsistent. Architecture overview says two encoders and two pitch/mod wheels; the detailed scanner sees seven encoder slots and 41 key positions. The ADC writeup first calls channels 3/4 wheels, then acknowledges channel 3 feeds battery measurement and exact assignment is unverified. Generic SMP locks likewise do not prove both cores are active in a given build. These are hypotheses in the stock classification, not evidence for extra physical controls.

Preserve the already inspected Sloop hardware mapping: 27 note keys + 14 buttons, seven rotary encoders without push switches, separate MASTER analog control, actual display/input drivers. Do not redesign our HOME/SELECT/Assist/K1 control plan around wheels, clickable encoders, or 41 playable note keys.

- [Overview containing speculative I/O summary](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/architecture.md#L18-L32).
- [41 matrix positions](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/07-input.md#L65-L77).
- [Seven encoder slots](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/07-input.md#L110-L126).
- [ADC claim and reconciliation](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/07-input.md#L156-L184).

## Compiler sources and reproducibility

The inspected setup script uses exactly the same official compiler endpoint already blocked in our runtime, `https://pkgman.jieliapp.com/s/linux-toolchain`, plus official `https://pkgman.jieliapp.com/s/linux-postbuild`. Official documentation link is `https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/build_download/index.html`. No alternate verified archive source or publisher archive hash was found. No download request was made during this audit.

Their documented/compiler-path evidence identifies **`jieli-linux-toolchains-20250324.1`**, vendor Clang/LLVM 4.0.1, `pi32v2/bin/cc`, `cxx`, `ld`, `nm`, and `common/bin/objdump`. Objects are `ELF32-pi32v2`, machine `0xf1`. Their installer follows moving redirect filenames, reuses a cached basename without checking contents, and extracts without a pinned archive digest. Use our verified TLS/download locking procedure, not this unpinned setup script. The post-build archive is relevant to their workflow and is not automatically a new Sloop packager dependency.

- [Official endpoints and installer implementation](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/scripts/setup_toolchain.sh#L5-L41).
- [Exact historical compiler path](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/scripts/disasm_toolchain_libs.sh#L5-L7).
- [Compiler layout, target and disassembler caution](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/04-toolchain-and-vendoring.md#L3-L57).

Useful research lesson: upstream reports the older Ghidra SLEIGH decoder misses the six-byte `80 ff` long call form. Their checked-in vendor objdump output is the better instruction-boundary reference; inferred function names and SDK-relative symbols still require corroboration.

## Historical custom branch is an experiment, not an alternate device backend

The actual historical Makefile builds a basic synth, LCD overlay and stock-app hooks (`demo.cpp`, `rt.cpp`, `hooks.cpp`, `synth.cpp`, `lcd.cpp`), explicitly **without Dexed in the device demo**. Dexed host render tests are a separate reference target. Its stock-V13 image builder patches absolute app callsites `0x02022CFE` and `0x0201F5F4`. The linker assumes XIP `0x02046600` inside the stock font/bitmap region and RAM `0x01C30000`, while the older builder describes writing a separate demo blob to USR `0xEA000`. These hard-coded experimental assumptions do not constitute a transferable replacement OS/HAL, nor evidence for JamBoxFM memory headroom.

- [Actual device and host build targets](https://github.com/AL-255/FM-1-RE/blob/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5/firmware/Makefile#L1-L85).
- [Experimental fixed linker layout](https://github.com/AL-255/FM-1-RE/blob/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5/firmware/link.ld#L1-L21).
- [Stock-specific patch builder](https://github.com/AL-255/FM-1-RE/blob/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5/tools/build_image.py#L1-L43).

## Licensing

Root authored material uses WTFPL v2, copyright AL-255. That does not independently establish redistribution rights for bundled stock firmware, vendor SDK objects, third-party submodules, or all historical synth files. For example historical `firmware/dexed/fm_core.cpp` expressly has Google Apache-2.0 notices. Prefer adopting protocol knowledge and independently written fixtures; if copying source, retain applicable per-file notices and audit the copied dependency chain. Do not assume bundled proprietary stock packages become freely redistributable because the research repository has a permissive root license. JamBoxFM's inherited GPL-3.0 code obligations remain.

- [Root license](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/LICENSE#L1-L13).
- [Example distinct source license](https://github.com/AL-255/FM-1-RE/blob/628fcaf8945f7353ed86ea4fe2c723ddd704c8a5/firmware/dexed/fm_core.cpp#L1-L15).

## Executed offline verification

After reading both complete test modules, the verification helper, `fm1_ota` import path, and the ALSA library import, ran:

```sh
cd /workspace/scratch/fm1-device-plan/fm1-re
PYTHONDONTWRITEBYTECODE=1 FM1_OTA_LOG=/dev/null python3 -m unittest discover -s tools/tests -v
```

**27 tests passed in 0.635 seconds.** Flash-state tests replace hardware paths with mocks/FakeLink. The ALSA layout test loads `libasound.so.2` and checks a structure; it does not open a port. Reflash-record tests validate the supplied redacted record and synthetic payload framing; they do not independently authenticate upstream's hardware run.

Log: `/workspace/scratch/fm1-device-plan/fm1-re-offline-tests.log`.

Pure response comparison described above: **1,028/1,028 byte-identical**. Log: `/workspace/scratch/fm1-device-plan/fm1-re-jambox-response-comparison.log`. No packet was sent.

## Proposed additions to JamBoxFM device gates

1. Resolve official compiler access and record actual archive digest/version. Build pinned, unchanged Sloop first; record successful ELF/map/image bounds before modifying hardware configuration.
2. Build JamBoxFM with its RAM-session-only configuration. Check actual code/RAM/noinit/stack budgets and runtime audio timing on a recoverable test setup before enabling persistent writes. FM-1-RE's stock memory gaps are not our budget.
3. Preserve Sloop's tested HAL, boot/loader markers and OTA identity compatibility; add firmware-family/release/hash manifest information without inventing a new USB vendor/model identity.
4. Audit Python and browser identity validation and add malformed-header/checksum cases. Preserve already-correct continuous 7-bit reply packing and cover all lengths including 481. Keep the post-boot query requirement, known-loader restrictions and full package checks.
5. Document separate transition evidence for current Sloop→JamBoxFM, future JamBoxFM updates, and return to known firmware. Recovering while the application or loader still enumerates must be distinguished from a dead application's independent recovery route. Never describe the stock PB01 reset as a proved rescue gesture.
6. Keep custom project schema/flash persistence behind its own tested migration gate. New family magic prevents misreading old projects but writing reused sectors still replaces old data; backups and explicit user action remain necessary.

These are engineering release gates. The external repository's own NO-GO prose is evidence about its experiments, not an extra approval requirement for already authorized read-only research or software implementation.
