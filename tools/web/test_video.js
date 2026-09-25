// Tests the phone page's clip converter (sendVideo in webpage.h) in headless Chromium against a fake board.
//   node test_video.js ../../SomudTick_v11.4/SomudTick/webpage.h [scenario]
// Needs Node + Playwright (npm i -g playwright). test.webm: 6 s test picture, 30 fps (should give 90 pictures at 15 fps).
// Scenarios: normal / noplay (the phone refuses to play) / stall (playing stops after 2 s) /
//            dropseek (1 in 15 "seeked" never comes) / noframes (no "seeked" ever: must stop with a message, not hang)
const path = require('path');
let chromium; try { ({ chromium } = require('playwright')); } catch (e) { ({ chromium } = require(path.join(process.execPath, '../../lib/node_modules/playwright'))); }
const fs = require('fs');
const src = fs.readFileSync(process.argv[2], 'utf8');
const html = src.slice(src.indexOf('R"HTMLPAGE(') + 11, src.indexOf(')HTMLPAGE"'));
const clip = fs.readFileSync(path.join(__dirname, 'test.webm'));
const scenarios = {
  normal: '',
  noplay: `HTMLMediaElement.prototype.play=function(){return Promise.reject(new Error('NotAllowed'))};`,
  stall: `{const op=HTMLMediaElement.prototype.play;let calls=0;HTMLMediaElement.prototype.play=function(){calls++;if(calls>1)return Promise.reject(new Error('no'));const v=this;setTimeout(()=>v.pause(),2000);return op.call(this)}}`,
  dropseek: `HTMLMediaElement.prototype.play=function(){return Promise.reject(new Error('NotAllowed'))};
    {const oa=EventTarget.prototype.addEventListener;let k=0;EventTarget.prototype.addEventListener=function(t,f,o){if(t==='seeked'&&this instanceof HTMLMediaElement){if(++k%15==0)return;}return oa.call(this,t,f,o)}}`,
  noframes: `HTMLMediaElement.prototype.play=function(){return Promise.reject(new Error('NotAllowed'))};
    {const oa=EventTarget.prototype.addEventListener;EventTarget.prototype.addEventListener=function(t,f,o){if(t==='seeked'&&this instanceof HTMLMediaElement)return;return oa.call(this,t,f,o)}}`,
};
(async () => {
  const b = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const only = process.argv[3];
  for (const [name, patch] of Object.entries(scenarios)) {
    if (only && name !== only) continue;
    const page = await b.newPage();
    const ups = [];
    await page.route('**/*', async (r) => {
      const u = new URL(r.request().url());
      if (u.pathname === '/') return r.fulfill({ contentType: 'text/html', body: html });
      if (u.pathname === '/test.webm') return r.fulfill({ contentType: 'video/webm', body: clip });
      if (u.pathname === '/upload') { ups.push({ dir: u.searchParams.get('dir'), body: r.request().postDataBuffer() }); return r.fulfill({ status: 200, body: 'ok' }); }
      if (u.pathname === '/api/sd') return r.fulfill({ contentType: 'application/json', body: '{"ok":false}' });
      return r.fulfill({ contentType: 'application/json', body: '{}' });
    });
    await page.addInitScript(patch);
    await page.goto('http://board.local/');
    await page.evaluate(() => { const d = document.createElement('div'); d.id = 'mst'; document.body.appendChild(d); window.__st = []; const o = window.setStatus; });
    const t0 = Date.now();
    const res = await page.evaluate(async () => {
      const seen = []; const e = document.getElementById('mst');
      const mo = new MutationObserver(() => seen.push(e.textContent)); mo.observe(e, { childList: true, characterData: true, subtree: true });
      const blob = await (await fetch('/test.webm')).blob();
      const f = new File([blob], 'My clip 1.webm', { type: 'video/webm' });
      try { await sendVideo(f); } catch (err) { return { err: err.message, seen }; }
      return { ok: true, seen };
    });
    const secs = (Date.now() - t0) / 1000;
    let frames = 0, sizes = [], distinct = 0;
    const mj = ups.find(u => u.body && u.body.includes(Buffer.from('.mjpeg')));
    if (mj) {
      const buf = mj.body; let prev = -1; const starts = [];
      for (let i = 0; i + 2 < buf.length; i++) if (buf[i] === 0xFF && buf[i + 1] === 0xD8 && buf[i + 2] === 0xFF) starts.push(i);
      frames = starts.length;
      const hashes = new Set(); for (let k = 0; k < starts.length; k++) { const s = starts[k], e = k + 1 < starts.length ? starts[k + 1] : buf.length; hashes.add(buf.slice(s, e).toString('base64').slice(-200)); }
      distinct = hashes.size;
    }
    const pcm = ups.find(u => u.body && u.body.includes(Buffer.from('.pcm')));
    const pct = res.seen.filter(s => /แปลงภาพ \d+%/.test(s)).map(s => +s.match(/(\d+)%/)[1]);
    console.log(`${name.padEnd(9)} ${res.err ? 'ERROR: ' + res.err : 'done'}  ${secs.toFixed(1)} s  frames=${frames} (want 90) distinct=${distinct}  audio=${pcm ? 'yes' : 'no'}  progress updates=${pct.length} max=${pct.length ? Math.max(...pct) : '-'}%  last="${res.seen[res.seen.length - 1] || ''}"`);
    await page.close();
  }
  await b.close();
})();
