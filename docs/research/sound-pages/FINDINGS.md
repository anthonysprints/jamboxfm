# Melodic sound pages: baseline findings and proposed next slice

October 15, 2026. Research against `b1662b65a3e49817146bf7fd3a9f6454261817c7`,
which has the unchanged Trial 05 instrument. No firmware/installer changes.

## Evidence and its limits

The [native probe](../probes/sound-page-probe.c) drives the actual panel input,
UI, LCD rendering and production DSP through the existing host harness. It
inventories all 36 curated melodic presets, producing 595 reachable page
records across EDIT, ENV, LFO and FX. Repeated appearances of a shared page are
records, not distinct screens. It also makes 23 paired two-second audio renders
and records the existing navigation behavior. Each audio pair forks an untouched
parent so noise, oscillator and effect histories start identically. Renders
include a held note and release, with audible output in both alternatives.

[inventory.tsv](inventory.tsv), [audio.tsv](audio.tsv), [routes.txt](routes.txt)
and [summary.json](summary.json) record the results. Equal render hashes plus
equal energy support the source analysis; unequal hashes establish a DSP
change, not its musical usefulness or perceptual strength. These are focused
probes, not exhaustive tests of every note, sound, voice mode or modulation.
The native fixture uses icons enabled, unlike the icon-disabled device build.
No hardware, USB, flashing or website publishing is involved.

The curated presets cover six of the twelve enabled engines:

| Engine | Curated sounds | Actual first / second EDIT titles |
| --- | ---: | --- |
| ANALOG | 19 | OSC / FLT |
| DIGITAL | 6 | OPS / MOD |
| PHASE | 3 | PHS / LINE |
| SAMPLE | 3 | SET / TONE |
| WHEEL (organ) | 2 | BARS / TONE |
| FM6 | 3 | OPS / PATCH |

The remaining six engines in advanced browsing are not audited here. A later
change that affects generic page visibility must still preserve their access.
The visible EDIT titles already follow the engine. Internal EDIT 1/2 names
alone are not evidence of a generic or broken on-device page.

## Confirmed confusion and what to do about it

| Finding | Evidence | Proposed treatment |
| --- | --- | --- |
| FM6 shows ordinary Attack, Decay, Sustain and Release plus an ADSR graph, although FM6 uses its patch's own envelopes. | SOFT PAD renders had equal hash/energy for extremes of each ADSR control. `voice.c` bypasses ordinary amplitude ADSR when `ownenv` is set. | Replace the misleading FM6 ENV presentation with a short explanation and existing effective controls. Do not promise new overall attack/release behavior. |
| FM6's ordinary ENV-to-filter and ENV-to-shape controls do not change this sound. | Paired renders agree; shared `m.envq15` is zero for FM6. | Omit these destinations for FM6. Preserve its separate, working pitch-envelope amount. |
| Organ ENV/LFO filter and LFO shape destinations do nothing in the engine. | ROCK ORGAN pairs agree; its renderer does not consume shared cutoff/shape modulation. Pitch LFO and ordinary amplitude Attack positive controls changed the renders. | Keep organ ADSR and pitch/amplitude modulation; omit unused engine destinations. Keep working rotary controls and effects. |
| SAMPLE shape modulation has no engine effect. | DUSTY PNO LFO-shape pair agrees; renderer consumes cutoff but not shape. Cutoff LFO positive control changed audio. | Omit shape destination for SAMPLE; keep its real cutoff modulation. ENV-shape omission is supported by source, not separately rendered here. |
| Detune can appear broken on a simple Analog bass. | ROUND BASS second-oscillator mix zero: detune pair agrees. With mix 64, the pair changes. | Retain oscillator detune, mark its dependency on MIX rather than auto-adding another oscillator. |
| Analog shape modulation only affects PWM. | Sine shape pair agrees; PWM pair changes. Renderer uses shared shape for PWM pulse width. | Label the destination PWM and explain availability for that wave. Never silently switch wave type. |
| Turning LFO Rate without a destination has no audible result. | MELLOW LEAD Rate pair agrees with every destination zero, changes with cutoff depth enabled. | Offer a small 'choose a destination' hint when appropriate. Keep depth and source distinct. |
| DIGITAL's FLT modulation name suggests a filter it does not implement. | RHODES ENV-FLT pair changes; DIGITAL adds shared cutoff/shape to FM index. PHASE likewise uses these buses for distortion depth. | Use engine-specific destination names. Do not remove working modulation merely because its inherited name says FLT. |
| Analog has two EDIT filter presentations sharing CUT/RES/KTR. | Inventory plus matching parameter IDs in `params.c`; the second adds TYPE. | Consolidate only after a clear map exists that preserves TYPE, key tracking and drive. A duplicated control is not another filter. |
| Blank slots on DIGITAL, PHASE and SAMPLE are deliberate unused slots. | Descriptor label `-`, min=max=0; UI leaves them blank and editing returns without mutation. | Do not treat these as broken sound knobs. Repacking useful controls is a later grouping improvement. |

The [FM6 ENV screenshot](fm6-visible-ENV.png) illustrates why the first item is
misleading: an ordinary ADSR curve is drawn for a sound whose amplitude comes
from its own FM patch envelopes.

Important distinctions:

- The engine's cutoff modulation bus is separate from **FX → FILTER**, which
  uses the part's working post-filter. An organ lacks that engine modulation
  destination; this does not justify removing its working track filter.
- FM6 **MEG** changes the **modulators' envelope times**, with positive values
  slower. It is not a universal amplitude attack/release control. Its audio
  comparison changed; the working fixed pitch-envelope amount changed too.
- Envelope pitch amount uses a separate short pitch envelope in `voice.c`, not
  necessarily the ADSR curve drawn on ENV. Keep it accessible on FM6 and explain
  the distinction rather than deleting the whole ENV family.
- DIGITAL and PHASE currently add both FLT and SHP buses into their timbre depth.
  Hiding one existing route while retaining an audible saved value would create
  another hidden control. Any later consolidation must preserve access to
  existing values and deliberately define migration, if required.
- Engine detune (two oscillators, FM operators) and VOICE 2 detune (unison voices)
  have different jobs. Voice detune is applied by `mono_play` for multiple unison
  voices. Label these distinctions before changing the voice model.

## Navigation observations

The route probe confirms current behavior:

1. Analog EDIT first opens OSC.
2. SELECT moves to FLT without changing BPM.
3. HOME followed by EDIT remembers FLT.
4. Another EDIT reaches the Analog-only FILTER page.
5. Loading DIGITAL while there safely falls back to MOD; no unavailable page
   remains displayed.
6. Back on Home, SELECT changes BPM. Switching to Drums from the melodic EDIT
   page safely returns Home.

Repeated button taps already provide page cycling; SELECT adds reverse walking.
There is a genuine consistency question with Home/Drums' SELECT-as-BPM role,
but changing it affects many families and should be a separate task. Keep the
current behavior in the first sound-page slice. Preserve remembered pages,
first-key audition, held layers and modal priority.

## Recommended implementation order

### 1. Show controls that match the engine

First candidate: a small, explicit applicability/label change. Keep engines,
patches, sounds, recorded music and parameter storage unchanged.

- For FM6 ENV, show **PATCH ENVELOPES** with two existing useful controls:
  **MOD TIME** (`P_E4`, longer/shorter modulator envelope times) and **PITCH AMT**
  (`P_ED_PIT`, short pitch movement). Other slots should be visibly unused.
  Actual on-device labels must fit the existing columns; use short labels such
  as TIME / PITCH with a brief explanation. Keep detailed FM patch editing on
  EDIT. Do not relabel MEG as ATTACK or RELEASE.
- Keep Analog/Digital/Phase/Sample/Organ ordinary amplitude envelopes where
  they work. The organ has a working common ADSR and must not inherit FM6's
  special treatment.
- Omit source-confirmed unavailable modulation destinations for organ/SAMPLE
  and FM6 ENV. Keep valid pitch/amplitude and engine-specific timbre routes.
- Make changed destinations readable, including PWM, FM index/color and FM6
  modulation level/feedback. First define the complete mapping; do not rename
  a working destination to an unrelated parameter.
- Do not zero an existing modulation amount automatically when temporarily
  inapplicable. Restoring the compatible engine/wave should recover its value.

This is a proposed map, not implemented firmware. It must be checked against
all enabled engines, current saved work and actual device resource limits.
No automatic voice-mode or envelope conversion is part of this slice.

### 2. Put useful sound controls first

After the applicability candidate is tried, mock up four-knob groupings using
existing parameters. Starting points, subject to screen and listening review:

| Sound engine | Candidate first four controls | Keep deeper access to |
| --- | --- | --- |
| ANALOG | Wave / Cutoff / Resonance / Drive | Oscillator mix/detune, noise, filter type, key tracking |
| DIGITAL | Color/index / Mod decay / Feedback / Algorithm | Three operator ratios and voice controls |
| PHASE | Wave / Distortion depth / Envelope amount / Sub | Alternate wave, detune and line mode |
| SAMPLE | Sample set / Tune / Tone / Drive | Bit reduction, looping and its supported sources |
| WHEEL | Registration / Body / Top / Rotary | Sub bars, percussion, key click, drive |
| FM6 | Mod level/color / Mod time / Feedback / Mod ratio | Algorithm, velocity response, operator detune and patch selection |

These are per-engine groups, not per-track replacements: a Lead organ needs
organ controls, and a Bass FM sound needs FM controls. A sparse second page
should not be expanded solely to force four active knobs. Do not introduce
fake filter or sample-edit parameters merely to fill a row.

Avoid implementing all six reorganizations in one commit. Begin with a static
mockup for one common engine, keep the patch sound identical, and verify that
all existing deep controls remain reachable before broadening the change.

### 3. Separate later cleanup

- Global-vs-part effect labels (ALL ECHO / ALL ROOM or equivalent on melodic
  tracks too); their current mixes and chorus controls need their own review.
- SELECT tempo vs page navigation consistency and displayed track order on
  deep pages. Preserve reverse navigation until a replacement is agreed.
- Voice-mode-dependent hints, system-menu unavailable entries and empty user
  sound sources. These are useful follow-ups, not requirements for step 1.
- Extra tracks, multiple song slots, autosave and new sound engines remain
  outside this cleanup. Manual Save now stays intentional.

## Validation plan for the first implementation

Use the smallest compatible UI change; avoid new engine, storage or DSP code.
The Trial 05 app has 1912 bytes of flash spare. Measure a target build early and
preserve RAM/pool reserves. No target build was needed for this research-only
commit because firmware files are unchanged.

For implementation, turn these probes into focused regression checks for the
selected engine map. Check page entry/cycling, zero/negative modulation amounts,
engine/wave changes with settings retained, first live key and release, chords,
recording, manual-save modal ownership, Home memory and held shortcuts. Compare
actual old/current cold saves and canonical identity if catalog files change.
Preserve working SYN copying and drum screens during part changes. Keep the
normal release and active drum preview unchanged; select a distinct preview
publication path before deployment. Physical owner testing remains separate.
