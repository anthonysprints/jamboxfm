# FM-1 implementation research and evidence

Research date: October 13, 2026. This pass supports the decisions in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md). It does not claim a JamBox firmware build or hardware flash.

## Conclusions that changed the implementation plan

1. **Use pinned Sloop as the first engine baseline.** Its eight synth voices and six separate drum voices suit bass + four-note chords + lead + drums. Felucca's current eight voices include its drum engine.
2. **Build new sound curation and interaction on those foundations.** Existing engines offer practical analog/FM and drum starting points; copying Sloop's interface and preset selection would miss the user's goal.
3. **Recording assistance is new work.** Sloop has swing and manually editable microtiming, but its recorder snaps to steps without storing a played timing residual. Off/Gentle/Tight needs a new capture path.
4. **Save expanded pitches.** Sloop's existing absolute-note steps support the promise that Assist changes future input. ChoralRoot's semantic chord looper is a different model that can revoice recorded harmony.
5. **The save extension fits only with tight checks.** Sloop's 3,816-byte project plus 24 bytes of JamBox metadata reaches the 3,840-byte object maximum. Retained RAM is also nearly full; target layout remains unbuilt.
6. **Browser DSP runs, but is not a device test.** The checked-in Sloop WASM rendered and stopped the intended roles in a Node probe. Target CPU, physical controls, USB updates and flash recovery remain separate gates.

## Method and source boundary

Used the installed REA skill. Its guidance routes complete source-repository analysis to ordinary repository tools; those were used for C, linker, storage and build inspection. REA itself analyzed the checked-in **browser distribution** under Sloop's `docs/webapp`, which includes published-style installer/editor assets and the compiled preview WASM.

All source checkouts stayed unchanged. Targeted upstream host tests generated headers and outputs under `/workspace/scratch/fm1-research/deep/`. The repository now retains the findings and small evidence/log files under [research/](research/), independent of those scratch checkouts.

The user-supplied hosted demo sites and baudgirl.com were blocked by the environment's egress proxy during the preceding pass. This pass inspected their available GitHub sources/artifacts. No claim is made that the checked-in assets equal the current live deployment. Baud Girl's implementation/license remains unknown; ChoralRoot documents its FM-1+VA manager as a UI reference.

## Pinned sources

| Project | Commit inspected | Role |
| --- | --- | --- |
| [Sloop](https://github.com/isod89/sloop-fm1/tree/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e) | `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e` | Selected initial baseline |
| [Felucca](https://github.com/hugelton/Felucca/tree/1727537624390cc3f9c7dff2456b43b3a8edb005) | `1727537624390cc3f9c7dff2456b43b3a8edb005` | Ancestor comparison and tests |
| [Ghoulbox](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth/tree/2f001b1f550eac695d8400f9f388c8eac6dbb4c6) | `2f001b1f550eac695d8400f9f388c8eac6dbb4c6` | Instrument identity and full UI simulator pattern |
| [ChoralRoot](https://github.com/Quixotic7/ChoralRootFM1/tree/aa5875af20e3046ee161f988c5d91d3ae66d0c74) | `aa5875af20e3046ee161f988c5d91d3ae66d0c74` | Chord interaction, SDK subset, installer/recovery comparison |
| [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter/tree/a632d923203170e3a08565f4056dbb3fd3d65ca5) | `a632d923203170e3a08565f4056dbb3fd3d65ca5` | Recovery requirements, not an exercised recovery route |

## Source map for the implementation decisions

All links below identify immutable commits. Detailed function traces and additional citations are retained in [audio notes](research/audio-notes.md), [Assist notes](research/assist-notes.md), and [device/build notes](research/device-notes.md).

| ID | Evidence | Consequence |
| --- | --- | --- |
| S1 | Sloop [core.h](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/core.h#L3-L16), [drums.c](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/drums.c#L3-L39) | Three synth parts, eight shared synth voices, separate six drum voices, 256-frame device halves |
| F1 | Felucca [core.h](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/core.h#L3-L15) | Its four general tracks share eight voices including DRUM |
| S2 | Sloop [main.c IRQ work](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/main.c#L3-L65), [mix_block](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L1045-L1106) | Keep interrupt priorities and sample-driven musical events |
| S3 | Sloop [voice allocator](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/voice.c#L68-L188), [audio measurement](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/audio.c#L10-L95) | Role policy is new; fading voices cost CPU; total ISR time matters |
| S4 | Sloop [linker map](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/app.ld#L3-L31), [build checks](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build.py#L241-L253) | Preserve partitions; actual remaining capacity requires target build |
| S5 | Sloop [chord/scale helpers](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L194-L281), [local note mapping/release](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L1543-L1623) | Reuse tonal formulas; retain original part/pitches until release |
| S6 | Sloop [step schemas](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/core.h#L181-L205), [undo/recording](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L443-L598) | Four notes per step; add timing capture and complete undo snapshot |
| S7 | Sloop [swing grid](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L393-L439), [microtiming playback](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L2026-L2122) | Capture against the same swung timebase and offset normalization |
| S8 | Sloop [FX buffers](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L3-L16), [punch buffer](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/punch.c#L9-L26), [slicer buffers](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/slicer.c#L22-L49) | Hiding effects does not release allocations; removing unused processors needs code changes |
| S9 | Sloop [MIDI ingress](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/midi_uart.c#L48-L72), [arp path](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/seq.c#L1120-L1180) | Preserve port identity and avoid held-arp remapping through Assist changes |
| H1 | Sloop [panel mapping](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/panel.c#L8-L30), Felucca [input API](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/hal/fm1_input.h#L144-L180) | 27 notes, 14 buttons, seven encoders; no assumed knob clicks or touchscreen |
| P1 | Sloop [project layouts](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/project.c#L9-L80), [A/B flash objects](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/storage.c#L3-L62) | 3816 + 24 bytes reaches the object limit; retain commit-last saves |
| P2 | Sloop [retained-state test allowance](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tests/project_test.c#L134-L135), [retained globals audit](research/device-notes.md#follow-up-exact-retained-ram-cost-of-jam1-extension) | NOINIT remains tight: 80 bytes spare against the conservative allowance |
| W1 | Sloop [C preview adapter](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/host/preview.c), [audio owner](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/web/preview-worklet.js#L3-L105), [44.1 kHz requirement](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/web/preview.js#L14-L39) | Share C behavior and preserve explicit browser-audio startup/rate checks |
| B1 | Sloop [device build requirements](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/BUILDING.md#L19-L50), [browser build](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build_preview.py) | JieLi pi32v2 device compiler and Emscripten are separate prerequisites |
| B2 | ChoralRoot [SDK provenance](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/tools/sdk/README.md), Sloop [warn-only mismatch behavior](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build.py#L315-L318) | Locally verified SDK bytes; add fail-closed build preflight |
| U1 | ChoralRoot [firmware classifier](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/web/fm1ota.js#L68-L100) | Numeric identity collides; inspect family/version and supported transitions |
| U2 | ChoralRoot [unresolved transition diagnosis](https://github.com/Quixotic7/ChoralRootFM1/blob/aa5875af20e3046ee161f988c5d91d3ae66d0c74/docs/INSTALL-COMPAT.md#L85-L98), Sloop [application-only rescue](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/recovery.c#L2-L4) | No blanket claim that a V15 detour or rescue key makes experimental flashing safe |
| U3 | Sloop [Web MIDI protocol](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/web/fm1ota.js#L4-L16), [staging](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/ota.c#L3-L26), [persistent-loader design](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/loader/ldr_core.c#L3-L22) | Static installer serves device requests; the staged loader's resume mechanism differs from app rescue |

## REA analysis: checked-in browser distribution

- Tool: `rea-agents@6.4.0 analyze-javascript-application`.
- Artifact: pinned Sloop's `docs/webapp` directory.
- Root artifact SHA-256: `4aac7f1eda36f3f11e002a26e1b91c37993783ee4604dd59eea880e787c1bea8`.
- Parent evidence: `ev_f60869e939549edde733061e724bd59c94b780df89c6abd28f815d3380e8a049`.
- Summary evidence: `ev_dcde9d43613809cae240f92eb707b36857d391974e4ccf14d06071dcc8cfea8d`.
- Module-page evidence: `ev_16f06f42ff0e80ba5fb90a6d9633cc1f970a02acf428919dca96053c3602c1b8`.

REA reports six relevant files, three parsed JavaScript files and zero parse failures. Its module view identifies `editor/preview.js`, `editor/preview-worklet.js`, and `sw.js` in their respective module/asset roles. Application inventory coverage is reported complete, but semantic coverage remains unknown with **543 semantic unknowns**. This does not establish full installer behavior or runtime execution.

Full output: [compressed REA evidence](research/evidence/sloop-web-rea.json.gz). Readable views: [summary](research/evidence/sloop-web-summary.json), [modules](research/evidence/sloop-web-modules.json). Static source tracing and runtime probing provide the additional evidence below; they are labeled separately rather than being presented as REA's own conclusions.

## Shipped WASM probe: actually executed

Executed the checked-in WASM and its checked-in `FirmwareAudio` adapter under Node v24.19.0. Probe: [probe-sloop-wasm.mjs](research/probes/probe-sloop-wasm.mjs). Result: [sloop-wasm-probe.json](research/evidence/sloop-wasm-probe.json).

WASM length: 773,070 bytes. SHA-256: `55e2b480ebc689664918f331cbb74b94c78f10b006a06c8e5807c9975ff69ffe`. Its import list is empty; its exports include note, track, parameter, play, position, panic and render functions.

Five scenarios passed: bass, four-note chord, lead, drum kick, and combined roles. Each produced nonzero finite stereo output bounded to [-1,1]. After note releases, panic and eight seconds of tail draining, each produced zero energy in the final tested second.

This executes an upstream artifact, not a rebuilt or newly implemented JamBox engine. The combined signal does not prove every requested voice survived allocation. No AudioContext, actual browser page, FM-1 timing, USB or flash operation was exercised. No subjective claim about sound quality is made from these numerical checks.

## Targeted native host checks: actually executed

| Source | Existing test/scenario | Outcome |
| --- | --- | --- |
| Sloop | `seq2_test.c` | Passed: sequencer/chord/grid/stop checks |
| Sloop | `scale_test.c` | Passed: 16 scales × 12 roots and input behavior |
| Sloop | `drumkit_test.c` | Passed: 32 synthesized kits × 16 sounds |
| Sloop | `arena_test.c` with `ARENA_STATS` | Passed: engine transitions, 300 seconds simulated activity and PHYS model changes |
| Sloop | `hostsim.c` four-track scenario | Passed: mixing, recording, routing, release/steal cases |
| Felucca | `audio_test.c` | Passed: overload and nested-interrupt accounting logic |
| Felucca | `drum_test.c` | Passed: drum allocation/retrigger and render checks |
| Felucca | `swing_test.c` | Passed: timing/grid checks for Felucca's own swing implementation |
| Felucca | `chord_test.c` | Passed: construction, ownership, recording and input cases |
| Felucca | `input_test.c` | Passed: debounce/input host simulation |
| Felucca | `storage_test.c` | Passed: A/B storage behavior |
| ChoralRoot | `cr_loop_test.c` | 471 passed, 0 failed under the adjusted warning policy below |

Logs are retained in [research/logs/](research/logs/). Felucca tests validate that source snapshot, not Sloop or JamBox merely because the projects are related.

Native test preparation initially exposed missing generated UI headers and the arena test's required `ARENA_STATS` define; those prerequisites were supplied and the affected tests rerun. ChoralRoot's strict compile initially failed on one signedness diagnostic at `cr_loop.c:523`; adding only `-Wno-error=sign-compare` retained that warning and allowed the 471-check run. It is not a clean pass under the original strict compile flags. Felucca's input harness emitted existing sequence-point warnings.

The earlier investigation also ran ChoralRoot's Node installer simulation: **212 checks passed**, exit 0. That was a fake DOM/MIDI device, not a real update. Its [log](research/logs/choralroot-installer-test.log) is retained; it was not rerun without a new reason.

## Verified SDK subset

The three ChoralRoot-vendored files matched both Sloop and Felucca expected hash tables:

| File under `cpu/wl82/tools/` | SHA-256 |
| --- | --- |
| `uboot.boot` | `4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4` |
| `cfg_tool.bin` | `276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b` |
| `cfg/eq_cfg_hw.bin` | `41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d` |

This verifies the expected bytes for the known SDK subset. It does not install the device compiler or establish a completed device build. The toolchain's moving download still needs a recorded version/artifact pin; a hash calculated after download supports future reproducibility but is not an independent vendor signature.

## Not yet verified

- Full Sloop or JamBox device build, ELF sections, disassembly and actual remaining RAM/flash.
- Rebuilt native/WASM audio equivalence or browser UI interaction in this environment.
- JamBox-specific chord ownership, recording, UI, save format or sound palettes: these are planned work.
- FM-1 latency, total audio deadlines, battery/power-loss behavior, actual recovery and USB recording.
- Owner's precise installed Sloop version and a supported transition from it.
- Live deployed websites, Baud Girl internals, or assets whose licenses have not been reviewed.

The question of how to implement the proposed workflow is answered with source-backed seams and explicit design decisions. Device readiness and release safety are intentionally left to the listed validation gates.
