# Publishing JamBoxFM to GitHub Pages

Use `sprintrstudio/jamboxfm` and a dedicated `gh-pages` branch containing only
the generated site. The source checkout and the inherited Sloop installer
remain separate from the publication branch. The page's relative asset URLs
work under the repository path, and `.nojekyll` keeps the generated site intact.
Use `main` when checking out the editable JamBox source for a future session.
The source branch includes the JamBox build inputs and research/test evidence;
the reserved upstream artwork and inherited generated deployment stay out of it.

## One-time setup

The cloud environment's existing HTTPS Git connection can publish the site.
An actual push to `gh-pages` and a subsequent remote comparison have passed;
no additional publishing token is needed. Keep this repository enabled in the
Codex GitHub connection for future sessions.

The environment configuration draft adds these two destinations while
preserving the device-compiler hostname and package-manager preset:

- `api.github.com`: inspect/configure Pages and check build status.
- `sprintrstudio.github.io`: verify the published page and its assets.

Both destinations were reachable during the final setup check. On a new
environment, review and save the settings, then publish the environment
configuration if these network additions have not been applied. A proxy
`Tunnel connection failed: 403 Forbidden` is a network failure, whereas
GitHub's JSON `Resource not accessible by integration` is an authorization
failure. The current connection successfully reads repository metadata and
pushes Git commits, but GitHub denied both reading and creating Pages settings.
Adding a token is unnecessary for the branch publishing workflow below.

After the `gh-pages` branch has been pushed, open
[repository Pages settings](https://github.com/sprintrstudio/jamboxfm/settings/pages)
and select **Deploy from a branch**, **gh-pages**, **/(root)**, then **Save**.
Changing this setting requires repository admin or maintainer access (or a
custom role with permission to manage Pages settings). Routine updates need
repository push permission, without continuing admin access to Pages settings.
This repository is private. If GitHub presents an upgrade prompt, Pages for
private repositories requires an eligible GitHub plan.

The expected project address is `https://sprintrstudio.github.io/jamboxfm/emu/`.
Treat it as a destination until an actual HTTP check succeeds. No custom domain,
DNS change or third-party hosting account is required.

## Publish an update

From the existing source checkout:

```sh
. tools/cloud/activate.sh
sh tests/run_jam_emu_tests.sh
python3 tools/publish_jam_web.py
python3 tools/publish_jam_web.py --publish
```

The test runner builds the current emulator and its publication package.
The first publisher invocation performs a Git push dry-run; the second
publishes the same validated package. Use `--publish` when the user requests
a website deployment or update. Publishing is not a startup action.

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
