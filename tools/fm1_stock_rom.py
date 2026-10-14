#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check reported stock V15 identity or request its temporary ROM USB mode.

Windows: py -m pip install python-rtmidi
         py tools/fm1_stock_rom.py list
         py tools/fm1_stock_rom.py check --input "EXACT NAME" --output "EXACT NAME"
         py tools/fm1_stock_rom.py enter-rom --input "EXACT NAME" --output "EXACT NAME"

No firmware files, updates, flash writes, loaders or reboot commands are handled.
The stock ROM softkey is documented in Transporter a632d923, DEVLOG lines 128-149,
on another physical unit. This helper's Windows/device behavior is untested.
MIDI identity has no nonce or authentication: a valid reply reports an identity,
not proof of the running firmware or physical USB device. Queued replies are
discarded before each request, and entry requires another request after consent.
"""
import argparse
import sys
import time

IDENTITY_QUERY = bytes.fromhex("f0 00 32 45 00 00 00 40 7f f7")
ROM_SOFTKEY = bytes.fromhex("f0 22 24 35 7d f7")
IDENTITY_TIMEOUT = 2.0
MAX_DRAIN = 128


class ROMError(Exception):
    pass


def is_stock_v15_identity(packet):
    """Only the exact canonical 27-byte, zero-padded V15 identity body passes."""
    if len(packet) != 41 or packet[0] != 0xF0 or packet[-1] != 0xF7:
        return False
    decoded, acc, bits = bytearray(), 0, 0
    for value in packet[1:-1]:
        if type(value) is not int or not 0 <= value <= 127:
            return False
        acc |= value << bits
        bits += 7
        while bits >= 8:
            decoded.append(acc & 255)
            acc >>= 8
            bits -= 8
    if acc or bits == 7 or len(decoded) != 34:
        return False
    body = decoded[6:-1]
    return (decoded[:6] == b"\x00\x59\x11\x1b\x00\x00" and
            body == b"FM-1_015".ljust(27, b"\0") and decoded[-1] == (~sum(body) & 255))


class MidiSession:
    """Polling RtMidi input/output; its only sends are the two constants above."""
    def __init__(self, module=None, *, clock=time.monotonic, sleep=time.sleep):
        if module is None:
            try:
                import rtmidi as module  # Default native backend is WinMM on Windows.
            except ImportError as exc:
                raise ROMError("Install the MIDI dependency: py -m pip install python-rtmidi") from exc
        self.clock, self.sleep = clock, sleep
        self.input = self.output = None
        self.snapshot = None
        self.verified_at = None
        self.rom_attempted = False
        try:
            self.input = module.MidiIn(queue_size_limit=MAX_DRAIN)
            self.output = module.MidiOut()
            self.input.ignore_types(sysex=False, timing=True, active_sense=True)
        except BaseException:
            self.close()
            raise

    def names(self):
        return tuple(self.input.get_ports()), tuple(self.output.get_ports())

    def open_selected(self, input_name, output_name):
        self.snapshot = self.names()
        for names, selected, direction in zip(self.snapshot, (input_name, output_name), ("input", "output")):
            if not selected or names.count(selected) != 1:
                raise ROMError(f"Select one exact, unique MIDI {direction} name from 'list'; got {selected!r}.")
        self.input.open_port(self.snapshot[0].index(input_name))
        self.output.open_port(self.snapshot[1].index(output_name))
        self.sleep(0.15)  # Allow the native MIDI ports to settle before the query.
        self.connected()

    def connected(self):
        if (self.snapshot is None or self.names() != self.snapshot or
                not self.input.is_port_open() or not self.output.is_port_open()):
            raise ROMError("MIDI ports changed or disconnected. List and select the ports again.")

    def receive(self):
        message = self.input.get_message()
        return None if message is None else message[0]

    def drain(self):
        end = self.clock() + 0.2
        for _ in range(MAX_DRAIN):
            self.connected()
            if self.clock() >= end:
                break
            if self.receive() is None:
                return
        raise ROMError("MIDI input did not become quiet; close other MIDI apps and retry.")

    def check_stock(self):
        self.verified_at = None
        self.drain()
        self.connected()
        self.output.send_message(list(IDENTITY_QUERY))
        end = self.clock() + IDENTITY_TIMEOUT
        for _ in range(256):
            self.connected()
            if self.clock() >= end:
                break
            packet = self.receive()
            if packet is not None:
                if not is_stock_v15_identity(packet):
                    raise ROMError("Reply was not the exact valid FM-1_015 identity. ROM entry refused.")
                if self.receive() is not None:
                    raise ROMError("Extra MIDI replies were queued. ROM entry refused.")
                self.connected()
                self.verified_at = self.clock()
                return
            self.sleep(0.01)
        raise ROMError("Timed out waiting for a fresh FM-1_015 identity. ROM entry refused.")

    def enter_rom_once(self):
        if self.rom_attempted or self.verified_at is None or self.clock() - self.verified_at > 1.0:
            raise ROMError("A fresh stock identity check is required; the ROM command is never retried.")
        self.connected()
        if self.receive() is not None:
            raise ROMError("Unexpected MIDI traffic arrived before ROM entry. Entry refused.")
        self.rom_attempted = True
        self.output.send_message(list(ROM_SOFTKEY))

    def close(self):
        for port in (self.input, self.output):
            if port is not None:
                try:
                    port.close_port()
                except Exception:
                    pass


def run(session, action, input_name=None, output_name=None, *, confirm=input, emit=print):
    try:
        if action == "list":
            inputs, outputs = session.names()
            for label, names in (("Inputs", inputs), ("Outputs", outputs)):
                emit(label + ":")
                for name in names:
                    emit("  " + repr(name))
                if not names:
                    emit("  (none)")
            return
        if action not in ("check", "enter-rom"):
            raise ROMError("Only list, check and enter-rom are supported.")
        session.open_selected(input_name, output_name)
        session.check_stock()
        emit("The selected MIDI pair reported the exact FM-1_015 identity.")
        if action == "check":
            return
        emit("ROM mode temporarily stops the instrument. This helper does not write firmware.")
        if confirm("Type ROM to request ROM mode, or anything else to cancel: ") != "ROM":
            raise ROMError("Cancelled; no ROM command was sent.")
        session.check_stock()  # Always obtain a fresh response after confirmation.
        session.enter_rom_once()
        emit("One ROM request was sent. Sending it does not prove the device entered ROM mode.")
        emit("To return to unchanged stock firmware, power the FM-1 fully off and unplug USB, then reconnect and power it on.")
        emit("This helper does not verify ROM USB enumeration or recovery.")
    finally:
        session.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="action", required=True)
    commands.add_parser("list", help="List MIDI names without sending messages")
    for action in ("check", "enter-rom"):
        command = commands.add_parser(action)
        command.add_argument("--input", required=True, dest="input_name")
        command.add_argument("--output", required=True, dest="output_name")
    args = parser.parse_args(argv)
    try:
        run(MidiSession(), args.action, getattr(args, "input_name", None), getattr(args, "output_name", None))
        return 0
    except (KeyboardInterrupt, EOFError):
        print("Stopped. No command is retried automatically.", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"Stopped: {exc}. No command is retried automatically.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
