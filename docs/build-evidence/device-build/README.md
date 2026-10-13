# First FM-1 target build evidence

October 13, 2026, Debian 13 x86-64. **The compiler and target builds work. The inherited full suite is not green. No device was connected or flashed, and no firmware release was published.**

The unchanged baseline is Sloop `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`. JamBox's application source is unchanged from `adc6f60766c64b99e799de22b0cc86c6bca9186a`; this follow-up changes build/setup tools and documentation. [Exact build-input hashes](clean-compiler-inputs.sha256) and [external inputs](clean-external-inputs.json) identify the clean exported working-tree snapshot, rather than pretending the uncommitted helper changes were already in that source commit.

## Compiler and reproduction

The owner uploaded `jieli-linux-toolchains-20260730.1.tar.xz`, 26,027,508 bytes, SHA-256 `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`. It reports Clang 4.0.1; the builder explicitly selects `-target pi32v2`. The official endpoint redirected to the Alibaba artifact host, where the cloud proxy denied access. The [acquisition lock](toolchain-lock.json) therefore records **owner-provided archive**, not a locally observed HTTPS download or independent vendor signature. The three pinned SDK binaries also verified; [SDK provenance](sdk-files.json).

The importer checks the requested digest, safely extracts the archive, confines symlinks to its release directory and compares all cached files/modes/links with a fresh verified extraction before execution. It rejects mismatches and leaves no acquisition lock after malformed input. [Thirteen focused tests](toolchain-tests.log) pass. `bash tools/cloud/setup.sh --device` successfully replayed with this cache: [setup log](setup-replay.log). Fresh online acquisition remains subject to runtime network access; [manual import instructions](../../CLOUD_SETUP.md).

From the existing JamBox checkout:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh
python3 tools/cloud/report_device.py > build/device-memory.json
sh tests/run_jam_emu_tests.sh
```

The wrapper sets `JAMBOX_PROTO=1`, `FELUCCA_FLASH=0`, `FELUCCA_OTA=0`, `FELUCCA_OTA_DRYRUN=0`, `FELUCCA_ICONS=0`, `FELUCCA_SLICE=0`. **This diagnostic target has app persistence, normal updating and app rescue disabled. Its `.fwsc` must not be installed.** The loader is built separately, but its presence in the package does not enable a return path in the running application.

## Build and memory results

Both builds exited 0: [unchanged baseline](baseline-build.log), [JamBox](jambox-build.log). The baseline used its default feature flags; JamBox disabled the features listed above. Size differences do not measure the cost of adding JamBox alone.

| Region | Capacity (bytes) | Sloop used | JamBox used | JamBox spare |
| --- | ---: | ---: | ---: | ---: |
| Complete app load image | 581,564 | 573,052 | 533,012 | 48,552 |
| Ordinary RAM, static | 98,304 | 92,696 | 70,520 | 27,784 |
| Large-buffer pool | 344,064 | 333,900 | 324,580 | 19,484 |
| RAM code | 24,576 | 2,964 | 0 | 24,576 |
| Retained NOINIT | 15,696 | 15,464 | 15,464 | 232 |
| Loader RAM span, including BSS/alignment | 81,920 | 19,360 | 19,360 | 62,560 |

The new read-only reporter passes for [Sloop](baseline-memory.json) and [JamBox](jambox-memory.json), along with [12 boundary/corruption tests](memory-tests.log). It checks ELF section boundaries/types, entry/load addresses, binary agreement with ELF contents, stack/guard addresses, at least 4 KiB ordinary-RAM spare and 8 KiB pool spare, unresolved/helper symbols, and loader placement. It also validates compressed OTA header/body CRCs and decoded bytes; complete `.fwsc` integration remains covered by the inherited builder/OTA tests. The loader image is 8,140 bytes; its `.bss` is 11,216 bytes with four alignment bytes. `ota.bin` is 6,863 bytes, with a 6,831-byte body below the 19,968-byte staging limit.

The 232-byte NOINIT margin is particularly small. Project slots occupy 15,264 bytes; other retained state and padding occupy 200. Future persistence changes need a new map check. Static stack reservations are not runtime high-water measurements, and the empty RAM-code section follows disabled application flash support.

## Clean-source comparison

A fresh export contained only Git-tracked working-tree files. It excluded the checkout's untracked upstream icons, artwork, sample pack, website and firmware. With icons and SLICE disabled, it built successfully: [log](clean-build.log), [status](clean-build-status.json). The app, loader, OTA file, `.fwsc` **and both ELF files are byte-identical** to the main build: [comparison and hashes](clean-artifact-comparison.json).

The JamBox app SHA-256 is `c7d33b68f761e92896a4c65965cbb81cec5f0db4467a7cb332493c0fce2f2ba1`; its diagnostic package is `e30e6e603ab3d367f8a0c97455486807bdbb3af4d6b02e4be35806067ec0b17f`. This demonstrates same-day/source/toolchain reproducibility: inherited `__DATE__` text changes bytes on a different build day. Binaries, ELF/disassembly and the compiler remain in the workspace, outside the public evidence directory. The inherited Sloop splash/package identity still need replacement before an installation candidate.

## Test outcomes and open gate

- JamBox emulator: **36 native checks and real Chromium tests passed**, including SNAP LEDs, AudioWorklet output, controls, desktop/mobile and standalone document: [log](jambox-emu-tests.log). This follow-up did not change or republish the live website.
- Unchanged Sloop `sh tests/run_tests.sh`: **exit 1**, preserved in [full log](baseline-tests.log) and [exit status](baseline-tests.exit). Native state/storage, bootguard/recovery, simulated OTA/loader and simulated installer/editor checks passed. All 182 golden renders were unchanged. Host CPU results include 19 advisory notes; they do not measure physical FM-1 timing.
- The target instruction-cost gate failed for `fm1_alnk0_irq`: Sloop **268 versus budget 174**, with a 10% allowed increase. The current JamBox diagnostic target separately reports **256 versus 174**: [log](jambox-target-budget.log). Other budgeted render functions pass; three FM6 functions have no stored budget and are not counted as passes of a budget gate. No budget or golden was rewritten to hide the failure.
- [IRQ investigation](irq-diagnosis.md): the rebuilt baseline matches the published Sloop 2.6 app except one build-date byte. Its entire 710-byte IRQ function is identical. The old budget therefore disagrees with already-shipped upstream code; this is not new compiler growth. The estimator combines conditional backward-branch spans and is not a physical CPU measurement. The inherited gate remains failed pending an explicit budget/performance review.
- The full runner also failed its final PWA launch because upstream's hard-coded/default Chromium locations were absent. An [external executable-only adapter](pwa-system-chromium.mjs) reran the unchanged test with `/usr/bin/chromium`: **14/15 assertions pass, exit 1**. The remaining installability assertion receives `in-incognito` from Chromium 154 for the test's nonpersistent context. All offline/cache/update assertions pass. [Runtime details](pwa-runtime-notes.md), [log](pwa.log), [versions/status](pwa.environment.json). Neither the assertions nor browser context were changed to hide that compatibility failure.

Compilation, reproducible artifacts and static memory gates are complete. The instruction-cost discrepancy requires a documented resolution before declaring all baseline gates passed. Physical audio deadlines, dynamic stack use, panel/USB behavior, compatible firmware updating and recovery remain untested. Next implementation work follows [milestones 2–4](../../FM1_DEVICE_PLAN.md), with app-data writes separated from updater support before any owner-device trial.
