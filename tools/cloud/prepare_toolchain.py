#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fetch the official JieLi compiler with verified HTTPS; pin the acquired bytes.

The inspected vendor fetch instructions publish no digest/signature. The lock
records the first TLS-verified acquisition for subsequent reproducibility; it
does not claim independent vendor authentication. All artifacts and the lock
stay under --base, outside the repository by default.
"""

import argparse
import hashlib
import json
import os
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


def prepare(base):
    base.mkdir(parents=True, exist_ok=True)
    archive = base / "jieli-linux-toolchain.tar.xz"
    lock = base / "toolchain-lock.json"
    pin = json.loads(lock.read_text()) if lock.is_file() else None
    if archive.exists():
        if pin is None:
            raise ValueError("Cached compiler archive has no acquisition lock; refusing to adopt it blindly")
        if digest_file(archive) != pin["archive_sha256"]:
            raise ValueError("Cached compiler archive checksum mismatch")
    else:
        part = base / "jieli-linux-toolchain.tar.xz.partial"
        try:
            response = subprocess.run(
                ["curl", "--fail", "--location", "--show-error", "--silent",
                 "--connect-timeout", "30", "--max-time", "600", "--retry", "2",
                 "--proto", "=https", "--proto-redir", "=https", "--output", str(part),
                 "--write-out", "%{url_effective}", SOURCE_URL],
                check=True, capture_output=True, text=True,
            )
        except subprocess.CalledProcessError as exc:
            part.unlink(missing_ok=True)
            raise ValueError(
                "Official compiler download failed with TLS verification enabled: "
                + exc.stderr.strip()
                + "; allow pkgman.jieliapp.com in runtime networking and retry after it changes"
            ) from exc
        digest = digest_file(part)
        if pin is not None and digest != pin["archive_sha256"]:
            part.unlink()
            raise ValueError("Vendor URL returned different bytes from the pin; no replacement installed")
        part.rename(archive)
        if pin is None:
            pin = {
                "source_url": SOURCE_URL,
                "resolved_url": response.stdout.strip(),
                "archive_sha256": digest,
                "archive_size": archive.stat().st_size,
                "acquisition_trust": "Verified HTTPS from documented official vendor endpoint; local digest pins acquired bytes, not a vendor signature",
            }
            write_lock(lock, pin)
    with tarfile.open(archive, "r:xz") as bundle:
        roots = {Path(member.name).parts[0] for member in bundle.getmembers() if Path(member.name).parts}
        if len(roots) != 1 or not next(iter(roots)).startswith("jieli-linux-toolchains-"):
            raise ValueError("Unexpected compiler archive root layout; refused extraction")
        rootname = next(iter(roots))
        target = base / rootname
        if not target.exists():
            with tempfile.TemporaryDirectory(prefix="extract-", dir=base) as staging:
                bundle.extractall(staging, filter="data")
                unpacked = Path(staging) / rootname
                if not (unpacked / "pi32v2/bin/clang").is_file():
                    raise ValueError("Compiler archive has no pi32v2 clang")
                unpacked.rename(target)
    for relative in REQUIRED_TOOLS:
        if not os.access(target / relative, os.X_OK):
            raise ValueError(f"Required compiler executable missing: {target / relative}")
    version = subprocess.check_output([str(target / "pi32v2/bin/clang"), "--version"], text=True).strip()
    link = base / "toolchain"
    if link.is_symlink():
        if link.resolve() != target.resolve():
            raise ValueError(f"Existing compiler symlink points elsewhere: {link}; left unchanged")
    elif link.exists():
        raise ValueError(f"Existing nonsymlink compiler path: {link}; left unchanged")
    else:
        link.symlink_to(target.name, target_is_directory=True)
    pin.update({"directory": rootname, "compiler_version": version, "JIELI_TOOLCHAIN": str(link)})
    write_lock(lock, pin)
    print(f"JIELI_TOOLCHAIN={link}\n{version}\narchive SHA256 {pin['archive_sha256']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=Path("/workspace/toolchains/jieli"))
    args = parser.parse_args()
    try:
        prepare(args.base.resolve())
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Compiler setup failed: {exc}\n")


if __name__ == "__main__":
    main()
