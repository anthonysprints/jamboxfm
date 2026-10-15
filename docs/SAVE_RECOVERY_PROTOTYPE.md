# Recovery prototype: host verification

This page records the first host-only milestone. For the later, gated device
integration and its current limits, see [Save/startup integration](SAVE_RECOVERY_INTEGRATION.md).

October 14, 2026 (owner's timezone). Implemented on `next/usability-and-saving`,
based on `d85862e2d3a896e5958d98a9fc01f5eefdf0f2e0`.

This completes the first **host-only codec/NOR proof**, not device saving.
The application, flash permissions, SAVE behavior, startup and live installer
are unchanged. No device compiler, physical flash or production publisher was
invoked. The new modules live under `firmware/experimental/recovery/` and are
not included by the instrument build.

Tracks [recovery #1](https://github.com/anthonysprints/jamboxfm/issues/1),
[save controls #3](https://github.com/anthonysprints/jamboxfm/issues/3) and
[roadmap #10](https://github.com/anthonysprints/jamboxfm/issues/10).
The [implementation plan](SAVE_RECOVERY_PLAN.md) still governs later integration.

## What exists

- [Codec](../firmware/experimental/recovery/jr_codec.h): a fixed-size,
  explicitly little-endian session representation with streaming reads and
  semantic validation. It does not dump native structs into flash or allocate
  a second whole-session image.
- [Store](../firmware/experimental/recovery/jr_store.h): callback-only NOR access,
  disjoint seven-sector A/B generations, commit-last header and full readback.
  There are no hardware hooks. It requires a source that stays immutable for
  the whole transaction; a device mutation-freezing/revision guard is later work.
- [Real-instrument fixture](../tests/jr_instrument_test.c): actual capture/apply,
  settings and DSP behind a test-only bridge, including synthetic edited sounds.
  `--emit` and `--verify` work in independent OS processes.
- [Fault tests](../tests/jr_store_test.c): simulated 1 MiB NOR, protected-byte
  assertions, callback failures and exhaustive interruption prefixes.
- [Asset identity check](../tools/check_jam_recovery_identity.py): two SHA-256
  digests bind the format to actual generated assets and the source catalog.
  The runner checks the generated identity instead of trusting a revision label.

## Wire layout and allocation

Payload size is exactly **27,272 bytes**. The five snapshots are the current
working loop followed by A-D. Missing sections and unused FM6 sidecars must be
zero, and a section mask determines which snapshots are present.

| Logical payload offset | Bytes | Content |
| ---: | ---: | --- |
| 0 | 22,920 | Five explicit 4,584-byte JRM1 musical blocks |
| 22,920 | 2,340 | Three actual 156-byte FM6 patch sidecars per snapshot |
| 25,260 | 1,464 | All four shared synthesized drum kits |
| 26,724 | 512 | Sound identities, editing position, own preferences, asset identity |
| 27,236 | 36 | A-D arrangement configuration |

Each 28,672-byte generation starts with a 256-byte JBR1 commit area, leaving
1,144 bytes beyond the current payload. The header fields are:

| Byte offset | Type | Meaning |
| ---: | --- | --- |
| 0 | 4 bytes | `JBR1` |
| 4, 6, 8, 10 | LE16 each | Schema 1, recovery kind 1, header size 256, allocation version 1 |
| 12, 16, 20, 24 | LE32 each | Exact payload size, generation, payload CRC-32, exact allocation-list CRC-32 |
| 28-247 | Zero | Reserved |
| 248 | LE32 | CRC-32 of header bytes 0-247 |
| 252 | LE32 | Commit marker `0x31434d4a` |

CRC-32 uses the IEEE/zlib polynomial. Generation comparison is modular, with
wraparound covered; equal-generation unequal bodies and half-range ambiguity
are read-only. Future outer or inner formats and incompatible asset sets remain
protected, even when an older compatible record can still be recovered.

The simulated allocation is unchanged from the plan:

- A: seven sectors `97000-9E000`.
- B: `9E000-A0000`, `DC000-E0000`, `FE000-FF000`, in that logical order.

Ranges include their start and exclude their end. This would retire legacy
project/autosave and disabled USER-preset allocations. It preserves samples,
FM6 banks, calibration, updater data, device data and the free FB sector. It
does not remove the 44 built-in Home sounds. **The physical allocation has not
been changed or initialized.** First-use backup/initialization UI is not built.

The store validates source content before erase, verifies every erased sector,
streams page-bounded data into the inactive copy, verifies the body, revalidates
the source, commits the header last and reads back the complete record. Unknown
or unreadable data blocks writes. A verified first record claims both declared
copies, permitting the later B checkpoint over the explicitly claimed legacy
allocation. An incomplete first header remains unclaimed and retryable.

## Verified results

Run from the checkout after sourcing `tools/cloud/activate.sh`:

```sh
sh tests/run_jam_recovery_tests.sh
CC="$CC" sh tests/jam_storage_run.sh
```

Results for this prototype:

| Check | Result |
| --- | --- |
| Codec, strict C11/warnings | 1,727 checks passed |
| Actual instrument parameters, dependencies and round trips | 17,649 checks passed |
| NOR model | 440 checks passed |
| Exhaustive interruption prefixes | 112,402 passed; first save and replacement save |
| Independent-process full-session recovery | Two distinct sessions saved, loaded and restored; 60 restoration checks each |
| Codec ASan + UBSan, system GCC | 1,727 checks passed |
| Store ASan + UBSan, system GCC | 440 checks and all 112,402 interruption prefixes passed |
| Existing persistence/update isolation runner | Passed, including firmware rescue and loader protection regressions |

The interruption harness records the writer's exact successful erase/program
operations, replays every byte prefix and calls the actual store scan after
each prefix. For first save it accepts no committed generation until one is
valid; for replacement it requires the complete previous or new generation.
It additionally injects failures into actual save callbacks, checks all 256
partial first-header prefixes, first-A/reboot/first-B ownership, falsely
successful writes, post-commit readback errors, source mutation, corruption,
future schemas/components/assets, generation wrap and conflicting ties.
This proves the **modeled NOR behavior**, not every physical power-failure mode.

Real-instrument tests cover all 12 engines and 44 curated sounds, hidden steps,
independent lengths, locks, all five musical snapshots, actual FM6 edits, all
SYN kits, arrangement metadata and the complete typed preference block.
Manual-note renders of recovered sounds and all SYN kits are sample-identical
and checked for audible signal, rather than comparing two silent outputs.
Mutable sample/FM6-bank references in base parameters or active locks are
rejected instead of pretending their missing assets were saved.

Review also caught and fixed future inner-component protection and malformed
duration/fill records that ordinary project application would silently normalize.
The asset checker and linked source hashes are recorded in
[machine-readable evidence](build-evidence/recovery-prototype.json).
Detailed local run logs remain under `build/jam-recovery-*.log`.

## What is deliberately still pending

The fixture's FM6 sidecars, preference restore and transient reset are **test-only
adapters**. They do not fix production section A -> B -> A transitions or the
arranger's working-loop backup. Recorded sequence playback through source-changing
FM6 locks, actual boot rollback after a late read error and section-application
audio deadlines remain integration checks.

Next implement real capture/apply/sidecar bookkeeping and boot rollback behind
a separate recovery gate, then manual Save now and honest status. Before any
device write, prove linked RAM/stack/image budgets, freeze or track every live
mutator, add deliberate backup/initialization, and retain the existing USB-rescue
route. Device autosave still needs sustained quiet detection including effect
tails/texture, timing/backoff, transfer exclusion and physical audio/USB checks.

No named-song library, user sample import, permanent user preset bank, extra
tracks, device installation or production publication is included in this
prototype. The current instrument remains usable throughout that later work.
