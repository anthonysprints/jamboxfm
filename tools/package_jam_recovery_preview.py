#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Assemble the separate saving trial site; never publishes or accesses a device.

Builds/verifies the recovery target and its corresponding source by default.
--no-build reuses the current target, with the same provenance/ELF/format checks.
Only build/jambox-recovery-preview is replaced; the normal site stays separate.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import package_jam_recovery as recovery
import publish_jam_recovery_preview as preview

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build/jambox-recovery-preview"
LANDING = b'''<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only -->
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex"><title>JamBoxFM saving trial</title>
<meta http-equiv="refresh" content="0;url=install/">
<h1>JamBoxFM saving trial</h1><p><a href="install/">Open the separate test installer</a>.</p>
<p>Manual saving only. This development build still needs a physical FM-1 check.</p></html>
'''


def assemble(bundle: Path = recovery.OUTPUT, output: Path = OUTPUT) -> dict:
    """Validate one snapshot, stage it, validate again, then atomically replace."""
    if bundle.is_symlink() or not bundle.is_dir():
        raise ValueError("Missing or symlink recovery bundle")
    bundle_files = {}
    for path in bundle.rglob("*"):
        name = path.relative_to(bundle).as_posix()
        if path.is_symlink():
            raise ValueError("Recovery bundle symlinks are forbidden")
        if path.is_dir():
            if name not in {"licenses"}:
                raise ValueError("Unexpected recovery bundle directory: " + name)
            continue
        if name not in recovery.BUNDLE_FILES or not path.is_file():
            raise ValueError("Unexpected recovery bundle entry: " + name)
        bundle_files[name] = path.read_bytes()
    recovery.validate_bundle_files(bundle_files)
    files = {"firmware/" + name: data for name, data in bundle_files.items()}
    files.update({target: (ROOT / source).read_bytes() for target, source in preview.RUNTIME_SOURCE.items()})
    files.update({"index.html": LANDING, ".nojekyll": b""})
    files["SHA256SUMS"] = "".join(f"{preview.sha(data)}  {name}\n"
        for name, data in sorted(files.items())).encode()
    preview.validate_snapshot(files)
    preview.validate_bundle(files)
    if output.is_symlink():
        raise ValueError("Refusing a symlink preview output")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".jambox-recovery-preview-", dir=output.parent) as temporary:
        stage = Path(temporary) / "site"
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        preview.validate_site(stage)
        if output.exists():
            shutil.rmtree(output)
        stage.rename(output)
    return {"directory": str(output), "files": len(files),
        "sha256sums_sha256": preview.sha(files["SHA256SUMS"])}


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
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, preview.DeploymentError) as error:
        parser.exit(1, f"Recovery preview packaging refused: {error}\n")


if __name__ == "__main__":
    main()
