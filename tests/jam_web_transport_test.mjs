// SPDX-License-Identifier: GPL-3.0-only
// Hardware-free framing, selection, cancellation and transfer boundary checks.
// Run: node --test tests/jam_web_transport_test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import { getEventListeners } from 'node:events';
import { Updater, pack7, unpack7, encodeMessage, decodeMessage, parseIdentity, parseRequest, OUR_LOADER } from '../web/fm1ota.js';

const HS = [0xf0, 0, 0x32, 0x45, 0, 0, 0, 0x40, 0x7f, 0xf7];
const UPGRADE = [0xf0, 0x22, 0x24, 0x35, 0x7f, 0xf7];
const equal = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const tick = () => new Promise(resolve => setImmediate(resolve));
const ascii = (text, length) => [...new TextEncoder().encode(text), ...Array(length - text.length).fill(0)];
const identity = (text = 'FM-1_700') => encodeMessage(0x11, ascii(text, 27));
const request = (addr = 0, length = 8, flags = 0) => encodeMessage(0x30,
  [flags, addr & 255, addr >>> 8 & 255, addr >>> 16 & 255, addr >>> 24, length & 255, length >>> 8 & 255, length >>> 16 & 255]);
function rawChange(packet, change) {
  const bytes = unpack7(packet.slice(1, -1)); change(bytes);
  return [0xf0, ...pack7(bytes), 0xf7];
}
function pair(access, suffix = '1', name = 'JamBoxFM', text = 'FM-1_700') {
  class Port extends EventTarget {
    constructor(type) { super(); this.type = type; this.id = type + suffix; this.name = name; this.state = 'connected'; this.opens = 0; this.closes = 0; this.sent = []; }
    async open() { this.opens++; return this; }
    async close() { this.closes++; return this; }
    send(data) { assert.equal(this.state, 'connected'); this.sent.push([...data]); this.onSend?.([...data]); }
    receive(data) { const event = new Event('midimessage'); event.data = Uint8Array.from(data); this.dispatchEvent(event); }
    disconnect() { this.state = 'disconnected'; this.dispatchEvent(new Event('statechange')); }
  }
  const input = new Port('input'), output = new Port('output');
  output.onSend = data => { if (equal(data, HS) && text) input.receive(identity(text)); };
  access.inputs.set(input.id, input); access.outputs.set(output.id, output);
  return { input, output, selected: { inputId: input.id, outputId: output.id }, text };
}
function device(options = {}) {
  const access = { inputs: new Map(), outputs: new Map() }, ports = pair(access, '1', options.name, options.text);
  const updater = new Updater(access, null, { pair: ports.selected, ...options.updater });
  return { access, ...ports, updater };
}
async function connected(options) {
  const mock = device(options), dev = await mock.updater.find(null, { pair: mock.selected });
  mock.output.sent.length = 0;
  return { ...mock, dev, link: dev.link };
}

test('strict identity and request envelopes reject noncanonical or corrupt frames', () => {
  assert.deepEqual(parseIdentity(identity()), { text: 'FM-1_700', model: 'FM-1', version: 700 });
  assert.deepEqual(parseRequest(request(0x87654321, 512, 5)), { fl: 5, addr: 0x87654321, len: 512 });
  for (const [valid, parse] of [[identity(), parseIdentity], [request(), parseRequest]]) {
    const changes = [
      data => { data[0] = 0xf1; }, data => { data[data.length - 1] = 0xf6; },
      data => { data[2] |= 0x80; }, data => { data[data.length - 2] |= 0x40; },
      data => { data.splice(data.length - 1, 0, 0); }, data => { data.splice(2, 1); },
    ];
    for (const change of changes) { const bad = valid.slice(); change(bad); assert.equal(parse(bad), null); }
    for (const offset of [0, 1, 2, 3, 4, 5, -1])
      assert.equal(parse(rawChange(valid, bytes => { const index = offset < 0 ? bytes.length - 1 : offset; bytes[index] ^= 1; })), null);
    assert.equal(parse(undefined), null); assert.equal(parse([]), null);
  }
  assert.equal(decodeMessage(identity(), 0x11, -1), null);
  assert.equal(decodeMessage(identity(), NaN, 27), null);
  assert.throws(() => encodeMessage(0x12, [256]), TypeError);
});

test('identity text is ASCII with canonical NUL padding and a numeric version', () => {
  for (const text of ['FM-1_700\0x', 'FM-1_7\n0', 'FM-1_7 0', 'FM-1_-1', 'FM-1_1e2', 'FM-1_9007199254740992', 'FM-1_70é']) {
    const body = new Uint8Array(27); body.set(new TextEncoder().encode(text));
    assert.equal(parseIdentity(encodeMessage(0x11, body)), null, text);
  }
  const fullWidth = 'ABCDEFGHIJKLMNOPQRSTUV_1234';
  assert.equal(fullWidth.length, 27);
  assert.equal(parseIdentity(encodeMessage(0x11, ascii(fullWidth, 27))).text, fullWidth);
  assert.equal(OUR_LOADER({ text: 'ota-FM-1_700' }), true);
  assert.equal(OUR_LOADER({ text: 'ota-FM-1_701' }), false);
  assert.equal(OUR_LOADER({ text: 'ota-FM-1_900' }), true);
});

test('read-only identify sends only the handshake and supplied query, then closes both ports', async () => {
  const mock = device();
  const capabilities = { family: 'jamboxfm', mode: 'normal' };
  const observation = await mock.updater.identify({ pair: mock.selected, query: async (link, id) => {
    assert.equal(id.text, 'FM-1_700'); assert.equal(link.send(encodeMessage(0x12)), true); return capabilities;
  } });
  assert.deepEqual(observation.capabilities, capabilities);
  assert.deepEqual(mock.output.sent, [HS, encodeMessage(0x12)]);
  assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
  assert.equal(mock.input.onmidimessage, undefined);
  await assert.rejects(new Updater(mock.access).identify(), { code: 'ambiguous' });
  await assert.rejects(mock.updater.identify({ query: link => link.send(UPGRADE) }), { code: 'readonly' });
  assert.deepEqual(mock.output.sent, [HS, encodeMessage(0x12), HS]);
  assert.equal(mock.input.closes, 2); assert.equal(mock.output.closes, 2);
});

test('identify allowlist rejects reset, editor writes, nonempty and noncanonical queries', async () => {
  const badFamily = encodeMessage(0x12); badFamily.splice(badFamily.length - 1, 0, 0);
  for (const bytes of [[0xff], [0xf0, 0x7d, 0x46, 0x4c, 3, 0, 0, 0, 0xf7],
    [0xf0, 0x7d, 0x46, 0x4c, 1, 0, 0xf7], encodeMessage(0x12, [0]), badFamily]) {
    const mock = device();
    await assert.rejects(mock.updater.identify({ query: link => link.send(bytes) }), { code: 'readonly' });
    assert.deepEqual(mock.output.sent, [HS]); assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
  }
  const mock = device(), info = [0xf0, 0x7d, 0x46, 0x4c, 1, 0xf7];
  await mock.updater.identify({ query: link => { assert.equal(link.send(HS), true); assert.equal(link.send(info), true); } });
  assert.deepEqual(mock.output.sent, [HS, HS, info]);
});

test('port discovery is deterministic and rejects ambiguous automatic or mismatched pairs', async () => {
  const access = { inputs: new Map(), outputs: new Map() };
  const z = pair(access, 'z', 'JamBoxFM Z'), a = pair(access, 'a', 'JamBoxFM A');
  const updater = new Updater(access);
  assert.deepEqual(updater.listPairs().map(value => value.inputId), [a.input.id, z.input.id]);
  const duplicate = pair(access, 'duplicate', 'JamBoxFM A');
  assert.deepEqual(updater.listPairs().map(value => value.inputId), [z.input.id]);
  assert.equal((await updater.identify({ pair: a.selected })).identity.text, 'FM-1_700');
  assert.equal(duplicate.output.sent.length, 0); assert.equal(z.output.sent.length, 0);
  await assert.rejects(updater.identify({ pair: { inputId: a.input.id, outputId: z.output.id } }), { code: 'ambiguous' });
  const wrong = device(); wrong.output.name = 'Different MIDI Device';
  assert.equal(await new Updater(wrong.access).find(), null);
  assert.equal(wrong.output.sent.length, 0);
});

test('abort before identify or while selecting/waiting never sends upgrade commands', async () => {
  const mock = device(), controller = new AbortController(); controller.abort();
  await assert.rejects(mock.updater.identify({ signal: controller.signal }), { code: 'aborted' });
  assert.equal(mock.input.opens, 0); assert.equal(mock.output.sent.length, 0);
  const waiting = new AbortController();
  const updater = new Updater(mock.access, null, { signal: waiting.signal, selectPair: () => new Promise(() => {}) });
  const pending = updater.waitFor(null, 1000); waiting.abort();
  await assert.rejects(pending, { code: 'aborted' });
  const absent = new AbortController();
  const noPorts = new Updater({ inputs: new Map(), outputs: new Map() }, null, { signal: absent.signal });
  const poll = noPorts.waitFor(null, 1000); await tick(); absent.abort();
  await assert.rejects(poll, { code: 'aborted' });
});

test('abort during port open is prompt and late cleanup cannot close a newer read', async () => {
  const mock = device(), controller = new AbortController(); let completeOpen;
  mock.input.open = () => { mock.input.opens++; return new Promise(resolve => { completeOpen = resolve; }); };
  const pending = mock.updater.identify({ signal: controller.signal }); await tick(); controller.abort();
  await assert.rejects(pending, { code: 'aborted' });
  assert.equal(mock.output.opens, 0); assert.equal(mock.output.sent.length, 0);
  const order = []; mock.input.close = async () => { order.push('close'); mock.input.closes++; };
  mock.input.open = async () => { order.push('new-open'); mock.input.opens++; };
  const nextRead = new Updater(mock.access).identify({ pair: mock.selected }); await tick();
  assert.equal(mock.input.opens, 1, 'new read waits for the pending open cleanup');
  completeOpen(); await nextRead;
  assert.deepEqual(order.slice(0, 2), ['close', 'new-open']);
});

test('stalled closes are bounded, detach listeners immediately, and prevent premature reopen', { timeout: 4000 }, async () => {
  const mock = await connected(), order = [];
  let finishClose;
  mock.input.close = () => {
    mock.input.closes++; order.push('old-close-start');
    return new Promise(resolve => { finishClose = () => { order.push('old-close-complete'); resolve(); }; });
  };
  const start = Date.now(), closing = mock.link.close();
  await tick();
  assert.equal(mock.link.closed, true);
  assert.equal(getEventListeners(mock.input, 'midimessage').length, 0);
  for (const port of [mock.input, mock.output]) assert.equal(getEventListeners(port, 'statechange').length, 0);
  mock.input.receive(identity());
  assert.equal(mock.link.q.length, 0, 'late packets cannot refill a detached link');
  const retry = new Updater(mock.access);
  await assert.rejects(retry.identify({ pair: mock.selected, deadline: Date.now() + 35 }), { code: 'timeout' });
  assert.equal(mock.input.opens, 1); assert.equal(mock.output.opens, 1);
  assert.equal(mock.input.closes, 1, 'a cleanup-wait timeout must not schedule another close');
  await closing;
  assert.ok(Date.now() - start < 1800, 'close itself must have a finite wait');
  const retryStart = Date.now();
  await assert.rejects(retry.identify({ pair: mock.selected }), { code: 'timeout' });
  assert.ok(Date.now() - retryStart < 1800, 'retry is bounded even without a caller deadline');
  assert.equal(mock.input.opens, 1); assert.equal(mock.input.closes, 1);
  mock.input.open = async () => { order.push('new-open'); mock.input.opens++; };
  mock.input.close = async () => { order.push('new-close'); mock.input.closes++; };
  const pending = retry.identify({ pair: mock.selected });
  await tick(); assert.equal(mock.input.opens, 1, 'timing out cleanup does not release the port');
  finishClose();
  assert.equal((await pending).identity.text, 'FM-1_700');
  assert.deepEqual(order, ['old-close-start', 'old-close-complete', 'new-open', 'new-close']);
});

test('failed closes settle without masking an observation or racing the next open', async () => {
  const mock = device(); let rejectClose;
  mock.input.close = () => { mock.input.closes++; return new Promise((_, reject) => { rejectClose = reject; }); };
  mock.output.close = async () => { mock.output.closes++; throw new Error('disconnected during close'); };
  const first = mock.updater.identify(); await tick();
  const second = new Updater(mock.access).identify({ pair: mock.selected }); await tick();
  assert.equal(mock.input.opens, 1);
  mock.input.close = async () => { mock.input.closes++; };
  rejectClose(new Error('close failed'));
  assert.equal((await first).identity.text, 'FM-1_700');
  assert.equal((await second).identity.text, 'FM-1_700');
  assert.equal(mock.input.opens, 2); assert.equal(mock.input.closes, 2); assert.equal(mock.output.closes, 2);
});

test('partial-open cleanup cannot hang an error or close a port owned by a pending retry', { timeout: 2500 }, async () => {
  const mock = device(); let finishClose;
  mock.output.open = async () => { mock.output.opens++; throw new Error('open refused'); };
  mock.input.close = () => { mock.input.closes++; return new Promise(resolve => { finishClose = resolve; }); };
  const start = Date.now();
  await assert.rejects(mock.updater.identify(), { code: 'permission' });
  assert.ok(Date.now() - start < 1800);
  assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
  await assert.rejects(new Updater(mock.access).identify({ pair: mock.selected, deadline: Date.now() + 35 }), { code: 'timeout' });
  assert.equal(mock.input.opens, 1); assert.equal(mock.output.opens, 1);
  assert.equal(mock.input.closes, 1, 'failed retry owns no ports to close');
  finishClose(); await tick();
});

test('cancelled late open tracks its stalled close until cleanup actually settles', async () => {
  const mock = device(), controller = new AbortController(); let finishOpen, finishClose;
  mock.input.open = () => { mock.input.opens++; return new Promise(resolve => { finishOpen = resolve; }); };
  mock.input.close = () => { mock.input.closes++; return new Promise(resolve => { finishClose = resolve; }); };
  const start = Date.now(), first = mock.updater.identify({ signal: controller.signal });
  await tick(); controller.abort();
  await assert.rejects(first, { code: 'aborted' });
  assert.ok(Date.now() - start < 300); assert.equal(mock.input.closes, 0);
  mock.input.open = async () => { mock.input.opens++; };
  finishOpen(); await tick();
  assert.equal(mock.input.closes, 1);
  await assert.rejects(new Updater(mock.access).identify({ pair: mock.selected, deadline: Date.now() + 35 }), { code: 'timeout' });
  assert.equal(mock.input.opens, 1); assert.equal(mock.output.opens, 0);
  assert.equal(mock.input.closes, 1);
  mock.input.close = async () => { mock.input.closes++; };
  const next = new Updater(mock.access).identify({ pair: mock.selected });
  await tick(); assert.equal(mock.input.opens, 1);
  finishClose();
  assert.equal((await next).identity.text, 'FM-1_700');
  assert.equal(mock.input.opens, 2); assert.equal(mock.input.closes, 2);
});

test('abort during handshake send and receive removes listeners and closes ports', async () => {
  for (const when of ['send', 'receive']) {
    const mock = device(), controller = new AbortController();
    mock.output.onSend = () => { if (when === 'send') controller.abort(); };
    const pending = mock.updater.identify({ signal: controller.signal });
    if (when === 'receive') { await tick(); controller.abort(); }
    await assert.rejects(pending, { code: 'aborted' });
    assert.deepEqual(mock.output.sent, [HS]);
    assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
    mock.input.receive(identity()); // detached listener cannot retain late traffic
  }
});

test('waitFor keeps its deadline during malformed floods, unresolved selection, and port opens', async () => {
  for (const kind of ['flood', 'selection', 'open']) {
    const mock = device(); let interval;
    mock.output.onSend = () => {};
    const updater = kind === 'selection' ? new Updater(mock.access, null, { selectPair: () => new Promise(() => {}) }) : mock.updater;
    if (kind === 'flood') interval = setInterval(() => mock.input.receive([0xf0, 0, 0xf7]), 1);
    if (kind === 'open') mock.input.open = () => new Promise(() => {});
    const start = Date.now();
    try { assert.equal(await updater.waitFor(null, 35), null); }
    finally { clearInterval(interval); }
    assert.ok(Date.now() - start < 300, kind);
  }
});

test('serve supports 481 and 512 byte datagrams with exact checksums and payloads', async () => {
  const mock = await connected(), image = Uint8Array.from({ length: 1200 }, (_, i) => i * 17 & 255);
  const reads = [[10, 481], [512, 512], [0xf0000000, 8]], progress = [];
  let index = 0;
  mock.output.onSend = data => {
    const [addr, length] = reads[index++], body = decodeMessage(data, 0x30, length + 8);
    assert.ok(body, `canonical response for ${length} bytes`);
    assert.equal((body[1] | body[2] << 8 | body[3] << 16 | body[4] << 24) >>> 0, addr);
    assert.deepEqual([...body.slice(8)], addr === 0xf0000000 ? ascii('success', 8) : [...image.slice(addr, addr + length)]);
    if (index < reads.length) mock.input.receive(request(...reads[index]));
  };
  mock.input.receive(request(...reads[0]));
  try {
    assert.deepEqual(await mock.updater.serve(mock.link, image, 0xf0000000, 100, (...args) => progress.push(args)), { served: 2, finished: true });
    assert.equal(index, 3); assert.deepEqual(progress, [[1, 10], [2, 512]]);
  } finally { await mock.link.close(); }
});

test('serve rejects zero, oversized, out-of-bounds and wrong-phase completion requests before responding', async () => {
  for (const [addr, length, flags] of [[0, 0, 0], [0, 513, 0], [1000, 100, 0], [0xfffffffe, 512, 0],
    [0xe0000000, 8, 0], [0xf0000000, 0, 0], [0xf0000000, 513, 0], [0xf0000000, 7, 0], [0xf0000000, 8, 1]]) {
    const mock = await connected(); mock.input.receive(request(addr, length, flags));
    try { await assert.rejects(mock.updater.serve(mock.link, new Uint8Array(1024), 0xf0000000, 100), { code: 'badreq' }); }
    finally { await mock.link.close(); }
    assert.equal(mock.output.sent.length, 0, `${addr.toString(16)}:${length}:${flags}`);
  }
});

test('malformed requests cannot extend idle deadline and floods have a bounded queue', async () => {
  const mock = await connected();
  const flood = setInterval(() => mock.input.receive(rawChange(request(), bytes => { bytes[bytes.length - 1] ^= 1; })), 1);
  const start = Date.now();
  try {
    assert.deepEqual(await mock.updater.serve(mock.link, new Uint8Array(32), 0xf0000000, 35), { served: 0, finished: false });
    assert.ok(Date.now() - start < 300); assert.equal(mock.output.sent.length, 0);
  } finally { clearInterval(flood); await mock.link.close(); }
  for (const kind of ['queue', 'packet']) {
    const bounded = await connected();
    if (kind === 'queue') for (let n = 0; n < 65; n++) bounded.input.receive([0xf0, 0xf7]);
    else bounded.input.receive([0xf0, ...Array(1024).fill(0), 0xf7]);
    try { await assert.rejects(bounded.link.read(100), { code: 'badreq' }); }
    finally { await bounded.link.close(); }
  }
});

test('completion and data send failures are never reported as finished', async () => {
  for (const addr of [0, 0xf0000000]) {
    const mock = await connected(); mock.output.send = () => { throw new Error('disconnected while sending'); };
    mock.input.receive(request(addr, 8));
    try {
      assert.deepEqual(await mock.updater.serve(mock.link, new Uint8Array(32), 0xf0000000, 100), { served: 0, finished: false, lost: true });
    } finally { await mock.link.close(); }
  }
});

test('disconnect wakes a pending read and abort after a data request sends no data', async () => {
  const mock = await connected(), start = Date.now();
  const pending = mock.updater.serve(mock.link, new Uint8Array(32), 0xf0000000, 5000);
  mock.input.disconnect();
  assert.deepEqual(await pending, { served: 0, finished: false, lost: true });
  assert.ok(Date.now() - start < 300); await mock.link.close();
  const controller = new AbortController(), aborted = await connected({ updater: { signal: controller.signal } });
  aborted.input.receive(request(0, 8));
  const transfer = aborted.updater.serve(aborted.link, new Uint8Array(32), 0xf0000000, 100);
  await tick(); controller.abort();
  await assert.rejects(transfer, { code: 'aborted' });
  assert.equal(aborted.output.sent.length, 0); await aborted.link.close();
});

test('family guard runs before application and loader UPGRADE and is mandatory for JamBox', async () => {
  for (const text of ['FM-1_700', 'ota-FM-1_700']) {
    for (const guard of [undefined, () => false]) {
      const mock = device({ text }), seen = [];
      const identityGuard = guard && ((id, context) => { seen.push([id.text, context.stage]); return guard(); });
      const result = text.startsWith('ota-') ? mock.updater.resume(new Uint8Array(32), null, { product: 'FM-1_700', identityGuard }) :
        mock.updater.install(new Uint8Array(32), 'FM-1_700', null, { identityGuard });
      await assert.rejects(result, { code: 'identity' });
      assert.deepEqual(mock.output.sent, [HS]);
      if (guard) assert.deepEqual(seen, [[text, text.startsWith('ota-') ? 'loader' : 'app']]);
      assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
    }
  }
});

test('application and loader command send failures propagate and clean up', async () => {
  for (const text of ['FM-1_700', 'ota-FM-1_700']) {
    const mock = device({ text }), handshake = mock.output.onSend;
    mock.output.onSend = data => { if (equal(data, UPGRADE)) throw new Error('send failed'); handshake(data); };
    const opts = { product: 'FM-1_700', identityGuard: () => true };
    const pending = text.startsWith('ota-') ? mock.updater.resume(new Uint8Array(32), null, opts) : mock.updater.install(new Uint8Array(32), 'FM-1_700', null, opts);
    await assert.rejects(pending, { code: 'lost' });
    assert.deepEqual(mock.output.sent, [HS, UPGRADE]); assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
  }
});

test('aborting in an asynchronous guard or upgrade send prevents any transfer data', async () => {
  for (const text of ['FM-1_700', 'ota-FM-1_700']) {
    for (const when of ['guard', 'send']) {
      const controller = new AbortController(), mock = device({ text, updater: { signal: controller.signal } });
      const handshake = mock.output.onSend;
      mock.output.onSend = data => { if (equal(data, UPGRADE)) controller.abort(); else handshake(data); };
      const opts = { product: 'FM-1_700', identityGuard: async () => { await tick(); if (when === 'guard') controller.abort(); return true; } };
      const pending = text.startsWith('ota-') ? mock.updater.resume(new Uint8Array(32), null, opts) : mock.updater.install(new Uint8Array(32), 'FM-1_700', null, opts);
      await assert.rejects(pending, { code: 'aborted' });
      assert.deepEqual(mock.output.sent, when === 'guard' ? [HS] : [HS, UPGRADE]);
      assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
    }
  }
});

test('final family guard is required before success even when numeric identity matches', async () => {
  const mock = device(), steps = [], seen = [];
  await assert.rejects(mock.updater.returned('FM-1_700', name => steps.push(name), {
    pair: mock.selected, product: 'FM-1_700', identityGuard: (id, context) => { seen.push([id.text, context.stage]); return false; },
  }), { code: 'identity' });
  assert.deepEqual(steps, ['reboot']); assert.deepEqual(seen, [['FM-1_700', 'final']]);
  assert.deepEqual(mock.output.sent, [HS]); assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
});

test('verified final return remains successful when disconnected port closes reject', async () => {
  const mock = device(), steps = [];
  for (const port of [mock.input, mock.output]) port.close = async () => { port.closes++; throw new Error('port already disconnected'); };
  const result = await mock.updater.returned('FM-1_700', name => steps.push(name), {
    pair: mock.selected, product: 'FM-1_700', identityGuard: () => true,
  });
  assert.equal(result, 'FM-1_700'); assert.deepEqual(steps, ['reboot', 'done']);
  assert.equal(mock.input.closes, 1); assert.equal(mock.output.closes, 1);
  assert.equal(getEventListeners(mock.input, 'midimessage').length, 0);
});
