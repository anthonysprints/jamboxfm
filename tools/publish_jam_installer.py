#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Update only the normal installer and its bound firmware/source downloads.

Preserves the browser instrument, preview and every other Git path/object/mode.
Default is a Git push dry-run; --publish makes a normal non-forced push.
"""
import argparse
from contextlib import contextmanager
from dataclasses import dataclass
import json
from pathlib import Path
import re
import tempfile
import package_jam_installer as package
import publish_jam_recovery_preview as gitops
from publish_jam_web import DeploymentError, sha
ROOT = package.ROOT
DESTINATION = 'https://github.com/anthonysprints/jamboxfm.git'
BRANCH = 'refs/heads/gh-pages'
UPDATED = package.FILES
ANCESTORS = frozenset((parent.as_posix().encode() for name in UPDATED for parent in Path(name).parents if parent.as_posix() != '.'))

def protected(entries):
    return {name: meta for name, meta in entries.items() if name.decode('utf-8', 'surrogateescape') not in UPDATED and name not in ANCESTORS}

def replace_paths(directory, parent, files):
    entries = gitops.tree_entries(directory, parent, recursive=False) if parent else {}
    groups = {}
    for name, body in files.items():
        first, separator, rest = name.partition('/')
        if separator:
            groups.setdefault(first, {})[rest] = body
        else:
            entries[first.encode()] = b'100644 blob ' + gitops.git(directory, 'hash-object', '-w', '--stdin', data=body).stdout.strip()
    for name, children in groups.items():
        old = entries.get(name.encode())
        if old and (not old.startswith(b'040000 tree ')):
            raise DeploymentError('Conflicting published path: ' + name)
        child = replace_paths(directory, old.split()[2].decode() if old else None, children)
        entries[name.encode()] = b'040000 tree ' + child.encode()
    payload = b''.join((meta + b'\t' + name + b'\x00' for name, meta in sorted(entries.items())))
    return gitops.git(directory, 'mktree', '-z', data=payload).stdout.decode().strip()

def merged_checksums(directory, parent, files):
    old = gitops.git(directory, 'show', parent + ':SHA256SUMS').stdout.decode('ascii')
    result = {}
    for line in old.splitlines():
        match = re.fullmatch('([0-9a-f]{64})  ([A-Za-z0-9_.][A-Za-z0-9_./-]*)', line)
        if not match or any((part in ('', '.', '..') for part in match[2].split('/'))) or match[2] in result:
            raise DeploymentError('Invalid existing root checksums')
        digest, name = match.groups()
        if name == 'SHA256SUMS':
            raise DeploymentError('Self-referencing root checksums')
        if sha(gitops.git(directory, 'show', parent + ':' + name).stdout) != digest:
            raise DeploymentError('Existing root checksum mismatch: ' + name)
        result[name] = digest
    result.update({name: sha(body) for name, body in files.items() if name != 'SHA256SUMS'})
    return ''.join((f'{digest}  {name}\n' for name, digest in sorted(result.items()))).encode()

@dataclass
class Prepared:
    directory: Path
    parent: str
    commit: str
    tree: str
    unchanged: dict
    files: dict
    changed: bool

    def finish(self, publish=False):
        if BRANCH != 'refs/heads/gh-pages' or DESTINATION != 'https://github.com/anthonysprints/jamboxfm.git':
            raise DeploymentError('Installer destination changed')
        if gitops.git_text(self.directory, 'rev-parse', 'HEAD') != self.commit or gitops.git_text(self.directory, 'rev-parse', self.commit + '^{tree}') != self.tree:
            raise DeploymentError('Prepared installer commit changed')
        if protected(gitops.tree_entries(self.directory, self.tree)) != self.unchanged:
            raise DeploymentError('Installer would change a protected path')
        if gitops.remote_tip(self.directory) != self.parent:
            raise DeploymentError('gh-pages changed; prepare again')
        if self.changed:
            gitops.git(self.directory, 'push', '--porcelain', *(() if publish else ('--dry-run',)), 'origin', 'HEAD:' + BRANCH)
        return {'action': ('published' if publish else 'dry-run') if self.changed else 'unchanged', 'parent': self.parent, 'commit': self.commit, 'tree': self.tree, 'updated_files': len(self.files), 'protected_paths': len(self.unchanged), 'root_checksums_sha256': sha(self.files['SHA256SUMS'])}

@contextmanager
def prepare(files, remote, identity, scratch=None):
    package.validate(files)
    files = dict(files)
    if scratch:
        Path(scratch).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='jambox-installer-publish-', dir=scratch) as folder:
        directory = Path(folder)
        gitops.git(directory, 'init', '--quiet', '--template=', '--initial-branch=gh-pages')
        gitops.git(directory, 'remote', 'add', 'origin', remote)
        gitops.git(directory, 'config', 'user.name', identity[0])
        gitops.git(directory, 'config', 'user.email', identity[1])
        parent = gitops.remote_tip(directory)
        gitops.git(directory, 'fetch', '--quiet', '--no-tags', 'origin', BRANCH)
        if gitops.git_text(directory, 'rev-parse', 'FETCH_HEAD') != parent:
            raise DeploymentError('gh-pages changed while fetching')
        before = protected(gitops.tree_entries(directory, parent))
        files['SHA256SUMS'] = merged_checksums(directory, parent, files)
        tree = replace_paths(directory, parent, files)
        if protected(gitops.tree_entries(directory, tree)) != before:
            raise DeploymentError('Protected paths changed')
        changed = gitops.git_text(directory, 'rev-parse', parent + '^{tree}') != tree
        commit = gitops.git(directory, '-c', 'commit.gpgsign=false', 'commit-tree', tree, '-p', parent, data=b'Publish owner-tested JamBox firmware in the normal Chrome installer\n').stdout.decode().strip() if changed else parent
        gitops.git(directory, 'update-ref', BRANCH, commit)
        yield Prepared(directory, parent, commit, tree, before, files, changed)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    try:
        files = package.read_package()
        identity = gitops.author_identity(ROOT, None, None)
        with prepare(files, DESTINATION, identity, ROOT.parent / 'scratch') as deployment:
            result = deployment.finish(args.publish)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, KeyError, DeploymentError) as exc:
        parser.exit(1, f'Installer publication refused: {exc}\n')
if __name__ == '__main__':
    main()
