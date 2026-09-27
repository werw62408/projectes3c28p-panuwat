// Render the 3D page in Chromium and save pictures of each view (images/*.png)
const { chromium } = require('/opt/node22/lib/node_modules/playwright');
(async () => {
  const out = process.argv[2];
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-webgl', '--ignore-gpu-blocklist'] });
  const p = await b.newPage({ viewport: { width: 1100, height: 900 }, deviceScaleFactor: 1 });
  const V = process.env.VENDOR;
  await p.route(/three\.min\.js|GLTFLoader\.js|OrbitControls\.js/, r => { const u = r.request().url(); const f = u.includes('GLTF') ? 'GLTFLoader.js' : u.includes('Orbit') ? 'OrbitControls.js' : 'three.min.js'; r.fulfill({ path: V + '/' + f, contentType: 'application/javascript' }); });
  await p.route(/fonts\.(googleapis|gstatic)/, r => r.abort());
  p.on('console', m => { if (m.type() === 'error') console.log('CONSOLE', m.text()); });
  p.on('pageerror', e => console.log('PAGEERR', e.message));
  await p.goto('file://' + __dirname + '/out/_preview.html');
  await p.waitForFunction(() => window.__view, null, { timeout: 60000 });
  await p.waitForTimeout(1500);
  const stage = await p.$('.stagebox');
  for (const v of ['play', 'closed', 'hinge', 'flat']) {
    await p.evaluate(v => window.__view(v), v); await p.waitForTimeout(900);
    await stage.screenshot({ path: `${out}/case_${v}.png` });
  }
  await p.screenshot({ path: `${out}/page_full.png`, fullPage: true });
  await b.close();
})();
