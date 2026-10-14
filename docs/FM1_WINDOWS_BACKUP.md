# Windows preparation for a factory FM-1

The owner has Windows, Python **3.12.7**, factory **FM-1_015**, and no personal
sounds/settings to preserve. The next device work is to establish recovery-mode
access and obtain a private full backup. **This does not install JamBox.**

The software checks described below run in the cloud with simulated devices.
Windows MIDI delivery, Windows disk access, and the owner's physical round trip
remain untested. A working stock soft key also does not establish recovery after
an interrupted firmware write: that entry route depends on working app code.

## 1. Prepare the computer with the FM-1 disconnected

Download the [JamBox source ZIP](https://github.com/anthonysprints/jamboxfm/archive/refs/heads/main.zip)
and extract it. Open PowerShell inside its `jamboxfm-main` folder (the folder
containing `tools`). Run:

```powershell
py -3.12 tools\prepare_fm1_windows.py --destination "$env:USERPROFILE\JamBoxFM-backup"
```

This downloads two exact, hash-checked upstream source archives into a **new**
private folder, checks their files, and applies our reviewed backup checks.
It does not connect to the FM-1, install firmware or change Windows settings.
If that destination already exists, choose a new folder name; the helper does
not overwrite it. Keep the folder outside the public source checkout and cloud
sync folders intended for sharing.

Then run these commands, stopping if any reports an error:

```powershell
& {
    $ErrorActionPreference = "Stop"
    Set-Location "$env:USERPROFILE\JamBoxFM-backup" -ErrorAction Stop
    py -3.12 -m venv .venv
    if ($LASTEXITCODE -ne 0) { throw "Python environment creation failed. Stop here." }
    .\.venv\Scripts\python.exe -B -m pip install -r requirements.txt
    if ($LASTEXITCODE -ne 0) { throw "Dependency installation failed. Stop here." }
    .\.venv\Scripts\python.exe -B prepare_fm1_backup.py --unbricker unbricker --jl-dir unbricker\jl-uboot-tool --check
    if ($LASTEXITCODE -ne 0) { throw "Backup source verification failed. Stop here." }
}
```

No PowerShell execution-policy change or virtualenv activation is needed. The
dependencies are version-pinned. They are not a fully hash-locked toolchain.
Python-rtmidi 1.5.8 publishes a Python 3.12 Windows x64 wheel. Other Windows
architectures have not been validated; if installation fails, keep the error
and stop rather than installing random drivers or compiler bundles.

The source/patch preparation is repeatable into a different new destination.
The `--check` command can be repeated in the same folder and changes nothing.

## 2. List the normal MIDI ports

Close browser tabs using MIDI and other music applications. Connect only the
FM-1 with a USB data cable directly to the PC and start its original factory
interface. Avoid USB hubs for this first trial. From the prepared folder:

```powershell
.\.venv\Scripts\python.exe -B fm1_stock_rom.py list
```

This lists MIDI ports and sends no messages. Save the input and output names.
They can differ. The helper never picks one automatically or falls back to a
different device. Use the **full names exactly as listed** in later commands;
the example names below are placeholders, not names to copy literally.

```powershell
.\.venv\Scripts\python.exe -B fm1_stock_rom.py check --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

Continue only if the check reports the exact factory `FM-1_015` identity. A
name or identity string is a routing check, not proof of the installed bytes.
Missing/duplicate ports, a timeout, an invalid response or another identity
must be resolved before any mode change.

## 3. First physical trial: change mode, observe, return

This step intentionally switches the running factory app to the chip's ROM
recovery mode. It does not send firmware, erase flash or run a backup loader.
The screen can go dark and the normal MIDI port should disappear.

```powershell
.\.venv\Scripts\python.exe -B fm1_stock_rom.py enter-rom --input "EXACT INPUT NAME" --output "EXACT OUTPUT NAME"
```

The helper checks identity, explains the mode change, requires typing `ROM`,
then checks identity again before sending the single ROM-entry message. A
successful send is not proof of entry. Observe the resulting Windows device:

```powershell
.\.venv\Scripts\python.exe -B unbricker\fm1_unbrick.py find
```

Expected: exactly one **WL82 UBOOT1.00 USB Device**. A raw USB descriptor can
instead name it `WL80UBOOT1.00`; the expected USB IDs are `VID_4C4A PID_8057`.
The `find` command only inventories Windows disks; it opens no disk handle.
If Windows offers to format or initialize this device, **cancel** that dialog.
It is a recovery interface, not a normal storage drive.

For this first trial, record the result and return to factory firmware:

1. Wait for the command to finish; close its session.
2. Disconnect USB, switch the FM-1 off, and wait about five seconds.
3. Switch it on and reconnect USB. Verify the original screen and keys work.
4. Run `list` again, then `check` using the current port names. Verify `FM-1_015`.

Upstream observed factory V15 running without USB after a warm restart, which
is why this uses a full power-off. If ROM was not seen, the round trip is
unproven. If factory mode does not return, report the screen and Windows device
state; do not try a restore or erase to investigate a mode-only test.

## 4. Backup after a successful mode-only round trip

Keep the owner's original `FM-1.fwsc` in the private preparation folder. Before
using it, verify it in PowerShell:

```powershell
(Get-Item .\FM-1.fwsc).Length
(Get-FileHash .\FM-1.fwsc -Algorithm SHA256).Hash
```

Required: **699956 bytes** and
`DB1642B2B6FA5C2CCCB11FFD13878068BB28601678D3644049F99DC40E7EDB8A`.
That file is the factory update, not the device backup.

Repeat the guarded ROM entry above. Open **PowerShell as Administrator**, then
run from the same private folder (adjust the folder if a different one was
chosen):

```powershell
& {
    $ErrorActionPreference = "Stop"
    Set-Location "$env:USERPROFILE\JamBoxFM-backup" -ErrorAction Stop
    if ((Get-Item .\FM-1.fwsc).Length -ne 699956 -or
        (Get-FileHash .\FM-1.fwsc -Algorithm SHA256).Hash -ne "DB1642B2B6FA5C2CCCB11FFD13878068BB28601678D3644049F99DC40E7EDB8A") {
        throw "Factory file does not match the verified V15 package. Stop here."
    }
    .\.venv\Scripts\python.exe -B prepare_fm1_backup.py --unbricker unbricker --jl-dir unbricker\jl-uboot-tool --check
    if ($LASTEXITCODE -ne 0) { throw "Backup source verification failed. Stop here." }
    .\.venv\Scripts\python.exe -B unbricker\fm1_unbrick.py find
    if ($LASTEXITCODE -ne 0) { throw "ROM discovery failed. Stop here." }
    .\.venv\Scripts\python.exe -B unbricker\fm1_unbrick.py backup backups --package FM-1.fwsc
    if ($LASTEXITCODE -ne 0) { throw "Backup failed. Preserve the log; do not use a partial dump." }
}
```

**Stop if either check fails.** Do not supply a guessed `PhysicalDrive` number
or an override flag. The backup tool requires the expected ROM device and chip
details. It uploads a pinned temporary program into **RAM**, then reads the
1 MiB flash twice, compares both reads and checks the saved file. The reviewed
host backup path sends no persistent flash erase/program command. The binary
RAM loader is pinned and used upstream; its entire machine code has not been
independently proven here.

A successful session contains `full-flash-before.bin` (1,048,576 bytes),
`backup.json` and a log. Retained RAM may also be recorded when readable. Keep
the entire session folder private and copy it to another safe location. Failed
or partial output is not a verified backup. The two-read match establishes
repeatability, not immunity to every device or driver fault.

After the backup process has finished and released the device, perform the
same full power-off/return and fresh identity check as in step 3. If a backup
command is interrupted, it may be incomplete; it has not requested a firmware
write. Preserve its log and return through the full power-off procedure.

The cloud review will then compare the actual boot head with the exact factory
file and inspect the reported chip/flash information before planning firmware
installation. Do not put dumps or reports in a public GitHub issue or commit.

## Why the local backup patches are required

The pinned upstream transport could ignore a short SCSI transfer and return a
full-sized, zero-padded buffer. Repeating the same short read could therefore
pass a double-read comparison. The preparation helper checks source hashes and
applies bounded fixes for transfer lengths, SCSI status, command-response
matching, Windows API argument widths, and the expected SPI NOR device type.
It refuses unfamiliar source changes instead of guessing where to patch.

No browser emulator, public identity checker, firmware image, or installer
policy changes as part of this workflow. The website checker remains read-only.

See [software evidence](build-evidence/factory-v15/windows-backup-validation.md)
and [mode-entry research](build-evidence/factory-v15/windows-mode-entry-audit.md)
for exact pins, tests, and the distinction between source review, mocks and
physical validation.
