// SPDX-License-Identifier: GPL-3.0-only
// Production recovery policy, page and Updater over a simulated Web MIDI wire.
// No target firmware executes and these checks do not validate physical flashing.
// . tools/cloud/activate.sh && node tests/jam_recovery_install_browser.mjs
// JAM_RECOVERY_SITE_ROOT=build/jambox-recovery-preview tests the staged preview.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { prepareRecoveryCandidate, validateRecoveryManifest } from '../web/jam-recovery-policy.js';
import { fakeWebMIDI } from './helpers/jam_recovery_midi_browser.mjs';

const playwright = process.env.PLAYWRIGHT_MODULE ? pathToFileURL(resolve(process.env.PLAYWRIGHT_MODULE)).href : 'playwright-core';
const { chromium } = await import(playwright);
const packaged = !!process.env.JAM_RECOVERY_SITE_ROOT;
const root = resolve(process.env.JAM_RECOVERY_SITE_ROOT || 'web');
const artifacts = resolve(packaged ? `${root}/firmware` : process.env.JAM_RECOVERY_CANDIDATE || 'build/jambox-recovery');
const manifestBytes = await readFile(resolve(artifacts, 'manifest.json'));
const manifest = validateRecoveryManifest(JSON.parse(manifestBytes));
const firmware = await readFile(resolve(artifacts, manifest.firmware.file));
const logicalImage = Buffer.from((await prepareRecoveryCandidate(manifest, firmware)).image());
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const trial = JSON.parse(await readFile(resolve(artifacts, 'trial.json')));
const receipt = JSON.parse(await readFile(resolve(artifacts, 'build-receipt.json')));
// Even a rehashed manifest cannot legitimize a damaged canonical container.
const noncanonical = Buffer.from(firmware); noncanonical[100] ^= 1;
const rehashed = JSON.parse(JSON.stringify(manifest)); rehashed.firmware.sha256 = digest(noncanonical);
await assert.rejects(() => prepareRecoveryCandidate(rehashed, noncanonical));
const backupFlash = Buffer.alloc(1048576, 0xa7); // Synthetic local-file fixture; never a real recovery dump.
const backupReport = Buffer.from(JSON.stringify({ tool: 'MvaveFM1Unbricker fm1_unbrick.py 1.1.0',
  device: { vendor: 'WL82', product: 'UBOOT1.00' }, chip_key: '0x980f', flash_id: '0x856014',
  full_backup: 'full-flash-before.bin', backup_sha256: digest(backupFlash) }));
const evidence = resolve('build/jam-recovery-install-evidence');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json' };
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    if (pathname === '/favicon.ico') { response.writeHead(204); response.end(); return; }
    const base = !packaged && pathname.startsWith('/firmware/') ? artifacts : root;
    const relative = base === artifacts ? pathname.slice('/firmware'.length) : pathname;
    const file = resolve(base, '.' + relative + (relative.endsWith('/') ? 'index.html' : ''));
    if (!file.startsWith(base + sep)) throw new Error('Outside site');
    const bytes = await readFile(file);
    response.writeHead(200, { 'Content-Type': mime[extname(file)] || 'application/octet-stream' }); response.end(bytes);
  } catch { response.writeHead(404); response.end('Not found'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const address = `http://127.0.0.1:${server.address().port}`;
const installURL = `${address}/${packaged ? 'install' : 'recovery'}/`;
let browser, passed = 0;
const errors = [], network = [];
const caseFilter = process.env.JAM_RECOVERY_CASE ? new RegExp(process.env.JAM_RECOVERY_CASE) : null;
async function runCase(name, config, body, options = {}) {
  if (caseFilter && !caseFilter.test(name)) return;
  const page = await browser.newPage({ viewport: { width: 1280, height: 1080 }, ...options });
  page.setDefaultTimeout(20000);
  page.on('pageerror', error => errors.push(`${name}: ${error.message}`));
  page.on('console', message => { if (message.type() === 'error') errors.push(`${name}: ${message.text()}`); });
  page.on('request', request => network.push({ url: request.url(), method: request.method() }));
  try {
    await page.addInitScript(fakeWebMIDI, { config, image: logicalImage.toString('base64') });
    if (config.badBundle) await page.route('**/firmware/trial.json', route => route.fulfill({ status: 200, body: '{}' }));
    if (config.badBinding) await page.route('**/firmware/trial.json', route => route.fulfill({
      status: 200, body: JSON.stringify({ ...trial, firmware_sha256: '0'.repeat(64) }) }));
    if (config.badFirmware) {
      const broken = Buffer.from(firmware); broken[100] ^= 1;
      await page.route(`**/firmware/${manifest.firmware.file}`, route => route.fulfill({ status: 200, body: broken }));
    }
    if (config.badLoaderRecovery) {
      const changedReceipt = JSON.parse(JSON.stringify(receipt)); changedReceipt.configuration.loader.flags.JAMBOX_RECOVERY = 1;
      const receiptBody = Buffer.from(JSON.stringify(changedReceipt));
      const changedManifest = JSON.parse(manifestBytes); changedManifest.build.receipt_sha256 = digest(receiptBody);
      const manifestBody = Buffer.from(JSON.stringify(changedManifest));
      await page.route('**/firmware/build-receipt.json', route => route.fulfill({ status: 200, body: receiptBody }));
      await page.route('**/firmware/manifest.json', route => route.fulfill({ status: 200, body: manifestBody }));
      await page.route('**/firmware/trial.json', route => route.fulfill({ status: 200, body: JSON.stringify({ ...trial, manifest_sha256: digest(manifestBody) }) }));
    }
    if (config.badFlags) {
      const bad = JSON.parse(manifestBytes); bad.build.flags.JAMBOX_RECOVERY = 0;
      const body = Buffer.from(JSON.stringify(bad));
      await page.route('**/firmware/manifest.json', route => route.fulfill({ status: 200, body }));
      await page.route('**/firmware/trial.json', route => route.fulfill({ status: 200, body: JSON.stringify({ ...trial, manifest_sha256: digest(body) }) }));
    }
    await page.goto(installURL);
    await page.waitForFunction(() => !document.getElementById('firmware-label').textContent.includes('Checking'));
    assert.deepEqual(await page.evaluate(() => fakeMIDI.snapshot().permissions), [], 'MIDI permission requires a click');
    assert(await page.locator('#install-device').isDisabled());
    await body(page);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).errors, [], 'independent fake checks every packet and image byte');
    passed++; console.log(`PASS ${name}`);
  } catch (error) {
    console.error(`FAIL ${name}`, await page.evaluate(() => ({ snapshot: fakeMIDI.snapshot(), status: document.getElementById('status')?.textContent })));
    throw error;
  } finally { await page.close(); }
}
async function connect(page) {
  await page.locator('#connect-device').click();
  await page.waitForFunction(() => !document.getElementById('connect-device').disabled);
  assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, [], 'inspection never starts an update');
}
async function backup(page) {
  await page.locator('#backup-report').setInputFiles({ name: 'backup.json', mimeType: 'application/json', buffer: backupReport });
  await page.locator('#backup-flash').setInputFiles({ name: 'full-flash-before.bin', mimeType: 'application/octet-stream', buffer: backupFlash });
  await page.waitForFunction(() => document.getElementById('backup-state').dataset.ready === 'true');
}
async function ready(page) {
  await connect(page); await backup(page); await page.locator('#disposable-jam').check();
  await page.waitForFunction(() => !document.getElementById('install-device').disabled);
}
async function result(page) {
  await page.locator('#result-card').waitFor({ state: 'visible', timeout: 40000 });
  return JSON.parse(await page.locator('#report-details').textContent());
}
async function noOpenLinks(page) {
  await page.waitForFunction(() => fakeMIDI.snapshot().open === 0 && fakeMIDI.snapshot().listeners === 0);
}
async function direct(page, code, args = {}) {
  return page.evaluate(async ({ code, args, report, flash }) => {
    const api = await import('/jam-recovery-policy.js');
    const installer = new api.RecoveryInstaller(fakeMIDI.access);
    const bundle = await api.loadRecoveryBundle({ baseURL: new URL('/firmware/', location.href) });
    const from64 = text => Uint8Array.from(atob(text), char => char.charCodeAt(0));
    const proof = await api.validateRecoveryBackup(from64(report), from64(flash));
    return await new Function('api', 'installer', 'bundle', 'proof', 'args', `return (async()=>{${code}})()`)(api, installer, bundle, proof, args);
  }, { code, args, report: backupReport.toString('base64'), flash: backupFlash.toString('base64') });
}

try {
  await mkdir(evidence, { recursive: true });
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox'] });
  await runCase('unsupported browser', { unsupported: true }, async page => {
    assert.match(await page.locator('#status').textContent(), /doesn’t support Web MIDI/);
    assert(await page.locator('#connect-device').isDisabled());
  });
  await runCase('permission denial sends nothing', { denied: true }, async page => {
    await connect(page); assert.match(await page.locator('#status').textContent(), /wasn’t allowed/);
    assert.equal((await page.evaluate(() => fakeMIDI.snapshot())).sent, 0);
  });
  for (const [label, config] of [['factory', { identity: 'FM-1_015' }], ['Sloop', { identity: 'FM-1_900' }],
    ['rescue', { identity: 'FM-1_000' }], ['future family schema', { futureSchema: 2 }],
    ['unknown release', { identity: 'FM-1_701' }], ['missing family', { missingAppProbe: true }]]) {
    await runCase(`${label} is rejected`, config, async page => {
      await connect(page); await backup(page); await page.locator('#disposable-jam').check();
      assert(await page.locator('#install-device').isDisabled());
      assert.match(await page.locator('#device-label').textContent(), /requires a running supported JamBox/);
      await noOpenLinks(page);
    });
  }
  for (const fault of ['badBundle', 'badBinding', 'badFlags', 'badFirmware', 'badLoaderRecovery']) await runCase(`${fault} blocks installation before MIDI writes`, { [fault]: true }, async page => {
    assert.match(await page.locator('#firmware-label').textContent(), /could not be verified/);
    await connect(page); await backup(page); await page.locator('#disposable-jam').check();
    assert(await page.locator('#install-device').isDisabled()); assert(await page.locator('#reload-firmware').isVisible());
    await noOpenLinks(page);
  });
  await runCase('local backup and disposable jam acknowledgement both required', {}, async page => {
    await connect(page);
    await page.locator('#disposable-jam').check(); assert(await page.locator('#install-device').isDisabled());
    await page.locator('#disposable-jam').uncheck(); await backup(page);
    assert(await page.locator('#install-device').isDisabled());
    await page.locator('#disposable-jam').check(); assert(await page.locator('#install-device').isEnabled());
    await page.locator('#backup-flash').setInputFiles({ name: 'full-flash-before.bin', mimeType: 'application/octet-stream', buffer: Buffer.alloc(1048576) });
    await page.waitForFunction(() => document.getElementById('backup-state').dataset.error === 'true');
    assert(await page.locator('#install-device').isDisabled()); await noOpenLinks(page);
  });
  await runCase('production controller rejects missing and forged backup or missing acknowledgement', {}, async page => {
    const checks = await direct(page, `const pair=installer.listPairs()[0], failures=[];
      for(const options of [{}, {backup:{...proof},disposableJamReady:true}, {backup:proof}, {backup:proof,disposableJamReady:'true'}]) {
        try {await installer.install(bundle,pair,options); failures.push('accepted');} catch(e){failures.push(e.code);}
      } return {failures,snapshot:fakeMIDI.snapshot()};`);
    assert.deepEqual(checks.failures, ['backup','backup','backup','backup']);
    assert.equal(checks.snapshot.sent, 0); assert.deepEqual(checks.snapshot.upgrades, []);
  });
  await runCase('multiple MIDI pairs block trial', { count: 2 }, async page => {
    await connect(page); await backup(page); await page.locator('#disposable-jam').check();
    assert(await page.locator('#pair-field').isVisible()); assert(await page.locator('#install-device').isDisabled());
    await page.locator('#midi-pair').selectOption({ index: 1 }); await connect(page);
    assert(await page.locator('#install-device').isDisabled()); assert.match(await page.locator('#status').textContent(), /only one FM-1/);
    await noOpenLinks(page);
  });
  await runCase('new device invalidates checked readiness', {}, async page => {
    await ready(page); await page.evaluate(() => fakeMIDI.add());
    assert(await page.locator('#install-device').isDisabled());
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []);
  });
  await runCase('hidden duplicate groups block production controller', { count: 3, hiddenDuplicates: true }, async page => {
    const response = await direct(page, `const pair=installer.listPairs()[0];
      const observed=await installer.inspect(pair);let code;try{await installer.install(bundle,pair,{backup:proof,disposableJamReady:true});}catch(e){code=e.code;}
      return {observed,code,snapshot:fakeMIDI.snapshot()};`);
    assert.equal(response.observed.eligible, false); assert.equal(response.observed.uniqueDevice, false);
    assert.equal(response.code, 'ambiguous'); assert.deepEqual(response.snapshot.upgrades, []); await noOpenLinks(page);
  });
  await runCase('disconnect invalidates checked readiness', {}, async page => {
    await ready(page); await page.evaluate(() => fakeMIDI.disconnect(0));
    assert(await page.locator('#install-device').isDisabled()); assert.match(await page.locator('#status').textContent(), /disconnected/);
  });
  await runCase('stale explicitly checked pair cannot become another device', {}, async page => {
    const response = await direct(page, `const pair=installer.listPairs()[0]; await installer.inspect(pair);
      fakeMIDI.disconnect(0);fakeMIDI.add();let code;try{await installer.install(bundle,pair,{backup:proof,disposableJamReady:true});}catch(e){code=e.code;}
      return {code,snapshot:fakeMIDI.snapshot()};`);
    assert.equal(response.code, 'notfound'); assert.deepEqual(response.snapshot.upgrades, []); await noOpenLinks(page);
  });
  for (const config of [{ changedIdentity: 'FM-1_015' }, { changedAppFamily: true }]) await runCase(`fresh starting-device guard rejects ${Object.keys(config)[0]}`, config, async page => {
    await ready(page); await page.locator('#install-device').click(); const report = await result(page);
    assert.equal(report.outcome, 'error'); assert.equal(report.updateStarted, false);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
  });
  await runCase('cancelling preparation never sends upgrade', {}, async page => {
    await ready(page); await page.evaluate(() => {fakeMIDI.devices[0].identityDelay=300;});
    await page.locator('#install-device').click(); await page.locator('#cancel-prepare').click();
    const report=await result(page);assert.equal(report.outcome,'cancelled');assert.equal(report.updateStarted,false);
    assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades, []); await noOpenLinks(page);
  });
  for (const config of [{ disconnectAtStart: true }, { disconnectDuringWrite: true }, { badLoaderFamily: true }, { badFinalRelease: true }])
    await runCase(`started failure ${Object.keys(config)[0]} locks all retry controls`, config, async page => {
      await ready(page); await page.locator('#install-device').click(); const report=await result(page);
      assert.equal(report.outcome,'error');assert.equal(report.updateStarted,true);assert.equal(report.finalIdentity,null);
      assert(await page.locator('#install-device').isDisabled());assert(await page.locator('#connect-device').isDisabled());
      assert(await page.locator('#cancel-prepare').isHidden()); assert.match(await page.locator('#result-guidance').textContent(), /Keep the FM-1 powered and connected/);
      const before=(await page.evaluate(() => fakeMIDI.snapshot())).upgrades;
      await page.waitForTimeout(150);assert.deepEqual((await page.evaluate(() => fakeMIDI.snapshot())).upgrades,before,'no automatic retry');
      assert(await page.evaluate(()=>{const event=new Event('beforeunload',{cancelable:true});dispatchEvent(event);return event.defaultPrevented;}));
      await noOpenLinks(page);
    });
  await runCase('complete normal to loader to normal transfer and private report', {}, async page => {
    await ready(page); await page.screenshot({ path: resolve(evidence, 'desktop-ready.png'), fullPage: true });
    await page.locator('#install-device').click(); const report=await result(page);
    assert.equal(report.outcome,'completed');assert.equal(report.finalIdentity,'FM-1_700');assert.equal(report.sourceIdentity,'FM-1_700');
    assert.equal(report.transition,'jambox-normal-to-jambox');assert.equal(report.hardwareValidated,false);assert.equal(report.automaticSaving,false);
    assert.equal(report.firmwareSHA256,digest(firmware));assert(report.stages.some(entry=>entry.stage==='done'));
    const snapshot=await page.evaluate(()=>fakeMIDI.snapshot());assert.deepEqual(snapshot.upgrades,['app','loader']);
    assert.deepEqual(snapshot.transfers.map(t=>t.bytes),[logicalImage.length,logicalImage.length]);
    assert.deepEqual(report.requestCounts,{verify:snapshot.transfers[0].chunks,write:snapshot.transfers[1].chunks});
    const downloadEvent=page.waitForEvent('download');await page.locator('#save-report').click();const download=await downloadEvent;
    assert.equal(download.suggestedFilename(),'jamboxfm-recovery-install-report.json');
    const exported=await readFile(await download.path(),'utf8');assert.deepEqual(JSON.parse(exported),report);
    assert(!/PRIVATE|inputId|outputId|portName|serial|backupSHA256|backup_sha256/i.test(exported),'report excludes backup and port identifiers');
    assert(!await page.evaluate(()=>{const event=new Event('beforeunload',{cancelable:true});dispatchEvent(event);return event.defaultPrevented;}));
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await page.screenshot({path:resolve(evidence,'desktop-complete.png'),fullPage:true});await noOpenLinks(page);
  });
  await runCase('delayed post-flash identity keeps result pending until actual response', { finalIdentityDelay: 600 }, async page => {
    await ready(page);await page.locator('#install-device').click();
    await page.waitForFunction(()=>fakeMIDI.devices[0].stage==='final', null, {timeout:40000});
    assert(await page.locator('#result-card').isHidden());
    const report=await result(page);assert.equal(report.outcome,'completed');assert.equal(report.finalIdentity,'FM-1_700');await noOpenLinks(page);
  });
  await runCase('mobile layout and labelled local-only backup actions', {}, async page => {
    await ready(page);assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    assert.equal(await page.locator('#status').getAttribute('role'),'status');assert.equal(await page.locator('#status').getAttribute('aria-live'),'polite');
    assert.equal(await page.locator('label[for="backup-report"]').count(),1);assert.equal(await page.locator('label[for="backup-flash"]').count(),1);
    assert.match(await page.locator('#disposable-jam').locator('..').textContent(),/loses its current temporary music/);
    await page.screenshot({path:resolve(evidence,'mobile-ready.png'),fullPage:true});
    await page.setViewportSize({width:320,height:740});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await noOpenLinks(page);
  }, { viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  assert.deepEqual(errors,[],'no browser JavaScript or console errors');
  assert(network.every(request=>request.method==='GET'&&request.url.startsWith(address+'/')),'all network requests are local GET; no backup upload');
  console.log(`Recovery installer: ${passed} Chromium cases passed. Wire simulation only; physical installation and saving remain unverified. Screenshots: ${evidence}`);
} finally { await browser?.close(); await new Promise(resolve=>server.close(resolve)); }
