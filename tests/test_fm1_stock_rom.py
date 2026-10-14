#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline ROM-helper checks. No MIDI library or physical device is needed."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / "tools" / "fm1_stock_rom.py"
SPEC = importlib.util.spec_from_file_location("stock_rom", SOURCE)
R = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(R)


def identity(text=b"FM-1_015", *, body=None, command=0x11, declared=27, checksum=None):
    body = text.ljust(27, b"\0") if body is None else body
    raw = bytes((0, 0x59, command)) + declared.to_bytes(3, "little") + body
    raw += bytes((~sum(body) & 255 if checksum is None else checksum,))
    bits = int.from_bytes(raw, "little")
    return bytes((240, *((bits >> (7 * i)) & 127 for i in range((len(raw) * 8 + 6) // 7)), 247))


VALID = identity()


class Clock:
    def __init__(self):
        self.now = 0

    def time(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Port:
    def __init__(self, module, direction):
        self.module, self.direction = module, direction
        self.opened = False
        self.opens = self.closes = self.reads = 0

    def ignore_types(self, **options):
        self.module.ignore = options

    def get_ports(self):
        return self.module.names[self.direction][:]

    def open_port(self, index):
        self.opens += 1
        if self.module.open_error == self.direction:
            raise RuntimeError("Mock open failed")
        self.opened = True
        self.module.selected[self.direction] = index

    def is_port_open(self):
        return self.opened

    def close_port(self):
        self.closes += 1
        self.opened = False

    def get_message(self):
        self.reads += 1
        if self.module.receive_interrupt:
            raise KeyboardInterrupt
        if self.module.flood:
            return list(VALID), 0.0
        return (list(self.module.queue.pop(0)), 0.0) if self.module.queue else None

    def send_message(self, packet):
        wire = bytes(packet)
        self.module.sent.append(wire)
        if wire == R.IDENTITY_QUERY:
            self.module.queries += 1
            if self.module.responses:
                self.module.queue.extend(self.module.responses.pop(0))
        self.module.on_send(wire)


class Midi:
    def __init__(self, responses=None):
        self.names = {"input": ["FM-1 input"], "output": ["FM-1 output"]}
        self.selected, self.sent, self.queue, self.ports = {}, [], [], []
        self.responses = [[VALID], [VALID]] if responses is None else responses
        self.flood = self.receive_interrupt = False
        self.queries, self.open_error = 0, None
        self.on_send = lambda wire: None

    def MidiIn(self, **options):
        self.queue_limit = options["queue_size_limit"]
        result = Port(self, "input"); self.ports.append(result)
        return result

    def MidiOut(self):
        result = Port(self, "output"); self.ports.append(result)
        return result


class HelperTests(unittest.TestCase):
    def session(self, midi=None):
        midi = midi or Midi()
        clock = Clock()
        return midi, clock, R.MidiSession(midi, clock=clock.time, sleep=clock.sleep)

    def run_action(self, session, action="enter-rom", confirm=lambda prompt: "ROM"):
        output = []
        R.run(session, action, "FM-1 input", "FM-1 output", confirm=confirm, emit=output.append)
        return output

    def closed(self, midi):
        self.assertTrue(all(port.closes == 1 and not port.opened for port in midi.ports))

    def no_rom(self, midi):
        self.assertNotIn(R.ROM_SOFTKEY, midi.sent)
        self.assertTrue(all(packet == R.IDENTITY_QUERY for packet in midi.sent))

    def test_identity_requires_exact_canonical_stock_response(self):
        self.assertTrue(R.is_stock_v15_identity(VALID))
        self.assertTrue(R.is_stock_v15_identity(list(VALID)))
        cases = [b"", VALID[1:], VALID[:-1], b"\0" + VALID, VALID + b"\0", VALID + VALID,
                 identity(command=0x12), identity(declared=0), identity(declared=28), identity(checksum=0),
                 identity(body=b"FM-1_015\0BAD".ljust(27, b"\0"))]
        for text in (b"FM-1_016", b"FM-1_15", b"FM-1_0015", b"FM-1_700", b"FM-1_900", b"ota-FM-1_015", b"OTHER_015"):
            cases.append(identity(text))
        for i in range(len(VALID)):
            changed = bytearray(VALID); changed[i] ^= 1; cases.append(changed)
        for i in range(1, len(VALID) - 1):
            changed = bytearray(VALID); changed[i] |= 128; cases.append(changed)
        tail = bytearray(VALID); tail[-2] |= 64; cases.append(tail)
        for packet in cases:
            with self.subTest(packet=packet):
                self.assertFalse(R.is_stock_v15_identity(packet))

    def test_list_never_opens_or_sends(self):
        midi, _, session = self.session()
        output = self.run_action(session, "list")
        self.assertIn("  'FM-1 input'", output)
        self.assertIn("  'FM-1 output'", output)
        self.assertEqual(midi.sent, [])
        self.assertTrue(all(port.opens == 0 for port in midi.ports))
        self.closed(midi)

    def test_check_only_sends_one_identity_query(self):
        midi, _, session = self.session()
        self.run_action(session, "check", lambda prompt: self.fail("check must not prompt"))
        self.assertEqual(midi.sent, [R.IDENTITY_QUERY])
        self.assertEqual(midi.ignore, {"sysex": False, "timing": True, "active_sense": True})
        self.assertEqual(midi.selected, {"input": 0, "output": 0})
        self.closed(midi)

    def test_success_rechecks_after_exact_confirmation_and_sends_only_one_softkey(self):
        midi, clock, session = self.session()
        def confirm(prompt):
            self.assertIn("Type ROM", prompt)
            self.assertEqual(midi.sent, [R.IDENTITY_QUERY])
            clock.now += 120  # A long confirmation pause cannot reuse the old check.
            return "ROM"
        output = self.run_action(session, confirm=confirm)
        self.assertEqual(midi.sent, [R.IDENTITY_QUERY, R.IDENTITY_QUERY, R.ROM_SOFTKEY])
        self.assertEqual(R.ROM_SOFTKEY, bytes.fromhex("f0 22 24 35 7d f7"))
        self.assertNotIn(bytes.fromhex("f0 22 24 35 7f f7"), midi.sent)
        self.assertTrue(any("does not prove" in line for line in output))
        self.assertTrue(any("fully off and unplug USB" in line for line in output))
        self.closed(midi)

    def test_missing_partial_or_duplicate_port_names_refuse_without_sends(self):
        for direction in ("input", "output"):
            for names in ([], ["FM-1"], [f"FM-1 {direction}"] * 2):
                midi, _, session = self.session()
                midi.names[direction] = names
                with self.subTest(direction=direction, names=names), self.assertRaises(R.ROMError):
                    self.run_action(session)
                self.assertEqual(midi.sent, [])
                self.assertTrue(all(port.opens == 0 for port in midi.ports))
                self.closed(midi)

    def test_partial_open_failure_closes_both_ports(self):
        midi, _, session = self.session()
        midi.open_error = "output"
        with self.assertRaisesRegex(RuntimeError, "open failed"):
            self.run_action(session)
        self.assertEqual(midi.sent, [])
        self.closed(midi)

    def test_wrong_identity_and_malformed_responses_never_prompt_or_enter(self):
        for packet in (identity(b"FM-1_016"), identity(b"FM-1_700"), identity(b"ota-FM-1_015"), VALID[:-1], bytes((0x90, 60, 127))):
            midi, _, session = self.session(Midi([[packet], [VALID]]))
            with self.subTest(packet=packet), self.assertRaises(R.ROMError):
                self.run_action(session, confirm=lambda prompt: self.fail("wrong identity must not prompt"))
            self.no_rom(midi); self.closed(midi)

    def test_confirmation_rejection_eof_and_interrupt_never_send_softkey(self):
        for answer in ("", "rom", " ROM", "ROM ", "yes", EOFError(), KeyboardInterrupt()):
            midi, _, session = self.session()
            def confirm(prompt):
                if isinstance(answer, BaseException):
                    raise answer
                return answer
            with self.subTest(answer=answer), self.assertRaises((R.ROMError, EOFError, KeyboardInterrupt)):
                self.run_action(session, confirm=confirm)
            self.assertEqual(midi.sent, [R.IDENTITY_QUERY])
            self.closed(midi)

    def test_stale_preexisting_response_cannot_satisfy_a_new_check(self):
        midi, clock, session = self.session(Midi([[]]))
        midi.queue.append(VALID)
        with self.assertRaisesRegex(R.ROMError, "Timed out"):
            self.run_action(session)
        self.assertLess(clock.now, 3)
        self.assertEqual(midi.sent, [R.IDENTITY_QUERY])
        self.closed(midi)

    def test_stale_confirmation_queue_cannot_replace_fresh_handshake(self):
        midi, _, session = self.session(Midi([[VALID], []]))
        def confirm(prompt):
            midi.queue.append(VALID)
            return "ROM"
        with self.assertRaisesRegex(R.ROMError, "Timed out"):
            self.run_action(session, confirm=confirm)
        self.assertEqual(midi.sent, [R.IDENTITY_QUERY, R.IDENTITY_QUERY])
        self.closed(midi)

    def test_extra_replies_or_changed_fresh_identity_refuse(self):
        for second in ([VALID, VALID], [identity(b"FM-1_016")], [VALID, identity(b"FM-1_700")]):
            midi, _, session = self.session(Midi([[VALID], second]))
            with self.subTest(second=second), self.assertRaises(R.ROMError):
                self.run_action(session)
            self.no_rom(midi); self.closed(midi)

    def test_flood_draining_and_empty_read_are_bounded(self):
        midi, _, session = self.session()
        midi.flood = True
        with self.assertRaisesRegex(R.ROMError, "did not become quiet"):
            self.run_action(session)
        self.assertEqual(midi.ports[0].reads, R.MAX_DRAIN)
        self.assertEqual(midi.sent, [])
        self.closed(midi)
        midi, clock, session = self.session(Midi([[]]))
        with self.assertRaisesRegex(R.ROMError, "Timed out"):
            self.run_action(session)
        self.assertLessEqual(midi.ports[0].reads, 258)
        self.assertLess(clock.now, 3)
        self.no_rom(midi); self.closed(midi)

    def test_changed_catalog_or_disconnected_port_refuse_after_confirmation(self):
        for direction in ("input", "output"):
            for change_names in (True, False):
                midi, _, session = self.session()
                def confirm(prompt):
                    if change_names:
                        midi.names[direction].append("another port")
                    else:
                        midi.ports[0 if direction == "input" else 1].opened = False
                    return "ROM"
                with self.subTest(direction=direction, change_names=change_names), self.assertRaises(R.ROMError):
                    self.run_action(session, confirm=confirm)
                self.assertEqual(midi.sent, [R.IDENTITY_QUERY])
                self.closed(midi)

    def test_disconnect_or_interrupt_during_fresh_handshake_refuse(self):
        for interrupt in (True, False):
            midi, _, session = self.session()
            def sent(wire):
                if wire == R.IDENTITY_QUERY and midi.queries == 2:
                    if interrupt:
                        raise KeyboardInterrupt
                    midi.names["input"] = []
            midi.on_send = sent
            with self.subTest(interrupt=interrupt), self.assertRaises((R.ROMError, KeyboardInterrupt)):
                self.run_action(session)
            self.no_rom(midi); self.closed(midi)

    def test_entry_requires_recent_check_and_never_retries_failed_send(self):
        midi, clock, session = self.session()
        session.open_selected("FM-1 input", "FM-1 output")
        with self.assertRaises(R.ROMError):
            session.enter_rom_once()
        session.check_stock()
        clock.now += 2
        with self.assertRaises(R.ROMError):
            session.enter_rom_once()
        self.no_rom(midi)
        session.check_stock()
        def fail_send(wire):
            if wire == R.ROM_SOFTKEY:
                raise RuntimeError("Mock send failed")
        midi.on_send = fail_send
        with self.assertRaisesRegex(RuntimeError, "send failed"):
            session.enter_rom_once()
        with self.assertRaises(R.ROMError):
            session.enter_rom_once()
        self.assertEqual(midi.sent.count(R.ROM_SOFTKEY), 1)
        session.close(); self.closed(midi)

    def test_late_traffic_before_softkey_refuses(self):
        midi, _, session = self.session()
        session.open_selected("FM-1 input", "FM-1 output")
        session.check_stock()
        midi.queue.append(VALID)
        with self.assertRaisesRegex(R.ROMError, "Unexpected MIDI traffic"):
            session.enter_rom_once()
        self.no_rom(midi)
        session.close(); self.closed(midi)

    def test_backend_send_failures_close_ports_and_do_not_retry(self):
        for fail_at in (1, 2, 3):
            midi, _, session = self.session()
            def fail_send(wire):
                if len(midi.sent) == fail_at:
                    raise RuntimeError("Mock send failed")
            midi.on_send = fail_send
            with self.subTest(fail_at=fail_at), self.assertRaisesRegex(RuntimeError, "send failed"):
                self.run_action(session)
            self.assertEqual(len(midi.sent), fail_at)
            self.assertLessEqual(midi.sent.count(R.ROM_SOFTKEY), 1)
            self.closed(midi)

    def test_output_constructor_failure_closes_created_input(self):
        midi = Midi()
        def fail_output():
            raise RuntimeError("Mock output creation failed")
        midi.MidiOut = fail_output
        with self.assertRaisesRegex(RuntimeError, "creation failed"):
            R.MidiSession(midi)
        self.assertEqual(len(midi.ports), 1)
        self.closed(midi)

    def test_cli_has_no_yes_raw_packet_package_update_or_flash_options(self):
        commands = [[], ["update"], ["flash"], ["restore"], ["loader"], ["enter-rom"],
                    ["check", "--input", "a"], ["list", "--packet", "f0"],
                    ["enter-rom", "--input", "a", "--output", "b", "--yes"],
                    ["enter-rom", "--input", "a", "--output", "b", "FM-1.fwsc"]]
        for args in commands:
            with self.subTest(args=args), patch.object(R, "MidiSession") as constructor, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    R.main(args)
                constructor.assert_not_called()

    def test_help_and_import_need_no_midi_dependency(self):
        with patch.dict("sys.modules", {"rtmidi": None}), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as result:
                R.main(["--help"])
            self.assertEqual(result.exception.code, 0)
            with self.assertRaisesRegex(R.ROMError, "python-rtmidi"):
                R.MidiSession()


if __name__ == "__main__":
    unittest.main(verbosity=2)
