# Development Save now and startup recovery

Implemented on `next/usability-and-saving`, following the
[host-only prototype](SAVE_RECOVERY_PROTOTYPE.md), for
[recovery #1](https://github.com/anthonysprints/jamboxfm/issues/1) and
[save controls #3](https://github.com/anthonysprints/jamboxfm/issues/3).
The owner confirmed on October 15, 2026 that Save now, full power-off and startup
restore the jam and edited sounds, and approved the same firmware for the normal
Chrome installer. `--with-updater` now enables the manual-saving profile; the
normal packager requires it. Automatic saving remains disabled. This is basic
owner-reported device validation, not proof of all power-failure/recovery behavior.
See [normal installer promotion](MANUAL_SAVING_RELEASE.md).

## What this stage does

The development build captures the current four-track jam, populated A–D
sections, arrangement, actual edited FM6 patches, all four shared SYN drum kits,
sound identities, selected track/octave/Home view and the typed preferences
listed in the [plan](SAVE_RECOVERY_PLAN.md). It restores the newest fully valid
checkpoint before audio and USB start, stopped and with recording disarmed.
An A–D section is still a section of this session, not a separate saved song.

The SAVE interaction in this build is:

1. Tap SAVE to open the recovery status screen. Opening it never writes.
2. On first use, another tap opens the allocation warning. It explains that
   setup replaces old project/USER-sound storage and preserves factory sounds.
   Turn knob 1 to acknowledge a backup while SAVE is released, then make a fresh
   two-second SAVE hold to initialize and save. HOME cancels.
3. Once initialized, a SAVE tap on this screen checkpoints the session.
   Playback/recording must be stopped, keys and sustain released, sound loads
   settled, and USB transfers closed. PLAY can stop an existing jam here.

Saving deliberately pauses audio. The duration, audio behavior and USB behavior
have **not** been measured on hardware. The screen reports Saving, Saved,
Pending, Error, Setup needed or Read only. Panel/MIDI activity and USB editor
mutations conservatively mark Pending; navigation can also mark it Pending.
Failed saves are not silently retried. Unsupported external USER/sample/FM6-bank
dependencies are rejected before erase, and selecting built-in sounds permits
a retry without rebooting.

**Idle autosave is not enabled.** Powering off recovers the last successful
manual checkpoint; edits since that checkpoint are not saved automatically.
Repeated explicit saves may create a new generation even without musical edits.
The owner selected this manual-checkpoint behavior for the current product:
unsaved experiments must not replace an intentional save. Autosave is deferred.

## How the integration stays bounded

- `jam_recovery_runtime.c` streams the existing 27,272-byte representation from
  one 4,584-byte staging project plus existing section storage, bounded sound
  sidecars and 512 bytes of metadata. It does not allocate another whole bundle.
- A–D changes and arrangement backup/restore carry actual FM6 patches and their
  labels together. Restoring an edited patch also synchronizes its PTCH marker,
  so ordinary polling does not overwrite the recovered edit.
- A synchronous save masks only the audio interrupt source and silences its DMA
  buffer. Other interrupts remain available between individual flash operations;
  the existing flash driver still uses its bounded critical sections. Live-state
  gates are checked again after masking. Queued musical input is discarded around
  the pause. Update/transfer requests are checked again during transaction
  validation; the injected mid-save request aborts before committing the record.
- Physical flash callbacks and the final RAM-driver guard restrict recovery
  writes to its fourteen allocated sectors, only during an authorized transaction.
  Legacy application persistence stays disabled; the updater keeps its own window.
- Startup scans and validates before restoration. The exact decoded/staged values
  are re-encoded, checked against the committed CRC and semantically validated
  before publication. A transient bad read therefore cannot publish a plausible
  but incorrect drum/patch/preference value. Native project/SYN headers receive
  their own checks too. Failed application resets all owned state before trying
  the other complete generation; uncertain reads leave writes disabled.
- A previous failed boot starts fresh with recovery read only for that boot,
  preserving the record. The existing manual USB-rescue boot path remains.

The [allocation and commit-last format](SAVE_RECOVERY_PROTOTYPE.md#wire-layout-and-allocation)
are unchanged. The owner subsequently initialized saving and confirmed a manual save/restart.

## Build and publication isolation

`JAMBOX_RECOVERY` defaults to zero. The normal updater target enables it; the trial command remains available:

```sh
bash tools/cloud/build_device.sh --with-recovery
```

The build requires JamBox, FLASH and OTA, with legacy PERSIST disabled. Its
receipt records the gate. The normal `--with-updater` configuration now enables this same approved
manual-saving profile, and normal packaging requires it. The separate preview
remains available. Publication and owner feedback are recorded in the release notes.

The target compiler, linker, RAM-resident flash-driver check, register-access
policy and independent resource report all pass:

| Resource | Recovery build | Capacity | Remaining |
| --- | ---: | ---: | ---: |
| App image | 574,572 B | 581,564 B | 6,992 B |
| Ordinary RAM | 91,432 B | 98,304 B | 6,872 B |
| Pool | 329,932 B | 344,064 B | 14,132 B |
| Retained RAM | 13,952 B | 15,696 B | 1,744 B |

RAM and pool clear their existing 4 KiB/8 KiB minimum reserves. These are linked
static budgets, not measured peak stack usage or physical audio deadlines.
The earlier recovery-off baseline occupied these image/RAM/pool budgets:
558,804 / 87,944 / 325,348 bytes. Historical byte identity is not claimed because
the firmware embeds the compilation date.

## Verification

Run after sourcing `tools/cloud/activate.sh`:

```sh
sh tests/run_jam_recovery_tests.sh
sh tests/run_jam_recovery_runtime_tests.sh
sh tests/run_jam_recovery_ui_tests.sh
sh tests/jam_storage_run.sh
sh tests/run_jam_home_nav_tests.sh
python3 tests/test_package_jam_device.py
```

The UI fixture needs icon-enabled generated headers; the device target deliberately
uses icon metadata only. `JAM_GEN_DIR` selects a separately generated fixture
directory when running them alongside target builds.

| Check | Result |
| --- | --- |
| Actual runtime integration | 306 checks plus 17 fresh-process child checks |
| Runtime GCC ASan + UBSan | Same checks; zero diagnostics |
| Real panel/LCD recovery interaction | 45 checks; also passed GCC UBSan |
| Existing Home behavior, recovery off | 115 checks |
| Normal package policy | 13 tests; the current normal profile requires manual saving |
| Prototype codec/instrument/NOR suite | Passed again, including 112,402 interruption prefixes |
| Existing storage/update/loader isolation runner | Passed; 246 storage checks and remaining transport/rescue/loader checks |
| Both recovery-on and normal target builds | Passed; no unresolved/unsupported runtime symbols |

Runtime checks exercise whole-session restoration, section A/B/A, arrangement
working-loop restoration, actual FM6/SYN edits, calibration preservation, no boot
writes, erase/program failures, 32 late read failures, ten transient successful
but incorrect reads, future/corrupt records, failed-boot suppression, unsupported
dependency retry, and an OTA request arriving during saving. A separate process
restores using only the simulated NOR bytes. UI tests cover exact hold timing,
fresh acknowledgement, cancel/release guards, sound loading within the modal,
recording exclusion and visible Saving state. These are meaningful host models;
they do not establish physical power-failure behavior.

Sources, binary hashes, memory results and local log hashes are recorded in
[integration evidence](build-evidence/recovery-integration.json).

## Next work before a release

The [separate development installer](FM1_SAVING_TRIAL.md) now provides the trial
workflow without replacing the stable website. The trial needs a verified backup, measured save/boot time and
stack/audio/USB behavior, section/FM6-lock playback, cold power cycles, recovery
after interrupted writes, and the existing rescue route. The production installer
must remain separate until those checks and adoption are deliberate.

Keep manual saving as the current behavior. Autosave is deferred unless the
owner explicitly reopens it; any future automatic recovery must protect the
intentional manual checkpoint. Named song archives, extra tracks and broader
screen cleanup remain separate issues; this stage changes none of them.
