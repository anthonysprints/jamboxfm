// SPDX-License-Identifier: GPL-3.0-only
// All 44 curated choices through the actual deliverable's physical panel ABI.
// The native reference cold-boots for each choice, exactly like each WASM
// instance. This catches browser-only engine loads and adapter discrepancies;
// native bank tests separately cover pattern preservation and long FX tails.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';

const file = resolve(process.env.JAM_EMU_WASM || 'build/jam-emu/jam-emu.wasm');
const bytes = await readFile(file);
const module = await WebAssembly.compile(bytes);
assert.deepEqual(WebAssembly.Module.imports(module), [], 'deliverable needs no runtime imports');
const nativePath = resolve(process.env.JAM_PRESETS_NATIVE || 'build/jam-emu-evidence/presets-native');
const role = [1, 2, 3, 0]; // Visual part position: Drums, Bass, Chords, Lead.
const bank = [
  [
    ['ROUND BASS', 0], ['MUTED BASS', 0], ['SUB BASS', 0], ['808 BOOM', 0],
    ['808 DIRTY', 0], ['FUNK BASS', 0], ['SAW BASS', 0], ['REESE', 0],
    ['ACID 303', 0], ['FM BASS', 1], ['FINGER BASS', 1], ['CZ BASS', 2],
  ],
  [
    ['DUST KEYS', 0], ['SOFT STABS', 0], ['RHODES', 1], ['WURLI', 1],
    ['CLAV', 1], ['DUSTY PNO', 4], ['ROCK ORGAN', 7], ['SOUL ORGAN', 7],
    ['WARM PAD', 0], ['SYN BRASS', 0], ['SOFT PAD', 9], ['NYLON PICK', 9],
  ],
  [
    ['MELLOW LEAD', 0], ['DUST REED', 0], ['FAT LEAD', 0], ['PWM LEAD', 0],
    ['G-FUNK LD', 0], ['NEON SAW', 0], ['CZ LEAD', 2], ['RESO PLUCK', 2],
    ['MUSIC BOX', 1], ['LOFI FLUTE', 4], ['VIBES', 4], ['GLASS BELL', 9],
  ],
  [
    ['DUST KIT', -1], ['SOFT KIT', -1], ['ROCK KIT', -1], ['TIGHT KIT', -1],
    ['808 KIT', -1], ['909 KIT', -1], ['TRAP KIT', -1], ['NEON KIT', -1],
  ],
];
let checks = 0, maxError = 0;
const evidence = [];
function equal(a, b, message) { assert.deepEqual(a, b, message); checks++; }
function check(condition, message) { assert(condition, message); checks++; }
function instance(track) {
  const w = new WebAssembly.Instance(module, {}).exports;
  w._initialize?.();
  w.jam_emu_init();
  equal([w.jam_emu_frames(), w.jam_emu_rate()], [32, 44100], 'device block size and rate');
  equal([w.jam_emu_value(0, 0), w.jam_emu_value(21, 0)], [1, 0], 'cold boot on Chords / Groove Home');
  w.jam_emu_encoder(1, role[track] - 2);
  equal(w.jam_emu_value(0, 0), track, 'physical ALGORITHM selects the requested part');
  return w;
}
function name(w) {
  const memory = new Uint8Array(w.memory.buffer);
  const start = w.jam_emu_sound_name();
  let end = start;
  while (end < memory.length && memory[end]) end++;
  check(end - start > 0 && end - start <= 12, 'curated sound label fits the panel');
  return new TextDecoder().decode(memory.subarray(start, end));
}
function audio(w) {
  const ptr = w.jam_emu_render();
  const samples = new Float32Array(w.memory.buffer, ptr, 64);
  for (const sample of samples)
    assert(Number.isFinite(sample) && Math.abs(sample) <= 1, 'WASM samples are finite and bounded');
  return samples;
}
function expected(w, sound) {
  equal(name(w), sound[0], 'physical PRESETS selects the advertised name');
  equal(w.jam_emu_value(15, 0), sound[1], 'the correct engine was requested');
  equal(w.jam_emu_value(13, 0), 1, 'Sound Home has usable macros');
}

for (let track = 0; track < 4; track++) {
  for (let preset = 0; preset < bank[track].length; preset++) {
    const sound = bank[track][preset], w = instance(track);
    w.jam_emu_encoder(2, preset);
    w.jam_emu_frame();
    expected(w, sound);
    for (let block = 0; block < 32; block++) audio(w);
    const key = track === 3 ? 0 : 7;
    w.jam_emu_key(key, 1);
    const rendered = new Float32Array(256 * 64);
    for (let block = 0; block < 256; block++) {
      if (block === 128) w.jam_emu_key(key, 0);
      rendered.set(audio(w), block * 64);
    }
    const raw = execFileSync(nativePath, [String(track), String(preset), sound[0], String(sound[1])]);
    equal(raw.byteLength, rendered.byteLength, 'native capture has exactly the expected duration');
    const native = new Float32Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength));
    let energy = 0, error = 0;
    for (let sample = 0; sample < rendered.length; sample++) {
      assert(Number.isFinite(native[sample]) && Math.abs(native[sample]) <= 1, 'native sample is finite and bounded');
      energy += Math.abs(rendered[sample]);
      error = Math.max(error, Math.abs(native[sample] - rendered[sample]));
    }
    check(energy > 0.01, `${sound[0]} is audible on a physical key with transport stopped`);
    check(error <= 1 / 32768, `${sound[0]} native / WASM audio agrees within one output quantization unit (${error})`);
    maxError = Math.max(maxError, error);
    equal([w.jam_emu_value(4, 0), w.jam_emu_value(1, 0)], [0, 0], 'capture releases its key and never starts transport');

    // The same real knobs must remain playable for the non-ANALOG engines.
    // Switch Home views physically; Groove knobs intentionally edit the track.
    w.jam_emu_button(6, 1); w.jam_emu_button(6, 0); w.jam_emu_frame();
    equal(w.jam_emu_value(21, 0), 1, 'HOME switches to Sound');
    for (let macro = 0; macro < 4; macro++) {
      w.jam_emu_encoder(3 + macro, -1024);
      equal(w.jam_emu_value(12, macro), 0, 'physical sound knob reaches its low endpoint');
      w.jam_emu_encoder(3 + macro, 1024);
      equal(w.jam_emu_value(12, macro), 100, 'physical sound knob reaches its high endpoint');
      audio(w);
    }
    w.jam_emu_frame();
    equal(w.jam_emu_value(18, 0), 0, 'engine and macro redraws never write outside the physical LCD');
    evidence.push({ track, preset, name: sound[0], engine: sound[1], absoluteSampleSum: energy, maxNativeWasmDelta: error });
  }
  // Complete clockwise / counterclockwise wrap, including the new last entry.
  const w = instance(track), sounds = bank[track];
  expected(w, sounds[0]);
  for (let preset = 1; preset < sounds.length; preset++) {
    w.jam_emu_encoder(2, 1); expected(w, sounds[preset]);
  }
  w.jam_emu_encoder(2, 1); expected(w, sounds[0]);
  w.jam_emu_encoder(2, -1); expected(w, sounds.at(-1));
  w.jam_emu_encoder(2, sounds.length * 3 + 1); expected(w, sounds[0]);
  w.jam_emu_frame();
  equal(w.jam_emu_value(18, 0), 0, 'complete-bank browsing stays inside LCD bounds');
}
equal(evidence.length, 44, 'all 36 melodic presets and eight drum kits were played');
const out = resolve('build/jam-emu-evidence');
await mkdir(out, { recursive: true });
await writeFile(resolve(out, 'presets-wasm.json'), JSON.stringify({
  wasmSha256: createHash('sha256').update(bytes).digest('hex'),
  sampleRate: 44100, framesPerSound: 8192, checks, maxNativeWasmDelta: maxError, sounds: evidence,
}, null, 2) + '\n');
console.log(`Curated presets deliverable WASM: 44 sounds, ${checks} checks; native / WASM max delta ${maxError}.`);
