// Research probe of Sloop's checked-in browser artifact, not an FM-1 hardware test.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const root = resolve(process.argv[2] || '/workspace/scratch/fm1-research/sloop');
const asset = join(root, 'docs/webapp/editor/sloop-preview.wasm');
const bytes = readFileSync(asset);
const sha256 = createHash('sha256').update(bytes).digest('hex');
assert.equal(sha256, '55e2b480ebc689664918f331cbb74b94c78f10b006a06c8e5807c9975ff69ffe', 'Pinned research artifact');
const { FirmwareAudio } = await import(pathToFileURL(join(root, 'docs/webapp/editor/preview-worklet.js')));
const module = await WebAssembly.compile(bytes);
assert.deepEqual(WebAssembly.Module.imports(module), [], 'No external imports');
const checks = [];

function fresh() {
  const dsp = new FirmwareAudio(module);
  assert.equal(dsp.w.preview_rate(), 44100);
  // P_AMODE=17, P_MUTE=40; use defaults with arpeggiation disabled.
  for (let part = 0; part < 3; part++) {
    dsp.w.preview_track(part, 0, 0);
    dsp.w.preview_param(part, 17, 0);
    dsp.w.preview_param(part, 40, 0);
  }
  return dsp;
}

function render(dsp, frames = 44100) {
  const left = new Float32Array(frames), right = new Float32Array(frames);
  dsp.render(left, right);
  let energy = 0, peak = 0;
  for (let i = 0; i < frames; i++) {
    for (const value of [left[i], right[i]]) {
      assert(Number.isFinite(value) && Math.abs(value) <= 1, 'Finite, bounded output');
      energy += Math.abs(value);
      peak = Math.max(peak, Math.abs(value));
    }
  }
  return { frames, energy, peak };
}

for (const [name, notes] of [
  ['bass', [[0, 36]]],
  ['four-note chord', [[1, 60], [1, 64], [1, 67], [1, 71]]],
  ['lead', [[2, 76]]],
  ['drum kick', [[3, 36]]],
  ['combined roles', [[0, 36], [1, 60], [1, 64], [1, 67], [1, 71], [2, 76], [3, 36], [3, 42]]],
]) {
  const dsp = fresh();
  for (const [part, note] of notes) dsp.w.preview_note(part, note, 1);
  const result = render(dsp);
  assert(result.energy > 0.01, `${name} produces output`);
  for (const [part, note] of notes) dsp.w.preview_note(part, note, 0);
  dsp.w.preview_panic();
  render(dsp, 44100 * 8);
  const stopped = render(dsp);
  assert(stopped.energy < 0.001, `${name}: panic releases notes and tails`);
  checks.push({ name, ...result, stopped_energy: stopped.energy });
}

console.log(JSON.stringify({
  artifact: asset, sha256, bytes: bytes.length,
  imports: WebAssembly.Module.imports(module),
  exports: WebAssembly.Module.exports(module).map(({ name }) => name),
  checks,
  result: 'passed',
  limits: [
    'Node execution of the checked-in WebAssembly and adapter, not a source rebuild.',
    'No AudioContext, browser UI, MIDI device, target CPU timing, flash or USB hardware tested.',
    'Combined output does not prove every requested voice survived allocation.',
    'Uses existing analog preset defaults; does not audition or implement JamBoxFM palettes.',
  ],
}, null, 2));
