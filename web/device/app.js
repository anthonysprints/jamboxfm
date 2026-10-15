// SPDX-License-Identifier: GPL-3.0-only
// Read-only device inventory. Transport and family classification live in the
// shared modules; this page never calls an installer or sends its own packets.
import { Updater, FM1_PORT } from '../fm1ota.js';
import { inspectDevice } from '../jam-device-policy.js';

const FAMILIES = { jamboxfm: 'JamBoxFM', sloop: 'Sloop', unknown: 'Unrecognized firmware' };
const MODES = { normal: 'Running normally', rescue: 'Recovery mode', loader: 'Update loader', unknown: 'Not confirmed' };
const CAPABILITIES = ['updater', 'temporary', 'appRescue', 'boundedLoader', 'readOnlyCalibration'];
const safeText = (value, length = 120) => typeof value === 'string' ? value.slice(0, length) : null;
const reportsFactoryV15 = report => !report.recognized && report.identity.text === 'FM-1_015' &&
  report.identity.model === 'FM-1' && report.identity.version === 15;

export function deviceReport(observation, checkedAt = new Date()) {
  const classification = observation.capabilities || {};
  const identity = observation.identity || observation.id || {};
  return {
    format: 'jamboxfm-device-report', formatVersion: 1,
    checkedAt: checkedAt.toISOString(),
    identity: {
      text: safeText(identity.text, 40), model: safeText(identity.model, 32),
      version: Number.isSafeInteger(identity.version) ? identity.version : null,
    },
    family: Object.hasOwn(FAMILIES, classification.family) ? classification.family : 'unknown',
    mode: Object.hasOwn(MODES, classification.mode) ? classification.mode : 'unknown',
    release: safeText(classification.release, 40), recognized: classification.recognized === true,
    reason: safeText(classification.reason, 240),
    capabilities: Object.fromEntries(CAPABILITIES.map(key => [key, classification.capabilities?.[key] === true])),
    hardwareValidated: false, installAllowed: false,
  };
}

function guidance(report) {
  if (reportsFactoryV15(report))
    return 'Your FM-1 reports the factory V15 identity. Its firmware and update capabilities have not been independently verified. “False” capability fields in the report mean this checker has not confirmed them. Keep your factory firmware file and any saved sounds/settings; this report is identification only, not a backup. JamBox installation remains unavailable.';
  if (report.mode === 'loader' || report.mode === 'rescue')
    return 'The device reports a recovery or update mode. Save this report and follow the recovery instructions for the firmware already on it. Backups may require that firmware to be running normally. This page leaves the current mode alone.';
  if (report.recognized && report.family === 'sloop')
    return 'Your FM-1 reports Sloop. Use Sloop’s own editor to back up projects, presets, samples, and settings before any firmware change. Keep that backup alongside this report.';
  if (report.recognized && report.family === 'jamboxfm')
    return 'Your FM-1 reports JamBoxFM. Check SAVE on the instrument: current builds offer manual saving; older builds use temporary sessions. Keep your recovery backup. This identification report does not back up your music.';
  return 'We couldn’t confirm this firmware family. Save the report and check which firmware is installed before making changes. Use that firmware’s own backup tools for your music; this report is identification only.';
}

function errorText(error) {
  if (error?.name === 'NotAllowedError' || error?.name === 'SecurityError' || error?.code === 'permission')
    return 'MIDI access wasn’t allowed. Allow MIDI and system exclusive access in your browser’s site settings, then try again.';
  if (error?.code === 'lost') return 'The FM-1 disconnected. Reconnect it, choose its matching ports, and read again.';
  if (error?.code === 'ambiguous') return 'More than one port matches. Choose the input and output belonging to the same FM-1, or unplug the other devices, then try again.';
  if (error?.code === 'identity') return 'No valid FM-1 identity was received. Check the selected ports and USB cable, close other MIDI apps, and try again.';
  if (error?.code === 'notfound') return 'The selected FM-1 ports are no longer available. Reconnect the device and choose both ports again.';
  return 'The device couldn’t be read. Check its USB connection, close other MIDI apps, and try again.';
}

// Dependencies are injectable for browser tests. The production entry uses
// real Web MIDI and the same read-only transport used by the shared policy.
export function mountDevicePage({
  document: doc = document,
  requestMIDIAccess = typeof navigator.requestMIDIAccess === 'function' ? navigator.requestMIDIAccess.bind(navigator) : null,
  createUpdater = access => new Updater(access),
  inspect = inspectDevice,
  now = () => new Date(),
} = {}) {
  const get = id => doc.getElementById(id);
  const ui = Object.fromEntries(['midi-enable', 'port-fields', 'midi-input', 'midi-output', 'pair-hint',
    'read-device', 'cancel-read', 'status', 'result', 'result-title', 'result-badge',
    'device-family', 'device-version', 'device-mode', 'result-guidance', 'save-report', 'report-details'].map(id => [id, get(id)]));
  let access = null, updater = null, controller = null, generation = 0, report = null, disposed = false;
  let inputPorts = [], outputPorts = [];
  const listeners = [];
  const on = (target, type, fn) => { target.addEventListener(type, fn); listeners.push(() => target.removeEventListener(type, fn)); };
  const status = (message, error = false) => { ui.status.textContent = message; ui.status.dataset.tone = error ? 'error' : 'info'; };
  const clearReport = () => { report = null; ui.result.hidden = true; ui['report-details'].textContent = ''; };

  function selectedPair() {
    const input = inputPorts.find(port => port.id === ui['midi-input'].value && port.state !== 'disconnected');
    const output = outputPorts.find(port => port.id === ui['midi-output'].value && port.state !== 'disconnected');
    return input && output && input.name === output.name ? { inputId: input.id, outputId: output.id } : null;
  }
  function updateControls() {
    const reading = !!controller;
    ui['port-fields'].disabled = !access || reading;
    ui['midi-enable'].disabled = !requestMIDIAccess || reading;
    ui['read-device'].disabled = reading || !selectedPair();
    ui['read-device'].textContent = reading ? 'Reading device…' : 'Read device →';
    ui['cancel-read'].hidden = !reading || controller.signal.aborted;
    ui['read-device'].setAttribute('aria-busy', String(reading));
  }
  function pairHint() {
    if (!access) return;
    if (!inputPorts.length || !outputPorts.length) {
      ui['pair-hint'].textContent = 'No matching FM-1 input and output are available. Check the USB data cable, then refresh the ports.';
    } else if (selectedPair()) {
      const name = inputPorts.find(port => port.id === ui['midi-input'].value).name;
      const duplicate = inputPorts.filter(port => port.name === name).length > 1 || outputPorts.filter(port => port.name === name).length > 1;
      ui['pair-hint'].textContent = duplicate ? 'Several ports have this name. Confirm these belong to the same FM-1; unplug the other devices if you’re unsure.' : 'Matching ports selected. Read device sends identification queries only.';
    } else if (ui['midi-input'].value && ui['midi-output'].value) {
      ui['pair-hint'].textContent = 'These port names don’t match. Select the input and output for the same FM-1.';
    } else {
      ui['pair-hint'].textContent = 'Choose both ports with the same name. If several look identical, unplug the other devices first.';
    }
  }
  function refreshPorts() {
    const oldInput = ui['midi-input'].value, oldOutput = ui['midi-output'].value;
    const candidates = ports => [...ports.values()].filter(port => port.state !== 'disconnected' && FM1_PORT.test(port.name || ''));
    inputPorts = candidates(access.inputs); outputPorts = candidates(access.outputs);
    for (const [id, ports, selected, placeholder] of [
      ['midi-input', inputPorts, oldInput, 'Choose an input'], ['midi-output', outputPorts, oldOutput, 'Choose an output'],
    ]) {
      const select = ui[id];
      select.replaceChildren(new Option(placeholder, ''));
      for (const [index, port] of ports.entries()) {
        const duplicates = ports.filter(other => other.name === port.name).length > 1;
        select.add(new Option(duplicates ? `${port.name} (${id === 'midi-input' ? 'input' : 'output'} ${index + 1})` : port.name, port.id));
      }
      if (ports.some(port => port.id === selected)) select.value = selected;
    }
    const disconnected = (oldInput && !inputPorts.some(port => port.id === oldInput)) || (oldOutput && !outputPorts.some(port => port.id === oldOutput));
    if (disconnected) {
      generation++;
      controller?.abort();
      clearReport();
      status('The FM-1 disconnected. Reconnect it, choose its matching ports, and read again.', true);
    }
    pairHint(); updateControls();
  }
  async function enable() {
    if (!requestMIDIAccess || disposed) return;
    if (access) { refreshPorts(); return; }
    const token = ++generation;
    ui['midi-enable'].disabled = true;
    status('Waiting for MIDI permission…');
    try {
      const granted = await requestMIDIAccess({ sysex: true });
      if (disposed || token !== generation) return;
      access = granted; updater = createUpdater(access);
      on(access, 'statechange', refreshPorts);
      ui['midi-enable'].textContent = 'Refresh ports';
      refreshPorts();
      status(inputPorts.length && outputPorts.length ? 'MIDI is ready. Choose your FM-1’s matching input and output.' : 'MIDI is ready. Connect an FM-1 to continue.');
    } catch (error) {
      if (!disposed && token === generation) status(errorText(error), true);
    } finally {
      if (!disposed && token === generation) updateControls();
    }
  }
  function renderResult(observation) {
    report = deviceReport(observation, now());
    const factoryReported = reportsFactoryV15(report);
    ui['device-family'].textContent = factoryReported ? 'Factory FM-1 (reported)' : FAMILIES[report.family];
    const reportedVersion = report.identity.text?.match(/_(\d+)$/)?.[1];
    ui['device-version'].textContent = report.release || (reportedVersion ? `${reportedVersion} (reported)` : 'Not confirmed');
    ui['device-mode'].textContent = MODES[report.mode];
    ui['result-title'].textContent = factoryReported ? 'Factory V15 identity reported.' : report.recognized ? 'Here’s your FM-1.' : 'Firmware not confirmed.';
    ui['result-badge'].textContent = factoryReported ? 'REPORTED' : report.recognized ? 'IDENTIFIED' : 'UNKNOWN';
    ui['result-guidance'].textContent = guidance(report);
    ui['report-details'].textContent = JSON.stringify(report, null, 2);
    ui.result.hidden = false;
    ui['result-title'].focus({ preventScroll: true });
    status('Read complete. You can save the identification report below.');
  }
  async function read() {
    const pair = selectedPair();
    if (!pair || !updater || controller || disposed) return;
    const token = ++generation;
    const operation = new AbortController(); controller = operation;
    clearReport(); updateControls(); status('Reading firmware identity…');
    try {
      const observation = await updater.identify({ pair, signal: operation.signal,
        query: (link, identity) => inspect(identity, link, { signal: operation.signal }) });
      if (!disposed && token === generation && !operation.signal.aborted) renderResult(observation);
    } catch (error) {
      if (!disposed && token === generation && !operation.signal.aborted) status(errorText(error), true);
    } finally {
      // Wait for the transport to close its ports before enabling another read.
      // A cancelled operation must not close ports opened by a newer operation.
      if (!disposed && controller === operation) { controller = null; updateControls(); }
    }
  }
  function cancel() {
    if (!controller) return;
    generation++; controller.abort();
    status('Reading cancelled. The device was left unchanged.'); updateControls();
  }
  function changePair() {
    generation++; controller?.abort();
    clearReport(); pairHint(); updateControls();
    status(selectedPair() ? 'Ready to read the selected FM-1.' : 'Choose your FM-1’s matching input and output.');
  }
  function saveReport() {
    if (!report) return;
    const url = URL.createObjectURL(new Blob([JSON.stringify(report, null, 2) + '\n'], { type: 'application/json' }));
    const link = doc.createElement('a');
    link.href = url; link.download = 'jamboxfm-device-report.json'; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  function dispose() {
    disposed = true; generation++; controller?.abort(); controller = null;
    for (const remove of listeners) remove();
  }
  on(ui['midi-enable'], 'click', enable);
  on(ui['read-device'], 'click', read);
  on(ui['cancel-read'], 'click', cancel);
  on(ui['midi-input'], 'change', changePair);
  on(ui['midi-output'], 'change', changePair);
  on(ui['save-report'], 'click', saveReport);
  on(doc.defaultView, 'pagehide', dispose);
  if (!requestMIDIAccess) {
    status('This browser doesn’t support Web MIDI. Open this page in Chrome or Edge on a computer, using HTTPS, to check your FM-1.', true);
    ui['pair-hint'].textContent = 'A browser with Web MIDI support is required.';
  }
  updateControls();
  return { dispose };
}

if (typeof document !== 'undefined' && document.getElementById('read-device')) mountDevicePage();
