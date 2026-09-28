// Tests the phone page's clip SOUND (makePcm + mp4Aac in webpage.h, v11.9) in headless Chromium.
//   node test_audio.js ../../SomudTick_v11.9/SomudTick/webpage.h
// clip.mov / clip.mp4: 4 s, H.264 picture + AAC sound (a 440 Hz tone), made with ffmpeg (tools/web/README in the repo).
// Scenarios:
//   normal : the browser can decode anything
//   iphone : a model of iPhone Safari / Chrome on iPhone: decodeAudioData refuses a whole video file (v11.8 bug: clips had
//            no sound); it only takes a plain AAC stream (ADTS)
// Each must give about 4 s of 16 kHz sound that is not silent.
const path = require('path');
let chromium; try { ({ chromium } = require('playwright')); } catch (e) { ({ chromium } = require(process.env.PW || 'playwright')); }
const fs = require('fs');
const src = fs.readFileSync(process.argv[2], 'utf8');
const html = src.slice(src.indexOf('R"HTMLPAGE(') + 11, src.indexOf(')HTMLPAGE"'));
const scen = {
  normal: '',
  iphone: `{const od=BaseAudioContext.prototype.decodeAudioData;BaseAudioContext.prototype.decodeAudioData=function(ab,ok,bad){
      const u=new Uint8Array(ab);const adts=u[0]==0xFF&&(u[1]&0xF0)==0xF0;
      if(!adts){const e=new Error('EncodingError');if(bad)bad(e);return Promise.reject(e)}
      return od.call(this,ab,ok,bad)}}`,
};
(async () => {
  const b = await chromium.launch(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {});
  let fail = 0;
  for (const clip of ['clip.mov', 'clip.mp4']) {
    const data = fs.readFileSync(path.join(__dirname, clip)).toString('base64');
    for (const [name, patch] of Object.entries(scen)) {
      const page = await b.newPage();
      await page.route('**/*', r => { const u = new URL(r.request().url()); if (u.pathname === '/') return r.fulfill({ contentType: 'text/html', body: html }); return r.fulfill({ contentType: 'application/json', body: '{}' }); });
      await page.addInitScript(patch);
      await page.goto('http://board.test/');
      const res = await page.evaluate(async (b64) => {
        const bin = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
        const f = new File([bin], 'clip.mov');
        try {
          const blob = await makePcm(f); const a = new Int16Array(await blob.arrayBuffer());
          let e = 0; for (let i = 0; i < a.length; i++) e += a[i] * a[i];
          return { n: a.length, rms: Math.sqrt(e / Math.max(1, a.length)) };
        } catch (err) { return { err: String(err) }; }
      }, data);
      const ok = !res.err && res.n > 3.5 * 16000 && res.n < 4.5 * 16000 && res.rms > 500;
      if (!ok) fail = 1;
      console.log(`${clip} ${name}: ${res.err ? 'error ' + res.err : (res.n / 16000).toFixed(2) + ' s, loudness ' + Math.round(res.rms)} ${ok ? 'PASS' : 'FAIL'}`);
      await page.close();
    }
  }
  await b.close();
  console.log(fail ? 'SOME FAILED' : 'ALL PASSED');
  process.exit(fail);
})();
