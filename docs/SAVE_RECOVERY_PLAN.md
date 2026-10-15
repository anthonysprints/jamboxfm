# Working-session recovery plan

Research date: October 14, 2026 (owner's timezone). The
[host-only codec/NOR prototype](SAVE_RECOVERY_PROTOTYPE.md) and subsequent
[gated Save/startup integration](SAVE_RECOVERY_INTEGRATION.md) are implemented
and software-verified on the development branch. The owner chose manual saving;
automatic saving is deferred, not a requirement for this release. Physical
device validation remains separate. Existing application persistence stays off in normal builds.
Source baseline: `99cbf62495d9f021be80ee3d20fcef739dd8c93e`; development starts
at `addd576e4616987f4178327a91f4fa3b731c6543` on
`next/usability-and-saving`. The owner has authorized merging the saving source
into `main`; ordinary builds still keep recovery disabled and the published
installer is unchanged. Further cleanup stays on the development branch. The
separate trial preview is the only authorized development publication subtree.

Tracks [recovery #1](https://github.com/anthonysprints/jamboxfm/issues/1) and
[save controls #3](https://github.com/anthonysprints/jamboxfm/issues/3), under
[roadmap #10](https://github.com/anthonysprints/jamboxfm/issues/10).
Named archives (#2) and additional tracks (#4) remain later work.

The owner's saving preference is an intentional checkpoint: it changes only
when Save now is chosen. Experimenting without saving and restarting should
restore that manual checkpoint. Do not add automatic writes over it. Earlier
autosave research below is retained for reference, outside the active scope.

## Recommended first result

First prove a bounded recovery record against simulated flash, with production
writes still disabled. A small **current-loop** fixture is a useful first
engineering step, not the finished promise of returning to the same session.

Before a general-purpose device save is called complete, restore the current
four-part loop **and populated A-D sections and arrangement**, including their
supported sound state. Do not display unrestricted `Saved` for a loop-only
prototype when other session content remains temporary. The current loop is
separate from the four stored sections, so the maximum is five snapshots.

Then deliver manual **Save now -> cold restart -> same session**, with playback
stopped and recording off. Manual saving is the chosen current behavior;
idle autosave is deferred unless the owner reopens it.
Recovery always means the last fully committed checkpoint; there is no verified
shutdown callback that can capture edits after power is removed.

## What the audit established

| Finding | Consequence |
| --- | --- |
| `project_t` is JRM1, 4,584 bytes; inherited storage payload is 3,840 bytes | Do not flip a persistence flag or assume the old save slot fits. |
| Boot clears A-D and creates Dusty | Recovery must have a deliberate place in initialization. |
| Snapshots keep patch/kit numbers, not all edited sound data | Notes alone are insufficient for faithful sound restoration. |
| The four sections and arrangement are separate objects | Saving the current loop does not save an entire arranged session. |
| `audio_quiet()` checks synth/drum voices only | Delay/reverb, inserts, freeze, click and always-on texture need separate handling. |
| Main and audio interrupt paths can change durable parameters | Stopped playback alone does not make serialization consistent. |
| Legacy readers/writers share foreign formats and storage | Add a separate JamBox recovery policy rather than enabling all inherited persistence. |

Evidence: [project types and assertions](../firmware/src/project.c#L54),
[capture/apply](../firmware/src/project.c#L475),
[autosave and voice-only quiet test](../firmware/src/project.c#L704),
[boot policy](../firmware/src/project.c#L797),
[instrument startup](../firmware/src/instrument_init.c#L23),
[flash hooks](../firmware/src/flash_access.c#L39), and
[application policy](../firmware/src/app_policy.h#L19).

## Durable content and boundaries

The recovery bundle has typed musical, sound-dependency, editing-position and
device-preference components. Future named-song loads must not change device
preferences just because a recovery bundle can contain them.

| Component | Include |
| --- | --- |
| Current loop and each used A-D section | All four parts; all 64 steps, including hidden steps; independent note lengths; dynamics/ratchets; microtiming/fill; parameter locks; engine and ordinary sound parameters; timing, harmony, mix and effects. |
| Actual FM6 sounds | Canonical actual patch bytes per melodic part and snapshot, not only PTCH numbers. Apply these after ordinary project restore and synchronize slot/pending state so `fm6_poll()` cannot replace them. |
| Synthesized drum edits | One shared bank containing all four SYN kits, preserving today's shared-bank semantics. Do not silently turn these into independent historical kit versions per section. |
| Arrangement/editing position | Valid section mask, order/repeats, arrangement enabled state and selected section where meaningful; no running arrangement clock. |
| Sound identity | Stable explicit catalog IDs/origin, independent of browser order, plus an edited/custom indication. Restore labels without loading a factory preset over saved parameters. |
| Playing position in the interface | Selected part, octave and last GROOVE/SOUND Home view. Enter HOME, not an open dialog or held layer. |
| Device preferences | A separately typed, bounded component for JamBox's own display/lights, recording setup and MIDI routing/Learn settings. Decode only this family's validated values; distinguish these from song content. |

Preserve ordinary track mute (`P_MUTE`). Treat temporary solo/lane-mute masks,
held notes, sustain/controllers, arpeggiator input, momentary fills/punch effects,
undo/redo history, effect buffers, waveform phases and running playhead as
transient. Reset them. Re-read the physical master volume instead of restoring
its previous gain. Recording, count-in and free-take start disarmed.

Hardware calibration is a separate physical configuration. Keep the existing
validated, read-only bridge to `FC000/FD000` and leave those sectors unchanged.
This milestone does not make new hardware calibration permanent; its existing
`CALIBRATION RAM ONLY` limitation remains explicit. Do not substitute foreign
musical settings for this narrow calibration import.

Current RAM sections and `song_keep` also omit actual FM6 edits. Add bounded
sound sidecars to section capture/apply and song backup/restore before claiming
per-section fidelity. Cold-boot storage cannot reconstruct data that the live
section snapshot never captured. Shared SYN-kit edits remain global to the
working session, as they are while powered on today.

### Resolve every reachable sound dependency

Inspect base parameters **and all parameter locks** in the current loop and
every used section. Locks can select other drum kits, SAMPLE/GRAIN sources or
FM6 PTCH slots. Capture all four editable SYN kits, not just the selected one.

The first format covers built-in samples/kits and embedded actual FM6 sounds.
Keep user sample import, USER preset storage and mutable FM6 bank upload
disabled. A loop with an unavailable/mutable source dependency outside that
contract must receive an explicit unsupported-source result before any erase,
not a misleading `Saved`. Define empty-bank fallback identities explicitly if
they are supported; never depend on whatever a bank slot contains later.

Bind built-in dependencies to stable IDs and a compatible asset/catalog
revision. Across firmware updates, migrate known identities or reject the
incompatible recovery without overwriting it. Missing assets must not be
silently replaced with a different sound. A complete external-sample backup
format is future work, not implied by the small record budget below.

## Storage alternatives and recommended full-session layout

The current map is defined by [object mapping](../firmware/src/storage.c#L52),
[range guards](../firmware/hal/fm1_flash_ranges.h),
[sample slots](../firmware/src/eng_sample.c#L36), and
[package descriptors](../tools/fm1pkg_make.py).
All hexadecimal ranges in this section include their first address and exclude
their end address.

| Region | Existing use / treatment |
| --- | --- |
| `00000-04000` | Boot head/SPL: preserve. |
| `04000-93000` | Application/update destination: preserve. |
| `93000-97000` | Ownership not proven for application storage: do not claim as free. |
| `97000-9F000` | Four legacy project A/B pairs: 8 sectors. |
| `9F000-A0000`, `FE000-FF000` | Legacy autosave copies: 2 sectors. |
| `A0000-DC000`, `E7000-FB000` | Four user sample slots: preserve. |
| `DC000-E0000` | Two currently disabled USER preset A/B banks: 4 sectors. |
| `E0000-E5000` | Updater staging and JamBox resume record: preserve. |
| `E5000-E7000` | FM6 bank: preserve. |
| `FB000-FC000` | One explicitly free sector. |
| `FC000-FE000` | Settings/calibration A/B: read-only in this milestone. |
| `FF000-100000` | Device data/key material: preserve. |

The validated stock-V15 update handoff can also require loader cleanup of
`E8000-E9000`. It is inside a sample allocation, **not** available recovery
space. Preserve the updater's existing exact ownership/cleanup contract.

### Size comparison

These original **planning allowances** are now matched by the prototype's
explicit fixed-size wire codec. Its exact bounds and host verification are in
[the prototype record](SAVE_RECOVERY_PROTOTYPE.md). Device capture and memory
integration still need proof; do not rely on host padding or compression ratios.
The FM6 allowance uses its current 156-byte array stride; the last byte is
reserved/sanitized, not a substitute for a dependency table.

| Payload component | Current-loop fixture | Full working session |
| --- | ---: | ---: |
| Existing musical snapshot allowance | 4,584 B | 5 x 4,584 = 22,920 B |
| Actual FM6 patch allowance | 3 x 156 = 468 B | 5 x 3 x 156 = 2,340 B |
| Shared four-kit SYN bank | 1,464 B | 1,464 B |
| Metadata/identities/preferences allowance | 512 B | 512 B |
| Arrangement config | Not included | 36 B |
| **Total payload allowance** | **7,028 B** | **27,272 B** |
| Generation capacity, after 256-byte commit area | 7,936 B (2 sectors) | 28,416 B (7 sectors) |
| Remaining payload margin | 908 B | 1,144 B |
| Two complete generations | 16 KiB | 56 KiB |

Within the full-session 512-byte metadata allowance, budget 160 bytes for five
sets of four 8-byte sound identities, 32 for editing-position/session flags,
128 for the separately typed device preferences (including the 40-byte MIDI
map/settings allowance), 64 for asset/catalog identity and 128 reserved. Commit
framing/component offsets must fit the separate 256-byte header area. These
sub-budgets must become explicit checked limits in the codec, not grow silently.

A loop-only host fixture fits two sectors per generation, for example
`97000-99000` and `99000-9B000`. This is an alternative test configuration,
not a second store to ship alongside the full layout. Supporting all 27 mutable
FM6 bank patches would already break that small allowance; samples are larger
still. Dependency validation is mandatory.

The old project/autosave sectors plus the free sector total 11 sectors/44 KiB,
short of the 14 sectors/56 KiB needed for this full-session approach.

**Recommended implementation candidate:** deliberately retire the disabled
USER preset banks and use this logical-record map:

| Copy | Ordered physical extents | Capacity |
| --- | --- | ---: |
| A | `97000-9E000` | 7 sectors |
| B | `9E000-A0000`, then `DC000-E0000`, then `FE000-FF000` | 2 + 4 + 1 sectors |

The first logical 256 bytes of each copy contain its commit area; the remaining
logical bytes hold the payload across these extents. Keep `FB000` free. No
sample, FM6-bank, calibration, updater or device-data allocation changes.

This is a **new storage map**, not a compatible extension of Sloop storage.
It trades currently disabled USER preset storage for complete-session recovery.
These are saved-user-sound banks, not the factory catalog: the 44 built-in HOME
sound choices remain available.
Future saved-sound banks and the two-to-four-song library need a separate
budget/format decision; do not promise that they also fit this map.
If this tradeoff is rejected during implementation review, redesign the codec
or allocation and prove its maximum size before device writes. Do not silently
ship a partial-session save or consume sample space to make a budget pass.

## Record validation, ownership and commit rules

The host prototype uses the distinct JamBox recovery family `JBR1`, with an
explicit schema version, record kind, length/count limits, generation number,
component layout/IDs, payload CRC and header CRC. Exact offsets and fixtures
are recorded in the prototype; JRM1 remains the private
runtime/export snapshot rather than being reinterpreted as a legacy flash object.

Keep `FELUCCA_PERSIST=0`. Introduce a separate recovery feature/policy gate and
narrow write hooks. The low-level allowed ranges must be the selected recovery
extents and the existing updater window, not the entire legacy store. Existing
legacy preset/project/section/FM6/sample/restore writers stay disabled. Preserve
the `!FELUCCA_PERSIST` calibration bridge and current updater isolation checks.

For each checkpoint:

1. Validate the supported content/dependency closure and exact size before erase.
2. Select the inactive generation; never erase the currently usable copy.
3. Erase its sectors and stream bounded payload pages, respecting NOR page
   boundaries. Preserve existing audio-silencing and watchdog requirements.
4. Validate the complete written body before commit. Commit the header last,
   only if the captured state is still the intended coherent generation.
5. Read back header and the complete payload, including semantic validation.
   Only then advance the saved revision and show `Saved`.

On boot, fully validate both candidates before choosing a usable generation.
Use wrap-safe generation comparison and deterministic tie handling. Reject
truncation and excess bytes; the inherited prefix-truncating `st_load()` is not
a suitable recovery interface. CRC-valid but malformed known-schema content
must not mask an older usable copy.

Unknown families/future schemas are read-only. An older compatible generation
may be offered for recovery, but must not authorize overwriting an unknown
newer record. On missing/corrupt data, create a usable in-memory Dusty session
without automatically saving defaults over the store.

First-use initialization must explicitly claim the new allocation after a
verified backup/export and a clear explanation of the old regions it replaces.
An erased store also needs an intentional first initialization. Establish
ownership with a fully verified first record carrying the exact allocation
version and extent claim. That claim covers both copies: after a successful A
initialization, a later B save may replace the exact legacy bytes included in
the confirmed allocation, even though B has not yet held a JamBox record.
It must not authorize erasing a valid unknown/newer JamBox recovery schema or
an incompatible allocation claim; detect those before initialization and before
each write. With no verified claim after an interrupted first save, return to
Setup needed without implicitly taking ownership. Cancellation must not claim
success. This is a future device action, not part of this research. Normal boot
never formats or silently repairs storage.

A failed write/readback leaves RAM intact and dirty, keeps the old generation,
and reports an error. A header may have committed before readback failed; a
later boot can validly find that new copy. Do not erase it merely to make the
error message match the old saved revision.

## Consistent capture and memory budget

Current build evidence (`build/jambox-device/memory.json`, inspected during this
research) reports:

| Resource | Used / capacity | Required remaining reserve |
| --- | ---: | ---: |
| Ordinary RAM | 87,944 / 98,304 B | 4,096 B |
| Pool | 325,348 / 344,064 B | 8,192 B |
| Retained region | 13,952 / 15,696 B | Do not enlarge it |
| Application | 558,804 / 581,564 B | Recheck linked image |

The 3,840-byte storage buffer and 4,584-byte arranger `song_keep` already exist.
The legacy `autosave_buf` and `proj_tmp` are currently removed by the linker;
they are not free buffers already paid for. Do not revive both unintentionally.

Stream the large record from bounded resident snapshots/components rather than
allocating a second 27-KiB session image. One new 4,584-byte pool capture buffer
would leave 14,132 pool bytes free, 5,940 above reserve. Four FM6 section sidecars
plus one arranger-backup sidecar cost 2,340 ordinary bytes before identities and
bookkeeping. Aim for no more than 4,096 new ordinary bytes and 6,144 new pool
bytes; these are implementation targets, not a passed allocation check.
Rebuild and inspect the real linked image before accepting the design.

Main owns UI/editor/kit edits, but the audio interrupt consumes keyboard/MIDI
events and can change parameters, locks and recording state. Use an audited
save transaction that freezes durable-state mutations at a block boundary,
while allowing audio tails to render. For a small capture, mask interrupts only
for a measured bounded copy/recheck, with encoding and hashes outside that window.

For the streamed session, hold a short explicit saving state that rejects or
defers UI/editor mutations and defers persistent MIDI/transport mutations.
Preserve note-off/panic handling. Define bounded queue/overflow behavior and
return busy for transfers; do not accumulate an unbounded event list. A mutation
generation check must cover every remaining writer and abort an uncommitted
copy if the captured state changes. Atomically validate that revision when
committing; a new edit after commit leaves `Pending` rather than marking it saved.
Do not keep interrupts disabled for the whole serialization/write sequence.

At boot, validate the whole generation before applying any live data. With
audio not started, decode components through bounded scratch/existing snapshot
space. Prevalidation cannot guarantee that a later flash read succeeds. Keep
audio and preference-dependent hardware activation unpublished until the full
decode/application pass succeeds. On any later failure, reinitialize every
partially loaded preference, snapshot, arrangement, sidecar and sound identity
before trying the older candidate or Dusty. No partial generation becomes an
active session. Revalidate source integrity during application rather than
mixing a previously checked header with unchecked later reads.

## Startup, scheduling and user feedback

Boot should read validated calibration independently, inspect the recovery
record read-only, restore its own device preferences before dependent hardware
setup, initialize engine/runtime defaults, then restore the musical components,
sound dependencies and editing position before first audio render. Ensure a
later Dusty initializer, Home assignment or `layers_init()` does not undo them.
Split existing initialization where needed so successful full record decoding
and preference selection precede external activation; do not start USB/audio
using preferences from a record whose later components have not yet succeeded.

Preserve USB rescue and the failed-boot escape. A crash/failed restore must not
loop forever through the same bad session or replace it with defaults. Support
a deliberate start-without-restoring path that leaves stored bytes untouched.
Treat healthy boot, intentional update reset and failed boot separately; the
retained bootguard is not proof of cold-boot persistence.

| Status | Meaning |
| --- | --- |
| Unsaved / Setup needed | No verified initialized recovery for this session. |
| Pending | Unsaved changes or conservative input tracking; another explicit Save now is required. No automatic write is queued. |
| Saving | A coherent checkpoint is being written/verified. |
| Saved | Current durable revision exactly matches the last fully verified record. |
| Error | The attempt failed; work remains in RAM and the last valid recovery is preserved. |

Manual Save now bypasses debounce/rate limits, but still requires stopped,
disarmed transport and a safe audio/transfer state. For indefinitely running
texture/freeze, offer a deliberate short fade/pause for manual saving; automatic
save must never silently cut audible sound. If not accepted, stay Pending.

### Deferred autosave research

This research is not an implementation task or release gate. Any future
automatic recovery design must preserve the owner's intentional manual checkpoint.

For a possible future autosave implementation, use a conservative proposed **10 seconds
of stable, quiet state**, at least **60 seconds between successful automatic
saves**, and **30-second backoff after failures**. These values follow inspected
Felucca/Ghoulbox practice and remain tunable after simulated wear/latency checks.
Save now provides the immediate stopped checkpoint. Track durable edits from
MIDI/editor as well as local keys/knobs; unchanged content produces no erase.

The quiet gate must include held local/MIDI notes and releases, click,
insert/slicer state, shared effect tails, freeze/texture, playback/recording/
count-in/free-take and active editor/OTA/backup transfers. Observe full internal
audio before the master volume, not just silence at a turned-down speaker.
Require sustained quiet, not a single zero sample or gap between delay echoes.
Document a threshold/tail bound and verify it; indefinite output simply defers
autosave. USB audio, watchdog/USB responsiveness and worst-case erase duration
need physical measurement. Seven-sector writes must not inherit a one-sector
timing assumption.

## Implementation batches and acceptance gates

1. **Codec and content proof, writes off.** Define exact JBR1 fields/bounds,
   supported dependencies and full-session fixtures. Round-trip all supported
   engines, four parts and all five musical snapshots. Add FM6 sidecars and
   stable identities without changing the live release.
2. **NOR storage proof, writes off on device.** Implement logical extents and
   A/B commit/readback in a dedicated host harness. Exhaust interruptions and
   malformed input; demonstrate independent-process cold boots and protected
   flash preservation. Do not weaken current persistence-off regressions.
3. **Manual save and restore candidate.** Add narrowly gated hooks, intentional
   initialization, Save now/status and startup integration. Recheck actual
   memory/image budgets and failed-boot/USB rescue. No general `Saved` until the
   entire declared session/dependencies are protected.
4. **Deliberate FM-1 trial and adoption.** Preserve a verified route back to the
   stable build, identify the development candidate, back up before storage
   initialization, measure audio/USB/erase behavior and verify cold restarts.
   Merge/publish only after review and owner feedback. This document requests
   no install, formatting, power interruption or device experiment now.

Quiet autosave is deferred outside these acceptance gates. Do not implement it
unless the owner requests it again, and do not overwrite intentional manual
saves with automatic recovery records.

Required fault/round-trip checks include:

- Maximum valid records; all 64 steps, four onsets, independent lengths,
  hidden content, locks, section boundaries, FM6 edits and all SYN-kit edits.
- Powered-session custom-FM6 A -> B -> A transitions and arrangement start ->
  stop restoring the edited working loop, including macros, sound identity and
  factory PTCH locks. Check `fm6_track_loaded()`/`fm6_poll()` interactions and
  audio timing of section application, not just storage byte round trips.
- Unknown engine/schema/component IDs, bad counts, inconsistent dependencies,
  truncated/oversized data and CRC-valid malformed content. Rejection must not
  mutate the live destination.
- First-save and replacement-save interruption before/during every erase and
  payload/header program, including partial pages and partial commit headers.
  Restore only a whole old or whole new generation, never mixed components.
- Read/program/erase failures, writes falsely reporting success, readback
  failures, newest/oldest/both-copy corruption, generation wrap and ties.
- Every protected flash byte unchanged, including the gap between B extents;
  reject address overflow, unaligned erase and writes crossing region/page ends.
- Separate writer/reader processes sharing only a file-backed NOR image.
  Retained/static RAM must not make a fake cold-boot test pass.
- First verified A initialization -> reboot -> first B checkpoint over the
  claimed legacy allocation, plus interrupted initialization and incompatible
  future-schema/allocation refusal. Inject a read failure after each streamed
  component and verify complete rollback before fallback or hardware activation.
- Boot stopped/REC off, no held notes, correct labels/Home/octave, safe corrupt-
  record fallback, no default checkpoint after failed restore, rescue intact.
- No unchanged saves, all persistent mutators tracked, stable-state timer and
  retry wrap, no automatic write during audible output or active transfers,
  and Pending retained for edits newer than the committed generation.

## Verified references and baseline checks

Pinned source inspected for this pass:

- [Sloop `8fad1a15`](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/project.c#L618):
  changed-content idle save, 2.5-second idle / 20-second gap. Its snapshot format
  and quiet test are not a drop-in solution for JamBox.
- [Felucca `17275376`](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/tests/persistence_test.c#L260)
  and [Ghoulbox `2f001b1f`](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth/blob/2f001b1f550eac695d8400f9f388c8eac6dbb4c6/tests/persistence_test.c#L260):
  10-second stable/quiet, 60-second successful-save gap, 30-second failed retry;
  tests include crash/boot suppression and simulated wear. Inspect the exact
  pins rather than assuming the older research snapshots have this policy.
- [ChoralRoot `aa5875af`](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/LOOPER.md#L541):
  deliberate manual/slot-leave saving, not idle autosave. Its
  [storage tests](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/tests/storage_test.c#L125)
  and [cross-process persistence script](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/tools/emu/test_persist.sh#L27)
  provide fault-injection/real-cold-start patterns. Its live slot-switch write
  exception can interrupt audio and is not adopted here.

Existing code checks actually rerun during this research, with the configured
Clang 19.1.7 environment and no connected device:

- `tests/storage_test.c`: **28 checks passed** against simulated NOR.
- `sh tests/run_jam_note_project_tests.sh`: **34 native checks and 17 editor
  protocol checks passed**.
- `sh tests/jam_storage_run.sh`: **passed**, including persistence-off isolation,
  foreign-data/calibration handling, real UI/editor refusal paths, updater/loader
  preservation, configuration guards and firmware rescue regressions. It used
  existing firmware binaries as host fixtures; no device compiler or flash ran.

A temporary host probe also confirmed two inherited behaviors that the new
format must avoid: generation rollover selects the wrong copy with ordinary
unsigned `>` ordering, and `st_load()` returns a truncated prefix when its buffer
is too small. Neither is a current JamBox save failure, because application
persistence is disabled. The passing baseline tests do **not** validate JBR1,
multi-sector commits, the proposed map or any new device saving behavior.
