# Independent note lengths

October 14, 2026. Status: implemented and software-verified.
Native checks, actual WASM/Chromium checks and the FM-1 target build passed.
Final package checks and publication remain pending. Results belong in the
[follow-up build record](build-evidence/independent-notes.md) when finalized.
An existing installed GROOVE-controls build retains the earlier tied-group
recording model until this follow-up is published and installed.

## Play what you hold

On a melodic part in **POLY**, hold C, add E a few steps later, then release E
while keeping C held. Recording now gives C and E separate lengths, so replay
can preserve that overlap. The same applies to notes added over an existing
loop and to the free-take recorder. There is no extra overlap switch.

**Bass and Lead still start in LEGATO**, with chords off. That familiar
one-note behavior is unchanged. To try individual overlapping notes, choose
the sound first, tap **EDIT**, turn **SELECT** to **VOICE**, set knob 1 to
**POLY**, and leave **CHORD OFF**. Enabling CHORD already selects POLY so the
whole generated chord can sound. Turning CHORD off leaves POLY selected.
Choosing another curated Bass/Lead sound with CHORD off still restores LEGATO;
choose the sound before changing its voice mode.

The difference is between playing capacity and recording representation:
POLY lets several pitches sound together; independent lengths remember when
each recorded pitch ends. This feature changes the second behavior without
raising the voice count or changing the Bass default.

## What the grid still means

- Note starts and endings use the existing nearest-step recording grid. This
  is musical note recording, not unrestricted audio or a continuous piano roll.
- Each step stores up to four note starts. Notes that began on earlier steps
  can remain held while later steps start other notes.
- Explicit lengths are whole steps, from 1 to 64. A recorded hold is bounded
  to one current loop; playback also limits a stored length to the current
  pattern length. Short taps become at least one step.
- A note can cross the last-to-first-step boundary. Its release follows the
  part's clock, including tempo and swing changes, rather than a wall-clock
  timer. Starting an unrelated later note does not end it.
- The three synth parts still share eight voices, and drums keep their
  separate voice pool. Dense overlaps and long releases can still cause voice
  stealing. Four starts on a step does not guarantee unlimited simultaneous
  held notes or release tails.
- Changing a part's step division ends its currently sequenced independent
  notes; playback resumes on the new grid. Tempo and swing changes preserve
  their grid-based endpoints.
- Two occurrences of the same pitch do not gain separate synth envelopes.
  The existing pitch-based voice behavior remains; a repeated pitch can
  retrigger or renew that pitch's hold.

These limits keep work bounded on the FM-1. Existing drums, mono/legato
recording and legacy NOTE/TIE articulation retain their existing paths.
Off/Gentle/Tight timing correction and complete external-MIDI source ownership
remain separate roadmap work.

## Editing an overlap

The selected onset's **LEN** control changes only notes that start there.
It does not fill later cells with ties or erase another note that starts
inside the hold. The displayed length is the longest note at that onset.
If those notes already have different lengths, a turn applies the same
length change to each, bounded at 1–64, rather than flattening them all to one
value. An edited length is used when that onset next triggers. Editing an onset is still a group operation on the device; a separate
per-note piano-roll interface is outside this increment.

For notes with explicit lengths, that stored length determines their ending
instead of the shared **GATE** or a following tie chain. Existing legacy
steps without explicit lengths keep their earlier articulation. Before POLY
live overdubbing, legacy tied holds are given explicit lengths so inserting
another onset cannot cut them off. The SEQ
note-roll display draws the new sustained spans, including overlap and loop
wrap, and invalidates its display when only lengths change. GROOVE retains
its literal stored-step overview.

Transpose, rotation, doubling, clearing, note removal and undo/redo must keep
each length attached to its onset and pitch. Shortening GROOVE's **STEPS**
control keeps hidden onsets and their length data; extending it reveals them
again. The effective playback hold remains bounded to the current loop.

## Representation and project boundary

The shared C engine adds a `note_len[64][4]` byte table beside each runtime
track's existing steps. It does not enlarge `step_t`, the drum step union or
Sloop's legacy project format. A zero length selects legacy NOTE/TIE
articulation; 1–64 is an explicit grid duration. Recorder bookkeeping and
sequence release ownership use fixed-capacity tables, with no allocation or
unbounded event list in the audio path.

The current private JamBox RAM/export project becomes **JRM1**, 4,584 bytes:
the existing project prefix plus three 64×4-byte synth-duration tables and
the checksum. The earlier **JRM0** and Sloop **FUN6** layouts remain 3,816
bytes. A legacy decode supplies zero explicit lengths; it must not reinterpret
old bytes as durations. This internal compatibility does not enable foreign
project import through the installed firmware's disabled restore path.

Four larger snapshots do not fit the old retained region. Three JRM1 slots
use that region and the fourth uses ordinary zero-initialized RAM. JamBox
still clears session state on normal boot. Internal arranger snapshots,
undo and project capture/apply must include lengths; a sound-only change must
leave them intact.

Editor protocol **11** appends four optional duration bytes to a synth step.
The browser editor must retain and round-trip those bytes when changing a
step. A legacy step replacement without duration bytes clears the replaced
step's durations, so old lengths cannot attach to new pitches. This does not
change the restore/upload prohibition.

**Permanent saving remains disabled** (`FELUCCA_PERSIST=0`). JRM1 is a private
RAM/export representation, not a flash-save format or a promise of reboot
persistence. Its 4,584 bytes exceed the existing 3,840-byte storage-object
payload. The older proposal to add 24 bytes to FUN6 for a 3,840-byte JAM1
object must therefore be redesigned before saving is implemented. A future
versioned format needs an explicit packing/storage budget, migration rules,
interruption tests and companion sound-bank handling.

## Validation before publication

Use the real shared recorder and playback paths, rather than a second
JavaScript note scheduler. Required checks include:

- Staggered starts and independent releases, same-step chords, both release
  orders, shared pitches, overdubs, and playback without physical keys held.
- Notes crossing the loop boundary, a whole-loop hold, stopping/restarting,
  count-in/free-take completion, tempo/swing changes and panic.
- Legacy ties and gate/ratchet behavior, mono/legato and drums, live-note
  ownership, and no duplicate attack from record monitoring.
- Length editing without clearing another onset; transpose/deduplication,
  rotation, doubling, clearing, hidden steps, undo/redo and held input during
  a pattern replacement.
- JRM1 round trips and malformed lengths; legacy schema size/checksum and
  Sloop rejection; arranger snapshots; protocol-11 round trips and old-editor
  replacement; unchanged storage isolation.
- Native and actual Chromium playback, note lights and controls; final
  updater-enabled FM-1 app/RAM/pool/retained-region fit; source/package hashes
  and the deployed site's bytes.

Software checks do not measure the physical FM-1's audio deadline, stack use,
USB behavior or audible voice stealing. Those remain device audition and
performance checks after the candidate is built.
