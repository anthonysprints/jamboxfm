# Separate sound-controls test installer

The owner requested all drum work in the normal installer and a separate
installer for the newer FM6/modulation cleanup. The normal release is the
adopted Trial 05 firmware SHA256
`a88970d9e601aed4042e28302d9ada19357b4ac549859a479f8eee5fba04b351`.
Its main adoption is recorded in [drum-sound-release](../drum-sound-release/README.md).

The separate test URL is:
https://anthonysprints.github.io/jamboxfm/preview/sound/install/

It offers the exact previously validated cleanup candidate, SHA256
`b3f31704d063f63d79b8019b5e166e2677bf39df9e9a9b4bddb86a7a887ff99a`,
611255 bytes, compiled cleanly at `728a0868836b57651ba4f76b39a5f013c47ee363`.
The corresponding-source archive includes the current installer and scoped
publish tools. Stock input/generated-header/ELF/container gates verify that
reusing the retained application is compatible with the current source.
No unrelated or untested rebuilt firmware is substituted.

The trial includes all drum features and the new melodic cleanup. The normal
installer keeps the adopted drum firmware. Users can update directly from
currently running JamBox; Save now first and retain the existing saving setup.
Both packages keep manual saving, compatible checkpoints and autosave off.

The new publisher is fixed to `preview/sound/` and preserves every outside Git
object/mode. Twenty-five local safety tests cover normal/emu/saving/drum trees,
odd paths, executable and symlink modes, empty trees, conflicts, wrong targets,
source/runtime mismatches, default dry run and non-forced concurrent updates.

Thirty production-controller Chromium cases pass with the packaged candidate,
including source-family rejection, forged backup refusal, stale/duplicate MIDI
pairs, cancellation, transfer failures, complete simulated transfer, final
identity, private local report, delayed response and desktop/mobile layout.
They additionally check that Sound controls trial labels survive mount, bundle
verification, device inspection and completion. These checks do not perform
physical installation. Device acceptance remains pending.

Website commit `6ecfccd149287c22b8a6e64fc45d944dd82136c3` adds only the separate
preview. Independent fresh-Git verification compares all 29 packaged files and
all outside objects/modes, including the now-updated normal installer and its
firmware. Five raw HTTP artifacts and live Pages availability are recorded
separately. See the JSON evidence for actual outcomes; source/site publication
is distinct from live HTTP and physical device behavior.
