# Preset library

This update expands the Home sound browser from two choices per part to
12 Bass, 12 Chords, 12 Lead and eight drum kits: 44 choices altogether.
It uses synthesis engines and licensed samples already included in JamBoxFM.
The original first two choices in every part keep their positions, and a new
Dusty session keeps its existing sounds, beat and settings.

Implementation and verification status is recorded in the
[preset build evidence](build-evidence/preset-bank.md). Software checks do not
replace listening feedback on the physical instrument.

## Playing and browsing

- Turn **ALGORITHM** to select Bass, Chords, Lead or Drums.
- Turn **PRESETS** to change that part's sound. Play the keys or leave your loop
  running while choosing a sound.
- Both Home views show the sound's position, such as **3/12**. A sound selected
  from the deeper engine browser shows **--/12** when it is outside this bank.
- Your recorded notes, key/scale, chord settings, timing and mixer settings
  stay with the part when you choose another sound.
- On **SOUND** Home, the four melodic knobs are **BRIGHT**, **MOVE**, **GRIT**
  and **SPACE**. Their underlying controls follow the engine used by the sound.
  The drum sound knobs are **TONE**, **LEVEL**, **ECHO** and **ROOM**.
- On **GROOVE** Home, the knobs remain **SWING**, **LEVEL**, **STEPS** and
  **PAN**. Press HOME while already at Home to switch between the two views.
- The deeper sound pages still expose the engine's individual controls.

A preset is a starting point: changing its knobs edits the sound you are
playing. Choosing it again reloads its starting settings. Browsing does not
write a user preset or permanently save the edited session.

BRIGHT changes tone using the engine's brightness control. MOVE adds its
appropriate modulation: filter movement on ANALOG and SAMPLE, FM-index
movement on DIGITAL, distortion-depth movement on PHASE, modulation-level
movement on FM6, and rotary speed on WHEEL. GRIT adds distortion; SPACE
adds delay and reverb. These controls shape the selected sound, so the
same setting can produce different results on different engines.

## Sound catalog

The character descriptions below describe the intended starting points;
they are not claims to reproduce an artist's recording or a particular
commercial instrument.

Numbers are browser positions starting at one. Factory names in the source
column identify the existing recipe being adapted; **JamBox original**
identifies the original Dusty palette. WHEEL is the organ engine (called
DRAWBAR internally).

### Bass

| # | Sound | Starting character | Engine / recipe source |
| --- | --- | --- | --- |
| 1 | ROUND BASS | Rounded, warm foundation | ANALOG / JamBox original |
| 2 | MUTED BASS | Shorter, restrained bass | ANALOG / JamBox original |
| 3 | SUB BASS | Simple deep low end | ANALOG / SUB BASS |
| 4 | 808 BOOM | Long electronic bass | ANALOG / 808 BOOM |
| 5 | 808 DIRTY | Rougher electronic bass | ANALOG / 808 DIRTY |
| 6 | FUNK BASS | Punchy, percussive line | ANALOG / FUNK BASS |
| 7 | SAW BASS | Buzzier synth bass | ANALOG / SAW BASS |
| 8 | REESE | Detuned bass texture | ANALOG / REESE |
| 9 | ACID 303 | Resonant acid-style bass | ANALOG / ACID 303 |
| 10 | FM BASS | Digital attack and body | DIGITAL / FM BASS |
| 11 | FINGER BASS | Bass-guitar-style synth | DIGITAL / BASS GTR |
| 12 | CZ BASS | Phase-distortion bass | PHASE / CZ BASS |

### Chords

| # | Sound | Starting character | Engine / recipe source |
| --- | --- | --- | --- |
| 1 | DUST KEYS | Soft, mellow keys | ANALOG / JamBox original |
| 2 | SOFT STABS | Short chord hits | ANALOG / JamBox original |
| 3 | RHODES | Electric-piano-style keys | DIGITAL / RHODES |
| 4 | WURLI | Reed-piano-style keys | DIGITAL / WURLI |
| 5 | CLAV | Percussive keyboard | DIGITAL / CLAV |
| 6 | DUSTY PNO | Recorded piano with color | SAMPLE / DUSTY PNO |
| 7 | ROCK ORGAN | Driven band-style organ | WHEEL / ROCK ORGAN |
| 8 | SOUL ORGAN | Organ for held chords and stabs | WHEEL / SOUL ORGAN |
| 9 | WARM PAD | Sustained synth chords | ANALOG / WARM PAD |
| 10 | SYN BRASS | Synthesized brass stabs | ANALOG / SYN BRASS |
| 11 | SOFT PAD | Smooth FM chord texture | FM6 / SOFT PAD |
| 12 | NYLON PICK | Plucked, guitar-like FM tone | FM6 / NYLON PICK |

### Lead

| # | Sound | Starting character | Engine / recipe source |
| --- | --- | --- | --- |
| 1 | MELLOW LEAD | Rounded melody sound | ANALOG / JamBox original |
| 2 | DUST REED | Soft, reedy synth | ANALOG / JamBox original |
| 3 | FAT LEAD | Fuller analog-style melody | ANALOG / FAT LEAD |
| 4 | PWM LEAD | Hollow pulse-wave color | ANALOG / PWM LEAD |
| 5 | G-FUNK LD | Sliding synth-hook starting point | ANALOG / G-FUNK LD |
| 6 | NEON SAW | Bright saw lead | ANALOG / SUPERSAW, role voice mode |
| 7 | CZ LEAD | Phase-distortion melody | PHASE / CZ LEAD |
| 8 | RESO PLUCK | Resonant short notes | PHASE / RESO PLUCK |
| 9 | MUSIC BOX | Delicate bell-like pluck | DIGITAL / MUSIC BOX |
| 10 | LOFI FLUTE | Colored recorded flute | SAMPLE / LOFI FLUTE |
| 11 | VIBES | Recorded vibraphone | SAMPLE / VIBES |
| 12 | GLASS BELL | Clear FM bell tone | FM6 / GLASS BELL |

### Drums

| # | Kit | Starting character | Existing kit source |
| --- | --- | --- | --- |
| 1 | DUST KIT | Dusty beat starting point | Synthesized / BOOMBAP |
| 2 | SOFT KIT | Softer lo-fi beat | Synthesized / LO-FI |
| 3 | ROCK KIT | Acoustic drum character | Sampled / ACOUSTIC |
| 4 | TIGHT KIT | Shorter acoustic drums | Sampled / TIGHT |
| 5 | 808 KIT | Rounded electronic drums | Synthesized / 808 |
| 6 | 909 KIT | Punchy electronic drums | Synthesized / 909 |
| 7 | TRAP KIT | Trap-style electronic drums | Synthesized / TRAP |
| 8 | NEON KIT | Synthwave-style electronic drums | Synthesized / SYNTHWV |

These are sound choices, rather than new patterns or full genre sessions.
Choose any kit with any Bass, Chords and Lead sounds. Presets leave Bass and
Lead in their normal single-note role unless chord mode is enabled; enabling
chords still selects full chord playback. Loading a sound disables its
factory arpeggiator so the keys and recorded notes remain direct.

## Where these sounds come from

An **engine** is the code that makes a type of sound. A **preset** is a named
recipe of settings for that engine: waveform or source, envelope, tone,
movement and effects. Many different presets can reuse one engine. A
**sample** is recorded audio; a sampled preset also tells the engine how to
play that recording. A **drum kit** chooses a collection of individual drum
sounds.

JamBoxFM inherited 153 factory presets across 12 melodic engines from Sloop,
plus 32 synthesized drum kits and five treatments of the sampled drum bank.
The first JamBox Home browser exposed only two deliberately curated sounds
per part. That small selection was a product choice, rather than the number
of sounds the FM-1 can hold. This update brings more of those existing
ingredients into the direct Home workflow, with role-specific choices and
consistent sound controls.

The six original melodic JamBox sounds are parameter recipes in
[`jam_palette.c`](../firmware/src/jam_palette.c). The added melodic sounds
adapt existing factory recipes. The inherited engine presets and their
measured trim table remain intact; curated gain offsets give the added
chord sounds headroom and compensate NEON SAW for its single-voice role.
These offsets avoid changing short plucks into the same measured average
level as sustained pads. The drum choices select existing kits.
No new audio recordings are imported by this expansion.

The sampled instruments already shipped with the code use CC0 recordings
from [VCSL](https://github.com/sgossner/VCSL) and
[VSCO-2 Community Edition](https://github.com/sgossner/VSCO-2-CE).
VCSL supplies the piano, vibraphone and acoustic drum/percussion sources;
VSCO-2 CE supplies the plucked double bass, brass, strings and flute.
The scratch recordings come from the Sonic Pi sample collection.
The exact source files and processing are recorded in
[`ATTRIBUTION.txt`](../assets/samples-cc0/ATTRIBUTION.txt) and
[`CREDITS.txt`](../assets/samples-cc0/CREDITS.txt).

The FM6 engine uses the Apache-2.0 msfa synthesis core; its existing factory
patches are Felucca's own. This does not imply that arbitrary DX7 cartridge
downloads have the same license. Full code and asset notices remain in
[`LICENSING.md`](../LICENSING.md).

## Lessons from the other firmware

[Ghoulbox's preset generator](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth/blob/42558bcb6cf3d5fd265d58682b5bae90701f5503/tools/ghoulbox_presets.py#L3)
is a useful model: one themed parameter table generates the firmware's
presets and the web editor's matching list. Its dungeon character comes
partly from curation, envelope/effect choices and instrument selection,
rather than a separate recording for every preset. Its generator appends
presets to preserve existing stored indexes. JamBox likewise keeps its
original curated positions stable and uses the same sound definitions in
the device and emulator.

The imported
[Sloop README](upstream/SLOOP_README.md#synths-and-sounds)
describes its broad factory library and categories. JamBox's inherited
source is pinned to
[`8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`](https://github.com/isod89/sloop-fm1/tree/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e),
so this update uses the recipes actually in this checkout rather than
assuming another firmware's newest sound list is compatible.

## Further sound-library work

After playing this bank, use feedback to decide which families need more
range: clean and driven basses; warm keys, organs and pads; bright and soft
leads; acoustic, dusty and electronic drums. Add distinct usable sounds
before adding near-duplicate variations. New presets should retain musical
settings, use useful macro ranges, have comparable loudness and leave room
for bass, chords, lead and drums together.

More original parameter recipes are the smallest next expansion. Additional
recorded drum packs need separate source/license review, conversion and
flash-size checks, level matching and a physical listening pass. CC0
libraries such as VCSL provide a documented starting point; downloading a
free pack does not by itself establish permission to bundle its recordings
in distributed firmware. No external bank or new sample pack is included
in this update.

Extra tracks are deferred at the owner's request. This expansion keeps the
four-part instrument and its existing shared voice limits.
