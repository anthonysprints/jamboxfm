# Factory V15 preparation

The owner's identity report returned `FM-1_015`, and they confirmed the original factory screen. The uploaded `FM-1.fwsc` exactly matches the independently recorded V15 pin: **699,956 bytes**, SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`. It is retained outside the public repository at `/workspace/deps/jambox-return/FM-1-V15.fwsc`. Manufacturer firmware is not redistributed.

This establishes the expected factory return **file**. It does not read the installed firmware byte for byte, identify the physical flash chip, or back up this unit's saved sounds/settings. The intended first hardware trial remains factory V15 → JamBox → factory V15, with installation disabled until its remaining gates are met.

## Software checks

The new offline factory-file validator accepts only the exact pinned file. The six existing JamBox/Sloop manifest transitions remain unchanged. Three additional factory transitions exist exclusively in the mock MIDI test helper; an explicit verified factory fixture supplies the test's factory-baseline assumption. The public device classifier still cannot authenticate factory identity or certify capabilities from the version number.

Host simulations now compare every byte of the logical package in each transfer stage, including 512-byte and 481-byte responses. They test port selection, re-enumeration, stage/final identities and faults, but use generic contiguous requests rather than the proprietary updater's actual request pattern. They do not execute the stock loader or prove it accepts JamBox.

Separate native tests execute the real JamBox OTA staging code against the uploaded factory file and simulated flash. Before staging the official loader, it compares **all 16 KiB of the installed boot head** with the return package. A mismatch is refused. A stock-origin JamBox fixture must preserve that head: the synthetic boot head in our generated JamBox package is different, and our loader intentionally never writes it. Native loader tests separately check the actual JamBox app-only write path from the stock-image fixture.

Results and exact commands are in [factory V15 evidence](build-evidence/factory-v15/README.md). All transfer orchestration remains test-only; no firmware or installer is published.

## What the read-only page means

The page now explains `FM-1_015` as **Factory V15 identity reported**. Its saved report still has `family: unknown`, `recognized: false` and installation disabled. False capability fields mean *not established by this checker*, not missing features or a broken device. The page sends only the identity query to factory V15; it does not try Sloop's editor or JamBox's family probe.

## Remaining before a physical trial

1. **Stock entry behavior:** the file layout and key match JamBox, but stock V15's actual loader acceptance, staging location and reset handover have not been demonstrated for JamBox. The native fixture supplies an E4F00 resume record as an explicit assumption. JamBox's bounded cleanup must not be broadened to erase arbitrary record-shaped data in foreign storage.
2. **Boot-head and device backup:** confirm the actual unit's head/chip details through an appropriate private backup route. The factory update file is not that backup. Stock does not implement Sloop/ChoralRoot backup commands; no complete factory-editor export workflow has been verified. Photos/notes and original imported DX7 files can preserve useful information, but they are not a full flash backup.
3. **Recovery:** a valid staged loader and flash resume record do not guarantee cold-boot recovery. Pinned ChoralRoot hardware evidence documents a failed interrupted update with both intact. JamBox's app rescue also requires application code to remain executable. The inherited unconditional power-cycle-resume comments have been corrected; no boot stub or automatic unbrick guarantee was added.
4. **Physical validation:** USB handover, controls, audio deadlines, flash behavior and the existing static IRQ-budget failure still require separate work. Passing host tests does not pass these gates.

## Windows preparation, if that is the owner's current host

The reviewed [MvaveFM1Unbricker source](https://github.com/Quixotic7/MvaveFM1Unbricker/tree/df394beea4c881da4161fbc8a87730050b57968c) has a Windows route using an ordinary USB cable **when the FM-1 is already reachable as `WL82 UBOOT1.00`**. It does not itself switch running factory V15 into that mode. Do not give the owner custom-firmware key combinations as unverified factory instructions.

The next preparation is to confirm the computer/Windows version and whether personal sounds/settings need preserving. Then prepare and review a controlled factory-to-ROM-and-back procedure. A ROM backup runs temporary loader code in RAM and reads flash twice; it does not program persistent flash, but is a different device operation from the current identity query. No ROM command has been run on the owner's device here.

No additional electronics are needed to continue software preparation. External Transporter hardware is an alternative for a unit that cannot otherwise enter ROM; that choice belongs to a concrete recovery plan. Current source includes Windows-capable tooling, while the pinned physical recovery evidence used Mac. [Detailed Windows audit](build-evidence/factory-v15/windows-recovery-prep.md) distinguishes source review from hardware proof.
