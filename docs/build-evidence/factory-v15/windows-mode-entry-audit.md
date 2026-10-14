# Stock V15 -> ROM -> stock: Windows mode-entry audit

Scope: source/documentation review only. No device commands, firmware transfer,
binary execution, publication, or project source edits. Owner has confirmed
Windows and no valued saved sounds/settings. This removes the need for sound
export instructions, not the value of a unit-specific calibration/flash snapshot.

Pinned evidence:
- FM-1-transporter `a632d923203170e3a08565f4056dbb3fd3d65ca5`.
- MvaveFM1Unbricker `df394beea4c881da4161fbc8a87730050b57968c` (previous source audit).

## Conclusion

There is credible upstream **physical stock V15 evidence** for a USB-MIDI soft
key that changes into ROM boot without programming flash. A Windows sender can
be prepared with an ordinary data cable and no extra electronics. The exact
Windows transport and a full round trip on this owner's unit are still untested.
This is an achievable next physical **mode-entry/return test**, not yet a firmware
install or a proof that an interrupted JamBox install can always be recovered.

A complete trial must verify the return after a **cold power cycle**. Upstream
observed stock V15 running without any USB enumeration after a warm RUN_APP or
watchdog reset. Do not mistake that known state for a new brick, and do not
promise a warm software exit restores MIDI.

## Exact packet and meaning

Application-level MIDI SysEx, six bytes:

    F0 22 24 35 7D F7

A normal MIDI API should receive these six bytes as one SysEx message (or its
four data bytes `22 24 35 7D` when the API supplies F0/F7 itself).

Transporter's exact USB-MIDI transfer is eight bytes:

    04 F0 22 24 07 35 7D F7

The two four-byte USB events have cable 0/CIN 4 (SysEx start/continue, three data
bytes) and cable 0/CIN 7 (SysEx end, three data bytes). `04` and `07` are **USB
headers**, not bytes to insert into the SysEx payload. The stock receive path
matches this short USB transfer; the Transporter sends it to the enumerated
MIDI bulk OUT endpoint. The C code gates the operation on a mounted stock VID/
PID and retries at most three times 20 ms apart until the device leaves the bus.

The value `7D` selects ROM UBOOT entry. Do not confuse it with `7F`: JamBox's
existing installer defines `F0 22 24 35 7F F7` as UPGRADE (firmware staging).
There is no OTA data envelope, firmware payload, checksum, flash address, or
flash erase/program command in this mode-entry packet.

Source:
https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/src/pio_host.c#L99-L119

## Physical evidence and expected observations

Transporter DEVLOG M2 explicitly records stock V15 accepting that key: V15 left
the bus about 21 ms after sending and returned as UBOOT about 1.0 s later. Those
are observed timings on the upstream setup, not Windows deadlines to enforce.
The protocol reference is labelled hardware-verified and names this alternative
entry independently of the external USB_KEY electrical waveform.

The real measurement used the Transporter's PIO USB host. Its boot procedure
adds pulses to stabilize host enumeration; a PC should use its ordinary USB
class driver, not try to reproduce those electrical pulses. The reference
`fm-1-research-lab/tools/fm1_softkey.py` is not present in the public checkout,
so there is no reviewed upstream Windows standalone sender to reuse verbatim.

Expected transition:

| Stage | Evidence |
| --- | --- |
| Normal factory V15 | Owner's factory screen; valid MIDI identity FM-1_015; USB 4C4A:C755 |
| ROM boot | Normal MIDI disappears; USB 4C4A:8057; USB product string WL80UBOOT1.00 |
| Windows ROM disk | Win32_DiskDrive model includes WL82 UBOOT1.00 USB Device, InterfaceType USB |
| SCSI inquiry, if backup is separately run | Vendor WL82, product UBOOT1.00, revision 1.00 |

`WL80UBOOT1.00` in a raw USB descriptor and `WL82 UBOOT1.00` in the disk/SCSI
name are both expected; do not demand one identical string in both places. A
black screen or disappearance of MIDI alone is insufficient proof of entry.
Successful OS enumeration is the first round-trip test's milestone. No SCSI disk
opening or RAM-loader upload is needed merely to demonstrate ROM enumeration.

Sources:
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/docs/DEVLOG.md#L128-L149
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/docs/PROTOCOL.md#L8-L32
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/src/pio_host.c#L276-L303
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L378-L407

## Return to intact factory firmware

The soft key itself does not change flash, so the existing app remains there.
The protocol says ROM waits while idle and resets/boots flash when the host
releases it. DEVLOG also reports that V15 can run after warm reset but not attach
USB; after a session the README explicitly calls for power-cycling.

For the planned mode-only trial, after recording ROM enumeration (or failure to
observe it), close the sender, disconnect USB, switch the FM-1 off, wait about
five seconds, switch it on, then reconnect USB. Five seconds follows the reviewed
Unbricker's normal completion instructions; it is not a measured minimum.
Verify both the original factory screen/keys and a fresh valid FM-1_015 identity
using the existing read-only device checker. Record failure as failure, without
calling any automatic restore/erase path.

Do not use RUN_APP as the required exit. Its source sends a loader command and
returns even if reset interrupts the response; the known V15 warm-USB behavior
makes it unsuitable as this test's final verification.

Evidence strength: upstream confirms V15 booted normally after a self-test that
rewrote its own sector bytes and verified the full image unchanged, plus the
power-cycle requirement. There is no pinned Windows log for a mode-only six-byte
send -> ROM -> cold-boot round trip on this owner's unit. Therefore the proposed
owner trial validates a gap; it is not presented as already completed.

Sources:
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/docs/PROTOCOL.md#L23-L32
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/docs/DEVLOG.md#L142-L150
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/README.md#L83-L92
- https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/src/jieli_uboot.c#L266-L274
- https://github.com/Quixotic7/MvaveFM1Unbricker/blob/df394beea4c881da4161fbc8a87730050b57968c/fm1_unbrick.py#L1168-L1175

## Recommended Windows sender design

Prepare a small **separate local mode-entry helper**, preserving the public
identity page's read-only contract. Python 3 with `mido` + `python-rtmidi` is the
most direct fit with this repository's reviewed MIDI parsing and Windows port
support. Use the existing strict `parse_identity`/handshake, but do not expose or
call Updater.install, resume, serve, UPGRADE, disk restore, or arbitrary SysEx.
A raw WinMM ctypes/PowerShell sender saves a dependency but adds buffer lifetime,
MIDIHDR layout, and callback complexity without improving the current safety
case. Web MIDI could also deliver the message, but the existing public checker
should not silently acquire mode-changing behavior.

The helper should:

1. Default to listing/checking only. Require an explicit `enter-rom` action.
2. List input and output ports; require an explicitly selected pair. Reject
   missing, duplicate/ambiguous, mismatched, or disconnected ports. Reuse exact
   matching/unique Windows suffix pairing only as a visible proposal.
3. Before sending, verify a fresh canonical identity with all three fields
   agreeing: text FM-1_015, model FM-1, version 15. Do not use a startsWith or
   number-only check. Optionally require exactly one present 4C4A:C755 in Windows
   inventory as corroboration; names or VID/PID alone do not authenticate bytes.
4. Recheck the selected connection at action time and send the single six-byte
   ROM key. The only transmitted messages should be the established identity
   query and that key. Stop sending immediately after the key; no automatic
   retry after re-enumeration and no fallback to UPGRADE.
5. Release MIDI handles in finally. An output error at the moment of disconnect
   is ambiguous, not proof the key failed: still inspect Windows enumeration.
6. Use bounded OS inventory polling (e.g. a 15-second overall observation window
   with short individual subprocess timeouts). Require exactly one new matching
   ROM device, record limited model/VID/PID evidence, and never open its disk.
   An existing ROM device before the action should prevent confusing another
   unit's enumeration with this transition.
7. Show the cold-power-cycle return instructions for both successful entry and
   timeout. A timeout should not trigger more commands or a recovery write.
8. Use the existing read-only checker for post-return identification. Treat
   incoming identity as observed software evidence, not cryptographic proof of
   firmware contents.

Useful meaningful tests before giving this helper to the owner: exact message
allowlist; wrong/changed identity results in no key; explicit pairing failures;
no key after cancellation/disconnect before send; handles close on every exit;
ambiguous/pre-existing ROM disks rejected; OS enumeration error/timeout cannot
be marked success; report contains no machine-specific serials or device paths.
No need to rerun unrelated DSP tests for a standalone host helper.

Mido example of correct API framing (design, not executed):

    output.send(mido.Message('sysex', data=[0x22, 0x24, 0x35, 0x7D]))

That API supplies F0 and F7. The Windows MIDI stack's exact USB packet grouping
for this short message remains an empirical gap: the upstream code used a
single exact eight-byte bulk transfer. If the guarded normal MIDI sender fails,
report it; do not switch USB drivers or attempt raw-disk writes to work around it.

## Owner-facing sequence once the helper is implemented and checked

- Fully charge the FM-1; use the known data cable directly; close other MIDI
  apps/tabs. The upstream protocol reports a soft-key failure with another
  full-speed device sharing a hub. This is a reported compatibility condition,
  not a reason to require disassembling the PC or changing USB drivers.
- Run the helper's read-only check. Confirm it shows FM-1_015 on the chosen pair.
- Invoke the explicit mode-entry action. Expect the screen/MIDI to disappear and
  Windows to show ROM boot. Cancel any Windows prompt to format the disk.
- Record the result and perform the cold-power-cycle return. Recheck the factory
  screen and FM-1_015 report. Stop here for a minimal round-trip trial.
- Once that succeeds, a separately planned ROM session can use only Unbricker's
  backup command to establish chip/flash IDs and preserve calibration/flash.
  It runs a RAM loader but does not erase/program flash. No sounds export is
  needed given the owner's answer. No new electronics purchase is needed to
  attempt these existing-stock steps.

## What this does not settle

A functioning stock app is required to receive the soft key. Proving entry now
does not make it callable after that app has been replaced or partially erased.
A future no-USB hard failure could still need external USB_KEY hardware. This
trial establishes actual Windows ROM access and a working cold return while the
factory app is intact; it does not validate JamBox startup, audio timing, OTA
writes, or power-loss recovery.
