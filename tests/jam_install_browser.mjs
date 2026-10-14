// SPDX-License-Identifier: GPL-3.0-only
// Real Chromium exercises the production page/controller/transport with fake
// Web MIDI. Device code does not execute; this is host and UI verification.
// . tools/cloud/activate.sh && node tests/jam_install_browser.mjs
// JAM_INSTALL_SITE_ROOT=build/jambox-web also tests the packaged site.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { prepareCandidate } from '../web/jam-device-policy.js';

const playwright = process.env.PLAYWRIGHT_MODULE ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve(process.env.JAM_INSTALL_SITE_ROOT || 'web');
const artifacts = resolve(process.env.JAM_INSTALL_SITE_ROOT ? `${root}/firmware` : process.env.JAM_INSTALL_CANDIDATE || 'build/jambox-device');
const manifestBytes = await readFile(resolve(artifacts, 'manifest.json'));
const manifest = JSON.parse(manifestBytes);
const firmware = await readFile(resolve(artifacts, manifest.firmware.file));
const logicalImage = Buffer.from((await prepareCandidate(manifest, firmware)).image());
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
// The unbundled source-page fixture binds its real candidate exactly like the
// publisher. Packaged-site runs must use the actual published trial descriptor.
const trial = Buffer.from(JSON.stringify({ schema_version: 2, kind: 'experimental-install', hardware_validated: false,
  transitions: ['factory-v15-to-jambox', 'jambox-normal-to-jambox'], manifest_sha256: digest(manifestBytes), firmware_sha256: digest(firmware) }));
const evidence = resolve('build/jam-install-evidence');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    let path, body;
    if (!process.env.JAM_INSTALL_SITE_ROOT && pathname.startsWith('/firmware/')) {
      path = resolve(artifacts, '.' + pathname.slice('/firmware'.length));
      if (!path.startsWith(artifacts + sep)) throw new Error('Outside artifacts');
      body = pathname === '/firmware/trial.json' ? trial : await readFile(path);
    } else {
      path = resolve(root, '.' + pathname + (pathname.endsWith('/') ? 'index.html' : ''));
      if (!path.startsWith(root + sep)) throw new Error('Outside site');
      body = await readFile(path);
    }
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' }); response.end(body);
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const address = `http://127.0.0.1:${server.address().port}`;

function fakeWebMIDI({ config, image }) {
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
  const state = globalThis.fakeMIDI = { permissions: [], upgrades: [], errors: [], sent: 0, devices: [], transfers: [], denied: !!config.denied };
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
    return device.stage === 'app' ? device.identity || config.identity || 'FM-1_015' :
      device.stage === 'loader' ? config.loaderIdentity || 'ota-FM-1_700' : config.finalIdentity || 'FM-1_700';
  }
  function transmit(device, packet) {
    state.sent++;
    if (equal(packet, [240, 0, 50, 69, 0, 0, 0, 64, 127, 247])) {
      device.handshakes++;
      if (config.changedIdentity && device.stage === 'app' && device.handshakes > 1) device.identity = config.changedIdentity;
      if (!device.silent) reply(device, frame(0x11, pad(identity(device), 27)), device.identityDelay || 0);
      return;
    }
    if (equal(packet, frame(0x12, []))) {
      const stage = device.stage === 'loader' ? 2 : 0;
      if (device.stage === 'app' && config.missingAppProbe) return;
      const badApp = device.stage === 'app' && (config.badAppFamily || config.changedAppFamily && device.handshakes > 1);
      const family = (config.badLoaderFamily && stage === 2 || badApp) ? 'FOREIGN!' : 'JAMBOXFM';
      const release = config.badFinalRelease && device.stage === 'final' ? '0.0.0' : '0.1.0-dev';
      reply(device, frame(0x12, [...text(family), 1, stage, stage === 2 ? 9 : 23, 0, ...pad(release, 20), ...pad(identity(device), 16)])); return;
    }
    if (equal(packet, [240, 125, 70, 76, 1, 247])) return;
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

let browser, passed = 0;
const errors = [], network = [];
async function runCase(name, config, body, options = {}) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 1080 }, ...options });
  page.setDefaultTimeout(20000);
  page.on('pageerror', error => errors.push(`${name}: ${error.message}`));
  page.on('request', request => network.push({ url: request.url(), method: request.method() }));
  try {
    await page.addInitScript(fakeWebMIDI, { config, image: logicalImage.toString('base64') });
    if (config.badBundle) await page.route('**/firmware/trial.json', route => route.fulfill({ status: 200, body: '{}' }));
    await page.goto(`${address}/install/`);
    await page.waitForFunction(() => !document.getElementById('firmware-label').textContent.includes('Checking'));
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().permissions), [], 'permission requires a user click');
    assert(await page.locator('#install-device').isDisabled());
    await body(page);
    const snapshot = await page.evaluate(() => fakeMIDI.snapshot());
    assert.deepEqual(snapshot.errors, [], 'fake device checked every transmitted firmware byte');
    assert.equal(await page.locator('input[type=file]').count(), 0, 'no private recovery files are uploaded');
    passed++; console.log(`PASS ${name}`);
  } finally { await page.close(); }
}
async function connect(page) {
  await page.locator('#connect-device').click();
  await page.waitForFunction(() => !document.getElementById('connect-device').disabled);
  assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().upgrades), [], 'connection and inspection never start an update');
}
async function ready(page) {
  await connect(page);
  await page.waitForFunction(() => !document.getElementById('install-device').disabled);
}
async function result(page) {
  await page.locator('#result-card').waitFor({ state: 'visible', timeout: 40000 });
  return JSON.parse(await page.locator('#report-details').textContent());
}
async function noOpenLinks(page) {
  await page.waitForFunction(() => fakeMIDI.snapshot().open === 0 && fakeMIDI.snapshot().listeners === 0);
}

try {
  await mkdir(evidence, { recursive: true });
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox'] });
  await runCase('unsupported browser', { unsupported: true }, async page => {
    assert.match(await page.locator('#status').textContent(), /doesn’t support Web MIDI/);
    assert(await page.locator('#connect-device').isDisabled());
  });
  await runCase('permission denial and explicit retry', { denied: true }, async page => {
    await connect(page); assert.match(await page.locator('#status').textContent(), /wasn’t allowed/);
    assert.equal((await page.evaluate(() => fakeMIDI.snapshot())).sent, 0);
    await page.evaluate(() => { fakeMIDI.denied = false; }); await ready(page);
    assert.equal((await page.evaluate(() => fakeMIDI.snapshot())).permissions.length, 2);
    await noOpenLinks(page);
  });
  await runCase('no ports and hotplug', { count: 0 }, async page => {
    await connect(page); assert.match(await page.locator('#status').textContent(), /No matching FM-1/);
    await page.evaluate(() => fakeMIDI.add());
    assert(await page.locator('#install-device').isDisabled(), 'hotplug requires another check');
    await ready(page); await noOpenLinks(page);
  });
  await runCase('multiple pairs allow inspection but block installation', { count: 2 }, async page => {
    await connect(page); assert(await page.locator('#pair-field').isVisible());
    assert(await page.locator('#install-device').isDisabled());
    assert.equal((await page.evaluate(() => fakeMIDI.snapshot())).sent, 0);
    await page.locator('#midi-pair').selectOption({ index: 2 }); await connect(page);
    assert(await page.locator('#install-device').isDisabled());
    assert.match(await page.locator('#status').textContent(), /only one FM-1/);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []);
    await page.locator('#midi-pair').selectOption({ index: 1 });
    assert(await page.locator('#install-device').isDisabled(), 'selection change invalidates inspection');
    await page.evaluate(() => fakeMIDI.disconnect(1)); await ready(page); await noOpenLinks(page);
  });
  await runCase('new second device invalidates readiness', {}, async page => {
    await ready(page); await page.evaluate(() => fakeMIDI.add());
    assert(await page.locator('#install-device').isDisabled());
    assert.match(await page.locator('#status').textContent(), /only one FM-1/);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []);
  });
  await runCase('hidden duplicate port groups also block installation', { count: 3, hiddenDuplicates: true }, async page => {
    await connect(page); assert(await page.locator('#install-device').isDisabled());
    assert.match(await page.locator('#status').textContent(), /only one FM-1/);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []);
    await noOpenLinks(page);
  });
  await runCase('late MIDI connection events do not invalidate a checked device', { lateCloseEvents: true }, async page => {
    await ready(page); await page.waitForTimeout(100);
    assert(await page.locator('#install-device').isEnabled());
    assert.match(await page.locator('#device-label').textContent(), /ready to install/);
    await noOpenLinks(page);
  });
  await runCase('unapproved starting firmware', { identity: 'FM-1_014' }, async page => {
    await connect(page); assert(await page.locator('#install-device').isDisabled());
    assert.match(await page.locator('#device-label').textContent(), /requires factory V15/); await noOpenLinks(page);
  });
  for (const fault of ['badAppFamily', 'missingAppProbe']) await runCase(`JamBox identity with ${fault} refuses update`,
    { identity: 'FM-1_700', [fault]: true }, async page => {
      await connect(page); assert(await page.locator('#install-device').isDisabled());
      assert.match(await page.locator('#device-label').textContent(), /requires factory V15 or supported JamBox/);
      assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
    });
  await runCase('fresh JamBox family check rejects changed capabilities before update',
    { identity: 'FM-1_700', changedAppFamily: true }, async page => {
      await ready(page); assert.match(await page.locator('#install-device').textContent(), /^Update/);
      await page.locator('#install-device').click();
      const report = await result(page); assert.equal(report.updateStarted, false); assert.equal(report.outcome, 'error');
      assert.equal(report.transition, 'jambox-normal-to-jambox');
      assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
    });
  await runCase('damaged trial descriptor blocks install', { badBundle: true }, async page => {
    await connect(page); assert(await page.locator('#install-device').isDisabled());
    assert(await page.locator('#reload-firmware').isVisible());
    assert.match(await page.locator('#firmware-label').textContent(), /could not be verified/);
  });
  await runCase('disconnect clears readiness', {}, async page => {
    await ready(page); await page.evaluate(() => fakeMIDI.disconnect(0));
    assert(await page.locator('#install-device').isDisabled());
    assert.match(await page.locator('#status').textContent(), /disconnected/);
  });
  await runCase('fresh identity check rejects a changed device', { changedIdentity: 'FM-1_014' }, async page => {
    await ready(page); await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.updateStarted, false); assert.equal(report.outcome, 'error');
    assert.equal(report.unexpectedIdentity, 'FM-1_014');
    assert.deepEqual(report.requestCounts, { verify: 0, write: 0 });
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
  });
  await runCase('cancel during preparation sends no upgrade', {}, async page => {
    await ready(page); await page.evaluate(() => { fakeMIDI.devices[0].identityDelay = 250; });
    await page.locator('#install-device').click(); await page.locator('#cancel-prepare').click();
    const report = await result(page); assert.equal(report.updateStarted, false); assert.equal(report.outcome, 'cancelled');
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
  });
  await runCase('started failure locks retry and warns before leaving', { disconnectAtStart: true }, async page => {
    await ready(page); await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.updateStarted, true); assert.equal(report.outcome, 'error');
    assert(await page.locator('#cancel-prepare').isHidden()); assert(await page.locator('#install-device').isDisabled());
    assert(await page.locator('#connect-device').isDisabled());
    assert.match(await page.locator('#result-guidance').textContent(), /Keep the FM-1 powered and connected/);
    assert(await page.evaluate(() => { const event = new Event('beforeunload', { cancelable: true }); window.dispatchEvent(event); return event.defaultPrevented; }));
    await noOpenLinks(page);
  });
  await runCase('failed report uses the freshly checked eligible source', { changedIdentity: 'FM-1_700', disconnectAtStart: true }, async page => {
    await ready(page); assert.match(await page.locator('#install-device').textContent(), /^Install/);
    await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'error'); assert.equal(report.updateStarted, true);
    assert.equal(report.sourceIdentity, 'FM-1_700'); assert.equal(report.transition, 'jambox-normal-to-jambox');
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, ['app']); await noOpenLinks(page);
  });
  await runCase('foreign loader is rejected before its upgrade command', { badLoaderFamily: true }, async page => {
    await ready(page); await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'error'); assert.equal(report.updateStarted, true);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, ['app']); await noOpenLinks(page);
  });
  await runCase('write interruption never reports success', { disconnectDuringWrite: true }, async page => {
    await ready(page); await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'error'); assert.equal(report.finalIdentity, null);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, ['app', 'loader']); await noOpenLinks(page);
  });
  await runCase('final release must be confirmed', { badFinalRelease: true }, async page => {
    await ready(page); await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'error'); assert.equal(report.finalIdentity, null);
    assert(!report.stages.some(entry => entry.stage === 'done')); await noOpenLinks(page);
  });
  await runCase('complete transfer, identity, local private report, and desktop layout', {}, async page => {
    await ready(page); await page.screenshot({ path: resolve(evidence, 'desktop-ready.png'), fullPage: true });
    await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'completed'); assert.equal(report.finalIdentity, 'FM-1_700');
    assert.equal(report.hardwareValidated, false); assert.equal(report.firmwareSHA256, digest(firmware));
    assert.equal(report.transition, 'factory-v15-to-jambox'); assert.equal(report.sourceIdentity, 'FM-1_015');
    assert(report.stages.some(entry => entry.stage === 'done'));
    const snapshot = await page.evaluate(() => fakeMIDI.snapshot());
    assert.deepEqual(snapshot.upgrades, ['app', 'loader']);
    assert.deepEqual(snapshot.transfers.map(t => t.bytes), [logicalImage.length, logicalImage.length]);
    assert.deepEqual(report.requestCounts, { verify: snapshot.transfers[0].chunks, write: snapshot.transfers[1].chunks });
    const downloadEvent = page.waitForEvent('download'); await page.locator('#save-report').click();
    const download = await downloadEvent; assert.equal(download.suggestedFilename(), 'jamboxfm-install-report.json');
    const exportedText = await readFile(await download.path(), 'utf8'); assert.deepEqual(JSON.parse(exportedText), report);
    assert(!/PRIVATE|inputId|outputId|portName|serial|backup/i.test(exportedText));
    assert(!await page.evaluate(() => { const event = new Event('beforeunload', { cancelable: true }); window.dispatchEvent(event); return event.defaultPrevented; }));
    assert.equal(await page.evaluate(() => document.activeElement.id), 'save-report');
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    await page.screenshot({ path: resolve(evidence, 'desktop-complete.png'), fullPage: true }); await noOpenLinks(page);
  });
  await runCase('mobile layout and accessible actions', {}, async page => {
    await ready(page); assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    assert.equal(await page.locator('#status').getAttribute('role'), 'status');
    assert.equal(await page.locator('#status').getAttribute('aria-live'), 'polite');
    assert.equal(await page.locator('a[href="../firmware/jamboxfm-source.zip"]').count(), 1);
    assert.equal(await page.locator('a[href="../LICENSES.html"]').count(), 1);
    await page.screenshot({ path: resolve(evidence, 'mobile-ready.png'), fullPage: true });
    await page.setViewportSize({ width: 320, height: 740 });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
  }, { viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  await runCase('installed JamBox updates through both stages and reports its actual transition', { identity: 'FM-1_700' }, async page => {
    await ready(page); assert.match(await page.locator('#device-label').textContent(), /ready to update/);
    assert.match(await page.locator('#install-device').textContent(), /^Update experimental JamBoxFM/);
    await page.locator('#install-device').click();
    const report = await result(page); assert.equal(report.outcome, 'completed');
    assert.equal(report.transition, 'jambox-normal-to-jambox');
    assert.equal(report.sourceIdentity, 'FM-1_700'); assert.equal(report.finalIdentity, 'FM-1_700');
    assert.equal(report.hardwareValidated, false); assert.equal(report.firmwareSHA256, digest(firmware));
    assert.match(await page.locator('#result-title').textContent(), /JamBox is updated/);
    const snapshot = await page.evaluate(() => fakeMIDI.snapshot());
    assert.deepEqual(snapshot.upgrades, ['app', 'loader']);
    assert.deepEqual(snapshot.transfers.map(t => t.bytes), [logicalImage.length, logicalImage.length]);
    await noOpenLinks(page);
  });
  assert.deepEqual(errors, [], 'no browser JavaScript errors');
  assert(network.every(request => request.method === 'GET' && request.url.startsWith(address + '/')), 'no uploads or external requests');
  console.log(`JamBox installer: ${passed} Chromium cases passed. These verify browser behavior and host transfers, not physical flashing. Screenshots: ${evidence}`);
} finally { await browser?.close(); await new Promise(resolve => server.close(resolve)); }
