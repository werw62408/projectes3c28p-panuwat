// Balance bot: plays whole games through the game's own functions (fast), several seeds per level.
// usage: node tests/bot.js [loc] [lvl] [seeds] [minutes]
const { chromium } = require('/opt/node22/lib/node_modules/playwright');
const path = require('path');
const [loc = 0, lvl = 0, seeds = 3, minutes = 40] = process.argv.slice(2).map(Number);
(async () => {
  const b = await chromium.launch({ args: ['--ignore-certificate-errors'] });
  const p = await b.newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message + ' | ' + (e.stack || '').split('\n')[1]));
  await p.goto('file://' + path.join(__dirname, '..', 'index.html'));
  await p.waitForTimeout(500);
  for (let s = Number(process.env.S0 || 1); s <= seeds; s++) {
    const r = await p.evaluate(([loc, lvl, seed, minutes]) => {
      const A = AR;
      A.newRun({ loc, lvl, seed, deck: [{ key: 'uqueen', rar: 1 }, { key: 'builder', rar: 0 }, { key: 'loader', rar: 0 }, { key: 'seeker', rar: 0 }, { key: 'attacker', rar: 0 }, { key: 'bodyguard', rar: 0 }], perks: [], boosts: ['bar4', 'food10'] });
      A.initJobs();
      const G = A.G;
      const log = [];
      const count = (k) => G.ants.filter((a) => a.key === k).length + G.eggs.filter((e) => e.key === k).length + G.queue.filter((q) => q.key === k).reduce((n, q) => n + q.n, 0);
      // a plan for the nest: dig a row under the queen room for barracks + food stock
      const plan = [];
      for (let x = 8; x <= 13; x++) plan.push(A.uIdx(x, 8));
      for (let x = 3; x <= 21; x++) plan.push(A.uIdx(x, 9), A.uIdx(x, 10));
      let next = 0, raided = 0, squad = null;
      for (let t = 0; t < minutes * 60 && !G.over; t += 1) {
        A.fast(1);
        if (t % 3 === 0) {
          if (count('builder') < 4) A.enqueue('builder');
          if (count('loader') < 10) A.enqueue('loader');
          if (count('seeker') < 3) A.enqueue('seeker');
          if (t > 60 && count('bodyguard') < 6) A.enqueue('bodyguard');
          if (t > 30 && G.queue.length < 3) A.enqueue('attacker');
        }
        if (t === 20) for (const i of plan) A.markDig(i);
        if (t > 30 && t % 10 === 0) {
          // barracks on dug tiles in rows 9-10 right side, food stock left side
          for (const i of plan) if (G.ut[i] === 0 && !G.ub[i]) {
            const x = A.uX(i);
            const kind = x >= 15 ? 'br' : x <= 6 ? 'fs' : null;
            if (kind && A.canPlace(kind, i)) A.addBuilding(kind, [i]);
          }
        }
        if (t === 150 && !squad) { squad = A.newSquad(); squad.want = { bodyguard: 6 }; squad.refill = true; A.fillSquad(squad); A.setOrder(squad, { kind: 'guard', x: G.nest.x, y: G.nest.y + 8 }); }
        if (squad) A.fillSquad(squad);
        if (t === 400) { const q2 = A.newSquad(); q2.want = { attacker: 3 }; A.fillSquad(q2); A.setOrder(q2, { kind: 'queen' }); }
        const war = G.ants.filter((a) => a.caste === 'warrior' && !a.raid && !a.squad && a.hp > a.maxhp * 0.8);
        const tgt = G.bases.filter((b) => b.alive && b.known).sort((p, q) => (p.x - G.nest.x) ** 2 + (p.y - G.nest.y) ** 2 - ((q.x - G.nest.x) ** 2 + (q.y - G.nest.y) ** 2))[0];
        if (tgt && war.length >= 6 && !G.ants.some((a) => a.raid)) { A.startRaid(tgt, 10); raided++; }
        if (t % 60 === 0) log.push(`${t / 60}m ants ${G.ants.length} food ${G.pile.food + Array.from(G.sf).reduce((a, b) => a + b, 0)} eggs ${G.eggs.length} q ${G.queue.length} bases ${G.bases.filter((b) => b.alive).length}/${G.bases.length} known ${G.bases.filter((b) => b.known).length} war ${G.ants.filter((a) => a.caste === 'warrior').length} mobs ${G.mobs.length} queenHP ${Math.round(G.queen.hp)} dug ${G.st.dug} builds ${G.builds.filter((b) => b.done).length}/${G.builds.length} fed ${G.fed} feedC ${G.feedComing} fres ${G.foodResv}`);
      }
      return { deaths: JSON.stringify(G.deaths), over: G.over, why: G.overWhy, t: Math.round(G.t), raided, kills: G.st.kills, sugar: G.sugar, log };
    }, [loc, lvl, s * 7919, minutes]);
    console.log(`seed ${s}: ${r.over || 'running'} ${r.why} t=${r.t}s raids=${r.raided} kills=${r.kills} sugar=${r.sugar}`);
    console.log('  ' + r.log.join('\n  ') + '\n  deaths ' + r.deaths);
  }
  console.log(errs.length ? 'ERRORS:\n' + [...new Set(errs)].join('\n') : 'no errors');
  await b.close();
})();
