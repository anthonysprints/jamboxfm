// SPDX-License-Identifier: GPL-3.0-only
// Real production panel, AudioWorklet and C navigation. Build the emulator first:
// . tools/cloud/activate.sh && node tests/jam_drum_navigation_browser.mjs
// Native drum-navigation tests check LCD rectangle bounds and modal details.
// These browser checks observe the unmodified worklet and its rendered canvas;
// they do not verify physical FM-1 installation, controls or flashing.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

const playwright = process.env.PLAYWRIGHT_MODULE
  ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve('build/jam-emu');
const evidence = resolve('build/jam-drum-nav-evidence');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    if (pathname === '/favicon.ico') { response.writeHead(204); response.end(); return; }
    const path = resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!path.startsWith(root + sep)) throw new Error('Outside emulator');
    const body = await readFile(path);
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' });
    response.end(body);
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(done => server.listen(0, '127.0.0.1', done));
const address = `http://127.0.0.1:${server.address().port}`;
const errors = [];
let browser;

const button = (page, id) => page.locator(`button[data-button="${id}"]`);
const key = (page, id) => page.locator(`button[data-key="${id}"]`);
const state = page => page.evaluate(() => globalThis.jamEmu.state);
const idle = page => page.waitForFunction(() => globalThis.jamEmu.state &&
  !globalThis.jamEmu.inFlight && !globalThis.jamEmu.pending.length);
async function tap(page, id) { await button(page, id).click({ delay: 70 }); await idle(page); }
async function hit(page, id = 2) { await key(page, id).click({ delay: 45 }); await idle(page); }
async function turn(page, id, direction) {
  await page.locator(`[role="slider"][data-encoder="${id}"]`).focus();
  await page.keyboard.press(direction);
  await idle(page);
}
async function screen(page) {
  return page.evaluate(() => {
    const canvas = document.getElementById('screen');
    const pixels = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
    let hash = 2166136261, lit = 0;
    for (let i = 0; i < pixels.length; i += 4) {
      hash = Math.imul(hash ^ (pixels[i] << 16 | pixels[i + 1] << 8 | pixels[i + 2]), 16777619) >>> 0;
      if (pixels[i] || pixels[i + 1] || pixels[i + 2]) lit++;
    }
    return { hash, lit, width: canvas.width, height: canvas.height };
  });
}
async function changedScreen(page, previous) {
  await page.waitForFunction(oldHash => {
    const canvas = document.getElementById('screen');
    const pixels = canvas.getContext('2d').getImageData(0, 0, 240, 240).data;
    let hash = 2166136261;
    for (let i = 0; i < pixels.length; i += 4)
      hash = Math.imul(hash ^ (pixels[i] << 16 | pixels[i + 1] << 8 | pixels[i + 2]), 16777619) >>> 0;
    return hash !== oldHash;
  }, previous.hash);
  const current = await screen(page);
  assert.equal(current.width, 240); assert.equal(current.height, 240);
  assert(current.lit > 100, 'firmware renders a nonempty 240 × 240 LCD');
  return current;
}
async function screenshot(page, name) {
  await page.screenshot({ path: resolve(evidence, `${name}.png`), fullPage: true });
  await page.locator('#screen').screenshot({ path: resolve(evidence, `${name}-lcd.png`) });
}
async function startDrums(view) {
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
    'the production AudioWorklet runs the shared C instrument');
  assert.equal((await state(page)).selected, 1, 'startup selects Chords');
  await turn(page, 1, 'ArrowDown');
  await page.waitForFunction(() => globalThis.jamEmu.state.selected === 0);
  await turn(page, 1, 'ArrowDown');
  await page.waitForFunction(() => globalThis.jamEmu.state.selected === 3);
  assert.equal((await state(page)).tempo, 88, 'ALGORITHM follows Home part order without changing BPM');
  if (view === 1) await tap(page, 6);
  await page.waitForFunction(expected => globalThis.jamEmu.state.homeControls &&
    globalThis.jamEmu.state.homeView === expected, view);
  return page;
}

try {
  await mkdir(evidence, { recursive: true });
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
    headless: true, args: ['--no-sandbox', '--autoplay-policy=document-user-activation-required'] });

  for (const view of [0, 1]) {
    const name = view ? 'sound' : 'groove';
    const page = await startDrums(view);
    const initial = await state(page), homeScreen = await screen(page);
    await tap(page, 9); // Ordinary SEQ tap, not a held layer.
    await page.waitForFunction(() => !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0);
    const gridScreen = await changedScreen(page, homeScreen);
    const gridPage = (await state(page)).page;
    assert.equal((await state(page)).homeView, view, 'entering GRID remembers the Home view');
    assert.deepEqual((await state(page)).events, initial.events, 'opening GRID preserves every track');
    assert(!(await state(page)).playing && !(await state(page)).recording && !(await state(page)).armed);

    // Physical key 2 is white step 2; its startup cell is entirely empty.
    // Event counts report occupied drum steps, so this distinguishes step entry
    // from a pad audition without adding a debug command or changing the worklet.
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    const added = await state(page);
    assert.deepEqual(added.events.slice(0, 3), initial.events.slice(0, 3));
    assert(!added.recording && !added.armed && !added.playing, 'step keys edit without REC or transport');
    await screenshot(page, `${name}-grid`);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);

    await tap(page, 9); // Existing GRID → KIT semantics remain for this increment.
    const kitScreen = await changedScreen(page, gridScreen);
    assert.equal((await state(page)).page, gridPage, 'KIT remains inside the dedicated drum workspace');
    await hit(page);
    assert.deepEqual((await state(page)).events, initial.events, 'KIT keys audition drums without editing steps');
    await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
    await screenshot(page, `${name}-kit`);
    await tap(page, 9);
    await changedScreen(page, kitScreen);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);

    // Queue HOME's full physical gesture and the pad tap in one browser task.
    // The production input queue preserves the edges; no idle frame is inserted
    // by this test between navigation and the first returned-to-Home key.
    await page.evaluate(() => {
      document.querySelector('button[data-button="6"]').click();
      document.querySelector('button[data-key="2"]').click();
    });
    await idle(page);
    await page.waitForFunction(expected => globalThis.jamEmu.state.homeControls &&
      globalThis.jamEmu.state.homeView === expected, view);
    assert.deepEqual((await state(page)).events, initial.events, 'immediate HOME return restores playable pads');
    await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);

    await tap(page, 2); // Real ENV deep page.
    await page.waitForFunction(() => !globalThis.jamEmu.state.home);
    assert.notEqual((await state(page)).page, gridPage, 'ENV is a distinct deep page');
    const deepScreen = await screen(page);
    await tap(page, 9);
    await page.waitForFunction(expected => globalThis.jamEmu.state.page === expected &&
      !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0, gridPage);
    await changedScreen(page, deepScreen);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);
    await tap(page, 6);
    await page.waitForFunction(expected => globalThis.jamEmu.state.homeControls &&
      globalThis.jamEmu.state.homeView === expected, view);
    assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
    await page.close();
    console.log(`PASS ${name} Home → GRID, reversible step edit, KIT/pad behavior, immediate HOME key, deep-page entry`);
  }

  const page = await startDrums(1);
  await tap(page, 9);
  const gridPage = (await state(page)).page;
  await tap(page, 10);
  await page.waitForFunction(() => globalThis.jamEmu.state.playing);
  assert.equal((await state(page)).page, gridPage);
  assert(!(await state(page)).home && !(await state(page)).recording, 'PLAY preserves GRID and does not arm REC');
  await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
  await tap(page, 10);
  await page.waitForFunction(() => !globalThis.jamEmu.state.playing);
  await tap(page, 11);
  await page.waitForFunction(() => globalThis.jamEmu.state.home && globalThis.jamEmu.state.homeView === 1 &&
    globalThis.jamEmu.state.armed && !globalThis.jamEmu.state.playing);
  await hit(page);
  await page.waitForFunction(() => globalThis.jamEmu.state.playing &&
    globalThis.jamEmu.state.recording === 8 && !globalThis.jamEmu.state.armed);
  assert((await state(page)).homeControls, 'first recorded pad returns to normal playable Sound Home');
  const beforeRecording = (await state(page)).events[3];
  for (let attempt = 0; attempt < 12 && (await state(page)).events[3] === beforeRecording; attempt++) {
    await page.waitForTimeout(95);
    await hit(page);
  }
  assert((await state(page)).events[3] > beforeRecording, 'live pad hits add a recorded event on an empty step');
  await screenshot(page, 'recording-on-sound-home');
  await tap(page, 11);
  await page.waitForFunction(() => !globalThis.jamEmu.state.recording && !globalThis.jamEmu.state.armed);
  assert((await state(page)).playing, 'ending recording leaves playback running');

  await tap(page, 9);
  await page.waitForFunction(expected => !globalThis.jamEmu.state.home &&
    globalThis.jamEmu.state.page === expected, gridPage);
  await tap(page, 11);
  await page.waitForFunction(() => globalThis.jamEmu.state.homeControls && globalThis.jamEmu.state.homeView === 1 &&
    globalThis.jamEmu.state.playing && globalThis.jamEmu.state.recording === 8);
  await tap(page, 9);
  await page.waitForFunction(expected => !globalThis.jamEmu.state.home &&
    globalThis.jamEmu.state.page === expected && globalThis.jamEmu.state.recording === 8, gridPage);
  await tap(page, 6);
  await page.waitForFunction(() => globalThis.jamEmu.state.homeControls &&
    globalThis.jamEmu.state.homeView === 1 && globalThis.jamEmu.state.recording === 8);
  await tap(page, 11);
  await page.waitForFunction(() => !globalThis.jamEmu.state.recording);
  await tap(page, 10);
  await page.waitForFunction(() => !globalThis.jamEmu.state.playing);
  assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
  await page.close();
  console.log('PASS GRID PLAY/stop, REC arm and first live pad, recorded events, playing overdub and ordinary SEQ ownership');
  assert.deepEqual(errors, [], 'no production browser JavaScript or AudioWorklet errors');
  console.log(`Drum navigation: 3 Chromium scenarios passed using the production panel, AudioWorklet, rendered LCD and audio. Physical FM-1 behavior remains unverified. Screenshots: ${evidence}`);
} finally {
  await browser?.close();
  await new Promise(done => server.close(done));
}
