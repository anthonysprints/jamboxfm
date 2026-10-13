#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Recreate the verified Debian browser/native toolchain without a system install.

Run on Debian 13 amd64 with Python 3, Node, binutils, libc6-dev, and GCC's C
headers/runtime already available. All downloads and extracted files stay below
/workspace/toolchains/debian-browser unless --prefix selects another writable root.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import urllib.request

HERE = Path(__file__).resolve().parent

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for part in iter(lambda: f.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, default=Path('/workspace/toolchains/debian-browser'))
    args = parser.parse_args()
    base = args.prefix.resolve()
    node = shutil.which('node')
    if not node or not shutil.which('dpkg-deb'):
        raise SystemExit('Node and dpkg-deb are required.')
    manifest = json.loads((HERE / 'packages.json').read_text())
    downloads, root = base / 'downloads', base / 'root'
    downloads.mkdir(parents=True, exist_ok=True)
    root.mkdir(parents=True, exist_ok=True)
    stamp_path = base / 'installed-packages.json'
    installed = json.loads(stamp_path.read_text()) if stamp_path.exists() else {}
    for package in manifest:
        archive = downloads / package['file']
        if archive.exists() and digest(archive) != package['sha256']:
            raise SystemExit(f'Checksum mismatch: {archive}; refusing to use or replace it.')
        if not archive.exists():
            temporary = archive.with_suffix('.download')
            print(f"Downloading {package['package']} {package['version']}", flush=True)
            # urllib retains normal TLS certificate and hostname verification.
            with urllib.request.urlopen(package['url'], timeout=60) as response, temporary.open('wb') as output:
                shutil.copyfileobj(response, output, length=1024 * 1024)
            if digest(temporary) != package['sha256']:
                raise SystemExit(f'Checksum mismatch: {temporary}; refusing to extract.')
            temporary.replace(archive)
        if installed.get(package['package']) != package['sha256']:
            print(f"Extracting verified {package['package']}", flush=True)
            subprocess.run(['dpkg-deb', '--extract', str(archive), str(root)], check=True)
            installed[package['package']] = package['sha256']
            stamp_path.write_text(json.dumps(installed, indent=2) + '\n')
    config = base / 'emscripten-config.py'
    config.write_text(
        f"LLVM_ROOT = {str(root / 'usr/lib/llvm-19/bin')!r}\n"
        f"BINARYEN_ROOT = {str(root / 'usr')!r}\n"
        f"NODE_JS = {node!r}\n"
        "LLVM_ADD_VERSION = None\nCLANG_ADD_VERSION = None\n"
        f"CACHE = {str(root / 'usr/share/emscripten/cache')!r}\n"
        "FROZEN_CACHE = True\n")
    (base / 'env.sh').write_text(
        '# Source this file before building browser or native preview.\n'
        f'export PATH="{root}/usr/share/emscripten:{root}/usr/lib/llvm-19/bin:{root}/usr/bin:$PATH"\n'
        f'export LD_LIBRARY_PATH="{root}/usr/lib/x86_64-linux-gnu${{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}}"\n'
        f'export NODE_PATH="{root}/usr/share/nodejs${{NODE_PATH:+:$NODE_PATH}}"\n'
        f'export EM_CONFIG="{config}"\n'
        f'export CC="{root}/usr/lib/llvm-19/bin/clang"\n')
    print(f'Ready: source {base / "env.sh"}')

if __name__ == '__main__':
    main()
