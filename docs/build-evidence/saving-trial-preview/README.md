# Separate saving-trial publication

The source implementation is `73df99c` on `next/usability-and-saving`. The
scoped publisher added only `preview/saving/` to `gh-pages` in `a6e8a232`.
Independent remote inspection compared all 29 preview files and confirmed that
all 56 preexisting paths outside the preview have identical bytes, modes and
Git objects. `main` remains `99cbf624`.

The final 611,255-byte firmware has SHA-256
`b57fcb452c05933b5aa895840067d18db10f70ee3f70c8d35e15bf760e10f4c7`.
Its clean-source build still passes compiler/linker/MMIO/RAM-driver limits; app
574,572 bytes, RAM 91,432 bytes and pool 329,932 bytes. The published bundle
contains its exact corresponding source, receipt, memory report and notices.

Thirty distinct Chromium cases pass across the full and final focused runs.
See browser-outcome.json for which versions/runs were exercised. Existing
normal policy/controller tests pass35; packaging13+18 and publisher16 pass.
Previously supplied private backup pairs also pass the production browser-local
validator. Neither private data nor their identifying digests are stored here.

Raw GitHub HTTP artifacts match the prepared preview. The cloud proxy returns
CONNECT403 for the Pages address despite its saved allowlist entry, so Pages
availability remains independently unconfirmed. Repository publication is not
a claim of live HTTP availability. An asynchronous user page check was requested.

No physical device was flashed or firmware executed in these tests. Manual
saving and startup restoration still require the small owner trial documented
in [FM1_SAVING_TRIAL.md](../../FM1_SAVING_TRIAL.md). Automatic saving remains off.
