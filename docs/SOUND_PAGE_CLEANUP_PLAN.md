# Melodic sound-page cleanup: investigation branch

Started October 15, 2026 at the owner's request for a separate branch while
Trial 05 drums remain under owner testing. Branch: `next/sound-page-cleanup`.
Baseline: `0e69bbd5fdb56e0739d8ab1155853ce161d20cf8`.

## Isolation

- `main` remains the adopted Trial 04 release.
- `next/usability-and-saving` remains the Trial 05 drum source/evidence baseline.
- The existing drum preview installer remains Trial 05; this branch must not
  publish over it while the owner tests that version.
- This initial commit is planning only. No firmware, save behavior or installer
  changes. Do not interpret a branch push as device or publication validation.
- Preserve manual Save now, existing checkpoint identity, factory assets,
  editable SYN banks, four tracks, recording and held shortcuts.

## Recommended next slice

Make Bass, Chords and Lead sound editing easier to understand before adding
further engines, tracks or storage features. Keep useful depth; reduce confusing
navigation and controls that have no effect. Begin with EDIT, then audit ENV,
LFO and FX using the same method. Do not assume an advanced feature is useless
merely because its name is unfamiliar.

## Source findings (not yet an audible-control audit)

- `params.c` defines generic EDIT 1 / EDIT 2, an Analog-only FILTER page, VOICE
  and VOICE 2. Engine parameters are resolved through `track_desc` rather than
  one universal sound-control layout.
- `page_shown` already restricts the Analog FILTER page and drum-specific pages.
  Most melodic page visibility is otherwise generic. This does not establish
  that the controls are ineffective; trace each engine and exercise its audio.
- `open_family` remembers the last page in each family; repeated button taps
  cycle pages. `page_walk` uses SELECT to walk multi-page families. SELECT is
  BPM on Home and ordinary drum workspaces. Investigate whether inconsistent
  SELECT behavior is useful before proposing a change.
- FX includes per-part processing and global echo/room controls. Drum pages
  already label shared buses ALL ECHO / ALL ROOM; melodic FX still includes
  REV/CHO. Distinguish shared scope in any proposed labels.
- The system menu explicitly shows USB SERIAL as UNAVAILABLE in the shipped
  no-legacy-persistence profile. Audit its real configuration/boot path before
  deciding to remove or enable it. This is a later menu issue, not an automatic
  addition to the first EDIT change.

## Investigation deliverables

1. Drive the real panel/LCD path for each engine used by the current curated
   melodic presets. Inventory reachable EDIT pages, knob names/ranges, page
   cycling and track changes. Include engine changes while a page is open.
2. Separate deliberate blank slots, conditional controls, true unavailable
   controls and working controls with unclear names. Confirm disputed controls
   against production DSP/audio; a non-null parameter descriptor is not proof.
3. Propose an engine-specific first EDIT screen with recognizable names and
   useful four-knob groups. Explain each control in beginner language. Keep
   deeper voice, modulation and effect settings reachable.
4. Record the proposed button/knob map and scope before changing behavior.
   First implementation should address one small category, not rearrange all
   pages at once.

## Implementation gates

- Do not alter sound defaults, recipes, parameter IDs, engine order, save layout
  or canonical checkpoint identity just to clean up navigation.
- The Trial 05 application has 1912 bytes of flash spare; reuse existing page
  structures and rendering. Measure a target build early. Maintain required
  RAM/pool reserves rather than assuming UI code is free.
- Verify first-key audition, held-note release, chord assistance, recording,
  Home returns, SAVE modal ownership and held layers after navigation changes.
- Preserve Trial 05 drum workspaces and copy confirmation under melodic/drum
  track switching. Keep the actual sound editing and saved SYN state intact.
- If source catalog files change for UI-only reasons, perform explicit identity
  compatibility review and old/current cold restore. Never regenerate the
  saved identity to bypass the check.
- Native/browser tests establish software behavior; a new owner device trial
  is needed before adopting changes. Choose a separately isolated publication
  destination before any preview deployment; do not reuse the active drum
  trial implicitly.

Related existing backlog: #6 controls match the sound; #9 everyday navigation;
#10 staged usability roadmap. This local plan does not change GitHub issue status.
