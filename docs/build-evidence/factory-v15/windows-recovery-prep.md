# Windows recovery preparation: source audit

This is the earlier source-only audit. The owner has since confirmed Windows,
Python 3.12.7 and no valued saved sounds/settings. Follow the newer
[Windows workflow](../../FM1_WINDOWS_BACKUP.md) and
[validation record](windows-backup-validation.md). Later review found that the
upstream transport could ignore short reads; the prepared local copy now
requires transfer/status checks before a backup can be treated as verified.
The original tool alone should not be recommended for this workflow.

Reviewed public source at
`Quixotic7/MvaveFM1Unbricker@df394beea4c881da4161fbc8a87730050b57968c`.
Sparse Git checkout:
`/workspace/scratch/jambox-stock-v15/unbricker-review`.
Only Python/text documentation was checked out. No setup, device command, loader
execution, tests, binary download, firmware transfer, or package installation was
run as part of this review. GitHub access worked.

Exact project from the pinned ChoralRoot Windows handoff:
https://github.com/Quixotic7/MvaveFM1Unbricker

## What this adds to the earlier audit

There is a software-only Windows recovery route with an ordinary USB data cable
when the FM-1 already appears as `WL82 UBOOT1.00` (VID 4C4A, PID 8057). It requires
no extra electronics in that state. It is distinct from normal MIDI updating.

This does **not** establish that the owner's currently working stock V15 can be
put into that mode by the reviewed Windows tool. `fm1_unbrick.py` has no MIDI
soft-key or enter-UBOOT command: it only discovers disks already in ROM mode.
Running `find` on normal stock should ordinarily find no UBOOT device; that is
not a failure of the synth. The README's OCT combinations and repeated restarts
are Felucca-family troubleshooting guidance, not a verified factory V15
instruction. Do not tell the owner to use those blindly.

A separate pinned Transporter source has a V15 USB-MIDI soft-key implementation
that enters UBOOT without a flash write (`src/pio_host.c`, lines 99 onward).
That is promising evidence for a no-extra-electronics **future controlled mode
entry test**, but the cited public Unbricker does not expose it on Windows. A
Windows sender, its packet handling, mode exit, and the resulting enumeration
need separate review before making a copy/paste owner procedure. A successful
mode-entry/backup test while V15 runs still would not guarantee access after a
future interrupted JamBox write, because the soft key relies on working stock
code.

## Minimum prerequisites

For offline preparation / extraction / simulation:
- Windows 10 or 11; Python 3.9+ (`py --version` checks an existing installation).
- Pinned project source and the owner's exact, verified V15 package.
- No FM-1 connection is needed for extraction or any `--dry-run`.

For real ROM discovery / backup later:
- USB data cable directly to the PC, FM-1 already enumerating as UBOOT.
- Administrator PowerShell for SCSI access during info/backup/restore.
- One setup download: pinned jl-uboot-tool
  `adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a`; setup verifies
  `wl82loader.bin` SHA-256
  `d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376`.
- Python dependencies installed by setup: crcmod, pyyaml, pycryptodomex, tqdm.
  Git is optional (the script falls back to GitHub ZIP).
- No custom Windows driver installation is specified by this project.

## Command boundary, verified in source

| Command | Effect |
| --- | --- |
| `find` | Queries Windows `Win32_DiskDrive`; opens no device handle, sends no device commands. No jl-uboot-tool setup needed. |
| `extract FM-1.fwsc app_v15.bin --verify-v15` | Local file parsing and extraction only. No device. |
| `setup` | Downloads pinned source/loader and runs pip on the PC. Does not connect to the FM-1. |
| Any command with `--dry-run` | Uses an in-memory mock for device operations. Does not prove real USB/ROM access. |
| `info` | Opens ROM device, reads optional retained RAM, uploads and executes a RAM loader, checks chip/flash identity. No flash erase/program call. |
| `backup backups --package FM-1.fwsc` | Same RAM-loader activity, then reads the entire 1 MiB flash twice, compares, saves a verified local dump/report. No flash erase/program call. |
| `restore FM-1.fwsc --verify-v15` | Can erase/program the application area. Takes backup first and asks for typed WRITE unless --yes. Not part of current prep. |

`backup` is therefore **non-writing to persistent flash**, but it is not a
zero-state-change identity probe: it runs temporary code in RAM. Be explicit
about that distinction. Keeping only the .fwsc does not save unit-specific data;
keeping `full-flash-before.bin` does. That full dump can include private songs and
sounds, and should remain with the owner rather than in a public repository.

## Concrete staged next-step plan

1. Finish the current cloud package checks and mock factory V15 -> JamBox -> V15
   transitions. No device operation is needed.
2. On the owner's PC, first establish Windows version and whether Python is
   available (`py --version`), keep the verified V15 file, and record whether
   personally saved sounds/settings exist. This is enough to prepare the next
   instructions without purchases.
3. Prepare the pinned Unbricker folder and local extraction/mock commands. An
   optional normal Windows disk inventory can be read now, but no UBOOT device
   is expected from ordinary stock mode and it establishes no recovery promise.
4. Separately review a controlled, non-flash stock V15 -> ROM -> stock test on
   Windows. Explicitly label the mode change. If ROM enumeration succeeds, run
   only the reviewed `backup` route, retain the two-read verified full dump and
   recorded chip key/flash ID, then return to stock without restore/erase/write.
   If entry is unavailable, stop that branch and discuss the available recovery
   routes; do not demand hardware as a prerequisite to continued software work.
5. Plan the first real JamBox write only after the return procedure, measured
   build concerns, and applicable recovery limits are concrete. No `restore`,
   `--yes`, `--i-know`, `--force-write`, or manual erase belongs in current prep.

## Updated Transporter note

The original pinned Transporter `fm1t.py` was Mac-oriented. The **newly reviewed
Unbricker wrapper** `fm1_transporter_recover.py` explicitly supports Windows and
uses pyserial port discovery plus Windows removable-volume discovery. Thus an
updated Windows-capable wrapper exists; do not keep claiming there is no Windows
Transporter tooling. The documented real recovery cited here still occurred on
Mac, so code support is not hardware validation on this owner's Windows PC.
The hardware route remains optional until an owner-specific physical test plan
requires deciding how to handle an FM-1 that no longer enumerates.

## Pinned sources

- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/README.md
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L727-L783 (setup)
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L816-L846 (RAM loader)
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L1072-L1115 (double-read backup)
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L1177-L1220 (find/info/backup)
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L1237-L1295 (restore)
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_transporter_recover.py#L443-L470
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/src/pio_host.c#L99-L119 (stock soft key)
