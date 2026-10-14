# Owner's factory backup validation

Reviewed October 14, 2026. The owner supplied the completed Windows backup's
full flash image, retained RAM, JSON report and log. The files remain private,
outside the repository and website. This record contains validation results
only; no cloud command accessed the physical instrument.

The full flash image is exactly 1,048,576 bytes. Its independently computed
SHA-256 matches the report and both logged device reads. The 512-byte retained
RAM file also matches its report and log. The log records successful execution
of the reviewed RAM loader, chip key `0x980F`, JEDEC `0x856014`, SPI NOR type 3
and a 32,768-byte USB buffer. No warning, traceback or error is present.

Using the reviewed package parser and exact privately held factory V15 file,
every byte of the installed boot head (`0x0000..0x3FFF`) and all 143 application
sectors (`0x4000..0x92FFF`) matches V15. Thus the entire 602,112-byte factory
`flash.bin` region matches, rather than merely reporting the same MIDI name.
The known boot head satisfies the head-equality precondition for the tested
official-return staging code, provided installation preserves it.

The owner also supplied a screenshot of the completed backup followed by a
fresh successful `FM-1_015` identity check after the instructed full power-off
and return procedure. The screenshot shows that identity result; it does not
independently record the physical switch action or establish audio behavior.

The JamBox staging area is blank in this pre-install snapshot. That does not
establish where stock firmware will put a future update record. Successful
RAM-loader reads demonstrate the backup path, not flash restoration or ROM
entry after application damage. Interrupted-install recovery remains unproven.

The prepared tool source and its earlier software checks are recorded in
[Windows backup validation](windows-backup-validation.md). The previous
[mode-only round trip](owner-windows-roundtrip.md) is separate evidence. No
JamBox installation or hardware runtime validation is claimed by this backup.
