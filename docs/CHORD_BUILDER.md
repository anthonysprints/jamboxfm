# Chord builder

This guide describes the new shared browser/FM-1 chord implementation.
Software checks and publication are tracked in the [build record](build-evidence/chord-builder.md).
Physical audition of this build remains separate from the earlier successful
device update, which did not include these changes.

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

Tap **HOME** to return to sound macros. From the first SEL page, turn the large
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

With live chords in OFF/SNAP, lights identify the keys you pressed and any
corrected root. In D# major, pressing D# lights only D#; pressing E lights E
and D#. It does not light every chord tone or other keys that could have
produced that root. Live chord roots take priority over playback/MIDI note
lights on the same part. Once no local chord keys are held, sounding
playback/MIDI pitches may light their actual keys. Brief chord-tone flashes
are suppressed in OFF/SNAP so they cannot look like an extra root suggestion.

The keyboard stores each held chord's resolved notes. Releasing one key must
not cut off notes still needed by another held key, including when two inputs
snap to the same root. This increment does not promise separate envelopes for
identical pitches, unlimited voices, or the full future external-MIDI versus
sequencer ownership system.

## Related plan

The next Home sequencer layout is still a proposal. A master Assist on/off
switch, Gentle/Tight recording correction, broader source ownership, complete
undo and saved sessions also remain future work. Current swing and deeper
editing controls continue to use their existing paths. See [First device
feedback](FIRST_DEVICE_FEEDBACK.md) and the [implementation roadmap](IMPLEMENTATION_PLAN.md).
