# Owner update report: loader handoff not confirmed

2026-10-14, 10:14 AM America/New_York. The owner uploaded the installer's
local report after attempting the first UI update. The private original report
is not committed; this record contains the relevant non-identifying fields.

## Established outcome

- Intended firmware SHA-256:
  `1e2c7a956081db0e71e2a455d1532b2c555e26d24bc3794f1d23e0b33255afd8`.
- Transition: normal JamBox to JamBox; source `FM-1_700`.
- Outcome: `error`, `noloader`; final identity absent.
- Stage-one request count: 20. Stage-two/write request count: 0.
- Stages reached: checking, start, verify, loader. The error followed the
  loader settling/discovery window.
- The owner reports the instrument operates normally afterward and confirms
  the Home ALGORITHM knob still changes sounds. Thus the new BPM behavior
  is not installed; normal operation alone must not be reported as success.

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

## Next step and limits

Keep the functioning existing build. Do not label the update as successful,
ask for a blind repeat, relax identity checks, or write via ROM to bypass an
undiagnosed handoff. Obtain the observed on-device error/reset behavior before
choosing the next write attempt. The already available MIDI device check is
read-only, but its family/release identity cannot distinguish these two builds.

If USB serial was already enabled, existing read-only console status/crash
and staging-record reads may help. Do not reboot just to enable it: normal
boot clears stale staging evidence. More detailed bounded reconnect reporting
is useful future work, separate from a verified handoff fix.
