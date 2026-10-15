// SPDX-License-Identifier: GPL-3.0-only
// Page state only. Package validation, device guards, and every MIDI write live
// in the isolated recovery trial controller, never in this UI or a test helper.
import { loadRecoveryBundle, RecoveryInstaller, validateRecoveryBackup } from '../jam-recovery-policy.js';

const STAGES = ['checking', 'start', 'verify', 'loader', 'write', 'reboot', 'done'];
const CODES = ['aborted', 'permission', 'ambiguous', 'notfound', 'identity', 'model', 'lost',
  'timeout', 'stopped', 'noloader', 'noreturn', 'mismatch', 'foreign', 'badreq', 'bundle',
  'manifest', 'trial', 'package', 'hash', 'busy', 'ineligible', 'configuration', 'operation', 'backup', 'policy'];
const safeCode = error => CODES.includes(error?.code) ? error.code : 'unknown';
const safeIdentity = identity => identity && /^(?:ota-)?FM-1_\d{3}$/.test(identity.text || '') ? identity.text : null;
const safeTransition = value => ['jambox-normal-to-jambox'].includes(value) ? value : null;

function beforeStartMessage(error) {
  if (error?.name === 'NotAllowedError' || error?.name === 'SecurityError' || error?.code === 'permission')
    return 'MIDI access wasn’t allowed. Allow MIDI and system exclusive access in your browser’s site settings, then connect again.';
  if (error?.code === 'backup') return 'Check both matching recovery files and confirm the current jam can be discarded. No update was started.';
  if (error?.code === 'aborted') return 'Preparation cancelled. No update was started.';
  if (error?.code === 'ambiguous') return 'Choose one FM-1 below, or disconnect other MIDI devices, then check the connection again.';
  if (error?.code === 'lost' || error?.code === 'notfound') return 'The FM-1 disconnected. Check the USB cable, then connect again.';
  if (error?.code === 'identity' || error?.code === 'ineligible') return 'The selected device could not be confirmed as a running supported JamBox build. No update was started.';
  return 'The device check could not finish. Close other MIDI apps, check the USB cable, and connect again. No update was started.';
}

export function mountRecoveryPage({
  document: doc = document,
  requestMIDIAccess = typeof navigator.requestMIDIAccess === 'function' ? navigator.requestMIDIAccess.bind(navigator) : null,
  loadBundle = loadRecoveryBundle,
  createInstaller = (access, callbacks) => new RecoveryInstaller(access, callbacks),
  checkBackup = validateRecoveryBackup,
  now = () => new Date(),
} = {}) {
  const ids = ['connect-device', 'pair-field', 'midi-pair', 'pair-hint', 'device-state', 'device-label',
    'firmware-state', 'firmware-label', 'reload-firmware', 'install-device', 'cancel-prepare', 'status',
    'progress-card', 'progress-title', 'progress-detail', 'stage-check', 'stage-loader', 'stage-write',
    'stage-return', 'backup-report', 'backup-flash', 'backup-state', 'backup-label', 'disposable-jam', 'result-card', 'result-title', 'result-eyebrow', 'result-guidance', 'save-report', 'report-details'];
  const ui = Object.fromEntries(ids.map(id => [id, doc.getElementById(id)]));
  const drumTrial = doc.documentElement?.dataset?.trial === 'drums';
  let access = null, installer = null, bundle = null, pairs = [], observation = null;
  let operation = null, permissionPending = false, bundlePending = false, attempted = false, success = false;
  let backup = null, backupPending = false, backupGeneration = 0;
  let disposed = false, report = null, selectedKey = '', stages = [], lastStage = null, knownTopology = '';
  let requestCounts = { verify: 0, write: 0 };
  const removers = [], bundleAbort = new AbortController();
  const on = (target, type, fn) => { target.addEventListener(type, fn); removers.push(() => target.removeEventListener(type, fn)); };
  const timestamp = () => now().toISOString();
  const started = () => attempted || installer?.started === true;
  const updating = () => observation?.transition === 'jambox-normal-to-jambox';
  const key = pair => JSON.stringify([pair.inputId, pair.outputId]);
  const topology = () => !access ? '' : JSON.stringify(['inputs', 'outputs'].flatMap(direction =>
    [...access[direction].values()].filter(port => port.state !== 'disconnected')
      .map(port => [direction, port.id, port.name || ''])).sort((a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b))));
  const selectedPair = () => pairs.find(pair => key(pair) === selectedKey) || null;
  const status = (message, error = false) => { ui.status.textContent = message; ui.status.dataset.tone = error ? 'error' : 'info'; };

  function controls() {
    const busy = !!operation || permissionPending;
    ui['connect-device'].disabled = !requestMIDIAccess || busy || started();
    ui['midi-pair'].disabled = busy || started();
    ui['install-device'].disabled = busy || started() || !bundle || !observation?.eligible || pairs.length !== 1 || !selectedPair() || !backup || backupPending || !ui['disposable-jam'].checked;
    ui['install-device'].textContent = drumTrial ? 'Install the drum trial →' : 'Install the saving trial →';
    for (const id of ['backup-report', 'backup-flash', 'disposable-jam']) ui[id].disabled = busy || started();
    ui['cancel-prepare'].hidden = !operation || started();
    ui['reload-firmware'].disabled = busy || bundlePending || started();
  }
  function invalidateDevice() {
    observation = null;
    ui['device-state'].dataset.ready = 'false';
    ui['device-state'].dataset.error = 'false';
    ui['device-label'].textContent = 'Waiting for your FM-1';
  }
  function refreshPairs() {
    if (!installer || started()) return;
    knownTopology = topology();
    const previous = selectedKey, previousCount = pairs.length;
    pairs = installer.listPairs();
    if (!pairs.some(pair => key(pair) === selectedKey)) selectedKey = pairs.length === 1 ? key(pairs[0]) : '';
    if (previous !== selectedKey || previousCount !== pairs.length) invalidateDevice();
    ui['midi-pair'].replaceChildren(new Option('Choose a device', ''));
    for (const pair of pairs) ui['midi-pair'].append(new Option(pair.name, key(pair)));
    ui['midi-pair'].value = selectedKey;
    ui['pair-field'].hidden = pairs.length <= 1;
    ui['connect-device'].textContent = 'Check connection again';
    controls();
  }
  function portsChanged() {
    // Web MIDI also emits statechange when a read opens/closes a port. Those
    // connection events can arrive after its promise settles; only changes to
    // connected device IDs/names invalidate the checked selection.
    const currentTopology = topology();
    if (currentTopology === knownTopology) return;
    knownTopology = currentTopology;
    if (started() || operation?.kind === 'install') return;
    if (operation?.kind === 'inspect') {
      const stillThere = installer.listPairs().some(pair => key(pair) === selectedKey);
      if (!stillThere) { invalidateDevice(); operation.abort.abort(); }
      return;
    }
    const before = selectedKey, wasReady = observation?.eligible === true;
    refreshPairs();
    // Even a raw port that cannot form a selectable pair can make the next
    // handover ambiguous. Any idle MIDI topology change requires a fresh read.
    if (wasReady) { invalidateDevice(); controls(); }
    if (pairs.length > 1) status('Connect only one FM-1 for this trial. Disconnect other matching MIDI devices, then check the connection again.');
    else if (before && before !== selectedKey) status('The selected FM-1 disconnected. Check the connection again.', true);
    else if (wasReady) status('The MIDI connections changed. Check your FM-1’s connection again before installing.');
  }
  async function prepareBundle() {
    if (bundlePending || started() || disposed) return;
    bundlePending = true; bundle = null;
    ui['reload-firmware'].hidden = true;
    ui['firmware-state'].dataset.ready = 'false'; ui['firmware-state'].dataset.error = 'false';
    ui['firmware-label'].textContent = 'Checking the firmware download…'; controls();
    try {
      const loaded = await loadBundle({ baseURL: new URL('../firmware/', doc.baseURI), signal: bundleAbort.signal });
      if (disposed) return;
      bundle = loaded;
      ui['firmware-label'].textContent = `${drumTrial ? 'Drum grid trial' : 'Saving trial'} ${loaded.version} · firmware and build evidence verified`;
      ui['firmware-state'].dataset.ready = 'true';
    } catch (error) {
      if (disposed) return;
      ui['firmware-label'].textContent = 'Firmware download could not be verified.';
      ui['firmware-state'].dataset.error = 'true'; ui['reload-firmware'].hidden = false;
      status('The firmware download could not be verified. Check your connection and check the download again. Installation is disabled.', true);
    } finally { bundlePending = false; if (!disposed) controls(); }
  }
  async function inspectSelected() {
    const pair = selectedPair();
    if (!pair || operation || started() || disposed) return;
    const current = { kind: 'inspect', abort: new AbortController() }; operation = current;
    invalidateDevice(); controls(); status('Reading your FM-1’s firmware…');
    try {
      const result = await installer.inspect(pair, { signal: current.abort.signal });
      if (disposed || current.abort.signal.aborted) return;
      refreshPairs();
      observation = { ...result, eligible: result.eligible === true && pairs.length === 1 };
      // Some ambiguous duplicate port groups cannot form visible listPairs()
      // entries. The controller also counts those raw ports during inspection.
      const multiple = pairs.length > 1 || result.uniqueDevice === false;
      ui['device-state'].dataset.ready = String(observation.eligible);
      ui['device-state'].dataset.error = String(!observation.eligible);
      ui['device-label'].textContent = multiple ? 'More than one matching MIDI device · disconnect the others' :
        observation.eligible ? (drumTrial ? 'JamBoxFM · ready for the drum trial' : 'JamBoxFM · ready for the saving trial') :
        `${safeIdentity(result.identity) || 'Unconfirmed firmware'} · requires a running supported JamBox`;
      status(multiple ? 'Connect only one FM-1 for this trial. Disconnect other matching MIDI devices, then check the connection again.' :
        observation.eligible ? 'JamBox is connected. Check your recovery backup below, then start with a disposable jam.' :
        'Use a running supported JamBoxFM build for this trial. No update was started.', !observation.eligible);
    } catch (error) {
      if (!disposed) status(beforeStartMessage(error), error?.code !== 'aborted');
    } finally {
      if (operation === current) operation = null;
      if (!disposed) { refreshPairs(); controls(); }
    }
  }
  async function connect() {
    if (!requestMIDIAccess || operation || permissionPending || started() || disposed) return;
    try {
      if (!access) {
        permissionPending = true; controls(); status('Waiting for MIDI permission…');
        const granted = await requestMIDIAccess({ sysex: true });
        if (disposed) return;
        access = granted;
        installer = createInstaller(access, { onStep: step, onLog: () => {} });
        on(access, 'statechange', portsChanged);
      }
      refreshPairs();
      if (!pairs.length) status('No matching FM-1 ports are available. Connect it by USB and close other MIDI apps, then check again.');
      else if (pairs.length > 1) status('Connect only one FM-1 for this trial. Disconnect other matching MIDI devices, then check the connection again.');
    } catch (error) { if (!disposed) status(beforeStartMessage(error), true); }
    finally { permissionPending = false; if (!disposed) controls(); }
    if (!disposed && selectedPair()) await inspectSelected();
  }
  function step(stage, detail) {
    if (disposed || !STAGES.includes(stage)) return;
    if (stage === 'start') attempted = true;
    if ((stage === 'verify' || stage === 'write') && Number.isSafeInteger(detail) && detail >= 0 && detail <= 1000000)
      requestCounts[stage] = Math.max(requestCounts[stage], detail);
    // Packet callbacks can arrive hundreds of times per stage. Announce only
    // stage changes so screen readers do not repeat the same live status.
    if (stage === lastStage) return;
    stages.push({ stage, at: timestamp() }); lastStage = stage;
    const index = { checking: 0, start: 1, verify: 1, loader: 1, write: 2, reboot: 3, done: 3 }[stage];
    ['stage-check', 'stage-loader', 'stage-write', 'stage-return'].forEach((id, i) => {
      ui[id].dataset.state = i < index ? 'done' : i === index ? 'active' : 'waiting';
      if (i === index) ui[id].setAttribute('aria-current', 'step'); else ui[id].removeAttribute('aria-current');
    });
    const descriptions = {
      checking: ['Checking your FM-1.', 'Checking the selected instrument again before sending an update.'],
      start: ['Preparing the update.', 'The update has started. Keep the FM-1 powered and connected, and leave this tab open.'],
      verify: ['Sending the update loader.', 'The FM-1 is requesting the firmware it needs to prepare the update.'],
      loader: ['Waiting for the update loader.', 'The USB connection will briefly change as the FM-1 enters its update loader.'],
      write: ['Installing JamBoxFM.', 'The update loader is writing the instrument. Keep USB and power connected.'],
      reboot: ['Waiting for your instrument.', 'The transfer has finished. Waiting for JamBoxFM to return and identify itself.'],
      done: ['Confirming the result.', 'JamBoxFM has replied. Finishing the connection check.'],
    };
    ui['progress-title'].textContent = descriptions[stage][0];
    ui['progress-detail'].textContent = descriptions[stage][1];
    status(descriptions[stage][1]); controls();
  }
  function makeReport(outcome, result = null, error = null) {
    // Select known fields only. Raw exceptions/logs can include private port
    // names, so they are deliberately excluded from the downloadable report.
    return {
      format: 'jamboxfm-recovery-install-report', formatVersion: 1, recordedAt: timestamp(),
      experimental: true, hardwareValidated: false, automaticSaving: false,
      transition: safeTransition(result?.transition) || safeTransition(error?.transition) || safeTransition(observation?.transition),
      outcome, updateStarted: started(),
      sourceIdentity: safeIdentity(result?.sourceIdentity || error?.sourceIdentity || observation?.identity), finalIdentity: safeIdentity(result?.identity),
      unexpectedIdentity: safeIdentity({ text: error?.detail }),
      version: typeof bundle?.version === 'string' ? bundle.version : null,
      firmwareSHA256: /^[a-f0-9]{64}$/.test(bundle?.firmwareSHA256 || '') ? bundle.firmwareSHA256 : null,
      stages: stages.map(entry => ({ ...entry })), requestCounts: { ...requestCounts }, errorCode: error ? safeCode(error) : null,
    };
  }
  function showResult(outcome, result, error) {
    report = makeReport(outcome, result, error);
    ui['report-details'].textContent = JSON.stringify(report, null, 2);
    ui['result-card'].dataset.outcome = outcome; ui['result-card'].hidden = false;
    if (outcome === 'completed') {
      ui['result-title'].textContent = drumTrial ? 'The drum trial is on your JamBox.' : 'The saving trial is on your JamBox.';
      ui['result-eyebrow'].textContent = 'JAMBOXFM IDENTIFIED';
      ui['result-guidance'].textContent = drumTrial
        ? 'The FM-1 returned as JamBoxFM. This confirms its firmware response; drum navigation still needs your device check. Select Drums and tap SEQ, then follow the drum grid checklist below. Manual saving remains available and automatic saving is off.'
        : 'The FM-1 returned as JamBoxFM. This confirms its firmware response; saving still needs your device check. Start at low volume, check the controls, then follow the Save → restart checklist below. Automatic saving is off.';
      ui['progress-title'].textContent = 'JamBoxFM is running.';
      ui['progress-detail'].textContent = 'Transfer and firmware identity confirmed. Try the sound and controls on your instrument.';
      for (const id of ['stage-check', 'stage-loader', 'stage-write', 'stage-return']) {
        ui[id].dataset.state = 'done'; ui[id].removeAttribute('aria-current');
      }
      status('JamBoxFM returned successfully. Start with low volume when checking this build.');
    } else if (started()) {
      ui['result-title'].textContent = 'Installation needs a closer look.';
      ui['result-eyebrow'].textContent = 'UPDATE STOPPED';
      ui['result-guidance'].textContent = 'The update started, but a complete installation was not confirmed. Firmware may be partly written. Keep the FM-1 powered and connected. Download this report and share it with us before retrying, unplugging, or restarting the instrument.';
      ui['progress-title'].textContent = 'Installation not confirmed.';
      ui['progress-detail'].textContent = 'Keep the instrument connected while we review the report. This page will not retry the update.';
      status('Installation stopped. Keep power and USB connected, and download the report below.', true);
    } else {
      ui['result-title'].textContent = outcome === 'cancelled' ? 'Preparation cancelled.' : 'The update did not start.';
      ui['result-guidance'].textContent = beforeStartMessage(error);
      status(beforeStartMessage(error), outcome !== 'cancelled');
      ui['progress-card'].hidden = true;
    }
    ui['result-title'].focus({ preventScroll: true });
  }
  async function install() {
    const pair = selectedPair();
    if (!bundle || !pair || !observation?.eligible || pairs.length !== 1 || !backup || backupPending || !ui['disposable-jam'].checked || operation || started() || disposed) return;
    const current = { kind: 'install', abort: new AbortController() }; operation = current;
    stages = []; lastStage = null; report = null; requestCounts = { verify: 0, write: 0 };
    ui['result-card'].hidden = true; ui['progress-card'].hidden = false;
    step('checking'); ui['progress-title'].focus({ preventScroll: true }); controls();
    try {
      const result = await installer.install(bundle, pair, { signal: current.abort.signal, backup, disposableJamReady: ui['disposable-jam'].checked });
      if (disposed) return;
      if (safeIdentity(result?.identity) !== 'FM-1_700') throw Object.assign(new Error('Final identity was not confirmed'), { code: 'mismatch', started: started() });
      success = true; showResult('completed', result, null);
    } catch (error) {
      if (disposed) return;
      if (error?.started) attempted = true;
      showResult(error?.code === 'aborted' && !started() ? 'cancelled' : 'error', null, error);
      if (!started()) invalidateDevice();
    } finally { if (operation === current) operation = null; if (!disposed) controls(); }
  }
  function saveReport() {
    if (!report) return;
    const url = URL.createObjectURL(new Blob([JSON.stringify(report, null, 2) + '\n'], { type: 'application/json' }));
    const link = doc.createElement('a'); link.href = url; link.download = 'jamboxfm-recovery-install-report.json'; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  function dispose() {
    disposed = true; bundleAbort.abort();
    if (!started()) operation?.abort.abort();
    for (const remove of removers) remove();
  }
  async function backupChanged() {
    const generation = ++backupGeneration;
    backup = null; backupPending = false;
    const reportFile = ui['backup-report'].files?.[0], flashFile = ui['backup-flash'].files?.[0];
    ui['backup-state'].dataset.ready = 'false'; ui['backup-state'].dataset.error = 'false';
    ui['backup-label'].textContent = 'Choose both files from your existing recovery folder';
    if (!reportFile || !flashFile) { controls(); return; }
    backupPending = true; controls(); ui['backup-label'].textContent = 'Matching your local backup files…';
    try {
      if (reportFile.size <= 0 || reportFile.size > 128 * 1024 || flashFile.size !== 1048576)
        throw new Error('Unexpected backup size');
      const [reportBytes, flashBytes] = await Promise.all([reportFile.arrayBuffer(), flashFile.arrayBuffer()]);
      const proof = await checkBackup(new Uint8Array(reportBytes), new Uint8Array(flashBytes));
      if (disposed || generation !== backupGeneration) return;
      backup = proof; ui['backup-state'].dataset.ready = 'true';
      ui['backup-label'].textContent = 'Backup files match · local check only · nothing uploaded';
    } catch {
      if (disposed || generation !== backupGeneration) return;
      ui['backup-state'].dataset.error = 'true';
      ui['backup-label'].textContent = 'Backup files did not match. Choose backup.json and the 1 MiB full-flash-before.bin from the same recovery folder.';
    } finally { if (generation === backupGeneration) { backupPending = false; if (!disposed) controls(); } }
  }
  on(ui['backup-report'], 'change', backupChanged);
  on(ui['backup-flash'], 'change', backupChanged);
  on(ui['disposable-jam'], 'change', controls);
  on(ui['connect-device'], 'click', connect);
  on(ui['midi-pair'], 'change', () => { selectedKey = ui['midi-pair'].value; invalidateDevice(); controls(); status('Check the selected FM-1’s connection before installing.'); });
  on(ui['install-device'], 'click', install);
  on(ui['cancel-prepare'], 'click', () => { if (operation && !started()) operation.abort.abort(); });
  on(ui['reload-firmware'], 'click', prepareBundle);
  on(ui['save-report'], 'click', saveReport);
  on(doc.defaultView, 'beforeunload', event => { if (started() && !success) { event.preventDefault(); event.returnValue = ''; } });
  on(doc.defaultView, 'pagehide', dispose);
  if (!requestMIDIAccess) status('This browser doesn’t support Web MIDI. Open this page in Chrome or Edge on a computer using HTTPS.', true);
  controls(); prepareBundle();
  return { dispose };
}

if (typeof document !== 'undefined' && document.getElementById('install-device')) mountRecoveryPage();
