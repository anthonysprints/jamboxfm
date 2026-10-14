// SPDX-License-Identifier: GPL-3.0-only
// Test-only transfer orchestration. Never imported by, or published with, the UI.
import assert from 'node:assert/strict';
import { Updater } from '../../web/fm1ota.js';
import { inspectDevice, simulationTransition, JAM_VERSION, SLOOP_26 } from '../../web/jam-device-policy.js';

// Independent whole-integer wire codec, separate from the production stream codec.
export function frame(command, body) {
  const raw = [0, 0x59, command, body.length & 255, (body.length >>> 8) & 255, body.length >>> 16,
    ...body, ~body.reduce((a, b) => a + b, 0) & 255];
  let bits = raw.reduce((n, b, i) => n | BigInt(b) << BigInt(i * 8), 0n);
  return [240, ...Array.from({ length: Math.ceil(raw.length * 8 / 7) }, (_, i) => Number((bits >> BigInt(i * 7)) & 127n)), 247];
}
function unframe(packet) {
  const bits = packet.slice(1, -1).reduce((n, b, i) => n | BigInt(b) << BigInt(i * 7), 0n);
  return Array.from({ length: Math.floor((packet.length - 2) * 7 / 8) }, (_, i) => Number(bits >> BigInt(i * 8) & 255n));
}
export function identity(text) {
  const match = /^(.+)_(\d+)$/.exec(text);
  return { text, model: match?.[1] || '', version: Number(match?.[2] || 0) };
}
export function identityFrame(text) { return frame(0x11, Array.from({ length: 27 }, (_, i) => text.charCodeAt(i) || 0)); }
export function familyFrame(mode = 'normal', changes = {}) {
  const body = new Uint8Array(48), index = ['normal', 'rescue', 'loader'].indexOf(mode);
  body.set(Buffer.from(changes.family ?? 'JAMBOXFM')); body[8] = changes.schema ?? 1; body[9] = changes.mode ?? index;
  const mask = changes.mask ?? [0x17, 0x05, 0x09][index]; body[10] = mask & 255; body[11] = mask >>> 8;
  body.set(Buffer.from(changes.release ?? JAM_VERSION), 12);
  body.set(Buffer.from(changes.identity ?? ['FM-1_700', 'FM-1_000', 'ota-FM-1_700'][index]), 32);
  return frame(0x12, [...body]);
}
// editor.c ED_INFO in pinned Sloop 2.6: core.h enums + engines/engines.c order.
export function sloopInfoFrame(changes = {}) {
  const strings = text => [...Buffer.from(text), 0];
  const names = changes.names ?? ['ANALOG', 'DIGITAL', 'PHASE', 'LOFI', 'SAMPLE', 'VOICE', 'TRIO', 'WHEEL', 'GRAIN', 'FM6', 'PHYS', 'NOISE'];
  return [240, 0x7d, 0x46, 0x4c, 1, ...strings(changes.name ?? 'FELUCCA SLOOP 2.6'),
    names.length, changes.parameters ?? 71, changes.globals ?? 45, 64, changes.engineParameterStart ?? 63,
    ...names.flatMap(strings), 4, changes.protocol ?? 10, 247];
}
export function profile(name, changes = {}) {
  const modes = { jam: 'normal', rescue: 'rescue', loader: 'loader' };
  if (name in modes) return { text: { jam: 'FM-1_700', rescue: 'FM-1_000', loader: 'ota-FM-1_700' }[name],
    family: familyFrame(modes[name]), ...changes };
  if (name === 'sloop') return { text: 'FM-1_900', info: sloopInfoFrame(), ...changes };
  return { text: name, ...changes };
}
function request(address, length = 8) {
  return frame(0x30, [0, address & 255, address >>> 8 & 255, address >>> 16 & 255, address >>> 24,
    length & 255, length >>> 8 & 255, length >>> 16]);
}
const mocks = new WeakSet();
class Port {
  constructor(access, type, stage) {
    this.access = access; this.type = type; this.id = `mock-${stage}-${type}`; this.name = `JamBoxFM simulation ${stage}`;
    this.state = 'connected'; this.listeners = new Map(); this.opens = 0; this.closes = 0;
  }
  async open() { this.opens++; }
  async close() { this.closes++; }
  addEventListener(kind, listener) { if (!this.listeners.has(kind)) this.listeners.set(kind, new Set()); this.listeners.get(kind).add(listener); }
  removeEventListener(kind, listener) { const set = this.listeners.get(kind); set?.delete(listener); if (!set?.size) this.listeners.delete(kind); }
  emit(packet) { for (const listener of this.listeners.get('midimessage') || []) listener({ data: Uint8Array.from(packet) }); }
  send(packet) { assert.equal(this.type, 'output'); this.access.send(Array.from(packet)); }
}
export class MockMIDIAccess {
  constructor({ source = profile('jam'), loader = profile('loader'), final = profile('jam'), fault = null } = {}) {
    this.inputs = new Map(); this.outputs = new Map(); this.transcript = []; this.ports = [];
    this.profiles = { app: source, loader, final }; this.stage = source.text.startsWith('ota-') ? 'loader' : 'app';
    if (this.stage === 'loader') this.profiles.loader = source;
    this.fault = fault; this.upgrades = []; this.responses = []; this.expectedImage = null;
    this.handshakes = 0;
    this.attach(this.stage); mocks.add(this);
  }
  attach(stage) {
    this.stage = stage;
    for (const port of this.ports) port.state = 'disconnected';
    this.input = new Port(this, 'input', stage); this.output = new Port(this, 'output', stage);
    this.ports.push(this.input, this.output); this.inputs.set(this.input.id, this.input); this.outputs.set(this.output.id, this.output);
  }
  pair() { return { inputId: this.input.id, outputId: this.output.id }; }
  send(packet) {
    this.transcript.push({ stage: this.stage, packet });
    const current = this.profiles[this.stage];
    if (String(packet) === '240,0,50,69,0,0,0,64,127,247') {
      this.handshakes++;
      if (this.fault?.stage === this.stage && this.fault.type === 'identity-change' && this.handshakes > 1)
        this.profiles[this.stage] = profile('FM-1_015');
      if (this.fault?.stage === this.stage && this.fault.type === 'abort-identify') this.fault.controller.abort();
      return this.input.emit(identityFrame(this.profiles[this.stage].text));
    }
    if (String(packet) === String(frame(0x12, []))) { if (current.family) this.input.emit(current.family); return; }
    if (String(packet) === '240,125,70,76,1,247') { if (current.info) this.input.emit(current.info); return; }
    if (String(packet) === '240,34,36,53,127,247') {
      this.upgrades.push(this.stage);
      if (this.fault?.stage === this.stage && this.fault.type === 'silence') { this.fault.onSilence(); return; }
      if (this.fault?.stage === this.stage && this.fault.type === 'abort') { this.fault.controller.abort(); return; }
      if (this.fault?.stage === this.stage && this.fault.type === 'disconnect') { this.input.state = this.output.state = 'disconnected'; return; }
      if (this.fault?.stage === this.stage && this.fault.type === 'bad-request') return this.input.emit(request(this.expectedImage.length, 1));
      this.input.emit(request(0, 512)); this.input.emit(request(this.expectedImage.length - 481, 481));
      this.input.emit(request(this.stage === 'app' ? 0xe0000000 : 0xf0000000)); return;
    }
    const raw = unframe(packet), body = raw.slice(6, -1);
    assert.equal(raw[2], 0x30, 'Mock rejects any unexpected write command');
    assert.deepEqual(packet, frame(0x30, body), 'Response must have canonical framing/checksum');
    const address = (body[1] | body[2] << 8 | body[3] << 16 | body[4] << 24) >>> 0;
    const length = body[5] | body[6] << 8 | body[7] << 16;
    assert.equal(length, body.length - 8);
    if (address >= 0xe0000000) {
      assert.deepEqual(body.slice(8), [...Buffer.from('success\0')]);
      if (this.fault?.stage === this.stage && this.fault.type === 'finish-send') throw new Error('Simulated failed terminal send');
      if (this.fault?.stage === this.stage && this.fault.type === 'missing-after-finish') {
        for (const port of this.ports) port.state = 'disconnected'; return;
      }
      this.attach(this.stage === 'app' ? 'loader' : 'final');
    } else {
      assert.deepEqual(body.slice(8), [...this.expectedImage.subarray(address, address + length)]);
      this.responses.push({ stage: this.stage, address, length });
    }
  }
}

export async function simulateInstall({ access, pair, candidate, signal }) {
  assert.ok(mocks.has(access), 'Explicit MockMIDIAccess required; physical MIDI is not supported');
  assert.ok(pair?.inputId && pair?.outputId, 'Select a mock input/output pair');
  const updater = new Updater(access, () => {}), steps = [], selected = [], guards = [];
  const initial = await updater.identify({ pair, signal, query: (link, id) => inspectDevice(id, link, { signal, timeoutMs: 5 }) });
  const source = initial.capabilities, permission = simulationTransition(source, candidate);
  assert.ok(permission.allowed, 'Transition is outside the simulated policy');
  access.expectedImage = candidate.image();
  let passedApp = false;
  const identityGuard = async (id, context) => {
    guards.push(context.stage);
    const observed = await inspectDevice(id, context.link, { signal, timeoutMs: 5 });
    if (context.stage === 'app') {
      passedApp = observed.recognized && observed.family === source.family && observed.mode === source.mode && observed.release === source.release;
      return passedApp;
    }
    if (context.stage === 'loader') {
      if (candidate.family === 'jamboxfm') return observed.recognized && observed.family === 'jamboxfm' && observed.mode === 'loader';
      // This foreign loader is trusted only after stage one of this exact pinned
      // return transaction. It never becomes a recognized standalone device.
      return passedApp && source.family === 'jamboxfm' && candidate.sha256 === SLOOP_26.sha256 && id.text === 'ota-FM-1_900';
    }
    return context.stage === 'final' && observed.recognized && observed.family === candidate.family &&
      observed.release === candidate.version && observed.mode === 'normal';
  };
  const options = { pair, signal, product: candidate.product, identityGuard, requireSelection: true,
    selectPair: ({ stage, pairs }) => {
      assert.equal(pairs.length, 1, 'Exactly one expected mock pair must be explicitly selected');
      const chosen = access.pair(); selected.push({ stage, ...chosen }); return chosen;
    } };
  const onStep = (...entry) => steps.push(entry);
  if (source.mode === 'loader') {
    assert.equal(await updater.resume(candidate.image(), onStep, options), true, 'Loader resume did not finish');
  } else await updater.install(candidate.image(), candidate.product, onStep, options);
  return { transition: permission.transition, simulationOnly: true, hardwareValidated: false,
    installAllowed: false, steps, selected, guards, transcript: access.transcript };
}
