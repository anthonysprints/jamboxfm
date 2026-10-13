# JamBoxFM audio architecture and baseline research

Inspected 2026-10-13. This is an implementation proposal supported by source inspection and targeted native tests; it is not a device-performance measurement or implemented JamBoxFM firmware.

## Recommendation

Use **Sloop as the primary firmware baseline**, pinned to `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`. Use current Felucca (`1727537624390cc3f9c7dff2456b43b3a8edb005`) as a source of individually reviewed fixes, emulator techniques, and tests. Do not merge its entire current application into Sloop.

The important reason is the instrument's audio structure, not its existing interface or factory sounds. JamBoxFM wants a bass, playable chords, a lead and drums at the same time. Sloop has three synth parts sharing eight voices **plus six independent drum voices**. Current Felucca has four general-purpose parts sharing eight voices **including its DRUM engine**. Three drum hits + a four-note chord + bass + lead require nine sounding voices; Felucca's current shared allocator must steal at least one. Sloop's existing architecture has space for this combination without expanding the voice pool.

The source-backed comparison:

| Concern | Sloop inspected revision | Felucca inspected revision |
|---|---|---|
| Musical parts | Three synth parts + a dedicated drum track | Four general-purpose synth parts; DRUM is an engine on a part |
| Sounding voices | Eight synth voices shared across the three parts; six separate drum voices | Eight shared across all parts, including drums |
| Drum lanes | 16 lanes, six simultaneous voices, sample/synth/user kits | Eight lanes, shared voice allocator, synthesized DRUM engine |
| Bass and lead under pressure | MONO/LEGATO lead protected; non-root poly notes are eligible for stealing | Similar policy, but drums compete in the same budget |
| I2S half-buffer | 256 frames, about 5.8 ms at nominal 44.1 kHz | 128 frames, about 2.9 ms |
| Reusable advantages | Dedicated drum path, chords/strum/voice leading, groove recording, user-kit infrastructure, existing browser DSP adapter | Full emulator, larger host test coverage, current USB/audio fixes, explicit RAM headroom guard, newer display infrastructure |
| Main integration cost | Replace the workflow and curated sounds; add Assist semantics and a complete control/display emulator | Would need either a more constrained music design or an additional independent drum architecture to meet our simultaneous-voice target |

Sources: [Sloop core types](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/core.h#L3-L16), [Sloop drums](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/drums.c#L3-L9), [Felucca core](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/core.h#L3-L15), [Felucca DRUM engine interface](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/eng_drum.c#L330-L349).

Keeping Sloop's code foundation does not require keeping its UI, naming, factory presets, or no-patterns-at-start design. The owner's objections concern those experiences. Existing Sloop project compatibility and safe firmware transitions are separate decisions; sharing a baseline does not prove either.

## Proposed first sound budget

| Role | Held notes | First implementation |
|---|---:|---|
| Bass | 1 | ANALOG, mono or legato; sine/triangle or restrained filtered saw. Keep its low end dry. |
| Chords | Up to 4 | DIGITAL four-operator FM for an electric-piano-like Dusty patch; ANALOG for later crunchy or bright chord patches. Short/moderate releases. |
| Lead | 1 | ANALOG, mono or legato; mellow triangle/PWM with a little modulation and delay. |
| Drums | Up to 6 simultaneous hits/tails | Retain the independent drum renderer. Expose a small kit at first, while preserving the existing 16-lane representation. |

This is **six held synth voices of the existing shared eight**, with two remaining slots available for release overlap. These are proposed role limits, not existing reservations. Long chord release tails or multiple held chord triggers can still consume the shared capacity. New chord triggers should release the prior chord group's generated notes, and the allocator should take old releases before current held chord tones. Retrigger and crossfade behavior needs to be measured and heard. Chord-mode OFF can allow manual poly playing within the same role cap.

Keep `NVOICE=8`, `NPART=3`, `NTRK=4`, `TRK_DRUM=3` and `NDRUM=6` initially. The product can display roles in its own order without reindexing these protocol/storage identities. A full six-drum-voice burst plus the chosen synth voices is a stress target, not a claim of spare CPU.

Avoid UNISON in the initial curated patches: one key can otherwise request many voices. Avoid PHYS/GRAIN as the chord engine: GRAIN is capped at three voices, PHYS at two for SYMP or three for the other models. FM6 allows six per part but still shares the synth pool; use it only after profiling. These engine limits are software decisions in these revisions, not hard FM-1 hardware counts.

Relevant seams:

- `voice.c:trk_nvoice()` (69–73): combine the existing engine cap with the proposed part's role cap; define whether the role cap counts held notes or all active/releasing notes, and test it explicitly.
- `voice.c:voice_victim()` (96–125), `voice_room()` (178–188): retain released-first stealing and bass/mono-lead protection; keep graceful fades.
- `voice.c:voice_kill()` (130–137): current Sloop code uses `KILL_BLOCKS=2`, despite the older introductory comment saying one block. Budget transient fading voices as well as `voices_busy()`, which excludes stage-4 voices.
- `drums.c:drum_synth_on()` (182–210), `drum_on()` (229 onward): six independent one-shot voices. Free slot or oldest voice; closed/pedal hat chokes open hat; a cut voice contributes its last output to a decaying tail. Six voices does not mean six lanes total.
- `engines.c:ENGINES[]` (64–72): stable engine indexes must remain stable in existing wire/storage representations.
- `engines.c:eng_arena_of()` (42–59): engine state is cleared on an ownership change. Do not add per-engine large arrays outside this mechanism without a memory audit.

Permalinks: [allocator](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/voice.c#L68-L188), [drum allocation](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/drums.c#L182-L210), [engine arena](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/engines.c#L32-L72), [PHYS limits](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/eng_phys.c#L79-L105), [FM6 limit](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/eng_fm6.c#L24-L34), [GRAIN limit](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/eng_grain.c#L19-L39).

## Sound identity without a new DSP engine first

The first palette should be custom-tuned Dusty: a short punchy kick, rounded snare, closed/open hats, muted percussion, warm mono bass, mellow FM keys, and a simple lead. The existing synthesis methods can do this; patch names alone do not demonstrate musical quality.

Sloop's DIGITAL engine already implements four sine operators, eight algorithms, ratio/index/modulator-decay/feedback controls, and electric-piano-style patches. See [digital_render and DIGITAL_PRESETS](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/eng_digital.c#L3-L155). Start from a reviewed patch, retune its envelopes/velocity/sends by ear, then level-match it in the full mix. Do not claim realistic piano or artist likeness.

ANALOG exposes wave/detune/mix/noise/cutoff/resonance/drive/keytracking; its waveform control and envelope parameters provide useful deeper editing immediately: [engine and presets](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/eng_analog.c#L125-L175). A parameter-mapping layer can offer Brightness/Movement/Grit/Space while writing these same parameters. It should use bounded ranges and smoothing, with full engine controls reachable underneath.

Exact Sloop engine identities at this pin: `0 ANALOG`, `1 DIGITAL` (four-op FM), `2 PHASE`, `3 LOFI`, `4 SAMPLE`, `5 FORMANT` (displayed VOICE), `6 TRIO`, `7 DRAWBAR` (displayed WHEEL), `8 GRAIN`, `9 FM6`, `10 PHYS`, `11 NOISE`. Do not copy numeric engine IDs from Felucca: they differ.

Proposed initial macro selectors (ranges need listening tests, render-health checks and target profiling):

| Macro | ANALOG source parameter | DIGITAL keys source parameter |
|---|---|---|
| Brightness | `P_E4` cutoff | `P_E4` FM modulation index |
| Movement | Small `P_LD_FLT` depth; restrained `P_LRATE`; PWM patch may instead map `P_LD_SHP` | Gentle `P_LD_AMP` tremolo with fixed musical LFO rate |
| Grit | `P_DIST`; avoid stacking a new insert on top initially | `P_DIST` at a more restrained range |
| Space | Curated `P_DLY` / `P_REV` send curve; near zero on bass | Curated `P_DLY` / `P_REV`; shared chorus can be part of the base patch |

The complete ANALOG edit selectors are `P_E0` wave, `E1` detune, `E2` oscillator mix, `E3` noise, `E4` cutoff, `E5` resonance, `E6` engine drive, `E7` keytracking. DIGITAL uses `E0` algorithm, `E1/E2/E3` ratios, `E4` index, `E5` modulator decay and `E6` feedback. These labels are concrete reuse seams; they are not promises that all engines expose the same oscillator/filter controls. Starting with ANALOG for all three melodic roles is an even smaller first slice; add the custom DIGITAL key patch after its four-note render test, then profile it on hardware.

Sloop's drum synth describes a tone/pitch envelope, click, noise, filter, drive and level per sound. Its generator includes a BOOMBAP kit, but this is reference material for auditioning, not a decision to inherit Sloop's sounds unchanged: [drum synth](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/drum_synth.c#L3-L41), [kit source](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/gen_drumkits.py#L201-L237).

Garage and Neon can reuse these engines with new patch/envelope/effect mappings after Dusty plays well. Guitar-like rhythmic synth stabs are feasible; realistic guitar synthesis remains outside the MVP. User samples can come later through Sloop's existing slot system, after license and flash-layout work. No audio sampled from the named artists is needed.

## Effect routing and budget

Keep one shared chorus, one shared tempo delay and one shared reverb bus, with per-part sends. Keep bass predominantly dry. A part's Grit macro can drive its existing DIST or one selected INSERT, rather than stacking multiple new processors. Initially use a single cheap saturating/crushing insert on the chord part, or a light shared chorus for width; enable additional insert combinations only after the full-jam stress profile passes.

The current path is `events_block` → synth render → DIST → INSERT → SLICER → track filter → gain/pan/sends; independent drums are mixed through their own slicer/filter/insert path; shared buses return; master Dust/punch/filter/limiter follow. `mix_block()` runs in the audio interrupt in 32-frame control blocks. Preserve the existing click suppression, gain ramps and look-ahead limiter. Do not allocate memory or write flash in the sound-generation path.

Evidence: [mix_part_run](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L635-L713), [mix_block](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L1045-L1106), [look-ahead limiter](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L56-L81).

**Turning sends to zero is not a CPU or RAM optimization by itself.** `fx_buses()` still runs its three buses each block, and delay/chorus/reverb arrays remain allocated. If profiling requires fewer effects, implement a tested bus bypass that waits for tails to die, or a compile-time omission; do not merely hide controls and claim the cost is gone. Muted tracks still render their voices before being discarded in `mix_part_run`, so mute is not a guaranteed CPU saving either.

For the first JamBoxFM build, consider compiling out unused Sloop punch-in looping and per-track stutter while retaining compatibility stubs or sanitizing their project parameters. Their dedicated buffers alone total 96 KiB (64 KiB punch ring + 32 KiB slicer buffers). These are proposed JamBoxFM build changes; the current tree does not provide an off flag for each subsystem. `FELUCCA_SLICE=0` controls the separate SLICE sample engine, **not** `slicer.c`'s effects buffers.

Static buffer evidence: [delay/chorus/reverb](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L3-L16), [punch size](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/punch.c#L9-L26), [slicer size](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/slicer.c#L22-L49), [bus execution](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/fx.c#L198-L240).

## Memory: capacity, partitions, and actual use are different

- Both linker layouts describe the same approximately 512 KiB address span beginning at `0x01C00000`, including reserved state, stacks and RAM-executed code. This is linker evidence about usable address layout, not an independently verified chip datasheet specification.
- Sloop's ordinary `.data/.bss` allocation is 96 KiB and its `.pool` allocation is 336 KiB. Current Felucca allocates 72 KiB and 360 KiB respectively. Both sum to 432 KiB; the different split does not give Felucca another 24 KiB of physical RAM.
- Both application linkers assign `XIP` a length of `0x8DFBC` (581,564 bytes). That is the app slot budget, not free flash or total storage. `tools/fm1pkg_make.py:FLASH_SIZE=0x93000` describes the package's flash image region, not the full physical NOR capacity.
- Exact scalar array costs: Sloop's mono delay is 65,536 × 2 = 131,072 bytes; chorus is 2,048 × 2 = 4,096 bytes; punch ring is 32,768 × 2 = 65,536 bytes; four slicers are 4 × 4,096 × 2 = 32,768 bytes. These static allocations remain when the corresponding UI control is OFF.
- Current Felucca allocates `ENG_MEM_PART=12888` per part, four parts, in its pool. Sloop uses a union of GRAIN/FM6/PHYS state for each of three parts. Sharing prevents all three large states from being allocated simultaneously per part; it does not make new engines free in code size or CPU.
- **Actual target `.data/.bss`, pool usage, app image length, stack high-water and CPU headroom are not measured in this research.** No target ELF/map/disassembly was built. Native executable memory layout and browser WASM memory size are not substitutes.

Source: [Sloop linker](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/app.ld#L3-L31), [Felucca linker](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/app.ld#L3-L33), [Sloop package constants](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/fm1pkg_make.py#L24-L31), [Felucca engine region](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/engines.c#L7-L22).

Sloop's build currently rejects ordinary-RAM overflow and requires at least 8 KiB pool headroom. Current Felucca additionally requires 4 KiB ordinary-RAM headroom. Port the stronger guard to JamBoxFM and record the first target build's measured numbers. Proposed initial acceptance: retain at least 4 KiB ordinary-RAM and 8 KiB pool headroom after the full build; target materially more after removing unused effect buffers. Verify real stack guards under stress. These are guardrails, not claims of current free space.

A net-new ordinary-RAM feature budget of at most 4 KiB for Assist/note ownership/commands is a sensible provisional cap. It does not establish that the current build has 4 KiB free; both the net growth cap and remaining-headroom check must pass. Reuse existing undo/snapshot facilities where possible rather than allocating another full song copy.

Evidence: [Sloop build checks](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tools/build.py#L241-L253), [Felucca build checks](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/tools/build.py#L277-L287).

## CPU and overload handling

Sloop renders a 256-frame half-buffer per interrupt: nominal deadline about 5.8 ms. The code measures total elapsed timer ticks, including nested TIMER5 interrupts, for overload decisions, while the displayed smoothed render CPU excludes nested ticks. It requests shedding after **two consecutive halves above 85%**. Shedding fades a releasing voice first, otherwise a non-root chord/unison voice; it preserves a part's mono lead or lowest poly note. The six dedicated drum voices are not in this synth-shedding loop.

Use these existing diagnostics in a development console: `audio_max_us`, `audio_halves`, `felucca_dbg.late`, `voice_kills`, `shed_count`, and the full elapsed interrupt duration. Do not evaluate safety from the smoothed screen percentage alone. Deliberate dense voice-budget stealing must be distinguished from CPU-overload shedding. Fading voices excluded from `voices_busy()` can still briefly consume render time.

Proposed device gate for the first palette: a 10-minute worst-case jam (rapid four-note chord changes, held bass/lead, six simultaneous drum hits, maximum exposed FX, repeated knob movement, screen redraw and USB audio) with zero late DMA halves and zero overload-triggered shedding. Initial target: total interrupt duration p99 below 70% of its half-buffer deadline, maximum below 85%; collect total time including nested interrupts rather than deriving percentiles from the smoothed screen CPU. A small fixed histogram can record p99 without unbounded logging in the audio path. These are initial engineering targets to validate, not measurements.

If the target fails, first remove the unused punch/slicer processing and shorten overly long patch releases, then replace expensive curated engines or insert combinations. Reduce the default chord voicing to a triad only as an explicit fallback, with rich four-note chords kept for a lighter patch. Lowering audio rate or raising `NVOICE` is not the first remedy. The initial device build should retain Sloop's buffer size; investigate Felucca's lower-latency 128-frame setup only after a complete profile, since a shorter buffer tightens deadlines and changes interrupt frequency.

Sources: [audio ISR / diagnostics](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/audio.c#L10-L95), [shed_voice](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/voice.c#L128-L174), [Felucca revised meter and overload implementation](https://github.com/hugelton/Felucca/blob/1727537624390cc3f9c7dff2456b43b3a8edb005/firmware/src/audio.c#L45-L140).

## Native validation actually run

Generated headers from each unmodified checkout into `/workspace/scratch/fm1-research/deep/audio-tests/{Felucca,sloop}/gen/`. Compiled selected existing tests with host `cc` and executed them. Nine targeted test programs/scenarios passed:

| Repository | Test | Result |
|---|---|---|
| Sloop | `seq2_test.c` | Passed; sequencing, drum grid, swing, chord entry, strum cancellation, voice-leading and stop behavior among its checks |
| Sloop | `scale_test.c` | Passed; 16 scales × 12 roots, transposition, bypass, silent keys, held-note changes and recording |
| Sloop | `drumkit_test.c` | Passed; 32 synthesized kits × 16 sounds bounded, audible, finite and level checks |
| Sloop | `arena_test.c` with `-DARENA_STATS` | Passed; 68 engine-state transition pairs; 300 seconds simulated random activity, 799 engine switches; separate 120-second PHYS model-flip run; no hanging notes, ending silence, no wrong-arena access |
| Sloop | `hostsim.c` with `TRACKS=...` | Passed; four-part mix, recording, MIDI routing, release lengths, swing recording, voice-budget stealing, engine-switch fades and relative host timings |
| Felucca | `audio_test.c` | Passed; single vs repeated overload, DMA late count, nested IRQ cost and CPU smoothing |
| Felucca | `drum_test.c` | Passed; drum controls, lane levels, eight-lane engine allocation, retrigger/choke, one-shots and audio checks |
| Felucca | `swing_test.c` | Passed; swing pair timing, bounded global+track addition, sequencer/slicer agreement |
| Felucca | `chord_test.c` | Passed; chord construction, releases, recording, arp, MIDI and drum bypass |

All logs are in the same `audio-tests/<repo>/` directories; renders under `audio-tests/sloop/tracks/` are upstream reference demos, not a JamBoxFM prototype. The Sloop hostsim scenario reported at most six synth voices, no voice stealing in the ordinary demo, zero near-full-scale samples; its explicit stealing scenario exercised five steals. Its CPU figures are **host nanoseconds per sample**, not FM-1 utilization.

Initial direct compile attempts needed two local prerequisite corrections: Felucca's chord test includes real UI code, requiring generated UI headers; Sloop's arena test requires its documented `-DARENA_STATS`. Both were rerun successfully after those corrections. No reference source file was changed.

Reproduction outline (outputs outside source checkout):

1. Run each repository's `gen_tables.py`, `gen_samples.py`, `gen_fm6_patches.py` with an explicit output header path. For Sloop also run `gen_drumkits.py`; for Felucca's chord/UI test additionally generate `ui_fonts.h` (`gen_aa_font.py --preset inter-tight`), `ui_icons.h`, `ui_keycaps.h`, and `ui_palettes.h`.
2. Compile with `cc -O2 -w -I<generated-dir> -I<repo>/firmware/src -I<repo>/firmware/hal -o <scratch-output> <repo>/tests/<test>.c -lm`; use `-DARENA_STATS` for `arena_test.c`. The chord and arena rechecks used `-O1`.
3. Execute from the respective repository directory. For `drumkit_test`, supply scratch WAV and report paths. For `hostsim`, set `TRACKS` to an existing scratch directory and pass `0 0 1 <scratch-wav>`.

The whole `tests/run_tests.sh` was not run: it requires target build outputs/SDK, and Sloop's runner explicitly checks for `build/felucca.fwsc`. No device toolchain was installed, no target firmware built, no golden files updated, no CPU-baseline hashes replaced, and no hardware flashed. Emscripten and Valgrind were not present for this subtask; browser equivalence and instruction-count profiling remain separate work.

## Required JamBoxFM-specific tests after implementation

- Play 4-note chords repeatedly over bass + live lead + all six drum voices. Assert bounded active/fading voices, bass/lead continuity, graceful release stealing and no stuck note after stop.
- Keep output samples bounded, no excessive DC or unintended clipping, silence after final release/FX decay, and no click on patch changes, part mutes, macro extremes or engine transitions.
- Held physical/MIDI notes must release the notes they originally generated even if Assist, scale/root, selected part, or preset changes mid-hold. The master Assist toggle must preserve individual options and recorded event bytes.
- Record/copy/undo phrases; assert turning assistance or swing on/off does not rewrite existing events. Test the newly designed gentle/tight quantization separately; upstream sequencing tests do not implement that product promise for us.
- Render the same deterministic event stream with native C and browser WASM and compare output/event timing. Test the actual JamBoxFM roles/patches and bindings, not only upstream engines.
- Repeat target build memory checks and disassembly cost checks; hardware tests include flash activity, USB/audio clocks, input latency, debug stack guards and CPU-overload counters. Host tests cannot certify these.

