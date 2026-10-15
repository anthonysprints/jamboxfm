// SPDX-License-Identifier: GPL-3.0-only
// Production WASM, AudioWorklet and physical panel controls. Build first with
// tools/build_jam_emu.py. No private parameter/debug exports are used: native
// sound-page tests verify exact targeting and preserved saved values.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { extname, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

const playwright = process.env.PLAYWRIGHT_MODULE
  ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve('build/jam-emu');
const evidence = resolve('build/jam-sound-pages-browser-evidence');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    if (pathname === '/favicon.ico') { response.writeHead(204); response.end(); return; }
    const path = resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!path.startsWith(root + sep)) throw new Error('Outside emulator');
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' });
    response.end(await readFile(path));
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(done => server.listen(0, '127.0.0.1', done));
const address = `http://127.0.0.1:${server.address().port}`;
const errors = [];
let browser;

const state = page => page.evaluate(() => globalThis.jamEmu.state);
const idle = page => page.waitForFunction(() => globalThis.jamEmu.state &&
  !globalThis.jamEmu.inFlight && !globalThis.jamEmu.pending.length);
async function painted(page) {
  const before = (await state(page)).frames;
  await page.waitForFunction(frame => globalThis.jamEmu.state.frames >= frame + 2, before);
}
async function tap(page, id) {
  await page.locator(`button[data-button="${id}"]`).click({ delay: 70 });
  await idle(page); await painted(page);
}
async function turn(page, id, amount) {
  await page.locator(`[role="slider"][data-encoder="${id}"]`).focus();
  for (let i = 0; i < Math.abs(amount); i++) {
    await page.keyboard.press(amount < 0 ? 'ArrowDown' : 'ArrowUp');
    await idle(page);
  }
  await painted(page);
}
async function columns(page) {
  return page.locator('#screen').evaluate(canvas => {
    const ctx = canvas.getContext('2d');
    // The shipping C layout has four55px column interiors at x4+60*n,
    // y26..69. Excluding separators avoids counting decorative rules as knobs.
    return Array.from({ length: 4 }, (_, slot) => {
      const data = ctx.getImageData(4 + slot * 60, 26, 55, 44).data;
      let lit = 0, hash = 2166136261;
      for (let i = 0; i < data.length; i += 4) {
        const rgb = data[i] << 16 | data[i + 1] << 8 | data[i + 2];
        if (rgb) lit++;
        hash = Math.imul(hash ^ rgb, 16777619) >>> 0;
      }
      return { lit, hash };
    });
  });
}
async function capture(page, name) {
  await painted(page);
  await page.screenshot({ path: resolve(evidence, `${name}.png`), fullPage: true });
  const url = await page.locator('#screen').evaluate(canvas => canvas.toDataURL('image/png'));
  await writeFile(resolve(evidence, `${name}-lcd.png`), Buffer.from(url.split(',')[1], 'base64'));
}

try {
  await mkdir(evidence, { recursive: true });
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
    headless: true, args: ['--no-sandbox', '--autoplay-policy=document-user-activation-required'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
  page.setDefaultTimeout(15000);
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.goto(address);
  await page.waitForFunction(() => !!globalThis.jamEmu);
  assert.equal(await page.evaluate(() => globalThis.jamEmu.context), null, 'audio awaits a user gesture');
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => globalThis.jamEmu.context?.state === 'running' &&
    globalThis.jamEmu.state?.screenVersion > 0);
  assert(await page.evaluate(() => globalThis.jamEmu.node instanceof AudioWorkletNode),
    'production AudioWorklet owns the shared firmware instance');
  const initial = await state(page);
  assert.equal(initial.selected, 1, 'startup selects Chords');
  assert(initial.home && !initial.playing && !initial.recording && !initial.armed);
  await turn(page, 2, 10); // Real PRESETS encoder, curated Chords SOFT PAD.
  assert.equal((await state(page)).sound, 'SOFT PAD');
  assert.equal((await state(page)).engine, 9, 'SOFT PAD uses the production FM6 engine');
  await tap(page, 2); // ENV
  let view = await columns(page);
  assert(view[0].lit > 50 && view[1].lit > 50, 'FM ENV renders its working TIME and PITCH columns');
  assert.equal(view[2].lit, 0, 'FM ENV third column is empty');
  assert.equal(view[3].lit, 0, 'FM ENV fourth column is empty');
  const fmPage = (await state(page)).page;
  assert(!(await state(page)).home);
  await capture(page, 'fm6-envelope');

  const before = await state(page);
  await turn(page, 5, 4); await turn(page, 6, -3); // Empty third/fourth knobs.
  const after = await state(page);
  assert.deepEqual(after.events, before.events, 'empty FM ENV knobs cannot edit sequencer events');
  assert.equal(after.sound, before.sound, 'empty FM ENV knobs cannot select another sound');
  assert.deepEqual(after.macros, before.macros, 'empty FM ENV knobs leave the published sound macro values alone');
  view = await columns(page);
  assert.equal(view[2].lit + view[3].lit, 0, 'empty columns stay empty after their physical knobs are turned');
  await turn(page, 3, 3);
  const time = await columns(page);
  assert.notEqual(time[0].hash, view[0].hash, 'the real TIME knob updates its LCD value');
  await turn(page, 4, 3);
  const pitch = await columns(page);
  assert.notEqual(pitch[1].hash, time[1].hash, 'the real PITCH knob updates its LCD value');
  assert.deepEqual((await state(page)).events, initial.events, 'sound editing preserves every existing track event');
  await tap(page, 2);
  assert.equal((await state(page)).page, fmPage, 'repeated ENV cannot cycle into FM6 generic ADSR screens');
  await capture(page, 'fm6-envelope-edited');

  // A held real key must reach the FM6 audio engine while on its new ENV page.
  const key = page.locator('button[data-key="7"]');
  await key.scrollIntoViewIfNeeded();
  const bounds = await key.boundingBox();
  assert(bounds);
  await page.mouse.move(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
  await page.mouse.down();
  await page.waitForFunction(() => globalThis.jamEmu.state.held > 0 && globalThis.jamEmu.peak > 0.005);
  assert.deepEqual((await state(page)).events, initial.events, 'first FM ENV audition plays without writing a step');
  await page.mouse.up(); await idle(page);
  await page.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.held);

  await tap(page, 6); // HOME
  assert((await state(page)).homeControls, 'HOME returns from FM ENV to the usable Home view');
  await turn(page, 2, -10); // Return to the starting Analog Chords preset.
  assert.equal((await state(page)).sound, initial.sound);
  assert.equal((await state(page)).engine, 0);
  await tap(page, 2);
  const ordinary = await columns(page);
  assert(ordinary.every(column => column.lit > 50), 'Analog ENV retains all four working ADSR columns');
  assert.notEqual((await state(page)).page, fmPage, 'changing engine repairs remembered ENV to ordinary ADSR');
  assert.deepEqual((await state(page)).events, initial.events);
  await capture(page, 'analog-envelope');
  assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
  assert.deepEqual(errors, [], 'no browser JavaScript or production AudioWorklet errors');
  console.log('PASS production FM6 TIME/PITCH LCD, empty knobs, real audio audition, unchanged events, repeated ENV, HOME and Analog ADSR return');
  console.log(`Sound page Chromium evidence: ${evidence}. Physical FM-1 behavior remains unverified.`);
} finally {
  await browser?.close();
  await new Promise(done => server.close(done));
}
