// SPDX-License-Identifier: GPL-3.0-only
// Run after build_jam_preview.py and compilation of jam_preview_native.c.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { JamEngine, CommandQueue } from '../web/jam/worklet.js';

const module = await WebAssembly.compile(readFileSync('build/jam-preview/jam-preview.wasm'));
assert.deepEqual(WebAssembly.Module.imports(module), [], 'firmware has no hardware/runtime imports');
function render(engine, frames = 8192) {
  const left = new Float32Array(frames), right = new Float32Array(frames);
  engine.render(left, right);
  let energy = 0;
  for (const channel of [left, right]) for (const x of channel) {
    assert(Number.isFinite(x) && Math.abs(x) <= 1, 'finite and bounded audio'); energy += Math.abs(x);
  }
  return { left, right, energy };
}
function command(engine, type, ...args) { engine.command({ type, args }); }
{
  const engine = new JamEngine(module), state = engine.snapshot();
  assert.equal(state.tempo, 88); assert.equal(state.playing, false); assert.equal(state.selected, 1);
  assert.deepEqual(state.tracks.map(t => t.track), [3, 0, 1, 2]);
  assert(state.tracks.every(t => t.soundCount === 2 && t.macros.length === 4));
  assert(state.steps.some(bits => bits & 1) && state.steps.some(bits => bits & 4));
  command(engine, 'key', 7, 1);
  assert.equal(engine.value(17, 0, 7), 4, 'local key expands the actual C chord');
  assert.deepEqual([0, 1, 2, 3].map(i => engine.value(18, 7, i)), [60, 63, 67, 70]);
  const a = render(engine, 4096); command(engine, 'key', 7, 0); const b = render(engine, 4096);
  assert(a.energy + b.energy > 1);
  const raw = execFileSync('build/host/jam_preview_native', ['--audio']);
  const native = new Float32Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength));
  const left = [...a.left, ...b.left], right = [...a.right, ...b.right];
  let error = 0;
  for (let i = 0; i < left.length; i++) error = Math.max(error, Math.abs(left[i] - native[2 * i]), Math.abs(right[i] - native[2 * i + 1]));
  assert(error <= 1 / 32768, `native/WASM chord samples match (${error})`);
  assert.equal(engine.value(12), 0);
  console.log(`native/WASM local chord audio matches: max delta ${error}`);
}
{
  const engine = new JamEngine(module);
  command(engine, 'record', 1); assert.equal(engine.value(20), 1);
  command(engine, 'key', 7, 1); render(engine, 14720); command(engine, 'key', 7, 0); render(engine, 14720);
  command(engine, 'key', 9, 1); render(engine, 14720); command(engine, 'key', 9, 0); command(engine, 'record', 0);
  assert.equal(engine.value(1), 1); assert.equal(engine.value(19), 0);
  const chordEvents = engine.value(21, 1); assert(chordEvents >= 8, 'two chords captured in firmware pattern');
  const chords = Array.from({ length: 64 }, (_, step) => Array.from({ length: engine.value(22, 1, step) }, (_, n) => engine.value(23, 1, step * 4 + n)));
  assert(chords.some(notes => notes.join() === '60,63,67,70'));
  assert(chords.some(notes => notes.length === 4 && notes[0] !== 60));
  command(engine, 'select', 0); command(engine, 'record', 1); command(engine, 'key', 7, 1);
  render(engine, 14720); command(engine, 'key', 7, 0); command(engine, 'record', 0);
  assert(engine.value(21, 0) > 0, 'bass capture uses the same recorder');
  assert.equal(engine.value(21, 1), chordEvents, 'bass capture preserves chord pattern');
  command(engine, 'panic'); render(engine, 44100 * 5);
  for (const track of [0, 2, 3]) command(engine, 'mute', track, 1);
  assert(render(engine, 44100).energy < 0.1);
  command(engine, 'play', 1);
  assert(render(engine, 44100 * 3).energy > 1, 'recorded chords play with no keys held');
  command(engine, 'record', 1); command(engine, 'key', 7, 1); command(engine, 'select', 2);
  assert.equal(engine.value(12), 0); assert.equal(engine.value(19), 0, 'part switch ends take and held input');
  console.log('WASM: two chords, bass overdub, record finish, replay and part-switch release passed');
}
{
  const engine = new JamEngine(module), before = engine.snapshot().steps[0];
  command(engine, 'step', 0, 7); assert.equal(engine.snapshot().steps[0], before ^ (1 << 7));
  command(engine, 'step', 0, 7); assert.equal(engine.snapshot().steps[0], before);
  command(engine, 'macro', 1, 0, 999); assert.equal(engine.snapshot().tracks[2].macros[0].value, 100);
  command(engine, 'macro', 1, 0, -100); assert.equal(engine.snapshot().tracks[2].macros[0].value, 0);
  command(engine, 'sound', 1, 1); assert.equal(engine.snapshot().tracks[2].soundIndex, 1);
  command(engine, 'play', 1); assert(render(engine, 44100).energy > 1, 'ready beat audible');
  command(engine, 'key', 7, 1);
  const queue = new CommandQueue(4);
  assert.equal(queue.add(Array.from({ length: 5 }, () => ({ type: 'tempo', args: [120] }))), false);
  queue.drain(engine); render(engine, 32);
  assert.equal(engine.value(12), 0); assert.equal(engine.value(1), 0, 'overflow stops/releases instead of losing note-offs');
  assert.equal(queue.count, 0);
  queue.add([{ type: 'tempo', args: [110] }, { type: 'swing', args: [35] }]); queue.drain(engine, 1);
  assert.equal(queue.count, 1); assert.equal(engine.value(2), 110); queue.drain(engine, 1); assert.equal(engine.value(3), 35);
  command(engine, 'togglePlay'); command(engine, 'togglePlay'); render(engine, 32);
  assert.equal(engine.value(1), 0, 'two toggles before a render cancel pending start');
  command(engine, 'toggleRecord'); command(engine, 'toggleRecord'); assert.equal(engine.value(20), 0);
  command(engine, 'toggleMute', 0); command(engine, 'toggleMute', 0); assert.equal(engine.value(5, 0), 0);
  const sound = engine.w.jam_sound_index(0);
  command(engine, 'soundStep', 0, 1); command(engine, 'soundStep', 0, 1); assert.equal(engine.w.jam_sound_index(0), sound);
  command(engine, 'octaveStep', 1); command(engine, 'octaveStep', 1); assert.equal(engine.value(4), 2);
  console.log('WASM: drum edits, macro bounds, sound browsing, ready beat and bounded event queue passed');
}
console.log('JamBox WASM contract tests passed');
