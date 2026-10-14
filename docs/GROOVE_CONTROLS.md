# Groove controls

GROOVE uses the Sloop-style performance controls requested after the first
two-Home-view build. SOUND keeps Bright/Move/Grit/Space (or the drum sound's
Tone/Level/Echo/Room). HOME still switches views and remembers the last one
when returning from other pages.

| Knob | GROOVE | Scope and behavior |
| --- | --- | --- |
| 1 | SWING | Shared sequencer swing, 0–100; 0 adds no global swing. |
| 2 | LEVEL | Selected part's volume, displayed as 0–100%. Explicit mute stays on. |
| 3 | STEPS | Selected pattern's length, 1–64 steps, one step per detent. |
| 4 | PAN | Selected part's stereo position: L100 through CENTER to R100. |

SELECT still changes tempo; ALGORITHM selects Drums/Bass/Chords/Lead;
PRESETS chooses the sound. The keyboard plays normally in either view.
The browser labels and accessible values follow the visible view and revert
to generic knob labels when a menu, recorder or held layer owns the controls.
All four Groove controls work with every sound engine.

Swing adds to the part swing already set on the deeper PATTERN page, capped
at the existing maximum. Triplet/long divisions and arpeggiators retain their
existing timing behavior; the arpeggiator has its own swing setting.

Level adjusts the whole drum kit when Drums is selected. This is the same
underlying drum volume as SOUND's drum Level control, using the mixer range.
Pan also acts on the whole kit; existing reverb/delay sends are not repositioned
by the dry pan control. Center is displayed explicitly.

Changing Steps takes effect immediately during playback and can change the
current loop position. It does not copy, erase or stretch notes: shortening
hides later steps and extending reveals them again. Notes, ties, locks and
timing data remain stored. At the usual 1/16 division, 16 steps is one 4/4 bar
and 32 is two; other divisions have different durations. Storage is still
temporary and resets on power-off.

The [Bass voice explanation](CHORD_BUILDER.md#bass-one-note-or-several)
distinguishes LEGATO/POLY and automatic chord building. These Groove controls
do not alter voice mode. The next [independent-note recording
follow-up](INDEPENDENT_NOTES.md) gives POLY notes their own grid lengths,
including staggered notes and overdubs, while preserving Bass's LEGATO
default. It passed native/browser checks, the FM-1 build and publication verification. Shortening Steps keeps the stored duration data with the hidden
onsets, and playback bounds held lengths to the current loop.

Build, tests and publication are recorded in [the follow-up evidence](build-evidence/groove-controls.md).
