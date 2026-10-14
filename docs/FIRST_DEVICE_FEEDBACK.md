# First FM-1 playing feedback — 2026-10-14

The owner reports that installation succeeded and most functions work on the
physical FM-1. This establishes an owner-reported first boot and playing
experience; it does not complete the hardware validation or recovery checklist.
The owner later confirmed that a successful update retry enabled the BPM
control and expected SNAP lights, then requested the Home knob swap below.

## 1. Home knobs: SELECT tempo, ALGORITHM part

Implemented for the JamBox Home screen:

| Physical position | Label | Home action |
| --- | --- | --- |
| Top left | MASTER | Output level |
| Top right | SELECT | Tempo, one BPM per detent, 40–240 BPM |
| Bottom left | PRESETS | Selected part's sound |
| Bottom right | ALGORITHM | Drums / Bass / Chords / Lead |

The first UI update removed the bottom-right sound-browsing alias and assigned
tempo to ALGORITHM. After confirming that update on the FM-1, the owner asked
to swap the Home roles: SELECT now controls tempo and ALGORITHM selects parts.
Tempo changes keep
playback running and preserve the selected part, sound and all recorded steps.
Both Home views show BPM and retain these same encoder assignments.
Held-button layers, record setup, menus and deeper pages keep their existing
control ownership. The browser labels Home controls only while Home actually
owns them, including when a held layer temporarily covers Home.

## 2. Sequencer prominence: two Home views

Implemented and software-verified. Home provides a sound-focused view and a
sequencer overview, with **GROOVE at power-on**:

- **SOUND:** large selected sound name and four sound macros.
- **GROOVE:** four literal 16-step rows, selected sound and compact versions
  of the same macros. Each row identifies its displayed range and length.
- **HOME while already on Home:** switch SOUND ↔ GROOVE.
- **HOME from another normal page:** return to the Home view used last,
  without switching it again.
- **A short HOME tap in settings:** close settings to the remembered Home view.

Both views keep SELECT as tempo, ALGORITHM as part, PRESETS as sound, and
knobs 1–4 as the same sound macros. The keyboard stays playable; SEQ owns
explicit step editing. Remember the Home view for the powered session, with
GROOVE confirmed by the owner as the power-on view; power-cycle persistence
of subsequent view choices is later work. Holding HOME retains settings access.
Recording overlays, confirmation gestures and held-layer shortcuts keep input
priority, so their HOME gestures do not secretly toggle the remembered view.
Ordinary playback and overdubbing can continue while the visible Home switches.

GROOVE shows each part's own literal 16-step window and range/length, following
that part's playhead. Stopping retains each displayed window; a cell represents
one stored step. The selected sound and macros follow the selected part.
The [two-Home-view specification](HOME_VIEWS_PLAN.md) contains the layout,
navigation priorities and acceptance checks. Native/browser checks and the
updater-enabled FM-1 build passed. Publication is verified; physical audition remains
pending in the [Home views build record](build-evidence/home-views.md).

Current shortcut: hold SEQ to see its 4×4 step tiles. To keep this editor open,
hold SEQ, tap HOME, then release SEQ. Tap HOME again to return. White keys now
enter steps. A quick SEQ tap opens the remembered STEP/PATTERN/SONG page instead.

## 3. Chords from the pressed root: implemented

The owner confirmed that black keys stayed silent whenever CHORD was enabled,
including on Lead. The cause was shared keyboard logic: any chord type forced
the WHITE degree layout even when QNT said OFF or SNAP. D# was reserved for
inverting a held white-key chord. This was a control mismatch, not a musical
rule that forbids a D# chord in D# major.

The new shared implementation separates keyboard mapping from chord shape:

- **CHORD OFF** plays single notes. **MAJOR**, **MINOR** and **DIM** build fixed
  major, minor and diminished triads on the root. **POWER** retains its fixed
  root/fifth/octave shape.
- **TRIAD**, **7TH**, **9TH** and **SUS4** retain their scale-derived choices.
  With C major selected, TRIAD on C is major, on D minor, and on B diminished.
  Selecting a major scale does not make every chord major.
- **QNT OFF** uses the printed note as the root, with octave/transposition
  applied. D# with MINOR produces D#–F#–A#.
- **QNT SNAP** keeps in-scale roots and rounds other roots down to the next
  note in the scale, then builds the chord. It does not snap individual chord
  tones afterward. In D# major, D# with TRIAD produces D#–G–A# using the
  instrument's note labels; E uses that same D# root.
- **QNT WHITE** explicitly retains white-key scale degrees and black-key
  chord modifiers. Chord mode no longer silently forces that layout.

Dusty now starts Chords in SNAP with 7TH selected; Lead keeps SNAP with CHORD
OFF. The existing CHORD IDs 0–5 retain their meanings, and MAJOR/MINOR/DIM append
IDs 6–8. No project field is added. Keyboard note ownership now preserves shared tones until their final local
key releases. Checks cover duplicate snapped roots, overlapping chords, both
release orders, arpeggiation and strum.

The owner's follow-up confirms that enabling CHORD should produce the whole
chord on Lead and Bass too. Enabling it now selects POLY voices across the
normal/held SEL controls, preview/editor parameter paths and curated sound
loads. Turning CHORD off leaves POLY rather than silently restoring an earlier
voice setting. Fresh Dusty Bass and Lead retain their legato defaults until
chords are enabled.

To keep black keys available as roots, the CHORD selector provides the major,
minor, seventh, ninth and suspended choices, while SEL 2 adds an explicit
INVERT control: ROOT, AUTO, +1, +2 and +3. From SEL's first page, turn SELECT
one click; knob 3 changes INVERT and knob 2 changes the existing STRUM setting.
Each + step moves the lowest note up one octave. On a triad, +3 moves the
whole chord up an octave. Occupied octaves are skipped to keep notes distinct
(for example, in POWER chords). AUTO retains the old voice-leading behavior.
INVERT reuses `P_VLEAD`: old values 0/1 become ROOT/AUTO and values 2–4 add
the manual choices. WHITE's existing modifier combinations remain available.

The owner's later October 14 lighting request replaces the first chord build's
root-only display: LEDs now show pressed keys plus all notes playing on the
selected part, including chord tones and simultaneous sequencer/MIDI notes.
Sounding notes use printed note positions in the selected octave, independently
of OFF/SNAP/WHITE mapping or transpose; notes outside the visible range are
not wrapped. Brief note flashes are restored. Single-note SNAP keeps its
pressed-key-plus-corrected-note behavior without lighting unrelated input aliases.
This lighting follow-up's software results are in the
[lighting build record](build-evidence/sounding-note-lights.md); the original
chord build record remains historical evidence for the preceding behavior.
The beginner controls and exact shape table are in [Chord builder](CHORD_BUILDER.md).
Software validation and physical audition of this new build are separate from
the owner's successful earlier tempo/light update; results are in the
[chord build record](build-evidence/chord-builder.md). The two Home views are
the current implementation increment above; a master Assist switch,
timing-correction strength and persistent sessions remain future work.

## 4. SNAP key lights: shared default corrected

The existing actual-pitch LED mapping was correct for single-note SNAP, but
only the browser enabled NOTES at startup. The hardware default was off.
JamBox now enables NOTES in the shared panel defaults and the browser-specific
override is removed. In D# major with chords off, pressing D# lights only D#;
pressing E sounds D# and lights E plus D#. Release, octave and transposition
behavior are covered by the earlier single-note checks. Sloop's default is
unchanged. The later chord-light follow-up above retains this single-note
SNAP behavior while adding the sounding chord tones.

The NOTES menu can still disable sounding-note lights. On the previous build,
hold HOME, turn SELECT to LIGHTS, turn knob 3 to NOTES ON, then OCT− to close.
Settings remain temporary, so the previous build needs this after reboot.

## Delivery and verification

The shared C changes apply to the browser and device. The Chrome installer
accepts an already-running JamBox with its matching family response, so this
update can use the same page as the first install. Loader/rescue entry and
foreign firmware remain outside that page's workflow. Build, test and
publication outcomes are recorded separately from these plans.
