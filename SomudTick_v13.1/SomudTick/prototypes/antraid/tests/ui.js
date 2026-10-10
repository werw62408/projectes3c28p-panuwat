// UI walk-through with button presses (like a player on the board), screenshots of every screen.
const { chromium } = require('/opt/node22/lib/node_modules/playwright');
const path = require('path'), fs = require('fs');
const OUT = process.argv[2] || path.join(__dirname, 'shots');
fs.mkdirSync(OUT, { recursive: true });
(async () => {
  const b = await chromium.launch({ args: ['--ignore-certificate-errors'] });
  const p = await b.newPage({ viewport: { width: 1000, height: 1100 } });
  const errs = [];
  p.on('pageerror', (e) => errs.push('pageerror: ' + e.message + ' ' + (e.stack || '').split('\n')[1]));
  await p.goto('file://' + path.join(__dirname, '..', 'index.html'));
  await p.evaluate(() => localStorage.clear());
  await p.reload(); await p.waitForTimeout(700);
  let n = 0;
  const shot = async (name) => { n++; await p.waitForTimeout(60); await p.locator('#cv').screenshot({ path: path.join(OUT, String(n).padStart(2, '0') + '_' + name + '.png') }); };
  const press = async (k, c = 1) => { for (let i = 0; i < c; i++) { await p.evaluate((k) => AR.press(k), k); await p.waitForTimeout(25); } };
  const dir = async (d, c = 1) => { for (let i = 0; i < c; i++) { await p.evaluate((d) => AR.dirPress(d), d); await p.waitForTimeout(15); } };
  const top = () => p.evaluate(() => { const t = AR.UI.top(); return t ? (t.title || '') + ' | ' + (t.items[t.sel] ? t.items[t.sel].t : '') : 'none'; });
  const fast = (s) => p.evaluate((s) => AR.fast(s), s);
  // ---- home screens
  await shot('home');
  await dir('down', 3); await press('A'); await shot('deck_edit'); await press('D');
  await dir('down', 1); await press('A'); await shot('forge'); await press('D');
  await dir('down', 1); await press('A'); await shot('perks'); await press('D');
  await dir('down', 1); await press('A'); await shot('shop'); await press('A'); await shot('shop_card'); await press('D');
  await dir('down', 1); await press('A'); await shot('qpath'); await press('D');
  await dir('down', 1); await press('A'); await shot('almanac'); await press('D');
  await dir('down', 1); await press('A'); await shot('help'); await press('D');
  await dir('up', 9); console.log('home sel:', await top());
  // ---- start a game with buttons
  await press('A'); await shot('your_deck'); await press('A'); await shot('boosts');
  await dir('right'); await press('A'); await press('A');
  console.log('mode', await p.evaluate(() => AR.APP.mode));
  await shot('game_start');
  // queen: add loaders
  await press('F'); await shot('queen_menu');
  for (let i = 0; i < 12; i++) { const t = await top(); if (t.includes('Loader')) break; await dir('down'); }
  await press('A', 2); await shot('queen_queued'); await press('D');
  // build mode: dig a few tiles to the right of the queen room, then place a barracks
  await press('C'); await shot('build_mode');
  await dir('down', 3); await press('A'); await dir('right'); await press('A'); await dir('right'); await press('A');
  await shot('build_dig_marked');
  await press('D');
  await fast(45); await shot('after_45s_nest');
  await press('C'); await press('F', 3); await shot('build_barracks_tool');
  await dir('left', 1); await press('A'); await shot('build_barracks_placed'); await press('D');
  // priorities + menu
  await press('D'); await shot('game_menu');
  await dir('down', 4); await press('A'); await shot('priority'); await dir('right'); await press('D');
  await dir('down', 1); await press('A'); await shot('workshop'); await press('D');
  await press('D');
  // surface, time passes
  await press('B'); await fast(150); await shot('surface_150s');
  // squads: new squad of bodyguards, patrol left of the nest (quick preset)
  await p.evaluate(() => { for (let i = 0; i < 3; i++) { const a = AR.G.ants.find((x) => x.key === 'loader'); } AR.enqueue('bodyguard'); AR.enqueue('bodyguard'); AR.enqueue('bodyguard'); });
  await fast(120);
  await press('E'); await shot('squads_list');
  await press('A'); await shot('squad_new'); 
  for (let i = 0; i < 12; i++) { const t = await top(); if (t.includes('ORDER')) break; await dir('down'); }
  await press('A'); await shot('squad_orders');
  for (let i = 0; i < 12; i++) { const t = await top(); if (t.includes('Left of the nest')) break; await dir('down'); }
  await press('A'); await press('D'); await press('D');
  await fast(20); await shot('squad_patrol_map');
  // squad pick on map: guard spot
  await press('E'); await press('A');
  for (let i = 0; i < 12; i++) { const t = await top(); if (t.includes('ORDER')) break; await dir('down'); }
  await press('A'); await dir('down'); await press('A');
  await p.evaluate(() => AR.holdDir('right', true)); await p.waitForTimeout(400); await p.evaluate(() => AR.holdDir('right', false));
  await shot('pick_guard'); await press('A'); await fast(15); await shot('squad_guard');
  // base menu: put the cursor on a known base
  const known = await p.evaluate(() => { const G = AR.G; G.bases.forEach((b) => { b.known = true; }); const b = G.bases[0]; G.view = 'S'; G.cur.S.x = b.x; G.cur.S.y = b.y; G.fog.fill(1); G.fogVer++; return b.kind; });
  await p.evaluate(() => { const c = AR.G.cur.S, cam = AR.G.cam.S; cam.x = c.x - 120; cam.y = c.y - 139; });
  await shot('base_hover'); await press('A'); await shot('base_menu'); await press('D');
  // zoom 2
  await p.evaluate(() => { AR.G.zoomV.S = 1; }); await shot('zoom1'); await p.evaluate(() => { AR.G.zoomV.S = 2; });
  // end: give up -> results -> home
  await press('D'); for (let i = 0; i < 14; i++) { const t = await top(); if (t.includes('Give up')) break; await dir('down'); }
  await press('A'); await press('A'); await shot('results'); await press('A'); await shot('home_after');
  console.log(errs.length ? errs.join('\n') : 'no errors');
  await b.close();
})();
