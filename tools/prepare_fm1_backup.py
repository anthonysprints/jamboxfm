#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare the pinned FM-1 backup tool locally; no network, pip, USB or firmware actions.

Patch fragments derive from kagaimiq/jl-uboot-tool (MIT, Andrey Grigoryev)
and MvaveFM1Unbricker (MIT, Michael P Jones). Preserve their LICENSE files in
the prepared checkout. This helper and its tests are JamBox GPL-3.0-only.

Only exact audited source bytes or their known patched hashes are accepted.
The RAM loader hash is checked, not its executable behavior. Python package
installation/version locking is performed separately by the Windows preparer.

MIT notices for the derived patch fragments:
Copyright (c) 2023 Andrey Grigoryev (kagaimiq)
Copyright (c) 2026 Michael P Jones

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

UNBRICKER_COMMIT = "df394beea4c881da4161fbc8a87730050b57968c"
JLUB_COMMIT = "adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a"
MARKER = ".fm1-unbrick-commit"

FILES = [{'tree': 'jlub',
  'path': 'requirements.txt',
  'original': 'd67f6908839964dcd0e0b3808c3008f9811fd760f583495cb02460882bb7caa6',
  'patched': 'd67f6908839964dcd0e0b3808c3008f9811fd760f583495cb02460882bb7caa6'},
 {'tree': 'jlub',
  'path': 'data/chips.yaml',
  'original': '6e1dba69c85c72ff18a889281e158f10b1ff959bbff20895b29e268e1e382d24',
  'patched': '6e1dba69c85c72ff18a889281e158f10b1ff959bbff20895b29e268e1e382d24'},
 {'tree': 'jlub',
  'path': 'data/usb-loaders.yaml',
  'original': '2c806452cca5cc953ac8e8244c62da7f8c07648f56aa0ab943d62366571f5631',
  'patched': '2c806452cca5cc953ac8e8244c62da7f8c07648f56aa0ab943d62366571f5631'},
 {'tree': 'jlub',
  'path': 'jltech/uboot.py',
  'original': 'd82cfe46e65b1d632de6f29cb48918524bef1644fa8faae6d89fba48193eb789',
  'patched': '07be3fe0a8a61606652d9427ce5244327c72ce2799533ebb5bc8bbc1cc48776c'},
 {'tree': 'jlub',
  'path': 'jltech/cipher.py',
  'original': '7a25b514866a2e563818bb91469b030d3d47f8ad0d944b1d5a2c5d9e1ae810b3',
  'patched': '7a25b514866a2e563818bb91469b030d3d47f8ad0d944b1d5a2c5d9e1ae810b3'},
 {'tree': 'jlub',
  'path': 'jltech/crc.py',
  'original': 'd400960a1a80ef993fc0108610173214c78ae4a9708d137ca6c9eb87c58d6a44',
  'patched': 'd400960a1a80ef993fc0108610173214c78ae4a9708d137ca6c9eb87c58d6a44'},
 {'tree': 'jlub',
  'path': 'jltech/utils.py',
  'original': 'f49e770b622768b7d26d30f2ff7808bb9f0a4ad1bfd1b9c0b8033fb3224af0b9',
  'patched': 'f49e770b622768b7d26d30f2ff7808bb9f0a4ad1bfd1b9c0b8033fb3224af0b9'},
 {'tree': 'jlub',
  'path': 'scsiio/__init__.py',
  'original': 'b4a753733386f5347f95b422625b3ab377d3dfb33e007059cbcf71d5df65a837',
  'patched': 'b4a753733386f5347f95b422625b3ab377d3dfb33e007059cbcf71d5df65a837'},
 {'tree': 'jlub',
  'path': 'scsiio/common.py',
  'original': '169846e27387011097e628f74ee26a268854026fbc81675becac1762839b6603',
  'patched': '169846e27387011097e628f74ee26a268854026fbc81675becac1762839b6603'},
 {'tree': 'jlub',
  'path': 'scsiio/win32port.py',
  'original': 'a00d0530f03edd1beba9e08d6786c8e87437d0d7821d731b7d6e54be5a1860ee',
  'patched': 'f9e11c39bb1d45bb2921a557681f7ff90e944b7701b999f7459ec27af693d713'},
 {'tree': 'jlub',
  'path': 'scsiio/linuxport.py',
  'original': 'caff4cd2ecd92f75d07aae3e873b02180f5f49791fbafa05cf7bdc734bb805da',
  'patched': 'caff4cd2ecd92f75d07aae3e873b02180f5f49791fbafa05cf7bdc734bb805da'},
 {'tree': 'unbricker',
  'path': 'fm1_unbrick.py',
  'original': 'c17d866d1054ca7d23c6ad8e633c1ec3aabd550b2e66c32ff7fc09ec12b5bf1b',
  'patched': '7be4a716d885bfb7dd680f5c70997676e6552bcaee60e5c66b1313f22720cd45'},
 {'tree': 'jlub',
  'path': 'data/loaderblobs/usb/wl82loader.bin',
  'original': 'd41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376',
  'patched': 'd41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376'}]

PATCHES = {'jltech/uboot.py': [('        self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, resp)\n'
                      '\n'
                      '        rcmd',
                      '        residual = self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, '
                      'resp)\n'
                      '        if type(residual) is not int or residual != 0:\n'
                      '            raise SCSIException("incomplete command response: residual %r" % '
                      '(residual,))\n'
                      '\n'
                      '        rcmd'),
                     ('            assert(cmd == rcmd)',
                      '            if cmd != rcmd:\n'
                      '                raise SCSIException("response command mismatch: expected %04X, '
                      'got %04X" % (cmd, rcmd))'),
                     ('        self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, data)\n'
                      '        return bytes(data)',
                      '        residual = self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, '
                      'data)\n'
                      '        if type(residual) is not int or residual != 0:\n'
                      '            raise SCSIException("incomplete data read: residual %r" % '
                      '(residual,))\n'
                      '        return bytes(data)'),
                     ('        self.dev.execute(self.cmd_prepare_cdb(cmd, args), data, None)',
                      '        residual = self.dev.execute(self.cmd_prepare_cdb(cmd, args), data, '
                      'None)\n'
                      '        if type(residual) is not int or residual != 0:\n'
                      '            raise SCSIException("incomplete data write: residual %r" % '
                      '(residual,))')],
 'scsiio/win32port.py': [('LPSECURITY_ATTRIBUTES = DWORD', 'LPSECURITY_ATTRIBUTES = ctypes.c_void_p'),
                         ("kernel32 = ctypes.WinDLL('kernel32')",
                          "kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)"),
                         ('GENERIC_READ     = 1\nGENERIC_WRITE    = 2',
                          'GENERIC_READ     = 0x80000000\nGENERIC_WRITE    = 0x40000000'),
                         ('raise ctypes.WinError()', 'raise ctypes.WinError(ctypes.get_last_error())'),
                         ("raise SCSIException('Transfer failed: %s' % ctypes.WinError())",
                          "raise SCSIException('Transfer failed: %s' % "
                          'ctypes.WinError(ctypes.get_last_error()))'),
                         ('        sptd = SCSI_PASS_THROUGH_DIRECT()',
                          '        if not 0 < len(cdb) <= 16:\n'
                          "            raise SCSIException('SCSI command must contain 1..16 bytes')\n"
                          '        sptd = SCSI_PASS_THROUGH_DIRECT()'),
                         ("raise NotImplemented('Indirect I/O is not supported')",
                          "raise NotImplementedError('Indirect I/O is not supported')"),
                         ('#DeviceIoControl.argtypes = [HANDLE, DWORD, ctypes.c_voidp, DWORD, '
                          'ctypes.c_voidp, DWORD, ctypes.POINTER(DWORD), DWORD]',
                          'DeviceIoControl.argtypes = [HANDLE, DWORD, ctypes.c_void_p, DWORD, '
                          'ctypes.c_void_p,\n'
                          '                            DWORD, ctypes.POINTER(DWORD), ctypes.c_void_p]'),
                         ('        if data_in:\n            inlen = sptd.data_len',
                          '        if xlen.value != ctypes.sizeof(SCSI_PASS_THROUGH_DIRECT):\n'
                          '            raise SCSIException("incomplete SCSI control result: %d of %d '
                          'bytes" %\n'
                          '                                (xlen.value, '
                          'ctypes.sizeof(SCSI_PASS_THROUGH_DIRECT)))\n'
                          '\n'
                          '        if sptd.status != 0:\n'
                          '            raise SCSIException("SCSI command failed: status 0x%02X" % '
                          'sptd.status)\n'
                          '        if sptd.data_len != datalen:\n'
                          '            raise SCSIException("incomplete SCSI transfer: %d of %d bytes" '
                          '%\n'
                          '                                (sptd.data_len, datalen))\n'
                          '\n'
                          '        if data_in:\n'
                          '            inlen = sptd.data_len')],
 'fm1_unbrick.py': [('    problems = []\n    if key != EXPECTED_CHIP_KEY:',
                     '    problems = []\n'
                     '    if ondev.get("type") != 3:\n'
                     '        problems.append("online device type %r is not SPI0 NOR (3)" % '
                     'ondev.get("type"))\n'
                     '    if key != EXPECTED_CHIP_KEY:')]}

class PreparationError(Exception):
    """Unexpected or incomplete source tree; nothing else should be run."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def patched_bytes(path: str, original: bytes) -> bytes:
    """Apply reviewed exact replacements; callers verify both whole-file hashes."""
    result = original
    for before, after in PATCHES.get(path, ()):
        old, new = before.encode("utf-8"), after.encode("utf-8")
        if result.count(old) != 1:
            raise PreparationError(f"patch preimage is not unique: {path}")
        result = result.replace(old, new)
    return result


def checked_file(root: Path, relative: str) -> Path:
    path = root / relative
    if root.is_symlink() or any(p.is_symlink() for p in (path, *path.parents) if p != root.parent):
        raise PreparationError(f"symlink is not accepted: {path}")
    if not path.is_file():
        raise PreparationError(f"required file missing: {path}")
    if not path.resolve().is_relative_to(root.resolve()):
        raise PreparationError(f"file escapes the selected tree: {path}")
    return path


def atomic_replace(path: Path, data: bytes) -> None:
    """Preserve raw UTF-8/LF bytes on Windows; never use text newline conversion."""
    fd, name = tempfile.mkstemp(prefix=path.name + ".jambox-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def prepare(unbricker: Path, jl_dir: Path, *, check: bool = False) -> dict:
    """Validate the full critical tree, then prepare it or check it without writes.

    Known mixed original/patched states can be repaired after an interruption.
    A partial prepared tree is refused by --check. Unknown bytes are never
    overwritten. This does not execute any downloaded Python or binary code.
    """
    roots = {"unbricker": Path(unbricker).absolute(), "jlub": Path(jl_dir).absolute()}
    for root in roots.values():
        if not root.is_dir():
            raise PreparationError(f"source directory missing: {root}")
    marker = roots["jlub"] / MARKER
    # The Windows downloader writes this only after validating its pinned ZIP.
    # Git checkouts may omit it; exact source hashes below remain authoritative.
    if marker.exists():
        marker = checked_file(roots["jlub"], MARKER)
        if marker.read_bytes().strip() != JLUB_COMMIT.encode("ascii"):
            raise PreparationError(f"unexpected jl-uboot-tool commit marker: {marker}")
    elif not (roots["jlub"] / ".git").is_dir():
        raise PreparationError(f"pinned ZIP checkout is missing {MARKER}")

    # Python can prefer compiled extensions or load sourceless bytecode over
    # the source we hashed. Use python -B for later backup commands.
    artifacts = (".pyc", ".pyo", ".pyd", ".so")
    allowed_top = {
        "jlub": {"jldevfind.py", "jlrunner.py", "jluboottool.py"},
        "unbricker": {"fm1_unbrick.py", "fm1_transporter_recover.py"},
    }
    for name, root in roots.items():
        for path in root.iterdir():
            if path.is_file() and (path.name.endswith(artifacts) or
                                  path.suffix == ".py" and path.name not in allowed_top[name]):
                raise PreparationError(f"unexpected import artifact: {path}")
            if path.is_dir() and (path.name in ("yaml", "crcmod", "__pycache__") or
                                  (path / "__init__.py").exists() and
                                  not (name == "jlub" and path.name in ("jltech", "scsiio")) and
                                  not (name == "unbricker" and path.name == "tests" and
                                       (path / "__init__.py").is_file() and
                                       sha256((path / "__init__.py").read_bytes()) ==
                                       "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")):
                raise PreparationError(f"unexpected import package or cache: {path}")
    for directory in ("jltech", "scsiio"):
        for path in (roots["jlub"] / directory).rglob("*"):
            if path.is_file() and path.name.endswith(artifacts):
                raise PreparationError(f"compiled import artifact is not accepted; use python -B: {path}")

    expected_python = {r["path"] for r in FILES if r["tree"] == "jlub" and r["path"].endswith(".py")}
    for directory in ("jltech", "scsiio"):
        for path in (roots["jlub"] / directory).rglob("*.py"):
            if path.relative_to(roots["jlub"]).as_posix() not in expected_python:
                raise PreparationError(f"unexpected importable source: {path}")

    pending, report = [], []
    for record in FILES:
        path = checked_file(roots[record["tree"]], record["path"])
        data = path.read_bytes()
        digest = sha256(data)
        if digest not in (record["original"], record["patched"]):
            raise PreparationError(f"unrecognized bytes in {path}; sha256 {digest}. Use a fresh pinned download.")
        needs_patch = record["original"] != record["patched"] and digest == record["original"]
        if needs_patch:
            if check:
                raise PreparationError(f"preparation incomplete: {path} still has upstream bytes")
            changed = patched_bytes(record["path"], data)
            if sha256(changed) != record["patched"]:
                raise PreparationError(f"internal patch hash mismatch: {path}")
            pending.append((path, data, changed))
        report.append({"tree": record["tree"], "path": record["path"],
                       "sha256": record["patched"],
                       "state": "patched" if record["original"] != record["patched"] else "verified"})

    # Preflight every source before modifying any source; re-check all inputs
    # once immediately before replacing files to detect concurrent edits.
    for path, original, _ in pending:
        if path.read_bytes() != original:
            raise PreparationError(f"source changed during preparation: {path}")
    for path, _, changed in pending:
        atomic_replace(path, changed)
    for record in FILES:
        path = roots[record["tree"]] / record["path"]
        if sha256(path.read_bytes()) != record["patched"]:
            raise PreparationError(f"final verification failed: {path}; run preparation again before backup")
    return {"schema_version": 1, "prepared": True, "check_only": check,
            "unbricker_commit": UNBRICKER_COMMIT, "jlub_commit": JLUB_COMMIT,
            "files": report, "flash_actions": False, "network_actions": False,
            "note": "Source and RAM-loader hash checks only; no device connected or loader executed."}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unbricker", type=Path, required=True)
    parser.add_argument("--jl-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="verify a fully prepared tree; change no files")
    args = parser.parse_args(argv)
    try:
        result = prepare(args.unbricker, args.jl_dir, check=args.check)
    except (PreparationError, OSError) as exc:
        print(f"Backup preparation refused: {exc}")
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
