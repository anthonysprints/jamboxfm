// SPDX-License-Identifier: GPL-3.0-only
// Test-only Web MIDI device; independently checks every canonical packet and image byte.
// No firmware executes, and this helper is never bundled with the installer.
export function fakeWebMIDI({ config, image }) {
  const nativeTimeout = globalThis.setTimeout;
  // Only transport inter-packet/handover waits are accelerated; operation and
  // identity deadlines stay real. This patch exists only inside this test page.
  globalThis.setTimeout = (fn, ms, ...args) => nativeTimeout(fn, [10, 2000, 3000].includes(ms) ? 0 : ms, ...args);
  const expected = Uint8Array.from(atob(image), char => char.charCodeAt(0));
  const equal = (a, b) => a.length === b.length && a.every((value, index) => value === b[index]);
  const check = (condition, message) => { if (!condition) { state.errors.push(message); throw new Error(message); } };
  const text = value => Array.from(value, char => char.charCodeAt(0));
  const pad = (value, n) => Array.from({ length: n }, (_, i) => value.charCodeAt(i) || 0);
  function frame(command, body) {
    const raw = [0, 0x59, command, body.length & 255, body.length >>> 8 & 255, body.length >>> 16,
      ...body, ~body.reduce((sum, byte) => sum + byte, 0) & 255];
    let bits = 0n;
    for (let i = 0; i < raw.length; i++) bits |= BigInt(raw[i]) << BigInt(i * 8);
    return [240, ...Array.from({ length: Math.ceil(raw.length * 8 / 7) }, (_, i) => Number(bits >> BigInt(i * 7) & 127n)), 247];
  }
  function unframe(packet) {
    let bits = 0n;
    for (let i = 1; i < packet.length - 1; i++) bits |= BigInt(packet[i]) << BigInt((i - 1) * 7);
    return Array.from({ length: Math.floor((packet.length - 2) * 7 / 8) }, (_, i) => Number(bits >> BigInt(i * 8) & 255n));
  }
  const access = new EventTarget(); access.inputs = new Map(); access.outputs = new Map(); access.sysexEnabled = true;
  const state = globalThis.fakeMIDI = { access, permissions: [], upgrades: [], errors: [], sent: 0, devices: [], transfers: [], denied: !!config.denied };
  const emitChange = port => { const event = new Event('statechange'); event.port = port; port.dispatchEvent(event); access.dispatchEvent(event); };
  function attach(device, stage) {
    if (device.input) for (const port of [device.input, device.output]) { port.state = 'disconnected'; emitChange(port); }
    device.stage = stage;
    class Port extends EventTarget {
      constructor(type) {
        super(); this.type = type; this.id = `${device.index}-${stage}-PRIVATE-${type}`;
        this.name = config.hiddenDuplicates && device.index > 0 ? 'FM-1 PRIVATE DUPLICATE' : `FM-1 PRIVATE ${device.index} ${stage}`;
        this.state = 'connected'; this.connection = 'closed'; this.listeners = new Set();
      }
      addEventListener(type, fn, ...rest) { if (type === 'midimessage') this.listeners.add(fn); super.addEventListener(type, fn, ...rest); }
      removeEventListener(type, fn, ...rest) { if (type === 'midimessage') this.listeners.delete(fn); super.removeEventListener(type, fn, ...rest); }
      async open() { this.connection = 'open'; emitChange(this); return this; }
      async close() {
        this.connection = 'closed';
        if (config.lateCloseEvents) nativeTimeout(() => emitChange(this), 30); else emitChange(this);
        return this;
      }
      send(bytes) { transmit(device, Array.from(bytes)); }
    }
    device.input = new Port('input'); device.output = new Port('output');
    access.inputs.set(device.input.id, device.input); access.outputs.set(device.output.id, device.output);
    emitChange(device.input); emitChange(device.output);
  }
  function reply(device, bytes, delay = 0) {
    const port = device.input;
    nativeTimeout(() => { const event = new Event('midimessage'); event.data = Uint8Array.from(bytes); port.dispatchEvent(event); }, delay);
  }
  function requestNext(device) {
    const transfer = device.transfer;
    const complete = transfer.bytes === expected.length;
    const addr = complete ? device.stage === 'app' ? 0xe0000000 : 0xf0000000 : transfer.bytes;
    const length = complete ? 8 : Math.min(transfer.chunks === 1 ? 481 : 512, expected.length - transfer.bytes);
    transfer.requestLength = length;
    reply(device, frame(0x30, [0, addr & 255, addr >>> 8 & 255, addr >>> 16 & 255, addr >>> 24,
      length & 255, length >>> 8 & 255, length >>> 16]));
  }
  function identity(device) {
    return device.stage === 'app' ? device.identity || config.identity || 'FM-1_700' :
      device.stage === 'loader' ? config.loaderIdentity || 'ota-FM-1_700' : config.finalIdentity || 'FM-1_700';
  }
  function transmit(device, packet) {
    state.sent++;
    if (equal(packet, [240, 0, 50, 69, 0, 0, 0, 64, 127, 247])) {
      device.handshakes++;
      if (config.changedIdentity && device.stage === 'app' && device.handshakes > 1) device.identity = config.changedIdentity;
      if (!device.silent) reply(device, frame(0x11, pad(identity(device), 27)), device.stage === 'final' ? config.finalIdentityDelay || 0 : device.identityDelay || 0);
      return;
    }
    if (equal(packet, frame(0x12, []))) {
      const stage = device.stage === 'loader' ? 2 : device.stage === 'app' && config.identity === 'FM-1_000' ? 1 : 0;
      if (device.stage === 'app' && config.missingAppProbe) return;
      const badApp = device.stage === 'app' && (config.badAppFamily || config.changedAppFamily && device.handshakes > 1);
      const family = (config.badLoaderFamily && stage === 2 || badApp) ? 'FOREIGN!' : 'JAMBOXFM';
      const release = config.badFinalRelease && device.stage === 'final' ? '0.0.0' : '0.1.0-dev';
      reply(device, frame(0x12, [...text(family), config.futureSchema || 1, stage, stage === 2 ? 9 : stage === 1 ? 5 : 23, 0, ...pad(release, 20), ...pad(identity(device), 16)])); return;
    }
    if (equal(packet, [240, 125, 70, 76, 1, 247])) {
      if (identity(device) === 'FM-1_900') {
        const str = value => [...text(value), 0];
        reply(device, [240, 125, 70, 76, 1, ...str('FELUCCA SLOOP 2.6'), 12, 71, 45, 64, 63,
          ...['ANALOG','DIGITAL','PHASE','LOFI','SAMPLE','VOICE','TRIO','WHEEL','GRAIN','FM6','PHYS','NOISE'].flatMap(str), 4, 10, 247]);
      }
      return;
    }
    if (equal(packet, [240, 34, 36, 53, 127, 247])) {
      state.upgrades.push(device.stage);
      device.transfer = { stage: device.stage, bytes: 0, chunks: 0, requestLength: 0 }; state.transfers.push(device.transfer);
      if (config.disconnectAtStart) { state.disconnect(device.index); return; }
      requestNext(device); return;
    }
    const raw = unframe(packet), body = raw.slice(6, -1);
    check(raw[2] === 0x30 && equal(packet, frame(0x30, body)), 'Unexpected or noncanonical packet');
    const addr = (body[1] | body[2] << 8 | body[3] << 16 | body[4] << 24) >>> 0;
    const length = body[5] | body[6] << 8 | body[7] << 16;
    check(length === body.length - 8, 'Response body length mismatch');
    if (addr >= 0xe0000000) {
      check(device.transfer.bytes === expected.length, 'Completion before all firmware bytes were served');
      check(equal(body.slice(8), [...text('success'), 0]), 'Incorrect completion token');
      attach(device, device.stage === 'app' ? 'loader' : 'final'); return;
    }
    check(addr === device.transfer.bytes && length === device.transfer.requestLength, 'Response does not match request');
    check(equal(body.slice(8), expected.subarray(addr, addr + length)), 'Response firmware bytes differ from candidate');
    device.transfer.bytes += length; device.transfer.chunks++;
    if (config.disconnectDuringWrite && device.stage === 'loader' && device.transfer.bytes >= 1024) { state.disconnect(device.index); return; }
    requestNext(device);
  }
  state.add = (options = {}) => { const device = { index: state.devices.length, handshakes: 0, ...options }; state.devices.push(device); attach(device, 'app'); };
  state.disconnect = index => { for (const port of [state.devices[index].input, state.devices[index].output]) { port.state = 'disconnected'; emitChange(port); } };
  state.snapshot = () => ({ permissions: state.permissions, upgrades: state.upgrades, errors: state.errors, sent: state.sent,
    transfers: state.transfers.map(({ stage, bytes, chunks }) => ({ stage, bytes, chunks })),
    open: [...access.inputs.values(), ...access.outputs.values()].filter(p => p.connection === 'open').length,
    listeners: [...access.inputs.values()].reduce((n, p) => n + p.listeners.size, 0) });
  for (let i = 0; i < (config.count ?? 1); i++) state.add();
  Object.defineProperty(navigator, 'requestMIDIAccess', { configurable: true, value: config.unsupported ? undefined : async options => {
    state.permissions.push(options);
    if (state.denied) throw new DOMException('Permission denied', 'NotAllowedError');
    return access;
  } });
}
