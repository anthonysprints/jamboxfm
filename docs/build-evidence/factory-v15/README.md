# Exact factory V15 follow-up

The owner supplied `FM-1.fwsc` after a read-only report of `FM-1_015` and confirmation of the factory interface. The file passes the pre-existing V15 pin: 699,956 bytes, SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`. The file remains in the private external fixture cache; it is not committed or published.

Starting source: `7eebc7b`. This follow-up adds offline factory-file validation, three mock-only factory transitions, native factory-return staging tests and a strict loader completion token. The device page explains the reported factory identity without certifying capabilities. It still offers no installation action. [Preparation and remaining gates](../../FM1_FACTORY_V15.md) distinguish software checks from physical proof.

## Executed results

| Check | Result | Evidence |
| --- | --- | --- |
| Exact factory file and nested package audit | Size/product/SHA pin passes; relevant UFW, app, chip-key and OTA CRCs pass; stock loader matches the pinned FM-1-RE specimen | [Audit](BINARY-AUDIT.md), [metadata](binary-audit.json), [reproducer](binary_audit.py) |
| Pinned FM-1-RE historical capture with the actual uploaded bytes | Offline replay passes | [Log](upstream-capture-verification.log) |
| Real JamBox OTA stage one, normal and rescue, against exact V15 | 34 ASan/UBSan checks pass: all 16 KiB boot-head blocks, complete 19,937-byte official loader body, 481-byte tail, write bounds and refusal/cleanup faults | [Log](native-return-staging.log) |
| Same-size modified stock fixture | Hash gate refuses before native compilation | Runner checks exact size and SHA before loading the fixture |
| Loader final-token regression before fix | Expected failures in legacy and JamBox modes | [Legacy](loader-finish-before-0.log), [JamBox](loader-finish-before-1.log) |
| Loader final-token regression after fix | 47 legacy / 48 JamBox assertions pass | [Legacy](loader-finish-after-0.log), [JamBox](loader-finish-after-1.log) |
| Real portable JamBox loader with stock old-image and final rebuilt candidate | All 48 assertions pass with simulated NOR | [Log](native-loader-final.log) |
| Public policy and mock transition faults | 13 tests pass; original nine groups retained | [Log](policy-tests.log) |
| Final candidate and two exact external return files | Nine host simulations pass; every logical-image byte compared across 17 transfer stages | [Result and byte coverage](actual-candidate.json) |
| Final pi32v2 build and package verification | Exit 0; receipt/configuration/source/artifact and static memory gates pass | [Build](device-build.log), [package](candidate-package.log), [receipt](build-receipt.json), [memory](memory.json) |
| Packaged read-only page | 18 Chromium cases pass, including factory explanation, no extra probes, no claimed capabilities and the exact query allowlist | [Log](device-browser.log) |
| Publication package using local bare remotes | Seven tests pass, including firmware exclusion and source equality | [Log](publisher-tests.log) |

The loader previously treated any valid eight-byte final response as success. It now requires exactly `success\0` before clearing recovery records; `failure\0` and `success!` fail after the existing bounded attempts, preserve recovery/protected bytes, and permit a later correctly acknowledged retry. This is a functional loader fix. The separate source comments that promised cold-boot resume were corrected without adding any recovery implementation.

Final JamBox candidate: **610,489 bytes**, SHA-256 `ef282f4b58476498813ad6e79d2590b86f703b53cc2212e719a1c5c1f95156ab`. App image remains 547,552 bytes; loader image is 8,632 bytes, compressed OTA 7,333 bytes with a 7,301-byte body below its 19,968-byte limit. The earlier package audited in `BINARY-AUDIT.md` is identified there separately. The receipt's starting revision and dirty-input record describe the actual build; final source hashes are in [source-inputs.sha256](source-inputs.sha256).

The original six-transition candidate manifest is unchanged and still has no approved physical transitions. The three factory transitions live only in the mock helper and require exact prepared stock bytes. `FM-1_015` and `ota-FM-1_015` remain unrecognized by the public family classifier; a stock loader is accepted by the helper only as the expected stage of a pinned return from a recognized JamBox normal/rescue app. No real Web MIDI object can enter the helper.

The host request pattern covers every byte but is deliberately generic; it is not the proprietary factory updater's transcript. The native stock-to-JamBox fixture supplies a supported JEDEC and E4F00 record. Neither test discovers stock's actual staging location, executes the proprietary loader/SPL, or proves USB handover, cold-boot recovery, audio timing or saved-data preservation. The inherited IRQ-budget failure remains open; it was not relaxed or rerun for this loader-only behavior change. The prior inherited PWA and DSP sanitizer limitations also remain.

## Reproduce

Activate the existing checkout's tools. Obtain the exact external fixtures without adding them to source/site files:

```sh
. tools/cloud/activate.sh
sh tests/jam_factory_ota_run.sh /workspace/deps/jambox-return/FM-1-V15.fwsc
node --test tests/jam_device_policy_test.mjs
bash tools/cloud/build_device.sh --with-updater
python3 tools/package_jam_device.py --no-build
node tests/jam_device_candidate_check.mjs
cc -O2 -DJAMBOX_PROTO=1 -o build/jam-factory-ota/loader-final tests/ldr_test.c
build/jam-factory-ota/loader-final /workspace/deps/jambox-return/FM-1-V15.fwsc build/felucca.fwsc
python3 tools/package_jam_emu.py
JAM_DEVICE_SITE_ROOT="$PWD/build/jambox-web" node tests/jam_device_browser.mjs
python3 tests/test_publish_jam_web.py
```

The policy suite accepts `JAMBOX_SLOOP_PACKAGE` and `JAMBOX_FACTORY_PACKAGE`; the candidate checker accepts bundle, Sloop and factory paths as its three positional arguments. In this run the existing emulator WASM was reused because no emulator behavior changed; its bytes were checked against the prior tested build. The target firmware and final candidate were rebuilt and verified separately.

## Recovery research

Follow-up: the owner confirmed Windows, Python 3.12.7 and no saved sounds/settings
they need to preserve. The [Windows preparation validation](windows-backup-validation.md)
records the separate mode-entry helper, pinned local preparation, and fixes for
short SCSI transfers. The original upstream double-read check alone could accept
repeatable zero-padded short reads. No owner-device mode change or backup has
been performed by these cloud checks.

[Factory backup/recovery audit](recovery-options-audit.md) and [Windows preparation](windows-recovery-prep.md) record pinned source evidence. Windows Unbricker can back up a unit already in ROM using a temporary RAM loader and two matching flash reads; the current identity check does not establish that access. No owner device mode change, RAM loader, flash dump, installation, or physical recovery test was performed here. The manufacturer update file is not a private-data backup.

No canonical manufacturer firmware, uploaded device report, extracted flash, proprietary compiler, or device installation package is included in these evidence files.

The final source was pushed as `6d56718`. Website commit `c07e0f4cebc4f790c03cddae9df0e6a5db824c29` publishes the read-only explanation and corresponding source. Eleven public HTML/module/WASM/checksum responses returned HTTP 200 and matched the tested package; see [publication verification](publication.json). Device firmware remains unpublished.
