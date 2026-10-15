# Trial 05: individual synthesized drum sounds

This development trial stays on `next/usability-and-saving` and the isolated
[drum installer](https://anthonysprints.github.io/jamboxfm/preview/drums/install/).
The adopted Trial 04 normal installer stays unchanged. Physical FM-1 validation
is pending owner testing; native and browser results are software evidence.

## Controls

- **SEQ:** existing hit grid; tap again to swap the eight-row drum bank.
- **EDIT:** existing whole-kit sound controls and playable drum keys.
- **ENV:** DRUM SOUND. Play a drum key to select and audition its sound.
- **FX:** existing whole-kit effects, slicer, insert and shared effect pages.
- **HOME:** return to the remembered Home view; also cancels copying.

DRUM SOUND has four right-hand knobs:

| Knob | Control | Meaning |
| --- | --- | --- |
| 1 | Tune | Base oscillator pitch, shown as a note. One detent is a semitone. Noise-only CHIP recipes use their noise clock pitch. Unpitched white noise shows `--`. |
| 2 | Decay | Shorter/longer tone and noise tails together, preserving their initial difference until a bound is reached. The displayed 0–127 value follows the tone for tonal sounds and noise otherwise. Hold, pitch drop and clap bursts keep their recipe settings. |
| 3 | Filter | Existing recipe cutoff, 0–127. The first turn enables filtering of the whole sound, enabling low-pass when the recipe had no filter. Existing low-/band-/high-pass type, envelope and resonance are retained. |
| 4 | Level | Individual sound gain, displayed in dB. One detent is 1 dB; inherited recipe precision remains quarter-dB. This is separate from whole-kit level. |

Kick, snare, hats and the other synthesized recipes use the same engine. Some
noise-only drums have no pitched oscillator: Tune correctly remains unavailable.
Closed and pedal hats share one recipe; the screen marks them **LINKED**. Their
ordinary pitch offset and open-hat choke behavior remain intact.

SELECT still changes BPM; ALGORITHM switches parts; PRESETS browses curated kits.
During copy confirmation PRESETS is consumed so the source cannot change.
Recording overlays, held gestures and manual-save screens keep priority. ENV
cannot begin an edit or confirm a copy while a take is armed/in progress.

## Factory kits and editable SYN banks

Factory synth kits are read-only. In DRUM SOUND, press ENV again to open **COPY
KIT?**, choose SYN1–4 with knob 1, then press ENV to confirm. The screen explicitly
warns **REPLACE WHOLE SYN KIT** and **SHARED BY SECTIONS A–D**. HOME cancels.
Confirmation copies the entire factory kit and selects that SYN bank. Other SYN
banks, factory recipes and recorded notes remain intact.

A SYN bank can be edited immediately. Changes affect every A–D section using that
bank. They remain in RAM until the normal **Save now** checkpoint; automatic
saving stays off. Restart restores the last intentional checkpoint. Copying is
not undoable through the step editor's undo history; cancel before confirming,
or restart without saving to return to the last checkpoint.

Sampled factory/user kits have no synth recipes. ENV explains **SAMPLED / USE
EDIT + FX**; it never silently replaces them with a synth kit. Sample-specific
editing and deeper oscillator/noise/drive controls are future work.

## Implementation and compatibility

`ui_drum_sound.c` reuses the existing 22-byte recipes and four 364-byte SYN kits
already serialized by manual recovery. No parameter IDs, factory recipes,
assets, kit ordering, codec, runtime identity or save layout change. UI maps the
panel's GM notes through `ds_lane`; panel order is different from recipe order.

An edit computes a complete local recipe, retires only voices reading that
recipe, then commits it with interrupts disabled. Copying retires voices of the
destination bank before replacing its recipes. The existing declick tail absorbs
their last output. Other voices continue. Changes sound on the next hit; an
ongoing hit is not continuously morphed.

## Validation and device check

Run the dedicated sound UI/DSP tests, actual save adapter tests, existing drum,
Home and recovery regressions, cross-version cold-restore test, production
AudioWorklet browser checks and the target build. Evidence belongs in
`docs/build-evidence/drum-sound/` after the final candidate is built.

For the owner trial:

1. Use Save now on the working version before installing the separate trial.
2. On Drums select a synthesized kit in EDIT. ENV opens its selected drum;
   ENV again opens copying. Try HOME cancellation first.
3. Choose a SYN bank you are willing to replace, then confirm with ENV.
4. Play kick, snare, closed/open hats and other drums; adjust each knob. Confirm
   neighboring drums keep their sounds. Check the linked closed/pedal hat label.
5. Play and record with the beat, return to SEQ/EDIT/FX/Home, and check held
   gestures and BPM/part switching still work normally.
6. Save now, fully power off/on, and confirm the edited kit and jam return.
   Without another Save now, later experiments should disappear after restart.

No deliberate interrupted write or power-loss test is requested.
