// SPDX-License-Identifier: GPL-3.0-only
// Actual browser + AudioWorklet/WASM smoke. Requires playwright-core and Chromium.
// PLAYWRIGHT_MODULE=/path/to/playwright-core/index.mjs CHROMIUM_PATH=/usr/bin/chromium node tests/jam_preview_browser.mjs
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

const playwright = process.env.PLAYWRIGHT_MODULE ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve('build/jam-preview');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm' };
const server = createServer(async (request, response) => {
  try {
    const path = resolve(root, '.' + (new URL(request.url, 'http://localhost').pathname === '/' ? '/index.html' : new URL(request.url, 'http://localhost').pathname));
    if (!path.startsWith(root + sep)) throw new Error('Outside preview');
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' }); response.end(await readFile(path));
  } catch { if (!response.headersSent) response.writeHead(404); response.end('Not found'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const address = `http://127.0.0.1:${server.address().port}`;
let browser;
try {
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true,
    args: ['--no-sandbox', '--autoplay-policy=document-user-activation-required'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  await page.goto(address); await page.waitForSelector('.pad');
  await page.evaluate(async () => { globalThis.testJam = (await import('./app.js')).session; });
  assert.equal(await page.locator('#pads .pad').count(), 16);
  assert.equal(await page.locator('#play').isDisabled(), true, 'audio requires explicit start');
  assert.equal(await page.evaluate(async () => globalThis.testJam.context === null), true);
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => !document.getElementById('play').disabled && !document.getElementById('transport-controls').disabled);
  await page.waitForFunction(() => globalThis.testJam.state?.tempo === 88);
  await page.locator('#play').click();
  await page.waitForFunction(() => document.getElementById('play').getAttribute('aria-pressed') === 'true');
  await page.waitForFunction(() => document.getElementById('meter').value > 0.01);
  assert.equal(await page.evaluate(async () => globalThis.testJam.context.sampleRate), 44100);

  await page.locator('#record').click();
  await page.keyboard.down('g'); await page.waitForTimeout(330); await page.keyboard.up('g');
  await page.waitForTimeout(330);
  await page.keyboard.down('h'); await page.waitForTimeout(330); await page.keyboard.up('h');
  await page.locator('#record').click();
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 1).events >= 8);
  const chordEvents = await page.evaluate(async () => globalThis.testJam.state.tracks.find(t => t.track === 1).events);
  await page.getByRole('button', { name: 'Select Bass', exact: true }).click();
  await page.waitForFunction(() => globalThis.testJam.state.selected === 0);
  await page.locator('#record').click();
  await page.keyboard.down('g'); await page.waitForTimeout(330); await page.keyboard.up('g');
  await page.locator('#record').click();
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 0).events > 0);
  assert.equal(await page.evaluate(async () => globalThis.testJam.state.tracks.find(t => t.track === 1).events), chordEvents);

  await page.getByRole('button', { name: 'Next Bass sound', exact: true }).click();
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 0).soundIndex === 1);
  await page.locator('[data-knob="0"]').fill('82'); await page.locator('[data-knob="0"]').dispatchEvent('input');
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 0).macros[0].value >= 79);
  await page.getByRole('button', { name: 'Mute Bass', exact: true }).click();
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 0).muted);
  await page.getByRole('button', { name: 'Mute Bass', exact: true }).click();
  await page.locator('#tempo').fill('94'); await page.locator('#tempo').dispatchEvent('change');
  await page.locator('#swing').fill('24'); await page.locator('#swing').dispatchEvent('input');
  await page.waitForFunction(() => { const s = globalThis.testJam.state; return s.tempo === 94 && s.swing === 24; });
  const step = page.getByRole('button', { name: 'RIM step 1', exact: true });
  const before = await step.getAttribute('aria-pressed'); await step.click();
  await page.waitForFunction(previous => document.querySelector('.step[data-lane="7"][data-step="0"]').getAttribute('aria-pressed') !== previous, before);

  // Pointer capture releases outside the pad; lost capture and blur cannot stick notes.
  const pad = page.locator('.pad[data-key="7"]'), bounds = await pad.boundingBox();
  await page.mouse.move(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2); await page.mouse.down();
  await page.waitForFunction(() => document.querySelector('.pad[data-key="7"]').classList.contains('down'));
  await page.mouse.move(5, 5); await page.mouse.up();
  assert.equal(await page.locator('.pad.down').count(), 0);
  await page.keyboard.down('g'); await page.waitForTimeout(70);
  await page.evaluate(() => window.dispatchEvent(new Event('blur'))); await page.keyboard.up('g');
  await page.waitForFunction(() => document.getElementById('play').getAttribute('aria-pressed') === 'false');
  assert.equal(await page.locator('.pad.down').count(), 0);
  await page.evaluate(() => document.activeElement?.blur());
  await page.keyboard.press('Space');
  await page.waitForFunction(() => document.getElementById('play').getAttribute('aria-pressed') === 'true');
  await page.keyboard.press('Escape');
  await page.waitForFunction(() => document.getElementById('play').getAttribute('aria-pressed') === 'false');
  // Regression: edit immediately after choosing another part, before the audio
  // thread can acknowledge selection. The edit must belong to Chords.
  await page.evaluate(() => {
    document.querySelector('.part[data-track=\"1\"] .part-select').click();
    const chord = document.getElementById('chord'); chord.value = '1'; chord.dispatchEvent(new Event('change'));
  });
  await page.waitForFunction(() => globalThis.testJam.state.tracks.find(t => t.track === 1).chord === 1);
  await page.waitForFunction(() => !globalThis.testJam.inFlight && !globalThis.testJam.pending.length);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.tracks.find(t => t.track === 0).chord), 0, 'rapid chord edit did not affect Bass');
  await page.evaluate(() => {
    document.querySelector('.part[data-track="0"] .part-select').click();
    document.getElementById('panic').click();
  });
  await page.waitForFunction(() => !globalThis.testJam.inFlight && !globalThis.testJam.pending.length && globalThis.testJam.selectionTarget == null);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.selected), 1, 'panic cancels queued selection');
  assert.equal(await page.locator('.part.selected').getAttribute('data-track'), '1', 'selection display reconciles after panic');
  await page.evaluate(() => {
    for (const id of ['play', 'record', 'octave-up']) { document.getElementById(id).click(); document.getElementById(id).click(); }
    const mute = document.querySelector('.part[data-track="1"] .mute'); mute.click(); mute.click();
    const sound = document.querySelector('.part[data-track="1"] .sound-next'); sound.click(); sound.click();
  });
  await page.waitForFunction(() => !globalThis.testJam.inFlight && !globalThis.testJam.pending.length);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.playing), false);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.armed), false);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.octave), 2);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.tracks.find(t => t.track === 1).muted), false);
  assert.equal(await page.evaluate(() => globalThis.testJam.state.tracks.find(t => t.track === 1).soundIndex), 0);
  await page.evaluate(() => { document.getElementById('octave-down').click(); document.getElementById('octave-down').click(); });
  await page.waitForFunction(() => !globalThis.testJam.inFlight && !globalThis.testJam.pending.length);
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => globalThis.testJam.context.state === 'suspended');
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => globalThis.testJam.context.state === 'running');
  assert.equal(await page.evaluate(async () => globalThis.testJam.state.tracks.find(t => t.track === 1).events), chordEvents);
  assert.equal(await page.locator('#chord').inputValue(), '1', 'display agrees with the real chord mode after pause/resume');
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), true, 'desktop fits viewport');
  await mkdir('build/jam-preview-evidence', { recursive: true });
  await page.screenshot({ path: 'build/jam-preview-evidence/desktop.png', fullPage: true });

  const mobile = await browser.newPage({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  mobile.on('pageerror', error => errors.push(error.message));
  await mobile.goto(address);
  await mobile.evaluate(async () => { globalThis.testJam = (await import('./app.js')).session; });
  await mobile.locator('#audio-start').tap();
  await mobile.waitForFunction(() => globalThis.testJam.state?.tempo === 88);
  await mobile.getByRole('button', { name: 'Select Drums', exact: true }).tap();
  await mobile.waitForFunction(() => globalThis.testJam.state.selected === 3);
  await mobile.locator('.pad[data-key="0"]').tap();
  await mobile.locator('#play').tap();
  await mobile.waitForFunction(() => document.getElementById('meter').value > 0.01);
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), true, 'mobile fits viewport; sequencer scrolls internally');
  await mobile.screenshot({ path: 'build/jam-preview-evidence/mobile.png', fullPage: true });
  assert.deepEqual(errors, [], 'no browser JavaScript errors');
  console.log('JamBox Chromium: explicit AudioWorklet start, audible beat, two-chord and bass capture, live macros/sound/mute/tempo/swing/steps, keyboard, pointer release, blur panic, pause/resume, desktop/mobile passed');
} finally {
  await browser?.close(); await new Promise(resolve => server.close(resolve));
}
