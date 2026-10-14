// SPDX-License-Identifier: GPL-3.0-only
// Deterministic sample-clock regression against the actual deliverable WASM.
// Build first: python3 tools/build_jam_emu.py
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const file = resolve(process.env.JAM_EMU_WASM || 'build/jam-emu/jam-emu.wasm');
const module = await WebAssembly.compile(await readFile(file));
assert.deepEqual(WebAssembly.Module.imports(module), [], 'firmware WASM has no runtime imports');
const w = new WebAssembly.Instance(module, {}).exports;
w._initialize?.();
w.jam_emu_init();
const frames = w.jam_emu_frames();
const rate = w.jam_emu_rate();
const value = (field, index = 0) => w.jam_emu_value(field, index);
const B = { SEL: 1, HOME: 6, PLAY: 10, REC: 11 };
const E = { SELECT: 0, K3: 5, K4: 6 };
let checks = 0;
function check(condition, message) { assert(condition, message); checks++; }
function render(n = 1) {
  while (n--) {
    const ptr = w.jam_emu_render();
    const audio = new Float32Array(w.memory.buffer, ptr, frames * 2);
    for (const sample of audio)
      assert(Number.isFinite(sample) && sample >= -1 && sample <= 1, 'WASM audio remains finite and bounded');
  }
}
function tap(button) {
  w.jam_emu_button(button, 1); render();
  w.jam_emu_button(button, 0); render();
}
function at(step) {
  const limit = Math.ceil(rate * 20 / frames);
  let n = 0;
  while ((value(26) >>> 0) !== step && n++ < limit) {
    const current = value(26) >>> 0;
    assert(current === 0xffffffff || current < step, `sequencer passed desired absolute step ${step} (at ${current})`);
    render();
  }
  assert.equal(value(26), step, `sequencer reaches absolute step ${step}`);
  // Move one eighth into this sixteenth-note step, avoiding endpoint rounding.
  render(Math.ceil(rate * 60 / value(8) / 4 / frames / 8));
  assert.equal(value(26), step, 'sample-clock fixture stays inside its requested step');
}
function length(step, slot = 0) { return value(23, step * 4 + slot); }
function sounding(note) { return value(24, note); }

check(frames === 32 && rate === 44100, 'deliverable uses the device audio block and sample rate');
check(value(0) === 1 && value(25) === 0, 'startup Chords part uses POLY');
// Change only through actual physical panel roles. Disable scale snap and the
// automatic chord builder so C and E are two independently held physical keys.
tap(B.SEL);
w.jam_emu_encoder(E.SELECT, -100); // First SEL page, even if remembered page changes.
w.jam_emu_encoder(E.K3, -100);
w.jam_emu_encoder(E.K4, -100);
tap(B.HOME);
check(value(6) === 1 && value(25) === 0, 'returning Home keeps Chords polyphonic with automatic chords disabled');

tap(B.PLAY); tap(B.REC);
check(value(1) === 1 && value(2) === 2, 'physical panel starts overdubbing the Chords track');
at(0); w.jam_emu_key(7, 1); // C4.
at(2); w.jam_emu_key(11, 1); // E4, while C is still down.
check(sounding(60) === 1 && sounding(64) === 1, 'actual WASM voices overlap when staggered E starts over held C');
at(4); w.jam_emu_key(11, 0);
check(sounding(60) === 1 && sounding(64) === 0, 'releasing live E preserves held C in WASM');
at(6); w.jam_emu_key(7, 0);
check(length(0) === 6 && length(2) === 2, 'WASM recording stores independent six-step C and two-step E durations');
check(value(19, 1) === 2, 'the selected pattern records exactly the two physical note onsets');

w.jam_emu_panic(); render(2); tap(B.PLAY);
check(value(1) === 1 && value(2) === 0, 'WASM transport restarts playback with recording disarmed');
at(0);
check(sounding(60) === 1 && sounding(64) === 0, 'WASM sequence begins with C alone');
at(2);
check(sounding(60) === 1 && sounding(64) === 1, 'WASM sequenced E starts while long C continues');
at(4);
check(sounding(60) === 1 && sounding(64) === 0, 'WASM E ends independently while sequenced C remains gated');
at(6);
check(sounding(60) === 0 && sounding(64) === 0, 'WASM long C ends at its independent endpoint');
check(length(0) === 6 && length(2) === 2, 'WASM playback leaves recorded durations unchanged');
w.jam_emu_panic(); render(2);
check(value(1) === 0 && value(2) === 0 && value(4) === 0 && sounding(60) === 0 && sounding(64) === 0,
      'WASM panic leaves no playing transport, held keys or gated test notes');
check(value(18) === 0, 'real WASM UI keeps LCD writes in bounds throughout the performance');
console.log(`Independent POLY deliverable WASM: ${checks} checks passed.`);
