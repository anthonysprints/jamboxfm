// SPDX-License-Identifier: GPL-3.0-only
// Exercise the production controller. Only these test ports simulate hardware;
// the shipped controller never imports a simulator or substitutes MIDI access.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadTrialBundle, TrialInstaller } from '../web/jam-trial-install.js';
import { logicalImage, sha256hex } from '../web/fm1pkg.js';
import { MockMIDIAccess, profile, familyFrame } from './helpers/jam_install_simulator.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const directory = process.env.JAMBOX_TRIAL_BUNDLE || path.join(root, 'build/jambox-device');
const manifestBytes = new Uint8Array(await fs.readFile(path.join(directory, 'manifest.json')));
const manifest = JSON.parse(new TextDecoder().decode(manifestBytes));
const firmware = new Uint8Array(await fs.readFile(path.join(directory, manifest.firmware.file)));
const descriptor = { schema_version: 1, kind: 'experimental-first-install', hardware_validated: false,
  transition: 'factory-v15-to-jambox', manifest_sha256: await sha256hex(manifestBytes), firmware_sha256: await sha256hex(firmware) };
const jsonBytes = object => new TextEncoder().encode(JSON.stringify(object));
function files(trial = descriptor, metadata = manifestBytes, image = firmware) {
  return new Map([['trial.json', jsonBytes(trial)], ['manifest.json', metadata], [manifest.firmware.file, image]]);
}
function localFetch(entries = files(), calls = []) {
  return async (url, options) => {
    calls.push({ url, options });
    assert.equal(options.credentials, 'omit'); assert.equal(options.redirect, 'error'); assert.equal(options.cache, 'no-store');
    const filename = new URL(url).pathname.split('/').at(-1), bytes = entries.get(filename);
    return bytes ? new Response(bytes.slice(), { status: 200 }) : new Response('missing', { status: 404 });
  };
}
async function load(options = {}) {
  return loadTrialBundle({ baseURL: 'https://example.test/jamboxfm/firmware/', fetchImpl: localFetch(), ...options });
}
const bundle = await load();
function ports(options = {}) {
  const access = new MockMIDIAccess({ source: profile('stock-v15'), ...options });
  access.expectedImage = logicalImage(firmware);
  return access;
}
function closed(access) { assert.ok(access.ports.every(port => !port.listeners.size && port.opens === port.closes)); }
async function fast(run, special = ms => ms) {
  const original = globalThis.setTimeout;
  // Keep actual deadlines intact. Only shorten the updater's intentional
  // per-packet and reboot delays, so its control flow and bytes stay real.
  globalThis.setTimeout = (fn, ms, ...args) => original(fn, special([10, 2000, 3000].includes(ms) ? 1 : ms), ...args);
  try { return await run(); } finally { globalThis.setTimeout = original; }
}

test('trial descriptor binds the exact candidate and retains unvalidated hardware/release flags', async () => {
  const calls = [], loaded = await load({ fetchImpl: localFetch(files(), calls) });
  assert.equal(loaded.firmwareSHA256, descriptor.firmware_sha256);
  assert.equal(loaded.firmwareSizeBytes, firmware.length);
  assert.equal(loaded.hardwareValidated, false);
  assert.equal(loaded.manifest.hardware_validated, false);
  assert.equal(loaded.manifest.release_enabled, false);
  assert.deepEqual(loaded.manifest.transitions.approved, []);
  assert.ok(Object.isFrozen(loaded)); assert.ok(Object.isFrozen(loaded.trial));
  assert.ok(Object.isFrozen(loaded.manifest.build.flags));
  assert.deepEqual(calls.map(call => new URL(call.url).pathname), ['/jamboxfm/firmware/trial.json',
    '/jamboxfm/firmware/manifest.json', `/jamboxfm/firmware/${manifest.firmware.file}`]);
});

test('unknown trial fields, enabled release claims, wrong transitions and corrupt bindings fail before MIDI', async () => {
  const changes = [x => x.schema_version = 2, x => x.kind = 'release', x => x.hardware_validated = true,
    x => x.transition = 'sloop-2.6-to-jambox', x => x.extra = true, x => delete x.manifest_sha256,
    x => x.manifest_sha256 = 'f'.repeat(64), x => x.firmware_sha256 = 'f'.repeat(64), x => x.firmware_sha256 = 'ABC'];
  for (const change of changes) {
    const trial = structuredClone(descriptor); change(trial);
    await assert.rejects(load({ fetchImpl: localFetch(files(trial)) }), { code: 'package', started: false });
  }
  for (const change of [x => x.release_enabled = true, x => x.hardware_validated = true, x => x.firmware.file = '../bad.fwsc']) {
    const metadata = structuredClone(manifest); change(metadata); const bytes = jsonBytes(metadata);
    await assert.rejects(load({ fetchImpl: localFetch(files({ ...descriptor, manifest_sha256: await sha256hex(bytes) }, bytes)) }));
  }
  const corrupt = firmware.slice(); corrupt[1000] ^= 1;
  await assert.rejects(load({ fetchImpl: localFetch(files(descriptor, manifestBytes, corrupt)) }), /SHA-256/);
  await assert.rejects(load({ fetchImpl: localFetch(files(descriptor, manifestBytes, firmware.subarray(1))) }), /SHA-256\/size/);
  await assert.rejects(load({ fetchImpl: localFetch(new Map([['trial.json', jsonBytes([])]])) }), /descriptor fields/);
});

test('loading rejects HTTP errors, oversized bodies, malformed metadata and cancellation', async () => {
  await assert.rejects(load({ fetchImpl: localFetch(new Map()) }), /HTTP 404/);
  await assert.rejects(load({ fetchImpl: async () => new Response('x', { headers: { 'content-length': '4097' } }) }), /size limit/);
  await assert.rejects(load({ fetchImpl: async () => new Response(new Uint8Array(4097)) }), /size limit/);
  await assert.rejects(load({ fetchImpl: async () => new Response('not JSON') }), /valid UTF-8 JSON/);
  const controller = new AbortController(); controller.abort(); let calls = 0;
  await assert.rejects(load({ signal: controller.signal, fetchImpl: async () => { calls++; throw Error('must not fetch'); } }), { code: 'aborted', started: false });
  assert.equal(calls, 0);
  const during = new AbortController();
  await assert.rejects(load({ signal: during.signal, fetchImpl: async () => {
    during.abort(); throw new DOMException('cancelled', 'AbortError');
  } }), { code: 'aborted', started: false });
});

test('inspection stays read-only and distinguishes reported stock eligibility from hardware validation', async () => {
  for (const [source, eligible] of [['stock-v15', true], ['jam', false], ['loader', false], ['FM-1_016', false], ['sloop', false]]) {
    const access = ports({ source: profile(source) }), installer = new TrialInstaller(access);
    const observed = await installer.inspect(access.pair());
    assert.equal(observed.eligible, eligible); assert.equal(observed.hardwareValidated, false);
    assert.equal(observed.identity.text, profile(source).text);
    if (eligible) assert.equal(observed.capabilities.recognized, false);
    assert.deepEqual(access.upgrades, []); assert.equal(installer.busy, false); assert.equal(installer.started, false);
    closed(access);
  }
});

test('production install transfers every candidate byte through guarded app/loader stages and verifies return', async () => fast(async () => {
  const access = ports(), steps = [], log = [], installer = new TrialInstaller(access, {
    onStep: (stage, detail) => steps.push([stage, detail]), onLog: message => log.push(message) });
  const result = await installer.install(bundle, access.pair());
  assert.deepEqual(access.upgrades, ['app', 'loader']);
  assert.deepEqual(access.transfers.map(stage => [stage.stage, stage.verifiedBytes]),
    [['app', firmware.length - 20], ['loader', firmware.length - 20]]);
  assert.equal(result.identity.text, 'FM-1_700'); assert.equal(result.hardwareValidated, false);
  assert.equal(result.experimental, true); assert.equal(result.verification, 'reported-family-and-release');
  assert.equal(result.firmwareSHA256, bundle.firmwareSHA256); assert.equal(result.transition, descriptor.transition);
  assert.equal(steps[0][0], 'checking'); assert.equal(steps.at(-1)[0], 'done');
  assert.ok(log.every(item => typeof item === 'string' && !item.includes('mock-')));
  assert.equal(installer.busy, false); assert.equal(installer.started, true); closed(access);
}));

test('forged bundles, other source identities and standalone loaders never start a write', async () => {
  const access = ports(), installer = new TrialInstaller(access);
  await assert.rejects(installer.install({ ...bundle }, access.pair()), /verify the trial bundle/);
  assert.deepEqual(access.upgrades, []); assert.equal(installer.busy, false);
  for (const source of ['FM-1_016', 'FM-1_15', 'FM-1_0015', 'jam', 'rescue', 'sloop', 'loader', 'ota-FM-1_015']) {
    const access = ports({ source: profile(source) }), installer = new TrialInstaller(access);
    await assert.rejects(installer.install(bundle, access.pair()), error => error.started === false);
    assert.deepEqual(access.upgrades, []); assert.equal(installer.started, false); assert.equal(installer.busy, false); closed(access);
  }
});

test('the source is checked freshly at installation, not trusted from the inspection result', async () => {
  const access = ports({ fault: { stage: 'app', type: 'identity-change', nextIdentity: 'FM-1_016' } });
  const installer = new TrialInstaller(access);
  assert.equal((await installer.inspect(access.pair())).eligible, true);
  await assert.rejects(installer.install(bundle, access.pair()), { code: 'identity', started: false });
  assert.deepEqual(access.upgrades, []); closed(access);
});

test('additional initial MIDI candidates, including hidden duplicate groups, refuse before staging', async () => {
  for (const duplicates of [1, 2]) {
    const access = ports(), installer = new TrialInstaller(access);
    for (let i = 0; i < duplicates; i++) {
      access.inputs.set(`other-in-${i}`, { id: `other-in-${i}`, name: 'Other FM-1', state: 'connected' });
      access.outputs.set(`other-out-${i}`, { id: `other-out-${i}`, name: 'Other FM-1', state: 'connected' });
    }
    const inspection = await installer.inspect(access.pair());
    assert.equal(inspection.identity.text, 'FM-1_015'); assert.equal(inspection.eligible, false);
    assert.equal(inspection.uniqueDevice, false);
    assert.match(inspection.reason, /only one FM-1/);
    await assert.rejects(installer.install(bundle, access.pair()), { code: 'ambiguous', started: false });
    assert.deepEqual(access.upgrades, []); assert.equal(installer.started, false); closed(access);
  }
});

test('unexpected loader identities and bad final family probes cannot produce successful installation', async () => fast(async () => {
  for (const config of [{ loader: profile('ota-FM-1_900') }, { loader: profile('ota-FM-1_015') },
    { loader: profile('loader', { family: familyFrame('loader', { release: '0.1.1' }) }) },
    { final: profile('jam', { family: familyFrame('normal', { mask: 0x97 }) }) }]) {
    const access = ports(config), steps = [], installer = new TrialInstaller(access, { onStep: stage => steps.push(stage) });
    await assert.rejects(installer.install(bundle, access.pair()), error => error.started === true);
    if (config.loader) assert.deepEqual(access.upgrades, ['app']);
    assert.ok(!steps.includes('done')); assert.equal(installer.busy, false); closed(access);
  }
}));

test('reconnect ambiguity refuses without scanning or issuing a loader upgrade', async () => fast(async () => {
  for (const duplicates of [1, 2]) {
    const access = ports(), attach = access.attach.bind(access);
    access.attach = stage => {
      attach(stage);
      if (stage === 'loader') for (let i = 0; i < duplicates; i++) {
        access.inputs.set(`unrelated-in-${i}`, { id: `unrelated-in-${i}`, name: 'Other FM-1', state: 'connected' });
        access.outputs.set(`unrelated-out-${i}`, { id: `unrelated-out-${i}`, name: 'Other FM-1', state: 'connected' });
      }
    };
    const installer = new TrialInstaller(access);
    await assert.rejects(installer.install(bundle, access.pair()), { code: 'ambiguous', started: true });
    assert.deepEqual(access.upgrades, ['app']); closed(access);
  }
}));

test('concurrent operations refuse, and cancellation before staging sends no upgrade', async () => {
  const access = ports(), installer = new TrialInstaller(access), controller = new AbortController();
  let inspected;
  const input = access.input, open = input.open.bind(input);
  input.open = async () => { await new Promise(resolve => { inspected = resolve; }); await open(); };
  const inspection = installer.inspect(access.pair(), { signal: controller.signal });
  await new Promise(resolve => setTimeout(resolve, 1));
  assert.equal(installer.busy, true);
  await assert.rejects(installer.install(bundle, access.pair()), { code: 'busy' });
  await assert.rejects(installer.inspect(access.pair()), { code: 'busy' });
  controller.abort(); inspected();
  await assert.rejects(inspection, { code: 'aborted', started: false });
  await new Promise(resolve => setTimeout(resolve, 1));
  assert.deepEqual(access.upgrades, []); assert.equal(installer.busy, false); closed(access);
  const cancelled = new AbortController(); cancelled.abort();
  await assert.rejects(installer.install(bundle, access.pair(), { signal: cancelled.signal }), { code: 'aborted', started: false });
  assert.deepEqual(access.upgrades, []);
});

test('post-start UI cancellation or callback exceptions do not abort the transfer', async () => fast(async () => {
  const access = ports(), controller = new AbortController();
  const installer = new TrialInstaller(access, { onLog() { throw Error('broken log renderer'); }, onStep(stage) {
    if (stage === 'start') controller.abort();
    throw Error('broken progress renderer');
  } });
  const result = await installer.install(bundle, access.pair(), { signal: controller.signal });
  assert.equal(result.identity.text, 'FM-1_700'); assert.deepEqual(access.upgrades, ['app', 'loader']);
  assert.equal(installer.busy, false); closed(access);
}));

test('partial transfer, failed completion and overall timeout report possible writes without retry', async () => fast(async () => {
  for (const fault of [{ stage: 'app', type: 'mid-disconnect' }, { stage: 'loader', type: 'finish-send' }]) {
    const access = ports({ fault }), installer = new TrialInstaller(access);
    await assert.rejects(installer.install(bundle, access.pair()), error => error.started === true);
    assert.ok(access.upgrades.length <= 2); assert.equal(installer.busy, false); closed(access);
  }
}));

test('overall installation deadline remains effective after external cancellation is disabled', async () => fast(async () => {
  const access = ports(), installer = new TrialInstaller(access);
  await assert.rejects(installer.install(bundle, access.pair()), { code: 'timeout', started: true });
  assert.equal(installer.started, true); assert.equal(installer.busy, false);
  assert.deepEqual(access.upgrades, ['app']); closed(access);
}, ms => ms === 600000 ? 15 : ms));
