# JamBoxFM repository workflow

The active public repository is `anthonysprints/jamboxfm`. Use the existing
checkout for cloud tasks.

## Stable version and development branch

The owner requested isolation for the saving and usability cleanup because the
current instrument is working well. Keep `main` as the stable source and do this
work on `next/usability-and-saving`, starting from
`99cbf62495d9f021be80ee3d20fcef739dd8c93e`. The live website is generated on
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
production publisher is still prohibited for development work. A Pages push
does not establish live HTTP availability or physical device validation.

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
