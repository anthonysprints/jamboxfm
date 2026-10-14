// SPDX-License-Identifier: GPL-3.0-only
// Execute the shipped protocol, including old replies and modern round trips.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
const html = readFileSync(new URL('../web/editor.html', import.meta.url), 'utf8');
const proto = html.slice(html.indexOf('/*PROTO-BEGIN*/'), html.indexOf('/*PROTO-END*/'));
const { parse, CMD, req } = vm.runInNewContext(proto + '\n;({parse, CMD, req})', {
  setTimeout, clearTimeout, setInterval, clearInterval, TextEncoder, TextDecoder,
  Blob, Response, DecompressionStream, console,
});
const plain = x => x === undefined ? x : JSON.parse(JSON.stringify(x));
let checks = 0;
const eq = (a,b) => { assert.deepEqual(plain(a),b); checks++; };
const old = [3, 2, 60, 64, 0, 0, 0, 1, 91];
eq(parse[CMD.STEP_GET](old).noteLen, undefined);
eq(req.stepSet(3, parse[CMD.STEP_GET](old))[1], old);
const v5 = [...old, 4, 3, 12];
eq(parse[CMD.STEP_GET](v5).lvl, 132);
eq(parse[CMD.STEP_GET](v5).rat, 140);
eq(req.stepSet(3, parse[CMD.STEP_GET](v5))[1], v5);
const modern = [...v5, 8, 2, 0, 0];
const parsed = parse[CMD.STEP_GET](modern);
eq(parsed.noteLen, [8,2,0,0]);
eq(req.stepSet(3, parsed)[1], modern);
eq(parse[CMD.TRACK_STEP]([1,...modern]).noteLen, [8,2,0,0]);
eq(req.trackStep(1,3,parsed)[1], [1,...modern]);
eq(req.stepSet(3,{...parsed,vel:103})[1].slice(-4), [8,2,0,0]);
eq(req.stepSet(3,{...parsed,noteLen:[-1,127,4,4]})[1].slice(-4), [0,0,0,0]);
eq(req.stepSet(3,{...parsed,noteLen:[64,1]})[1].slice(-4), [64,1,0,0]);
eq(req.stepSet(3,{...parsed,time:2})[1].slice(-4), [0,0,0,0]);
eq(parse[CMD.STEP_GET]([...v5,127,64,8,8]).noteLen, [0,64,0,0]);
const withoutDynamics = {...parsed}; delete withoutDynamics.lvl; delete withoutDynamics.rat;
eq(req.stepSet(3,withoutDynamics)[1].slice(-7), [0,0,0,8,2,0,0]);
eq(parse[CMD.STEP_GET]([...v5,8,2]).noteLen, undefined);
// Drum replies retain the existing v5 shape, with no synthetic duration field.
eq(parse[CMD.STEP_GET](v5).noteLen, undefined);
console.log(`JamBox editor note lengths: ${checks} protocol checks passed.`);
