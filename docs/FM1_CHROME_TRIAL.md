# Install and update JamBoxFM in Chrome

**Current hardware finding:** the owner's first normal-JamBox update stopped
with `noloader` before the application-writing stage. The existing instrument
still works, but the UI update is not installed. See the
[handoff failure record](build-evidence/ui-update-handoff-failure.md).
Do not repeat that attempt without diagnosing the handoff; the software tests
below did not establish this physical transition.

This page supports an experimental first installation from original factory
V15 (`FM-1_015`) and an update from a running JamBox build (`FM-1_700` with
the matching JamBox family response). Both perform a real firmware write.
The owner reports that the first installation succeeded and that the
instrument is generally working. This is useful hardware feedback, rather
than a completed validation of every control, sound or recovery path.
Musical sessions in this build are temporary.
The separate `/device/` page remains read-only.

## Owner workflow

1. Keep the verified backup folder. Close other MIDI applications and FM-1
   browser tabs, and connect one powered FM-1 by USB.
2. Open the site's `/install/` page in desktop Chrome or Edge. Connect and
   allow MIDI access. The page checks the selected device and downloads and
   verifies the exact experimental build.
3. Select **Install experimental JamBoxFM**, or **Update experimental JamBoxFM**
   if JamBox is already running. Keep USB and power connected and
   the page open. Progress follows preparing, writing and reconnecting stages;
   a transfer count is not a measured flash percentage.
4. Success requires the final JamBox identity and its matching family response.
   Download the installation report and check that the screen shows JamBox.
   Start at low listening volume, play the starter groove, then try chords,
   bass, lead and the sound controls. Report any resets, silence, stuck notes,
   incorrect controls or audible dropouts.

If installation stops, keep the device connected, save the report and share
the result before retrying or resetting. The app may be partly written. A
verified backup is useful for recovery but does not guarantee that ROM entry
will remain available after an interruption. Recovery can require additional
hardware. This page does not offer automatic retry, loader
resume, factory restoration or general firmware selection.

## Scope of the experimental workflow

The owner requested actual installation. Their private double-read backup
has been independently checked: all boot-head and application bytes match
factory V15, the chip key and flash ID match, and the Windows RAM-loader read
path ran successfully. See [backup evidence](build-evidence/factory-v15/owner-backup-validation.md).
No raw backup, retained RAM, manufacturer package or user report is published.

The candidate manifest retains `release_enabled: false`,
`hardware_validated: false` and its existing simulation record. An additional
schema-2 `trial.json` binds the exact manifest and firmware digests to exactly
`factory-v15-to-jambox` and `jambox-normal-to-jambox`. This is distinct from a stable
release or completed hardware validation.

A [bounded stock-record correction](build-evidence/factory-v15/stock-record-cleanup.md) handles the factory updater’s distinct handoff location without a broad scan or erase of data sectors.

The production controller uses the shared updater directly. It accepts only
the exact factory V15 identity or the exact normal JamBox application profile.
For JamBox, the family response must have schema 1, family `JAMBOXFM`, normal
mode, release `0.1.0-dev`, capability mask `0x17`, and identity `FM-1_700`.
It repeats the identity and family checks immediately before staging, checks
that only one device is present, and requires exact JamBox family responses
at loader and final stages. Rescue mode and standalone loaders remain
ineligible starting points. The controller
refuses ambiguous re-enumeration, foreign loaders and mismatched downloads.
No test simulator is imported by the page. Firmware, corresponding source,
licenses and build evidence are packaged together.

The installation report records which transition actually completed. The
offered firmware is bound by SHA-256; final device verification confirms
family and release. Multiple development builds share `0.1.0-dev`, so that
final response does not independently identify the installed build's bytes.

## Remaining hardware work

The owner has reported a successful first installation and general musical
use, with specific knob, chord and key-lighting feedback now being addressed.
The new JamBox-to-JamBox route has host transfer and browser coverage; its
next update on this instrument remains a physical trial. Sustained audio,
all controls and interrupted-update recovery are not yet fully validated.
A later return-to-factory
trial is separate. App rescue relies on executable application code and is
not a protected bootloader or a promise of power-loss recovery.

The inherited static IRQ estimate remains **256 versus its 174 budget**.
The published Sloop 2.6 ISR matches its unchanged rebuild, which scores 268;
none of the 18 compared metrics increased in JamBox. The threshold has not
been relaxed. This heuristic does not demonstrate a JamBox regression, and
physical audio timing is an outcome of the trial rather than a circular
requirement to have already run it. The earlier full Sloop test run also had
an unrelated Chromium PWA assertion failure; neither is reported as passing.

## Build and package

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/package_jam_device.py --no-build
python3 tools/package_jam_emu.py --with-trial
node --test tests/jam_trial_install_test.mjs tests/jam_web_transport_test.mjs
JAM_INSTALL_SITE_ROOT="$PWD/build/jambox-web" node tests/jam_install_browser.mjs
python3 tests/test_publish_jam_web.py
python3 tools/publish_jam_web.py
```

The site packager's explicit `--with-trial` option includes the installer and
verified candidate. Without that option the base package remains the emulator
and read-only checker. Publication requires the separate `--publish` command
described in [Pages instructions](GITHUB_PAGES.md). A successful Git push alone
does not establish that the new files are live.
