# JamBoxFM

Custom M-VAVE FM-1 instrument: scrappy band jams, dusty beats, and big synth hooks. The first software prototype has drums, bass, chords and lead, an 88 BPM Dusty groove, two curated sounds per part, and four sound-shaping controls.

Development continues in the public [anthonysprints/jamboxfm repository](https://github.com/anthonysprints/jamboxfm), on `main`. The website publishing target is `https://anthonysprints.github.io/jamboxfm/emu/`; check its deployment status before treating it as live.

The [browser instrument](docs/WEB_EMULATOR.md) has the FM-1's 27 keys, 14 buttons, seven encoders, master control and live 240×240 screen. The firmware's C UI, sound and sequencing run together through WebAssembly. You can play one-key chords, record, browse sounds and use the real deeper editing pages. The earlier four-part browser workbench is also retained.

## Develop in the cloud

```sh
bash tools/cloud/setup.sh
. tools/cloud/activate.sh
sh tests/run_jam_emu_tests.sh
```

This builds and packages the device-style emulator, tests its native controls and runs actual Chromium interactions. Outputs include `build/jambox-emulator.html` and the isolated static site `build/jambox-web/`. See [browser opening/hosting instructions](docs/WEB_EMULATOR.md) for delivery details and local-file limitations. Packaging does not publish a website.

For repeatable public updates, use the [GitHub Pages publishing instructions](docs/GITHUB_PAGES.md). The dedicated publication branch contains only the built JamBox website and its source/license downloads.

`sh tests/run_jam_tests.sh` still builds the four-part workbench in `build/jam-preview/` and runs the palette, Home and native/WASM audio contracts. See [cloud setup](docs/CLOUD_SETUP.md) for all commands and device prerequisites.

The first slice uses a RAM session and Sloop's existing chord/scale/recording behavior. The planned Assist master switch, Gentle timing, new note-ownership policy, complete undo and versioned persistence remain future milestones. The first FM-1 diagnostic build compiles and fits the device's memory limits; update/rescue support, hardware timing and flashing remain untested. See [device build evidence](docs/build-evidence/device-build/README.md).

## Design and provenance

- [Product brief](PRODUCT_BRIEF.md): agreed musical direction and user experience.
- [Implementation plan](docs/IMPLEMENTATION_PLAN.md): selected firmware baseline, modules, control mapping, sound/voice budgets, recording, persistence, browser and installer architecture.
- [First FM-1 device build plan](docs/FM1_DEVICE_PLAN.md): Dinghy/FM-1-RE findings, compiler path, update/recovery work, first hardware trial and release gates.
- [Research evidence](docs/RESEARCH_EVIDENCE.md): pinned source links, REA results, executed host checks and limitations.
- [Validation plan](docs/VALIDATION_PLAN.md): reproducible build, software, browser, hardware and release gates.
- [Current build status](docs/BUILD_STATUS.md): implemented slice, executed checks and remaining hardware blocker.

Sloop's source is imported at `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`, with its licenses and notices retained. The original README is in [docs/upstream/SLOOP_README.md](docs/upstream/SLOOP_README.md); [the import manifest](docs/upstream/import-manifest.json) records original file hashes. Research is retained under `docs/research/`; current build evidence is under `docs/build-evidence/`.

The FM-1 prototype is selected with `JAMBOX_PROTO=1`; the default device build still selects the inherited Sloop application. Use the documented prototype wrapper. No JamBox installer or release package is ready yet.
