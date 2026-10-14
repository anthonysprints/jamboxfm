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
The screen already shows BPM; its footer identifies SELECT as tempo and ALGO
as part selection.
Held-button layers, record setup, menus and deeper pages keep their existing
control ownership. The browser labels Home controls only while Home actually
owns them, including when a held layer temporarily covers Home.

## 2. Sequencer prominence: proposal, no layout change yet

The first Home prototype prioritized choosing a part and shaping its sound.
The FM-1 can show a sequencer: the inherited TRACKS view already renders four
16-step rows. JamBox's Home replaced that view with role tabs, a large sound
name and four macros. This is a UI choice, not a hardware limitation.

Proposed next layout: tempo/transport header (32px), four part/activity rows
(96px), selected sound (28px), compact four macro columns (56px), and help
line (28px). Review a 240×240 mockup before implementation. Home remains a
playable keyboard; the separate SEQ view explicitly owns keys for editing.

Current shortcut: hold SEQ to see its 4×4 step tiles. To keep this editor open,
hold SEQ, tap HOME, then release SEQ. Tap HOME again to return. White keys now
enter steps. A quick SEQ tap opens the remembered STEP/PATTERN/SONG page instead.

## 3. Chords from the pressed root: diagnosis and proposed contract

The existing SEL page already has CHORD on knob 4: OFF, TRIAD, 7TH, 9TH,
SUS4 and POWER. TRIAD/7TH/9TH/SUS4 build notes from the selected scale. In C
major, TRIAD on successive scale degrees yields C major, D minor, and so on;
the B triad is diminished. POWER uses fixed root-relative intervals.

Currently any enabled chord type forces the white-key degree layout even when
QNT says OFF or SNAP. Black keys are CHORD+ modifiers: D# inverts a held chord
and is silent by itself. Thus the reported D# behavior is real and misleading.
The owner's follow-up favors choosing the actual root and a chord shape,
rather than treating the chord feature itself as snapping.

Proposed implementation, kept separate from the tempo/light fixes:

- **CHORD OFF:** play single notes.
- Add explicit **MAJOR**, **MINOR** and **DIM** root-relative shapes; keep
  POWER and the existing scale-built choices. A triad means three notes, while
  major/minor/diminished describe how those notes are spaced.
- **QNT OFF:** the printed key is the root. D# + MINOR gives D#–F#–A#.
- **QNT SNAP:** optionally correct the root into the chosen scale, then build
  the selected chord. Never snap its individual tones afterward: that would
  change the chosen chord shape.
- **QNT WHITE:** retain degree-based white keys and black-key modifiers as an
  explicit alternate layout. Chord mode must no longer silently override QNT.
- Keep the automatic scale-based triad/seventh choices for easy progressions;
  preserve existing stored numeric IDs, append new shapes, and explain which
  choices follow the scale. No new project parameter is necessary.
- For the proposed chromatic/SNAP chord layout, root feedback should show the
  pressed root and corrected root without lighting alternate input aliases.
  Showing all chord tones would be a distinct display preference.

Before changing chord behavior, verify exact output pitches, black-key roots,
duplicate snapped roots and overlapping chord notes, both release orders,
held setting changes, arpeggiator, strum and recording. The current release
path is pitch-based; don't promise independent ownership without testing it.
This proposal is not implemented by the current tempo/light update.

## 4. SNAP key lights: shared default corrected

The existing actual-pitch LED mapping was correct for single-note SNAP, but
only the browser enabled NOTES at startup. The hardware default was off.
JamBox now enables NOTES in the shared panel defaults and the browser-specific
override is removed. In D# major with chords off, pressing D# lights only D#;
pressing E sounds D# and lights E plus D#. Release, octave and transposition
behavior remain covered by tests. Sloop's default is unchanged.

The NOTES menu can still disable sounding-note lights. On the previous build,
hold HOME, turn SELECT to LIGHTS, turn knob 3 to NOTES ON, then OCT− to close.
Settings remain temporary, so the previous build needs this after reboot.

## Delivery and verification

The shared C changes apply to the browser and device. The Chrome installer
accepts an already-running JamBox with its matching family response, so this
update can use the same page as the first install. Loader/rescue entry and
foreign firmware remain outside that page's workflow. Build, test and
publication outcomes are recorded separately from these plans.
