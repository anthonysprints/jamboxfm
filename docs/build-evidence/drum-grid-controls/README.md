# Drum GRID controls — trial 02, October 15, 2026

The owner reported that trial 01 worked. That broad report does not validate this
new candidate. Trial 02 adds eight named rows, fixed editing windows with ranges
and off-page playhead marks, idempotent SEQ and EDIT→GRID/KIT. The four controls
are Drum / Step / Strength / Repeat; ordinary left knobs retain Home tempo,
curated-kit browser and displayed part order. Inputs retain immediate keyboard
ownership and recorder/modal priority. One selected drum is shared across live,
grid and held-step paths; the held editor starts/returns the same step bank.

Native tests cover all 16 lanes, lengths 1/16/17/32/64, all valid banks, scoped
strength/repeat, undo/no-op safety, auditions, kit/tempo/part preservation,
shrunk lengths, arrangement guards, save-input resets and held release hazards.
GCC ASan/UBSan passes. Four Chromium scenarios execute the production panel,
AudioWorklet, C audio/input and renderer; the normal emulator regression passes. All 30 installer wire/browser checks
and 20 scoped packaging/publisher checks pass.
The upstream native UI and 20000-frame fuzz also pass, with its behavior gated
separately from JamBox. The actual target, pinned compiler/SDK provenance,
ELF/memory and canonical firmware-package gates pass.

The eight 16px rows render as two 64px strips; the existing 124px canvas is not enlarged.
Ordinary RAM and pool use remain unchanged. The journal schema, payload and
writers are unchanged; the UI preserves its transient selected drum across the
writer's existing input reset. Autosave remains off. Raw runtime recovery and
save-modal regressions still pass.

Keep normal installer, emulator, saving preview and main unchanged. The same
fixed `preview/drums/` publisher updates only that subtree. Record actual installer
checks, publication, independent remote bytes/object preservation and HTTP
outcomes separately. No host/browser test establishes physical control, audio
deadline or power-loss behavior. The owner subsequently reported that Trial 02
worked through their testing and looked good; see [owner feedback](owner-feedback.md).
That positive hands-on report is distinct from independent hardware validation
or a normal-installer promotion. No private backup/report or manufacturer binary
is included.

See [current trial guide](../../DRUM_GRID_TRIAL.md). Source revision in status.json
identifies the clean target/source bundle; later evidence-only commits do not
change that provenance.

Publication updated only `preview/drums/`. Independent Git inspection confirms
all 29 package files match and all 89 protected entries/modes are unchanged.
Five raw GitHub HTTP artifacts match. The cloud proxy still blocks live Pages
HTTP with CONNECT403; live reachability is not independently claimed. The normal
installer, emulator, saving preview and main remain unchanged.
