# Sound controls that match the engine: first implementation

October 15, 2026. Development branch: `next/sound-page-cleanup`.
This builds on Trial 05 without updating the normal installer, live browser
instrument or active drum preview. Physical device acceptance is pending.
The [research](research/sound-pages/FINDINGS.md) explains the source/audio audit;
this document describes the implemented first slice.

## FM6: ENV shows working controls

With an FM6 sound such as SOFT PAD, NYLON PICK or GLASS BELL selected, ENV opens
**FM ENV**. The ordinary ADSR display and ENV DEST page are unavailable for FM6,
whose amplitude follows envelopes inside the FM patch.

| Right knob | Label | Actual control |
| --- | --- | --- |
| 1 | TIME | Existing modulator-envelope time offset (`P_E4` / MEG). Positive values slow it down; negative values speed it up. |
| 2 | PITCH | Existing amount of the short pitch envelope (`P_ED_PIT`). This is independent of the patch's amplitude envelopes. |
| 3–4 | Blank | No control on this page. |

The screen says **PATCH ENVELOPES** and **+TIME: SLOWER MOD**. TIME is a relative
offset, not a duration or a universal amplitude attack/release knob. The detailed
FM6 algorithm, feedback, modulation, detune and patch controls remain in EDIT.
Repeated ENV stays on FM ENV. Its one-page family follows the existing SELECT
rule: SELECT changes BPM there. Other multi-page ENV/LFO families retain SELECT
page navigation. Home controls, held shortcuts and sound browsing retain their
existing roles.

Other engines keep their normal ENV and ENV DEST pages. In particular, the
organ's working amplitude envelope stays available. This change never copies
or converts ordinary ADSR values into an FM patch.

## Modulation destinations follow the selected engine and mode

Only the unavailable destination columns are blanked. The parameters remain
stored and accessible through their supported engine/mode; changing a wave or
algorithm does not zero them.

| Engine / mode | Cutoff/timbre bus | Shape bus |
| --- | --- | --- |
| ANALOG | Available | Available only for PWM wave; labelled PWM |
| DIGITAL, algorithms 1–7 on screen | Available as IDX1 | Available as IDX2 |
| DIGITAL, algorithm 8 on screen (parallel sines) | Blank | Blank |
| PHASE | Available as DEP1 | Available as DEP2 |
| LOFI | Available; its existing filter only opens for positive bus values | Blank |
| SAMPLE | Available | Blank |
| FORMANT | Available | Available |
| TRIO | Available | Available only for pulse-containing wave sets 1, 2, 6, 7, 10, 15 (internal indices) |
| WHEEL / organ | Blank | Blank |
| GRAIN | Available | Available |
| FM6 LFO | Available as MOD (modulator level) | Available as FDBK (feedback) |
| PHYS | Available | Blank |
| NOISE | Available | Available in all four source modes |

The table describes ENV/LFO timbre buses, not the separate **FX → FILTER**.
That working post-filter remains available on the organ and other parts.
Ordinary ENV pitch, LFO pitch/amplitude, rate, fade, sync, phase, trigger and
polarity remain available. DIGITAL and PHASE retain both additive timbre routes
so existing saved amounts never become inaccessible just to simplify a label.

Turning a blank modulation column gives a brief contextual hint. PWM, DIGITAL's
parallel-sine algorithm and TRIO's pulse requirement have specific explanations;
other unavailable routes say **UNUSED FOR THIS SOUND**. These hints do not
select a MIDI-learn or sequencer-lock parameter. Intentional empty FM ENV
columns simply stay empty.

## Preservation and limits

This slice changes page descriptions, visibility, labels and feedback only.
Parameter numbering/ranges/defaults, factory presets, engines, audio rendering,
recording data, SYN banks, project layout and manual-save codec are unchanged.
Hidden amounts remain intact, including negative values, and become editable
again when a compatible sound mode is selected. Save now remains intentional;
there is no automatic saving.

The canonical saved identity stays unchanged. An exact new source-catalog
review records the UI-only `params.c` change in
`firmware/experimental/recovery/jr_catalog_compatibility.json`. Generated assets,
TP/GP definitions, engine/kit ordering, codec/runtime and layouts must still
match. Unknown future catalog/asset changes remain refused. Old/current cold
restore is checked independently against the existing manual-save baseline.

Navigation preserves first live-key playing, note releases, chord assistance,
recording overlays, manual-save modals and held gestures. Trial 05's individual
drum editor, explicit factory-to-SYN copy and SEQ bank shortcut remain intact.

This is not the complete cleanup. Later candidates may improve main EDIT knob
groupings, shared-effect labels and broader page navigation. Known conditional
cases such as LOFI STEP envelopes, LFO Rate while synced, and noise pitch with
filter key tracking need separate treatment; no working control is removed
merely because it has a conditional effect.

## Verification

The dedicated native panel fixture checks all twelve engines and conditional
wave/algorithm modes, saved-value preservation, actual MIDI/lock targets,
Home/page repair, first-key audition, recording/manual-save priority and the
drum trial workflow. Run:

```sh
. tools/cloud/activate.sh
sh tests/run_jam_sound_pages_tests.sh
sh tests/run_jam_sound_modulation_audio_tests.sh
python3 tools/build_jam_emu.py
node tests/jam_sound_pages_browser.mjs
```

The original 23 paired production audio renders match the pre-cleanup report.
Another 56 pairs verify DIGITAL's parallel-sine exception and all sixteen TRIO
wave sets, including static pulse width and both modulation sources. Native
ASan/UBSan, existing Home/drum/recovery checks, cross-version cold restore,
production AudioWorklet browser checks and a target build supplement them.
See [build evidence](build-evidence/sound-page-cleanup/README.md) for exact
counts, build revision, memory figures and remaining limits.

No physical FM-1 is accessed by these checks. No new installer was published;
the owner can keep testing Trial 05 while this candidate stays isolated.
