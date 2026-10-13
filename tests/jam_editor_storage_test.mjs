// SPDX-License-Identifier: GPL-3.0-only
// Execute the real editor parser and Save handler with refused-device replies.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const html = readFileSync(new URL('../web/editor.html', import.meta.url), 'utf8');
const proto = html.slice(html.indexOf('/*PROTO-BEGIN*/'), html.indexOf('/*PROTO-END*/'));
const api = vm.runInNewContext(proto + '\n;({parse, CMD, req})', {
  setTimeout, clearTimeout, setInterval, clearInterval, TextEncoder, TextDecoder,
  Blob, Response, DecompressionStream, console,
});
const parse = api.parse[api.CMD.PROJECT];
assert.equal(parse([1, 0, 1]).rc, 0); // Older three-byte success is still supported.
assert.equal(parse([1, 0, 1, 1]).busy, true);
assert.equal(parse([1, 0, 0, 6]).rc, 6);

const start = html.indexOf('async function project(op, slot) {');
const end = html.indexOf('/* ---------------------------------------------------------- steps', start);
const handler = html.slice(start, end);
const checkRc = html.match(/^const checkRc = .*$/m)?.[0];
assert.ok(start >= 0 && end > start && checkRc);
for (const [reply, expected] of [
  [[1, 0, 0, 6], 'storageDisabled'],
  [[1, 0, 1, 1], 'Stop playback'],
  [[1, 0, 1, 99], 'flashFail'],
  [[1, 0, 1], null],
]) {
  const events = [];
  const ctx = vm.createContext({
    ...api, dev: { slotUsed: [0, 0, 0, 0] }, busy: 0,
    t: key => key, confirm: () => true, rq: async () => reply,
    renderSlots: () => events.push('render'),
    afterSoundChange: async () => events.push('sound'),
    selfLoad: () => events.push('load'),
    sayK: key => events.push(key),
    report: err => events.push('error:' + err.message),
  });
  vm.runInContext(checkRc + '\n' + handler, ctx);
  await vm.runInContext('project(1, 0)', ctx);
  assert.equal(ctx.busy, 0);
  if (expected) {
    assert.equal(events.length, 1);
    assert.ok(events[0].startsWith('error:' + expected));
    assert.equal(ctx.dev.slotUsed[0], 0);
  } else {
    assert.deepEqual(events, ['render', 'savedSlot']);
    assert.equal(ctx.dev.slotUsed[0], 1);
  }
}
console.log('JamBox editor: refused Save never reports success or changes slots; busy/legacy replies preserved.');
