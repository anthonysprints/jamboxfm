#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Publish only the saving-development preview below preview/saving/.

The default validates the prepared package and stages a local commit, then runs
Git's push dry-run. --publish makes a normal non-forced push to gh-pages. Every
existing path outside preview/saving/ must retain its exact Git object and mode.
This tool never invokes the production publisher or checks out remote files.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import dataclass
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
from typing import Iterator

# Only pure validators are shared; the production deployment functions are not
# called. These helpers inspect archives and checksums without executing them.
from publish_jam_web import DeploymentError, sha, source_contents, validate_hashes
import package_jam_recovery as recovery_package

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "build/jambox-recovery-preview"
DESTINATION = "https://github.com/anthonysprints/jamboxfm.git"
BRANCH = "refs/heads/gh-pages"
PREFIX = "preview/saving/"
RUNTIME_SOURCE = {
    "install/index.html": "web/recovery/index.html",
    "install/style.css": "web/recovery/style.css",
    "install/app.js": "web/recovery/app.js",
    "fm1ota.js": "web/fm1ota.js",
    "fm1pkg.js": "web/fm1pkg.js",
    "jam-device-policy.js": "web/jam-device-policy.js",
    "jam-recovery-policy.js": "web/jam-recovery-policy.js",
}
RUNTIME_FILES = frozenset(RUNTIME_SOURCE) | {"index.html", ".nojekyll"}
LICENSE_FILES = recovery_package.LICENSE_FILES
FIRMWARE = recovery_package.FIRMWARE
BUNDLE_FILES = recovery_package.BUNDLE_FILES
EXPECTED_FILES = RUNTIME_FILES | {"firmware/" + name for name in BUNDLE_FILES} | {"SHA256SUMS"}
EXPECTED_DIRECTORIES = frozenset({"install", "firmware", "firmware/licenses"})
GIT_LOCATION_ENV = frozenset({"GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR",
    "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_PREFIX", "GIT_NAMESPACE",
    "GIT_SHALLOW_FILE", "GIT_QUARANTINE_PATH", "GIT_REPLACE_REF_BASE"})
GIT_OPTIONS = ["-c", "core.hooksPath=/dev/null", "-c", "core.fsmonitor=false",
    "-c", "core.autocrlf=false", "-c", "core.attributesFile=/dev/null"]


def check_target() -> None:
    """Destination changes require a separately reviewed tool, not a CLI flag."""
    if (BRANCH != "refs/heads/gh-pages" or PREFIX != "preview/saving/" or
            DESTINATION != "https://github.com/anthonysprints/jamboxfm.git"):
        raise DeploymentError("Only the fixed gh-pages preview/saving/ destination is permitted.")


def validate_snapshot(files: dict[str, bytes]) -> None:
    if set(files) != EXPECTED_FILES or any(not isinstance(data, bytes) for data in files.values()):
        raise DeploymentError("Preview must contain exactly the approved regular files.")
    if sum(map(len, files.values())) > 128 * 1024 * 1024:
        raise DeploymentError("Preview package exceeds the supported size.")
    validate_hashes(files, "Preview")


def validate_site(site: Path) -> dict[str, bytes]:
    """Read regular files once, then validate the immutable in-memory snapshot."""
    if site.is_symlink() or not site.is_dir():
        raise DeploymentError("Preview directory is missing or a symlink.")
    files = {}
    total_bytes = 0
    for path in site.rglob("*"):
        name = path.relative_to(site).as_posix()
        if path.is_symlink():
            raise DeploymentError(f"Preview symlink is forbidden: {name}")
        if path.is_dir():
            if name not in EXPECTED_DIRECTORIES:
                raise DeploymentError(f"Unexpected preview directory: {name}")
            continue
        if name not in EXPECTED_FILES or not path.is_file():
            raise DeploymentError(f"Unexpected preview file: {name}")
        # Do not follow a file or any parent component exchanged for a symlink
        # after traversal. The package is read through anchored directory fds.
        descriptors = []
        try:
            descriptor = os.open(site, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            descriptors.append(descriptor)
            for part in path.relative_to(site).parts[:-1]:
                descriptor = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                    dir_fd=descriptor)
                descriptors.append(descriptor)
            leaf = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=descriptor)
            descriptors.append(leaf)
            info = os.fstat(leaf)
            if not stat.S_ISREG(info.st_mode) or info.st_size > 64 * 1024 * 1024:
                raise DeploymentError(f"Preview file is not a bounded regular file: {name}")
            with os.fdopen(os.dup(leaf), "rb") as stream:
                files[name] = stream.read(64 * 1024 * 1024 + 1)
            total_bytes += len(files[name])
            if len(files[name]) > 64 * 1024 * 1024 or total_bytes > 128 * 1024 * 1024:
                raise DeploymentError("Preview package exceeds the supported size.")
        except OSError as error:
            raise DeploymentError(f"Cannot safely read preview file {name}: {error}") from error
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)
    validate_snapshot(files)
    validate_bundle(files)
    return files


def validate_bundle(files: dict[str, bytes]) -> None:
    """Cross-check firmware, corresponding source, evidence and runtime links."""
    bundle = {name: files["firmware/" + name] for name in BUNDLE_FILES}
    try:
        recovery_package.validate_bundle_files(bundle)
    except (ValueError, KeyError, subprocess.SubprocessError) as error:
        raise DeploymentError(f"Invalid recovery candidate: {error}") from error
    source = source_contents(bundle["jamboxfm-source.zip"])
    for published, original in RUNTIME_SOURCE.items():
        if source.get(original) != files[published]:
            raise DeploymentError(f"Preview {published} differs from its corresponding source.")
    if b"install/" not in files["index.html"]:
        raise DeploymentError("Preview landing page must link its separate installer.")
    if b"jamboxfm-source.zip" not in files["install/index.html"]:
        raise DeploymentError("Preview installer must link corresponding firmware source.")


def git(directory: Path, *arguments: str, data: bytes | None = None,
        check: bool = True) -> subprocess.CompletedProcess:
    environment = {key: value for key, value in os.environ.items() if key not in GIT_LOCATION_ENV}
    result = subprocess.run(["git", *GIT_OPTIONS, *arguments], cwd=directory, input=data,
        env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode:
        detail = (result.stderr or result.stdout).decode("utf-8", "replace").strip()
        raise DeploymentError(f"Git {arguments[0]} failed: {detail}")
    return result


def git_text(directory: Path, *arguments: str, check: bool = True) -> str:
    return git(directory, *arguments, check=check).stdout.decode("utf-8", "strict").strip()


def remote_tip(directory: Path) -> str:
    lines = git_text(directory, "ls-remote", "--heads", "origin", BRANCH).splitlines()
    if len(lines) != 1:
        raise DeploymentError("Preview requires one existing gh-pages reference.")
    match = re.fullmatch(r"([0-9a-f]{40,64})\s+refs/heads/gh-pages", lines[0])
    if not match:
        raise DeploymentError("Remote gh-pages reference was malformed.")
    return match[1]


def author_identity(root: Path, name: str | None, email: str | None) -> tuple[str, str]:
    name = name or git_text(root, "config", "--get", "user.name", check=False)
    email = email or git_text(root, "config", "--get", "user.email", check=False)
    if not name or not email or any(char in name + email for char in "\r\n\x00<>"):
        raise DeploymentError("Configure Git user.name and user.email before publishing.")
    return name, email


def tree_entries(directory: Path, revision: str, *, recursive: bool = True) -> dict[bytes, bytes]:
    result = {}
    options = ["-r", "-t"] if recursive else []
    for entry in git(directory, "ls-tree", *options, "-z", "--full-tree", revision).stdout.split(b"\0"):
        if entry:
            metadata, name = entry.split(b"\t", 1)
            if name in result:
                raise DeploymentError("Duplicate path in remote Git tree.")
            result[name] = metadata
    return result


def outside_preview(entries: dict[bytes, bytes]) -> dict[bytes, bytes]:
    # preview's containing tree object changes when its saving child changes.
    # All other directories, including empty Git subtrees, remain protected.
    return {name: value for name, value in entries.items()
        if name not in (b"preview", b"preview/saving") and not name.startswith(PREFIX.encode())}


def replace_child_tree(directory: Path, parent: str | None, child: bytes, tree: str) -> str:
    entries = tree_entries(directory, parent, recursive=False) if parent else {}
    previous = entries.get(child)
    if previous is not None and not previous.startswith(b"040000 tree "):
        raise DeploymentError("Existing file conflicts with the separate preview directory.")
    entries[child] = b"040000 tree " + tree.encode()
    data = b"".join(metadata + b"\t" + name + b"\0" for name, metadata in sorted(entries.items()))
    return git(directory, "mktree", "-z", data=data).stdout.decode().strip()


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
            raise DeploymentError("Prepared preview commit changed; restage the package.")
        if outside_preview(tree_entries(self.directory, self.tree)) != self.protected:
            raise DeploymentError("Preview would change paths outside its destination.")
        if remote_tip(self.directory) != self.parent:
            raise DeploymentError("gh-pages changed during preparation; rerun against its new tip.")
        if self.changed:
            options = [] if publish else ["--dry-run"]
            git(self.directory, "push", "--porcelain", *options, "origin", f"HEAD:{BRANCH}")
        return {"action": "unchanged" if not self.changed else "published" if publish else "dry-run",
            "branch": BRANCH, "prefix": PREFIX, "parent": self.parent, "commit": self.commit,
            "tree": self.tree, "files": len(self.files), "bytes": sum(map(len, self.files.values())),
            "package_sha256": sha(self.files["SHA256SUMS"]),
            "protected_paths": len(self.protected)}


@contextmanager
def prepare_deployment(files: dict[str, bytes], remote: str, identity: tuple[str, str],
                       scratch: Path | None = None) -> Iterator[PreparedPreview]:
    """Stage a validated snapshot; internal remote argument enables local tests.

    The CLI exposes no remote, branch, prefix or source-directory override.
    Remote files are retained as Git objects and never written to the filesystem.
    """
    check_target()
    validate_snapshot(files)
    files = dict(files)
    if scratch:
        scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="jambox-saving-preview-", dir=scratch) as temporary:
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
        for name in (b"preview", b"preview/saving"):
            if name in previous and not previous[name].startswith(b"040000 tree "):
                raise DeploymentError("Existing file conflicts with the separate preview directory.")
        git(directory, "read-tree", "--empty")
        additions = []
        for name, data in sorted(files.items()):
            object_id = git(directory, "hash-object", "-w", "--stdin", data=data).stdout.strip()
            additions.append(b"100644 " + object_id + b"\t" + name.encode() + b"\0")
        git(directory, "update-index", "-z", "--index-info", data=b"".join(additions))
        saving_tree = git_text(directory, "write-tree")
        preview_metadata = tree_entries(directory, parent, recursive=False).get(b"preview")
        old_preview = preview_metadata.split(b" ", 2)[2].decode() if preview_metadata else None
        preview_tree = replace_child_tree(directory, old_preview, b"saving", saving_tree)
        tree = replace_child_tree(directory, parent, b"preview", preview_tree)
        current = tree_entries(directory, tree)
        if outside_preview(current) != protected:
            raise DeploymentError("Preview would change paths outside its destination.")
        expected_paths = ({PREFIX.encode() + name.encode() for name in files} |
            {b"preview", b"preview/saving"} |
            {PREFIX.encode() + name.encode() for name in EXPECTED_DIRECTORIES})
        if set(current) - set(protected) != expected_paths:
            raise DeploymentError("Preview Git tree contains unexpected paths.")
        changed = git_text(directory, "rev-parse", f"{parent}^{{tree}}") != tree
        if changed:
            message = ("Publish separate JamBox saving development preview\n\n"
                f"Destination: {PREFIX}\nPackage SHA256: {sha(files['SHA256SUMS'])}\n").encode()
            commit = git(directory, "-c", "commit.gpgsign=false", "commit-tree", tree, "-p", parent,
                data=message).stdout.decode().strip()
        else:
            commit = parent
        git(directory, "update-ref", BRANCH, commit)
        yield PreparedPreview(directory, parent, commit, tree, changed, files, protected)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publish", action="store_true", help="publish only the separate preview (default: dry-run)")
    args = parser.parse_args()
    try:
        files = validate_site(SITE)
        identity = author_identity(ROOT, None, None)
        with prepare_deployment(files, DESTINATION, identity, ROOT.parent / "scratch") as prepared:
            result = prepared.finish(args.publish)
        import json
        result["destination"] = DESTINATION
        print(json.dumps(result, indent=2, sort_keys=True))
    except (DeploymentError, ValueError, KeyError, OSError, UnicodeError) as error:
        parser.exit(1, f"Preview publication stopped: {error}\n")


if __name__ == "__main__":
    main()
