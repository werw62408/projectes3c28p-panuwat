// Tests the phone page's Deck tab and "send game" button (v11.7) in headless Chromium against a fake board.
//   node test_deck.js ../../SomudTick_v11.7/SomudTick/webpage.h
const path = require('path');
let chromium; try { ({ chromium } = require('playwright')); } catch (e) { ({ chromium } = require(path.join(process.execPath, '../../lib/node_modules/playwright'))); }
const fs = require('fs');
const src = fs.readFileSync(process.argv[2], 'utf8');
const html = src.slice(src.indexOf('R"HTMLPAGE(') + 11, src.indexOf(')HTMLPAGE"'));
const R = ok => ok ? 'PASS' : 'FAIL';
(async () => {
  const b = await chromium.launch(); const page = await b.newPage();
  let deck = Array.from({ length: 24 }, (_, i) => ({ label: 'K' + i, kind: i < 12 ? 2 : 1, mod: 0, key: 4, media: 0xCD, text: '' }));
  let posted = null; const ups = []; const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.route('**/*', async r => {
    const u = new URL(r.request().url());
    if (u.pathname === '/') return r.fulfill({ contentType: 'text/html', body: html });
    if (u.pathname === '/api/deck') { if (r.request().method() === 'POST') { posted = JSON.parse(r.request().postData()); if (posted.length) deck = posted; } return r.fulfill({ contentType: 'application/json', body: JSON.stringify(deck) }); }
    if (u.pathname === '/upload') { ups.push({ dir: u.searchParams.get('dir'), body: r.request().postDataBuffer() }); return r.fulfill({ body: 'ok' }); }
    if (u.pathname === '/api/sd') return r.fulfill({ contentType: 'application/json', body: '{"ok":false}' });
    return r.fulfill({ contentType: 'application/json', body: '{}' });
  });
  await page.goto('http://board.local/');
  await page.evaluate(() => go('deck')); await page.waitForTimeout(400);
  const cards = await page.$$eval('[data-d]', e => e.length);
  console.log(`W1 Deck tab shows ${cards} keys ${R(cards === 24)}`);
  // key 13 (page 2 #2): shortcut Ctrl+Shift+S, label "Snip"
  await page.fill('[data-d="13"] [data-k="label"]', 'Snip');
  await page.check('[data-d="13"] [data-mod="1"]'); await page.check('[data-d="13"] [data-mod="2"]');
  await page.selectOption('[data-d="13"] [data-k="key"]', String(0x16));
  // key 0: change to text
  await page.selectOption('[data-d="0"] [data-k="kind"]', '3'); await page.waitForTimeout(200);
  await page.fill('[data-d="0"] [data-k="text"]', 'hi there');
  await page.evaluate(() => saveDeck()); await page.waitForTimeout(400);
  const k13 = posted && posted[13], k0 = posted && posted[0];
  console.log(`W2 save: key 14 = ${k13 && k13.label} mod ${k13 && k13.mod} key 0x${k13 && k13.key.toString(16)}, key 1 = text "${k0 && k0.text}" ${R(k13 && k13.label === 'Snip' && k13.mod === 3 && k13.key === 0x16 && k0.kind === 3 && k0.text === 'hi there')}`);
  // game upload: only .gb, to /roms
  await page.evaluate(() => go('media')); await page.waitForTimeout(300);
  await page.evaluate(async () => { const a = new File([new Uint8Array(32768)], 'Tobu Tobu Girl.gb'); const c = new File(['x'], 'notes.txt'); await sendGames([a, c]); });
  await page.waitForTimeout(300);
  console.log(`W3 send games: ${ups.length} upload(s) to ${ups.map(u => u.dir).join(',')} (the .txt refused) ${R(ups.length === 1 && ups[0].dir === '/roms' && ups[0].body.includes(Buffer.from('Tobu_Tobu_Girl.gb')))}`);
  console.log(`W4 no page errors ${R(errors.length === 0)} ${errors.join(' | ')}`);
  await b.close();
})();
