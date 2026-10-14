# JamBox software prototype and FM-1 update isolation

The owner reports that a retry of the normal-JamBox UI update succeeded, with
BPM control and SNAP lights working as expected on the FM-1. No intervening
firmware fix was published; the earlier `noloader` failure remains unexplained
in the [handoff record](build-evidence/ui-update-handoff-failure.md). The next
requested Home mapping is SELECT for tempo and ALGORITHM for part selection,
with PRESETS still choosing sounds. This follow-up's [build and publication
record](build-evidence/home-knob-swap.md) is separate; the owner's confirmation
applies to the preceding update.

October 14, 2026. The owner reports a successful Chrome installation and is playing JamBox on the FM-1. The four UI follow-ups, focused fixes and proposed chord/sequencer work are tracked in [First device feedback](FIRST_DEVICE_FEEDBACK.md). This remains experimental; the owner's report does not complete the hardware-validation or recovery checklist. Earlier milestones below describe their historical validation state.

The [physical-device plan](FM1_DEVICE_PLAN.md) incorporates Dinghy and FM-1-RE. The default device wrapper builds the OTA-disabled diagnostic target; `--with-updater` selects the experimental update-enabled target. The owner’s factory backup is verified. The [Chrome trial workflow](FM1_CHROME_TRIAL.md) provides installation and normal JamBox updates at `/install/`, while `/device/` remains read-only and the candidate’s hardware/release flags remain false.

## Implemented

- Four roles: Drums, Bass, Chords and Lead, preserving Sloop's internal track IDs.
- An 88 BPM, two-bar synthesized drum groove; empty melodic patterns ready for recording.
- Two curated sounds per role. Melodic macros control brightness, movement, grit and space; drum macros control tone, level, echo and room.
- A 240×240 Home screen with real physical encoder/button handling and access to the existing deeper sound pages.
- A browser AudioWorklet running the shared C DSP, keyboard mapping, chord generation and sequencer. Keyboard/touch pads, sound browsing, macros, mute, tempo, swing and drum-step editing work through C commands.
- Chord and bass capture through the existing grid-snapped recorder. Finishing a take leaves playback running. Browser part selection finishes the current take and releases its held input.
- Bounded browser commands, panic on overflow/focus loss, and explicit audio start.

Exact patch, beat and macro values are in [DUSTY_PALETTE.md](DUSTY_PALETTE.md). The default application remains Sloop; `JAMBOX_PROTO=1` selects JamBox. `FELUCCA_PERSIST=0` disables app-data persistence independently of `FELUCCA_FLASH` and `FELUCCA_OTA`. The experimental Chrome installer and candidate are now published; the owner reports successful installation.

## Milestone 2: update support with temporary sessions

- `bash tools/cloud/build_device.sh --with-updater` enables low-level flash and OTA, with app persistence disabled and both OTA dry-run/RAM-only modes disabled.
- Normal JamBox boot clears retained projects and settings, starts Dusty, and imports no foreign musical state. A separate read-only helper accepts only a recognized, CRC-validated settings schema containing a strictly valid PAN5 physical-control mapping.
- Project/preset save, erase and editor restore/upload actions report storage unavailable before changing banks or flash. Settings and calibration edits are labeled temporary. Autosave and pre-reset app-data writes are disabled.
- Internal arranger snapshots use private `JRM0` magic. Returning to Sloop cannot treat them as `FUN6` projects. Settings and panel mappings still use shared `SET3`/`PAN5` RAM layouts: if Sloop has no valid flash settings, temporary JamBox preferences/calibration may carry back on a warm return.
- The app's lowest flash writer permits only OTA staging, `[0xE0000, 0xE5000)`. The JamBox loader writes the app region and cleans only its known resume-record sector. Unknown flash identity refuses writes. Normal boot may clear stale OTA staging; the guarantee is no app-data writes, not a completely read-only chip.
- USB timeout/retry and bus-reset handling close or discard abandoned SysEx state, and rescue drains complete messages. Loader cleanup errors prevent a success reset and keep the current loader session available for retry; final regression results are retained with the software evidence.

The [update-isolation evidence](build-evidence/update-isolation/README.md) is the source for final build sizes, flags, hashes and test outcomes. These software checks cannot establish physical USB recovery, flash timing, power-loss behavior or audio deadlines.

## Device-style web emulator

The new [browser instrument](WEB_EMULATOR.md) exposes the physical FM-1 layout: 27 keys, 14 buttons, seven encoders and master level. Its live LCD comes from the real C UI, sharing one WASM instance and sample-driven clock with the DSP. Held-button layers, deeper sound pages, Home, PLAY and REC use the physical input handlers. Calibration is a nonblocking browser notice; storage reports unavailable instead of pretending to save.

The adapter passed 27 native checks. Real Chromium checks passed for screen pixels, selection, sound and macro edits, drag/wheel input, master level, held layers, deeper pages, chord recording, focus-loss release, rapid repeated PLAY, pause during input, and desktop/mobile layout. Independent probes confirmed audible short taps and correct layer release ordering. This is a host port; it does not emulate the AC791 chip or establish device timing.

`tools/package_jam_emu.py` creates a standalone HTML and a separate static site rooted at `build/jambox-web/`. Source and notices accompany the runtime. The public [JamBox browser instrument](https://anthonysprints.github.io/jamboxfm/emu/) is deployed and verified. Follow-up source `5b24d1e` passed 36 native adapter checks and the real Chromium suite, including corrected SNAP note lights; its published HTML, JavaScript and WASM matched the tested build. Managed Chromium blocks local-file navigation, so direct double-click execution is not claimed as tested; use HTTPS hosting when local-file AudioWorklet access is unavailable.

## Validation

The unchanged pinned Sloop source built with Emscripten 3.1.69 and Clang 19.1.7. All twelve engines matched native/WASM samples exactly, and the existing sequence, sample, live edit, mute, transport and timing checks passed.

The JamBox palette tests passed 435 checks. The physical Home harness passed 42 checks, including drawing bounds, real page/button/encoder actions, recorder ownership and sound-label invalidation. The new native/WASM chord render matched exactly; two-chord capture, bass overdub, replay without held keys, parameter bounds, drum edits and queue overflow passed.

The saved aggregate test command passed from the activated cloud environment. The separate Chromium test passed with the actual AudioWorklet: explicit audio start, nonzero audio output, chord/bass capture, keyboard/pointer/touch input, focus-loss panic, pause/resume, immediate part selection followed by editing, canceled selection, repeated toggles, and desktop/mobile layout. Both tests exited 0. An independent review confirmed the corrected selection acknowledgement and pending-transport handling.

Screenshots: [browser desktop](build-evidence/screens/browser-desktop.png), [browser mobile](build-evidence/screens/browser-mobile.png), [physical Home harness](build-evidence/screens/fm1-home.png). The browser screenshots show the interaction test's edited session; boot defaults are documented in the palette specification. Exact commands, logs and artifact hashes are indexed in [the evidence record](build-evidence/README.md).

The audition fixture uses the actual new patches with a demo-only Cm7/Abmaj7 phrase, bass and lead. It peaked at 17,994 on the signed 16-bit range, used at most six busy synth voices in that phrase and reached silence after release/tail draining. Maximum-macro fixtures produced no full-scale samples. This is numeric audio validation; musical voicing still needs listening feedback.

Before milestone 1, the prototype device C composition passed a host syntax check and its then-forbidden enabled-flash configuration failed at a build guard. Those historical checks were superseded by actual pi32v2 builds and milestone 2's separate persistence policy; they never established device timing.

The UI sanitizer investigation found no address failures but did report inherited signed-shift undefined behavior in `drum_synth.c` and `fx.c`. It is not a sanitizer-clean result. Existing host compiler warnings are retained in the logs. No golden DSP baselines or assertions were weakened to pass.

The device build follow-up now produces `.fwsc` files for the unchanged Sloop baseline and JamBox diagnostic target. Its full inherited runner outcome, memory gates and clean-source comparison are recorded in [device build evidence](build-evidence/device-build/README.md). Current validation commands are in [CLOUD_SETUP.md](CLOUD_SETUP.md).

Milestone 2 adds real application/UI/editor code over simulated NOR, foreign/retained-data fixtures, strict calibration cases, explicit save rejection, staging-boundary checks, OTA and loader failures, and USB/browser-parser recovery regressions. Run `sh tests/jam_storage_run.sh build/felucca.fwsc` after the update-enabled build. Final test counts and logs belong to the [update-isolation record](build-evidence/update-isolation/README.md); they are separate from the inherited full-suite result below.

That full Sloop run exited 1. Its IRQ instruction-cost budget is stale relative to the published Sloop 2.6 machine code: the rebuilt IRQ is byte-identical to the shipped one. Separately, the PWA launch-path workaround ran 14/15 assertions successfully; Chromium 154 reports `in-incognito` for the unchanged test's installability check. Both failures remain visible. JamBox's own native/emulator browser regression passed, and no baseline or assertion was relaxed.

## Remaining implementation boundaries

The new Assist master switch, Off/Gentle/Tight timing capture, stronger live/playback note ownership, complete undo and JAM1 persistence/import are not implemented. First installation and a normal JamBox update have owner-reported success; complete hardware validation, including interrupted-update recovery, remains outstanding. Browser sessions are temporary. Its existing chord/scale setting changes release currently held pads; the future new-input-only Assist contract is a separate milestone. Home macros do not yet register MIDI Learn or the last-selected lock parameter. Other synthesis engines remain on their existing deeper editing pages.

Milestone 1's diagnostic target occupied 533,012 of 581,564 available app bytes, 70,520 of 98,304 ordinary RAM bytes and 324,580 of 344,064 pool bytes, with OTA/storage disabled. Use the [milestone 3 report](build-evidence/candidate/README.md) for current update-enabled sizes and reserves. Static section checks cannot establish total audio interrupt timing, dynamic stack use, USB/TRS behavior, saved-data migration or physical recovery. Those remain future gates. The owner's original read-only report returned `FM-1_015`, and the factory interface was confirmed as the installation baseline. Its exact uploaded return file is verified; separate host simulations and native staging checks are recorded in [factory preparation](FM1_FACTORY_V15.md). The owner has since reported successful JamBox installation and an update; return to factory remains unverified. See the [factory follow-up](FM1_DEVICE_PLAN.md#software-follow-up-for-the-confirmed-factory-baseline).

## Environment

Windows recovery preparation now has a [separate local workflow](FM1_WINDOWS_BACKUP.md).
The owner confirmed Windows with Python 3.12.7 and no personal sounds/settings to
preserve. The helper verifies the factory identity before a one-shot ROM-mode
request; source preparation pins upstream archives and fixes short-transfer/status
handling in the local backup tool. These are host software changes, with
[mock/source-check evidence](build-evidence/factory-v15/windows-backup-validation.md).
Those software checks establish no owner-device operation. Subsequent
[owner-provided Windows evidence](build-evidence/factory-v15/owner-windows-roundtrip.md)
shows stock MIDI identity, ROM disk enumeration, and a return to factory MIDI
identity. The subsequent [full backup validation](build-evidence/factory-v15/owner-backup-validation.md) establishes Windows RAM-loader startup and full reads, the expected chip/key, and exact factory boot-head/application bytes. JamBox firmware installation remains pending. The public checker remains read-only.

Cloud setup, setup replay, pinned SDK download into a fresh destination, and local serving of HTML/JavaScript/WASM passed. Python, browser/native compilers, SDK and Playwright dependencies are installed outside the checkout. Reusable installation and startup instructions have been saved to the environment draft.

The official vendor endpoint became reachable, but its `jl-update.oss-cn-shenzhen.aliyuncs.com` archive redirect remained blocked by proxy CONNECT 403. The owner's uploaded archive resolved that prerequisite locally. Its SHA-256, Clang version and acquisition method are recorded honestly; future setup reuses the verified cache or requires the same bytes. Network additions and reusable instructions are saved in the environment draft. Saving that draft does not publish the environment.
