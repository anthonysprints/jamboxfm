# Dusty vertical slice, implemented 2026-10-13

Shared implementation: `/workspace/jamboxfm/firmware/src/jam_palette.c`, included after `seq.c`. Parent owns device integration; browser adapter includes it after the same upstream DSP/sequencer. No device flash, persistent format migration, new voice allocator, new Assist model, new timing model, or target CPU measurement is part of this slice.

## Session and role routing

`jam_new_dusty()` returns 1 only when transport, count-in, and first-take capture are stopped; it never starts playback. It produces 88 BPM, straight timing, 32 sixteenth-note steps per track (two bars), root C/minor, a ready drumbeat, and empty playable melodic patterns. Selected internal track 1 is CHORDS. Existing Sloop seventh-chord and WHITE scale helpers are enabled on chords; WHITE scale helper is enabled on lead. Arpeggiators and inserts are off. These are existing helpers, not the planned JamBox Assist/ownership contract.

Display order DRUMS/BASS/CHORDS/LEAD maps to internal track 3/0/1/2. Track 0 bass and track 2 lead use V_LEGATO; track 1 chords uses V_POLY. Every melodic track uses ANALOG engine 0. Synth voice sharing remains the upstream eight-voice allocator. The audition phrase reached six busy synth voices; that is an observed phrase result, not a new six-voice cap. Six dedicated drum voices remain upstream behavior.

New clears the preceding session's lane mutes, erase state, held key/MIDI/sustain/bend ownership, queued MIDI/step/program/audition events, prior voice-leading context, rolls, punch request, and musical globals including master tuning and texture settings. Physical keys still down must be lifted before they can enter the new session. Existing short sound/punch fades and effects tails are allowed to decay; large audio rings are not cleared with interrupts disabled. Device MIDI routing/channels/sync/bend range and master volume are retained. No settings or flash writes occur.

## Sound definitions

ANALOG engine parameters below are raw 0–127-style firmware parameters, not perceptually linear percentages. E0..E7 mean WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR. ENV is ATK, DEC, SUS, REL. Wave IDs used: 0 saw, 2 triangle, 4 pulse/PWM. All three roles avoid unison. Two-oscillator chord detune is modest; bass and lead have oscillator MIX 0.

| Track | Variant | E0..E7 | ENV | Filter env | Transpose | LFO rate | Move/Grit/Space % |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Bass | ROUND BASS | 2,0,0,0,78,8,8,20 | 2,66,88,20 | 12 | -24 | 28 | 0/10/0 |
| Bass | MUTED BASS | 0,0,0,0,52,14,16,38 | 1,54,52,12 | 20 | -24 | 28 | 8/20/0 |
| Chords | DUST KEYS | 2,5,32,0,68,8,4,32 | 2,74,28,42 | 18 | 0 | 32 | 15/8/28 |
| Chords | SOFT STABS | 4,5,24,0,54,6,4,32 | 0,58,36,26 | 20 | 0 | 32 | 8/12/18 |
| Lead | MELLOW LEAD | 2,0,0,0,88,12,6,48 | 4,66,104,28 | 8 | 0 | 36 | 12/8/32 |
| Lead | DUST REED | 4,0,0,0,72,10,6,48 | 6,74,100,24 | 6 | 0 | 36 | 18/10/24 |

Synth track levels are 94,86,92. LFO is free-running. Chords have fixed chorus send 12; bass and lead chorus sends are 0. Sound loading resets non-preserved common synthesis parameters to upstream defaults before applying this table. Patch definitions are deliberately independent of factory preset indexes and do not copy factory sequences.

DUST KIT selects the built-in synthesized BOOMBAP kit by name, not a generated numeric kit position. SOFT KIT selects synthesized LO-FI by name. Searches are restricted to the generated synthesized bank (`DRUM_SAMPLED..DRUM_USR`), so failure cannot silently select a sampled kit. There are no imported/user samples and no overwrites of SYN user slots. Drum variant changes preserve the drum mix/effect macro settings.

Global delay is eighth-note TIME 1, FDBK 36, COLOR 52, MIX 72. Reverb SIZE 48/DAMP 84. Chorus RATE 30/DEPTH 28. Drum LEVEL 88, REV 8, DLY 0. Master dust, duck and filter are 0; texture source is off. These are conservative initial values; no claim of target timing savings is implied by zero effect sends because upstream effect buses still render.

## Beat, zero-based step positions

| Lane | Steps and levels |
| --- | --- |
| 0 kick | 0,10,16,26 normal; 7,22 soft |
| 2 snare | 4,12,20,28 normal; 31 ghost |
| 4 closed hat | every even step 0..30; alternate normal/soft |
| 5 open hat | 15,31 soft |

Total 29 hits, ratchet 0 (one hit), no microtiming, fill flags or parameter locks. Other drum lanes are empty. Melodies are empty so the player's first recording owns them.

## Home macros

All setters clamp 0..100 and round to nearest integer `(pct * range + 50) / 100`. Getters derive percentages from the actual primary parameter, so they quantize when a parameter has fewer than 101 values. Home physical encoder steps should be at least 5% with these ranges; 1% get-plus-set can stall. Browser sliders set absolute percentages.

| Role | Macro 0 | Macro 1 | Macro 2 | Macro 3 |
| --- | --- | --- | --- | --- |
| Bass | BRIGHT: E4 40..104 | MOVE: LD_FLT 0..16 | GRIT: DIST 0..20 | SPACE: DLY 0..12 + REV 0..8 |
| Chords | BRIGHT: E4 36..104 | MOVE: LD_FLT 0..24 | GRIT: DIST 0..36 | SPACE: DLY 0..32 + REV 0..28 |
| Lead | BRIGHT: E4 48..116 | MOVE: LD_FLT 0..20 | GRIT: DIST 0..28 | SPACE: DLY 0..44 + REV 0..30 |
| Drums | TONE: TFLT -32..0 | LEVEL: G_DRLVL 0..110 | ECHO: G_DRDLY 0..24 | ROOM: G_DRREV 0..24 |

Drum macros address the actual drum path, not the melodic DIST parameter or the master dust effect. Melodic macros return 0/do nothing for non-ANALOG engines; Home UI exposes the deeper editor for those. Macro changes update an active parameter lock's restored base as well as the live parameter, preventing the next lock restore from discarding the edit.

## APIs and lifecycle

All track-taking functions take internal tracks 0..3. Only `jam_role_track(display_role)` takes display role order. Include after `seq.c`; no header is needed in the shared single translation unit. Writes use upstream `fm1_irq_off/on` convention. Host adapters supply no-op IRQ helpers. Calls should originate with IRQs enabled; no flash work or dynamic allocation occurs.

```
int jam_new_dusty(void);
uint32_t jam_role_track(uint32_t role);
const char *jam_role_name(uint32_t track);
uint32_t jam_sound_count(uint32_t track);
uint32_t jam_sound_index(uint32_t track);
const char *jam_sound_name(uint32_t track);
int jam_sound_load(uint32_t track, uint32_t index);
void jam_sound_invalidate(uint32_t track);
const char *jam_macro_name(uint32_t track, uint32_t macro);
uint32_t jam_macro_get(uint32_t track, uint32_t macro);
void jam_macro_set(uint32_t track, uint32_t macro, int32_t percent);
```

Each valid track has two curated sounds. Invalid sound requests return 0. Unknown melodic identity or a foreign engine produces sound index 0 and actual engine name. Generic factory/user/project loads must call `jam_sound_invalidate`; the three explicit loader seams call this hook. A subsequent curated load restores identity. Deep KIT changes are detected from actual kit parameter and display the actual kit name. Identity metadata is five bytes of session RAM, not part of the persistent project format. Deep parameter edits retain the curated origin name as an edited patch. Save/restore identity and editor naming are later format/UI work.

`jam_sound_load` preserves full step, microtiming, fill and lock-record arrays, mix level/pan/mute, sequencer length/division/swing/gate, root/scale/quantization/chord, track filter, strum and voice-leading choices. It uses `sound_change`/panic for melodic crossfades. It does not clear or seed the pattern. Pending active parameter locks are restored before loading the new base sound.

## Validation and audition artifact

Run `sh tests/run_jam_palette_tests.sh`. The runner generates required headers when absent/stale, compiles native real firmware DSP+sequencer, and writes `build/jam-tests/dusty-demo.wav` from the actual module. At completion: 435 checks, 0 failures.

Coverage: fresh session routing/beat/empty melody; playing-session rejection; all-pattern/micro/fill/lock preservation through every variant; mix/key/timing preservation; identity fallback and revalidation; all macro clamps and cross-role isolation; active lock base update; repeated New after held chord, muted lanes, pedal/bend, queued notes/programs/auditions and punch/roll; device settings retained; next block does not retrigger old physical key.

Audition adds explicit demo-only Cm7/Abmaj7, bass and live lead to the otherwise melodic-empty session. It renders four bars (~10.9 s) plus three seconds of effects tail into the WAV; tests continue the release render for 16 seconds. Peak 17994 on integer PCM range ±32768; observed maximum six busy synth voices; all synth/drum voices released; final one-second tail peak 0. Separate three-second renders of both variants with all macros at 100% also peaked at 17994 with zero full-scale samples. This checks bounded output from integer DSP; it is not a subjective sound-quality verdict or target CPU test.

Existing host compiler warnings refer to upstream hardware-address casts, an unused USB setting, and possible punch filter initialization. UI-agent sanitizer also observed existing signed-shift undefined behavior in upstream drum/fx code. These remain a target/runtime review task; this palette code introduces no float DSP, allocation or new engine implementation. FM-1 memory map, total nested-IRQ timing, load/shedding, physical controls, sound quality and recovery/flashing remain hardware acceptance work.
