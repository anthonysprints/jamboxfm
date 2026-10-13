# First JamBox software prototype

October 13, 2026. The source contains the first Dusty palette, physical Home screen and shared C browser instrument. The owner-supplied compiler now builds the first JamBox FM-1 diagnostic target. This remains a software prototype, with no physical-device test or firmware release.

The follow-up [physical-device plan](FM1_DEVICE_PLAN.md) incorporates Dinghy and FM-1-RE. It separates temporary musical sessions from the flash support needed for firmware updates and early USB rescue. The current OTA-disabled prototype wrapper remains a diagnostic compile target, not an owner-device installation candidate.

## Implemented

- Four roles: Drums, Bass, Chords and Lead, preserving Sloop's internal track IDs.
- An 88 BPM, two-bar synthesized drum groove; empty melodic patterns ready for recording.
- Two curated sounds per role. Melodic macros control brightness, movement, grit and space; drum macros control tone, level, echo and room.
- A 240×240 Home screen with real physical encoder/button handling and access to the existing deeper sound pages.
- A browser AudioWorklet running the shared C DSP, keyboard mapping, chord generation and sequencer. Keyboard/touch pads, sound browsing, macros, mute, tempo, swing and drum-step editing work through C commands.
- Chord and bass capture through the existing grid-snapped recorder. Finishing a take leaves playback running. Browser part selection finishes the current take and releases its held input.
- Bounded browser commands, panic on overflow/focus loss, and explicit audio start.

Exact patch, beat and macro values are in [DUSTY_PALETTE.md](DUSTY_PALETTE.md). The default application remains Sloop; `JAMBOX_PROTO=1` selects the device prototype. The prototype rejects enabled flash storage or OTA. No JamBox package has been flashed or published.

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

The prototype device C composition passed a host syntax check with freestanding settings. Its enabled-flash configuration correctly failed at the explicit build guard. These checks are not a pi32v2 compile, link, memory report or real-device timing measurement.

The UI sanitizer investigation found no address failures but did report inherited signed-shift undefined behavior in `drum_synth.c` and `fx.c`. It is not a sanitizer-clean result. Existing host compiler warnings are retained in the logs. No golden DSP baselines or assertions were weakened to pass.

The device build follow-up now produces `.fwsc` files for the unchanged Sloop baseline and JamBox diagnostic target. Its full inherited runner outcome, memory gates and clean-source comparison are recorded in [device build evidence](build-evidence/device-build/README.md). Current validation commands are in [CLOUD_SETUP.md](CLOUD_SETUP.md).

That full Sloop run exited 1. Its IRQ instruction-cost budget is stale relative to the published Sloop 2.6 machine code: the rebuilt IRQ is byte-identical to the shipped one. Separately, the PWA launch-path workaround ran 14/15 assertions successfully; Chromium 154 reports `in-incognito` for the unchanged test's installability check. Both failures remain visible. JamBox's own native/emulator browser regression passed, and no baseline or assertion was relaxed.

## Remaining implementation boundaries

The new Assist master switch, Off/Gentle/Tight timing capture, stronger live/playback note ownership, complete undo, JAM1 persistence/import and supported installer transitions are not implemented. Browser sessions are temporary. Its existing chord/scale setting changes release currently held pads; the future new-input-only Assist contract is a separate milestone. Home macros do not yet register MIDI Learn or the last-selected lock parameter. Other synthesis engines remain on their existing deeper editing pages.

The diagnostic target occupies 533,012 of 581,564 available app bytes. Ordinary RAM uses 70,520 of 98,304 bytes; the large-buffer pool uses 324,580 of 344,064. These figures are for OTA/storage disabled and will change when those capabilities are added. Static section checks cannot establish total audio interrupt timing, dynamic stack use, USB/TRS behavior, saved-data migration or recovery. Those remain future gates. The owner's installed Sloop version has not been identified.

## Environment

Cloud setup, setup replay, pinned SDK download into a fresh destination, and local serving of HTML/JavaScript/WASM passed. Python, browser/native compilers, SDK and Playwright dependencies are installed outside the checkout. Reusable installation and startup instructions have been saved to the environment draft.

The official vendor endpoint became reachable, but its `jl-update.oss-cn-shenzhen.aliyuncs.com` archive redirect remained blocked by proxy CONNECT 403. The owner's uploaded archive resolved that prerequisite locally. Its SHA-256, Clang version and acquisition method are recorded honestly; future setup reuses the verified cache or requires the same bytes. Network additions and reusable instructions are saved in the environment draft. Saving that draft does not publish the environment.
