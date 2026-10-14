// SPDX-License-Identifier: GPL-3.0-only
// Real Chromium with Web MIDI injected before loading the production page.
// . tools/cloud/activate.sh && node tests/jam_device_browser.mjs
// JAM_DEVICE_SITE_ROOT=build/jam-site also checks the packaged /device/ page.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

const playwright = process.env.PLAYWRIGHT_MODULE ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve(process.env.JAM_DEVICE_SITE_ROOT || 'web');
const evidence = resolve('build/jam-device-evidence');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    const path = resolve(root, '.' + pathname + (pathname.endsWith('/') ? 'index.html' : ''));
    if (!path.startsWith(root + sep)) throw new Error('Outside site');
    const body = await readFile(path);
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' });
    response.end(body);
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const address = `http://127.0.0.1:${server.address().port}`;

// Explicit byte allowlist: handshake, JamBox family query, Sloop editor INFO.
// Every outbound MIDI packet from every case must be exactly one of these.
const allowed = [
  [0xf0, 0, 0x32, 0x45, 0, 0, 0, 0x40, 0x7f, 0xf7],
  [0xf0, 0, 0x32, 0x49, 0, 0, 0, 0x40, 0x7f, 0xf7],
  [0xf0, 0x7d, 0x46, 0x4c, 1, 0xf7],
];

function fakeWebMIDI(config) {
  const handshake = [0xf0, 0, 0x32, 0x45, 0, 0, 0, 0x40, 0x7f, 0xf7];
  const capability = [0xf0, 0, 0x32, 0x49, 0, 0, 0, 0x40, 0x7f, 0xf7];
  const info = [0xf0, 0x7d, 0x46, 0x4c, 1, 0xf7];
  const equal = (a, b) => a.length === b.length && a.every((value, index) => value === b[index]);
  const text = value => Array.from(value, char => char.charCodeAt(0));
  const padded = (value, size) => { const bytes = Array(size).fill(0); bytes.splice(0, value.length, ...text(value)); return bytes; };
  // Fixture encoder is independent of the production module under test.
  function message(command, body) {
    const raw = [0, 0x59, command, body.length, 0, 0, ...body, ~body.reduce((sum, byte) => sum + byte, 0) & 255];
    const packed = []; let bits = 0, count = 0;
    for (const byte of raw) {
      bits |= byte << count; count += 8;
      while (count >= 7) { packed.push(bits & 127); bits >>>= 7; count -= 7; }
    }
    if (count) packed.push(bits & 127);
    return [0xf0, ...packed, 0xf7];
  }
  const access = new EventTarget(); access.inputs = new Map(); access.outputs = new Map(); access.sysexEnabled = true;
  const state = globalThis.fakeMIDI = { requests: [], sent: [], opened: [], closed: [], devices: [], denied: !!config.denied };
  const change = port => { const event = new Event('statechange'); event.port = port; access.dispatchEvent(event); };
  function addDevice(options = {}) {
    const index = state.devices.length;
    const device = { kind: 'jam', mode: 'normal', name: 'JamBoxFM PRIVATE PORT', serial: 'PRIVATE-SERIAL-93810', ...options };
    const id = suffix => `${index}-PRIVATE-${suffix}-ID`;
    class Port extends EventTarget {
      constructor(type) {
        super(); this.type = type; this.name = device.name; this.id = id(type); this.state = 'connected'; this.connection = 'closed'; this.listeners = new Set();
      }
      addEventListener(type, fn, ...rest) { if (type === 'midimessage') this.listeners.add(fn); super.addEventListener(type, fn, ...rest); }
      removeEventListener(type, fn, ...rest) { if (type === 'midimessage') this.listeners.delete(fn); super.removeEventListener(type, fn, ...rest); }
      async open() { state.opened.push(this.id); this.connection = 'open'; change(this); return this; }
      async close() {
        if (device.closeDelay) await new Promise(resolve => setTimeout(resolve, device.closeDelay));
        state.closed.push(this.id); this.connection = 'closed'; change(this); return this;
      }
      send(bytes) {
        const data = Array.from(bytes); state.sent.push({ id: this.id, bytes: data });
        let response = null, delay = device.delay || 0;
        const identity = device.identity || (device.kind === 'sloop' ? 'FM-1_900' : device.mode === 'loader' ? 'ota-FM-1_700' : device.mode === 'rescue' ? 'FM-1_000' : 'FM-1_700');
        if (equal(data, handshake)) {
          response = message(0x11, padded(identity, 27));
          if (device.badIdentity) response[response.length - 2] ^= 1;
          delay = device.identityDelay ?? delay;
        } else if (equal(data, capability) && device.kind === 'jam' && !device.noCapability) {
          response = message(0x12, [...text('JAMBOXFM'), 1, ['normal', 'rescue', 'loader'].indexOf(device.mode),
            { normal: 0x17, rescue: 0x05, loader: 0x09 }[device.mode], 0,
            ...padded('0.1.0-dev', 20), ...padded(identity, 16)]);
          if (device.badCapability) response[response.length - 2] ^= 1;
          delay = device.capabilityDelay ?? delay;
        } else if (equal(data, info) && device.kind === 'sloop') {
          const engines = ['ANALOG', 'DIGITAL', 'PHASE', 'LOFI', 'SAMPLE', 'VOICE', 'TRIO', 'WHEEL', 'GRAIN', 'FM6', 'PHYS', 'NOISE'];
          response = [0xf0, 0x7d, 0x46, 0x4c, 1, ...text('FELUCCA SLOOP 2.6'), 0, 12, 71, 45, 64, 63,
            ...engines.flatMap(name => [...text(name), 0]), 4, 10, 0xf7];
        }
        if (response && !device.silent) setTimeout(() => {
          const event = new Event('midimessage'); event.data = Uint8Array.from(response); device.input.dispatchEvent(event); state.delivered++;
        }, delay);
      }
    }
    device.input = new Port('input'); device.output = new Port('output'); state.devices.push(device);
    access.inputs.set(device.input.id, device.input); access.outputs.set(device.output.id, device.output);
    change(device.input); change(device.output); return index;
  }
  state.delivered = 0;
  state.addDevice = addDevice;
  state.disconnect = index => { const device = state.devices[index]; for (const port of [device.input, device.output]) { port.state = 'disconnected'; change(port); } };
  state.snapshot = () => ({ requests: state.requests, sent: state.sent, opened: state.opened, closed: state.closed,
    listeners: state.devices.map(device => device.input.listeners.size), delivered: state.delivered });
  for (const device of config.devices ?? [{}]) addDevice(device);
  Object.defineProperty(navigator, 'requestMIDIAccess', { configurable: true, value: config.unsupported ? undefined : async options => {
    state.requests.push(options);
    if (state.denied) throw new DOMException('Test permission denial', 'NotAllowedError');
    return access;
  } });
}

let browser, passed = 0;
const errors = [], network = [];
async function runCase(name, config, body, options = {}) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 1080 }, ...options });
  page.on('pageerror', error => errors.push(`${name}: ${error.message}`));
  page.on('request', request => network.push({ url: request.url(), method: request.method() }));
  try {
    await page.addInitScript(fakeWebMIDI, config);
    await page.goto(`${address}/device/`);
    await page.waitForFunction(() => document.getElementById('read-device')?.disabled);
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().requests), [], 'MIDI permission requires a user action');
    await body(page);
    const snapshot = await page.evaluate(() => fakeMIDI.snapshot());
    for (const packet of snapshot.sent) assert(allowed.some(bytes => JSON.stringify(bytes) === JSON.stringify(packet.bytes)), `${name}: only read-only packets may be sent: ${packet.bytes}`);
    assert.equal(await page.locator('button').filter({ hasText: /install|upgrade|reset|flash/i }).count(), 0, 'no firmware write control is exposed');
    assert.equal(await page.locator('input[type=file]').count(), 0, 'no firmware upload is exposed');
    passed++; console.log(`PASS ${name}`);
  } finally { await page.close(); }
}
async function enable(page) {
  await page.locator('#midi-enable').click();
  await page.waitForFunction(() => !document.getElementById('port-fields').disabled);
  assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().requests), [{ sysex: true }]);
}
async function select(page, input = 0, output = input) {
  await page.locator('#midi-input').selectOption(`${input}-PRIVATE-input-ID`);
  await page.locator('#midi-output').selectOption(`${output}-PRIVATE-output-ID`);
}
async function read(page) {
  await page.locator('#read-device').click();
  await page.locator('#result').waitFor({ state: 'visible' });
  await page.waitForFunction(() => document.getElementById('cancel-read').hidden && !document.getElementById('read-device').disabled);
}
async function report(page) { return JSON.parse(await page.locator('#report-details').textContent()); }
async function noOpenLinks(page) {
  const snapshot = await page.evaluate(() => fakeMIDI.snapshot());
  assert(snapshot.opened.every(id => snapshot.closed.includes(id)), 'all opened MIDI ports were closed');
  assert(snapshot.listeners.every(count => count === 0), 'all MIDI message listeners were removed');
}

try {
  await mkdir(evidence, { recursive: true });
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox'] });
  await runCase('unsupported browser', { unsupported: true }, async page => {
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('doesn’t support Web MIDI'));
    assert(await page.locator('#midi-enable').isDisabled()); assert(await page.locator('#read-device').isDisabled());
  });
  await runCase('permission denied and retry', { denied: true }, async page => {
    await page.locator('#midi-enable').click();
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('wasn’t allowed'));
    assert(await page.locator('#read-device').isDisabled());
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().sent), []);
    await page.evaluate(() => { fakeMIDI.denied = false; });
    await page.locator('#midi-enable').click();
    await page.waitForFunction(() => !document.getElementById('port-fields').disabled);
    assert.equal((await page.evaluate(() => fakeMIDI.snapshot().requests)).length, 2);
  });
  await runCase('no ports and hotplug', { devices: [] }, async page => {
    await enable(page);
    assert.match(await page.locator('#pair-hint').textContent(), /No matching FM-1/);
    assert(await page.locator('#read-device').isDisabled());
    await page.evaluate(() => fakeMIDI.addDevice());
    await page.waitForFunction(() => document.getElementById('midi-input').options.length === 2);
    assert.equal(await page.locator('#midi-input').inputValue(), '');
    assert.equal(await page.locator('#midi-output').inputValue(), '');
    await select(page); await read(page); await noOpenLinks(page);
  });
  await runCase('explicit ports, JamBox report and private JSON export', {}, async page => {
    await page.screenshot({ path: resolve(evidence, 'desktop-ready.png'), fullPage: true });
    await enable(page);
    assert(await page.locator('#read-device').isDisabled());
    await page.locator('#midi-input').selectOption('0-PRIVATE-input-ID');
    assert(await page.locator('#read-device').isDisabled());
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().sent), []);
    await page.locator('#midi-output').selectOption('0-PRIVATE-output-ID'); await read(page);
    assert.equal(await page.locator('#device-family').textContent(), 'JamBoxFM');
    assert.equal(await page.locator('#device-version').textContent(), '0.1.0-dev');
    assert.equal(await page.locator('#device-mode').textContent(), 'Running normally');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'result-title');
    assert.equal(await page.locator('#status').getAttribute('role'), 'status');
    assert.equal(await page.locator('#status').getAttribute('aria-live'), 'polite');
    assert.match(await page.locator('#result-guidance').textContent(), /not a backup/);
    const downloadEvent = page.waitForEvent('download'); await page.locator('#save-report').click();
    const download = await downloadEvent;
    assert.equal(download.suggestedFilename(), 'jamboxfm-device-report.json');
    const exportedText = await readFile(await download.path(), 'utf8'), exported = JSON.parse(exportedText);
    assert.deepEqual(exported, await report(page));
    assert(!/PRIVATE|serial|inputId|outputId|manufacturer|portName/i.test(exportedText), 'export omits private port metadata and serial numbers');
    assert.equal(exported.installAllowed, false); assert.equal(exported.hardwareValidated, false);
    const sanitized = await page.evaluate(async () => {
      const { deviceReport } = await import('./app.js');
      return deviceReport({ name: 'PRIVATE PORT', pair: { inputId: 'PRIVATE ID' }, serial: 'PRIVATE SERIAL',
        identity: { text: 'FM-1_700', model: 'FM-1', version: 700, serial: 'PRIVATE SERIAL', portId: 'PRIVATE ID' },
        capabilities: { family: 'jamboxfm', mode: 'normal', recognized: true, serial: 'PRIVATE SERIAL', capabilities: { updater: true, serial: 'PRIVATE SERIAL' } } });
    });
    assert(!JSON.stringify(sanitized).includes('PRIVATE'), 'report selects allowed fields instead of copying observations');
    await noOpenLinks(page);
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    await page.screenshot({ path: resolve(evidence, 'desktop-identified.png'), fullPage: true });
    await page.locator('#midi-input').selectOption('');
    assert(await page.locator('#result').isHidden(), 'changing selection clears the previous device result');
  });
  await runCase('mismatched port names', { devices: [{ name: 'JamBoxFM one' }, { name: 'Sloop two' }] }, async page => {
    await enable(page); await select(page, 0, 1);
    assert(await page.locator('#read-device').isDisabled());
    assert.match(await page.locator('#pair-hint').textContent(), /don’t match/);
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().sent), []);
  });
  await runCase('same-name ambiguity requires explicit selection', { devices: [{}, {}] }, async page => {
    await enable(page);
    assert(await page.locator('#read-device').isDisabled());
    const labels = await page.locator('#midi-input option').allTextContents();
    assert.equal(new Set(labels).size, 3, 'duplicate names have separate visible labels');
    await select(page, 1);
    assert.match(await page.locator('#pair-hint').textContent(), /Several ports have this name/);
    await read(page);
    assert((await page.evaluate(() => fakeMIDI.snapshot().sent)).every(packet => packet.id === '1-PRIVATE-output-ID'), 'only the explicitly selected output was queried');
    await noOpenLinks(page);
  });
  await runCase('Sloop identification and backup guidance', { devices: [{ kind: 'sloop', name: 'Sloop PRIVATE PORT' }] }, async page => {
    await enable(page); await select(page); await read(page);
    assert.equal(await page.locator('#device-family').textContent(), 'Sloop');
    assert.equal(await page.locator('#device-version').textContent(), '2.6');
    assert.match(await page.locator('#result-guidance').textContent(), /Sloop’s own editor.*projects, presets, samples, and settings/);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot().sent)).map(packet => packet.bytes), [allowed[0], allowed[2]]);
    await noOpenLinks(page);
  });
  await runCase('factory V15 report is explained without certifying capabilities or sending extra probes', { devices: [{ kind: 'unknown', identity: 'FM-1_015' }] }, async page => {
    await enable(page); await select(page); await read(page);
    const result = await report(page);
    assert.equal(await page.locator('#result-title').textContent(), 'Factory V15 identity reported.');
    assert.equal(await page.locator('#result-badge').textContent(), 'REPORTED');
    assert.equal(await page.locator('#device-family').textContent(), 'Factory FM-1 (reported)');
    assert.match(await page.locator('#result-guidance').textContent(), /not been independently verified/);
    assert.match(await page.locator('#result-guidance').textContent(), /not a backup/);
    assert.equal(result.family, 'unknown'); assert.equal(result.recognized, false);
    assert.equal(result.hardwareValidated, false); assert.equal(result.installAllowed, false);
    assert(Object.values(result.capabilities).every(value => value === false));
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot().sent)).map(packet => packet.bytes), [allowed[0]]);
    await noOpenLinks(page);
    await page.screenshot({ path: resolve(evidence, 'factory-v15-reported.png'), fullPage: true });
  });
  for (const mode of ['rescue', 'loader']) await runCase(`JamBox ${mode} mode`, { devices: [{ mode }] }, async page => {
    await enable(page); await select(page); await read(page);
    assert.equal((await report(page)).mode, mode);
    assert.equal(await page.locator('#device-mode').textContent(), mode === 'rescue' ? 'Recovery mode' : 'Update loader');
    assert.match(await page.locator('#result-guidance').textContent(), /leaves the current mode alone/);
    await noOpenLinks(page);
  });
  for (const [name, device] of [
    ['unknown firmware', { kind: 'unknown', identity: 'FM-1_999' }],
    ['JamBox identity without family evidence', { noCapability: true }],
    ['malformed family reply', { badCapability: true }],
  ]) await runCase(name, { devices: [device] }, async page => {
    await enable(page); await select(page); await read(page);
    const result = await report(page);
    assert.equal(result.family, 'unknown'); assert.equal(result.recognized, false); assert.equal(result.installAllowed, false);
    assert.equal(await page.locator('#device-family').textContent(), 'Unrecognized firmware');
    assert.equal(await page.locator('#result-title').textContent(), 'Firmware not confirmed.');
    assert.match(await page.locator('#result-guidance').textContent(), /couldn’t confirm/);
    await noOpenLinks(page);
  });
  await runCase('malformed identity', { devices: [{ badIdentity: true }] }, async page => {
    await enable(page); await select(page); await page.locator('#read-device').click();
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('No valid FM-1 identity'));
    assert(await page.locator('#result').isHidden()); await noOpenLinks(page);
  });
  await runCase('cancel ignores late reply and waits for port cleanup', { devices: [{ identityDelay: 250, closeDelay: 80 }] }, async page => {
    await enable(page); await select(page); await page.locator('#read-device').click();
    await page.waitForFunction(() => fakeMIDI.sent.length === 1);
    await page.locator('#cancel-read').click();
    assert(await page.locator('#read-device').isDisabled(), 'a new read waits for async close');
    assert.match(await page.locator('#status').textContent(), /cancelled/);
    await page.waitForFunction(() => fakeMIDI.delivered > 0 && !document.getElementById('read-device').disabled);
    assert(await page.locator('#result').isHidden(), 'a late reply cannot restore a cancelled result');
    assert.match(await page.locator('#status').textContent(), /cancelled/); await noOpenLinks(page);
    await page.evaluate(() => { fakeMIDI.devices[0].identityDelay = 0; });
    await read(page); assert.equal((await report(page)).family, 'jamboxfm'); await noOpenLinks(page);
  });
  await runCase('disconnect ignores stale read and requires new ports', { devices: [{ capabilityDelay: 250 }] }, async page => {
    await enable(page); await select(page); await page.locator('#read-device').click();
    await page.waitForFunction(() => fakeMIDI.sent.length === 2);
    await page.evaluate(() => fakeMIDI.disconnect(0));
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('disconnected'));
    await page.waitForFunction(() => fakeMIDI.delivered === 2);
    assert(await page.locator('#result').isHidden()); assert(await page.locator('#read-device').isDisabled());
    await noOpenLinks(page);
    await page.evaluate(() => fakeMIDI.addDevice({ kind: 'sloop' }));
    assert.equal(await page.locator('#midi-input').inputValue(), ''); assert.equal(await page.locator('#midi-output').inputValue(), '');
    assert(await page.locator('#read-device').isDisabled());
    await select(page, 1); await read(page); assert.equal((await report(page)).family, 'sloop');
    await page.evaluate(() => fakeMIDI.disconnect(1));
    assert(await page.locator('#result').isHidden(), 'disconnect also clears a completed report');
  });
  await runCase('page exit cancels an active read', { devices: [{ identityDelay: 150 }] }, async page => {
    await enable(page); await select(page); await page.locator('#read-device').click();
    await page.waitForFunction(() => fakeMIDI.sent.length === 1);
    await page.evaluate(() => window.dispatchEvent(new Event('pagehide')));
    await page.waitForFunction(() => fakeMIDI.delivered === 1);
    assert(await page.locator('#result').isHidden()); await noOpenLinks(page);
  });
  await runCase('mobile layout and result', {}, async page => {
    await enable(page); await select(page); await read(page);
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), '390 px page has no horizontal overflow');
    assert.equal(await page.locator('a[href="../jambox-source.zip"]').count(), 1);
    assert.equal(await page.locator('a[href="../LICENSES.html"]').count(), 1);
    await page.screenshot({ path: resolve(evidence, 'mobile-identified.png'), fullPage: true });
    await page.setViewportSize({ width: 320, height: 740 });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), '320 px page has no horizontal overflow');
  }, { viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  assert.deepEqual(errors, [], 'no browser JavaScript errors');
  assert(network.every(request => request.method === 'GET' && request.url.startsWith(address + '/')), 'identification sends no network uploads or external requests');
  assert(network.every(request => !/\.(?:fw|bin|uf2|zip)(?:$|\?)/i.test(request.url)), 'no firmware or source packages are fetched during identification');
  console.log(`JamBox device page: ${passed} Chromium cases passed; every outgoing MIDI packet matched the read-only allowlist. Screenshots: ${evidence}`);
} finally {
  await browser?.close(); await new Promise(resolve => server.close(resolve));
}
