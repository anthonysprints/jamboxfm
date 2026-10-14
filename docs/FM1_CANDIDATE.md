# JamBox FM-1 candidate packaging and device identification

This is the software preparation for the first hardware trial. The local candidate has firmware update support and temporary musical sessions. Its manifest has `release_enabled: false`, `hardware_validated: false` and no approved physical transitions. The public device page only reads identification; it has no installation action or downloadable firmware.

## Read the owner's current device

Use the [device check](https://anthonysprints.github.io/jamboxfm/device/) in a desktop browser that supports Web MIDI with system exclusive messages, such as Chrome or Edge. Publication and software test results are recorded in [candidate evidence](build-evidence/candidate/README.md).

1. Turn on the FM-1 and connect it with a USB data cable. Close other applications using its MIDI connection.
2. Select **Enable MIDI access** and allow the browser's MIDI/system-exclusive permission.
3. Select the matching FM-1 input and output, then select **Read device**. If several devices have identical names, disconnect the others first.
4. Save the identification report. It contains firmware fields, not songs, samples, MIDI port IDs or a raw flash backup.

A known numeric ID alone does not establish firmware family. Unknown or inconsistent replies remain unrecognized. The report always says installation is unavailable at this stage. Use the current firmware's own backup tools before a future change; the read-only report is not a music backup.

A report of `FM-1_015` matches the stock V15 identity. The current checker leaves it `unknown` because its audited profiles cover JamBox and exact Sloop 2.6. For unrecognized firmware, false capability fields mean **not established by this checker**, not proof those features are absent. The owner’s report and confirmation of the original interface establish factory V15 as the working baseline. The [device plan](FM1_DEVICE_PLAN.md#software-follow-up-for-the-confirmed-factory-baseline) now requires a separately verified factory entry/return path; no firmware change is needed to complete identification.

## Identity contract

| Mode | Transport identity | USB product name | Family-probe mode / capability mask |
| --- | --- | --- | --- |
| JamBox application | `FM-1_700` | `JamBoxFM` | `0` / `0x17` |
| JamBox application rescue | `FM-1_000` | `JamBoxFM Rescue` | `1` / `0x05` |
| JamBox package loader | `ota-FM-1_700` | `JamBoxFM Update` | `2` / `0x09` |

The package marker is `FM-1_700`; the release string is `0.1.0-dev`. These constants live in [jam_identity.h](../firmware/src/jam_identity.h). `700` was not observed in the inspected research set; it is not a globally registered allocation. The inherited `FELUCCA-LOADER-1` binary marker and SPL resume-record identity are compatibility details, not the displayed firmware family, and remain intact.

The empty OTA-envelope command `0x12` requests a read-only 48-byte record: ASCII family `JAMBOXFM` (8 bytes), schema `1`, mode (1 byte), capabilities (16-bit little-endian), release (20 ASCII bytes, NUL-padded) and transport identity (16 ASCII bytes, NUL-padded). Capability bits are updater `1`, temporary session `2`, application rescue `4`, bounded loader `8`, and read-only calibration import `16`. Framing, canonical 7-bit encoding, exact lengths, checksum, padding, family, release, mode and capabilities must agree.

This helper has no instrument or flash-write dependency. Rescue and loader serve it before the instrument starts; normal mode retains the existing USB startup order. This protocol identifies the responding software; it is not cryptographic authentication or independent proof of the physical chip revision.

The inspected published Sloop 2.6 app reports `FM-1_900`, with editor INFO name `FELUCCA SLOOP 2.6`. The full audited INFO layout is checked too. The `--release 2.6` builder's potential `926` identity is not substituted for the actual published image. The exact Sloop package used in offline return simulations is 610,019 bytes, SHA-256 `199829bb5b892feae9058158dfed7a137c83ec4d8dfc1223761be1f5614ad6a2`; it is not bundled with JamBox or republished.

## Local candidate bundle

From the existing source checkout, activate the configured tools and build:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/package_jam_device.py --no-build
```

The target build records its actual preprocessed configuration, compiler dependencies, generated inputs, artifact hashes and compiler/SDK provenance. A failed build invalidates its old receipt. The packager refuses disabled updating, enabled application persistence, stale or modified inputs, mismatched artifacts, bad package structure and out-of-bounds memory. The receipt is a reproducibility record, not a signature from a trusted third party.

The generated `build/jambox-device/` directory contains the candidate `.fwsc`, corresponding source archive, licenses/notices, build receipt, memory report, manifest and checksums. Package verification reconstructs the expected package from the verified app/loader and pinned SDK inputs. The source archive excludes reserved upstream artwork, the external compiler/SDK binaries and generated firmware. Build products remain ignored by Git.

The manifest fixes the FM-1 model, AC791N/WL82 platform, 1 MiB flash, JEDEC `0x856014` and chip key `0x980f`. The device's actual updater/loader retains its hardware and chip-key checks before writes. A package manifest cannot read or certify a disconnected instrument.

## Simulated transitions and remaining hardware work

Tests exercise JamBox normal/rescue/loader to JamBox, audited Sloop 2.6 to JamBox, and JamBox normal/rescue back to the exact pinned Sloop package. They use the actual host updater with simulated MIDI endpoints, explicit pair selection, stage-by-stage family checks, package validation and post-restart verification. Sloop's `ota-FM-1_900` loader is accepted only as the expected stage of the pinned return simulation; an unidentified loader cannot become a supported transition through its number alone.

Malformed identities, wrong firmware, bad checksums, incorrect completion requests, cancellation, stalled/disconnected ports and inconsistent return identity must fail. Tests include a 481-byte final data block and the 512-byte limit. The legacy Python installer rejects an unmodified `FM-1_700` JamBox candidate, including with `--force`; this is an accidental-use guard, not protection against deliberate relabeling. Candidate transfer orchestration exists only in the test helpers. The public page sends identification requests only.

Before a physical trial, identify the owner's installed version, export and verify its available backup, preserve the exact return package, and establish a usable recovery path. Real USB behavior, panel mapping, audio deadlines, stack use and return-to-Sloop data preservation remain untested. The inherited static IRQ-budget failure is still open; passing these simulations does not close it. Follow [milestone 4](FM1_DEVICE_PLAN.md#milestone-4-prepare-and-perform-the-first-physical-trial).
