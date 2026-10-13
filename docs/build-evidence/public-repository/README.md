# Public repository migration and commit attribution

Executed October 13, 2026. The project moved from the former private repository
to the public [anthonysprints/jamboxfm](https://github.com/anthonysprints/jamboxfm).
Both source (`main`) and generated site (`gh-pages`) were transferred, and the
publishing destination and current documentation were updated. The local
checkout now uses the new repository as `origin`; the old remote remains a
historical reference. Existing untracked upstream files were preserved.

The static package rebuilt successfully with unchanged emulator WASM:
SHA256 `88f52ef09404069593fe36f4a2304a821f32f14190e353cfbd18d070f9dd99da`.
All five publisher tests passed. The updated package was uploaded to the public
repository while preserving the existing site history. See [build](package.log),
[tests](publisher-tests.log) and [initial public upload](initial-public-upload.json).

## Attribution correction

The inherited local Git email caused GitHub to attribute commits to the old
account. At the owner's request, author and committer identities were corrected
across all six existing source/site commits, using the account-linked GitHub
noreply address. GitHub's API confirmed `anthonysprints` as author and committer
for every corrected commit on both branches.

The original histories were saved in a verified complete Git bundle outside
the checkout. An independent review confirmed identical file trees, message
bytes, timestamps and timezone offsets; only identities and parent references
changed. Both remote branches were updated atomically with explicit old-tip
leases. Routine future publishing still uses ordinary pushes that preserve
history. Local Git identity and root AGENTS.md now record the correct account.

See [metadata mapping](author-correction.json), [GitHub attribution](github-attribution.json)
and [remote package verification](remote-package-verify.json). Older evidence
retains the commit IDs from its original execution; the mapping records their
corrected equivalents where applicable.

## Pages status

GitHub Pages is configured for `gh-pages` at `/`. The [deployment record](pages-deployment.json)
reports GitHub's build/deployment state for the corrected site commit. The
public instrument address is https://anthonysprints.github.io/jamboxfm/emu/.

The cloud proxy rejected a direct site request with CONNECT 403, as recorded
in [the HTTP check](live-http-check.json). That attempt received no response
from the site itself, so it does not verify live-browser audio or asset hashes.
The new hostname is saved in the environment's network configuration; applying
and publishing those settings is a separate product action.
