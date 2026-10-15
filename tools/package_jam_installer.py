#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the normal Chrome installer without rebuilding the browser instrument.

Verifies the current owner-approved manual-saving build and corresponding source.
Nothing publishes or accesses a device. --no-build reuses a verified target.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import package_jam_device as device
import publish_jam_web as web
ROOT = device.ROOT
OUTPUT = ROOT / 'build/jambox-installer'
RUNTIME = {name: 'web/' + name for name in ('install/index.html', 'install/style.css', 'install/app.js', 'jam-trial-install.js', 'jam-device-policy.js', 'fm1ota.js', 'fm1pkg.js', 'device/app.js')}
FILES = frozenset(RUNTIME) | {'jambox-source.zip', 'SHA256SUMS'} | {'firmware/' + name for name in web.TRIAL_BUNDLE_FILES | {'trial.json'}}

def validate(files):
    if set(files) != FILES:
        raise ValueError('Installer package must contain exactly its approved files')
    web.validate_hashes(files, 'Installer')
    web.validate_trial(files)
    manifest = web.json_object(files['firmware/manifest.json'], 'manifest')
    record = web.json_object(files['firmware/build-receipt.json'], 'receipt')
    device.check_configuration(record['configuration'])
    source = web.source_contents(files['firmware/jamboxfm-source.zip'])
    if files['jambox-source.zip'] != files['firmware/jamboxfm-source.zip']:
        raise ValueError('Root and firmware corresponding source must match')
    for name, original in RUNTIME.items():
        if files[name] != source.get(original):
            raise ValueError('Installer runtime differs from corresponding source: ' + name)
    return manifest

def read_package(directory=OUTPUT):
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Missing or symlink installer package')
    files = {}
    for path in directory.rglob('*'):
        name = path.relative_to(directory).as_posix()
        if path.is_symlink():
            raise ValueError('Installer symlinks are forbidden')
        if path.is_dir():
            if name not in {'install', 'device', 'firmware', 'firmware/licenses'}:
                raise ValueError('Unexpected installer directory: ' + name)
        elif name not in FILES or not path.is_file() or path.stat().st_size > 64 * 1024 * 1024:
            raise ValueError('Unexpected installer file: ' + name)
        else:
            files[name] = path.read_bytes()
    validate(files)
    return files

def assemble():
    files = {name: (ROOT / original).read_bytes() for name, original in RUNTIME.items()}
    files.update({'firmware/' + name: (device.OUTPUT / name).read_bytes() for name in web.TRIAL_BUNDLE_FILES})
    manifest = json.loads(files['firmware/manifest.json'])
    trial = {'schema_version': 2, 'kind': 'experimental-install', 'hardware_validated': False, 'transitions': ['factory-v15-to-jambox', 'jambox-normal-to-jambox'], 'manifest_sha256': web.sha(files['firmware/manifest.json']), 'firmware_sha256': manifest['firmware']['sha256']}
    files['firmware/trial.json'] = device.receipt.json_bytes(trial)
    files['jambox-source.zip'] = files['firmware/jamboxfm-source.zip']
    files['SHA256SUMS'] = ''.join((f'{web.sha(body)}  {name}\n' for name, body in sorted(files.items()))).encode()
    validate(files)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if OUTPUT.is_symlink():
        raise ValueError('Refusing a symlink installer output')
    with tempfile.TemporaryDirectory(prefix='.jambox-installer-', dir=OUTPUT.parent) as folder:
        stage = Path(folder) / 'site'
        for name, body in files.items():
            path = stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(body)
        read_package(stage)
        if OUTPUT.exists():
            shutil.rmtree(OUTPUT)
        stage.rename(OUTPUT)
    return {'files': len(files), 'firmware_sha256': manifest['firmware']['sha256'], 'checksums_sha256': web.sha(files['SHA256SUMS'])}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    args = parser.parse_args()
    try:
        if not args.no_build:
            subprocess.run(['bash', 'tools/cloud/build_device.sh', '--with-updater'], cwd=ROOT, check=True)
        device.package(toolchain=Path(os.environ.get('JIELI_TOOLCHAIN', '/workspace/toolchains/jieli/toolchain')).resolve(), sdk=Path(os.environ.get('AC79_SDK', '/workspace/deps/ac79-sdk')).resolve())
        print(json.dumps(assemble(), indent=2))
    except (ValueError, KeyError, OSError, subprocess.SubprocessError, web.DeploymentError) as exc:
        parser.exit(1, f'Installer packaging refused: {exc}\n')
if __name__ == '__main__':
    main()
