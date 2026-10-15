# JamBoxFM repository workflow

The active public repository is `anthonysprints/jamboxfm`. Use the existing
checkout for cloud tasks.

## Stable version and development branch

The owner requested isolation for the saving and usability cleanup because the
current instrument is working well. The owner confirmed that Save now survives
full power-off and restores the jam and edited sounds, then explicitly approved
making it the normal Chrome installer version. The ordinary `--with-updater`
build now enables `JAMBOX_RECOVERY=1` with legacy `FELUCCA_PERSIST=0`.
Use `tools/package_jam_installer.py` and `tools/publish_jam_installer.py` for
normal installer updates; they preserve the browser instrument, development
preview and unrelated public files. The owner-approved firmware for this
promotion has SHA256 b57fcb452c05933b5aa895840067d18db10f70ee3f70c8d35e15bf760e10f4c7.
Owner-reported manual cold-boot success does not prove every recovery scenario.
Do further usability work on `next/usability-and-saving`. The pre-saving source
baseline is `99cbf62495d9f021be80ee3d20fcef739dd8c93e`. The live website is generated on
`gh-pages`; its baseline for this work is
`c4896b796312e8b26a9c01206d25bdc2d8b93589`.

Follow [roadmap #10](https://github.com/anthonysprints/jamboxfm/issues/10) and make
small, independently reviewable changes tied to its issues. Preserve
the four-part starting jam and access to deeper sound editing. Check the relevant
native/browser behavior and firmware resource budgets, and arrange a deliberate
physical FM-1 trial before treating device behavior as verified. Saving also needs
interrupted-write, corrupt-record and cold-boot recovery checks.

A source branch does not isolate publication: `tools/publish_jam_web.py --publish`
always targets the live `gh-pages`, even when run on a development branch. Do not
publish development builds over the live site or installer. Use local checks;
design a separate preview destination before sharing a development preview.
Merge into `main` and publish only after validation and the owner is ready to adopt
the changes. Follow `docs/GITHUB_PAGES.md` for an authorized production release;
its older references to developing on `main` do not override this branch policy.

The owner subsequently authorized a separate manual-saving trial preview at
`preview/saving/`. Use `tools/package_jam_recovery_preview.py` and
`tools/publish_jam_recovery_preview.py` for it, following
`docs/FM1_SAVING_TRIAL.md`. Only that subtree may change on `gh-pages`;
every stable root file and all other paths/modes must remain identical. The
full-site production publisher is still prohibited for development work. A Pages push
does not establish live HTTP availability or physical device validation.

The owner also approved the staged drum workflow. Its first entry/recording
increment has a separate `preview/drums/` trial. Use only
`tools/package_jam_drums_preview.py` and `tools/publish_jam_drums_preview.py`,
following `docs/DRUM_GRID_TRIAL.md`. Preserve the normal installer, emulator,
`preview/saving/` and every other outside path/mode. The owner reported that the
first drum trial worked, then confirmed that Trial 02 worked through their
testing and looked good. These are owner reports, not independent or itemized
hardware validation. Both stages remain on the development branch and isolated
preview; further cleanup and normal-installer adoption remain separate steps.

The owner approved the next KIT SOUND / KIT FX cleanup on the same isolated
drum preview; follow `docs/DRUM_KIT_TRIAL.md` for Trial 03. Keep the canonical
manual-save identity unchanged. Whole-file catalog hashing also covers UI code,
so this compatible change has an exact reviewed catalog alias in
`firmware/experimental/recovery/jr_catalog_compatibility.json`. Unknown catalog
changes and changed audio assets must still fail the identity check. Do not use
`check_jam_recovery_identity.py --write` to bypass a compatibility failure:
replacing the runtime identity would reject existing checkpoints. Check the
identity refusals and actual Trial 02/current cold restore in both directions.

The owner chose manual saving for the current product. Keep automatic saving
disabled and outside the active plan unless the owner explicitly reopens it.
The saved checkpoint changes only through an intentional Save now; unsaved
experiments must not overwrite it. Restart restores the last valid manual save.

## Commit attribution

Before creating source or website commits, set the repository-local identity
to the owner's GitHub account:

```sh
git config --local user.name anthonysprints
git config --local user.email 6182444+anthonysprints@users.noreply.github.com
```

The authenticated GitHub connection and Git's author identity are separate.
The old environment default email attributed commits to another account.
Use the account-linked noreply address above for future commits. The website
publisher reads this local identity. Preserve history during routine updates.
