# Unchanged Sloop baseline PWA check

Baseline commit: `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`.
Checkout: `/workspace/scratch/fm1-research/sloop`.
The existing full-suite output generated `build/host/pwa-site`.

The baseline test first tries `/opt/pw-browsers/chromium`, then Playwright's
default browser cache. Neither executable exists in this environment. The
external `pwa-system-chromium.mjs` preload changes only the executable passed
to the actual resolved Playwright `chromium.launch` method to
`/usr/bin/chromium`. It retains all other options. It does not replace browser
contexts, intercept protocol responses, modify any assertion, or edit the
baseline checkout. It uses the same NODE_PATH fallback resolution as the test.

Versions recorded during the run:

- Node: `v24.19.0`.
- Playwright: `1.62.1`, resolved at
  `/opt/codex/runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.js`.
- Browser: `/usr/bin/chromium`, `Chromium 154.0.8037.57 built on Debian GNU/Linux 13 (trixie)`.
- NODE_PATH: `/opt/codex/runtimes/codex-primary-runtime/dependencies/node/node_modules`.

The exact original invocation is recorded in `pwa.environment.json`. Repeat
the affected test using the retained adapter (after generating the baseline site):

```sh
cd /workspace/scratch/fm1-research/sloop
node --import /workspace/jamboxfm/docs/build-evidence/device-build/pwa-system-chromium.mjs \
  tests/pwa_test.mjs build/host/pwa-site
```

Result: exit **1**; **14 of 15 assertions pass**. Browser launch is resolved.
The unchanged installability assertion reports `in-incognito` from Chromium's
`Page.getInstallabilityErrors`. The test creates a nonpersistent context with
`browser.newContext()`. This is a remaining browser/context compatibility
failure, and is retained rather than hidden. Editor/installer loading,
service-worker registration/control, offline firmware/editor/font access,
query-path fallback, unknown-page rejection, online cache refresh, and
service-worker cache replacement all pass.

`pwa.log` contains stdout/stderr including effective launch options and
Playwright version. `pwa.exit` records the status. `pwa.environment.json`
records the command, browser version, git status before/after, unchanged
tracked-file hashes, generated-fixture restoration status, and subprocess
cleanup. The detailed baseline file hashes and subprocess supervisor remain
under `/workspace/scratch/fm1-device-build/baseline/`.
Git status was empty before and after, all tracked-file hashes match, the
test restored both generated fixtures itself, and all subprocesses belonging
to this rerun were reaped. Existing unrelated processes were not touched.

This reruns only the PWA test, not the full host suite. The independent
baseline IRQ-budget failure is outside this check. No MIDI or flashing is
performed.
