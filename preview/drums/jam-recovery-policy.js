// SPDX-License-Identifier: GPL-3.0-only
// Isolated development saving trial. Normal install policy remains unchanged.
// Transport and handover guards follow jam-trial-install.js; firmware validation
// reuses the reviewed canonical package parser without relaxing its manifest policy.
import { Updater, FM1_PORT } from './fm1ota.js';
import { JAM_VERSION, JAM_IDENTITIES, SLOOP_26, validatePackage, inspectDevice } from './jam-device-policy.js';
import { sha256hex } from './fm1pkg.js';

const REQUIRED_FLAGS = Object.freeze({ JAMBOX_PROTO: 1, FELUCCA_FLASH: 1, FELUCCA_PERSIST: 0, FELUCCA_OTA: 1,
  FELUCCA_OTA_DRYRUN: 0, FELUCCA_OTA_RAMONLY: 0, FELUCCA_ICONS: 0, FELUCCA_SLICE: 0, JAMBOX_RECOVERY: 1 });
const TARGET = Object.freeze({ model: 'M-VAVE FM-1', soc: 'AC791N', platform: 'WL82', flash_bytes: 1048576,
  flash_jedec: '0x856014', chip_key: '0x980f' });
function insist(ok, message, code = 'policy') {
  if (!ok) { const error = new Error(message); error.code = code; throw error; }
}
function plain(value) { return value !== null && typeof value === 'object' &&
  (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null); }
function keys(value, expected, label) {
  insist(plain(value) && Object.keys(value).sort().join('|') === [...expected].sort().join('|'), `${label}: unexpected or missing fields`, 'manifest');
}
const hash = value => typeof value === 'string' && /^[0-9a-f]{64}$/.test(value);
const size = value => Number.isSafeInteger(value) && value > 0 && value <= 64 * 1024 * 1024;
function path(value) { return typeof value === 'string' && /^[A-Za-z0-9][A-Za-z0-9_./-]*$/.test(value) &&
  !value.split('/').some(part => !part || part === '.' || part === '..'); }
function freeze(value) { if (value && typeof value === 'object') { for (const item of Object.values(value)) freeze(item); Object.freeze(value); } return value; }

export function validateRecoveryManifest(manifest) {
  keys(manifest, ['schema_version', 'family', 'version', 'release_enabled', 'hardware_validated', 'target', 'identities',
    'firmware', 'source', 'build', 'transitions', 'return_firmware', 'format_validation', 'licenses', 'kind', 'recovery'], 'manifest');
  insist(manifest.schema_version === 1 && manifest.kind === 'development-recovery-candidate' && manifest.family === 'jamboxfm' && manifest.version === JAM_VERSION,
    'Unsupported candidate manifest version or family', 'manifest');
  insist(manifest.release_enabled === false && manifest.hardware_validated === false,
    'Only an unvalidated development recovery candidate is accepted', 'manifest');
  for (const [name, expected] of [['target', TARGET], ['identities', JAM_IDENTITIES], ['return_firmware', SLOOP_26]]) {
    keys(manifest[name], Object.keys(expected), name);
    for (const [field, value] of Object.entries(expected)) insist(manifest[name][field] === value, `${name}.${field} mismatch`, 'manifest');
  }
  keys(manifest.firmware, ['file', 'size_bytes', 'sha256'], 'firmware');
  keys(manifest.source, ['file', 'size_bytes', 'sha256', 'revision', 'dirty'], 'source');
  insist(manifest.firmware.file === `jamboxfm-recovery-${JAM_VERSION}.fwsc` && manifest.source.file === 'jamboxfm-source.zip',
    'Unexpected artifact filenames', 'manifest');
  for (const item of [manifest.firmware, manifest.source]) insist(size(item.size_bytes) && hash(item.sha256), 'Invalid artifact size or SHA-256', 'manifest');
  insist(manifest.firmware.size_bytes >= 960 && manifest.firmware.size_bytes <= 2 * 1024 * 1024, 'Invalid firmware size', 'manifest');
  insist((manifest.source.revision === null && manifest.source.dirty === null) ||
    (typeof manifest.source.revision === 'string' && /^[0-9a-f]{40}$/.test(manifest.source.revision) && typeof manifest.source.dirty === 'boolean'),
  'Invalid source provenance', 'manifest');
  keys(manifest.build, ['receipt_file', 'receipt_sha256', 'flags', 'memory_report_file', 'memory_report_sha256'], 'build');
  insist(manifest.build.receipt_file === 'build-receipt.json' && manifest.build.memory_report_file === 'memory.json' &&
    hash(manifest.build.receipt_sha256) && hash(manifest.build.memory_report_sha256), 'Invalid build evidence descriptor', 'manifest');
  keys(manifest.build.flags, [...Object.keys(REQUIRED_FLAGS), 'FELUCCA_CDC', 'FELUCCA_UART'], 'build.flags');
  insist(Object.values(manifest.build.flags).every(value => value === 0 || value === 1), 'Invalid build flags', 'manifest');
  for (const [flag, value] of Object.entries(REQUIRED_FLAGS)) insist(manifest.build.flags[flag] === value, `Required build flag ${flag} mismatch`, 'manifest');
  keys(manifest.transitions, ['mode', 'approved', 'simulated'], 'transitions');
  insist(manifest.transitions.mode === 'development-trial' && Array.isArray(manifest.transitions.approved) && !manifest.transitions.approved.length &&
    Array.isArray(manifest.transitions.simulated) && JSON.stringify(manifest.transitions.simulated) === JSON.stringify(TRANSITIONS),
  'Only the isolated JamBox-to-JamBox development transition is accepted', 'manifest');
  const evidence = ['package_header_crc', 'entry_headers_crc', 'payload_crc', 'chip_key_match', 'canonical_reconstruction', 'app_loader_elf_match', 'memory_regions'];
  keys(manifest.format_validation, evidence, 'format_validation');
  for (const field of evidence)
    insist(manifest.format_validation[field] === true, `Missing package evidence: ${field}`, 'manifest');
  insist(Array.isArray(manifest.licenses) && manifest.licenses.length > 0 && manifest.licenses.length <= 32, 'Missing license evidence', 'manifest');
  const files = new Set();
  for (const item of manifest.licenses) {
    keys(item, ['file', 'sha256', 'size_bytes'], 'license');
    insist(path(item.file) && hash(item.sha256) && size(item.size_bytes) && !files.has(item.file), 'Invalid license descriptor', 'manifest');
    files.add(item.file);
  }
  keys(manifest.recovery, ['schema_version', 'automatic_saving', 'requires_storage_setup', 'legacy_storage_replaced'], 'recovery');
  insist(manifest.recovery.schema_version === 1 && manifest.recovery.automatic_saving === false &&
    manifest.recovery.requires_storage_setup === true && manifest.recovery.legacy_storage_replaced === true,
    'Unsupported recovery behavior', 'manifest');
  return freeze(JSON.parse(JSON.stringify(manifest)));
}


export async function prepareRecoveryCandidate(manifest, bytes) {
  insist(bytes instanceof Uint8Array, 'Expected package bytes', 'package');
  const checked = validateRecoveryManifest(manifest), copy = new Uint8Array(bytes);
  insist(copy.length === checked.firmware.size_bytes && await sha256hex(copy) === checked.firmware.sha256,
    'Development firmware SHA-256/size mismatch', 'package');
  const validated = validatePackage(copy, checked.identities.package), image = validated.image.slice();
  return Object.freeze({ family: 'jamboxfm', version: JAM_VERSION, sha256: checked.firmware.sha256,
    product: validated.product, manifest: checked, releaseEnabled: false, hardwareValidated: false,
    developmentRecovery: true, image: () => image.slice() });
}

const TRANSITIONS = Object.freeze(['jambox-normal-to-jambox']);
const OPEN_TIMEOUT = 10000, LOAD_TIMEOUT = 30000, INSTALL_TIMEOUT = 10 * 60 * 1000;
const loadedBundles = new WeakMap();
const checkedBackups = new WeakSet();

// This matches local files only. It does not prove backup age, USB serial,
// which physical instrument supplied the dump, or contents of the current RAM jam.
// Neither file is uploaded or retained by this module.
export async function validateRecoveryBackup(reportBytes, flashBytes) {
  insist(reportBytes instanceof Uint8Array && reportBytes.length > 0 && reportBytes.length <= 128 * 1024,
    'Choose the backup.json from your recovery folder', 'backup');
  insist(flashBytes instanceof Uint8Array && flashBytes.length === 1048576,
    'Choose the 1,048,576-byte full-flash-before.bin from the same folder', 'backup');
  let report;
  try { report = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(reportBytes)); }
  catch { throw failure('backup', 'The backup report is not valid UTF-8 JSON'); }
  insist(plain(report) && report.tool === 'MvaveFM1Unbricker fm1_unbrick.py 1.1.0' &&
    plain(report.device) && report.device.vendor === 'WL82' && report.device.product === 'UBOOT1.00' &&
    report.chip_key === '0x980f' && report.flash_id === '0x856014' && report.full_backup === 'full-flash-before.bin' &&
    typeof report.backup_sha256 === 'string' && /^[a-fA-F0-9]{64}$/.test(report.backup_sha256),
    'These files do not match the reviewed FM-1 recovery backup format', 'backup');
  const digest = await sha256hex(flashBytes);
  insist(digest === report.backup_sha256.toLowerCase(), 'The flash backup does not match backup.json', 'backup');
  const proof = Object.freeze({ backupSHA256: digest, sizeBytes: flashBytes.length,
    rollbackBaseline: true, freshnessVerified: false, deviceBindingVerified: false });
  checkedBackups.add(proof);
  return proof;
}

function failure(code, message) { const error = new Error(message); error.code = code; return error; }
function cancelled(signal) { if (signal?.aborted) throw failure('aborted', 'Cancelled before installation began.'); }
function sourceTransition(identity, capabilities) {
  if (identity?.text === JAM_IDENTITIES.app && capabilities?.recognized &&
      capabilities.family === 'jamboxfm' && capabilities.release === JAM_VERSION && capabilities.mode === 'normal') return TRANSITIONS[0];
  return null;
}
function extraCandidates(access) {
  // listPairs omits ambiguous same-name groups, so count raw candidates too.
  const count = ports => [...ports.values()].filter(port => port.state !== 'disconnected' && FM1_PORT.test(port.name || '')).length;
  return count(access.inputs) > 1 || count(access.outputs) > 1;
}
function oneDevice(access) {
  insist(!extraCandidates(access), 'Connect only one FM-1 MIDI device before starting this trial. Disconnect other matching MIDI devices and check again.', 'ambiguous');
}
function scope(parent, timeout, canCancel = () => true) {
  const controller = new AbortController(); let timedOut = false;
  const stop = () => { if (canCancel()) controller.abort(); };
  parent?.addEventListener('abort', stop, { once: true });
  if (parent?.aborted) stop();
  const timer = setTimeout(() => { timedOut = true; controller.abort(); }, timeout);
  return { signal: controller.signal, get timedOut() { return timedOut; },
    close() { clearTimeout(timer); parent?.removeEventListener('abort', stop); } };
}
function scopedError(error, operation, started = false) {
  if (operation.timedOut) error = failure('timeout', started ?
    'The installation exceeded its time limit. Firmware may be partially written; keep the device connected and identify its current mode.' :
    'The operation timed out before installation began.');
  else if (operation.signal.aborted) error = failure('aborted', 'Cancelled before installation began.');
  if (!(error instanceof Error)) error = failure('operation', String(error));
  error.started = started;
  return error;
}

async function fetchBytes(url, limit, fetchImpl, signal) {
  cancelled(signal);
  const response = await fetchImpl(url.href, { signal, cache: 'no-store', credentials: 'omit', redirect: 'error' });
  insist(response.ok, `${url.pathname}: HTTP ${response.status}`);
  const length = response.headers?.get('content-length');
  insist(length === null || length === undefined || (/^\d+$/.test(length) && Number(length) <= limit), 'Artifact exceeds its size limit');
  let bytes;
  if (response.body?.getReader) {
    const reader = response.body.getReader(), parts = []; let size = 0;
    try {
      for (;;) {
        cancelled(signal);
        const { done, value } = await reader.read();
        if (done) break;
        size += value.length;
        if (size > limit) { await reader.cancel(); throw failure('package', 'Artifact exceeds its size limit'); }
        parts.push(value);
      }
      bytes = new Uint8Array(size); let offset = 0;
      for (const part of parts) { bytes.set(part, offset); offset += part.length; }
    } finally { reader.releaseLock(); }
  } else {
    bytes = new Uint8Array(await response.arrayBuffer());
    insist(bytes.length <= limit, 'Artifact exceeds its size limit');
  }
  cancelled(signal);
  return bytes;
}
function json(bytes) {
  try { return JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes)); }
  catch { throw failure('package', 'Trial metadata is not valid UTF-8 JSON'); }
}

export async function loadRecoveryBundle({ baseURL = new URL('./firmware/', import.meta.url), fetchImpl = fetch, signal } = {}) {
  const base = new URL(baseURL, import.meta.url), operation = scope(signal, LOAD_TIMEOUT);
  if (!base.pathname.endsWith('/')) base.pathname += '/';
  base.search = ''; base.hash = '';
  try {
    const trial = json(await fetchBytes(new URL('trial.json', base), 4096, fetchImpl, operation.signal));
    const fields = ['schema_version', 'kind', 'hardware_validated', 'release_enabled', 'transitions', 'manifest_sha256', 'firmware_sha256'];
    insist(trial && typeof trial === 'object' && !Array.isArray(trial) &&
      Object.keys(trial).sort().join('|') === fields.sort().join('|'), 'Invalid trial descriptor fields');
    insist(trial.schema_version === 1 && trial.kind === 'development-recovery-install' && trial.hardware_validated === false && trial.release_enabled === false &&
      JSON.stringify(trial.transitions) === JSON.stringify(TRANSITIONS) && hash(trial.manifest_sha256) && hash(trial.firmware_sha256),
    'Unsupported experimental trial descriptor');
    const manifestBytes = await fetchBytes(new URL('manifest.json', base), 128 * 1024, fetchImpl, operation.signal);
    insist(await sha256hex(manifestBytes) === trial.manifest_sha256, 'Trial manifest SHA-256 mismatch');
    const manifest = json(manifestBytes);
    // Validate before using the manifest's filename, and before any MIDI access.
    const checked = validateRecoveryManifest(manifest);
    insist(checked.firmware.sha256 === trial.firmware_sha256, 'Trial firmware binding mismatch');
    const bytes = await fetchBytes(new URL(checked.firmware.file, base), checked.firmware.size_bytes, fetchImpl, operation.signal);
    const candidate = await prepareRecoveryCandidate(checked, bytes);
    const [receiptBytes, memoryBytes] = await Promise.all([
      fetchBytes(new URL(checked.build.receipt_file, base), 4 * 1024 * 1024, fetchImpl, operation.signal),
      fetchBytes(new URL(checked.build.memory_report_file, base), 128 * 1024, fetchImpl, operation.signal),
    ]);
    insist(await sha256hex(receiptBytes) === checked.build.receipt_sha256 &&
      await sha256hex(memoryBytes) === checked.build.memory_report_sha256, 'Build evidence SHA-256 mismatch', 'hash');
    const receipt = json(receiptBytes), memory = json(memoryBytes);
    insist(receipt.schema_version === 1 && receipt.kind === 'jambox-device-build' && memory.ok === true,
      'Missing passing build evidence', 'bundle');
    const configuration = receipt.configuration;
    insist(configuration?.app?.identity === JAM_IDENTITIES.app && configuration.app.version === `JamBoxFM ${JAM_VERSION}` &&
      configuration?.loader?.identity === JAM_IDENTITIES.loader && configuration.package_identity === JAM_IDENTITIES.package,
      'Build receipt identity mismatch', 'bundle');
    keys(configuration.app.flags, Object.keys(checked.build.flags), 'receipt.app.flags');
    for (const [flag, value] of Object.entries(checked.build.flags))
      insist(configuration.app.flags[flag] === value, `Receipt build flag ${flag} mismatch`, 'bundle');
    keys(configuration.loader.flags, Object.keys(checked.build.flags), 'receipt.loader.flags');
    insist(Object.values(configuration.loader.flags).every(value => value === 0 || value === 1), 'Invalid loader flags', 'bundle');
    for (const [flag, value] of Object.entries({ JAMBOX_PROTO: 1, FELUCCA_OTA: 1, FELUCCA_OTA_DRYRUN: 0,
      FELUCCA_OTA_RAMONLY: 0, FELUCCA_PERSIST: 0, JAMBOX_RECOVERY: 0 }))
      insist(configuration.loader.flags?.[flag] === value, `Receipt loader flag ${flag} mismatch`, 'bundle');
    insist(receipt.source?.revision === checked.source.revision && receipt.source?.dirty === checked.source.dirty,
      'Build receipt source provenance mismatch', 'bundle');
    const built = receipt.artifacts?.['felucca.fwsc'];
    insist(built?.sha256 === candidate.sha256 && built.size_bytes === bytes.length,
      'Build receipt firmware binding mismatch', 'bundle');
    cancelled(operation.signal);
    Object.freeze(trial.transitions);
    const bundle = Object.freeze({ version: candidate.version, firmwareSHA256: candidate.sha256,
      firmwareSizeBytes: bytes.length, manifest: candidate.manifest, trial: Object.freeze(trial), hardwareValidated: false });
    loadedBundles.set(bundle, candidate);
    return bundle;
  } catch (error) { throw scopedError(error, operation); }
  finally { operation.close(); }
}

export class RecoveryInstaller {
  #busy = false;
  #started = false;
  constructor(access, { onStep = () => {}, onLog = () => {} } = {}) {
    insist(access?.inputs && access?.outputs && typeof onStep === 'function' && typeof onLog === 'function', 'Web MIDI access and callbacks are required', 'configuration');
    this.access = access; this.onStep = onStep; this.onLog = onLog;
  }
  get busy() { return this.#busy; }
  get started() { return this.#started; }
  listPairs() { return new Updater(this.access).listPairs(); }
  log(message) { try { this.onLog(message); } catch { /* UI logging cannot stop a flash transfer. */ } }
  step(stage, detail) { try { this.onStep(stage, detail); } catch { /* Likewise for progress rendering. */ } }
  available() { if (this.busy) throw failure('busy', 'Another device operation is still running.'); }

  async inspect(pair, { signal } = {}) {
    this.available(); this.#busy = true;
    const operation = scope(signal, OPEN_TIMEOUT);
    try {
      const updater = new Updater(this.access);
      const observation = await updater.identify({ pair, signal: operation.signal, deadline: Date.now() + OPEN_TIMEOUT,
        query: (link, identity) => inspectDevice(identity, link, { signal: operation.signal }) });
      const ambiguous = extraCandidates(this.access), transition = sourceTransition(observation.identity, observation.capabilities);
      const eligible = !!transition && !ambiguous;
      return { identity: observation.identity, capabilities: observation.capabilities, eligible, transition,
        uniqueDevice: !ambiguous, hardwareValidated: false,
        reason: ambiguous ? 'Connect only one FM-1 MIDI device before starting this trial. Disconnect other matching MIDI devices and check again.' :
          eligible ? 'JamBox is ready for the development saving trial.' :
          'Start from a running JamBox build with the exact matching family response. Factory firmware and other instruments are not accepted here.' };
    } catch (error) { throw scopedError(error, operation); }
    finally { operation.close(); this.#busy = false; }
  }

  async install(bundle, pair, { signal, backup, disposableJamReady = false } = {}) {
    this.available();
    insist(loadedBundles.has(bundle), 'Load and verify the trial bundle before installation');
    insist(checkedBackups.has(backup), 'Check your matching recovery backup files before installation', 'backup');
    insist(disposableJamReady === true, 'Confirm the current temporary jam can be discarded before installation', 'backup');
    this.#busy = true; this.#started = false;
    // External UI cancellation stops preparation only. Once staging may have
    // started, neither a cancelled UI task nor a rendering error aborts a write.
    const operation = scope(signal, INSTALL_TIMEOUT, () => !this.started);
    const candidate = loadedBundles.get(bundle), updater = new Updater(this.access);
    let passedApp = false, passedLoader = false, finalIdentity = null, sourceIdentity = null, transition = null;
    try {
      cancelled(operation.signal);
      oneDevice(this.access);
      this.step('checking');
      const identityGuard = async (identity, context) => {
        if (context.stage === 'app') {
          oneDevice(this.access); // Recheck immediately before staging, not only at inspection.
          const observed = await inspectDevice(identity, context.link, { signal: operation.signal });
          oneDevice(this.access); // A device can appear while the family query is pending.
          transition = sourceTransition(identity, observed);
          passedApp = !!transition;
          if (passedApp) {
            sourceIdentity = { ...identity };
            this.log('Fresh JamBox application identity and family response checked.');
          }
          return passedApp;
        }
        const observed = await inspectDevice(identity, context.link, { signal: operation.signal });
        oneDevice(this.access); // Re-enumeration can change during a family query too.
        const jambox = observed.recognized && observed.family === 'jamboxfm' && observed.release === candidate.version;
        if (context.stage === 'loader') {
          passedLoader = passedApp && jambox && observed.mode === 'loader' && identity.text === JAM_IDENTITIES.loader;
          if (passedLoader) this.log('Expected JamBox update loader identified.');
          return passedLoader;
        }
        if (context.stage === 'final' && passedApp && passedLoader && jambox && observed.mode === 'normal' && identity.text === JAM_IDENTITIES.app) {
          finalIdentity = { ...identity }; this.log('JamBox application identity and release checked.'); return true;
        }
        return false;
      };
      await updater.install(candidate.image(), candidate.product, (stage, detail) => {
        if (stage === 'start') this.#started = true; // Conservative: immediately before the first upgrade send attempt.
        this.step(stage, detail);
      }, { pair, requireSelection: true, signal: operation.signal, deadline: Date.now() + OPEN_TIMEOUT, identityGuard,
        selectPair: ({ stage, pairs }) => {
          insist(stage === 'loader' || stage === 'final', 'Unexpected reconnect stage', 'identity');
          insist(pairs.length <= 1 && !extraCandidates(this.access),
            'Multiple MIDI candidates are available. Keep the FM-1 connected and identify its current mode before retrying.', 'ambiguous');
          // No arbitrary scan: open only the sole current pair, then require
          // the exact expected stage identity before either writing or success.
          return pairs[0] || null;
        } });
      insist(finalIdentity, 'The expected JamBox application did not identify itself', 'identity');
      return { identity: finalIdentity, sourceIdentity, version: candidate.version, firmwareSHA256: candidate.sha256,
        hardwareValidated: false, experimental: true, transition, started: true,
        verification: 'reported-family-and-release' };
    } catch (error) {
      const scoped = scopedError(error, operation, this.started);
      // A fresh, eligible source can differ from the earlier inspection. Keep
      // failed-transfer reports tied to the source we actually checked.
      if (sourceIdentity && transition) {
        scoped.sourceIdentity = sourceIdentity;
        scoped.transition = transition;
      }
      throw scoped;
    }
    finally { operation.close(); this.#busy = false; }
  }
}
