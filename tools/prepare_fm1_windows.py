#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare reviewed FM-1 recovery sources in a NEW private folder.

Downloads and extracts pinned source archives, copies JamBox's local helpers,
and applies their reviewed source fixes. Never runs downloaded code, installs
dependencies, opens a device, uploads a loader, or writes firmware.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile
import time
import urllib.request
import zipfile


SOURCE_DIR = Path(__file__).resolve().parent
MAX_ARCHIVE_BYTES = 4 * 1024 * 1024
MAX_EXPANDED_BYTES = 16 * 1024 * 1024
MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_ENTRIES = 512
ARCHIVES = (
    {
        "name": "unbricker",
        "repository": "Quixotic7/MvaveFM1Unbricker",
        "revision": "df394beea4c881da4161fbc8a87730050b57968c",
        "root": "MvaveFM1Unbricker-df394beea4c881da4161fbc8a87730050b57968c",
        "size_bytes": 144170,
        "sha256": "181c34f9668e28874f83b06a0999460a692c68f0d9cbf5c4901c69cdd1a32479",
    },
    {
        "name": "jl-uboot-tool",
        "repository": "kagaimiq/jl-uboot-tool",
        "revision": "adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a",
        "root": "jl-uboot-tool-adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a",
        "size_bytes": 453910,
        "sha256": "21823aac1f0aabd3d9f76aca7c1a841a310bb01d92fa5c9e93e7eab5ec5a7026",
    },
)
HELPERS = {
    "fm1_stock_rom.py": "fm1_stock_rom.py",
    "prepare_fm1_backup.py": "prepare_fm1_backup.py",
    "fm1_windows_requirements.txt": "requirements.txt",
}
RESERVED_NAMES = {"CON", "PRN", "AUX", "NUL", *(
    f"{prefix}{number}" for prefix in ("COM", "LPT") for number in range(1, 10)
)}


class PreparationError(Exception):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def download_archive(spec: dict) -> bytes:
    url = f"https://codeload.github.com/{spec['repository']}/zip/{spec['revision']}"
    request = urllib.request.Request(url, headers={"User-Agent": "JamBoxFM-source-preparation"})
    chunks, size, deadline = [], 0, time.monotonic() + 45
    with urllib.request.urlopen(request, timeout=15) as response:
        declared = response.headers.get("Content-Length")
        if declared is not None:
            try:
                valid_size = int(declared) == spec["size_bytes"]
            except ValueError:
                valid_size = False
            if not valid_size:
                raise PreparationError(f"{spec['name']}: archive download size changed")
        while True:
            if time.monotonic() > deadline:
                raise PreparationError(f"{spec['name']}: archive download timed out")
            # read1 performs at most one buffered/raw read, allowing the wall
            # deadline to be checked even if the server trickles its body.
            chunk = response.read1(65536)
            if not chunk:
                break
            size += len(chunk)
            if size > min(MAX_ARCHIVE_BYTES, spec["size_bytes"]):
                raise PreparationError(f"{spec['name']}: archive download exceeded its size limit")
            chunks.append(chunk)
    return b"".join(chunks)


def verify_archive(data: bytes, spec: dict) -> None:
    if not isinstance(data, bytes) or len(data) > MAX_ARCHIVE_BYTES:
        raise PreparationError(f"{spec['name']}: archive exceeds size limit")
    if len(data) != spec["size_bytes"] or sha256(data) != spec["sha256"]:
        raise PreparationError(f"{spec['name']}: archive integrity check failed; nothing was prepared")


def _archive_entries(archive: zipfile.ZipFile, prefix: str):
    entries = archive.infolist()
    if not entries or len(entries) > MAX_ENTRIES:
        raise PreparationError("Archive contains an unexpected number of entries")
    total, paths, checked = 0, {}, []
    for entry in entries:
        name = entry.filename
        mode = stat.S_IFMT(entry.external_attr >> 16)
        if (entry.flag_bits & 1 or mode not in (0, stat.S_IFREG, stat.S_IFDIR)
                or entry.external_attr & 0x400):
            raise PreparationError("Archive contains encrypted, linked, or special entries")
        if not name or "\\" in name or len(name) > 512:
            raise PreparationError("Archive contains an unsafe path")
        parts = name[:-1].split("/") if entry.is_dir() else name.split("/")
        if parts[0] != prefix:
            raise PreparationError("Archive has an unexpected root directory")
        for part in parts:
            if (part in ("", ".", "..") or any(ord(c) < 32 for c in part)
                    or any(c in '<>:"|?*' for c in part) or part.endswith((".", " "))
                    or part.split(".", 1)[0].upper() in RESERVED_NAMES):
                raise PreparationError("Archive contains an unsafe path")
        if mode == stat.S_IFDIR and not entry.is_dir():
            raise PreparationError("Archive directory metadata disagrees with its path")
        if entry.is_dir() and (entry.file_size or mode == stat.S_IFREG):
            raise PreparationError("Archive directory contains file data")
        if not 0 <= entry.file_size <= MAX_FILE_BYTES:
            raise PreparationError("Archive file exceeds its size limit")
        total += entry.file_size
        if total > MAX_EXPANDED_BYTES:
            raise PreparationError("Archive expanded size exceeds its limit")
        key = "/".join(parts).casefold()
        if key in paths:
            raise PreparationError("Archive contains duplicate or case-colliding paths")
        paths[key] = entry.is_dir()
        if len(parts) == 1:
            if not entry.is_dir():
                raise PreparationError("Archive root is not a directory")
            continue
        checked.append((entry, PurePosixPath(*parts[1:])))
    for key in paths:
        parent = PurePosixPath(key).parent
        while str(parent) != ".":
            if str(parent) in paths and not paths[str(parent)]:
                raise PreparationError("Archive file conflicts with a parent directory")
            parent = parent.parent
    return checked


def extract_archive(data: bytes, spec: dict, destination: Path) -> None:
    """Validate the pinned archive before creating its previously absent root."""
    verify_archive(data, spec)
    if os.path.lexists(destination):
        raise PreparationError("Extraction destination already exists")
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            entries = _archive_entries(archive, spec["root"])
            destination.mkdir(mode=0o700)
            for entry, relative in entries:
                target = destination.joinpath(*relative.parts)
                if entry.is_dir():
                    target.mkdir(mode=0o700, parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                with archive.open(entry) as source, target.open("xb") as output:
                    count = 0
                    while chunk := source.read(65536):
                        count += len(chunk)
                        if count > entry.file_size or count > MAX_FILE_BYTES:
                            raise PreparationError("Expanded file exceeded its declared size")
                        output.write(chunk)
                    if count != entry.file_size:
                        raise PreparationError("Expanded file was truncated")
    except (zipfile.BadZipFile, RuntimeError) as error:
        raise PreparationError(f"Archive extraction failed: {error}") from error


def _load_patcher(path: Path):
    # Only a copy of the checked-out JamBox helper is imported. Downloaded
    # Python is data; its directory is never added to the import path.
    spec = importlib.util.spec_from_file_location("_jambox_backup_preparer", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.prepare


def prepare(destination: Path, *, source_dir: Path = SOURCE_DIR,
            downloader=None, patcher=None) -> dict:
    destination = Path(os.path.abspath(destination.expanduser()))
    if os.path.lexists(destination):
        raise PreparationError("Destination already exists; choose a NEW private folder")
    if not destination.parent.is_dir():
        raise PreparationError("Destination's parent folder must already exist")
    for name in HELPERS:
        source = source_dir / name
        if source.is_symlink() or not source.is_file():
            raise PreparationError(f"Missing local JamBox helper: {name}")
    downloader = downloader or download_archive
    # TemporaryDirectory owns only its randomly named sibling. Never remove an
    # existing user directory, or the final destination, on failure.
    with tempfile.TemporaryDirectory(prefix=".jambox-prepare-", dir=destination.parent) as tmp:
        stage = Path(tmp) / "prepared"
        stage.mkdir(mode=0o700)
        archives_dir = stage / "source-archives"
        archives_dir.mkdir(mode=0o700)
        for spec in ARCHIVES:
            data = downloader(spec)
            verify_archive(data, spec)
            (archives_dir / f"{spec['name']}-{spec['revision']}.zip").write_bytes(data)
            output = stage / "unbricker"
            if spec["name"] == "jl-uboot-tool":
                output /= "jl-uboot-tool"
            extract_archive(data, spec, output)
        unbricker = stage / "unbricker"
        jl_dir = unbricker / "jl-uboot-tool"
        with (jl_dir / ".fm1-unbrick-commit").open("x", encoding="ascii") as marker:
            marker.write(ARCHIVES[1]["revision"] + "\n")
        for name, target in HELPERS.items():
            shutil.copyfile(source_dir / name, stage / target)
        patcher_api = patcher or _load_patcher(stage / "prepare_fm1_backup.py")
        patched = patcher_api(unbricker, jl_dir)
        verified = patcher_api(unbricker, jl_dir, check=True)
        if not patched.get("prepared") or not verified.get("prepared"):
            raise PreparationError("Source fixes did not pass their independent check")
        report = {
            "schema_version": 1, "prepared": True, "hardware_accessed": False,
            "dependencies_installed": False, "downloaded_code_executed": False,
            "archives": [dict(spec) for spec in ARCHIVES],
            "helpers": [{"file": target, "sha256": sha256((stage / target).read_bytes())}
                        for target in HELPERS.values()],
            "source_fixes": verified,
        }
        (stage / "preparation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        # copytree's exclusive root mkdir refuses a destination created during
        # preparation too. No dirs_exist_ok, no merge and no overwrite fallback.
        try:
            shutil.copytree(stage, destination)
        except FileExistsError as error:
            raise PreparationError("Destination appeared during preparation; it was not overwritten") from error
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True, help="NEW private folder in an existing parent")
    args = parser.parse_args(argv)
    try:
        prepare(args.destination)
    except Exception as error:
        print(f"Preparation stopped: {error}", file=sys.stderr)
        print("No hardware was accessed. Existing folders were not removed. Choose a new destination if a partial folder remains.", file=sys.stderr)
        return 1
    print(f"Reviewed sources prepared in: {args.destination.absolute()}")
    print("No device was accessed and no dependencies were installed.")
    print("Next, open PowerShell in that folder and prepare a local Python environment:")
    print("  py -3.12 -B -m venv .venv")
    print(r"  .\.venv\Scripts\python.exe -B -m pip install -r requirements.txt")
    print("Keep the entire private folder, including its source archives and licenses.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
