// SPDX-License-Identifier: GPL-3.0-only
// Real production panel, AudioWorklet and C navigation. Build the emulator first:
// . tools/cloud/activate.sh && node tests/jam_drum_navigation_browser.mjs
// Native drum-navigation tests check LCD rectangle bounds and modal details.
// These browser checks observe the unmodified worklet and its rendered canvas;
// they do not verify physical FM-1 installation, controls or flashing.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
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
async function audition(page, expectedEvents, message) {
  // The worklet publishes a fresh peak each frame. Wait for the previous pad's
  // tail to finish so an earlier hit cannot satisfy the audio assertion.
  await page.waitForFunction(() => globalThis.jamEmu.peak < 0.001);
  await hit(page);
  await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
  assert.deepEqual((await state(page)).events, expectedEvents, message);
}
async function silentBankTap(page) {
  await page.waitForFunction(() => globalThis.jamEmu.peak < 0.001);
  // Observe throughout the gesture, not just after release: an accidental short
  // audition could finish before the ordinary click helper returns.
  const peaks = page.evaluate(async () => {
    let maximum = 0;
    for (let frame = 0; frame < 18; frame++) {
      await new Promise(requestAnimationFrame);
      maximum = Math.max(maximum, globalThis.jamEmu.peak);
    }
    return maximum;
  });
  await tap(page, 9);
  assert((await peaks) < 0.001, 'a stopped SEQ bank tap never auditions a drum');
}
async function turn(page, id, direction) {
  await page.locator(`[role="slider"][data-encoder="${id}"]`).focus();
  await page.keyboard.press(direction);
  await idle(page);
}
async function turnBy(page, id, steps) {
  const direction = steps < 0 ? 'ArrowDown' : 'ArrowUp';
  for (let i = 0; i < Math.abs(steps); i++) await turn(page, id, direction);
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
  const before = (await state(page)).frames;
  await page.waitForFunction(frame => globalThis.jamEmu.state.frames >= frame + 2, before);
  await page.screenshot({ path: resolve(evidence, `${name}.png`), fullPage: true });
  // Save the actual 240 × 240 framebuffer rather than its CSS-scaled canvas.
  const native = await page.locator('#screen').evaluate(canvas => canvas.toDataURL('image/png'));
  await writeFile(resolve(evidence, `${name}-lcd.png`), Buffer.from(native.split(',')[1], 'base64'));
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

    await tap(page, 9);
    assert.equal((await state(page)).page, gridPage, 'repeated SEQ changes row banks inside the drum workspace');
    assert.deepEqual((await state(page)).events, initial.events, 'changing the drum row bank does not alter the beat');

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

    await tap(page, 4); // EDIT always opens playable KIT SOUND.
    const kitScreen = await changedScreen(page, gridScreen);
    assert.equal((await state(page)).page, gridPage, 'KIT remains inside the dedicated drum workspace');
    await turn(page, 0, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 89);
    await hit(page);
    assert.deepEqual((await state(page)).events, initial.events,
      'SELECT changes KIT BPM by one and keeps keys auditioning drums, not editing steps');
    await page.waitForFunction(() => globalThis.jamEmu.peak > 0.005);
    await turn(page, 0, 'ArrowDown');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 88);
    await screenshot(page, `${name}-kit`);
    await tap(page, 9); // SEQ returns KIT to GRID, never toggles GRID into KIT.
    await changedScreen(page, kitScreen);
    await turn(page, 0, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 89);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);
    await turn(page, 0, 'ArrowDown');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 88);
    await tap(page, 4);
    await hit(page);
    assert.deepEqual((await state(page)).events, initial.events, 'EDIT returns to playable KIT');
    await tap(page, 4);
    await hit(page);
    assert.deepEqual((await state(page)).events, initial.events,
      'repeated EDIT stays on KIT SOUND and keeps keys playing pads');
    await tap(page, 9);
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

    await tap(page, 0); // Real KIT FX deep page; synth ENV is unavailable on drums.
    await page.waitForFunction(() => !globalThis.jamEmu.state.home);
    assert.notEqual((await state(page)).page, gridPage, 'KIT FX is a distinct useful deep page');
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
    console.log(`PASS ${name} Home → GRID, SEQ row banks and idempotent EDIT, KIT and BPM, reversible step edit, immediate HOME key, KIT FX entry`);
  }

  {
    const page = await startDrums(0);
    const initial = await state(page);
    await tap(page, 9);
    const gridPage = (await state(page)).page;

    // Knobs 3/4 edit an existing hit. The startup second step is empty on
    // every lane, so aggregate step counts can detect accidental hit creation.
    await turn(page, 4, 'ArrowUp'); // Step 2, with no audition or hit creation.
    await turn(page, 5, 'ArrowUp');
    await turn(page, 6, 'ArrowUp');
    assert.deepEqual((await state(page)).events, initial.events,
      'Step, Strength and Repeat knobs leave the empty cell empty');
    assert(!(await state(page)).playing && !(await state(page)).recording && !(await state(page)).armed,
      'choosing/editing a cell neither starts transport nor arms recording');
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await turn(page, 5, 'ArrowUp'); // Strength.
    await turn(page, 6, 'ArrowUp'); // Repeat.
    assert.equal((await state(page)).events[3], initial.events[3] + 1,
      'Strength and Repeat preserve the occupied step count');
    await screenshot(page, 'first-eight-named-lanes-strength-repeat');
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);

    const kits = ['DUST KIT', 'SOFT KIT', 'ROCK KIT', 'TIGHT KIT',
      '808 KIT', '909 KIT', 'TRAP KIT', 'NEON KIT'];
    for (const workspace of ['grid', 'kit']) {
      if (workspace === 'kit') await tap(page, 4);
      assert.equal((await state(page)).sound, kits[0]);
      for (const expected of [...kits.slice(1), kits[0]]) {
        await turn(page, 2, 'ArrowUp');
        await page.waitForFunction(sound => globalThis.jamEmu.state.sound === sound, expected);
        const current = await state(page);
        assert.equal(current.page, gridPage);
        assert.equal(current.selected, 3);
        assert.equal(current.tempo, 88);
        assert.deepEqual(current.events, initial.events, `${workspace} kit changes preserve every part's notes`);
      }
    }
    await tap(page, 9);
    const firstRows = await screen(page);
    await turnBy(page, 3, 15); // Last named drum; scroll the eight-row window.
    await changedScreen(page, firstRows);
    assert.deepEqual((await state(page)).events, initial.events, 'choosing later drums only selects/auditions');
    await screenshot(page, 'later-eight-named-lanes');

    await tap(page, 6);
    await page.waitForFunction(() => globalThis.jamEmu.state.homeControls && globalThis.jamEmu.state.homeView === 0);
    assert.equal((await state(page)).groove[2], 32);
    await turnBy(page, 5, -12);
    await page.waitForFunction(() => globalThis.jamEmu.state.groove[2] === 20);
    assert.deepEqual((await state(page)).events, initial.events, 'shortening playback keeps hidden notes');
    await tap(page, 9);
    const firstPage = await screen(page);
    await hit(page, 3); // Second black page key selects literal steps 17–20 / 20.
    await changedScreen(page, firstPage);
    await screenshot(page, 'partial-second-page-17-to-20');
    await hit(page, 7); // Fifth white key would be step 21, beyond length 20.
    assert.deepEqual((await state(page)).events, initial.events, 'disabled second-page step cannot create a hit');

    // The ordinary ALGORITHM knob follows the displayed Drums/Bass/Chords/Lead
    // order, preserving tempo, notes and the remembered Groove Home view.
    await turn(page, 1, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 0 && globalThis.jamEmu.state.homeControls);
    await turn(page, 1, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 1);
    await turn(page, 1, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 2);
    await turnBy(page, 1, -3);
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 3 && globalThis.jamEmu.state.homeControls);
    await tap(page, 9);
    await tap(page, 4);
    await turn(page, 1, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 0 && globalThis.jamEmu.state.homeControls);
    const final = await state(page);
    assert.equal(final.homeView, 0);
    assert.equal(final.tempo, 88);
    assert.deepEqual(final.events, initial.events, 'GRID/KIT part switches preserve all recorded parts');
    assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
    await page.close();
    console.log('PASS GRID cell ownership, curated kits on GRID/KIT, named-row and partial-page LCDs, displayed part order');
  }

  {
    const page = await startDrums(0);
    const initial = await state(page);
    await tap(page, 9);
    const gridPage = (await state(page)).page;

    for (const [id, name] of [[2, 'ENV'], [3, 'LFO'], [1, 'SEL'], [8, 'ARP']]) {
      const previous = await screen(page);
      // Queue the ordinary function tap and first step in the same task. A
      // contextual hint must neither take keyboard ownership nor open a dead
      // synth page, even before a separate idle input frame.
      await page.evaluate(buttonId => {
        document.querySelector(`button[data-button="${buttonId}"]`).click();
        document.querySelector('button[data-key="2"]').click();
      }, id);
      await idle(page);
      await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1,
        initial.events[3]);
      const current = await state(page);
      assert.equal(current.page, gridPage, `${name} guidance preserves the drum workspace`);
      assert(!current.home && !current.layer && !current.recording && !current.armed && !current.playing);
      await changedScreen(page, previous);
      if (name === 'ENV') await screenshot(page, 'grid-context-guidance');
      await hit(page);
      await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);
    }

    await tap(page, 4);
    await tap(page, 4);
    await audition(page, initial.events, 'repeated EDIT leaves KIT SOUND pads audible without editing the pattern');
    const kitBefore = await state(page);
    await turn(page, 4, 'ArrowDown'); // KIT SOUND level.
    await page.waitForFunction(previous => globalThis.jamEmu.state.groove[1] < previous, kitBefore.groove[1]);
    const toneBefore = (await state(page)).macros[0];
    await turn(page, 5, 'ArrowDown'); // KIT SOUND tone is the real whole-kit filter.
    await page.waitForFunction(previous => globalThis.jamEmu.state.macros[0] < previous, toneBefore);
    const panBefore = (await state(page)).groove[3];
    await turn(page, 6, 'ArrowUp');
    await page.waitForFunction(previous => globalThis.jamEmu.state.groove[3] > previous, panBefore);
    assert.deepEqual((await state(page)).events, initial.events, 'whole-kit level, tone and pan preserve all notes');
    await screenshot(page, 'kit-sound-working-controls');

    // The full KIT SOUND browser includes the 37 factory kits and four real
    // editable SYN defaults. Its empty USR slots must not masquerade as sounds.
    // Curated aliases are intentionally retained by the production snapshot.
    const fullKits = ['ROCK KIT', 'DEEP', 'TIGHT KIT', 'BRIGHT', 'DUST',
      '808 KIT', '909 KIT', '606', '80S', 'VINTAGE', 'TRAP KIT', 'DRILL',
      'DUST KIT', 'SOFT KIT', 'PHONK', 'HOUSE', 'D.HOUSE', 'TECHNO', 'MINIMAL',
      'ELECTRO', 'DISCO', 'GARAGE', 'JUNGLE', 'DUBSTEP', 'DEMBOW', 'AMAPIANO',
      'AFRO', 'LATIN', 'TRIBAL', 'NEON KIT', 'CHIP', 'ARCADE', 'GLITCH', 'INDUSTR',
      'HYPER', 'AMBIENT', 'JAZZ', 'SYN1', 'SYN2', 'SYN3', 'SYN4'];
    await turnBy(page, 3, -46); // Clamp to first populated kit.
    for (const [index, expected] of fullKits.entries()) {
      if (index) await turn(page, 3, 'ArrowUp');
      await page.waitForFunction(sound => globalThis.jamEmu.state.sound === sound, expected);
      const current = await state(page);
      assert.equal(current.page, gridPage);
      assert.equal(current.selected, 3);
      assert.equal(current.tempo, 88);
      assert.deepEqual(current.events, initial.events, `full kit ${expected} changes sound without editing notes`);
      if (expected === 'SYN1')
        await audition(page, initial.events, 'the first populated SYN kit has audible pad defaults');
    }
    await turn(page, 3, 'ArrowUp');
    assert.equal((await state(page)).sound, 'SYN4', 'full kit browsing clamps at the last populated kit');
    await screenshot(page, 'kit-sound-populated-syn-bank');
    await tap(page, 2);
    await audition(page, initial.events, 'ENV guidance on KIT SOUND keeps the first following pad audible');

    await tap(page, 0);
    const fxPage = (await state(page)).page;
    assert.notEqual(fxPage, gridPage, 'FX opens its dedicated whole-kit effects page');
    const fxBefore = await state(page);
    await turn(page, 3, 'ArrowDown');
    await page.waitForFunction(previous => globalThis.jamEmu.state.macros[0] < previous, fxBefore.macros[0]);
    await turn(page, 4, 'ArrowUp');
    await page.waitForFunction(previous => globalThis.jamEmu.state.macros[2] > previous, fxBefore.macros[2]);
    await turn(page, 5, 'ArrowUp');
    await page.waitForFunction(previous => globalThis.jamEmu.state.macros[3] > previous, fxBefore.macros[3]);
    await turn(page, 6, 'ArrowUp');
    await page.waitForFunction(previous => globalThis.jamEmu.state.groove[3] > previous, fxBefore.groove[3]);
    await audition(page, initial.events, 'KIT FX keeps pads audible while changing filter, echo, room and pan');
    await screenshot(page, 'kit-fx-working-controls');

    await turn(page, 0, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 89);
    assert.equal((await state(page)).page, fxPage, 'SELECT changes BPM without navigating away from KIT FX');
    await turn(page, 0, 'ArrowDown');
    await page.waitForFunction(() => globalThis.jamEmu.state.tempo === 88);
    await turn(page, 2, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.sound === 'SOFT KIT');
    assert.equal((await state(page)).page, fxPage, 'PRESETS keeps the curated eight-kit browser on KIT FX');

    const fxPages = [fxPage];
    for (const name of ['slicer', 'insert', 'insert-mix', 'all-echo', 'all-room']) {
      await tap(page, 0);
      const current = await state(page);
      assert(!fxPages.includes(current.page), `${name} is a distinct applicable effects page`);
      fxPages.push(current.page);
      assert(!current.home && !current.layer && !current.recording && !current.armed && !current.playing);
      await audition(page, initial.events, `${name} keeps the keyboard playing drums`);
      if (name === 'all-echo' || name === 'all-room') {
        const previous = await screen(page);
        await turn(page, 3, 'ArrowDown');
        await changedScreen(page, previous);
        await screenshot(page, name);
      }
    }
    await tap(page, 0);
    assert.equal((await state(page)).page, fxPage,
      'FX wraps directly to KIT FX without silent synth FX, redundant filter or unused chorus pages');
    assert.deepEqual((await state(page)).events, initial.events, 'effects browsing and edits preserve every track');
    await turn(page, 1, 'ArrowUp');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 0 && globalThis.jamEmu.state.homeControls);
    await turn(page, 1, 'ArrowDown');
    await page.waitForFunction(() => globalThis.jamEmu.state.selected === 3 && globalThis.jamEmu.state.homeControls);
    await tap(page, 0);
    assert.equal((await state(page)).page, fxPage, 'fresh drum FX entry starts with the useful whole-kit controls');
    await tap(page, 9);
    assert.equal((await state(page)).page, gridPage, 'SEQ returns from effects to GRID');
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);
    assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
    await page.close();
    console.log('PASS contextual drum guidance and first step, idempotent EDIT, working KIT SOUND/FX, 41 populated kits, six-page FX cycle and left knobs');
  }

  {
    const page = await startDrums(0);
    const initial = await state(page);
    await tap(page, 9);
    const gridPage = (await state(page)).page;
    await screenshot(page, 'seq-bank-1-8');
    const firstBank = await screen(page);
    await silentBankTap(page);
    await changedScreen(page, firstBank);
    await screenshot(page, 'seq-bank-9-16');
    const secondBank = await screen(page);
    let current = await state(page);
    assert.equal(current.page, gridPage);
    assert.equal(current.groove[2], initial.groove[2], 'row banks preserve loop length');
    assert.equal(current.tempo, initial.tempo);
    assert(!current.home && !current.layer && !current.playing && !current.recording && !current.armed);
    assert.deepEqual(current.events, initial.events, 'the first row-bank change preserves all recorded parts');

    await silentBankTap(page);
    await changedScreen(page, secondBank);
    assert.deepEqual(await screen(page), firstBank,
      'two stopped SEQ taps restore the exact original LCD, selected row and step');
    assert.deepEqual((await state(page)).events, initial.events, 'returning to the first row bank changes no notes');

    // Choose steps 17–32 and step 18, empty on every startup lane. Adding the
    // same vertical step on the other row bank must keep occupied-step count
    // at +1. A lost step bank would incorrectly add another occupied step.
    await hit(page, 3);
    await turn(page, 4, 'ArrowUp');
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count + 1, initial.events[3]);
    await silentBankTap(page);
    assert.equal((await state(page)).events[3], initial.events[3] + 1);
    await hit(page);
    assert.equal((await state(page)).events[3], initial.events[3] + 1,
      'the other row bank edits the same step 18, without moving to the first step bank');
    await silentBankTap(page);
    await hit(page);
    assert.equal((await state(page)).events[3], initial.events[3] + 1,
      'removing the first-row-bank hit preserves the other drum on that same step');
    await silentBankTap(page);
    await hit(page);
    await page.waitForFunction(count => globalThis.jamEmu.state.events[3] === count, initial.events[3]);
    assert.deepEqual((await state(page)).events, initial.events, 'both independent row-bank edits are reversible');

    // Let the normal hot-knob highlight expire before comparing exact LCDs.
    const beforeSettling = (await state(page)).frames;
    await page.waitForFunction(frame => globalThis.jamEmu.state.frames >= frame + 45, beforeSettling);
    const selectedSecondBank = await screen(page);
    for (const [buttonId, name] of [[6, 'Home'], [4, 'KIT SOUND'], [0, 'KIT FX']]) {
      await tap(page, buttonId);
      await silentBankTap(page);
      assert.deepEqual(await screen(page), selectedSecondBank,
        `SEQ entry from ${name} preserves the selected row bank and edit step`);
      assert.deepEqual((await state(page)).events, initial.events);
    }
    await tap(page, 10);
    await page.waitForFunction(() => globalThis.jamEmu.state.playing);
    await tap(page, 9);
    current = await state(page);
    assert(current.playing && !current.recording && !current.armed,
      'changing the visible drum rows leaves playback running');
    assert.equal(current.page, gridPage);
    assert.equal(current.groove[2], initial.groove[2]);
    assert.deepEqual(current.events, initial.events, 'row-bank navigation during playback preserves the loop');
    await tap(page, 10);
    await page.waitForFunction(() => !globalThis.jamEmu.state.playing);
    assert.equal(await page.evaluate(() => globalThis.jamEmu.failed), false);
    await page.close();
    console.log('PASS silent SEQ row-bank toggle, exact two-tap LCD return, preserved step-18 edits and loop, remembered entry from Home/KIT/FX');
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
  console.log(`Drum navigation: 6 Chromium scenarios passed using the production panel, AudioWorklet, rendered LCD and audio. Physical FM-1 behavior remains unverified. Native 240 × 240 LCD screenshots: ${evidence}`);
} finally {
  await browser?.close();
  await new Promise(done => server.close(done));
}
