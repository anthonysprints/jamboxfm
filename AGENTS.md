# JamBoxFM repository workflow

The active public repository is `anthonysprints/jamboxfm`. Develop source on
`main`; publish the generated website to `gh-pages` using
`docs/GITHUB_PAGES.md`. Use the existing checkout for cloud tasks.

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
