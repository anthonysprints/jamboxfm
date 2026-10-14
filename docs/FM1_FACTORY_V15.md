# Factory V15 preparation

The owner's identity report returned `FM-1_015`, and they confirmed the original factory screen. The uploaded `FM-1.fwsc` exactly matches the independently recorded V15 pin: **699,956 bytes**, SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`. It is retained outside the public repository at `/workspace/deps/jambox-return/FM-1-V15.fwsc`. Manufacturer firmware is not redistributed.

The later [owner backup validation](build-evidence/factory-v15/owner-backup-validation.md) now establishes this unit's actual factory boot/application bytes and expected chip identity as well. The [experimental Chrome workflow](FM1_CHROME_TRIAL.md) prepares factory V15 → JamBox; a physical return-to-factory trial is separate work. Hardware validation remains false until actual device results are collected.

## Software checks

The new offline factory-file validator accepts only the exact pinned file. The six existing JamBox/Sloop manifest transitions remain unchanged. Three additional factory transitions exist exclusively in the mock MIDI test helper; an explicit verified factory fixture supplies the test's factory-baseline assumption. The public device classifier still cannot authenticate factory identity or certify capabilities from the version number.

Host simulations now compare every byte of the logical package in each transfer stage, including 512-byte and 481-byte responses. They test port selection, re-enumeration, stage/final identities and faults, but use generic contiguous requests rather than the proprietary updater's actual request pattern. They do not execute the stock loader or prove it accepts JamBox.

Separate native tests execute the real JamBox OTA staging code against the uploaded factory file and simulated flash. Before staging the official loader, it compares **all 16 KiB of the installed boot head** with the return package. A mismatch is refused. A stock-origin JamBox fixture must preserve that head: the synthetic boot head in our generated JamBox package is different, and our loader intentionally never writes it. Native loader tests separately check the actual JamBox app-only write path from the stock-image fixture.

Results and exact commands for that software-only milestone are in [factory V15 evidence](build-evidence/factory-v15/README.md). The newer first-trial controller is separate production code and does not alter the original simulated transition manifest.

## What the read-only page means

The page now explains `FM-1_015` as **Factory V15 identity reported**. Its saved report still has `family: unknown`, `recognized: false` and installation disabled. False capability fields mean *not established by this checker*, not missing features or a broken device. The page sends only the identity query to factory V15; it does not try Sloop's editor or JamBox's family probe.

## Remaining before a physical trial

1. **Stock entry behavior:** the [stock record audit](build-evidence/factory-v15/stock-record-cleanup.md) found that V15 uses `E8F00`, whereas custom firmware uses `E4F00`. JamBox now validates the stock record and staged-loader metadata before app writes, then clears only these two explicit record sectors after a verified transfer. Arbitrary record-shaped data elsewhere stays protected. Native tests cover this distinction; actual stock loader acceptance and USB handover remain outcomes of the first device trial.
2. **Boot-head and device backup — completed:** the uploaded full flash image matches both logged reads; all boot-head and app bytes equal the exact V15 file. The expected chip key, flash ID and Windows RAM-loader read path are demonstrated. The factory update file alone was not that backup. Stock does not implement Sloop/ChoralRoot backup commands; no complete factory-editor export workflow has been verified. Photos/notes and original imported DX7 files can preserve useful information, but they are not a full flash backup.
3. **Recovery:** a valid staged loader and flash resume record do not guarantee cold-boot recovery. Pinned ChoralRoot hardware evidence documents a failed interrupted update with both intact. JamBox's app rescue also requires application code to remain executable. The inherited unconditional power-cycle-resume comments have been corrected; no boot stub or automatic unbrick guarantee was added.
4. **Physical validation:** USB handover, controls, audio deadlines, flash behavior and the existing static IRQ-budget failure still require separate work. Passing host tests does not pass these gates.

## Windows preparation

The reviewed [MvaveFM1Unbricker source](https://github.com/Quixotic7/MvaveFM1Unbricker/tree/df394beea4c881da4161fbc8a87730050b57968c) has a Windows route using an ordinary USB cable **when the FM-1 is already reachable as `WL82 UBOOT1.00`**. It does not itself switch running factory V15 into that mode. Do not give the owner custom-firmware key combinations as unverified factory instructions.

The owner confirmed **Windows, Python 3.12.7, and no saved sounds/settings they need to keep**. No separate personal sound export is needed. A unit-specific backup still records the actual boot head, flash identity and device data. Windows amd64 dependency installation and the prepared source check passed on the owner's PC; the exact Windows release and host CPU architecture have not been independently established.

The [Windows workflow](FM1_WINDOWS_BACKUP.md) separates computer preparation, a factory-to-ROM-and-back test, and a later double-read backup. The owner's screenshots now show the [successful MIDI/ROM/factory identity sequence](build-evidence/factory-v15/owner-windows-roundtrip.md). A ROM backup runs temporary loader code in RAM; its host path does not request persistent flash programming, but it is a different device operation from the completed identity/mode-entry steps. The later real backup and RAM-loader session are now complete and independently checked in the linked owner backup evidence. No JamBox install or recovery write has yet been demonstrated.

No additional electronics are needed to continue software preparation. External Transporter hardware is an alternative for a unit that cannot otherwise enter ROM; that choice belongs to a concrete recovery plan. Current source includes Windows-capable tooling, while the pinned physical recovery evidence used Mac. [Detailed Windows audit](build-evidence/factory-v15/windows-recovery-prep.md) distinguishes source review from hardware proof.
