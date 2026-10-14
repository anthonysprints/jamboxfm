// SPDX-License-Identifier: GPL-3.0-only
// Real Chromium, AudioWorklet and firmware UI/input integration. Build first:
// PLAYWRIGHT_MODULE=/path/to/playwright-core/index.mjs CHROMIUM_PATH=/usr/bin/chromium node tests/jam_emu_browser.mjs
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir, access } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

const playwright = process.env.PLAYWRIGHT_MODULE ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const root = resolve('build/jam-emu');
const evidence = resolve('build/jam-emu-evidence');
const standalone = resolve('build/jambox-emulator.html');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm', '.svg': 'image/svg+xml' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    const path = pathname === '/standalone.html' ? standalone : resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (path !== standalone && !path.startsWith(root + sep)) throw new Error('Outside emulator');
    const body = await readFile(path);
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' });
    response.end(body);
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const address = `http://127.0.0.1:${server.address().port}`;
const errors = [];
let browser;

const button = (page, id) => page.locator(`button[data-button="${id}"]`);
const encoder = (page, id) => page.locator(`[role="slider"][data-encoder="${id}"]`);
const state = page => page.evaluate(() => globalThis.jamEmu.state);
const idle = page => page.waitForFunction(() => globalThis.jamEmu.state && !globalThis.jamEmu.inFlight && !globalThis.jamEmu.pending.length);
const tap = (page, id) => button(page, id).click({ delay: 70 });
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
async function waitForScreenChange(page, previous) {
  // Logical state is acknowledged before the next UI frame/DMA publication.
  await page.waitForFunction(oldHash => {
    const canvas = document.getElementById('screen');
    const pixels = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
    let hash = 2166136261;
    for (let i = 0; i < pixels.length; i += 4)
      hash = Math.imul(hash ^ (pixels[i] << 16 | pixels[i + 1] << 8 | pixels[i + 2]), 16777619) >>> 0;
    return hash !== oldHash;
  }, previous);
}
async function turn(page, id, direction = 'ArrowUp') {
  await encoder(page, id).focus();
  await page.keyboard.press(direction);
}
async function pointerDown(page, locator) {
  await locator.scrollIntoViewIfNeeded();
  const bounds = await locator.boundingBox();
  assert(bounds, 'physical control has a bounding box');
  await page.mouse.move(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
  await page.mouse.down();
}
async function start(page, url) {
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(url);
  await page.waitForFunction(() => !!globalThis.jamEmu);
  assert.equal(await page.evaluate(() => globalThis.jamEmu.context === null), true, 'audio waits for a user gesture');
  assert.equal(await page.locator('button[data-key]').count(), 27, 'all 27 physical keys are represented');
  assert.equal(await page.locator('button[data-button]').count(), 14, 'all 14 panel buttons are represented');
  assert.equal(await page.locator('[role="slider"][data-encoder]').count(), 7, 'seven physical encoders are represented');
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => globalThis.jamEmu.context?.state === 'running' && globalThis.jamEmu.state?.frames > 0);
  await page.waitForFunction(() => globalThis.jamEmu.state.screenVersion > 0);
  await idle(page);
  const initial = await state(page), pixels = await screen(page);
  assert.equal(initial.tempo, 88);
  assert.equal(initial.selected, 1, 'boot selects Chords');
  assert(initial.home, 'actual firmware starts at JamBox Home');
  assert(!initial.playing, 'the transport waits for PLAY');
  assert.equal(pixels.width, 240); assert.equal(pixels.height, 240);
  assert(pixels.lit > 100, 'firmware drew a nonempty LCD framebuffer');
  return pixels;
}

try {
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true,
    args: ['--no-sandbox', '--autoplay-policy=document-user-activation-required'] });
  await mkdir(evidence, { recursive: true });
  const page = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
  const initialScreen = await start(page, address);
  await page.screenshot({ path: resolve(evidence, 'desktop-home.png'), fullPage: true });

  // Encoder events go through the firmware panel roles, not a browser patch editor.
  await turn(page, 1);
  await page.waitForFunction(() => globalThis.jamEmu.state.selected === 2);
  assert.equal((await state(page)).tempo, 88, 'ALGORITHM changes part without changing tempo');
  await waitForScreenChange(page, initialScreen.hash);
  assert.notEqual((await screen(page)).hash, initialScreen.hash, 'part selection redraws the real LCD');
  await turn(page, 1, 'ArrowDown');
  await page.waitForFunction(() => globalThis.jamEmu.state.selected === 1);
  const soundBefore = (await state(page)).sound;
  await turn(page, 2);
  await page.waitForFunction(previous => globalThis.jamEmu.state.sound !== previous, soundBefore);
  const tempoSound = (await state(page)).sound;
  await turn(page, 0);
  await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 89);
  assert.equal((await state(page)).selected, 1, 'top-right Home knob preserves part');
  assert.equal((await state(page)).sound, tempoSound, 'top-right Home knob preserves sound');
  assert.equal(await encoder(page, 0).getAttribute('aria-valuetext'), 'Tempo: 89 BPM');
  await turn(page, 0, 'ArrowDown');
  await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 88);
  const macroBefore = (await state(page)).macros[0];
  await turn(page, 3);
  await page.waitForFunction(previous => globalThis.jamEmu.state.macros[0] !== previous, macroBefore);
  const macroAfter = (await state(page)).macros[0];
  await encoder(page, 3).hover();
  await page.mouse.wheel(0, -100);
  await page.waitForFunction(previous => globalThis.jamEmu.state.macros[0] !== previous, macroAfter);
  const moveBefore = (await state(page)).macros[1];
  const moveKnob = encoder(page, 4);
  await pointerDown(page, moveKnob);
  const moveBounds = await moveKnob.boundingBox();
  await page.mouse.move(moveBounds.x + moveBounds.width / 2, moveBounds.y + moveBounds.height / 2 - 35, { steps: 5 });
  await page.mouse.up();
  await page.waitForFunction(previous => globalThis.jamEmu.state.macros[1] !== previous, moveBefore);
  const masterBefore = (await state(page)).master;
  await page.locator('[role="slider"][data-master]').focus();
  await page.evaluate(() => {
    const master = document.querySelector('[role="slider"][data-master]');
    master.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', code: 'ArrowDown', bubbles: true }));
    document.getElementById('panic').click();
  });
  await idle(page);
  assert.equal((await state(page)).master, masterBefore, 'panic cancels a master turn queued in the same browser task');
  await page.keyboard.press('ArrowDown');
  await page.waitForFunction(previous => globalThis.jamEmu.state.master !== previous, masterBefore);
  assert.equal((await state(page)).master, masterBefore - 1, 'the next master detent moves exactly once after cancellation');

  // The held layers and deep pages are firmware screens with distinct pixels.
  const homeScreen = await screen(page);
  await pointerDown(page, button(page, 0));
  await page.waitForFunction(() => globalThis.jamEmu.state.layer !== 0);
  await waitForScreenChange(page, homeScreen.hash);
  assert.notEqual((await screen(page)).hash, homeScreen.hash, 'holding FX displays the firmware FX layer');
  await page.waitForTimeout(500); // Beyond the firmware's 450 ms tap threshold.
  await page.mouse.up();
  await page.waitForFunction(() => globalThis.jamEmu.state.layer === 0 && globalThis.jamEmu.state.home);
  await pointerDown(page, button(page, 1));
  await page.waitForFunction(() => globalThis.jamEmu.state.layer !== 0);
  await page.waitForTimeout(500);
  await page.mouse.up();
  await page.waitForFunction(() => globalThis.jamEmu.state.layer === 0);
  await tap(page, 2);
  await page.waitForFunction(() => !globalThis.jamEmu.state.home);
  await waitForScreenChange(page, homeScreen.hash);
  assert.notEqual((await screen(page)).hash, homeScreen.hash, 'ENV opens a real deep screen');
  await tap(page, 6);
  await page.waitForFunction(() => globalThis.jamEmu.state.home);

  // Real momentary PLAY / REC and local key events exercise the shared recorder.
  await tap(page, 10);
  await page.waitForFunction(() => globalThis.jamEmu.state.playing);
  await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
  await tap(page, 11);
  await page.waitForFunction(() => !!globalThis.jamEmu.state.recording);
  const note = page.locator('button[data-key="12"]');
  await pointerDown(page, note);
  await page.waitForFunction(() => globalThis.jamEmu.state.keys !== 0);
  await page.waitForTimeout(330);
  await page.mouse.move(3, 3); await page.mouse.up();
  await page.waitForFunction(() => globalThis.jamEmu.state.keys === 0);
  await tap(page, 11);
  await page.waitForFunction(() => !globalThis.jamEmu.state.recording && !globalThis.jamEmu.state.armed);
  assert((await state(page)).playing, 'finishing the take leaves the shared transport playing');
  await page.waitForFunction(() => globalThis.jamEmu.state.events?.[1] > 0);
  await page.screenshot({ path: resolve(evidence, 'desktop-playing.png'), fullPage: true });

  // Pointer capture and focus loss must release physical notes and held buttons.
  await page.evaluate(() => document.activeElement?.blur());
  await page.keyboard.down('z');
  await page.waitForFunction(() => globalThis.jamEmu.state.keys !== 0);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.keyboard.up('z');
  await page.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.buttons && !globalThis.jamEmu.state.playing);
  await pointerDown(page, button(page, 0));
  await page.waitForFunction(() => globalThis.jamEmu.state.buttons !== 0);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.mouse.up();
  await page.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.buttons && globalThis.jamEmu.state.layer === 0);
  await tap(page, 6);
  await page.waitForFunction(() => globalThis.jamEmu.state.home);

  await page.evaluate(() => {
    const play = document.querySelector('button[data-button="10"]');
    play.click(); play.click();
  });
  await idle(page);
  assert(!(await state(page)).playing, 'two synchronous physical PLAY clicks cancel after their ordered press/release edges');

  const beforePause = await state(page);
  await page.evaluate(() => {
    document.getElementById('audio-start').click();
    window.dispatchEvent(new KeyboardEvent('keydown', { key: 'z', code: 'KeyZ', bubbles: true }));
  });
  await page.waitForFunction(() => globalThis.jamEmu.context.state === 'suspended');
  await page.locator('#audio-start').click();
  await page.waitForFunction(() => globalThis.jamEmu.context.state === 'running' && !globalThis.jamEmu.state.playing);
  await idle(page);
  assert.equal((await state(page)).keys, 0, 'a note pressed during the pause acknowledgement cannot survive resume');
  assert.equal((await state(page)).held, 0, 'no physical pitch ownership survives the pause race');
  await page.evaluate(() => window.dispatchEvent(new KeyboardEvent('keyup', { key: 'z', code: 'KeyZ', bubbles: true })));
  assert.equal((await state(page)).sound, beforePause.sound, 'pause/resume preserves the selected sound');
  assert.deepEqual((await state(page)).events, beforePause.events, 'pause/resume preserves the captured notes');
  assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'desktop emulator fits the viewport');

  // SNAP lights the played key and sounding pitch, without lighting every
  // neighboring key that could quantize to that pitch. Use a fresh session.
  const snap = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
  await start(snap, address);
  await turn(snap, 1);
  await snap.waitForFunction(() => globalThis.jamEmu.state.selected === 2);
  await tap(snap, 1);
  await snap.waitForFunction(() => !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0);
  for (let i = 0; i < 3; i++) await turn(snap, 3); // C -> D# root.
  await turn(snap, 4, 'ArrowDown'); // Minor -> major; Lead retains SNAP.
  await idle(snap);
  await tap(snap, 6);
  await snap.waitForFunction(() => globalThis.jamEmu.state.home);
  await snap.evaluate(() => {
    globalThis.snapLedSamples = [];
    const receive = globalThis.jamEmu.node.port.onmessage;
    globalThis.jamEmu.node.port.onmessage = event => {
      const current = event.data.state;
      if (current?.keys === (1 << 10) && current.held === 1 && current.leds[24] === 255)
        globalThis.snapLedSamples.push(current.leds[25]);
      receive(event);
    };
  });
  await pointerDown(snap, snap.locator('button[data-key="10"]')); // D#4.
  await snap.waitForFunction(() => globalThis.jamEmu.state.keys === (1 << 10) && globalThis.jamEmu.state.held === 1 && globalThis.jamEmu.state.leds[24] === 255);
  await snap.waitForTimeout(180); // Includes the six-frame note-hit flash.
  const dSharpSamples = await snap.evaluate(() => globalThis.snapLedSamples);
  assert(dSharpSamples.length > 1, 'observe both initial and sustained sounding-note LEDs');
  assert(dSharpSamples.every(level => level === 0), 'D#4 never lights unpressed E4, including its note-hit flash');
  await snap.mouse.up();
  await snap.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.held && globalThis.jamEmu.state.leds[24] !== 255 && globalThis.jamEmu.state.leds[25] !== 255);
  await pointerDown(snap, snap.locator('button[data-key="11"]')); // E4 snaps down to D#4.
  await snap.waitForFunction(() => globalThis.jamEmu.state.keys === (1 << 11) && globalThis.jamEmu.state.held === 1 && globalThis.jamEmu.state.leds[24] === 255 && globalThis.jamEmu.state.leds[25] === 255);
  await snap.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
  await snap.mouse.up();
  await snap.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.held && globalThis.jamEmu.state.leds[24] !== 255 && globalThis.jamEmu.state.leds[25] !== 255);
  await snap.close();

  // Chord roots follow the same physical keyboard on every melodic part.
  // Configure the actual SEL page so this also covers access to fixed shapes.
  for (const part of [1, 2]) {
    const chords = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
    await start(chords, address);
    if (part === 2) {
      await turn(chords, 1);
      await chords.waitForFunction(() => globalThis.jamEmu.state.selected === 2);
    }
    await tap(chords, 1);
    await chords.waitForFunction(() => !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0);
    for (let i = 0; i < 3; i++) await turn(chords, 3); // C -> D# root.
    await turn(chords, 4, 'ArrowDown'); // Minor -> major; both parts default to SNAP.
    const chordSetting = part === 1 ? 1 : 6; // Chords: TRIAD; Lead: fixed MAJOR.
    if (part === 1) await turn(chords, 6, 'ArrowDown'); // Startup 7TH -> TRIAD.
    else for (let i = 0; i < chordSetting; i++) await turn(chords, 6);
    await idle(chords);
    await tap(chords, 6);
    await chords.waitForFunction(() => globalThis.jamEmu.state.home);

    async function holdChordKey(key, held, litKeys) {
      await chords.evaluate(({ key, held }) => {
        globalThis.chordLedSamples = [];
        const receive = globalThis.jamEmu.node.port.onmessage;
        globalThis.chordLedReceive = receive;
        globalThis.jamEmu.node.port.onmessage = event => {
          const current = event.data.state;
          if (current?.keys === (1 << key) && current.held === held && current.leds[14 + key] === 255)
            globalThis.chordLedSamples.push(current.leds.slice(14).flatMap((level, index) => level === 255 ? [index] : []));
          receive(event);
        };
      }, { key, held });
      await pointerDown(chords, chords.locator(`button[data-key="${key}"]`));
      await chords.waitForFunction(({ key, held }) => globalThis.jamEmu.state.keys === (1 << key) && globalThis.jamEmu.state.held === held,
        { key, held });
      await chords.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
      await chords.waitForTimeout(180); // Observe the note-hit flash and sustained chord.
      const samples = await chords.evaluate(() => globalThis.chordLedSamples);
      assert(samples.length > 1, `part ${part}: chord LEDs are observed while ${held} notes sound`);
      for (const sample of samples) assert.deepEqual(sample, litKeys, `part ${part}: only the pressed key and actual chord pitches light`);
      await chords.mouse.up();
      await chords.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.held && !globalThis.jamEmu.state.leds.slice(14).includes(255));
      await chords.evaluate(() => { globalThis.jamEmu.node.port.onmessage = globalThis.chordLedReceive; });
    }

    await holdChordKey(10, 3, [10, 14, 17]); // D# major lights D#/G/A#, without the unpressed E alias.
    await holdChordKey(11, 3, [10, 11, 14, 17]); // Snapped E lights the pressed E plus the same D# major chord.
    await chords.screenshot({ path: resolve(evidence, `chord-builder-part-${part}.png`), fullPage: true });

    // QNT OFF leaves chromatic roots available even while the builder is on.
    await tap(chords, 1);
    await chords.waitForFunction(() => !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0);
    await turn(chords, 5, 'ArrowDown'); // SNAP -> OFF.
    await idle(chords);
    await tap(chords, 6);
    await chords.waitForFunction(() => globalThis.jamEmu.state.home);
    // Scale-built TRIAD keeps E/G/A#; fixed MAJOR gives E/G#/B, irrespective of the chosen scale.
    await holdChordKey(11, 3, part === 1 ? [11, 14, 17] : [11, 15, 18]);

    // Turning CHORD OFF restores one note through the same panel controls.
    await tap(chords, 1);
    await chords.waitForFunction(() => !globalThis.jamEmu.state.home && globalThis.jamEmu.state.layer === 0);
    for (let i = 0; i < chordSetting; i++) await turn(chords, 6, 'ArrowDown');
    await idle(chords);
    await tap(chords, 6);
    await chords.waitForFunction(() => globalThis.jamEmu.state.home);
    await holdChordKey(10, 1, [10]);
    await chords.close();
  }

  const mobile = await browser.newPage({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  await start(mobile, address);
  await button(mobile, 10).tap();
  await mobile.waitForFunction(() => globalThis.jamEmu.state.playing);
  await mobile.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
  await mobile.locator('button[data-key="12"]').tap();
  await mobile.waitForFunction(() => !globalThis.jamEmu.state.keys && !globalThis.jamEmu.state.buttons);
  assert(await mobile.evaluate(() => document.documentElement.scrollWidth <= innerWidth), '390 px emulator fits without horizontal page overflow');
  await mobile.screenshot({ path: resolve(evidence, 'mobile.png'), fullPage: true });
  await mobile.locator('#panic').tap();
  await mobile.waitForFunction(() => !globalThis.jamEmu.state.playing);

  let packaged = false;
  try { await access(standalone); packaged = true; } catch { /* The builder may intentionally omit this artifact. */ }
  if (packaged) {
    const offline = await browser.newPage({ viewport: { width: 1280, height: 1060 } });
    const external = [];
    const documentURL = `${address}/standalone.html`;
    // This managed browser blocks file:// navigation. Load only the embedded
    // document over localhost, then deny every additional network request. This
    // verifies self-containment without claiming a successful file-origin test.
    await offline.route('**/*', route => {
      const request = route.request();
      if (!/^https?:/.test(request.url()) || (request.url() === documentURL && request.isNavigationRequest())) return route.continue();
      external.push(request.url()); return route.abort('blockedbyclient');
    });
    await start(offline, documentURL);
    await tap(offline, 10);
    await offline.waitForFunction(() => globalThis.jamEmu.state.playing && globalThis.jamEmu.peak > 0.005);
    const downloadEvent = offline.waitForEvent('download');
    await offline.locator('#source-download').click();
    const download = await downloadEvent;
    assert.equal(download.suggestedFilename(), 'jambox-source.zip');
    const downloadedSource = await readFile(await download.path());
    assert.equal(downloadedSource.readUInt32LE(0), 0x04034b50, 'corresponding source download is a ZIP archive');
    assert(downloadedSource.equals(await readFile(resolve('build/jambox-source.zip'))), 'the embedded source download matches the packaged corresponding source');
    assert.deepEqual(external, [], 'the embedded emulator makes no network requests after its initial document');
    await offline.screenshot({ path: resolve(evidence, 'standalone.png'), fullPage: true });
    await offline.close();
  }
  assert.deepEqual(errors, [], 'no browser JavaScript errors');
  console.log(`JamBox physical emulator: real LCD, seven encoders, layers/deep pages/Home, PLAY/REC/key capture, audible DSP, SNAP and chromatic chord roots on Chords/Lead, sounding chord-tone LEDs without SNAP aliases, release/blur panic, pause/resume, desktop/mobile${packaged ? ', standalone document with all further network blocked' : ''} passed`);
} finally {
  await browser?.close();
  await new Promise(resolve => server.close(resolve));
}
