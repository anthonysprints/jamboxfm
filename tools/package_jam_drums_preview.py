#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package only the separate drum-navigation trial; never publish or use MIDI.

Reuses the reviewed JamBox-only recovery candidate and its storage policy.
The normal installer, browser instrument and saving preview stay separate.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile

import package_jam_recovery as recovery
from publish_jam_web import DeploymentError, sha, source_contents, validate_hashes

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build/jambox-drums-preview"
RUNTIME_SOURCE = {
    "install/index.html": "web/drums/index.html",
    "install/style.css": "web/recovery/style.css",
    "install/app.js": "web/recovery/app.js",
    "fm1ota.js": "web/fm1ota.js",
    "fm1pkg.js": "web/fm1pkg.js",
    "jam-device-policy.js": "web/jam-device-policy.js",
    "jam-recovery-policy.js": "web/jam-recovery-policy.js",
}
EXPECTED_FILES = frozenset(RUNTIME_SOURCE) | {"index.html", ".nojekyll", "SHA256SUMS"} | {
    "firmware/" + name for name in recovery.BUNDLE_FILES}
EXPECTED_DIRECTORIES = frozenset({"install", "firmware", "firmware/licenses"})
LANDING = b'''<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only -->
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex"><title>JamBoxFM drum grid trial</title>
<meta http-equiv="refresh" content="0;url=install/">
<h1>JamBoxFM drum grid trial</h1><p><a href="install/">Open the separate drum test installer</a>.</p>
<p>Save now before updating. Manual saving stays enabled; automatic saving stays off.</p></html>
'''


def validate_snapshot(files: dict[str, bytes]) -> None:
    if set(files) != EXPECTED_FILES or any(not isinstance(data, bytes) for data in files.values()):
        raise DeploymentError("Drums preview must contain exactly the approved regular files.")
    if any(len(data) > 64 * 1024 * 1024 for data in files.values()) or sum(map(len, files.values())) > 128 * 1024 * 1024:
        raise DeploymentError("Drums preview package exceeds the supported size.")
    validate_hashes(files, "Drums preview")


def validate_bundle(files: dict[str, bytes]) -> None:
    bundle = {name: files["firmware/" + name] for name in recovery.BUNDLE_FILES}
    try:
        recovery.validate_bundle_files(bundle)
    except (ValueError, KeyError, subprocess.SubprocessError) as error:
        raise DeploymentError(f"Invalid drums recovery candidate: {error}") from error
    source = source_contents(bundle[recovery.SOURCE])
    for published, original in RUNTIME_SOURCE.items():
        if source.get(original) != files[published]:
            raise DeploymentError(f"Drums preview {published} differs from its corresponding source.")
    if files["index.html"] != LANDING or files[".nojekyll"] != b"":
        raise DeploymentError("Drums preview landing files differ from the approved trial.")
    if b"jamboxfm-source.zip" not in files["install/index.html"] or b'data-trial="drums"' not in files["install/index.html"]:
        raise DeploymentError("Drums installer must identify its trial and link corresponding firmware source.")


def validate_site(site: Path = OUTPUT) -> dict[str, bytes]:
    """Read bounded regular files through anchored descriptors, then validate."""
    site = Path(site)
    if site.is_symlink() or not site.is_dir():
        raise DeploymentError("Drums preview directory is missing or a symlink.")
    files = {}
    total = 0
    for path in site.rglob("*"):
        name = path.relative_to(site).as_posix()
        if path.is_symlink():
            raise DeploymentError(f"Drums preview symlink is forbidden: {name}")
        if path.is_dir():
            if name not in EXPECTED_DIRECTORIES:
                raise DeploymentError(f"Unexpected drums preview directory: {name}")
            continue
        if name not in EXPECTED_FILES or not path.is_file():
            raise DeploymentError(f"Unexpected drums preview file: {name}")
        descriptors = []
        try:
            descriptor = os.open(site, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            descriptors.append(descriptor)
            for part in path.relative_to(site).parts[:-1]:
                descriptor = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
                descriptors.append(descriptor)
            leaf = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=descriptor)
            descriptors.append(leaf)
            info = os.fstat(leaf)
            if not stat.S_ISREG(info.st_mode) or info.st_size > 64 * 1024 * 1024:
                raise DeploymentError(f"Drums preview file is not a bounded regular file: {name}")
            with os.fdopen(os.dup(leaf), "rb") as stream:
                files[name] = stream.read(64 * 1024 * 1024 + 1)
            total += len(files[name])
            if len(files[name]) > 64 * 1024 * 1024 or total > 128 * 1024 * 1024:
                raise DeploymentError("Drums preview package exceeds the supported size.")
        except OSError as error:
            raise DeploymentError(f"Cannot safely read drums preview file {name}: {error}") from error
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)
    validate_snapshot(files)
    validate_bundle(files)
    return files


def assemble(bundle: Path = recovery.OUTPUT, output: Path = OUTPUT) -> dict:
    if bundle.is_symlink() or not bundle.is_dir():
        raise DeploymentError("Missing or symlink recovery bundle")
    bundle_files = {}
    for path in bundle.rglob("*"):
        name = path.relative_to(bundle).as_posix()
        if path.is_symlink():
            raise DeploymentError("Recovery bundle symlinks are forbidden")
        if path.is_dir():
            if name != "licenses":
                raise DeploymentError("Unexpected recovery bundle directory: " + name)
            continue
        if name not in recovery.BUNDLE_FILES or not path.is_file() or path.stat().st_size > 64 * 1024 * 1024:
            raise DeploymentError("Unexpected recovery bundle entry: " + name)
        bundle_files[name] = path.read_bytes()
    recovery.validate_bundle_files(bundle_files)
    files = {"firmware/" + name: data for name, data in bundle_files.items()}
    files.update({target: (ROOT / source).read_bytes() for target, source in RUNTIME_SOURCE.items()})
    files.update({"index.html": LANDING, ".nojekyll": b""})
    files["SHA256SUMS"] = "".join(f"{sha(data)}  {name}\n" for name, data in sorted(files.items())).encode()
    validate_snapshot(files)
    validate_bundle(files)
    if output.is_symlink():
        raise DeploymentError("Refusing a symlink drums preview output")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".jambox-drums-preview-", dir=output.parent) as temporary:
        stage = Path(temporary) / "site"
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        validate_site(stage)
        if output.exists():
            shutil.rmtree(output)
        stage.rename(output)
    return {"directory": str(output), "files": len(files), "sha256sums_sha256": sha(files["SHA256SUMS"])}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()
    try:
        if not args.no_build:
            subprocess.run(["bash", "tools/cloud/build_device.sh", "--with-recovery"], cwd=ROOT, check=True)
        recovery.package(toolchain=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")).resolve(),
            sdk=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")).resolve())
        print(assemble())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, DeploymentError) as error:
        parser.exit(1, f"Drums preview packaging refused: {error}\n")


if __name__ == "__main__":
    main()
