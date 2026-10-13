# Device, controls, build, persistence, and update research

Research date: 2026-10-13. Source inspection and host simulations only; no hardware connected or flashed. All proposed JamBoxFM behavior below remains design, not implemented. Main baseline recommendation is pinned Sloop for its three synth parts plus dedicated drum track, with selected Felucca/ChoralRoot techniques ported individually after baseline validation.

## Exact source pins

- Sloop: `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`
- Felucca: `1727537624390cc3f9c7dff2456b43b3a8edb005`
- ChoralRoot: `aa5875af20e3046ee161f988c5d91d3ae66d0c74`
- Ghoulbox: `2f001b1f550eac695d8400f9f388c8eac6dbb4c6`
- FM-1-transporter: `a632d923203170e3a08565f4056dbb3fd3d65ca5`

## Actual hardware: source evidence

The device has **27 note keys, 14 buttons, seven encoders, one separate MASTER volume potentiometer, and a 240×240 LCD**. There are no encoder-click events in the panel API; all navigation below uses rotation or existing buttons. Do not invent a touchscreen, four dedicated track buttons, or clickable knobs.

Sloop [`panel.c` lines 8–30](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/panel.c#L8-L30):

| Physical label | Logical identifier | Default matrix index |
|---|---|---:|
| FX | B_FX | 2 |
| SEL (between FX and ENV; named SCL in older fork docs) | B_SCL | 3 |
| ENV | B_ENV | 4 |
| LFO | B_LFO | 5 |
| EDIT | B_EDIT | 6 |
| GLO | B_GLO | 7 |
| HOME | B_HOME | 8 |
| SAVE | B_SAVE | 9 |
| ARP | B_ARP | 10 |
| SEQ | B_SEQ | 11 |
| PLAY | B_PLAY | 12 |
| REC | B_REC | 13 |
| OCT− | B_OCTDN | 0 |
| OCT+ | B_OCTUP | 1 |

Encoder roles: `EN_SELECT`, `EN_ALGO`, `EN_PRESET`, `EN_K1..4`; physical matrix indices respectively `0,1,6,2,3,4,5`. Use the existing validated calibration table and `panel_enc()` instead of hard-coding raw indices in JamBox UI. The source allows per-unit learned mappings.

Felucca [`fm1_input.h` lines 144–180](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/hal/fm1_input.h#L144-L180) defines 41 total keys, IDs 0–13 buttons and 14–40 note keys; note bit 0 is F3 and bit 26 G5. [`lcd.c` lines 3–12](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/lcd.c#L3-L12) identifies ST7789-class 240×240 SPI1 panel. [`master.c` lines 3–28](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/master.c#L3-L28) reads the master pot via ADC and initializes its level before audio starts.

Input is an 11-column × 6-row diode matrix behind two 74HC595s. IRQ scan ~10 kHz gives one full matrix frame ~1.1 ms. Latest Felucca uses two closed frames for press and eight open frames for release; encoders emit detents rather than raw quadrature changes. See [`fm1_input.h` lines 3–40, 107–108](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/hal/fm1_input.h#L3-L40). Preserve baseline HAL rather than porting it casually alongside UI changes. Latest Felucca/Sloop main loops read UI repeatedly between ~15 ms draw frames; audio/input IRQs remain independent.

## Proposed first control map

The product is four roles but physical Sloop indices stay fixed initially: synth slots 0–2 = Bass / Chords / Lead; drum slot 3 = Drums. The Home display orders roles Drums / Bass / Chords / Lead through a mapping table, without renumbering storage or MIDI IDs. Every page shows selected role; no hidden track switch on a knob originally expected to shape a sound.

| Actual control | JamBox proposal |
|---|---|
| HOME tap | Return to four-role Jam overview. Each role displays name, sound, mute and record status. |
| SELECT rotate on HOME | Select Drums / Bass / Chords / Lead. |
| SELECT rotate in a menu | Select labeled row; in sound editor, select labeled page. |
| KNOB 1–4 on HOME | Selected sound macros Brightness / Movement / Grit / Space, with actual values/labels on screen. |
| ALGORITHM rotate | Select role on secondary pages; on HOME it may mirror SELECT. Do not silently replace engines in beginner mode. |
| PRESETS rotate | Audition next curated sound for the selected role. Preserve recorded events; make sound change undoable. |
| PLAY | Start/stop whole jam, globally accessible unless a blocking confirmation explicitly consumes it. |
| REC | Stopped: arm selected role and show count-in; playing: begin/end selected-role capture at documented boundary. Visible red record state. |
| SEL / internal B_SCL tap | Assist page. SELECT chooses row; KNOB 1 changes its value. Rows: Assist master; One-key chords; Stay in key; Timing Off/Gentle/Tight; Key; Scale. Role shown in header. |
| GLO tap | Groove/global page: tempo, swing, palette, mixer. SELECT chooses row; KNOB 1 changes it. Swing is outside Assist. |
| GLO mix subpage | Four displayed role levels mapped to K1–4; a separate labeled Mutes page has K1–4 left=mute/right=unmute. Clear button route back to Jam. |
| EDIT tap | Deep sound page; SELECT page; K1–4 actual wave/filter/envelope parameters. |
| ENV / LFO / FX tap | Direct shortcut to envelope/movement/effects page, using the same four-knob presentation. |
| SEQ tap | Pattern/capture view; 16 steps per displayed bank for up to 64 existing steps; keep existing drum grid implementation initially. |
| ARP tap | Rhythm/arp page (initially existing arp with simpler labels; strum later). |
| SAVE tap | Explicit save menu, with pending/saved distinction. Flash writes wait for stopped/quiet state. |
| EDIT + OCT− / OCT+ | Existing Sloop undo/redo, labeled on screen; must consume octave event. |
| OCT− / OCT+ alone | Selected melodic role octave. Preserve baseline drum velocity modifiers where used and show hints. |
| MASTER pot | Master loudness, always. |

This is an initial mapping to test in the emulator, not a promise all optional actions fit cleanly. Palette selection replaces a set of sounds/groove and must have a clear preview/commit route; changing palettes must not erase a recorded jam without an explicit undoable action. No long-press needed to discover the Assist settings. Master Assist OFF preserves per-role choices, alters only new input, and never reinterprets already recorded notes. Swing 0 means straight; per-role/global scope must be shown.

The menu does not need six rows plus full synth graphics simultaneously on 240×240. Use header, four visible rows with scroll, and persistent bottom `SELECT: ROW  K1: CHANGE` hint. Existing input state machines must consume combinations/holds once and prevent a hold release from also firing a tap. Keep live note input when browsing ordinary settings; confirmation/name entry explicitly blocks it and releases owned notes. Note releases must target the track and pitches captured on press even if the user changes role, key, or Assist while holding a key.

Sloop's familiar transport/control evidence: [`README.md` lines 278–289](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/README.md#L278-L289). Felucca's tap/hold suppression implementation: [`ui_input.c` lines 1295–1312](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/ui_input.c#L1295-L1312).

## Persistence: retain one layout, define a new family

Sloop [`storage.c` lines 14–24, 52–62](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/storage.c#L14-L24) uses 4096-byte sectors, 256-byte payload offset, maximum payload 3840 bytes. A/B sectors alternate: write payload first, then CRC-bearing header last; load newest valid copy. Preserve these atomic-save mechanics. An erase stalls audio, so queue saves while transport runs and flush only when stopped/quiet. Do not show SAVED before actual flash commit.

| Sloop object | ID | Sectors / range |
|---|---:|---|
| Settings | 0 | 0xFC000 / 0xFD000 |
| Projects A–D | 1–4 | 0x97000..0x9EFFF, one A/B pair per project |
| User preset banks | 5–6 | 0xDC000..0xDFFFF |
| Working project/autosave | 7 | 0x9F000 / 0xFE000 |
| FM6 bank | 8 | 0xE5000 / 0xE6000 |
| User samples (other path) | — | 0xA0000..0xDBFFF |
| Incoming update loader staging | — | 0xE0000..0xE4FFF |

Firmware image app area is 0x4000..0x92FFF; flash head 0..0x3FFF and device data at 0xFF000 must remain protected. Do not enlarge the app by consuming project or OTA storage.

Sloop current `FUN6` project is **3816 bytes** with 64-step synth/drum patterns, per-step nudge/fill, 24 parameter locks per track, and parameters. Source: [`project.c` lines 9–24, 44–62](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/project.c#L9-L24). Only 24 bytes spare in a 3840-byte payload. Felucca's own current FUN10 payload is already 3840 bytes; its older FUN6 is not Sloop FUN6. Numeric magic coincidences are not compatibility.

For initial JamBox projects, prefer `JAM1` magic, explicit schema version, and per-role Assist metadata in a fixed compact extension, replacing/bumping serializers and their hash together. A possible **24-byte extension** before the final checksum can fit Sloop's current wire size exactly, but must be proven with compile-time size assertions and round-trip tests rather than assumed. Store master state; separate chord and scale enable masks; per-role timing choices; key/scale and chord flavor where not already in stable track parameters; reserved bytes zero. Keep existing per-role swing parameters rather than duplicating them. Do not enable an old `FUN*` conversion on JamBox first boot by default.

If raw-timing capture, names, patch snapshots, or other metadata outgrow that ceiling, reduce/reshape the project schema deliberately or allocate a tested multi-sector object with new backup protocol. Never truncate metadata or quietly overwrite adjacent sectors. Sloop projects refer to FM6 bank patches rather than self-contained 512-byte snapshots; backup must include every referenced bank, sample and settings object or explain missing dependencies.

Using a new payload magic prevents accidental interpretation, but does not preserve old data once JamBox saves into the same sectors. Before a firmware transition, export the source firmware's backup, validate its checksum/format, and keep it off-device. First boot should reject foreign payloads read-only and offer explicit initialization, not automatically erase/migrate. Family marker checks must cover settings and user presets as well as projects; existing preset conversion-at-boot paths are a historical fault location. Preserve valid hardware calibration through a narrowly tested import, not wholesale foreign settings adoption.

Why not graft ChoralRoot storage wholesale: it repurposes the four project sector pairs for VA/FM6/CZ patch stores, sample space for CZ banks/session and loops; [its storage map](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/firmware/src/storage.c#L20-L38) is incompatible with Sloop's. Felucca uses object 7 for a retired FM6 bank where Sloop stores autosave and object 9 autosave at 0xE5000 where Sloop has its FM6 bank. Treat each whole storage layout as a unit.

## Build plan and verified prerequisites

Machine inspection: Linux x86_64; Python 3.12.14; cc GCC 14.2; Node v24.19; Pillow and fontTools import successfully. `emcc` is not in PATH and no JieLi clang was found in surveyed workspace/PATH. No toolchains installed in this research.

Sloop native build requires JieLi pi32v2 clang 4.0.1 toolchain (`pi32v2/bin/clang`, `pi32v2/bin/ld`, `common/bin/objcopy`, `common/bin/objdump`), Python/Pillow and three AC79 SDK blobs from `AC79NN_SDK_V1.2.1_2023-12-13`. No Docker needed on this cloud x86_64 Linux. [Build instructions](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/BUILDING.md#L19-L50).

**Verified locally:** ChoralRoot's vendored SDK subset at `tools/sdk/cpu/wl82/tools/` matches **both Sloop and Felucca** exact `SDK_SHA256` tables:

| file | SHA256 |
|---|---|
| uboot.boot | 4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4 |
| cfg_tool.bin | 276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b |
| cfg/eq_cfg_hw.bin | 41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d |

This is a usable pinned SDK source without relying on a fresh Gitee fetch. Preserve the included Apache-2.0 license. Source/provenance: [SDK README](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/tools/sdk/README.md#L1-L15). Important: upstream `build.py` **warns and continues** on checksum mismatch ([Sloop lines 315–318](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build.py#L315-L318)). JamBox build wrapper must fail its preflight if the hashes differ; do not describe the existing builder as fail-closed.

Upstream [`get_toolchain.sh`](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/get_toolchain.sh#L4-L12) streams HTTPS `https://pkgman.jieliapp.com/s/linux-toolchain` into `tar`. It does not pin archive version or verify an archive checksum. For our reusable environment, download from that authoritative HTTPS endpoint with TLS verification, retain the artifact and resolved version, obtain upstream digest/signature if available, and record an artifact digest in a checked manifest. A locally calculated hash proves subsequent reproducibility, not independent authenticity; don't present it as vendor authentication. Never disable TLS or substitute arbitrary binaries.

After tools are pinned/installed, baseline commands (not run here):

```sh
# Run in the pinned Sloop source, before JamBox behavior changes.
export JIELI_TOOLCHAIN=/workspace/toolchains/jieli/toolchain
export AC79_SDK=/workspace/scratch/fm1-research/ChoralRootFM1/tools/sdk
./build.sh
tests/run_tests.sh
# Browser preview: Emscripten separately pinned in environment.
python3 tools/build_preview.py
tests/run_preview_tests.sh
```

Build output names upstream are `build/felucca.bin`, `build/loader/ota.bin`, `build/felucca.fwsc`. Use dev build for first reproduction; don't imply `--release` is branding-safe (it maps release X.Y to shared FM-1_9XY and adds BETA to INFO). Save source SHA, compiler version/artifact hash, build flags, SDK hashes, generated-asset manifest, ELF, disassembly, region sizes, package SHA and test logs.

Baseline firmware memory layout: [Sloop `app.ld`](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/app.ld#L3-L30): XIP slot 0x8DFBC; RAM .data/.bss 96 KiB; pool 0x54000 = 336 KiB; RAM code 0x6000; stacks/reserved/noinit fixed. [Builder checks](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build.py#L245-L253) enforce RAM overflow and at least 8 KiB pool spare. Collect actual sizes before picking more engines/effects. Do not copy latest Felucca's different 72/360 KiB boundary without measuring.

## Firmware identity and installer

Web page is a static host serving a prebuilt .fwsc; browser requests Web MIDI with SysEx permission in HTTPS/localhost, queries identity and read-only INFO, then answers device package-read requests. In step 1 current app stages **the incoming package's loader**; in step 2 that loader requests and installs the image. Protocol is device-pulled `0x30` read requests, max 512 data bytes, 7-bit SysEx packing ([ChoralRoot `fm1ota.js` lines 4–12](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/web/fm1ota.js#L4-L12)). File extension is `.fwsc`, not a raw app `.bin`.

Keep USB hardware model `FM-1` and required OTA transport format. Brand `INFO` as `JAMBOXFM <semver>` with explicit protocol/schema capabilities; update firmware, editor, backup importer, package metadata and installer classification together. A new arbitrary numeric FM-1_9xx does not guarantee uniqueness. ChoralRoot FM-1_920 already collides with Sloop 2.0; [classification source](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/web/fm1ota.js#L68-L93) uses family INFO text. Do not simply replace `FM-1` with a new model string: installers validate model and package identity. Do not rename loader compatibility marker `FELUCCA-LOADER-1` without a migration design, because existing apps use it to accept incoming loaders ([Felucca `ota.c` lines 269–282](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/ota.c#L269-L282)).

Installer manifest should pin package SHA256, version, family, flash layout/schema and source commit. Verify package structure, chip identity and CRCs before presenting Install; distinguish connected, backup completed, staging, writing, rebooting, verified. After reboot require JamBox INFO family/version, not just matching FM-1_9xx numeric identity. Treat disconnection as uncertain; do not display success before read-back/reboot identity.

## Recovery and transition facts; no universal safe route exists

Sloop's source explicitly calls its rescue **application-level recovery, not a second firmware bank**, requiring intact startup/RAM flash driver/USB stack: [`recovery.c` lines 2–4](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/recovery.c#L2-L4). It skips audio, settings and projects and can service updater without reading foreign data, but cannot rescue a half-written app that never reaches recovery.

Retain Sloop baseline gestures at first:

- OCT− alone at power-on: ~50 ms recovery-key scan, USB rescue. [`recovery.c` lines 41–69](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/recovery.c#L41-L69), [`bootguard.h` lines 27–29](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/bootguard.h#L27-L29).
- OCT− + OCT+ at power-on: calibration.
- OCT− + OCT+ for five seconds **while stopped**: countdown from 2 s then ROM UBOOT; no update gesture during playback. [`main.c` lines 200–227](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/main.c#L200-L227).
- Bootguard: repeated warm-reset failures → application recovery; failure in recovery → ROM UBOOT. Power-on clears guard if reset bits support that classification. [`main.c` lines 284–319](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/main.c#L284-L319).

ChoralRoot reports actual Sloop→ChoralRoot and Felucca-beta→newer failures even when byte write is correct. It refuses these transitions and recommends official V15 first. However, [INSTALL-COMPAT lines 85–98](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/INSTALL-COMPAT.md#L85-L98) says the cause is unknown and whether V15 changes anything is open; stock VM range does not include 0xFC000..0xFEFFF settings. **Do not promise that returning to V15 sanitizes all old data or makes JamBox installation safe.** User's exact installed Sloop version and exported backup remain required before any actual transition test. That does not block this planning/browser work.

ChoralRoot adds an independent first-sector boot stub and commit marker to detect incomplete app bodies and enter staged loader at cold boot. That is stronger than Sloop app rescue, but changes linker layout, loader, package, boot, cleanup and recovery as a coordinated set. Do not cherry-pick only the gesture/UI. Its model still has 16 unprotected cut points when initially installing/changing the stub; [BOOT-SAFETY lines 381–403](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/BOOT-SAFETY.md#L381-L403). Consider porting it as a separate hardware-safety milestone, with own family identity and interruption tests, before distribution; do not claim inherited protection from merely reusing ChoralRoot's installer.

A totally dark device may require **FM-1-transporter** or compatible unbricker route. Transporter needs additional Seeed XIAO RP2040 hardware on USB D+, D− and ground, not VBUS; can dump all 1 MiB and verified-write only app sectors. [README lines 3–33](https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/README.md#L3-L33), [lines 84–90](https://github.com/kurogedelic/FM-1-transporter/blob/a632d923203170e3a08565f4056dbb3fd3d65ca5/README.md#L84-L90). No physical recovery route is established for this owner's unit yet.

## Concrete bring-up gates

1. **Untouched baseline:** pin source, tools/SDK; build firmware + web preview; run upstream tests without updating golden outputs; save region sizes/package digest and host audio renders. Any baseline failures get recorded before JamBox changes.
2. **Input/UI only:** map roles and Assist menu in browser, test all 27 note keys and 14 buttons, encoder detents, hold-release consumption, role changes while keys down, disabled Assist persistence, keyboard input vs blocked modal entry, no recovery gesture regression.
3. **Jam logic/sounds:** baseline audio + one curated palette and event pipeline. Hardware deadline/memory evidence must come from build and later physical profiling; browser smoothness is insufficient.
4. **Storage schema:** serializer size ≤3840, reject wrong magic/family/unknown versions, round-trip Assist flags, checksum corruption, A/B torn writes at every simulated erase/program point, no old-foreign-data writes at boot, stop-to-flush behavior, backup dependencies.
5. **Update host simulation:** stock/Sloop/JamBox identities and unknown families, package corruption, incoming-loader consistency, malformed requests, disconnect/resume, postboot INFO verification. Baseline Sloop version is a candidate transition, not automatically whitelisted because ancestry matches.
6. **Prepared physical test:** exact firmware/hardware identified; backup verified; charged unit and stable cable; known restore image and feasible recovery tool; explicitly selected test package. First prove baseline build boots and can reinstall/recover, then small UI/audio changes, then four-part stress, save/power-cycle, USB and overload handling. Do not perform intentional power-cut stress on the user's only device.
7. **Release gate:** precise tested transition matrix, installation/recovery guide, corresponding source and licenses with package hashes, static installer/emu, no claims of guaranteed recovery. First-stub-install vulnerability documented if stub adopted.

## Checks actually run during this research

Compiled and ran **Felucca** upstream `tests/input_test.c` and `tests/storage_test.c` with host GCC, placing binaries/logs outside checkout at `deep/device-checks/`. Input test printed `input debounce: all ok`, 136 output lines, zero FAIL. Storage test printed `storage test passed`, 36 output lines, zero FAIL. GCC emitted sequence-point warnings from the input host-register macro; these are host test harness warnings, not a successful target hardware validation. Do not report this as Sloop/JamBox tests or the full firmware suite.

Compared three SDK files against both project build pins, all match. No download, firmware build, Wasm build, hardware test or flash was performed in this subtask. Read-only source checkouts left unchanged.

## Follow-up: exact retained-RAM cost of JAM1 extension

A proposed 24-byte extension to each 3816-byte project makes each slot 3840 and four slots 15360 bytes. The Sloop NOINIT region is `0x3D50 = 15696` bytes. A search of all firmware `.noinit` declarations finds:

| Object | Raw bytes |
|---|---:|
| Four proposed project slots | 15360 |
| panel_t (magic + 14 button IDs + 7 encoder IDs + 7 directions) | 32 |
| settings (4 uint32) | 16 |
| bootguard_t (3 uint32) | 12 |
| felucca_dbg (19 uint32) | 76 |
| fm1_crash_t (16 uint32) | 64 |
| Total before linker alignment | 15560 |
| Remaining before linker alignment | 136 |

Existing [`tests/project_test.c` lines 134–135](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tests/project_test.c#L134-L135) reserves 256 bytes for ancillary retained state; proposed four 3840-byte projects satisfy that inequality with 80 bytes spare. This makes JAM1 extension plausible, not validated: inspect final target section sizes/alignment and require explicit NOINIT bound checks. Put new live Assist state, gesture ownership and undo history in normal RAM/pool; do not add more retained globals casually. References: [`audio.c` lines 14–21](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/audio.c#L14-L21), [`fm1_irq.h` lines 41–50](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/hal/fm1_irq.h#L41-L50), [`panel.c` lines 18–24](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/panel.c#L18-L24), [`core.h` lines 358–362](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/core.h#L358-L362).
