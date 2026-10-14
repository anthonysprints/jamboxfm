# Chord builder

This guide describes the new shared browser/FM-1 chord implementation.
The original chord build's software checks and publication are tracked in its
[build record](build-evidence/chord-builder.md). The October 14 lighting
follow-up below replaces that build's root-only chord display. Its software
checks and publication are in the [lighting build record](build-evidence/sounding-note-lights.md);
physical audition remains separate.

## Start with one key

On Home, turn **ALGORITHM** to select **Chords**, then tap **SEL**. If SEL 2
opens, turn **SELECT** one click left to reach the first SEL page. The four
small knobs now control:

| Knob | Screen label | What it changes |
| --- | --- | --- |
| 1 | ROOT | The selected part's musical key, such as C or D# |
| 2 | SCL | Its scale, such as MAJ or MIN |
| 3 | QNT | How the printed keys become played roots: OFF, SNAP or WHITE |
| 4 | CHORD | Single notes, automatic scale chords or a fixed chord shape |

For the reported D# example, choose **ROOT D#**, **SCL MAJ**, **QNT SNAP**
and **CHORD TRIAD**. Press **D#**: it builds a major triad, displayed as
**D#–G–A#**. Those are the same sounding notes as E-flat–G–B-flat; the
instrument uses sharp labels. D# is already in the scale, so its root stays
D#. Pressing E instead rounds the root down to D# and builds the same chord.

The old behavior silently changed every chord mode to a white-key layout.
That made D# a modifier instead of a chord root, even when QNT showed SNAP.
The new behavior follows QNT on every melodic part, including Lead when its
CHORD setting is enabled. Turning CHORD on also selects polyphonic voices, so
Lead or Bass can sound the whole chord instead of only one of its notes.
Turning CHORD OFF leaves that voice setting in POLY; it does not restore an
earlier mono/legato setting. A fresh Dusty session still starts Bass and Lead
in their original legato mode until chords are enabled. Drum pads retain
their drum assignments.

Tap **HOME** to return to the remembered Home view; tap it again if needed for SOUND macros. From the first SEL page, turn the large
**SELECT** knob one click to reach **SEL 2** for transpose, strum and inversion
controls. Settings apply to the selected part. They are temporary in the
current firmware.

## Choose keyboard behavior separately from chord shape

| QNT | What a key does |
| --- | --- |
| OFF | Uses its printed note, adjusted by octave and transpose, as the root. Every black key works. |
| SNAP | Keeps roots already in the scale; rounds other roots **down** to the next scale note. Every black key works. |
| WHITE | White keys walk scale degrees, with C4 mapped to the selected musical root. With CHORD enabled, black keys modify held white-key chords rather than start chords. |

SNAP corrects the root before chord building. It does not move the other chord
tones afterward, so a chosen minor chord stays minor. To deliberately play a
root outside the scale, use QNT OFF.

In WHITE, the existing black-key modifiers remain: F# changes the third,
G# adds a seventh, A# gives a suspended fourth, C# adds a ninth and D# inverts
the held chord. This is an alternate playing layout, not a requirement for
using the chord builder.

For normal OFF/SNAP playing, use the CHORD choices below for major, minor,
seventh, ninth or suspended shapes, and **INVERT** on SEL 2 to move notes
between octaves. This keeps the black keys available as roots. The CHORD
choices select one recipe at a time; the existing WHITE modifier combinations
remain available as the alternate layout.

## Choose a chord

| CHORD | Result |
| --- | --- |
| OFF | One note; QNT still determines that note. |
| TRIAD | Root, third and fifth selected from the scale. |
| 7TH | Root, third, fifth and seventh selected from the scale. |
| 9TH | Four-note voicing: root, third, seventh and ninth selected from the scale. |
| SUS4 | Root, fourth and fifth selected from the scale. |
| POWER | Root, perfect fifth and octave: semitone offsets 0, 7, 12. |
| MAJOR | Fixed major triad: offsets 0, 4, 7. D# gives D#–G–A#. |
| MINOR | Fixed minor triad: offsets 0, 3, 7. D# gives D#–F#–A#. |
| DIM | Fixed diminished triad: offsets 0, 3, 6. D# gives D#–F#–A. |

**TRIAD** means a three-note chord whose quality follows the selected scale.
In C major, C builds C major, D builds D minor, and B builds B diminished.
**MAJOR** means “make a major chord on whichever root I play.” Choosing SCL MAJ
does not mean every automatic chord becomes major. Choose TRIAD for easy
progressions that follow the scale, or MAJOR/MINOR/DIM when you want that exact
sound on each root. With SCL CHR, the scale-derived chord options retain the
existing minor-scale fallback.

Dusty starts with **Chords: SNAP + 7TH** and **Lead: SNAP + OFF**. You can
change either independently. Existing CHORD values 0–5 keep their meanings;
MAJOR, MINOR and DIM append values 6–8.

## Change the voicing and strum

Tap **SEL**, then turn **SELECT** one click from its first page to **SEL 2**.
Small knob 2 controls **STRUM** and small knob 3 controls **INVERT**.

| INVERT | What it does |
| --- | --- |
| ROOT | Uses the chord's root-position notes. |
| AUTO | Moves the next chord's notes between octaves to stay near the previous chord. |
| +1 | Moves the lowest chord note up one octave. |
| +2 | Repeats that operation twice. |
| +3 | Repeats it three times. On a triad, this moves the whole chord up an octave. |

For D#–G–A#, **+1** gives G–A#–D# with that last D# in the next octave.
The chord keeps its identity; the lowest sounding note changes. AUTO is useful
for smoother moves between different chords. On a four-note seventh chord,
+3 produces its third inversion; on a three-note triad it is an octave shift,
not a new inversion. If an octave is already occupied, as in a POWER chord,
the moved note uses the next free octave so the voiced notes remain distinct.
Moves that would exceed the available note range stop at the last valid voicing.

**STRUM 0** starts the notes together. Positive values spread them from low
to high, and negative values from high to low; the number is milliseconds
between notes. Start with a small value for a softer, less simultaneous attack.

INVERT reuses the existing voice-leading setting: stored value 0 becomes ROOT
and value 1 becomes AUTO; +1/+2/+3 append values 2–4. The project layout is
unchanged. Selecting a curated JamBox sound while CHORD is on keeps polyphonic
voices so the chord remains audible.

## Lights and held keys

The October 14 follow-up shows **the keys you press plus every note playing
on the selected part**, including all sounding chord tones. In D# major with
TRIAD, ROOT inversion and no strum, pressing D# lights D#, G and A#. Pressing
E in SNAP lights E as well, because E is the physical input while D#–G–A#
is the chord being played.

With CHORD OFF, the original single-note SNAP behavior remains: D# lights
only D#; E sounds D# and lights E plus D#. E does not light merely because it
could have produced D#. Additional lit keys represent physical inputs or
notes actually playing, including simultaneous sequencer/MIDI notes on the
selected part.

Sounding notes use their printed positions in the selected keyboard octave,
independent of OFF/SNAP/WHITE input mapping or transpose. Transposing or
inverting a chord therefore moves its sounding-note lights; pressed keys
still light in place. Notes beyond the visible keyboard range are not wrapped
onto unrelated keys. Drum pads, step editing and held-button layers keep
their existing control indications.

Brief note flashes of about a tenth of a second make quick taps visible.
Held-note lights follow note-on/note-off, rather than continuing through
reverb or other effect tails. These lights show the selected part, not an
aggregate of every track in the session.

The keyboard stores each held chord's resolved notes. Releasing one key must
not cut off notes still needed by another held key, including when two inputs
snap to the same root. This increment does not promise separate envelopes for
identical pitches, unlimited voices, or the full future external-MIDI versus
sequencer ownership system.

## Related plan

The [GROOVE/SOUND Home views](HOME_VIEWS_PLAN.md) and
[GROOVE track controls](GROOVE_CONTROLS.md) are implemented. A master Assist
on/off switch, Gentle/Tight recording correction, broader source ownership,
complete undo and saved sessions remain future work. Independent note lengths
are the [current software follow-up](INDEPENDENT_NOTES.md), with native/browser checks and the FM-1 build passed; publication pending. See [First device
feedback](FIRST_DEVICE_FEEDBACK.md) and the [implementation roadmap](IMPLEMENTATION_PLAN.md).

## Bass: one note or several?

Bass and Lead start in **LEGATO**, a one-note mode. Overlapping presses select
one note according to the voice priority, and live recording keeps only one
note on a step. That is a common bass setup, not a musical rule.

To play several notes yourself, choose the Bass sound first, tap **EDIT**, turn
**SELECT** until **VOICE** appears (not VOICE 2), then set knob 1 to **POLY**.
Leave **CHORD OFF** for individual keys; POLY is the number of simultaneous
notes, while CHORD builds several notes from one key. Tap HOME to return.
Choosing another curated Bass/Lead sound currently restores LEGATO when CHORD
is off, so set POLY after choosing the sound. This follow-up does not change
that existing behavior or the default.

The published GROOVE-controls build still records tied note groups: starting
a new group on a later step interrupts the earlier hold. The
[independent-note follow-up](INDEPENDENT_NOTES.md) removes that limitation in
the working software for POLY recording. It stores a grid length for each
note, so holding C, adding E later, then releasing E can leave C sounding.
Each step still accepts up to four note starts, and the synth parts still
share eight voices. Bass/Lead defaults and the sound-browsing behavior above
are unchanged. Native/browser checks and the FM-1 build passed. Publication and physical audition
remain pending; an older installed build retains the group-based behavior.
