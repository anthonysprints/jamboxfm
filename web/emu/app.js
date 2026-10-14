// SPDX-License-Identifier: GPL-3.0-only
// The panel sends physical inputs. All musical decisions and screen drawing are C.
const $ = id => document.getElementById(id);
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
const BUTTONS = ['FX', 'SEL', 'ENV', 'LFO', 'EDIT', 'GLO', 'HOME', 'SAVE', 'ARP', 'SEQ', 'PLAY', 'REC', 'OCT−', 'OCT+'];
const shortcuts = new Map(), labels = new Map(), owners = new Map();
const chars = { Comma: ',', Period: '.', Slash: '/', Semicolon: ';', Quote: "'", Minus: '−', Equal: '=', BracketLeft: '[', BracketRight: ']', Space: 'SPACE' };
function bind(code, type, index) {
  const target = `${type}:${index}`; shortcuts.set(code, target);
  labels.set(target, chars[code] || code.replace(/^(Key|Digit)/, ''));
}
['KeyZ', 'KeyX', 'KeyC', 'KeyV', 'KeyB', 'KeyN', 'KeyM', 'Comma', 'Period', 'Slash'].forEach((code, i) => bind(code, 'key', WHITE[i]));
[['KeyS', 1], ['KeyD', 3], ['KeyF', 5], ['KeyH', 8], ['KeyJ', 10], ['KeyL', 13], ['Semicolon', 15], ['Quote', 17]].forEach(([code, key]) => bind(code, 'key', key));
['KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT', 'KeyY', 'KeyU', 'KeyI', 'KeyO'].forEach((code, i) => bind(code, 'key', 18 + i));
['Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0', 'Minus', 'Equal', 'BracketLeft', 'BracketRight'].forEach((code, i) => bind(code, 'button', i));
shortcuts.set('Space', 'button:10');
const screen = $('screen').getContext('2d', { alpha: false });
screen.imageSmoothingEnabled = false;
function status(text, error = false) { $('status').textContent = text; $('status').classList.toggle('error', error); }
function setEnabled(on) {
  $('panel-controls').disabled = !on;
  for (const button of document.querySelectorAll('[data-key], [data-button]')) button.disabled = !on;
  for (const knob of document.querySelectorAll('.encoder')) knob.setAttribute('aria-disabled', String(!on));
  $('panic').disabled = !on;
}
function clearOwners() {
  owners.clear();
  for (const element of document.querySelectorAll('.is-active')) element.classList.remove('is-active');
}
function drawFrame(buffer) { screen.putImageData(new ImageData(new Uint8ClampedArray(buffer), 240, 240), 0, 0); }

let modulePromise;
function firmwareModule() {
  if (!modulePromise) modulePromise = (async () => {
    if (globalThis.JAMBOX_EMBEDDED) {
      const binary = atob(globalThis.JAMBOX_EMBEDDED.wasm);
      return WebAssembly.compile(Uint8Array.from(binary, c => c.charCodeAt(0)));
    }
    const response = await fetch(new URL('./jam-emu.wasm', import.meta.url));
    if (!response.ok) throw new Error('The instrument could not load. Reload and try again.');
    return WebAssembly.compile(await response.arrayBuffer());
  })();
  return modulePromise;
}

class EmuSession {
  constructor() {
    this.context = null; this.node = null; this.state = null; this.peak = 0;
    this.pending = []; this.inFlight = false; this.sequence = 0; this.scheduled = false;
    this.busy = false; this.failed = false; this.accepting = false;
  }
  async start() {
    if (this.failed) throw new Error('The audio engine stopped. Reload to start a fresh session.');
    if (this.node) { await this.context.resume(); this.accepting = true; setEnabled(true); return; }
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (!Audio || !window.AudioWorkletNode || !window.isSecureContext) throw new Error('Open this page in a current browser over HTTPS. Downloaded HTML also needs a browser that permits local audio worklets.');
    this.context = new Audio({ sampleRate: 44100, latencyHint: 'interactive' });
    if (this.context.sampleRate !== 44100) {
      await this.context.close(); this.context = null;
      throw new Error('This output cannot open at 44.1 kHz. Try another audio output.');
    }
    const resumed = this.context.resume();
    let workletURL;
    try {
      await resumed;
      const module = await firmwareModule();
      workletURL = globalThis.JAMBOX_EMBEDDED
        ? URL.createObjectURL(new Blob([globalThis.JAMBOX_EMBEDDED.worklet], { type: 'text/javascript' }))
        : new URL('./worklet.js', import.meta.url);
      await this.context.audioWorklet.addModule(workletURL);
      this.node = new AudioWorkletNode(this.context, 'jambox-emu', {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { module },
      });
      this.node.port.onmessage = ({ data }) => {
        if (data.error) { this.fail(data.error); return; }
        if (data.warning) { clearOwners(); this.pending.length = 0; status(data.warning, true); }
        if (data.state) { this.state = data.state; renderState(data.state); }
        if (data.peak != null) { this.peak = data.peak; $('meter').value = data.peak; }
        if (data.frame) {
          drawFrame(data.frame);
          this.node.port.postMessage({ type: 'frame-return', buffer: data.frame }, [data.frame]);
        }
        if (data.ack === this.sequence) { this.inFlight = false; this.schedule(); }
      };
      this.node.onprocessorerror = () => this.fail('The audio engine stopped. Reload to start a fresh session.');
      this.gain = this.context.createGain(); this.gain.gain.value = 0.6;
      this.node.connect(this.gain).connect(this.context.destination);
      this.accepting = true; setEnabled(true);
    } catch (error) {
      this.node?.disconnect(); await this.context?.close(); this.node = this.context = null;
      throw error;
    } finally {
      if (globalThis.JAMBOX_EMBEDDED && workletURL) URL.revokeObjectURL(workletURL);
    }
  }
  fail(text) {
    this.failed = true; this.accepting = false; this.pending.length = 0; this.inFlight = false;
    clearOwners(); setEnabled(false); this.node?.disconnect(); status(text, true);
  }
  command(type, args = []) {
    if (!this.node || !this.accepting || this.context.state !== 'running' || this.failed) return false;
    const previous = this.pending.at(-1);
    if (type === 'master' && previous?.type === type) previous.args = args;
    else if (type === 'encoder' && previous?.type === type && previous.args[0] === args[0] && Math.sign(previous.args[1]) === Math.sign(args[1])) previous.args[1] = Math.max(-64, Math.min(64, previous.args[1] + args[1]));
    else {
      if (this.pending.length >= 64) { this.panic(); status('Inputs arrived too quickly. Notes released; press Play to continue.', true); return false; }
      this.pending.push({ type, args });
    }
    this.schedule(); return true;
  }
  schedule() {
    if (this.scheduled || this.inFlight || !this.pending.length) return;
    this.scheduled = true;
    requestAnimationFrame(() => {
      this.scheduled = false;
      if (!this.node || this.inFlight || !this.pending.length) return;
      this.inFlight = true; this.sequence++;
      this.node.port.postMessage({ type: 'commands', sequence: this.sequence, commands: this.pending.splice(0, 16) });
    });
  }
  panic() {
    clearOwners(); this.pending.length = 0;
    if (!this.node || this.failed) return;
    this.inFlight = true; this.sequence++;
    this.node.port.postMessage({ type: 'panic', sequence: this.sequence });
  }
  async pause() {
    this.accepting = false; setEnabled(false);
    this.panic();
    // Wait for physical releases to reach C before freezing its audio clock.
    const deadline = performance.now() + 1500;
    while (this.inFlight && !this.failed && performance.now() < deadline) await new Promise(resolve => setTimeout(resolve, 10));
    if (this.inFlight) throw new Error('The instrument did not pause. Reload to start a fresh session.');
    await this.context.suspend(); setEnabled(false); $('meter').value = 0;
  }
}
const session = globalThis.jamEmu = new EmuSession();

function elementFor(target) {
  const [type, index] = target.split(':');
  return document.querySelector(`[data-${type}="${index}"]`);
}
function hold(owner, target) {
  if (owners.has(owner) || !session.node || session.context.state !== 'running' || session.failed) return;
  const held = [...owners.values()].includes(target);
  if (!held && !session.command(target.split(':')[0], [Number(target.split(':')[1]), 1])) return;
  owners.set(owner, target); elementFor(target)?.classList.add('is-active');
}
function release(owner) {
  const target = owners.get(owner); if (!target) return;
  owners.delete(owner);
  if (![...owners.values()].includes(target)) {
    const [type, index] = target.split(':'); session.command(type, [Number(index), 0]);
    elementFor(target)?.classList.remove('is-active');
  }
}
let clickSerial = 0;
function momentary(element, target) {
  element.addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    event.preventDefault(); element.setPointerCapture(event.pointerId); hold(`pointer:${event.pointerId}`, target);
  });
  element.addEventListener('pointerup', event => {
    release(`pointer:${event.pointerId}`);
    if (element.hasPointerCapture(event.pointerId)) element.releasePointerCapture(event.pointerId);
  });
  element.addEventListener('pointercancel', event => release(`pointer:${event.pointerId}`));
  element.addEventListener('lostpointercapture', event => release(`pointer:${event.pointerId}`));
  element.addEventListener('keydown', event => {
    if (event.code === 'Space' || event.code === 'Enter') {
      event.preventDefault(); event.stopPropagation(); hold(`accessible:${target}`, target);
    }
  });
  element.addEventListener('keyup', event => {
    if (event.code === 'Space' || event.code === 'Enter') {
      event.preventDefault(); event.stopPropagation(); release(`accessible:${target}`);
    }
  });
  element.addEventListener('blur', () => release(`accessible:${target}`));
  // Screen readers and programmatic click actions have no pointer edges.
  element.addEventListener('click', event => {
    if (event.detail === 0) {
      const owner = `click:${target}:${++clickSerial}`;
      hold(owner, target);
      if (target.startsWith('key:')) setTimeout(() => release(owner), 90);
      else release(owner);
    }
  });
}

BUTTONS.forEach((name, index) => {
  const button = document.createElement('button'); button.type = 'button'; button.className = 'panel-button'; button.dataset.button = index;
  button.setAttribute('aria-label', name); button.setAttribute('aria-pressed', 'false');
  button.innerHTML = `<span class="button-led" aria-hidden="true"></span><span class="control-label">${name}</span><span class="keyhint">${labels.get(`button:${index}`)}</span>`;
  momentary(button, `button:${index}`); $(index < 12 ? 'buttons' : 'octaves').append(button);
});
for (let key = 0; key < 27; key++) {
  const white = WHITE.indexOf(key), black = BLACK.indexOf(key);
  const button = document.createElement('button'); button.type = 'button';
  button.className = `device-key ${white >= 0 ? 'is-white' : 'is-black'}`; button.dataset.key = key;
  button.style.setProperty('--key-column', white >= 0 ? white * 2 + 1 : AFTER[black] * 2 + 2);
  const midi = 53 + key, name = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'][midi % 12] + (Math.floor(midi / 12) - 1);
  button.setAttribute('aria-label', `Key ${name}`);
  button.innerHTML = `<span class="key-led" aria-hidden="true"></span><span class="key-name">${name}</span><span class="keyhint">${labels.get(`key:${key}`)}</span>`;
  momentary(button, `key:${key}`); $('keys').append(button);
}

const knobValues = new Map();
function knob(name, role, parent, master = false) {
  const control = document.createElement('div'); control.className = 'encoder'; control.tabIndex = 0; control.role = 'slider';
  control.setAttribute('aria-label', name); control.setAttribute('aria-valuemin', '0'); control.setAttribute('aria-valuemax', '100'); control.setAttribute('aria-valuenow', master ? '75' : '50');
  if (master) control.dataset.master = ''; else control.dataset.encoder = role;
  control.innerHTML = `<span class="control-label">${name}</span><span class="knob"><span class="knob-indicator"></span></span><span class="keyhint">${master ? 'OUTPUT' : role >= 3 ? 'KNOB ' + (role - 2) : 'DRAG / SCROLL'}</span>`;
  let angle = 0, origin = null;
  const turn = amount => {
    if (!amount || !session.accepting || !session.node || session.context.state !== 'running' || session.failed) return;
    amount = Math.max(-32, Math.min(32, amount));
    if (master) {
      if (!session.command('masterStep', [amount])) return;
      const displayed = Math.max(0, Math.min(100, Number(control.getAttribute('aria-valuenow')) + amount));
      control.setAttribute('aria-valuenow', String(displayed)); angle = -135 + displayed * 2.7;
    } else { session.command('encoder', [role, amount]); angle += amount * 12; }
    control.querySelector('.knob').style.setProperty('--rotation', `${angle}deg`);
  };
  control.addEventListener('pointerdown', event => {
    if (event.button !== 0 || control.getAttribute('aria-disabled') === 'true') return;
    event.preventDefault(); control.focus(); control.setPointerCapture(event.pointerId); origin = { id: event.pointerId, y: event.clientY };
  });
  control.addEventListener('pointermove', event => {
    if (!origin || origin.id !== event.pointerId) return;
    const steps = Math.trunc((origin.y - event.clientY) / 6);
    if (steps) { turn(steps); origin.y -= steps * 6; }
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) control.addEventListener(type, event => { if (origin?.id === event.pointerId) origin = null; });
  window.addEventListener('blur', () => { origin = null; });
  document.addEventListener('visibilitychange', () => { if (document.hidden) origin = null; });
  control.addEventListener('wheel', event => { event.preventDefault(); turn(event.deltaY < 0 ? 1 : event.deltaY > 0 ? -1 : 0); }, { passive: false });
  control.addEventListener('keydown', event => {
    if (['ArrowUp', 'ArrowRight', 'ArrowDown', 'ArrowLeft'].includes(event.code)) {
      event.preventDefault(); event.stopPropagation(); turn(['ArrowUp', 'ArrowRight'].includes(event.code) ? 1 : -1);
    }
  });
  knobValues.set(master ? 'master' : role, control); $(parent).append(control);
}
knob('MASTER', -1, 'encoders', true); knob('SELECT', 0, 'encoders'); knob('PRESETS', 2, 'encoders'); knob('ALGORITHM', 1, 'encoders');
for (let i = 0; i < 4; i++) knob(['BRIGHT', 'MOVE', 'GRIT', 'SPACE'][i], 3 + i, 'macros');

function renderState(state) {
  for (let i = 0; i < 14; i++) {
    const element = elementFor(`button:${i}`);
    element.classList.toggle('is-on', state.leds[i] >= 128); element.classList.toggle('is-dim', state.leds[i] > 0 && state.leds[i] < 128);
    element.setAttribute('aria-pressed', String(!!(state.buttons & (1 << i))));
  }
  for (let key = 0; key < 27; key++) {
    const element = elementFor(`key:${key}`), led = state.leds[key + 14];
    element.classList.toggle('is-on', led >= 128); element.classList.toggle('is-dim', led > 0 && led < 128);
  }
  for (let i = 0; i < 4; i++) {
    const control = knobValues.get(3 + i), available = state.homeControls && state.macroAvailable;
    const name = available ? (state.selected === 3 ? ['TONE', 'LEVEL', 'ECHO', 'ROOM'] : ['BRIGHT', 'MOVE', 'GRIT', 'SPACE'])[i] : `KNOB ${i + 1}`;
    control.querySelector('.control-label').textContent = name; control.setAttribute('aria-label', name);
    control.setAttribute('aria-valuenow', String(available ? state.macros[i] : 50));
    control.setAttribute('aria-valuetext', available ? `${name} ${state.macros[i]} percent` : 'Turn to change the parameter shown on the screen');
    if (available) control.querySelector('.knob').style.setProperty('--rotation', `${-135 + state.macros[i] * 2.7}deg`);
  }
  knobValues.get(0).setAttribute('aria-valuetext', `Selected part: ${['Bass', 'Chords', 'Lead', 'Drums'][state.selected]}`);
  const tempoKnob = knobValues.get(1);
  tempoKnob.querySelector('.keyhint').textContent = state.homeControls ? 'HOME: BPM' : 'DRAG / SCROLL';
  tempoKnob.setAttribute('aria-valuemin', state.homeControls ? '40' : '0');
  tempoKnob.setAttribute('aria-valuemax', state.homeControls ? '240' : '100');
  tempoKnob.setAttribute('aria-valuenow', state.homeControls ? String(state.tempo) : '50');
  tempoKnob.setAttribute('aria-valuetext', state.homeControls ? `Tempo: ${state.tempo} BPM` : 'Controls follow the current screen');
  knobValues.get(2).setAttribute('aria-valuetext', state.sound);
  knobValues.get('master').setAttribute('aria-valuenow', String(state.master));
  knobValues.get('master').querySelector('.knob').style.setProperty('--rotation', `${-135 + state.master * 2.7}deg`);
}

window.addEventListener('keydown', event => {
  if (event.defaultPrevented || event.ctrlKey || event.metaKey || event.altKey || event.target.closest?.('input, select, textarea, [contenteditable="true"]')) return;
  if (event.code === 'Escape') { event.preventDefault(); session.panic(); return; }
  const target = shortcuts.get(event.code);
  if (target) { event.preventDefault(); if (!event.repeat) hold(`keyboard:${event.code}`, target); }
});
window.addEventListener('keyup', event => {
  if (owners.has(`keyboard:${event.code}`)) { event.preventDefault(); release(`keyboard:${event.code}`); }
});
window.addEventListener('blur', () => session.panic());
document.addEventListener('visibilitychange', () => { if (document.hidden) session.panic(); });
$('panic').addEventListener('click', () => { session.panic(); status('Stopped. Press PLAY when you’re ready.'); });
$('audio-start').addEventListener('click', async () => {
  if (session.busy) return;
  session.busy = true; $('audio-start').disabled = true;
  try {
    if (session.context?.state === 'running') {
      await session.pause(); $('audio-start').textContent = 'Resume audio'; status('Audio paused. Your session is still here.');
    } else {
      status('Starting audio…'); await session.start(); $('audio-start').textContent = 'Pause audio';
      status('Ready. Press PLAY for the beat, then try the keys.');
    }
  } catch (error) { setEnabled(false); status(error.message, true); }
  finally { session.busy = false; $('audio-start').disabled = session.failed; }
});

setEnabled(false);
(async () => {
  try {
    const module = await firmwareModule();
    const instance = new WebAssembly.Instance(module, {}), w = instance.exports;
    w._initialize?.(); w.jam_emu_init();
    drawFrame(new Uint8Array(w.memory.buffer, w.jam_emu_screen(), 240 * 240 * 4).slice().buffer);
    const master = w.jam_emu_value(16, 0);
    knobValues.get('master').setAttribute('aria-valuenow', String(master));
    knobValues.get('master').querySelector('.knob').style.setProperty('--rotation', `${-135 + master * 2.7}deg`);
    $('screen').dataset.ready = 'true';
    if (!session.context) status('Ready to switch on. No FM-1 connection needed.');
  } catch (error) { status(error.message, true); }
})();
