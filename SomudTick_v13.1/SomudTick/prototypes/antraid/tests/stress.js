// Extreme cases: button mashing everywhere, corrupt save, a very long game, a nest dug full, phone width.
const { chromium } = require('/opt/node22/lib/node_modules/playwright');
const path = require('path');
const OUT = process.argv[2] || path.join(__dirname, 'shots');
(async () => {
  const b = await chromium.launch({ args: ['--ignore-certificate-errors'] });
  const errs = [];
  const page = async (w, h) => { const p = await b.newPage({ viewport: { width: w, height: h } }); p.on('pageerror', (e) => errs.push(e.message + ' ' + (e.stack || '').split('\n')[1])); return p; };
  let p = await page(1000, 1100);
  const url = 'file://' + path.join(__dirname, '..', 'index.html');
  // 1. corrupt save
  await p.goto(url); await p.evaluate(() => localStorage.setItem('antraid.v1', '{bad json')); await p.reload(); await p.waitForTimeout(400);
  console.log('corrupt save -> sugar', await p.evaluate(() => AR.META.sugar));
  // 2. mash buttons and stick (home + game), 6000 random inputs with time passing
  const r = await p.evaluate(() => {
    const keys = ['A', 'B', 'C', 'D', 'E', 'F', 'K'], dirs = ['up', 'down', 'left', 'right'];
    let games = 0, s = 12345; const rnd = (n) => { s = (s * 1103515245 + 12345) & 0x7fffffff; return s % n; };
    for (let i = 0; i < 6000; i++) {
      const k = rnd(10);
      if (k < 5) AR.press(keys[rnd(7)]); else if (k < 9) AR.dirPress(dirs[rnd(4)]); else AR.tap(rnd(240), rnd(320));
      if (AR.APP.mode === 'game' && AR.G) { if (!AR.G._seen) { AR.G._seen = 1; games++; } AR.fast(0.5); if (rnd(400) === 0) { AR.UI.clear(); AR.G.over = 'lose'; AR.openHome(); } }
      if (i % 500 === 0) AR.render();
    }
    return { games, mode: AR.APP.mode, stack: AR.UI.stack.length };
  });
  console.log('mash:', JSON.stringify(r));
  // 3. long game, max speed: 60 game minutes with nothing done but eggs
  const L = await p.evaluate(() => {
    AR.UI.clear();
    AR.newRun({ loc: 0, lvl: 2, seed: 99, deck: [{ key: 'uqueen', rar: 1 }, { key: 'builder', rar: 0 }, { key: 'loader', rar: 0 }, { key: 'seeker', rar: 0 }, { key: 'attacker', rar: 0 }, { key: 'defender', rar: 0 }], perks: [], boosts: ['queen3', 'bar4'] });
    AR.initJobs(); AR.APP.mode = 'game';
    const G = AR.G; G.zoomV = { S: 2, U: 1 };
    const t0 = performance.now(); let maxMobs = 0, maxAnts = 0;
    for (let m = 0; m < 60 && !G.over; m++) {
      for (let k = 0; k < 20; k++) { AR.enqueue(['loader', 'defender', 'attacker', 'builder'][k % 4]); }
      AR.fast(60); maxMobs = Math.max(maxMobs, G.mobs.length); maxAnts = Math.max(maxAnts, G.ants.length);
    }
    return { over: G.over, t: Math.round(G.t), ms: Math.round(performance.now() - t0), maxMobs, maxAnts, foods: G.foods.length, fx: G.fx.length };
  });
  console.log('long game:', JSON.stringify(L));
  // 4. a nest dug full: mark every tile, run, place rooms everywhere, draw
  const D = await p.evaluate(() => {
    AR.newRun({ loc: 1, lvl: 0, seed: 7, deck: [{ key: 'uqueen', rar: 3 }, { key: 'builder', rar: 3 }, { key: 'loader', rar: 0 }, { key: 'seeker', rar: 0 }, { key: 'attacker', rar: 0 }], perks: ['diggers'], boosts: ['build2'] });
    AR.initJobs(); AR.APP.mode = 'game'; const G = AR.G; G.zoomV = { S: 2, U: 1 };
    G.bases.forEach((b) => { b.waveT = 1e9; b.thiefT = 1e9; });
    for (let i = 0; i < 30 * 40; i++) AR.markDig(i);
    for (let k = 0; k < 10; k++) { AR.enqueue('builder'); AR.fast(120); }
    let placed = 0; for (let i = 0; i < 30 * 40; i++) for (const kind of ['br', 'fs', 'qs']) { const t = AR.canPlace(kind, i); if (t && placed < 80) { AR.addBuilding(kind, t); placed++; break; } }
    G.pile.food += 2000; AR.fast(300);
    G.view = 'U'; AR.render();
    return { dug: G.st.dug, placed, done: G.builds.filter((b) => b.done).length, slots: Math.min(8, Math.floor(G.builds.filter(b => b.kind === 'qs' && b.done).length / 2)), over: G.over };
  });
  console.log('dug full:', JSON.stringify(D));
  await p.locator('#cv').screenshot({ path: path.join(OUT, 'stress_nest.png') });
  // 5. phone width: no sideways scroll
  const ph = await page(390, 844); await ph.goto(url); await ph.waitForTimeout(500);
  console.log('phone scrollWidth', await ph.evaluate(() => document.documentElement.scrollWidth));
  await ph.screenshot({ path: path.join(OUT, 'phone.png'), fullPage: true });
  console.log(errs.length ? 'ERRORS\n' + [...new Set(errs)].join('\n') : 'no errors');
  await b.close();
})();
