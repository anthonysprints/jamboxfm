// SPDX-License-Identifier: GPL-3.0-only
// Verify a real generated bundle and exercise its bytes against mock MIDI only.
// Usage: node tests/jam_device_candidate_check.mjs [bundle-directory] [Sloop-2.6.fwsc]
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { prepareCandidate, prepareReturnPackage, validateManifest, SIMULATED_TRANSITIONS, JAM_IDENTITIES } from '../web/jam-device-policy.js';
import { sha256hex } from '../web/fm1pkg.js';
import { MockMIDIAccess, profile, simulateInstall } from './helpers/jam_install_simulator.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const directory = path.resolve(process.argv[2] || path.join(root, 'build/jambox-device'));
const returnFile = path.resolve(process.argv[3] || '/workspace/deps/jambox-return/SLOOP-2.6.fwsc');
const manifest = validateManifest(JSON.parse(await fs.readFile(path.join(directory, 'manifest.json'), 'utf8')));
async function artifact(name, sha256, size) {
  const filename = path.join(directory, name), stat = await fs.lstat(filename);
  assert.ok(stat.isFile() && !stat.isSymbolicLink(), `${name}: expected a regular bundle file`);
  const bytes = new Uint8Array(await fs.readFile(filename));
  if (size !== undefined) assert.equal(bytes.length, size, `${name}: size mismatch`);
  assert.equal(await sha256hex(bytes), sha256, `${name}: SHA-256 mismatch`); return bytes;
}
const firmware = await artifact(manifest.firmware.file, manifest.firmware.sha256, manifest.firmware.size_bytes);
const candidate = await prepareCandidate(manifest, firmware);
await artifact(manifest.source.file, manifest.source.sha256, manifest.source.size_bytes);
const receipt = JSON.parse(new TextDecoder().decode(await artifact(manifest.build.receipt_file, manifest.build.receipt_sha256)));
const memory = JSON.parse(new TextDecoder().decode(await artifact(manifest.build.memory_report_file, manifest.build.memory_report_sha256)));
for (const license of manifest.licenses) await artifact(license.file, license.sha256, license.size_bytes);
assert.equal(receipt.schema_version, 1); assert.equal(receipt.kind, 'jambox-device-build');
assert.equal(receipt.configuration.app.identity, JAM_IDENTITIES.app);
assert.equal(receipt.configuration.loader.identity, JAM_IDENTITIES.loader);
assert.equal(receipt.configuration.package_identity, JAM_IDENTITIES.package);
assert.equal(receipt.configuration.app.version, `JamBoxFM ${manifest.version}`);
assert.deepEqual(receipt.configuration.app.flags, manifest.build.flags);
assert.deepEqual(receipt.artifacts['felucca.fwsc'], { size_bytes: firmware.length, sha256: candidate.sha256 });
assert.ok(memory.app && memory.loader, 'Expected app and loader memory evidence');
const exactReturn = await prepareReturnPackage(new Uint8Array(await fs.readFile(returnFile)));
const cases = [['jam', candidate], ['rescue', candidate], ['loader', candidate], ['sloop', candidate], ['jam', exactReturn], ['rescue', exactReturn]];
const transitions = [], originalTimer = globalThis.setTimeout;
// Real updater control flow and wire codec; only delays are shortened. Every
// port is a test-owned MockMIDIAccess port, with no Web MIDI or native backend.
globalThis.setTimeout = (fn, ms, ...args) => originalTimer(fn, Math.min(ms, 1), ...args);
try {
  for (const [source, prepared] of cases) {
    const returning = prepared.family === 'sloop';
    const access = new MockMIDIAccess({ source: profile(source), loader: profile(returning ? 'ota-FM-1_900' : 'loader'), final: profile(returning ? 'sloop' : 'jam') });
    const result = await simulateInstall({ access, pair: access.pair(), candidate: prepared });
    assert.equal(result.steps.at(-1)[0], 'done'); assert.equal(result.installAllowed, false);
    assert.ok(access.ports.every(port => port.listeners.size === 0 && port.opens === port.closes));
    transitions.push(result.transition);
  }
} finally { globalThis.setTimeout = originalTimer; }
assert.deepEqual(transitions, SIMULATED_TRANSITIONS);
console.log(JSON.stringify({ result: 'passed', family: candidate.family, version: candidate.version,
  firmware_size_bytes: firmware.length, firmware_sha256: candidate.sha256,
  bundle_descriptors_verified: 4 + manifest.licenses.length, simulated_transitions: transitions,
  return_sha256: exactReturn.sha256, release_enabled: false, hardware_validated: false, install_allowed: false }, null, 2));
