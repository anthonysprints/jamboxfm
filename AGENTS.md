# JamBoxFM repository workflow

The active public repository is `anthonysprints/jamboxfm`. Use the existing
checkout for cloud tasks.

## Adopted firmware and development

On October 15, 2026 the owner approved moving the individual-drum editor
through Trial 05 to `main` and the normal Chrome installer. The adopted firmware
is 611255 bytes, SHA256
a88970d9e601aed4042e28302d9ada19357b4ac549859a479f8eee5fba04b351.
Its owner-used clean source was 4c04594; a promotion rebuild must match those
exact bytes. See `docs/DRUM_RELEASE.md` for source, evidence and delivery status.
Trial 04 used 77812285810b5c0494d94783d63d49c747e5e918c3815a689c66f4fe67e3cb90.
Owner acceptance is not universal hardware/recovery validation.

Use `main` for the adopted source. Keep new usability experiments on
`next/usability-and-saving` until validation and owner adoption. Preserve the
four-part starting jam and deeper sound controls. Follow roadmap #10 and make
small reviewable changes. Native/browser checks and target resource budgets
must remain valid; device behavior needs a deliberate owner trial.

The drum workflow is SEQ for GRID, EDIT for KIT SOUND and FX for useful kit
controls. Another SEQ tap inside GRID swaps drums 1–8 / 9–16, preserving row
position and step window. Entry from another screen preserves the selected
drum. Held gestures, recorder overlays and manual-save modals retain priority.
ENV now opens individual synthesized drum editing. Factory kits require an
explicit copy to a chosen SYN bank, with overwrite/shared-section guidance.
Sampled kits retain whole-kit EDIT/FX. See `docs/DRUM_SOUND_TRIAL.md` and
`docs/DRUM_KIT_TRIAL.md` for controls and historical trials.

The owner chose manual saving. Automatic saving remains disabled and outside
the active plan unless explicitly reopened. Only intentional Save now replaces
the checkpoint; restart restores the last valid manual save. Keep the canonical
saved identity unchanged for compatible UI edits. The exact reviewed catalog
alias is `firmware/experimental/recovery/jr_catalog_compatibility.json`.
Unknown catalog edits or changed audio assets must fail the identity check.
Do not use `check_jam_recovery_identity.py --write` to bypass a failure: replacing
the runtime identity rejects existing checkpoints. Check identity refusals and
actual old/current cold restore when compatibility changes require review.

The separate `next/sound-page-cleanup` branch is experimental. Keep its new
FM6/modulation pages out of the normal installer until owner adoption. Its test
installer must use a dedicated `preview/sound/` destination.

## Publishing

The generated site lives on `gh-pages`. A source branch alone does not isolate
publication. For normal-installer releases use `tools/package_jam_installer.py`
and `tools/publish_jam_installer.py`, following `docs/DRUM_RELEASE.md` and
`docs/GITHUB_PAGES.md`. Preserve the browser instrument, both previews and every
unrelated Git path/object/mode. The ordinary `--with-updater` profile enables
JAMBOX_RECOVERY=1, FELUCCA_PERSIST=0, FLASH and OTA. Reuse the exact approved
binary when promoting a tested candidate; verify its hash and receipt.

Development saving and drum previews remain separately scoped to
`preview/saving/` and `preview/drums/`. Use their dedicated package/publish tools
only for the corresponding preview. Never use `tools/publish_jam_web.py` to
publish development over the normal site. Merge/promote to main and publish
normal updates only when the owner has authorized adoption. The owner has
explicitly authorized this drum adoption. A Git push does not prove live Pages
availability; verify remote artifacts and report live HTTP access separately.

## Commit attribution

Before creating source or website commits, set repository-local identity:

```sh
git config --local user.name anthonysprints
git config --local user.email 6182444+anthonysprints@users.noreply.github.com
```

GitHub authentication and commit author identity are separate. Preserve history
and keep the account-linked noreply address above for future commits.
