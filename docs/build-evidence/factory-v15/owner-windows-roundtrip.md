# Owner-observed Windows ROM round trip

October 14, 2026, owner-provided PowerShell screenshots. These are observations
from the owner's FM-1, distinct from the earlier cloud simulations. No device
operation was performed remotely by the cloud environment.

## Observed sequence

1. Python 3.12.7 installed the pinned requirements successfully, using Windows
   amd64 wheels. `prepare_fm1_backup.py --check` reported `prepared: true` and
   the expected original/patched source and RAM-loader hashes.
2. MIDI listing showed one FM-1 input and one FM-1 output. An initial WinMM
   input-open error stopped before any query. A subsequent invocation succeeded;
   the exact cause of the temporary failure is unconfirmed.
3. `fm1_stock_rom.py check` reported the exact canonical `FM-1_015` identity.
4. `enter-rom` reported the same identity, received typed `ROM`, and reported
   sending one ROM request after its fresh identity check.
5. `fm1_unbrick.py find` found `WL82 UBOOT1.00 USB Device` and reported
   **Exactly one device: OK**. The volatile Windows disk number is not a fixed
   device identifier and must not be copied into later commands.
6. After the instructed USB-disconnect/full-power-off/return procedure, the
   next screenshot showed a fresh successful `FM-1_015` identity check. The
   console does not independently record the physical switch operation, LCD or
   audio behavior.

This establishes owner-observed Windows MIDI communication, the stock soft-key
route to ROM disk enumeration, and subsequent factory MIDI identity. It does
not establish SCSI inquiry, actual RAM-loader execution, a flash backup, chip
key/JEDEC details, installation or recovery after damaged application code.
The working-stock entry route depends on executable stock firmware. No global
firmware `hardwareValidated` or installation flag is enabled by this record.

## Backup follow-up

The subsequent real backup is now complete and has been independently checked;
see [owner backup validation](owner-backup-validation.md). The instructions
below record the procedure used, rather than a request to repeat it.

Run the prepared backup route from Administrator PowerShell after another
guarded ROM entry. `backup backups` needs no `--package`: both full 1 MiB reads,
their comparison, device guards, durable saved-file/readback checks, report and
log remain. Only optional comparison against a local firmware package is
omitted; the exact factory file is already held privately in the cloud for
later offline comparison. `packages: []` is expected in this variant's report.

`Backup OK` is logged before report writing finishes. Require successful process
exit as well as the session's `full-flash-before.bin` (1,048,576 bytes),
`backup.json` and `unbrick.log`. Failed/partial output remains unverified. Once
the process exits, perform the same full power-off and fresh factory identity
check. Keep the entire session private.

See the [Windows workflow](../../FM1_WINDOWS_BACKUP.md) for commands and the
[software validation record](windows-backup-validation.md) for tool provenance.
No uploaded firmware, device report, raw dump or owner screenshot is committed.
