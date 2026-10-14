// SPDX-License-Identifier: GPL-3.0-only
// Offline only: all transfer exercises require the test-only MockMIDIAccess.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import * as policy from '../web/jam-device-policy.js';
import { logicalImage, sha256hex } from '../web/fm1pkg.js';
import { frame, identity, identityFrame, familyFrame, sloopInfoFrame, profile, MockMIDIAccess, simulateInstall } from './helpers/jam_install_simulator.mjs';

async function pinnedBaseline() {
  if (process.env.JAMBOX_SLOOP_PACKAGE) return fs.readFile(process.env.JAMBOX_SLOOP_PACKAGE);
  try { return await fs.readFile('/workspace/deps/jambox-return/SLOOP-2.6.fwsc'); }
  catch (error) { if (error.code !== 'ENOENT') throw error; return fs.readFile(new URL('../docs/firmware/sloop-2.6.fwsc', import.meta.url)); }
}
const baseline = new Uint8Array(await pinnedBaseline());
function withProduct(bytes, product) {
  const copy = bytes.slice();
  for (let i = 0; i < 20; i++) copy[i * 48 + 47] = i < product.length ? product.charCodeAt(i) + i + 1 : 0x7d;
  return copy;
}
// This is a synthetic package fixture made from the local audited baseline.
// It proves format/policy behavior, and is neither a JamBox build nor released.
const fixture = withProduct(baseline, 'FM-1_700');
const digest = await sha256hex(fixture);
const manifest = {
  schema_version: 1, family: 'jamboxfm', version: '0.1.0-dev', release_enabled: false, hardware_validated: false,
  target: { model: 'M-VAVE FM-1', soc: 'AC791N', platform: 'WL82', flash_bytes: 1048576, flash_jedec: '0x856014', chip_key: '0x980f' },
  identities: { ...policy.JAM_IDENTITIES }, firmware: { file: 'jamboxfm-0.1.0-dev.fwsc', size_bytes: fixture.length, sha256: digest },
  source: { file: 'jamboxfm-source.zip', size_bytes: 4096, sha256: '0'.repeat(64), revision: '0'.repeat(40), dirty: true },
  build: { receipt_file: 'build-receipt.json', receipt_sha256: '1'.repeat(64), memory_report_file: 'memory.json', memory_report_sha256: '2'.repeat(64),
    flags: { JAMBOX_PROTO: 1, FELUCCA_FLASH: 1, FELUCCA_PERSIST: 0, FELUCCA_OTA: 1, FELUCCA_OTA_DRYRUN: 0,
      FELUCCA_OTA_RAMONLY: 0, FELUCCA_ICONS: 0, FELUCCA_SLICE: 0, FELUCCA_CDC: 0, FELUCCA_UART: 0 } },
  transitions: { mode: 'simulation-only', approved: [], simulated: [...policy.SIMULATED_TRANSITIONS] }, return_firmware: { ...policy.SLOOP_26 },
  format_validation: Object.fromEntries(['package_header_crc', 'entry_headers_crc', 'payload_crc', 'chip_key_match', 'canonical_reconstruction', 'app_loader_elf_match', 'memory_regions'].map(k => [k, true])),
  licenses: [{ file: 'LICENSE', size_bytes: 10, sha256: '3'.repeat(64) }],
};
const prepared = await policy.prepareCandidate(manifest, fixture);
const pinnedReturn = await policy.prepareReturnPackage(baseline);
const factoryBytes = new Uint8Array(await fs.readFile(process.env.JAMBOX_FACTORY_PACKAGE || '/workspace/deps/jambox-return/FM-1-V15.fwsc'));
const pinnedFactory = await policy.prepareStockReturnPackage(factoryBytes);
const copy = value => structuredClone(value);

test('manifest strictly validates candidate gates, provenance, artifacts, target and evidence', () => {
  const checked = policy.validateManifest(manifest);
  assert.ok(Object.isFrozen(checked.build.flags));
  assert.throws(() => { checked.release_enabled = true; }, TypeError);
  const sourceExport = copy(manifest); sourceExport.source.revision = sourceExport.source.dirty = null;
  assert.equal(policy.validateManifest(sourceExport).source.revision, null);
  const cases = [
    x => x.release_enabled = true, x => x.hardware_validated = true, x => x.version = '2.6', x => x.family = 'sloop',
    x => x.schema_version = 2, x => x.extra = true, x => delete x.target, x => x.target.flash_bytes = 2097152,
    x => x.target.chip_key = '0x1234', x => x.identities.rescue = 'FM-1_700', x => x.identities.loader = 'ota-FM-1_900',
    x => x.return_firmware.product = 'FM-1_926', x => x.return_firmware.sha256 = 'f'.repeat(64),
    x => x.firmware.file = '../candidate.fwsc', x => x.firmware.size_bytes = 2 ** 53, x => x.firmware.size_bytes = 959,
    x => x.firmware.sha256 = digest.toUpperCase(), x => x.source.revision = null, x => x.source.dirty = null,
    x => x.source.revision = 'x'.repeat(40), x => x.build.receipt_file = 'other.json', x => delete x.build.flags.JAMBOX_PROTO,
    x => x.build.flags.FELUCCA_PERSIST = 1, x => x.build.flags.FELUCCA_OTA_DRYRUN = 1,
    x => x.build.flags.FELUCCA_CDC = 2, x => x.build.flags.EXTRA = 0,
    x => x.transitions.mode = 'enabled', x => x.transitions.approved.push('jambox-normal-to-jambox'),
    x => x.transitions.simulated.reverse(), x => delete x.format_validation.payload_crc,
    x => x.format_validation.memory_regions = false, x => x.format_validation.future = true,
    x => x.licenses = [], x => x.licenses.push(copy(x.licenses[0])), x => x.licenses[0].file = '/LICENSE',
    x => x.licenses[0].file = 'licenses/../LICENSE', x => x.licenses[0].sha256 = 'abc',
  ];
  for (const mutate of cases) { const bad = copy(manifest); mutate(bad); assert.throws(() => policy.validateManifest(bad), { code: 'manifest' }, String(mutate)); }
});

test('preparation pins exact bytes and keeps prepared image immutable', async () => {
  const bytes = fixture.slice(), checked = await policy.prepareCandidate(manifest, bytes), first = checked.image();
  bytes.fill(0); checked.image().fill(0);
  assert.deepEqual(checked.image(), first); assert.equal(checked.installAllowed, false);
  assert.equal(checked.releaseEnabled, false); assert.equal(checked.hardwareValidated, false);
  await assert.rejects(policy.prepareCandidate(manifest, baseline), /SHA-256/);
  const corrupt = fixture.slice(); corrupt[1000] ^= 1;
  await assert.rejects(policy.prepareCandidate(manifest, corrupt), /SHA-256/);
  await assert.rejects(policy.prepareReturnPackage(fixture), /exact audited Sloop/);
  await assert.rejects(policy.prepareReturnPackage(baseline.subarray(1)), /exact audited Sloop/);
  await assert.rejects(policy.prepareCandidate(manifest, 3), /package bytes/);
  assert.throws(() => policy.simulationTransition({}, { ...prepared }), /validated prepared/);
});

const u16 = (b, p) => b[p] | b[p + 1] << 8;
const u32 = (b, p) => (b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24) >>> 0;
function set16(b, p, n) { b[p] = n; b[p + 1] = n >>> 8; }
function crc(bytes) { let n = 0; for (const b of bytes) { n ^= b << 8; for (let bit = 0; bit < 8; bit++) n = ((n << 1) ^ (n & 0x8000 ? 0x1021 : 0)) & 65535; } return n; }
function crypt(bytes, key = 65535) { const result = bytes.slice(); for (let i = 0; i < result.length; i++) { result[i] ^= key & 255; key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 65535; } return result; }
function wrap(image) {
  const result = new Uint8Array(image.length + 20);
  for (let i = 0; i < 20; i++) result.set(image.subarray(i * 47, (i + 1) * 47), i * 48);
  result.set(image.subarray(940), 960); return withProduct(result, 'FM-1_700');
}
// Recalculate outer CRCs after corrupting nested metadata. These cases ensure
// validation checks the inner format rather than stopping at the first CRC.
function mutatePayload(index, mutate) {
  const image = logicalImage(fixture), entry = crypt(image.subarray(64 + index * 80, 144 + index * 80));
  const offset = u32(entry, 8), bytes = image.subarray(offset, offset + u32(entry, 12)); mutate(bytes);
  set16(entry, 4, crc(bytes)); image.set(crypt(entry), 64 + index * 80);
  const header = crypt(image.subarray(0, 64)); set16(header, 2, crc(image.subarray(64, 224))); set16(header, 0, crc(header.subarray(2)));
  image.set(crypt(header)); return wrap(image);
}
function changeDirectory(bytes, offset, mutate) {
  const entry = crypt(bytes.subarray(offset, offset + 32)); mutate(entry); set16(entry, 0, crc(entry.subarray(2))); bytes.set(crypt(entry), offset);
}
test('full package validator rejects bounds, metadata and nested payload corruption even with valid outer CRCs', () => {
  assert.equal(policy.validatePackage(baseline, 'FM-1_900').chipKey, 0x980f);
  const cases = [fixture.subarray(0, 900), withProduct(fixture, 'FM-1_900'),
    mutatePayload(0, b => b[0] ^= 1),
    mutatePayload(0, b => changeDirectory(b, 128, e => e[5] = 0xee)), // protected key offset
    mutatePayload(0, b => changeDirectory(b, 32, e => e[12] = 0x82)), // SPL flags
    mutatePayload(0, b => changeDirectory(b, 64, e => e[8] = 0xff)), // chip-key bounds
    mutatePayload(0, b => b[0x4000 + 400] ^= 1), // encrypted application payload
    mutatePayload(0, b => b[0x4000 + 96] ^= 1), // encrypted region header
    mutatePayload(1, b => b[0] ^= 1), mutatePayload(1, b => b[64] ^= 1),
    mutatePayload(1, b => { b[32 + 8] ^= 1; set16(b, 32, crc(b.subarray(34, 64))); set16(b, 2, crc(b.subarray(32))); set16(b, 0, crc(b.subarray(2, 32))); }),
  ];
  for (const bad of cases) assert.throws(() => policy.validatePackage(bad, 'FM-1_700'), { code: 'package' });
  for (const offset of [0, 47, 70, 230, 1000, fixture.length - 1]) {
    const bad = fixture.slice(); bad[offset] ^= 1; assert.throws(() => policy.validatePackage(bad, 'FM-1_700'), { code: 'package' });
  }
  const image = logicalImage(fixture); image[224] = 0; assert.throws(() => policy.validatePackage(wrap(image), 'FM-1_700'), /padding/);
});

test('family probe requires exact framing, fields and matching identity', () => {
  for (const [mode, text, mask] of [['normal', 'FM-1_700', 0x17], ['rescue', 'FM-1_000', 5], ['loader', 'ota-FM-1_700', 9]]) {
    const valid = familyFrame(mode), familyProbe = policy.parseFamilyProbe(valid);
    const checked = policy.classifyDevice({ identity: identity(text), familyProbe });
    assert.equal(checked.recognized, true); assert.equal(checked.mode, mode); assert.equal(familyProbe.mask, mask);
    for (const changes of [{ family: 'OTHERFAM' }, { schema: 2 }, { mode: 3 }, { release: '0.1.1' }, { mask: mask | 0x8000 }, { identity: 'FM-1_900' }])
      assert.equal(policy.classifyDevice({ identity: identity(text), familyProbe: policy.parseFamilyProbe(familyFrame(mode, changes)) }).recognized, false);
    for (const bad of [valid.slice(1), valid.slice(0, -1), [...valid, 0], frame(0x11, Array(48).fill(0))]) assert.equal(policy.parseFamilyProbe(bad), null);
    for (let i = 1; i < valid.length - 1; i++) { const bad = [...valid]; bad[i] |= 128; assert.equal(policy.parseFamilyProbe(bad), null); }
    const checksum = [...valid]; checksum[10] ^= 1; assert.equal(policy.parseFamilyProbe(checksum), null);
    assert.equal(policy.classifyDevice({ identity: { ...identity(text), version: 999 }, familyProbe }).recognized, false);
  }
});

test('Sloop identification requires exact audited INFO and rejects numeric-only identities', () => {
  const info = policy.parseSloopInfo(sloopInfoFrame());
  assert.equal(policy.classifyDevice({ identity: identity('FM-1_900'), info }).family, 'sloop');
  for (const text of ['FM-1_926', 'FM-1_015', 'ota-FM-1_900', 'OTHER_900', 'FM-1_700'])
    assert.equal(policy.classifyDevice({ identity: identity(text), info }).recognized, false);
  for (const changes of [{ name: 'FELUCCA SLOOP 2.6 (MOCK)' }, { parameters: 94 }, { globals: 28 }, { engineParameterStart: 86 }, { protocol: 9 }, { names: ['FM6'] }])
    assert.equal(policy.classifyDevice({ identity: identity('FM-1_900'), info: policy.parseSloopInfo(sloopInfoFrame(changes)) }).recognized, false);
  const good = sloopInfoFrame();
  for (const bad of [good.slice(1), good.slice(0, -1), [...good, 0], [...good.slice(0, -1), 0, 247], good.map((b, i) => i === 5 ? 10 : b)])
    assert.equal(policy.parseSloopInfo(bad), null);
});

test('inventory sends only identity-specific read-only query shapes and handles loss/abort', async () => {
  for (const [name, command] of [['jam', frame(0x12, [])], ['rescue', frame(0x12, [])], ['loader', frame(0x12, [])],
    ['sloop', [240, 125, 70, 76, 1, 247]], ['FM-1_015', null], ['ota-FM-1_900', null]]) {
    const p = profile(name), sent = [], packets = p.family ? [p.family] : p.info ? [p.info] : [];
    const link = { drain() {}, send(bytes) { sent.push(bytes); return true; }, async read() { return packets.shift(); } };
    const result = await policy.inspectDevice(identity(p.text), link, { timeoutMs: 1 });
    assert.deepEqual(sent, command ? [command] : []); assert.equal(result.installAllowed, false);
  }
  const lost = { drain() {}, send() { return true; }, async read() { this.lost = true; return null; } };
  await assert.rejects(policy.inspectDevice(identity('FM-1_700'), lost, { timeoutMs: 1 }), { code: 'lost' });
  const c = new AbortController(); c.abort(new Error('test cancelled'));
  await assert.rejects(policy.inspectDevice(identity('FM-1_700'), lost, { signal: c.signal }), /cancelled/);
  assert.ok(!Object.keys(policy).some(name => /install|transfer|write/i.test(name)), 'Public policy must expose no transfer orchestration');
});

async function fastTimers(run) {
  const original = globalThis.setTimeout;
  globalThis.setTimeout = (fn, ms, ...args) => original(fn, Math.min(ms, 1), ...args);
  try { return await run(); } finally { globalThis.setTimeout = original; }
}
test('all six existing simulated transitions use actual updater and explicitly selected mock pairs', async () => fastTimers(async () => {
  const cases = [['jam', prepared], ['rescue', prepared], ['loader', prepared], ['sloop', prepared], ['jam', pinnedReturn], ['rescue', pinnedReturn]];
  const completed = [];
  for (const [source, candidate] of cases) {
    const access = new MockMIDIAccess({ source: profile(source), loader: profile(candidate.family === 'sloop' ? 'ota-FM-1_900' : 'loader'), final: profile(candidate.family === 'sloop' ? 'sloop' : 'jam') });
    const result = await simulateInstall({ access, candidate, pair: access.pair() }); completed.push(result.transition);
    assert.equal(result.hardwareValidated, false); assert.equal(result.installAllowed, false);
    assert.deepEqual(result.guards, source === 'loader' ? ['loader', 'final'] : ['app', 'loader', 'final']);
    assert.deepEqual(access.upgrades, source === 'loader' ? ['loader'] : ['app', 'loader']);
    assert.equal(result.steps.at(-1)[0], 'done'); assert.equal(result.selected.at(-1).stage, 'final');
    assert.ok(access.ports.every(port => port.listeners.size === 0 && port.opens === port.closes));
    assert.equal(result.coverage.length, source === 'loader' ? 1 : 2);
    assert.ok(result.coverage.every(stage => stage.verifiedBytes === candidate.image().length));
    assert.equal(access.responses.reduce((sum, item) => sum + item.length, 0), candidate.image().length * result.coverage.length);
  }
  assert.deepEqual(completed, policy.SIMULATED_TRANSITIONS);
}));

test('unknown families, stock, standalone foreign loader and unsupported return paths refuse before any upgrade', async () => {
  await assert.rejects(simulateInstall({ access: { inputs: new Map(), outputs: new Map() }, candidate: prepared, pair: { inputId: 'x', outputId: 'y' } }), /MockMIDIAccess/);
  for (const [source, candidate] of [[profile('FM-1_015'), prepared], [profile('FM-1_926'), prepared], [profile('FM-1_900'), prepared],
    [profile('ota-FM-1_900'), prepared], [profile('jam', { family: null }), prepared], [profile('loader'), pinnedReturn], [profile('sloop'), pinnedReturn]]) {
    const access = new MockMIDIAccess({ source });
    await assert.rejects(simulateInstall({ access, candidate, pair: access.pair() }), /outside the simulated policy/);
    assert.deepEqual(access.upgrades, []);
  }
});

test('stage faults, cancellation, identity mismatches and failed final acknowledgement cannot produce done', async () => fastTimers(async () => {
  for (const stage of ['app', 'loader']) for (const type of ['disconnect', 'bad-request', 'finish-send', 'abort']) {
    const controller = new AbortController(), access = new MockMIDIAccess({ fault: { stage, type, controller } });
    await assert.rejects(simulateInstall({ access, candidate: prepared, pair: access.pair(), signal: controller.signal }));
    assert.ok(access.ports.every(port => port.listeners.size === 0), `${stage}/${type} leaked listeners`);
    assert.ok(access.stage !== 'final');
  }
  for (const stage of ['app', 'loader']) {
    const realNow = Date.now; let clock = realNow();
    const access = new MockMIDIAccess({ fault: { stage, type: 'silence', onSilence() { Date.now = () => clock += 200000; } } });
    try { await assert.rejects(simulateInstall({ access, candidate: prepared, pair: access.pair() }), { code: 'stopped' }); }
    finally { Date.now = realNow; }
    assert.ok(access.ports.every(port => port.listeners.size === 0));
    const missing = new MockMIDIAccess({ fault: { stage, type: 'missing-after-finish' } });
    await assert.rejects(simulateInstall({ access: missing, candidate: prepared, pair: missing.pair() }), /expected mock pair/);
  }
  for (const config of [{ loader: profile('ota-FM-1_900') }, { loader: profile('loader', { family: familyFrame('loader', { mask: 0x89 }) }) },
    { final: profile('FM-1_015') }, { final: profile('jam', { family: familyFrame('normal', { release: '0.1.1' }) }) }]) {
    const access = new MockMIDIAccess(config);
    await assert.rejects(simulateInstall({ access, candidate: prepared, pair: access.pair() }));
    if (config.loader) assert.deepEqual(access.upgrades, ['app']);
    assert.ok(access.ports.every(port => port.listeners.size === 0));
  }
  const access = new MockMIDIAccess({ loader: profile('ota-FM-1_901'), final: profile('sloop') });
  await assert.rejects(simulateInstall({ access, candidate: pinnedReturn, pair: access.pair() }));
  assert.deepEqual(access.upgrades, ['app']);
  const changed = new MockMIDIAccess({ fault: { stage: 'app', type: 'identity-change' } });
  await assert.rejects(simulateInstall({ access: changed, candidate: prepared, pair: changed.pair() }));
  assert.deepEqual(changed.upgrades, [], 'A changed identity must be rejected before stage one');
  const controller = new AbortController(), cancelled = new MockMIDIAccess({ fault: { stage: 'final', type: 'abort-identify', controller } });
  await assert.rejects(simulateInstall({ access: cancelled, candidate: prepared, pair: cancelled.pair(), signal: controller.signal }), { code: 'aborted' });
  assert.ok(cancelled.ports.every(port => port.listeners.size === 0));
  const wrongReturn = new MockMIDIAccess({ loader: profile('ota-FM-1_900'), final: profile('sloop', { info: sloopInfoFrame({ protocol: 9 }) }) });
  await assert.rejects(simulateInstall({ access: wrongReturn, candidate: pinnedReturn, pair: wrongReturn.pair() }));
}));

test('factory return preparation pins the complete uploaded V15 file and preserves existing manifest policy', async () => {
  assert.equal(factoryBytes.length, policy.FACTORY_V15.size_bytes);
  assert.equal(await sha256hex(factoryBytes), policy.FACTORY_V15.sha256);
  assert.equal(pinnedFactory.product, 'FM-1_015');
  assert.equal(pinnedFactory.family, 'factory');
  assert.equal(pinnedFactory.version, '15');
  assert.equal(pinnedFactory.installAllowed, false);
  assert.equal(pinnedFactory.releaseEnabled, false);
  assert.equal(pinnedFactory.hardwareValidated, false);
  const original = pinnedFactory.image(), external = factoryBytes.slice();
  const independent = await policy.prepareStockReturnPackage(external);
  external.fill(0); independent.image().fill(0);
  assert.deepEqual(independent.image(), original);
  for (const bytes of [factoryBytes.subarray(1), new Uint8Array([...factoryBytes, 0]), fixture, baseline,
    withProduct(factoryBytes, 'FM-1_700'), withProduct(factoryBytes, 'FM-1_016')])
    await assert.rejects(policy.prepareStockReturnPackage(bytes), /unmodified official FM-1 V15/);
  for (const position of [0, 47, 1000, factoryBytes.length - 1]) {
    const bytes = factoryBytes.slice(); bytes[position] ^= 1;
    await assert.rejects(policy.prepareStockReturnPackage(bytes), /unmodified official FM-1 V15/);
  }
  await assert.rejects(policy.prepareStockReturnPackage('FM-1.fwsc'), /package bytes/);
  assert.throws(() => policy.simulationTransition({}, { ...pinnedFactory }), /validated prepared/);
  const stock = policy.classifyDevice({ identity: identity('FM-1_015') });
  assert.equal(stock.recognized, false);
  assert.equal(stock.family, 'unknown');
  assert.ok(Object.values(stock.capabilities).every(value => value === false));
  assert.equal(policy.simulationTransition(stock, prepared).allowed, false);
  assert.equal(policy.simulationTransition(policy.classifyDevice({ identity: identity('FM-1_700'),
    familyProbe: policy.parseFamilyProbe(familyFrame()) }), pinnedFactory).allowed, false);
  assert.equal(policy.validateManifest(manifest).transitions.simulated.length, 6);
});

test('all three factory transitions exercise every package byte only under an explicit mock factory assumption', async () => fastTimers(async () => {
  const cases = [['stock-v15', prepared], ['jam', pinnedFactory], ['rescue', pinnedFactory]], completed = [];
  for (const [source, candidate] of cases) {
    const returning = candidate === pinnedFactory;
    const access = new MockMIDIAccess({ source: profile(source), loader: profile(returning ? 'ota-FM-1_015' : 'loader'),
      final: profile(returning ? 'stock-v15' : 'jam') });
    const result = await simulateInstall({ access, candidate, pair: access.pair(), factoryBaseline: pinnedFactory });
    completed.push(result.transition);
    assert.equal(result.installAllowed, false); assert.equal(result.hardwareValidated, false);
    assert.equal(result.deviceExecutionValidated, false); assert.equal(result.hostSimulationOnly, true);
    assert.equal(result.factoryBaselineAssumed, true);
    assert.deepEqual(result.guards, ['app', 'loader', 'final']);
    assert.deepEqual(access.upgrades, ['app', 'loader']);
    assert.equal(result.steps.at(-1)[0], 'done');
    assert.deepEqual(result.coverage.map(stage => [stage.stage, stage.verifiedBytes]),
      [['app', candidate.image().length], ['loader', candidate.image().length]]);
    for (const stage of ['app', 'loader']) {
      const responses = access.responses.filter(item => item.stage === stage);
      assert.ok(responses.some(item => item.length === 512));
      assert.ok(responses.some(item => item.length === 481));
      assert.equal(responses.reduce((sum, item) => sum + item.length, 0), candidate.image().length);
    }
    const factoryStages = returning ? ['loader', 'final'] : ['app'];
    assert.ok(access.transcript.filter(item => factoryStages.includes(item.stage)).every(item =>
      String(item.packet) !== String(frame(0x12, [])) && String(item.packet) !== '240,125,70,76,1,247'),
    'Factory identities must not receive unsupported JamBox/Sloop probes');
    assert.ok(access.ports.every(port => port.listeners.size === 0 && port.opens === port.closes));
  }
  assert.deepEqual(completed, policy.FACTORY_SIMULATED_TRANSITIONS);
}));

test('factory simulation rejects absent or forged baseline evidence, other identities and standalone factory loaders', async () => {
  for (const [source, candidate, factoryBaseline] of [
    ['stock-v15', prepared, null], ['stock-v15', prepared, pinnedReturn], ['stock-v15', prepared, { ...pinnedFactory }],
    ['FM-1_016', prepared, pinnedFactory], ['FM-1_15', prepared, pinnedFactory], ['OTHER_015', prepared, pinnedFactory],
    ['FM-1_900', prepared, pinnedFactory], ['ota-FM-1_015', prepared, pinnedFactory],
    ['ota-FM-1_015', pinnedFactory, pinnedFactory], ['loader', pinnedFactory, pinnedFactory],
    ['sloop', pinnedFactory, pinnedFactory], ['stock-v15', pinnedFactory, pinnedFactory],
  ]) {
    const access = new MockMIDIAccess({ source: profile(source) });
    await assert.rejects(simulateInstall({ access, candidate, pair: access.pair(), factoryBaseline }),
      /outside the simulated policy|validated prepared/);
    assert.deepEqual(access.upgrades, []);
    assert.ok(access.ports.every(port => port.listeners.size === 0 && port.opens === port.closes));
  }
});

test('factory stages reject changed identity, unexpected loader/final firmware, partial transfers and cancellation', async () => fastTimers(async () => {
  for (const config of [
    { source: profile('stock-v15'), candidate: prepared, loader: profile('ota-FM-1_015') },
    { source: profile('stock-v15'), candidate: prepared, final: profile('stock-v15') },
    { candidate: pinnedFactory, loader: profile('ota-FM-1_900') },
    { candidate: pinnedFactory, loader: profile('loader') },
    { candidate: pinnedFactory, loader: profile('ota-FM-1_016') },
    { candidate: pinnedFactory, loader: profile('ota-FM-1_015'), final: profile('FM-1_016') },
    { candidate: pinnedFactory, loader: profile('ota-FM-1_015'), final: profile('jam') },
  ]) {
    const { candidate, ...ports } = config, access = new MockMIDIAccess(ports);
    await assert.rejects(simulateInstall({ access, candidate, pair: access.pair(), factoryBaseline: pinnedFactory }));
    if (!config.final) assert.deepEqual(access.upgrades, ['app']);
    assert.ok(access.ports.every(port => port.listeners.size === 0));
  }
  for (const stage of ['app', 'loader']) for (const type of ['mid-disconnect', 'bad-request', 'finish-send', 'abort']) {
    const controller = new AbortController();
    const access = new MockMIDIAccess({ loader: profile('ota-FM-1_015'), final: profile('stock-v15'), fault: { stage, type, controller } });
    await assert.rejects(simulateInstall({ access, candidate: pinnedFactory, pair: access.pair(), signal: controller.signal }));
    assert.notEqual(access.stage, 'final');
    assert.ok(access.ports.every(port => port.listeners.size === 0));
  }
  const changed = new MockMIDIAccess({ fault: { stage: 'app', type: 'identity-change' } });
  await assert.rejects(simulateInstall({ access: changed, candidate: pinnedFactory, pair: changed.pair() }));
  assert.deepEqual(changed.upgrades, [], 'JamBox identity must remain verified before stock return stage one');
  const factoryChanged = new MockMIDIAccess({ source: profile('stock-v15'), fault: { stage: 'app', type: 'identity-change', nextIdentity: 'FM-1_016' } });
  await assert.rejects(simulateInstall({ access: factoryChanged, candidate: prepared, pair: factoryChanged.pair(), factoryBaseline: pinnedFactory }));
  assert.deepEqual(factoryChanged.upgrades, [], 'Reported factory identity must match the explicit test assumption at stage one');
}));
