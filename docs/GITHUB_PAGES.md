# Publishing JamBoxFM to GitHub Pages

Use the public `anthonysprints/jamboxfm` repository and a dedicated `gh-pages`
branch containing only the generated site. The source checkout and the inherited Sloop installer
remain separate from the publication branch. The page's relative asset URLs
work under the repository path, and `.nojekyll` keeps the generated site intact.
Use `main` when checking out the editable JamBox source for a future session.
The source branch includes the JamBox build inputs and research/test evidence;
the reserved upstream artwork and inherited generated deployment stay out of it.

## One-time setup

Keep `anthonysprints/jamboxfm` enabled in the Codex GitHub connection for
future sessions. Use the cloud environment's existing HTTPS Git connection
and the publisher's dry-run to verify access to this destination. The
publisher targets only this repository; the previous repository is no
longer the operational destination. Historical deployment evidence under
`docs/build-evidence/pages/` describes that earlier setup.

The cloud environment needs these destinations in its network configuration,
alongside its existing Git, device-compiler and package-manager access:

- `api.github.com`: inspect/configure Pages and check build status.
- `anthonysprints.github.io`: verify the published page and its assets.

On a new environment, review and save the settings, then publish the environment
configuration if these network additions have not been applied. A proxy
`Tunnel connection failed: 403 Forbidden` is a network failure, whereas
GitHub's JSON `Resource not accessible by integration` is an authorization
failure. Git push permission and permission to configure Pages are separate;
check the specific operation before considering another credential.

After the `gh-pages` branch has been pushed, open
[repository Pages settings](https://github.com/anthonysprints/jamboxfm/settings/pages)
and select **Deploy from a branch**, **gh-pages**, **/(root)**, then **Save**.
Changing this setting requires repository admin or maintainer access (or a
custom role with permission to manage Pages settings). Routine updates need
repository push permission, without continuing admin access to Pages settings.
The destination repository is public, so Pages does not require a plan
that supports Pages for private repositories.

The expected project address is `https://anthonysprints.github.io/jamboxfm/emu/`.
Treat it as a destination until an actual HTTP check succeeds. No custom domain,
DNS change or third-party hosting account is required.

## Publish the normal manual-saving installer

The owner approved manual saving for the normal installer after confirming a
full power-off/restoration on their FM-1. For installer-only updates, follow
[the manual-saving release workflow](MANUAL_SAVING_RELEASE.md): use
`tools/package_jam_installer.py` and `tools/publish_jam_installer.py`.
These preserve the browser instrument, preview and unrelated site files while
updating the installer, firmware/source downloads and checksums. The older
full-site workflow below replaces the site and is not used for this release.

## Publish a full site update

From the existing source checkout:

```sh
. tools/cloud/activate.sh
sh tests/run_jam_emu_tests.sh
JAM_DEVICE_SITE_ROOT="$PWD/build/jambox-web" node tests/jam_device_browser.mjs
python3 tests/test_publish_jam_web.py
python3 tools/publish_jam_web.py
python3 tools/publish_jam_web.py --publish
```

The test runner builds the current emulator and its publication package.
The first publisher invocation performs a Git push dry-run; the second
publishes the same validated package. Use `--publish` when the user requests
a website deployment or update. Publishing is not a startup action.

The default public package includes the browser instrument at `/emu/` and the read-only device check at `/device/`. Its exact 27-file allowlist excludes firmware and installation pages. To include the experimental factory-V15 installer, build the updater target, then run `python3 tools/package_jam_emu.py --with-trial` (add `--no-build` only when the emulator is already current). This creates a separate exact expanded file set, including `/install/`, the bound candidate, corresponding source and licenses. The publisher checks both forms and rejects unexpected files. See [Chrome trial checks](FM1_CHROME_TRIAL.md); keep raw backups and manufacturer firmware out of both forms.

The publisher validates the exact static-site file list, hashes and included
source archive, creates a temporary deployment checkout, and commits the
current package on top of the existing `gh-pages` history. It does not alter
the source checkout, force-push or upload the inherited `docs/` site. A remote
branch race fails instead of overwriting someone else's update. An unchanged
package needs no new deployment commit.

Future agents should reuse the platform's HTTPS Git proxy. A successful Git
read proves neither push permission nor GitHub API permissions; use the
publisher's dry-run to test the specific push path. If a future runtime loses
repository access, restore the repository grant in the existing GitHub
connection. Only consider another credential after the specific operation
is reachable and its authorization failure is understood.

GitHub takes time to build a Pages site after a push. Verify the deployment
status and load the published HTML, JavaScript and WASM before reporting
that the public site is live. Git push success alone is insufficient.

## Reference

- [Configure a Pages publishing source](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
- [Pages REST API](https://docs.github.com/en/rest/pages/pages)

GitHub documents that a workflow push made with `GITHUB_TOKEN` does not
trigger a branch-based Pages build. This publisher runs native Git through
the cloud's repository connection, outside a GitHub Actions job. If future
deployment moves into Actions, use the supported Pages artifact/deployment
actions instead of pushing the branch with its `GITHUB_TOKEN`.
