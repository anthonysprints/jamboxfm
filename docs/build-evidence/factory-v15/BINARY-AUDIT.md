# Exact manufacturer V15 package: offline audit

Input: `/workspace/deps/jambox-return/FM-1-V15.fwsc`, 699,956 bytes, SHA-256
`db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`.
The package contains product `FM-1_015`; marker stripping produces 699,936 bytes.
This matches the prior Felucca/Sloop exact V15 pin and the pinned FM-1-RE
capture. No uploaded executable was run, no MIDI device was opened, and no
manufacturer firmware bytes were written into the repository or redistributed.

## Package contents and bounds

Offsets in this table are logical package offsets, **not flash addresses**.

| UFW entry | Type | Offset | Stored length |
|---|---:|---:|---:|
| flash.bin | 0 | 0x400 | 602,112 / 0x93000 |
| info.log | 2 | 0x93400 | 0 |
| USR | 50 | 0x93400 | 73,226 |
| isd_config.ini | 52 | 0xA5220 | 3,296 |
| ota.bin | 100 | 0xA5F00 | 19,969 / 0x4E01 |
| script.ver | 251 | 0xAAD20 | 27 |
| blimit.bin | 161 | 0xAAD40 | 144 |
| tail.bin | 255 | 0xAADE0 | 64 |

The inspected V15 `flash.bin` uses app area 0x4000, XIP entry 0x02000120,
app.bin offset 0x120 within that area and length 581,564 / 0x8DFBC. Its VM
region begins at 0x93000 and runs to 0xE9000; BTIF is 0xE9000..0xEA000;
USR is 0xEA000..0xFC000; key_mac is 0xFF000..0x100000. Chip-key blob decodes
to 0x980F. These V15 values match JamBox's inherited app/storage boundaries;
the V13-specific 0x94000 VM boundary in earlier research must not be applied
to this V15 file.

Verified UFW header/list CRCs, raw flash.bin and ota.bin CRCs, relevant nested
JLFS header CRCs, chip-key blob CRC, decrypted app-area/app/cfg_tool CRCs,
nested cfg/eq CRCs and the OTA inner/outer/decompressed-image CRCs. The
stored-byte CRCs for auxiliary UFW entries and encoded SPL payload are not
asserted as plaintext CRCs: this audit did not decode those auxiliary
representations. The exact whole-file SHA-256 pins their actual bytes.

`binary-audit.json` contains the machine-readable metadata and every hash
computed. `binary_audit.py` reproduces it with JamBox's pure packager/decoder
helpers; it emits metadata only.

## Loader size and identity

Stock `ota.bin`:

- Outer payload length 19,937 / 0x4DE1; outer HCRC 0xEBAA and DCRC 0x5881.
- Inner image length 23,324 / 0x5B1C, load address 0x01C0A800.
- Six LZ4 blocks, continuous dictionary: decoded 4096 × 5 + 2844 bytes.
- Whole ota.bin SHA-256 `ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541`.
- Decompressed image SHA-256 `42798ffc24f385302f07865216726fbdd0bfeff1339bdd909edcc93f3b1bb53c`.
- Entire ota.bin is byte-identical to pinned FM-1-RE
  `analysis/device/ota-loader/ota_stock.bin` at commit
  `ec832f281ca165a7d5e0722985f3acb1465046d5`. Its existing static loader
  analysis therefore addresses this loader build, not merely a similar one.

The initial JamBox candidate inspected in this audit (before the completion-acknowledgement fix; SHA-256
`ab0c2071c2a3e18ce6c8ee7a55621af8f20aa1fe978c1784481352542fc2a0b2`)
has a 7,309-byte ota.bin, outer payload 7,277 bytes and decoded loader 8,600
bytes. Its loader is well below FM-1-RE's observed 19,456-byte forced-loader
replacement limit. That size comparison does **not** prove that stock V15
will accept its custom loader or stage it where JamBox expects.

## Installed boot head matters on the return path

Stock V15 flash head [0,0x4000) SHA-256:
`d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67`.

Synthetic JamBox/Sloop package head SHA-256:
`53144a0a192b505e30cbdba2477c9771a05564cd7b6441ca12e85f16ca88b077`.

They differ. This is expected: JamBox's loader preserves the installed head,
not the synthetic head that our packager includes for format compatibility.
An actual stock-origin installation should retain its installed stock head
if that loader path behaves as designed. The identity report alone does
not establish that this particular unit's 16 KiB head equals the canonical
package head.

JamBox `ota.c:373` compares all 16 KiB against the official target before
staging that loader and refuses mismatch with -5. Keep that check. A model
that seeds the entire fake flash from JamBox's package and then expects an
official return to succeed is not modeling a stock-origin installed device.
Use the preserved stock head plus JamBox app as the return fixture. The
separate native first-stage tests cover that case and mismatch refusal.

## Native JamBox loader-core test actually performed

Compiled the existing `tests/ldr_test.c` with `-DJAMBOX_PROTO=1` and ran it
with stock V15 as old image and the current JamBox package as new image.
It passed all inherited checks: initial completion had 1,180 requests and
144 sector erases (143 app sectors plus the test's E4000 record sector),
stock head preserved, correct target app bytes, app/record write bounds,
unknown-JEDEC refusal, corrupted-transfer handling and cleanup-fault retry.
Log: `native-loader-stock-to-jam.log`.

This test executes the real portable **JamBox** loader C implementation
against a simulated NOR. Its fixture explicitly supplies JEDEC 0x856014 and
an update record at 0xE4F00. It does **not** execute stock stage one, validate
stock's loader acceptance policy, discover its actual staging allocation,
exercise the SPL, or boot the resulting instrument. The test's E4F00 record
must never be presented as an observed stock staging address.

## Prior stock capture replay actually performed

Ran the inspected pinned FM-1-RE offline verifier with this exact uploaded
file:

```
PYTHONDONTWRITEBYTECODE=1 FM1_OTA_LOG=/dev/null \
  python3 tools/verify_reflash_record.py --firmware /workspace/deps/jambox-return/FM-1-V15.fwsc
```

It passed the supplied modified-stock-V16-to-stock-V15 record with actual
package payload bytes. That record contains 48 staging data requests plus
E0000000, then 1,167 flash data requests plus F0000000. It specifically
covers a final staging payload of 481 bytes at logical 0xAAB20, and records
an independently queried post-boot V15 identity. Log:
`upstream-capture-verification.log`.

The stock stage-two bulk is 1,144 requests of 512 bytes (143 app sectors),
descending from logical 0x93200 to 0x4400, plus directory/metadata reads.
The trace is not a full-flash dump and cannot by itself prove the exact
stock-loader erase/program side effects. Auxiliary USR/config payloads
present in the package are not requested in this recorded path. Do not
claim factory return restores all user data from the package or that the
stock loader obeys JamBox's narrower write boundaries.

A host replay verifies framing and supplied bytes. It does not independently
authenticate the historical hardware run, execute the proprietary loader,
or prove JamBox-to-stock installation on the owner's unit.

## Concrete unresolved physical transition/recovery gates

1. **Stock stage-one staging placement remains unresolved.** Neither the
   upload nor the read-only identity report contains the running VM state or
   staging address. Existing stock firmware research describes VM/loader
   allocation but does not establish exact V15 placement at E0000/E4F00.
   JamBox's bounded cleanup intentionally clears only E4F00's sector. Do not
   broaden it to scan/erase every CRC-looking record in foreign storage.
2. **Stock acceptance and reset handover remain untested for JamBox.** The
   loader size and app directory are compatible on paper; stock's historical
   no-op/product gates are not a native JamBox implementation test. An
   unrelated stock-to-stock capture cannot close this gap.
3. **Official return's proprietary write/finalize behavior remains static
   evidence plus a prior stock-origin capture.** JamBox stage-one normal and
   rescue simulation can verify acceptance, bounds and corruption handling;
   host emulation of ota-FM-1_015 cannot prove its implementation ran.
4. **Power-loss recovery is a real open issue, not an assumed flash-record
   feature.** Pinned ChoralRoot commit
   `aa5875af20e3046ee161f988c5d91d3ae66d0c74`, `docs/BOOT-SAFETY.md:23–40`,
   describes two identical hardware flash reads of an interrupted update:
   intact V15 boot head, intact staged loader, valid E4F00 record, but no
   ota-FM-1 after cold power-on. The authors cannot distinguish SPL ignoring
   the record from a cold-start loader failure; either way, the flash record
   did not provide recovery. Their subsequent stub/Transporter design is
   not part of JamBox and must not be claimed for it. JamBox's loader-core
   replay and preserved record do not prove cold-boot resume.
5. **The upload is not a backup of this FM-1.** It does not record the
   owner's current VM settings, custom DX7 banks, calibration/identity
   sectors or whole-chip state. The current read-only site identifies
   software and does not acquire those bytes. A physical recovery/backup
   route remains to be established before a supervised installation;
   upstream Transporter/ROM claims are not a tested setup for this owner.
6. Hardware flash JEDEC/key, boot-head equality, USB timing, actual audio
   behavior and the inherited IRQ timing-budget failure remain separate
   gates. No native or browser simulation resolves them.

There is no discovered byte-format blocker to continuing **offline host
transition simulations** with the exact stock package. There is also no
basis to enable public installation or claim a physically verified round
trip. Keep firmware release and installer publication disabled.
