# Two Home views: Sound and Groove

Status: design proposal, 2026-10-14. This document develops part 2 of
[First device feedback](FIRST_DEVICE_FEEDBACK.md#2-sequencer-prominence-two-home-views-planned).
No firmware or installer change is included in this planning update.

## 1. Playing experience

Keep the existing sound-focused Home and add a sequencer-focused Home:

| View | Main purpose | What stays visible |
| --- | --- | --- |
| SOUND | Choose a sound and shape it with the four macros. | Tempo, transport, part selection, large sound name and macro values. |
| GROOVE | See how the four patterns fit together while playing. | Tempo, transport, four step rows, selected sound and compact versions of the same macros. |

These are two views of the same live session. Switching does not change the
selected part, notes, sound, pattern, tempo, recording, or knob assignments.
Both views keep the keyboard playable, including chord building and SNAP.

The owner's navigation rule is the central decision:

- Tap HOME on SOUND: show GROOVE.
- Tap HOME on GROOVE: show SOUND.
- Tap HOME from a normal sound/scale/sequencer page: return to whichever Home
  view was used last. Do not immediately toggle it.
- Returning Home through another action, such as leaving the drum editor,
  also restores that remembered view.

Recommend GROOVE at power-on because it puts the beat and recording in view
for a beginner. This startup preference is proposed, not an approved firmware
change. Remember subsequent choices for the current powered session. Changing
sounds or starting a fresh musical session should not reset the Home view.
Remembering it across power cycles belongs to the later settings-storage work.

Example: GROOVE → HOME → SOUND → ENV → HOME returns to SOUND; the next
HOME tap switches to GROOVE. A small SOUND/GROOVE heading and a footer such
as `HOME: SOUND` make both the current view and the next action discoverable.

## 2. Controls and gesture priority

| Control | Action in either ordinary Home view |
| --- | --- |
| MASTER | Output level. |
| SELECT | Tempo, one BPM per detent, 40–240 BPM. |
| ALGORITHM | Select Drums, Bass, Chords or Lead. |
| PRESETS | Change the selected part's sound, preserving its pattern. |
| Knobs 1–4 | The same four named sound macros in both views. |
| Note keys | Play notes/chords or drum pads; GROOVE does not turn them into step switches. |
| PLAY / REC | Existing transport and recording actions. |
| OCT− / OCT+ | Existing octave/part-specific behavior. |
| SEQ | Existing sequencer pages/layer; explicit editing remains separate. |

The overview's compact macros must still have labels and values. Do not show
the inherited TRACKS footer's swing/level/steps/pan controls while the knobs
actually operate sound macros. Unsupported sound engines retain the current
`EDIT` / `--` indication instead of presenting working macros that do nothing.

HOME needs the following precedence, beyond the simple navigation rule:

1. A hold still opens/closes settings after the existing approximately 700 ms
   threshold. Its release must never count as a view-switch tap.
2. For consistent navigation, propose a short HOME tap in the settings menu
   closing it to the remembered Home view. Today that short tap does nothing.
   Route it through `menu_close()` and consume it; do not toggle afterward.
3. Held SEQ + HOME retains its existing layer-lock action. HOME used to
   unlock a held layer is consumed by that action, without switching Home
   views. Preserve the other held-button combinations and release orders.
4. Armed-recording, free-take, count-in and hold-to-confirm overlays retain
   their input/display ownership. HOME must not secretly change the remembered
   view behind them, cancel a take, or confirm a destructive action.
5. Ordinary playback and overdubbing, with Home actually visible, allow
   toggling freely while notes and audio continue.

Classify HOME ownership when it is pressed and check it again on release.
If another layer owned any part of that gesture, do not reinterpret the
release as an ordinary Home toggle. Test HOME together with PLAY: the current
layer handler intentionally lets PLAY continue through some early returns.

## 3. A readable 240×240 layout

The earlier proposal squeezed the sequencer into the only Home screen. Two
views let SOUND retain its large controls while GROOVE gives patterns space.

![Conceptual Sound and Groove Home layouts](design/home-views.svg)

The drawing is a static planning wireframe, not a screenshot or working UI.
Its typography budgets use the existing small font's 8×16 character cells.

| GROOVE screen region | Height | Content |
| --- | ---: | --- |
| Header, y 0–31 | 32 px | GROOVE, BPM, play/stop and recording state. |
| Four rows, y 32–143 | 112 px | Drums, Bass, Chords, Lead; 28 px each. |
| Selected sound, y 144–175 | 32 px | Selected role and readable sound name. |
| Four macros, y 176–215 | 40 px | Four columns with labels, values and compact bars. |
| Footer, y 216–239 | 24 px | `HOME: SOUND  HOLD SEQ: EDIT`, or a temporary status message. |

Each row has a 16 px text line and a small 16-cell step strip. Use the full
224 px inset width for the strip: 16 cells at 14 px pitch, with visible gaps
and beat-group accents every four steps. Selection uses an outline/side mark
as well as color; playhead, stored notes and mute must be distinguishable.
Long names truncate safely within their own region. No scrolling text is
needed for the first version.

## 4. What the sequencer display means

- Each cell is one **actual stored step**, not a compressed summary of several
  steps. Filled cells show programmed notes/hits; a different line mark shows
  melodic ties; empty cells show no event. Out-of-length cells are disabled.
- The drum row summarizes whether any drum lane has a hit on that step.
  Individual kick/snare/hat editing stays in the drum/SEQ editor.
- Every row shows its own range and total length, for example `01-16/32` or
  `17-32/64`. Support all current lengths from 1 to 64, including a partial
  final window. These are step ranges, not guaranteed musical bars.
- While playing, each row follows that part's actual playhead and 16-step
  window. Parts can have different lengths and step divisions. Show the DIV
  value when it differs from the usual 1/16; do not imply a shared time scale.
- On stop, retain the last displayed window for each row. Before any playback,
  show its first window. Clamp a remembered window after a length change and
  reset it for a genuinely replaced pattern/session. Do not reuse or move the
  step editor's cursor to achieve this.
- Keep stored cells visible when muted. Show explicit mute, solo and recording
  separately from the selected-row marker. Also dim parts excluded by another
  solo or with zero level. Do not relabel every silent part as manually muted.
- A playhead shows sequencer position; a stored cell does not promise an audible
  hit. Conditions, muted drum lanes, swing/microtiming, arpeggiation and live
  input can change what is heard. Existing keyboard LEDs show playing notes;
  a full audio-event visualization is outside this first layout increment.

The first GROOVE screen is a performance overview, not a piano roll. It does
not need chord names, individual note pitches, velocity editing or a second
editing cursor. Holding SEQ shows the existing editing layer; holding SEQ and
tapping HOME locks that layer, as today. A quick SEQ tap still opens the
remembered STEP/PATTERN/SONG page. Show these differences in the help text.

## 5. Implementation boundaries

| Area | Planned change |
| --- | --- |
| [UI state and `go_home`](../firmware/src/ui.c) | Add a small RAM-only Home-view enum. Both views keep `ui.home == 1`; `go_home()` returns without toggling. Preserve its ordinary exit cleanup. |
| [HOME input](../firmware/src/ui_input.c) | Handle deliberate view switches only in the JamBox ordinary-tap path, with press/release ownership and consumed-gesture checks. Add the explicit menu-return behavior above. |
| [Home renderer/input](../firmware/src/jam_ui.c) | Keep one guarded control handler, dispatch two cached renderers, and invalidate their caches on view changes. |
| [Existing TRACKS view](../firmware/src/ui_studio.c) | Reuse row-drawing ideas and pattern helpers, not its page scope, controls or stopped-pattern folding behavior. |
| [Draw precedence](../firmware/src/ui_draw.c) | Preserve menus, recorder and layer precedence ahead of Home. |
| [Browser adapter](../firmware/host/jam_emu.c) / [worklet](../web/emu/worklet.js) | Append an explicit Home-view value for browser labels/tests while preserving existing `home` and control-ownership fields. Both screens still come from shared C. |

Keep Home out of `FAM_SEQ`, `SC_STEP` and `SC_DRUM`: those scopes change what
keys and controls do. Retain the existing role-to-track mapping rather than
assuming visual row order equals internal track order.

Use the existing 240×124 strip canvas. Draw/copy one bounded strip or row at
a time, with per-row change detection; no new full-screen framebuffer or
audio-thread work. Include every visible distinction in cache keys: selection,
length/window, playback position, division, mute/solo/record state, note/tie
occupancy and displayed macro/sound values. Do not keep interrupts disabled
through hashing, drawing or LCD transfer.

## 6. Delivery order and acceptance

1. Review the two-view wireframe and navigation contract. Keep the startup
   view as a clearly identified preference, not a hidden consequence of enum
   numbering. Use native-size and enlarged previews to assess legibility.
2. Implement the view state and HOME navigation with the existing Sound screen
   retained. Verify ordinary taps, returns, menu exit, holds, layer locks and
   both release orders before adding the pattern renderer.
3. Add GROOVE rows, compact macros, transport/recording states and truthful
   16-step windows. Keep controls and the musical keyboard identical in both.
4. Extend the existing Home/native/browser harnesses. Verify toggling while
   holding single notes/chords and during playback/overdub; compare musical
   state and recorded patterns before/after. Test all return routes, repeat
   taps, simultaneous HOME/PLAY, recorder overlays and unsupported macros.
5. Check 1/15/16/17/32/64-step patterns, unequal lengths/divisions, ties,
   partial windows, empty/muted/solo tracks, selection changes and cache
   invalidation. Verify no fake playhead while stopped, no stale screen
   fragments, bounded LCD transfers and stable audio under rapid switching.
6. Build the updater-enabled FM-1 target and browser from the same source;
   verify memory fit, installer packaging and published bytes. Then audition
   both views and HOME gestures on the physical unit.

This planning change does not deploy new firmware. Persistence, a redesigned
step editor, arrangement tools, per-drum-lane Home rows and new knob modes are
separate later work. The first useful result is being able to watch the band,
tap HOME to shape its sound, and return without interrupting the jam.
