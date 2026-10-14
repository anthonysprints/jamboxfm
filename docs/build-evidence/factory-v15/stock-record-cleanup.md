# Stock V15 resume-record cleanup

The JamBox loader now handles the known stock resume-record slot at `0xE8F00`,
in addition to the existing Sloop/JamBox slot at `0xE4F00`. Previously it could
successfully write the new app while leaving a stock handoff record in flash.
The change permits erasing only the two enclosing 4 KiB sectors, and adds
record/staged-loader ownership checks for the additional stock slot.

This is a bounded compatibility fix for the first experimental stock V15
installation. It is not a report of a completed physical JamBox install or a
guarantee of recovery after power loss. No uploaded firmware was executed and
no proprietary firmware or private backup bytes are included here.

## Evidence and its limits

The exact inspected manufacturer package is 699,956 bytes, SHA-256
`db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`.
Its app VM region is `[0x93000,0xE9000)`. Package structure and CRC evidence are
in [BINARY-AUDIT.md](BINARY-AUDIT.md).

The explicit stock slot comes from pinned
[Felucca loader source](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/loader/ldr_core.c#L33)
and its [first-install test](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/tests/ldr_test.c#L151):
they distinguish the stock `E8F00` record from the custom `E4F00` record and
require both to be cleared. These are source/test evidence, not an observed
intermediate flash dump from the owner's device.

Pinned [ChoralRoot's handoff report](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/TRANSPORTER-HANDOFF.md#L163)
reports a physical factory `FM-1_015` to ChoralRoot install. That loader scanned
a wider range of possible records, so its success did not establish that
JamBox's former E4-only cleanup was sufficient. The wider scan is deliberately
not copied: valid-looking record bytes can occur in unrelated user data.

Static analysis of the exact V15 app confirms that the record's loader address
and flash write location are dynamically supplied by the staging allocator.
The following are **file offsets within decoded `app.bin`**, SHA-256
`306e47065f35d7a7a05ada7f5dd092f6e770952054f33f0a86b75fd10ffe3203`:

| File offsets | Observed operation |
|---|---|
| `0x29306` onward | Loader staging allocation/defragmentation. |
| `0x29536`–`0x29578` | Constructs the 32-byte `LOADER.BIN` descriptor: staging address at +4, body length +32 at +8, then header CRC. |
| `0x29598`–`0x295B0` | Returns loader data at allocated start +32, and record address at allocated end −256. |
| `0x29D86` onward | Successful-stage callback allocates 112 bytes and clears the first 80. |
| `0x29DD0` onward | Copies stock migration state to record +40/+44; sets record mode/result/magic; stores the loader area at +72. |
| `0x29E02`–`0x29E38` | Clears +20..39, writes `ota-FM-1_015`, calculates CRC16 over bytes +2..79. |
| `0x29E3E`–`0x29E4E` | Writes/verifies all 112 bytes through the staging callback, then copies all 112 to retained RAM. |
| `0x29E92` | Requests a core reset. |

The +2 mode `0x5A0D` is supplied by the callback descriptor at app offset
`0x4E808`; +4 is `0x5A01`, and +6 is `0x5441`. The retained-RAM and flash
records are part of the actual stock handoff. JamBox does not depend on the
record to find its incoming package once its loader is running in RAM.

PI32 parallel bundles must be interpreted using their input register values.
At `0x29548`/`0x2954C`, the instruction computing `r1 = r9 + 32` is bundled
with a store of the **previous** `r1` to header +4. That previous value is the
staging address set at `0x29536`, preserved by the simple setter at `0x8255C`.
The separate store at `0x2954E` writes the new length to +8. This was checked
against an independent C compilation with the pinned PI32 compiler:
`void f(unsigned *p, unsigned address, unsigned length) { p[0] = address; p[1] = length + 32; }`
produces the same paired load/store pattern. Similarly the bundle at
`0x29DEE`/`0x29DF2` writes the previous `5A01` to record +4 while assigning
`5441` for the next store to +6. Treating the listing as sequential scalar
instructions would misidentify both fields.

The allocator evidence supports a variable staging start and a record at the
allocation end minus 256. It does not independently prove that every stock
device state selects `E8F00`. The new policy handles the explicit upstream stock
slot; it does not erase additional sectors based on a speculative scan.

Two stock details matter for compatibility:

- The migration fields at +40/+44 are meaningful, and bytes +80..111 lie
  outside the CRC-covered structure. Neither is required to be zero.
- Stock copies 16 bytes starting at a `LOADER.BIN` string literal. Bytes after
  its terminating NUL include adjacent literal text. Only the name through
  its NUL is compared, and all 30 descriptor bytes remain covered by its CRC.

## Why a stale flash record matters

V15's encoded 14,384-byte SPL hashes to
`b5a0715940db344f951595e2d5a66050631c7721703cb06d33d8dc94eca3c861`.
Decoding ENC with key FFFF separately for its 16-byte bank header and payload
produces SHA-256
`730e54f0a439f58d147be4364ad21e19566945ada9d3a7bbc8371dce5068d3ef`.
Header and payload CRCs pass, and re-encoding exactly reproduces the input.
All 14,384 decoded bytes match the V13/V14 SPL and all 4,897 byte-bearing rows
of [the pinned FM-1-RE listing](https://github.com/AL-255/FM-1-RE/blob/ec832f281ca165a7d5e0722985f3acb1465046d5/analysis/device/uboot/uboot_objdump.txt).
The entire 16 KiB boot head also matches across these package versions.

That exact listing checks the retained-RAM record, then has a **conditional**
flash fallback at file offsets `0x2E08`–`0x2E72`. It reads 80-byte records at a
boundary minus 256, checks magic/CRC, and uses the selected record's area
pointer (+72) at `0x2E7A` to open the staged loader. It validates/decompresses
the inner loader before calling it. The listing includes a 16-byte bank
header: payload runtime addresses equal `0x01C02000 + file_offset - 0x10`.

Therefore leaving an eligible stock record is a real unhandled boot path.
It does **not** follow that every cold boot scans it or that a flash record
guarantees recovery. ChoralRoot's
[physical cold-boot counterexample](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/BOOT-SAFETY.md#L24)
remains applicable. The owner's successful Windows backup/mode entry verifies
a working-device recovery entry, not entry after an interrupted install.

## Implemented ownership and write policy

Before changing the app, and again before cleanup, the JamBox loader reads
only the additional record at `E8F00`. An absent or invalid-CRC record is
left alone. A CRC-valid AT record must meet all of these conditions or the
session returns `LDR_ERECORD` without a success reset:

- Mode/result `5A0D/5A01`, and exact NUL-terminated `ota-FM-1_015` identity.
- Loader address aligned to 256 bytes, above the app (`>=93000`), with all
  reads and the complete declared stage ending at or before `E8F00`.
- Total stage length at least the 136-byte inspected prefix and at most
  `4F00`; this bounds the accepted small custom-loader handoff.
- Valid outer header CRC, `LOADER.BIN` name, type `41`, and +4 containing
  exactly the staging address. A length value in the address field is refused.
- Valid inner header CRC, `usb_hid_ota.bin` name, load address `01C0A800`,
  and nonzero decoded length no greater than `14000`.
- `FELUCCA-LOADER-1` in the first LZ4 literal window, using the same ownership
  check as the existing first-stage updater.

These checks establish structural ownership for cleanup. They do not decompress
the staged body again or claim a new body-integrity check. The native fixtures
use the real candidate compressed loader body but do not execute stock stage
one, the SPL, or target machine code.

After package verification and the exact host `success\0` reply, cleanup
revalidates E8, erases only `E8000..E8FFF`, and reads back the complete 112-byte
record as erased. It does this before the existing E4 cleanup: the stock-staged
loader header may itself be in `E4000`. Read/erase/readback failure keeps the
current loader available and preserves RAM cleanup state for a host retry.

The actual target flash guard permits only app erases and these two exact
record sectors. The programming hook is further restricted to the app
`[4000,93000)`. All head, project, other staging, bank, sample and global
sectors are preserved. Legacy non-JamBox cleanup behavior is unchanged.

## Native validation

`tests/ldr_test.c` passed with exact V15 as the old image and the then-current
JamBox candidate as the new image in both legacy and JamBox modes. The
AddressSanitizer + UndefinedBehaviorSanitizer runs passed 47 legacy checks and
107 JamBox checks without diagnostics. Checks include:

- Actual candidate loader bodies in synthesized 112-byte stock records,
  nonzero migration/tail data, and stock/nonzero or custom/zero filename padding.
- Both record slots together, including a staged header inside E4000;
  successful app verification, exact protected-byte comparison and idempotence.
- Bad modes/identities, an extended identity, invalid/overflowing pointers
  and lengths, damaged outer/inner descriptors, and missing loader marker;
  every live malformed candidate fails before any erase/program.
- Missing/negative/unterminated completion replies preserve both records.
- Record/header-read failures, erase failure, failed readback and a silently
  ignored erase; retries finish without changing protected bytes.
- Existing unknown-flash, package-corruption, key-mismatch and E4 regression
  tests, including arbitrary record lookalikes outside the two selected slots.

Reproduce with a locally supplied exact stock package and a built candidate:

```sh
cc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -DJAMBOX_PROTO=1 -o /tmp/ldr-stock-test tests/ldr_test.c
/tmp/ldr-stock-test /path/to/FM-1-V15.fwsc build/felucca.fwsc
```

Repeat with `-DJAMBOX_PROTO=0` for legacy coverage. The normal
`tests/jam_storage_run.sh` also exercises both modes using its synthetic old
app fixture. Target compilation and the final installer/package integration
are recorded separately by the build workflow.
