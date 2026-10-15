# Separate FM-1 manual-saving trial

This development installer belongs to `next/usability-and-saving`:

`https://anthonysprints.github.io/jamboxfm/preview/saving/install/`

It updates an already-running JamBoxFM application to the gated manual-saving
build. It does not accept factory firmware, Sloop, rescue-only or loader-only
starting states. Automatic saving is off. The normal `/install/`, browser
instrument and stable source remain separate.

The [integration notes](SAVE_RECOVERY_INTEGRATION.md) describe the implementation,
fault tests and target resource limits. Passing host tests and receiving the
JamBox firmware identity do not verify physical saving or interrupted-power
recovery. The test below is the first saving check on the owner's instrument.

## Prepare and install

1. Start with a disposable jam. Updating restarts the instrument; the stable
   build's current temporary music is not preserved by this installation.
2. Keep the previously verified FM-1 recovery folder and original factory
   firmware separately. The usual Windows folder is
   `C:\Users\homepc\JamBoxFM-backup\backups\fm1-recovery-...`.
   Find the matching `backup.json` and `full-flash-before.bin` inside one run.
3. In desktop Chrome/Edge, close other MIDI applications and instrument tabs,
   connect only one FM-1 with a data cable, and open the separate trial page.
   Connect and allow MIDI/system-exclusive access.
4. Choose those two backup files in the page. They stay on the computer. The
   check compares the 1 MiB dump with the report's SHA-256 and expected backup
   fields. It does not establish freshness, USB serial, historical double-read
   verification, or inclusion of the current RAM jam. No new backup is captured
   by Chrome; the prior reviewed backup remains the recovery baseline.
5. Acknowledge that the current jam is disposable, then choose Install the
   saving trial. Keep power/USB connected and the page open through the update
   loader and the returned JamBox application.
6. Save the installation report. Transfer completion checks the firmware family
   and release response, which are shared with the earlier JamBox build. Check
   that tapping SAVE actually opens the new recovery screen before treating the
   saving build as running. A failed transfer is not automatically retried.

The raw full-flash dump is a private diagnostic/recovery baseline, not a Chrome
installation file. The unbricker's firmware restore takes a `.fwsc` package;
do not feed it the raw `.bin` as an update. Keep the existing recovery instructions
and factory package. Starting from a damaged application or deliberately cutting
power during an update is outside this first trial.

## Small save-and-restart check

1. Make a short jam and choose an obvious tempo, such as 93 BPM. Change one
   sound setting so the result is easy to recognize. Optionally store distinct
   A/B sections, including a different FM6 sound, to check section restoration.
2. Stop playback/recording and release keys and sustain. Let sound changes
   settle. Tap SAVE to open recovery status.
3. On first use, status is Setup needed. Tap SAVE again to read the storage
   warning. Initialization replaces the old project/USER-sound allocations;
   factory sounds, calibration and protected areas stay separate. With SAVE
   released, turn knob 1 to BACKUP READY: YES, then make a fresh two-second
   SAVE hold. Wait for Saved. HOME cancels before setup.
4. Later checkpoints use a SAVE tap from the recovery screen. Wait for Saved
   before turning off. Audio pauses during a manual save; its real duration
   still needs measuring on the device.
5. Fully power off. Unplug USB if it keeps the instrument powered. Turn it back
   on and check that playback is stopped, recording is off, and the notes,
   tempo, actual edited sounds, A–D sections and selected Home view return.
6. Start playback and listen, then change one item, save again and repeat the
   restart. Note whether the first and second checkpoints both return correctly.

Report what the SAVE screen says, approximate save/boot time, whether audio and
controls return, and which musical items match. If saving reports Error or Read
only, keep the instrument and its report available; the software does not label
that checkpoint Saved or silently retry it. Navigation can conservatively mark
Pending even without a musical edit. Named song archives and idle autosave are
later work.

## Maintainer workflow

Read `AGENTS.md` and use the existing checkout. Do not invoke the production
publisher for this preview. After activating the existing cloud tools:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-recovery
python3 tools/package_jam_recovery_preview.py --no-build
python3 tests/test_package_jam_recovery.py
python3 tests/test_publish_jam_recovery_preview.py
JAM_RECOVERY_SITE_ROOT=build/jambox-recovery-preview node tests/jam_recovery_install_browser.mjs
python3 tools/publish_jam_recovery_preview.py
python3 tools/publish_jam_recovery_preview.py --publish
```

Package after all source/UI/doc edits settle: the bundle includes their exact
corresponding source. The normal packager still rejects recovery-enabled builds.
The separate publisher has a fixed `preview/saving/` prefix on `gh-pages`, makes
a non-forced push, rejects remote races and preserves every outside Git object,
path and mode. It changes no stable root file, firmware URL or source branch.
Source changes are committed/pushed only on `next/usability-and-saving`.

Validate the actual published HTTP artifacts before claiming the preview is
live. A Git push verifies repository publication, not completion of Pages or
reachability through the current cloud network. Private backup dumps/reports
never enter either publication package.
