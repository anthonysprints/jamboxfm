#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Publish only the sound-page cleanup trial below fixed preview/sound/.

Default: validate and stage a local commit, then Git push --dry-run. --publish
uses a normal non-forced push. Every outside Git object and mode must survive.
No remote files are checked out, and no other publisher entrypoint is called.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import dataclass
import json
from pathlib import Path
import tempfile
from typing import Iterator

import package_jam_sound_preview as package
from package_jam_sound_preview import (DeploymentError, EXPECTED_DIRECTORIES,
    EXPECTED_FILES, ROOT, sha, validate_bundle, validate_site, validate_snapshot)
# Reviewed Git primitives only; saving/production deployment functions are not used.
from publish_jam_recovery_preview import (author_identity, git, git_text,
    remote_tip, replace_child_tree, tree_entries)

SITE = package.OUTPUT
DESTINATION = "https://github.com/anthonysprints/jamboxfm.git"
BRANCH = "refs/heads/gh-pages"
PREFIX = "preview/sound/"


def check_target() -> None:
    if (DESTINATION != "https://github.com/anthonysprints/jamboxfm.git" or
            BRANCH != "refs/heads/gh-pages" or PREFIX != "preview/sound/"):
        raise DeploymentError("Only the fixed gh-pages preview/sound/ destination is permitted.")


def outside_preview(entries: dict[bytes, bytes]) -> dict[bytes, bytes]:
    return {name: value for name, value in entries.items()
        if name not in (b"preview", b"preview/sound") and not name.startswith(PREFIX.encode())}


@dataclass
class PreparedPreview:
    directory: Path
    parent: str
    commit: str
    tree: str
    changed: bool
    files: dict[str, bytes]
    protected: dict[bytes, bytes]

    def finish(self, publish: bool = False) -> dict:
        check_target()
        if (git_text(self.directory, "rev-parse", "HEAD") != self.commit or
                git_text(self.directory, "rev-parse", f"{self.commit}^{{tree}}") != self.tree):
            raise DeploymentError("Prepared sound preview commit changed; restage the package.")
        if outside_preview(tree_entries(self.directory, self.tree)) != self.protected:
            raise DeploymentError("Sound preview would change paths outside its destination.")
        if remote_tip(self.directory) != self.parent:
            raise DeploymentError("gh-pages changed during preparation; rerun against its new tip.")
        if self.changed:
            git(self.directory, "push", "--porcelain", *([] if publish else ["--dry-run"]),
                "origin", f"HEAD:{BRANCH}")
        return {"action": "unchanged" if not self.changed else "published" if publish else "dry-run",
            "branch": BRANCH, "prefix": PREFIX, "parent": self.parent, "commit": self.commit,
            "tree": self.tree, "files": len(self.files), "bytes": sum(map(len, self.files.values())),
            "package_sha256": sha(self.files["SHA256SUMS"]), "protected_paths": len(self.protected)}


@contextmanager
def prepare_deployment(files: dict[str, bytes], remote: str, identity: tuple[str, str],
                       scratch: Path | None = None) -> Iterator[PreparedPreview]:
    """Validate before any Git mutation; the internal remote enables local tests."""
    check_target()
    files = dict(files)
    validate_snapshot(files)
    validate_bundle(files)
    if scratch:
        scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="jambox-sound-preview-", dir=scratch) as temporary:
        directory = Path(temporary)
        git(directory, "init", "--quiet", "--template=", "--initial-branch=gh-pages")
        git(directory, "remote", "add", "origin", remote)
        git(directory, "config", "user.name", identity[0])
        git(directory, "config", "user.email", identity[1])
        parent = remote_tip(directory)
        git(directory, "fetch", "--quiet", "--no-tags", "origin", BRANCH)
        if git_text(directory, "rev-parse", "FETCH_HEAD") != parent:
            raise DeploymentError("gh-pages changed while fetching; rerun against its new tip.")
        previous = tree_entries(directory, parent)
        protected = outside_preview(previous)
        for name in (b"preview", b"preview/sound"):
            if name in previous and not previous[name].startswith(b"040000 tree "):
                raise DeploymentError("Existing file conflicts with the separate sound preview directory.")
        git(directory, "read-tree", "--empty")
        additions = []
        for name, data in sorted(files.items()):
            object_id = git(directory, "hash-object", "-w", "--stdin", data=data).stdout.strip()
            additions.append(b"100644 " + object_id + b"\t" + name.encode() + b"\0")
        git(directory, "update-index", "-z", "--index-info", data=b"".join(additions))
        sound_tree = git_text(directory, "write-tree")
        metadata = tree_entries(directory, parent, recursive=False).get(b"preview")
        old_preview = metadata.split(b" ", 2)[2].decode() if metadata else None
        preview_tree = replace_child_tree(directory, old_preview, b"sound", sound_tree)
        tree = replace_child_tree(directory, parent, b"preview", preview_tree)
        current = tree_entries(directory, tree)
        if outside_preview(current) != protected:
            raise DeploymentError("Sound preview would change paths outside its destination.")
        expected_paths = ({PREFIX.encode() + name.encode() for name in files} |
            {b"preview", b"preview/sound"} |
            {PREFIX.encode() + name.encode() for name in EXPECTED_DIRECTORIES})
        if set(current) - set(protected) != expected_paths:
            raise DeploymentError("Sound preview Git tree contains unexpected paths.")
        changed = git_text(directory, "rev-parse", f"{parent}^{{tree}}") != tree
        if changed:
            message = ("Publish separate JamBox sound-page cleanup trial\n\n"
                f"Destination: {PREFIX}\nPackage SHA256: {sha(files['SHA256SUMS'])}\n").encode()
            commit = git(directory, "-c", "commit.gpgsign=false", "commit-tree", tree, "-p", parent,
                data=message).stdout.decode().strip()
        else:
            commit = parent
        git(directory, "update-ref", BRANCH, commit)
        yield PreparedPreview(directory, parent, commit, tree, changed, files, protected)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publish", action="store_true", help="publish only the separate sound trial (default: dry-run)")
    args = parser.parse_args()
    try:
        files = validate_site(SITE)
        identity = author_identity(ROOT, None, None)
        with prepare_deployment(files, DESTINATION, identity, ROOT.parent / "scratch") as prepared:
            result = prepared.finish(args.publish)
        result["destination"] = DESTINATION
        print(json.dumps(result, indent=2, sort_keys=True))
    except (DeploymentError, ValueError, KeyError, OSError, UnicodeError) as error:
        parser.exit(1, f"Sound preview publication stopped: {error}\n")


if __name__ == "__main__":
    main()
