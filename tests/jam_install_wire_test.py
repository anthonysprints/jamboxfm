#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline framing and refusal regressions; no MIDI backend is imported/opened.

Fixtures use a whole-integer reference packer independent of the stream codec.
The 481-byte regression follows the audited FM-1-RE packet contract, without
adopting its experimental FM-1_016 checksum exception or stock-loader policy.
"""
import io
import argparse
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I


def packed(raw):
    bits = int.from_bytes(raw, "little")
    return bytes((bits >> (7 * i)) & 127 for i in range((len(raw) * 8 + 6) // 7))


def unpacked(wire):
    bits = sum(b << (7 * i) for i, b in enumerate(wire))
    return bits.to_bytes((len(wire) * 7 + 7) // 8, "little")[:len(wire) * 7 // 8]


def envelope(raw):
    return b"\xf0" + packed(raw) + b"\xf7"


def message(cmd, body, declared=None, checksum=None):
    raw = bytes((0, 0x59, cmd)) + (len(body) if declared is None else declared).to_bytes(3, "little") + body
    return envelope(raw + bytes((~sum(body) & 255 if checksum is None else checksum,)))


def request(addr=0x12345678, n=512, fl=0):
    return message(0x30, bytes((fl,)) + addr.to_bytes(4, "little") + n.to_bytes(3, "little"))


def identity(text=b"FM-1_900", **kwargs):
    return message(0x11, text.ljust(27, b"\0"), **kwargs)


def raw_package(product):
    """Only identity markers are needed for candidate rejection before CRC/MIDI."""
    raw = bytearray(1024)
    for i in range(20):
        raw[i * 48 + 47] = (product[i] + i + 1) & 255 if i < len(product) else 0x7D
    return bytes(raw)


class Link:
    def __init__(self, packets=(), *, send_ok=True, lost=False, interrupt=False):
        self.packets = list(packets)
        self.sent = []
        self.send_ok, self.lost, self.interrupt = send_ok, lost, interrupt
        self.closed = False

    def send(self, packet):
        self.sent.append(packet)
        return self.send_ok

    def read(self, timeout):
        if self.interrupt:
            raise KeyboardInterrupt
        return self.packets.pop(0) if self.packets else None

    def close(self):
        self.closed = True

    def drain(self):
        pass


class WireTests(unittest.TestCase):
    def test_complete_codec_roundtrip_and_independent_wire(self):
        for n in range(770):
            raw = bytes((i * 37 + n) & 255 for i in range(n))
            with self.subTest(n=n):
                self.assertEqual(I.pack7(raw), packed(raw))
                self.assertEqual(I.unpack7(packed(raw)), raw)

    def test_bad_packed_data_and_noncanonical_tails(self):
        for value in (-1, 128, 255, 256):
            with self.subTest(value=value), self.assertRaises(ValueError):
                I.unpack7([value])
        for raw in (b"", b"1234567", b"0123456789abcd"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                I.unpack7(packed(raw) + b"\0")
        for raw in (b"x", b"12", b"123", b"1234", b"12345", b"123456"):
            p = bytearray(packed(raw)); p[-1] |= 64
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                I.unpack7(p)
        for value in (-1, 256):
            with self.assertRaises(ValueError):
                I.pack7([value])

    def test_request_fields_and_exact_header(self):
        for fl in (0, 3, 255):
            for n in (0, 1, 481, 512, 513, 0xFFFFFF):
                # Transfer limits belong to serve(), which reports badreq
                # rather than treating a well-framed impossible read as noise.
                self.assertEqual(I.parse_request(request(0xFFFFFFFF, n, fl)), (fl, 0xFFFFFFFF, n))
        raw = unpacked(request()[1:-1])
        for offset in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14):
            bad = bytearray(raw); bad[offset] ^= 1
            with self.subTest(offset=offset):
                self.assertIsNone(I.parse_request(envelope(bad)))
        self.assertIsNone(I.parse_request(message(0x30, raw[6:-1], declared=0)))
        self.assertIsNone(I.parse_request(message(0x30, raw[6:-1], declared=9)))

    def test_identity_fields_padding_and_checksum(self):
        for text, model, version in ((b"FM-1_015", "FM-1", 15), (b"ota-FM-1_900", "ota-FM-1", 900),
                                     (b"FM-1_700", "FM-1", 700), (b"XY-9_001", "XY-9", 1),
                                     (b"ABCDEFGHIJKLMNOPQRSTUV_1234", "ABCDEFGHIJKLMNOPQRSTUV", 1234)):
            with self.subTest(text=text):
                got = I.parse_identity(identity(text))
                self.assertIsNotNone(got)
                self.assertEqual((got.text, got.model, got.version), (text.decode(), model, version))
        for text in (b"", b"FM-1", b"FM-1_", b"_900", b"FM-1_-1", b"FM-1_+1", b"FM_1_900",
                     b"FM-1_900\0X", b" FM-1_900", b"FM-1_900 ", b"FM-1\n_900", b"FM-\xff_900",
                     b"FM-1_\xd9\xa1", b"FM-1_9\0" + b"00"):
            with self.subTest(text=text):
                self.assertIsNone(I.parse_identity(identity(text)))
        raw = unpacked(identity()[1:-1])
        for offset in range(34):
            bad = bytearray(raw); bad[offset] ^= 1
            with self.subTest(offset=offset):
                self.assertIsNone(I.parse_identity(envelope(bad)))
        self.assertIsNone(I.parse_identity(identity(declared=0)))
        # FM-1-RE's experimental patch retained the V15 checksum (0x19).
        self.assertIsNone(I.parse_identity(identity(b"FM-1_016", checksum=0x19)))
        self.assertEqual(I.parse_identity(identity(b"FM-1_016")).version, 16)

    def test_envelope_bounds_and_canonical_tail(self):
        for packet, parser in ((request(), I.parse_request), (identity(), I.parse_identity)):
            cases = [b"", packet[1:], packet[:-1], b"\0" + packet, packet + b"\0", packet + packet,
                     packet[:4] + b"\0" + packet[4:], packet[:4] + packet[5:],
                     b"\xf0" + bytes(100000) + b"\xf7"]
            for offset in range(1, len(packet)-1):
                bad = bytearray(packet); bad[offset] |= 128; cases.append(bytes(bad))
            tail = bytearray(packet); tail[-2] |= 64; cases.append(bytes(tail))
            for bad in cases:
                with self.subTest(parser=parser.__name__, bad=bad[:12], n=len(bad)):
                    self.assertIsNone(parser(bad))

    def test_response_all_lengths_and_partial_final_481(self):
        for n in range(513):
            data = bytes((i * 37 + n) & 255 for i in range(n))
            for addr, fl in ((0xAAB20, 0), (0xFFFFFFFF, 255)):
                with self.subTest(n=n, addr=addr, fl=fl):
                    packet = I.response(addr, data, fl)
                    raw = unpacked(packet[1:-1])
                    self.assertEqual((packet[0], packet[-1]), (240, 247))
                    self.assertTrue(all(b < 128 for b in packet[1:-1]))
                    self.assertEqual(len(raw), n + 15)
                    self.assertEqual(raw[:3], b"\0Y0")
                    self.assertEqual(int.from_bytes(raw[3:6], "little"), n + 8)
                    self.assertEqual(raw[6], fl)
                    self.assertEqual(int.from_bytes(raw[7:11], "little"), addr)
                    self.assertEqual(int.from_bytes(raw[11:14], "little"), n)
                    self.assertEqual(raw[14:-1], data)
                    self.assertEqual(raw[-1], ~sum(raw[6:-1]) & 255)
        raw = unpacked(I.response(123, bytes(481))[1:-1])
        self.assertEqual(int.from_bytes(raw[3:6], "little"), 489)
        self.assertEqual(I.response(0x1234, b"\0\x7f\x80\xff").hex(),
                         "f000324161000000003424000040000000007e017c7f16f7")

    def test_response_rejects_oversized_or_unrepresentable_fields(self):
        for addr, data, fl in ((0, bytes(513), 0), (-1, b"x", 0), (1 << 32, b"x", 0),
                               (0, b"x", -1), (0, b"x", 256)):
            with self.subTest(addr=addr, n=len(data), fl=fl), self.assertRaises(ValueError):
                I.response(addr, data, fl)


class ServingTests(unittest.TestCase):
    def setUp(self):
        self.up = I.Updater(None)
        self.delay = patch.dict(I.DELAY, {key: 0 for key in ("start", "reply", "loader", "reboot")})
        self.delay.start()

    def tearDown(self):
        self.delay.stop()

    def serve(self, link, image=bytes(2048), finish=I.FINISH_WRITE):
        return self.up.serve(link, image, finish, 1, lambda *args: None)

    def test_exact_final_block_and_maximum_payload(self):
        image = bytes(i & 255 for i in range(993))
        link = Link([request(0,512), request(512,481), request(I.FINISH_WRITE,8)])
        self.assertEqual(self.serve(link,image), (2, True, False))
        for packet, expected in zip(link.sent, (image[:512], image[512:], b"success\0")):
            self.assertEqual(unpacked(packet[1:-1])[14:-1], expected)

    def test_invalid_lengths_ranges_and_terminal_requests_never_answered(self):
        for addr, n in ((0,0), (0,513), (0,0xFFFFFF), (2048,1), (2047,2), (0xFFFFFFFF,1),
                        (I.FINISH_WRITE,0), (I.FINISH_WRITE,7), (I.FINISH_WRITE,512), (I.FINISH_CHECK,8)):
            link = Link([request(addr,n)])
            with self.subTest(addr=addr,n=n), self.assertRaises(I.InstallError) as e:
                self.serve(link)
            self.assertEqual(e.exception.code,"badreq")
            self.assertEqual(link.sent,[])

    def test_failed_completion_reply_and_disconnect_cannot_claim_success(self):
        link=Link([request(I.FINISH_WRITE,8)],send_ok=False)
        self.assertEqual(self.serve(link),(0,False,True))
        link=Link([request(I.FINISH_WRITE,8)],lost=True)
        self.assertEqual(self.serve(link),(0,False,True)); self.assertEqual(link.sent,[])

    def test_malformed_packet_flood_cannot_extend_idle_deadline(self):
        link=Link([b"junk"]*100)
        with patch.object(I.time,"monotonic",side_effect=[i/10 for i in range(200)]):
            self.assertEqual(self.serve(link),(0,False,False))
        self.assertGreater(len(link.packets),80)
        self.assertEqual(link.sent,[])

    def test_ctrl_c_and_bad_requests_close_active_pair(self):
        for method, text in ((self.up.check,"FM-1_900"),(self.up.write,"ota-FM-1_900")):
            for interrupt in (True,False):
                link=Link([request(0,0)],interrupt=interrupt)
                dev=I.Device(link,I.parse_identity(identity(text.encode())),"simulated")
                with self.subTest(text=text,interrupt=interrupt), self.assertRaises(KeyboardInterrupt if interrupt else I.InstallError):
                    method(dev,bytes(2048),lambda *args: None)
                self.assertTrue(link.closed)
                self.assertEqual(link.sent,[I.UPGRADE])

    def test_failed_upgrade_send_closes_without_serving(self):
        for method in (self.up.check,self.up.write):
            link=Link([request(0,64)],send_ok=False)
            dev=I.Device(link,I.parse_identity(identity()),"simulated")
            with self.assertRaises(I.InstallError) as e:
                method(dev,bytes(2048),lambda *args: None)
            self.assertEqual(e.exception.code,"lost"); self.assertTrue(link.closed)
            self.assertEqual(len(link.sent),1); self.assertEqual(len(link.packets),1)

    def test_newly_discovered_foreign_loader_never_receives_stage_two(self):
        for text in ("ota-FM-1_015","ota-FM-1_700","ota-XY-9_900","FM-1_900"):
            source=I.Device(Link(),I.parse_identity(identity()),"app")
            loader=I.Device(Link(),I.parse_identity(identity(text.encode())),"loader")
            with patch.object(self.up,"check",return_value=loader), self.assertRaises(I.InstallError) as e:
                self.up.install(source,b"image","FM-1_900")
            self.assertEqual(e.exception.code,"foreign"); self.assertTrue(loader.link.closed)
            self.assertEqual(loader.link.sent,[])

    def test_candidate_package_rejected_before_midi_even_with_force(self):
        class NoMidi:
            def input_names(self):
                self.fail("candidate rejection must precede MIDI enumeration")
        with tempfile.TemporaryDirectory() as tmp:
            for product in (b"FM-1_700",b"fm-1_0700",b"ota-FM-1_700"):
                path=Path(tmp)/"candidate.fwsc"; path.write_bytes(raw_package(product))
                for force in (False,True):
                    with self.subTest(product=product,force=force), self.assertRaises(I.InstallError) as e:
                        I.load_package(path,force)
                    self.assertEqual(e.exception.code,"badpkg")
                    self.assertIn("transfers are disabled",str(e.exception))
                with patch.object(I,"MidoBackend",side_effect=AssertionError("no MIDI")):
                    # Injected backend permits an assertion against enumeration,
                    # not just against opening a hardware connection.
                    with patch.object(sys,"stderr",io.StringIO()):
                        rc=I.main([str(path),"--force","--yes"],backend=NoMidi(),out=io.StringIO())
                    self.assertEqual(rc,2)
                    with patch.object(sys,"stderr",io.StringIO()):
                        rc=I.main([str(path),"--force","--yes"],out=io.StringIO())
                    self.assertEqual(rc,2, "package rejection must precede backend construction")
        link=Link(); dev=I.Device(link,I.parse_identity(identity()),"app")
        with self.assertRaises(I.InstallError): self.up.install(dev,b"image","FM-1_700")
        self.assertTrue(link.closed); self.assertEqual(link.sent,[])

    def test_confirmation_interrupt_closes_pair_without_upgrade(self):
        dev=I.Device(Link(),I.parse_identity(identity()),"FM-1")
        args=argparse.Namespace(info=False,port=None,package="unused",force=False,yes=False)
        with patch.object(I.Updater,"find",return_value=dev):
            with self.assertRaises(KeyboardInterrupt):
                I.run(args,object(),io.StringIO(),lambda _: (_ for _ in ()).throw(KeyboardInterrupt()),
                      package=("FM-1_900",b"checked bytes"))
        self.assertTrue(dev.link.closed)
        self.assertEqual(dev.link.sent,[])

    def test_input_cannot_pair_with_unrelated_single_output(self):
        self.assertIsNone(I.pair_output("FM-1",["Arturia KeyStep"]))
        self.assertIsNone(I.pair_output("FM-1 2",["FM-1 1","FM-1 3"]))
        self.assertEqual(I.pair_output("FM-1 2",["FM-1 3"]),"FM-1 3")
        self.assertEqual(I.pair_output("JamBoxFM",["JamBoxFM"]),"JamBoxFM")


if __name__ == "__main__":
    unittest.main(verbosity=2)
