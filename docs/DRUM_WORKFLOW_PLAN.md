# Drums: investigation and proposed controls

October 15, 2026. Research and design only, against source
`da394a87b3345555e796257012e7adf4bbbc8872`. No firmware, installer or storage
behavior changes are included. Manual saving remains intentional; autosave is off.

The owner's goal is to lay out beats, play them live and explore sounds without
remembering hidden screens. Preserve the working four-part jam and useful depth.

Implementation follow-up: [GRID trial 02](DRUM_GRID_TRIAL.md) covers direct entry,
keyboard ownership, readable rows and the main grid controls. The owner reported
that trials 01 and 02 worked, including positive feedback on Trial 02's look.
The investigation
and screenshots below describe the preceding baseline. Individual drum sound
editing and the remaining page cleanup are separate later work.

## Earlier ideas recovered from GitHub

- [#5: useful drum navigation](https://github.com/anthonysprints/jamboxfm/issues/5):
  SEQ should open the existing drum grid; replace or skip inactive synth pages;
  keep tone/filter, kits, mixer controls and performance shortcuts.
- [#9: everyday navigation](https://github.com/anthonysprints/jamboxfm/issues/9):
  make editing versus auditioning explicit, preserve HOME and useful held layers,
  agree on the flow before changing it in small batches.
- [#6: controls match the sound](https://github.com/anthonysprints/jamboxfm/issues/6):
  preserve depth but expose controls that actually affect the selected engine.
- [#7: usable banks](https://github.com/anthonysprints/jamboxfm/issues/7): empty
  USR sample kits can be silent; SYN1–4 have real editable defaults and must stay.
- [#3: SAVE / SEQ / ARRANGE](https://github.com/anthonysprints/jamboxfm/issues/3):
  keep arranging sections separate from saving a jam. Its original claim that
  saving is disabled is historical: manual saving has now shipped.

This extends those ideas rather than replacing the backlog. #5 should start
small; the complete workflow below is a design target, not one release.

## What the current code and UI actually do

The research probe drives real shared UI press/release, draw and audio paths in
the existing host harness. It starts each route from JamBox Home with Drums
selected. It is not a physical FM-1 test, and uses recovery off in the host
fixture; SAVE behavior is not tested by this probe. Its icon-enabled host setup
also differs from the icon-disabled device profile.

| Action today | Observed result / consequence |
| --- | --- |
| Tap SEQ from Home | STEP → PATTERN → SONG on successive taps. STEP has no drum parameter columns; PATTERN works; SONG is arrangement. |
| Tap EDIT | EDIT 1 → EDIT 2 → VOICE, with no active drum parameter columns. |
| Tap ENV, LFO, SEL or ARP | Synth page families open with no active drum parameter columns. This does not mean their held shortcuts are useless. |
| Tap FX | FX and FILTER have no active drum parameter columns; SLICER works, as do the INSERT pages. The track filter itself works in DSP and through other routes. |
| Hold SEQ, tap HOME, release SEQ | The locked 4×4 STEPS layer stays open. This is a different editor from the dedicated drum grid. |
| Dedicated GRID | Working 16-drum × 16-step overview; keys edit one selected drum's steps. Existing normal entry depends on the inherited TRACKS page, while JamBox Home uses a different route. |
| Dedicated KIT | Named 4×4 pads; keys play sounds. It is also another whole-kit selector/mixer. |

The probe counts parameter descriptors on ordinary pages. Zero descriptors on
custom GRID/KIT, SONG or held-layer screens do **not** imply ignored controls;
those screens use separate handlers.

Current dedicated GRID knobs are **sound / step / hit / level**. White keys toggle
steps; the first four black keys select step pages. Selecting a sound auditions
it; moving the step cursor auditions all hits on that step. KIT uses **kit / level /
reverb / pan**. SEQ or EDIT taps toggle GRID/KIT, and SELECT also changes GRID/KIT.
PRESETS browses the full inherited kit list here, unlike Home's eight curated
kits. ALGORITHM uses the internal numeric track order, unlike Home's displayed
Drums/Bass/Chords/Lead order. These differences need explicit checks when routing
users into this editor.

The current dense grid devotes only six pixels of height to each drum. It has
no readable drum names beside its rows. Opening it directly helps discovery,
but will not by itself make editing easy to understand.

Actual existing screens:

- [Dedicated GRID](research/drums/dedicated-GRID.png)
- [Dedicated KIT](research/drums/dedicated-KIT.png)
- [Locked STEPS layer](research/drums/locked-SEQ.png)

## Recommended organization

Use three obvious jobs with direct buttons:

| Job | Entry | What the keyboard does |
| --- | --- | --- |
| **PLAY** | HOME, with Drums selected | Play the kit; REC records the performance. GROOVE/SOUND Home toggle remains. |
| **STEPS** | SEQ | Sixteen white keys place/remove hits for the named selected drum. Editing works without REC. |
| **SOUND** | EDIT | Select and audition a drum, then shape it with controls appropriate to its source. Later work; an honest whole-kit sound page can precede it. |

FX provides clearly labelled **KIT FX**, applying to the entire drum part. It
should not imply that increasing ROOM affects only the selected snare. SAVE
keeps the current manual checkpoint behavior. HOME returns to the remembered
Home view; another HOME tap there toggles GROOVE/SOUND as it does now.

Repeated SEQ should keep users in STEPS, rather than cycle to KIT or an arranger.
Use the existing held SEQ editor for advanced step gestures initially. Ordinary
entry must not require holding a button or locking a layer. Advanced behavior
should be shown on screen, with a predictable HOME escape.

Avoid requiring a separate KIT page for the common job of changing kits: PRESETS
already does that. A named pad map remains useful for learning which key plays
which drum; retain that content in the drum selector/SOUND area instead of making
it an unexplained second sequencer mode. No useful feature needs to be deleted
merely because its old route is removed.

## Physical controls: proposed destination

### The four knobs on the left

In the ordinary new drum workspaces, keep MASTER = output, SELECT = BPM,
ALGORITHM = part in the same display order as Home, PRESETS = whole kit.
Start with the existing eight curated kits; provide an explicit labelled route
to more usable factory kits and SYN kits. Skip or explain empty USR sources.
Never change the recorded beat just because a kit changes.

These are proposed ordinary-workspace roles. Existing held-step gestures use
PRESETS for parameter locks and ALGORITHM for their target; retain that ownership
and label it while held. Do not make one detent edit both a layer and its page.
Record-setup overlays and confirmations also retain priority.

Do not change SELECT's page-navigation role across every synth/settings page as
part of this drum work. The proposed consistency is scoped to drum workspaces.

### The four knobs on the right

| View | 1 | 2 | 3 | 4 | Scope |
| --- | --- | --- | --- | --- | --- |
| GROOVE Home | Swing | Level | Steps | Pan | Existing controls; swing is shared across the jam. |
| SOUND Home, Drums | Tone | Level | Echo | Room | Existing whole-kit macros. |
| STEPS, eventual simpler layout | Drum | Step | Strength | Repeat | Selected drum/cell. Strength = ghost/soft/normal/hard; Repeat = 1–4 hits within the step. |
| KIT FX, proposed useful first page | Filter | Echo | Room | Pan | Whole drum part, using existing working DSP parameters. |
| DRUM SOUND, synthesized basic page | Tune | Decay | Tone | Level | Selected synthesized recipe; requires new device UI. |

For the first navigation-only implementation, keep the existing GRID knobs
unchanged and document them. Moving hit on/off entirely to the step keys frees
its redundant HIT knob for Repeat later; do that as a second reviewed change.

On the proposed STEPS layout, white keys remain the explicit hit on/off action.
Knob 2 focuses a step without rewriting it. Strength/Repeat change an existing
hit; on an empty cell show an explicit empty state and do not silently create a
hit. Keep the selected drum and selected step visible. Deciding to edit a hit
must not require REC.

Audition a newly selected drum, but avoid automatically firing a whole step on
every cursor move during playback. Previewing a sound must not arm recording,
start the transport or add hits. Exact audition behavior needs separate tests.

### Other buttons

| Button | Recommended treatment on Drums |
| --- | --- |
| PLAY / REC | Preserve transport, live recording, overdub and existing clear confirmation. Distinguish REC READY from direct step editing. |
| SEQ tap | Open STEPS directly, independent of the last synth sequencer subpage. |
| EDIT tap | Useful drum sound entry; first distinguish KIT SOUND from future selected-DRUM SOUND. Never show inactive synth VOICE pages. |
| FX tap | Open meaningful KIT FX; retain visible access to insert/slicer and advanced shared effects. |
| ENV tap | When supported, go to the selected synthesized drum's decay/envelope controls. Until implemented, give brief contextual guidance instead of opening a dead synth envelope page. Sampled drums need their own applicable envelope controls. |
| LFO tap | No per-drum LFO is presently exposed. Keep the current workspace and explain briefly; do not invent a modulation destination with ignored knobs. Revisit after actual DSP support is designed. |
| SEL tap | Do not open chord/scale pages for drum hits. Direct the user to drum selection. Its held song-key gesture affects melodic parts and must be preserved or explicitly redesigned separately. |
| ARP tap | A small, useful ROLL/rate entry could replace the inactive synth ARP family. Preserve held ARP + pad note repeat and its recording. |
| GLO | Keep working mix/mute/solo/fill access. MIDI/system consolidation belongs to the wider navigation issue. |
| OCT− / OCT+ | PLAY keeps ghost/hard hits. In a later ordinary STEPS redesign, use clearly labelled previous/next step page; held-layer meanings stay explicit. |

This table is a direction for review. Do not ship placeholder drum envelope/LFO
pages or blindly repurpose held gestures to match the tap labels.

## Make the pattern readable

Prefer **eight named drum rows at a time**, with 16 literal steps per row,
over 16 anonymous miniature rows. Scroll the visible rows with the selected
drum. Show the entire selected drum name, its key position, current kit, step
range and total length. Group the steps in fours for the common 1/16 grid.

The 240×240 display can plausibly fit a compact 56-pixel header, eight 16-pixel
rows and the existing 56-pixel control strip. A five-character drum label uses
40 pixels; sixteen 11-pixel cells use 176, leaving room for margins. This is a
layout budget to prototype, not a verified finished screen. Render and inspect
it at native size before committing to it.

Keep page selection explicit, e.g. **17–32 / 32**, with disabled steps after the
length. The editor should keep the page the player selected while they edit;
do not chase a playhead to another page mid-gesture. Show an off-page playhead
indicator. Home can continue following playback as it does today.

White-key lights show the selected drum's steps in STEPS, and drum hits in PLAY.
Use a visible PLAY / STEPS / SOUND label so the same physical key's job is clear.
Do not also light unrelated sounding-pad aliases in STEPS at full brightness:
that can look like extra enabled steps. Preserve actual sounding-note lights in
ordinary playing and test layer/page transitions.

## Sound design: useful depth without false promises

Already available:

- Eight curated kits plus more inherited factory kits, sampled and synthesized.
- Tone/filter, kit volume, pan, echo, room, working inserts/slicer and live FX.
- Six independent drum voices, separate from the eight shared melodic voices.
- Sixteen drum lanes, simultaneous hits, four strengths and per-lane ratchets.
- Four writable SYN kits; their 22-byte sound recipes include tone, pitch drop,
  click, noise, filtering and drive. Current manual saving already captures all
  four SYN kits. The inherited web editor can edit their RAM values, but its
  old STORE command is disabled by the current persistence policy; do not imply
  that legacy STORE replaces JamBox Save now.

New device sound editing can use those SYN recipes before adding an engine.
Start with Tune/Decay/Tone/Level; put body/pitch drop, noise/click and drive on
clearly named deeper tabs. A hat's Tone cannot blindly alter a sine body that
the hat does not use. Decay may need to change both tone and noise envelopes;
the musical mapping needs auditioning for each drum family.

Factory recipes are immutable. Editing one requires an explicit copy into a
SYN working kit; never silently overwrite one of the four kits. These kits are
shared across A–D sections, so editing one changes every section referencing it.
Show that scope before a replacement. A new kit architecture is a separate
decision, not a hidden addition to navigation cleanup.

Do not assume keyboard lane index equals synthesis recipe index. Their orders
differ: map through the panel's GM note and `ds_lane`. Closed and pedal hats
share a recipe with a pitch offset. A future sound editor must describe that
link, rather than pretend they have independent stored recipes.

ROCK/TIGHT kits use samples; they cannot use synthesized body/noise controls.
Per-sample tune, decay, gain or independent effects would need a real playback
control path and possibly new saved state. Until then, retain honest kit-wide
controls. Mixing arbitrary sampled and synthesized sounds within a new kit is
also a separate engine/storage feature.

## Features to preserve, with honest scope

- Live finger drumming, overdub, ghost/hard hits, rolls and undo/redo.
- Per-drum mute and erase are different: mute stops pattern playback without
  removing hits; erase changes the beat. Existing EDIT + black-key mute mode is
  particularly hidden. Offer a labelled route later; never make a normal pad
  unexpectedly erase. Per-lane live mutes currently reset when a project is
  applied; do not claim they form part of the restored manual checkpoint.
- Strength/repeat are per drum within a step. Microtiming and fill conditions
  currently apply to **every drum on that step**. Track parameter locks affect
  the drum track. Per-snare timing/FX locks would require a different data model.
- Existing step undo does not restore locks/microtiming/fill metadata. Any new
  advanced editor must state or improve that coverage rather than promise all
  edits are undoable. Audit cleanup when the last hit on a step is removed.
- Shortening Steps hides later hits; it does not delete them. Restore them on
  extension and show page bounds consistently.
- More drum voices, more tracks, probability, song slots, import and autosave
  are outside this navigation/sound-workflow investigation.

## Suggested implementation order

1. **Make the existing editor reachable (#5a).** Ordinary SEQ on Drums opens
   GRID from either Home view and ordinary pages. Do not inherit a synth's
   remembered STEP/SONG destination. Preserve the existing GRID/KIT behavior and
   held SEQ/EDIT routes in this first batch. Check HOME, PLAY/REC, SAVE and
   release ownership. Retain the current stable installer.
2. **Unify the drum workspace (#5b, #9).** Make SEQ idempotent, use consistent
   BPM/part/kit encoders, reconcile `drum_lane` versus held-layer `pen_lane`,
   introduce readable rows/ranges, and review the Strength/Repeat knob proposal.
   Prototype the full 240×240 layout before changing input.
3. **Remove dead destinations (#5c, #6/#7).** Provide honest KIT SOUND/KIT FX,
   filter out inactive synth pages and empty sources, and preserve useful
   performance shortcuts. Route planning must include every tap, hold, repeat
   tap and track change, not just the first entry from Home.
4. **Shape an individual synthesized drum.** Add explicit copy-to-SYN behavior,
   safe edits, applicable basic/deep controls and shared-kit warnings. Verify
   manual-save/restart of the edited recipe. Plan sampled-drum controls
   separately instead of showing knobs that do nothing.
5. **Make performance tools discoverable.** Labelled mute/erase, roll, fill,
   copy/clear/undo and advanced timing; expand only with a tested gesture model.

The owner approved this staged direction. The first entry stage received broad
positive owner feedback, and the second grid/control stage is a separate trial.
Stages 3–5 remain later work. Keep all
implementation on `next/usability-and-saving` and use an isolated trial before
adopting it on the normal installer.

## Validation for future implementation

- Native real input paths: Drums SEQ from GROOVE/SOUND/deep pages; remembered
  synth page independence; press/release thresholds; held and locked layers;
  HOME view memory; track changes while keys are down; no duplicate knob reads.
- Grid: all 16 lanes; 1/16/17/32/64 lengths; page bounds; selected lane/cell;
  simultaneous hits; four strengths; repeats; audition versus write; empty-cell
  controls; last-hit deletion metadata; undo; arrangement playback guards.
- Recording: stopped/playing, armed/count-in/free take, overdub and roll. Editing
  an existing step must not secretly start recording or a free take.
- Kit/sound changes: preserve all patterns, intended mix and section references;
  unavailable banks explained; correct lane-to-recipe mapping and hat choke.
- Native/browser screenshots: inspect actual 240×240 legibility, step/playing
  LEDs, visible control labels and page indicators; retain melodic chord/snap
  and note-light behavior.
- Manual save/cold boot: current jam, edited recipes and A–D remain coherent;
  no autosave, implicit flash writes or new storage layout in navigation batches.
- Target build and audio: the shipped app has only 6,992 bytes of flash headroom,
  6,872 ordinary RAM spare and 14,132 pool spare, with existing reserve checks.
  Reuse rendering/DSP; measure every stage rather than assume a UI addition fits.
- Deliberate physical FM-1 trial before adoption; browser/host checks do not
  establish hardware knob, audio timing or power-loss behavior.

## Evidence and reproducibility

Current source anchors: [tap routing](../firmware/src/ui_input.c),
[page applicability](../firmware/src/params.c),
[custom GRID/KIT draw and input](../firmware/src/ui_studio.c),
[held controls](../firmware/src/ui_layers.c),
[recording, rolls and mutes](../firmware/src/seq.c),
[drum voices and panel lanes](../firmware/src/drums.c),
[synth recipes](../firmware/src/drum_synth.c),
[editor RAM/STORE policy](../firmware/src/editor_dsyn.c),
[manual saved payload](../firmware/src/jam_recovery_runtime.c).

Comparison uses the imported, pinned [Sloop manual](../SLOOP.md#the-drum-track)
and source at upstream `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`.
Its combined GRID/KIT route, held steps, performance layers and editor synth kits
are useful foundations. This is not a live test of current upstream Sloop.
Groove OS's site and an external product guide were blocked by the cloud proxy;
no new behavior claims about those products are made here.

Run the read-only host probe from the repository root (generated assets required):

```sh
mkdir -p build/drum-research-gen build/drum-research-screens
python3 tools/gen_icons.py build/drum-research-gen/felucca_icons.h
cc -O1 -fwrapv -fno-strict-aliasing -w \
  -Ibuild/drum-research-gen -Ibuild/gen -Ifirmware/src \
  docs/research/probes/drum-navigation-probe.c -o build/drum-navigation-probe -lm
build/drum-navigation-probe
```

[Probe source](research/probes/drum-navigation-probe.c) and
[observed routes](research/drums/navigation-probe.txt). Ordinary-page descriptor
counts establish UI availability, not a rendered DSP effect for every parameter.
No new device build, flash operation, external issue edit or publication was
performed for this investigation.
