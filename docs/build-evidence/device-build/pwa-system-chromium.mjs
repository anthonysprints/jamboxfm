// External runtime adapter: does not change the Sloop checkout or test assertions.
// Resolve the same global Playwright instance as tests/pwa_test.mjs's NODE_PATH fallback.
import { createRequire } from 'node:module';
import { join } from 'node:path';

const req = createRequire(join(process.cwd(), 'x.js'));
const resolved = req.resolve('playwright', {
  paths: (process.env.NODE_PATH || '').split(':').filter(Boolean),
});
const { chromium } = req(resolved);
const version = req(req.resolve('playwright/package.json', {
  paths: (process.env.NODE_PATH || '').split(':').filter(Boolean),
})).version;
const originalLaunch = chromium.launch.bind(chromium);
chromium.launch = (options = {}) => {
  const effective = { ...options, executablePath: '/usr/bin/chromium' };
  console.error('pwa external launcher:', JSON.stringify({
    playwright: resolved, version, node: process.version, options: effective,
  }));
  return originalLaunch(effective);
};
