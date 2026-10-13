#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Acquire the JieLi compiler through official HTTPS or an owner-provided archive.

The inspected vendor fetch instructions publish no digest/signature. The lock
records the acquisition method and bytes for subsequent reproducibility; it
does not claim independent vendor authentication. --expected-sha256 checks a
previously recorded digest. All artifacts and the lock stay under --base,
outside the repository by default.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path


SOURCE_URL = "https://pkgman.jieliapp.com/s/linux-toolchain"
REQUIRED_TOOLS = ("pi32v2/bin/clang", "pi32v2/bin/ld", "common/bin/objcopy", "common/bin/objdump")


def digest_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_lock(path, data):
    temporary = path.with_suffix(".json.partial")
    temporary.write_text(json.dumps(data, indent=2) + "\n")
    temporary.replace(path)


def sha256_argument(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", value):
        raise ValueError("Expected SHA-256 must contain exactly 64 hexadecimal characters")
    return value.lower()


def tree_inventory(root):
    """Hash a release tree without following links outside that release."""
    if root.is_symlink() or not root.is_dir():
        raise ValueError(f"Compiler release root is not a real directory: {root}")
    real_root = root.resolve()
    inventory = {}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            if not path.resolve().is_relative_to(real_root):
                raise ValueError(f"Compiler symlink escapes its release root: {relative}")
            inventory[relative] = ("symlink", os.readlink(path))
        elif path.is_dir():
            inventory[relative] = ("directory", path.stat().st_mode & 0o777)
        elif path.is_file():
            inventory[relative] = ("file", path.stat().st_mode & 0o777, digest_file(path))
        else:
            raise ValueError(f"Unsupported compiler release entry: {relative}")
    for relative in REQUIRED_TOOLS:
        executable = root / relative
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise ValueError(f"Required compiler executable missing: {executable}")
    return inventory


def prepare(base, supplied_archive=None, expected_sha256=None):
    expected = sha256_argument(expected_sha256) if expected_sha256 is not None else None
    base.mkdir(parents=True, exist_ok=True)
    archive = base / "jieli-linux-toolchain.tar.xz"
    lock = base / "toolchain-lock.json"
    pin = json.loads(lock.read_text()) if lock.is_file() else None
    existing_pin = pin
    pinned_digest = sha256_argument(pin["archive_sha256"]) if pin is not None else None
    if expected is not None and pinned_digest is not None and expected != pinned_digest:
        raise ValueError("Requested checksum differs from the existing acquisition lock; left unchanged")
    if supplied_archive is not None:
        if not supplied_archive.is_file():
            raise ValueError(f"Supplied compiler archive is not a readable file: {supplied_archive}")
        supplied_digest = digest_file(supplied_archive)
        if expected is not None and supplied_digest != expected:
            raise ValueError("Supplied compiler archive checksum differs from --expected-sha256")
        if pinned_digest is not None and supplied_digest != pinned_digest:
            raise ValueError("Supplied compiler archive differs from the acquisition lock; left unchanged")
    if archive.exists():
        if pin is None:
            raise ValueError("Cached compiler archive has no acquisition lock; refusing to adopt it blindly")
        if digest_file(archive) != pinned_digest:
            raise ValueError("Cached compiler archive checksum mismatch")
        candidate = archive
    else:
        part = base / "jieli-linux-toolchain.tar.xz.partial"
        try:
            if supplied_archive is not None:
                shutil.copyfile(supplied_archive, part)
                acquisition = {
                    "source_kind": "owner_provided_archive",
                    "supplied_archive_name": supplied_archive.name,
                    "documented_source_url": SOURCE_URL,
                    "acquisition_trust": "Owner-provided archive; local HTTPS acquisition was not observed. SHA-256 pins the supplied bytes, not a vendor signature",
                }
            else:
                response = subprocess.run(
                    ["curl", "--fail", "--location", "--show-error", "--silent",
                     "--connect-timeout", "30", "--max-time", "600", "--retry", "2",
                     "--proto", "=https", "--proto-redir", "=https", "--output", str(part),
                     "--write-out", "%{url_effective}", SOURCE_URL],
                    check=True, capture_output=True, text=True,
                )
                acquisition = {
                    "source_kind": "official_https",
                    "source_url": SOURCE_URL,
                    "resolved_url": response.stdout.strip(),
                    "acquisition_trust": "Verified HTTPS from documented official vendor endpoint; local digest pins acquired bytes, not a vendor signature",
                }
        except subprocess.CalledProcessError as exc:
            part.unlink(missing_ok=True)
            raise ValueError(
                "Official compiler download failed with TLS verification enabled: "
                + exc.stderr.strip()
                + "; allow the vendor endpoint and its official redirect host in runtime networking, or use --archive"
            ) from exc
        digest = digest_file(part)
        if expected is not None and digest != expected:
            part.unlink()
            raise ValueError("Acquired compiler archive checksum differs from --expected-sha256; not installed")
        if pinned_digest is not None and digest != pinned_digest:
            part.unlink()
            raise ValueError("Acquired compiler archive differs from the pin; no replacement installed")
        if supplied_archive is not None and digest != supplied_digest:
            part.unlink()
            raise ValueError("Supplied compiler archive changed while being copied; not installed")
        candidate = part
        if pin is None:
            pin = {
                **acquisition,
                "archive_sha256": digest,
                "archive_size": part.stat().st_size,
            }
    try:
        with tarfile.open(candidate, "r:xz") as bundle:
            roots = {Path(member.name).parts[0] for member in bundle.getmembers() if Path(member.name).parts}
            if len(roots) != 1 or not next(iter(roots)).startswith("jieli-linux-toolchains-"):
                raise ValueError("Unexpected compiler archive root layout; refused extraction")
            rootname = next(iter(roots))
            target = base / rootname
            with tempfile.TemporaryDirectory(prefix="extract-", dir=base) as staging:
                bundle.extractall(staging, filter="data")
                unpacked = Path(staging) / rootname
                expected_tree = tree_inventory(unpacked)
                target_exists = target.exists() or target.is_symlink()
                if target_exists:
                    if existing_pin is None or existing_pin.get("directory") != rootname:
                        raise ValueError(f"Existing compiler release directory is unmanaged: {target}; left unchanged")
                    if tree_inventory(target) != expected_tree:
                        raise ValueError("Installed compiler tree differs from the pinned archive; left unchanged")
                link = base / "toolchain"
                if link.is_symlink():
                    if link.resolve() != target.resolve():
                        raise ValueError(f"Existing compiler symlink points elsewhere: {link}; left unchanged")
                elif link.exists():
                    raise ValueError(f"Existing nonsymlink compiler path: {link}; left unchanged")
                probe_root = target if target_exists else unpacked
                version = subprocess.check_output(
                    [str(probe_root / "pi32v2/bin/clang"), "--version"], text=True,
                ).strip()
                if not target_exists:
                    unpacked.rename(target)
        if candidate != archive:
            candidate.rename(archive)
        if not link.is_symlink():
            link.symlink_to(target.name, target_is_directory=True)
        pin.update({"directory": rootname, "compiler_version": version, "JIELI_TOOLCHAIN": str(link)})
        write_lock(lock, pin)
    finally:
        if candidate != archive:
            candidate.unlink(missing_ok=True)
    print(f"JIELI_TOOLCHAIN={link}\n{version}\narchive SHA256 {pin['archive_sha256']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=Path("/workspace/toolchains/jieli"))
    parser.add_argument("--archive", type=Path, help="Import an owner-provided archive instead of downloading")
    parser.add_argument("--expected-sha256", help="Require this SHA-256 for cached, imported or downloaded bytes")
    args = parser.parse_args()
    try:
        prepare(args.base.resolve(), args.archive.resolve() if args.archive else None, args.expected_sha256)
    except (OSError, ValueError, KeyError, RuntimeError, tarfile.TarError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Compiler setup failed: {exc}\n")


if __name__ == "__main__":
    main()
