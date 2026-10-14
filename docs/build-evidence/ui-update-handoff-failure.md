# Owner update report: loader handoff not confirmed

2026-10-14, 10:14 AM America/New_York. The owner uploaded the installer's
local report after attempting the first UI update. The private original report
is not committed; this record contains the relevant non-identifying fields.

**Later owner follow-up:** a retry succeeded, and the owner confirmed that
BPM control and SNAP lights work as expected on the FM-1. No firmware fix was
published between these attempts. The successful retry supersedes the earlier
hold below; it does not establish the cause of this first failure or complete
hardware validation. The owner then requested swapping Home tempo to SELECT
and part selection to ALGORITHM as a separate change.

## Established outcome of the first attempt

- Intended firmware SHA-256:
  `1e2c7a956081db0e71e2a455d1532b2c555e26d24bc3794f1d23e0b33255afd8`.
- Transition: normal JamBox to JamBox; source `FM-1_700`.
- Outcome: `error`, `noloader`; final identity absent.
- Stage-one request count: 20. Stage-two/write request count: 0.
- Stages reached: checking, start, verify, loader. The error followed the
  loader settling/discovery window.
- The owner reported the instrument operated normally afterward and confirmed
  the Home ALGORITHM knob still changed sounds. Thus the new BPM behavior
  was not installed at that point; normal operation alone was not evidence
  of update success.

The attempt did not reach the browser's main application-writing stage.
Stage one can modify loader staging and resume records, so zero write-stage
requests does **not** mean that no flash bytes changed.

## What the code audit establishes

The offered OTA body is 8,067 bytes; 20 ordinary staging reads are exactly
two UFW-header reads, two loader-header reads, and sixteen loader-body reads.
The terminal completion request is not counted. Entering the browser's
loader stage means it accepted the terminal reply for sending, not that the
device received it, committed its record, or executed a reset.

The old and offered package loaders are byte-identical: 8,099 bytes including
the outer header, SHA-256
`ab1e368dd10ffe8dd1ccfdc03bc235df8992ee5961b20e056ae55079b7659d7e`.
The UI update did not change device staging, reset or loader code. Successful
factory-to-JamBox installation used the factory application's first stage;
it did not validate this JamBox application-to-loader handoff.

`noloader` covers missing ports, silent responders, continuing normal-app
identity replies and some deadline failures. The current report does not
retain these reconnect observations. Existing simulated transfers use new
port objects and do not execute the physical reset/SPL boundary.

Possible device-side branches still needing observation are an unreceived
completion response (`FAILED -13`), failed resume-record write/readback
(`FAILED -14`), or a reset that boots the existing application instead of
the staged loader. None is established by this report. Web MIDI specifies
that closing a connected output finishes already queued immediate messages;
an arbitrary delay before close is not a demonstrated fix.

## Advice at the time of the failed attempt

The recommendation was to keep the functioning existing build and obtain
the observed on-device error/reset behavior before another write attempt.
The failed attempt was not labeled successful, and neither identity checks
nor the ROM write path were changed to bypass the undiagnosed handoff. This
hold is historical: the owner subsequently reported a successful retry as
recorded above. The read-only MIDI device check's family/release identity
cannot distinguish these two development builds.

Existing read-only USB serial console status/crash and staging-record reads
were considered for diagnosis if serial was already enabled. Normal boot
clears stale staging evidence, so the earlier failure may no longer be
diagnosable from the device. More detailed bounded reconnect reporting remains
useful future work. No handoff bug or fix has been established by this record.
