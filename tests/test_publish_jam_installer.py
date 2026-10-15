#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Normal installer containment against local bare remotes; no device/network."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import publish_jam_installer as publish
from publish_jam_recovery_preview import git, git_text, tree_entries

class InstallerPublicationTests(unittest.TestCase):

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.remote = self.root / 'remote.git'
        self.remote.mkdir()
        git(self.remote, 'init', '--bare', '--quiet', '--template=')
        self.seed = self.root / 'seed'
        self.seed.mkdir()
        git(self.seed, 'init', '--quiet', '--template=', '--initial-branch=gh-pages')
        git(self.seed, 'config', 'user.name', 'Local test')
        git(self.seed, 'config', 'user.email', 'test@example.invalid')
        old = {'emu/index.html': b'unchanged emulator', 'preview/saving/index.html': b'unchanged preview', 'install/app.js': b'old installer', 'device/index.html': b'unchanged checker HTML', 'custom-file': b'preserved binary\x00\xff'}
        for name, body in old.items():
            path = self.seed / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(body)
        (self.seed / 'custom-file').chmod(493)
        (self.seed / 'external-link').symlink_to('../../must-never-check-out')
        (self.seed / 'SHA256SUMS').write_bytes(''.join((f'{publish.sha(body)}  {name}\n' for name, body in sorted(old.items()))).encode())
        git(self.seed, 'add', '.')
        git(self.seed, 'commit', '--quiet', '-m', 'Existing site')
        git(self.seed, 'remote', 'add', 'origin', str(self.remote))
        git(self.seed, 'push', '--quiet', 'origin', 'HEAD:refs/heads/gh-pages')
        self.original = git_text(self.remote, 'rev-parse', publish.BRANCH)
        git(self.remote, 'update-ref', 'refs/heads/main', self.original)
        self.before = publish.protected(tree_entries(self.remote, self.original))
        self.files = {name: b'new ' + name.encode() for name in publish.UPDATED}
        self.validation = patch.object(publish.package, 'validate')
        self.validation.start()
        self.addCleanup(self.validation.stop)

    def prepare(self):
        return publish.prepare(self.files, str(self.remote), ('Local test', 'test@example.invalid'))

    def test_dry_run_preserves_remote_and_all_unrelated_objects(self):
        with self.prepare() as prepared:
            report = prepared.finish()
            self.assertEqual(report['action'], 'dry-run')
            self.assertEqual(publish.protected(tree_entries(prepared.directory, prepared.tree)), self.before)
            self.assertEqual(git_text(self.remote, 'rev-parse', publish.BRANCH), self.original)
            self.assertFalse((prepared.directory / 'external-link').exists())

    def test_explicit_publish_updates_only_installer_and_merges_checksums(self):
        with self.prepare() as prepared:
            prepared.finish(True)
            self.assertEqual(publish.protected(tree_entries(self.remote, prepared.commit)), self.before)
            sums = git(self.remote, 'show', prepared.commit + ':SHA256SUMS').stdout.decode()
            self.assertIn(publish.sha(b'unchanged emulator') + '  emu/index.html', sums)
            self.assertIn(publish.sha(self.files['install/app.js']) + '  install/app.js', sums)
            self.assertEqual(git_text(self.remote, 'rev-parse', 'refs/heads/main'), self.original)
        with self.prepare() as prepared:
            self.assertEqual(prepared.finish(True)['action'], 'unchanged')

    def test_remote_race_refused(self):
        with self.prepare() as stale:
            self.files['install/app.js'] = b'concurrent update'
            with self.prepare() as winner:
                winner.finish(True)
                commit = winner.commit
            with self.assertRaisesRegex(publish.DeploymentError, 'changed'):
                stale.finish(True)
            self.assertEqual(git_text(self.remote, 'rev-parse', publish.BRANCH), commit)

    def test_push_race_after_precheck_is_rejected_without_force(self):
        with self.prepare() as stale:
            self.files['install/app.js'] = b'concurrent update'
            with self.prepare() as winner:
                winner.finish(True)
                commit = winner.commit
            with patch.object(publish.gitops, 'remote_tip', return_value=stale.parent):
                with self.assertRaisesRegex(publish.DeploymentError, 'Git push failed'):
                    stale.finish(True)
            self.assertEqual(git_text(self.remote, 'rev-parse', publish.BRANCH), commit)

    def test_mutated_prepared_head_refused(self):
        with self.prepare() as prepared:
            git(prepared.directory, 'update-ref', publish.BRANCH, prepared.parent)
            with self.assertRaisesRegex(publish.DeploymentError, 'commit changed'):
                prepared.finish(True)

    def test_invalid_old_checksums_cannot_be_silently_replaced(self):
        (self.seed / 'custom-file').write_bytes(b'corruption without checksum update')
        git(self.seed, 'add', '.')
        git(self.seed, 'commit', '--quiet', '-m', 'Corrupt old checksum')
        git(self.seed, 'push', '--quiet', 'origin', 'HEAD:refs/heads/gh-pages')
        with self.assertRaisesRegex(publish.DeploymentError, 'checksum mismatch'):
            with self.prepare():
                pass

    def test_runtime_validation_runs_before_any_git_mutation(self):
        with patch.object(publish.package, 'validate', side_effect=ValueError('bad candidate')):
            with self.assertRaisesRegex(ValueError, 'bad candidate'):
                with self.prepare():
                    pass
        self.assertEqual(git_text(self.remote, 'rev-parse', publish.BRANCH), self.original)
if __name__ == '__main__':
    unittest.main()
