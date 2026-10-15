# Drum kit controls — trial 03, October 15, 2026

This follows the owner-tested Trial 02 grid. SEQ keeps GRID; EDIT opens the
playable KIT SOUND pad map (Kit / Level / Tone / Pan); FX starts with KIT FX
(Filter / Echo / Room / Pan). Repeated FX reaches slicer, insert, insert mix,
ALL ECHO and ALL ROOM. The last two control shared buses. Ordinary left knobs
retain Home tempo, curated kits and displayed part order. Inactive synth pages
are skipped on Drums, with short tap guidance; useful held gestures remain.
The full kit knob offers 37 factory kits plus SYN1–4 and skips empty user samples.

Source: `793474c809334398951527c324494a46e5ec21c7`. The device build receipt
records clean source. Candidate firmware SHA256:
`ad26a098520dc55dbd0937f257b97e7eeba3751bc462025f907f31c07f824b83`.
Its 577264-byte app leaves 4300 bytes FLASH spare. RAM 91432/98304 and pool
329932/344064 remain unchanged; the canvas is unchanged. Target provenance,
ELF regions, package format and loader/container checks pass without warnings.

336 focused UI checks cover exact parameter namespaces, kit availability,
navigation, immediate keyboard ownership, held gestures and the save modal.
72 audio checks use actual production drum voices and effects across sampled,
factory synthesized and SYN kits; sends produce audible tails, all insert types
work, and zero effect amount bypasses correctly. Both fixtures pass GCC
ASan/UBSan. The existing 491 GRID checks, Home/save UI, 306 recovery runtime
checks plus its 17 fresh-process checks, upstream UI/20000-frame fuzz and full
browser emulator regression pass. Five Chromium scenarios use the production
panel, worklet, renderer and audio; native 240×240 screens were inspected.

The source catalog guard hashes all of params.c and drums.c, including their UI
code. Its original strict check correctly refused the new unreviewed digest.
After review, an exact catalog alias permits this compatible UI/browser change
while requiring the original runtime header and identical audio assets. The
persisted 64-byte identity, codec, storage writer, project/sidecar layouts and
sound IDs are unchanged. Twelve identity tests reject unknown catalogs, changed
assets, altered headers, malformed reviews and accidental --write rebinding.

The actual Trial 02 and current native adapters exchange synthetic saves in
both directions through independent OS-process cold boots: 17 existing checks
per direction, 34 total. Both writers produce byte-identical 1 MiB NOR images.
The canonical identity and ABI/core/engine preconditions are asserted. Only
logs/hashes are included here; neither synthetic NOR images nor private user
backup/report data is committed. Autosave remains off.

The normal installer, emulator, saving preview and main must remain unchanged.
Trial 02's exact 29-file package was retained and validated locally from pinned
website commit d246c1d8f70b85ac65be0d35cf3782d8fdfb563c. Its public Git history
remains the return reference. See [trial guide](../../DRUM_KIT_TRIAL.md).
Installer/publication outcomes are recorded separately below and in status.json.
No physical Trial 03 behavior is claimed before the owner's FM-1 check.

All 30 Chromium installer cases pass using the actual candidate package,
including the complete two-stage byte transfer, local-only backup/report flow,
disconnects, changed identities, delayed return and mobile layout. This is wire
simulation, not an instrument installation. Nineteen package checks and twenty
scoped publisher tests also pass.

Publication commit `acb8782a6b30c31ec6cfeb0226e3fd5478cd3df8` updates only preview/drums/. Independent
Git inspection verifies all 29 package files and every one of the 89 protected
objects/modes. Normal installer, emulator, saving preview and main are unchanged.
Five pinned raw-GitHub HTTP artifacts match the candidate bytes. Live Pages HTTP
is blocked from this cloud environment with CONNECT403, so live reachability
is not independently claimed. The page identifies the offered build as Trial 03.

The owner subsequently reported that Trial 03 looked good and requested the
SEQ bank shortcut; see [owner feedback](owner-feedback.md). Trial 04 implements
that small follow-up in the same isolated preview.
