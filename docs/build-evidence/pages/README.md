# GitHub Pages publishing evidence

Executed October 13, 2026. The native HTTPS Git connection successfully created
and updated the dedicated `gh-pages` branch. It uses the existing repository
connection; no new credential was requested or stored.

- [Publisher tests](publisher-tests.log): all five passed against local bare
  remotes, covering initial publication, an update preserving history, unchanged
  packages, concurrent changes, inherited Git path variables, and rejected
  unexpected files, symlinks, hashes and source-archive contents.
- [Initial Git dry-run](initial-dry-run.json), [initial upload](initial-publish.json)
  and [remote comparison](initial-remote-verify.json) verified the real repository.
- [Final package build](final-package.log) identifies the rebuilt site after the
  publishing documentation was finalized. [Final upload](final-publish.json)
  identifies the update on top of the initial deployment commit.
- [Final remote comparison](final-remote-verify.log) confirms the published
  site matches the local package and records both remote branch tips.
- [Access checks](access-check.json): GitHub repository metadata was readable;
  both reading and creating Pages settings returned GitHub HTTP 403, `Resource
  not accessible by integration`. The expected public page returned HTTP 404.
  This is a GitHub integration capability limit, distinct from the earlier
  proxy CONNECT denial.

The site files are uploaded, but a live GitHub Pages deployment was not verified.
The remaining one-time action is in [repository Pages settings](https://github.com/sprintrstudio/jamboxfm/settings/pages):
**Deploy from a branch → gh-pages → /(root) → Save**. Private repository Pages
requires an eligible GitHub plan. Routine updates use the tested Git connection.
See [publishing instructions](../../GITHUB_PAGES.md).

## Fresh source checkout

The editable source was pushed to `main` at
`d40de1a87ec2f581e653a8ec77356bf5ba88a0f7`. A fresh shallow clone of that exact
commit compiled successfully with the external toolchain, packaged the site,
and passed all five publishing tests. Its WASM matched the original build
byte for byte: 679,279 bytes, SHA256
`88f52ef09404069593fe36f4a2304a821f32f14190e353cfbd18d070f9dd99da`.
See [build log](fresh-source-build.log), [comparison](fresh-source-wasm.json),
[package log](fresh-source-package.log) and [tests](fresh-source-tests.log).

The remote default branch remains `gh-pages`: changing repository settings
also returned GitHub's integration permission error. The saved cloud
configuration already selects `main`, and new source clones should explicitly
select that branch. Repository visibility was not changed.

The emulator's native, browser, audio and source-rebuild validation is recorded
separately in [the emulator evidence](../emulator/README.md). This publishing
work changes delivery tooling and documentation; no FM-1 binary was produced.
