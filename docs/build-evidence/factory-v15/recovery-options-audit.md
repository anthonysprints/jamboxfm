# Factory V15 backup/recovery audit

Scope: read-only review of existing pinned research checkouts. No connected device,
firmware transfer, download, publication, or changes to project source. Owner has
confirmed the original factory interface; the device report says FM-1_015.

## Immediate next step

The owner does not need to install anything or buy hardware to continue the
software-only work. Keep the exact uploaded `FM-1.fwsc` locally with its recorded
SHA-256. If they have saved personal sounds or changed important factory settings,
record those now (photos/notes and copies of any original imported DX7 files).
Treat this as a record, not an automated backup. Optional clarification before the
physical trial: whether there are personal sounds/settings worth preserving, and
what recovery equipment (if any) they already have. These answers do not block the
current package validation and simulations.

A manufacturer update file is a return image, not a dump of this individual unit.
It does not capture that unit's user data or calibration. Keep the report and
firmware package as separate evidence.

## Stock editor / sound backup

ChoralRoot BUILDING.md explicitly says its Felucca-compatible backup SysEx is not
implemented by stock firmware. Thus Sloop/ChoralRoot Backup buttons are not a
factory V15 backup solution. JamBox's current read-only identity page is not a
flash-dump or sound-export tool either.

FM-1-RE documents stock DX7 voice/bank storage and a UI string `Dx7 32 Voice Save
To ...`; an earlier overview calls this bank export. The more detailed storage
analysis ties it to on-device flash banks. USB TX code also handles DX7 dumps.
These are useful leads, but they do not establish a verified user-facing Windows
editor workflow that exports all V15 sounds/settings. No such workflow or factory
editor manual was found in these pinned repositories. Do not turn this evidence
into a claim that the owner can click a particular Backup/Export button.

Sources:
- https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/BUILDING.md#L184-L194
- https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/09-storage.md#L128-L146
- https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/06-usb.md#L390-L400

## Recovery options and their limits

1. **Normal USB MIDI return to exact V15 while an application or loader is still
   reachable.** FM-1-RE contains a recorded Windows return from a locally modified
   V15-derived image identifying as version 16 to official V15. It verifies
   successful terminal stages and separate post-boot V15 identification. This is
   evidence that normal return is possible, not a JamBox hardware trial, complete
   flash backup, or interrupted-update recovery guarantee.

2. **Windows ROM recovery if the FM-1 enumerates as WL82 UBOOT1.00.** ChoralRoot's
   Windows handoff describes MvaveFM1Unbricker/jl-uboot-tool as the conditional
   software route, including a backup before restoration. The referenced unbricker
   is not among the local pinned repositories, so its current code/Windows
   procedure still needs a separate review before asking this owner to run it.
   It cannot be assumed to reach an FM-1 that does not enumerate. Entering ROM
   mode is a device action beyond the present read-only identity check; do not ask
   the novice owner to do that simply because they uploaded the firmware file.

3. **External USB_KEY / FM-1 Transporter if no application or ROM USB device is
   reachable.** A documented ChoralRoot recovery used a Seeed XIAO RP2040 on the
   FM-1's external USB D+/D-/GND lines, no VBUS, to enter ROM, take two identical
   full 1 MiB reads, restore application sectors, and return to running V15.
   This does not require opening the FM-1 in that documented setup, but does
   require preparing a board and three-wire cable. It is an option to arrange for
   a physical development trial, not an automatic purchase requirement now.
   Transporter is documented/tested on Mac. Its Python port auto-discovery is
   `/dev/cu.usbmodem*`; explicit serial port support exists, but Windows success
   is not established by this pinned evidence. Do not present it as a tested
   Windows one-click recovery.

4. **JamBox/Dinghy app rescue is useful when that early code can still run.**
   Dinghy documents OCT- at boot and crash rescue, but that does not prove
   arbitrary interrupted writes or power loss are recoverable. ChoralRoot's
   actual failed update left an intact staged loader and valid flash record yet
   no usable USB after cold boot. Its later boot-stub design is separate work;
   its claims cannot be inherited merely by using similar app rescue code.

Sources:
- https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/docs/io/12-v15-reflash-proof.md
- https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/UNBRICK-HANDOFF.md
- https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/TRANSPORTER-HANDOFF.md
- https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/BOOT-SAFETY.md#L22-L46
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/README.md
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/tools/fm1t.py#L1-L64
- https://github.com/zvenson/dinghy/blob/d44df7db09b98ccad2f1233b8f83f5f09c04829e/README.md#L22-L39

## Suggested project next step

Finish exact-package validation and mock V15 -> JamBox -> V15 transitions now.
Before a physical firmware test, prepare a concrete owner-specific Windows return
and recovery procedure, with the backup limits stated accurately. A byte-correct
simulation cannot prove boot timing, audio, USB enumeration on this unit, or
power-loss recovery. Do not promise that keeping V15.fwsc alone guarantees an
unbrick path.
