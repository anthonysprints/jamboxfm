# Windows backup preparation: software validation

Starting JamBox source: `d422ef7`. Owner-confirmed baseline: original factory
FM-1 V15, Windows, Python 3.12.7, no saved personal sounds/settings to keep.
The host's Windows release/architecture and physical recovery behavior remain
unverified. [Owner workflow](../../FM1_WINDOWS_BACKUP.md).

## What was added

- `tools/fm1_stock_rom.py` lists exact MIDI names, checks the canonical stock V15
  identity, or requests ROM entry after explicit typed confirmation and another
  fresh identity exchange. It exposes no firmware/package/write operation and
  sends the ROM key once. The public checker remains read-only.
- `tools/prepare_fm1_windows.py` downloads exact pinned source ZIPs, bounds and
  checks extraction, retains licenses, copies the JamBox helpers, and prepares
  a **new** private folder. Downloaded Python/binary code is not executed by
  this preparation command. No device or pip operation is performed by it.
- `tools/prepare_fm1_backup.py` verifies critical source and loader hashes, then
  applies exact source patches or checks a previously prepared copy. It refuses
  unfamiliar edits, incomplete preparation and import-shadow artifacts. Later
  commands use `python -B` so cached bytecode does not bypass source checking.
- `tools/fm1_windows_requirements.txt` pins Python dependency versions. These are
  not a complete hash-locked build environment. Installation succeeded in an
  isolated Linux Python 3.12 virtualenv; `pip check` passed. PyPI provides a
  Python 3.12 Windows x64 wheel for python-rtmidi 1.5.8. Installation on the
  owner's PC is still a separate step.

## Pinned upstream inputs

| Input | Revision / SHA-256 |
| --- | --- |
| MvaveFM1Unbricker source | `df394beea4c881da4161fbc8a87730050b57968c` |
| Source ZIP, 144170 bytes | `181c34f9668e28874f83b06a0999460a692c68f0d9cbf5c4901c69cdd1a32479` |
| jl-uboot-tool source | `adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a` |
| Source ZIP, 453910 bytes | `21823aac1f0aabd3d9f76aca7c1a841a310bb01d92fa5c9e93e7eab5ec5a7026` |
| wl82loader.bin, 24064 bytes | `d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376` |

The ZIPs and RAM loader stay in the external preparation folder; none are
redistributed in JamBox source/site outputs. Per-file original and patched
digests are in the preparation helper and the [source-check record](windows-source-check.json).

## Concrete bug and changes

The original JieLi command helpers discarded the SCSI residual and returned a
preallocated, full-length buffer. The Windows backend also ignored SCSI status.
A deterministic mock returned only 16 bytes of each requested 512-byte read;
the unmodified tool accepted two matching, zero-padded 1 MiB backups. Only
32 KiB per pass had actually been supplied. This is a host-tool integrity bug,
not evidence of a fault on the owner's device.

The prepared source rejects nonzero/invalid residuals, nonzero SCSI status,
short/oversized data transfers, incomplete control results and mismatched
response commands. The Windows changes declare pointer-width-safe APIs, use
the documented access flags and preserve the OS error code. The Unbricker
device guard additionally requires the expected SPI NOR device type. No flash
write or new recovery algorithm was added to JamBox firmware.

The Windows control-result guard requires the complete SCSI_PASS_THROUGH_DIRECT
structure: that is the supplied output buffer and no sense buffer is allocated.
Tests substitute the Windows API and do not establish actual driver behavior.
The prepared RAM loader is hash-pinned, not fully independently audited machine
code. Host-command review establishes RAM upload and flash reads in `backup`;
it does not establish every possible effect of that binary on real hardware.

## Executed results

| Check | Result / scope |
| --- | --- |
| MIDI helper | 20/20 mock tests: canonical identity, exact/unique ports, stale queues, post-confirmation recheck, cancellation, changed device/topology, timeout/flood bounds, close-on-error, one-shot key |
| Local patcher and transport | 17/17 tests, including exact original-source transforms, refusal/idempotence, Windows API mocks, bad status/short data/control response, mismatched commands and repeatable short-read regression |
| Download/preparation | 13/13 tests: pinned size/hash, unsafe paths, special entries, duplicate names, limits, existing-folder preservation and failed preparation |
| Complete fresh online preparation | Both pinned ZIPs downloaded and verified; extraction, source patches, full check and final private folder passed |
| Prepared upstream suite | 42 discovered; **40 passed, 2 skipped** because optional black-box oracle and its unrelated package were absent. Real V15 fixture and protocol mock tests executed. Restore cases operate only on mocks. |
| Exact uploaded V15 extraction | `--verify-v15` passed; app region 585728 bytes, SHA-256 `6edf3c37fb5bbbc33607c89375ee024d5477c17914d72221c8c68e58a8255686` |
| CLI backup with `--dry-run` | Two identical mock 1 MiB reads; saved dump hash `cfb4ce8af322932852077a68d879a54b83cde79eb377f88bc95fc42e89576494`. Mock contents intentionally differ from stock. |
| Source verification after all integration tests | Passed with `-B`; no source drift/cache artifacts. |

All 50 new JamBox tests passed with the exact optional source fixture enabled.
Without that fixture, one explicit source-comparison test skips; the other 49
still run. [Helper log](windows-helper-tests.log),
[upstream log](windows-upstream-tests.log),
[extraction log](windows-factory-extraction.log),
[mock backup log](windows-backup-mock.log).

An initial full-archive check correctly stopped before producing a destination
because a legitimate upstream `tests` package was missing from the import
allowlist. Its exact empty `__init__.py` was added with a regression test. Final
fresh preparation passed. No bypass or source/hash assertion was relaxed.

## Reproduce software checks

The current instance's isolated Python environment is
`/workspace/deps/jambox-windows-prep-python`. It is optional: install the pinned
requirements into a separate environment when reproducing elsewhere. Use a
new preparation destination and keep factory files/extracted data outside the
repository. These commands do not connect to a device:

```sh
cd /workspace/jamboxfm
FM1_JLUB_SOURCE_REVIEW=/workspace/scratch/jambox-stock-v15/jlub-source-review \
  python3 -B -m unittest discover -s tests -p 'test_*fm1*.py' -v
python3 -B tools/prepare_fm1_windows.py --destination /workspace/scratch/fm1-prep-new
```

Inside the new folder, use the isolated interpreter with `-B`:

```sh
python3 -B prepare_fm1_backup.py --unbricker unbricker --jl-dir unbricker/jl-uboot-tool --check
python3 -B unbricker/fm1_unbrick.py extract /path/to/FM-1.fwsc factory-app.bin --verify-v15
python3 -B unbricker/fm1_unbrick.py backup mock-backups --package /path/to/FM-1.fwsc --dry-run
```

For upstream tests, run unittest discovery in `unbricker` with `FM1_V15_FWSC`
pointing at the exact stock file and `FM1_JLUB_DIR` at the prepared dependency.
`FM1_JLUB_SOURCE_REVIEW` for JamBox tests instead points to **unmodified** pinned
source. Do not supply a patched tree as an original-source fixture.

## Remaining physical work

First observe normal MIDI, then separately test factory → ROM enumeration →
cold power-off → factory identity. Only after that round trip, prepare the
administrator backup session and obtain the actual unit's private dump. A send
success, black screen, software mock, factory update file or source hash is not
that evidence. The [mode-entry audit](windows-mode-entry-audit.md) records the
upstream physical evidence and the warm-restart USB caveat.

No Windows USB/WinMM/SCSI operation, actual RAM-loader execution, owner-device
backup, firmware installation or cold-boot recovery was performed here. The
existing firmware staging-location uncertainty and IRQ-budget failure remain
open. Device firmware and the public installation policy are unchanged; no
Pages deployment is needed for these source/docs changes.
