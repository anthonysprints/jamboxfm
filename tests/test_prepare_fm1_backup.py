# SPDX-License-Identifier: GPL-3.0-only
"""Offline preparation/transport regressions; no downloads, pip or device access.

Pinned source fixtures below: kagaimiq/jl-uboot-tool adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a.
Copyright (c) 2023 Andrey Grigoryev (kagaimiq), MIT.
The complete MIT permission/disclaimer for these fragments is preserved in
prepare_fm1_backup.py, whose patches this test executes.
"""
import ast
import ctypes
import importlib.util
import os
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_backup", ROOT / "tools/prepare_fm1_backup.py")
pb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pb)

PROTOCOL_SOURCE = r'''class JL_MSCProtocolBase:
    """
    Common stuff for the USB Mass Storage Class-based protocols
    """

    def __init__(self, dev):
        self.dev = dev

    def cmd_prepare_cdb(self, cmd, args):
        """ Prepare command's CDB """
        cdb = cmd.to_bytes(2, 'big') + args

        # pad the rest of CDB with 0xFF's
        if len(cdb) < 16:
            cdb += b'\xff' * (16 - len(cdb))

        return cdb

    def cmd_exec(self, cmd, args, check_response=True):
        """ Execute command - no data (response is received instead) """
        resp = bytearray(16)
        self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, resp)

        rcmd = int.from_bytes(resp[:2], 'big')
        resp = bytes(resp[2:])

        if check_response:
            # Check if the command in response matched the sent one
            assert(cmd == rcmd)
            return resp
        else:
            # Return raw-ish response
            return (rcmd, resp)

    def cmd_exec_datain(self, cmd, args, dlen):
        """ Execute command - data in (receive from device) """
        data = bytearray(dlen)
        self.dev.execute(self.cmd_prepare_cdb(cmd, args), None, data)
        return bytes(data)

    def cmd_exec_dataout(self, cmd, args, data):
        """ Execute command - data out (send to device) """
        self.dev.execute(self.cmd_prepare_cdb(cmd, args), data, None)'''

WINDOWS_SOURCE = r'''from scsiio.common import *
import ctypes

from ctypes.wintypes import HANDLE, BOOL
from ctypes.wintypes import LPCWSTR, LPCSTR
from ctypes.wintypes import BYTE, WORD, DWORD

class SCSI_PASS_THROUGH_DIRECT(ctypes.Structure):
    _fields_ = [
        ('len',       WORD),
        ('status',    BYTE),
        ('path_id',   BYTE),
        ('target_id', BYTE),
        ('lun',       BYTE),
        ('cdb_len',   BYTE),
        ('sense_len', BYTE),
        ('dir',       BYTE),
        ('data_len',  DWORD),
        ('timeout',   DWORD),
        ('data_ptr',  ctypes.POINTER(BYTE)),
        ('sense_off', DWORD),
        ('cdb',       BYTE * 16)
    ]

LPSECURITY_ATTRIBUTES = DWORD

INVALID_HANDLE_VALUE = HANDLE(-1).value

GENERIC_READ     = 1
GENERIC_WRITE    = 2

FILE_SHARE_READ  = 1
FILE_SHARE_WRITE = 2
OPEN_EXISTING    = 3

IOCTL_SCSI_PASS_THROUGH_DIRECT = 0x4d014

SCSI_IOCTL_DATA_OUT         = 0
SCSI_IOCTL_DATA_IN          = 1
SCSI_IOCTL_DATA_UNSPECIFIED = 2 # maybe?

#-------------

kernel32 = ctypes.WinDLL('kernel32')

CreateFile = kernel32.CreateFileW
CreateFile.restype  = HANDLE
CreateFile.argtypes = [LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE]

CloseHandle = kernel32.CloseHandle
CloseHandle.restype  = BOOL
CloseHandle.argtypes = [HANDLE]

DeviceIoControl = kernel32.DeviceIoControl
DeviceIoControl.restype  = BOOL
#DeviceIoControl.argtypes = [HANDLE, DWORD, ctypes.c_voidp, DWORD, ctypes.c_voidp, DWORD, ctypes.POINTER(DWORD), DWORD]

#-----------------------------------------------

class SCSIDev(SCSIDevBase):
    """
    SCSI Generic I/O device implementation for Win32's SCSI_PASS_THROUGH_DIRECT ioctl
    """

    def open(self, path):
        self.fhandle = CreateFile(path, GENERIC_READ|GENERIC_WRITE,
                             FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
                             OPEN_EXISTING, 0, 0)

        if self.fhandle == INVALID_HANDLE_VALUE:
            raise ctypes.WinError()

        self.is_open = True

    def close(self):
        if self.is_open:
            if self.fhandle is not None:
                CloseHandle(self.fhandle)
                self.fhandle = None

            self.is_open = False

    def execute(self, cdb, data_out, data_in, max_sense_len=32, return_sense_buffer=False):
        sptd = SCSI_PASS_THROUGH_DIRECT()
        sptd.len = ctypes.sizeof(sptd)

        sptd.path_id   = 0
        sptd.target_id = 1
        sptd.lun       = 0

        sptd.timeout = 5 # TODO ; in seconds

        #sptd.sense_off = 0
        #sptd.sense_len = max_sense_len

        sptd.cdb_len = len(cdb)
        for i in range(sptd.cdb_len):
            sptd.cdb[i] = cdb[i]

        if data_out and data_in:
            raise NotImplemented('Indirect I/O is not supported')

        elif data_out:
            sptd.dir      = SCSI_IOCTL_DATA_OUT
            sptd.data_len = len(data_out)

            databuff = ctypes.create_string_buffer(data_out)
            sptd.data_ptr = ctypes.cast(databuff, ctypes.POINTER(BYTE))

        elif data_in:
            sptd.dir      = SCSI_IOCTL_DATA_IN
            sptd.data_len = len(data_in)

            databuff = ctypes.create_string_buffer(sptd.data_len)
            sptd.data_ptr = ctypes.cast(databuff, ctypes.POINTER(BYTE))

        else:
            sptd.dir      = SCSI_IOCTL_DATA_UNSPECIFIED

        # after the ioctl the data_len field might change to the actual amount
        # of data transferred, so use this for the residue...
        datalen = sptd.data_len

        xlen = DWORD()

        sptdp = ctypes.cast(ctypes.addressof(sptd), ctypes.POINTER(BYTE))

        res = DeviceIoControl(self.fhandle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                              sptdp, sptd.len, ctypes.byref(sptd), sptd.len,
                              ctypes.byref(xlen), 0)

        if not res:
            raise SCSIException('Transfer failed: %s' % ctypes.WinError())

        if data_in:
            inlen = sptd.data_len
            data_in[:inlen] = bytearray(databuff.raw[:inlen])

        # TODO, residue & sense buffer! #
        if return_sense_buffer:
            return datalen - sptd.data_len, b''
        else:
            return datalen - sptd.data_len
'''


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.unbricker = Path(self.tmp.name) / "unbricker"
        self.jl = self.unbricker / "jl-uboot-tool"
        self.jl.mkdir(parents=True)
        (self.jl / pb.MARKER).write_bytes(pb.JLUB_COMMIT.encode() + b"\n")
        self.records = []
        # Synthetic content exercises the complete tree/preimage/atomic-write
        # machinery. Protocol behavior below uses exact upstream source.
        for tree, path in (("unbricker", "fm1_unbrick.py"), ("jlub", "jltech/uboot.py"),
                           ("jlub", "data/chips.yaml"), ("jlub", "scsiio/win32port.py")):
            data = ("\n".join(pair[0] for pair in pb.PATCHES.get(path, [])) or "verified fixture").encode()
            target = (self.unbricker if tree == "unbricker" else self.jl) / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            self.records.append({"tree": tree, "path": path, "original": pb.sha256(data),
                                 "patched": pb.sha256(pb.patched_bytes(path, data))})
        self.patch = mock.patch.object(pb, "FILES", self.records)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def prep(self, **kw):
        return pb.prepare(self.unbricker, self.jl, **kw)

    def test_prepare_idempotent_and_check_read_only(self):
        with self.assertRaisesRegex(pb.PreparationError, "incomplete"):
            self.prep(check=True)
        report = self.prep()
        self.assertTrue(report["prepared"])
        self.assertFalse(report["flash_actions"])
        with mock.patch.object(pb, "atomic_replace", side_effect=AssertionError("unexpected write")):
            self.prep()
            self.prep(check=True)
        for record in self.records:
            path = (self.unbricker if record["tree"] == "unbricker" else self.jl) / record["path"]
            self.assertEqual(pb.sha256(path.read_bytes()), record["patched"])

    def test_late_drift_prevents_all_mutations(self):
        source = self.unbricker / "fm1_unbrick.py"
        before = source.read_bytes()
        (self.jl / "data/chips.yaml").write_bytes(b"changed after download")
        with self.assertRaisesRegex(pb.PreparationError, "unrecognized bytes"):
            self.prep()
        self.assertEqual(source.read_bytes(), before)

    def test_known_partial_patch_can_repair_but_check_refuses(self):
        path = self.unbricker / "fm1_unbrick.py"
        path.write_bytes(pb.patched_bytes(path.name, path.read_bytes()))
        with self.assertRaisesRegex(pb.PreparationError, "incomplete"):
            self.prep(check=True)
        self.prep()
        self.prep(check=True)

    def test_commit_marker_refused(self):
        (self.jl / pb.MARKER).write_text("wrong")
        with self.assertRaisesRegex(pb.PreparationError, "commit marker"):
            self.prep()

    def test_unknown_import_artifacts_refused(self):
        for relative in ("jltech/__init__.py", "jltech/__init__.pyc", "jltech/uboot.pyd",
                         "jltech/__pycache__/uboot.cpython-312.pyc", "scsiio/extra.py",
                         "yaml.py", "yaml/__init__.py", "crcmod/__init__.py", "extra.so"):
            with self.subTest(relative=relative):
                path = self.jl / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"unreviewed")
                with self.assertRaises(pb.PreparationError):
                    self.prep()
                path.unlink()
                if path.parent.name in ("yaml", "crcmod"):
                    path.parent.rmdir()

    def test_symlink_refused(self):
        path = self.jl / "data/chips.yaml"
        original = path.read_bytes()
        path.unlink()
        other = Path(self.tmp.name) / "other"
        other.write_bytes(original)
        try:
            path.symlink_to(other)
        except OSError:
            self.skipTest("symlinks unavailable")
        with self.assertRaisesRegex(pb.PreparationError, "symlink"):
            self.prep()

    def test_pinned_empty_upstream_tests_package_allowed(self):
        tests = self.unbricker / "tests"
        tests.mkdir()
        (tests / "__init__.py").write_bytes(b"")
        self.prep()
        (tests / "__init__.py").write_bytes(b"print('unreviewed')")
        with self.assertRaises(pb.PreparationError):
            self.prep(check=True)

    def test_line_ending_conversion_refused(self):
        path = self.unbricker / "fm1_unbrick.py"
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        with self.assertRaisesRegex(pb.PreparationError, "unrecognized bytes"):
            self.prep()


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        class SCSIException(Exception):
            pass
        self.error = SCSIException
        namespace = {"SCSIException": SCSIException}
        patched = pb.patched_bytes("jltech/uboot.py", PROTOCOL_SOURCE.encode()).decode()
        exec(compile(patched, "<patched-protocol-fixture>", "exec"), namespace)
        self.proto = namespace["JL_MSCProtocolBase"]

    def test_repeatable_short_read_is_rejected(self):
        class Short:
            def execute(self, cdb, out, incoming):
                incoming[:16] = b"A" * 16
                return len(incoming) - 16
        with self.assertRaisesRegex(self.error, "incomplete data read"):
            self.proto(Short()).cmd_exec_datain(0xFD05, b"", 512)

    def test_every_command_rejects_residual_or_invalid_result(self):
        for residual in (1, -1, None, True, (0, b"")):
            for kind in ("reply", "read", "write"):
                with self.subTest(residual=residual, kind=kind):
                    device = types.SimpleNamespace(execute=lambda *args: residual)
                    proto = self.proto(device)
                    with self.assertRaises(self.error):
                        if kind == "reply":
                            proto.cmd_exec(0xFC0A, b"")
                        elif kind == "read":
                            proto.cmd_exec_datain(0xFD05, b"", 512)
                        else:
                            proto.cmd_exec_dataout(0xFB06, b"", b"RAM loader block")

    def test_complete_transfer_and_response_command(self):
        def execute(cdb, out, incoming):
            if incoming is not None:
                incoming[:] = cdb[:2] + b"X" * (len(incoming) - 2)
            return 0
        proto = self.proto(types.SimpleNamespace(execute=execute))
        self.assertEqual(proto.cmd_exec(0xFC0A, b""), b"X" * 14)
        self.assertEqual(len(proto.cmd_exec_datain(0xFD05, b"", 512)), 512)
        proto.cmd_exec_dataout(0xFB06, b"", b"test")
        with self.assertRaisesRegex(self.error, "command mismatch"):
            self.proto(types.SimpleNamespace(execute=lambda *args: 0)).cmd_exec(0xFC0A, b"")


class WindowsBackendTests(unittest.TestCase):
    def setUp(self):
        self.status = 0
        self.returned_length = None
        self.control_length = None
        self.calls = 0
        self.io_ok = True
        class Fn:
            def __init__(self, function): self.function = function
            def __call__(self, *args): return self.function(*args)
        common = types.ModuleType("scsiio.common")
        common.SCSIException = type("SCSIException", (Exception,), {})
        common.SCSIDevBase = type("SCSIDevBase", (), {})
        self.error = common.SCSIException
        self.handle = 0x1234567887654321
        self.kernel = types.SimpleNamespace(
            CreateFileW=Fn(lambda *a: self.handle), CloseHandle=Fn(lambda *a: True),
            DeviceIoControl=Fn(self.ioctl))
        patched = pb.patched_bytes("scsiio/win32port.py", WINDOWS_SOURCE.encode()).decode()
        namespace = {}
        with mock.patch.dict(sys.modules, {"scsiio.common": common}), \
             mock.patch.object(ctypes, "WinDLL", return_value=self.kernel, create=True), \
             mock.patch.object(ctypes, "WinError", side_effect=lambda code: OSError(code, "mock error"), create=True):
            exec(compile(patched, "<patched-windows-fixture>", "exec"), namespace)
        self.ns = namespace
        self.device = namespace["SCSIDev"]()
        self.device.fhandle = self.handle
        self.device.is_open = True

    def ioctl(self, handle, command, inptr, inlen, outptr, outlen, count, overlap):
        self.calls += 1
        self.assertEqual(handle, self.handle)
        self.assertEqual(command, 0x4D014)
        sptd = ctypes.cast(outptr, ctypes.POINTER(self.ns["SCSI_PASS_THROUGH_DIRECT"])).contents
        requested = sptd.data_len
        ctypes.cast(count, ctypes.POINTER(self.ns["DWORD"])).contents.value = (
            ctypes.sizeof(sptd) if self.control_length is None else self.control_length)
        sptd.status = self.status
        if self.returned_length is not None:
            sptd.data_len = self.returned_length
        if sptd.dir == 1 and requested:
            ctypes.memset(sptd.data_ptr, 0x5A, min(requested, sptd.data_len))
        return self.io_ok

    def test_complete_read_and_pointer_prototypes(self):
        incoming = bytearray(512)
        self.assertEqual(self.device.execute(b"\xFD\x05", None, incoming), 0)
        self.assertEqual(incoming, b"Z" * 512)
        signature = self.kernel.DeviceIoControl.argtypes
        self.assertIs(signature[0], ctypes.c_void_p)  # HANDLE, preserves 64 bits
        self.assertIs(signature[2], ctypes.c_void_p)
        self.assertIs(signature[4], ctypes.c_void_p)
        self.assertIs(signature[7], ctypes.c_void_p)
        self.assertIs(self.ns["LPSECURITY_ATTRIBUTES"], ctypes.c_void_p)
        self.assertEqual(self.ns["GENERIC_READ"] | self.ns["GENERIC_WRITE"], 0xC0000000)

    def test_status_short_and_oversized_transfer_rejected_before_copy(self):
        for status, count in ((2, None), (0, 16), (0, 513)):
            with self.subTest(status=status, count=count):
                self.status, self.returned_length = status, count
                incoming = bytearray(b"A" * 512)
                with self.assertRaises(self.error):
                    self.device.execute(b"\xFD\x05", None, incoming)
                self.assertEqual(incoming, b"A" * 512)

    def test_missing_or_truncated_control_header_rejected(self):
        for count in (0, 1, ctypes.sizeof(self.ns["SCSI_PASS_THROUGH_DIRECT"]) - 1,
                      ctypes.sizeof(self.ns["SCSI_PASS_THROUGH_DIRECT"]) + 1):
            with self.subTest(count=count):
                self.control_length = count
                incoming = bytearray(b"A" * 512)
                with self.assertRaisesRegex(self.error, "control result"):
                    self.device.execute(b"\xFD\x05", None, incoming)
                self.assertEqual(incoming, b"A" * 512)

    def test_invalid_cdb_and_bidirectional_fail_before_io(self):
        for cdb in (b"", b"X" * 17):
            with self.assertRaises(self.error):
                self.device.execute(cdb, None, bytearray(10))
        with self.assertRaises(NotImplementedError):
            self.device.execute(b"\xFD\x05", b"out", bytearray(10))
        self.assertEqual(self.calls, 0)

    def test_os_failure_preserves_error_code(self):
        self.io_ok = False
        with mock.patch.object(ctypes, "get_last_error", return_value=5, create=True), \
             mock.patch.object(ctypes, "WinError", side_effect=lambda code: OSError(code, "denied"), create=True):
            with self.assertRaisesRegex(self.error, "denied"):
                self.device.execute(b"\xFD\x05", None, bytearray(10))


class ExactSourceTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("FM1_JLUB_SOURCE_REVIEW"), "optional pinned original source tree not supplied")
    def test_actual_source_transforms_match_declared_hashes(self):
        root = Path(os.environ["FM1_JLUB_SOURCE_REVIEW"])
        for record in pb.FILES:
            if record["tree"] != "jlub" or record["original"] == record["patched"]:
                continue
            data = (root / record["path"]).read_bytes()
            self.assertEqual(pb.sha256(data), record["original"])
            self.assertEqual(pb.sha256(pb.patched_bytes(record["path"], data)), record["patched"])
            ast.parse(pb.patched_bytes(record["path"], data))


if __name__ == "__main__":
    unittest.main()
