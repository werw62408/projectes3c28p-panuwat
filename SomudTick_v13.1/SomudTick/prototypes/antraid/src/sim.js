"use strict";
// ===== Simulation: ants, enemies, queen, eggs, workshop, raids, squads =====

const cardOf = (key) => G.deck.find((c) => c.key === key);
const casteOf = (key) => ANTS[key].castes[0];
const hasCaste = (key, c) => ANTS[key].castes.includes(c);

function antStats(key) {
  const A = ANTS[key], l = G.lvls[key] || 1, card = cardOf(key), rar = card ? card.rar : 0;
  const s = {};
  for (const k of ["hp", "dmg", "as", "cd", "cc", "spd", "str", "vis", "dist", "price", "hatch"]) s[k] = lvlv(A[k], l);
  s.range = A.range || 0;
  if (A.rb && rar) { const m = 1 + rar * 0.1; for (const k of A.rb) s[k] = k === "dmg" ? s[k] + rar * 0.5 : Math.ceil(s[k] * m - 0.0001); }
  if (rar >= 2) s.price = Math.round(s.price * (rar === 2 ? 0.9 : 0.8));
  if (rar === 3 && A.leg) { const [k, v] = A.leg; s[k] = v < 1 ? s[k] * (1 + v) : s[k] + v; }
  const c0 = A.castes[0], P = G.perks;
  if (c0 === "builder" && P.has("carry")) { s.spd *= 0.4; s.str += 1; }
  if (c0 === "loader" && P.has("bigger")) { s.str += 1; s.spd /= 1.7; }
  if (c0 === "loader" && G.boosts.has("loadstr")) s.str += 1;
  if ((c0 === "scout" || c0 === "loader") && P.has("predator")) s.dmg += 1;
  if (c0 === "scout" && P.has("attent")) { s.vis *= 1.5; s.dist *= 0.75; }
  if (P.has("decisive")) s.cc = 0.2;
  if (P.has("kinder")) { s.price *= 0.8; s.hatch *= 5; }
  if (P.has("golden")) { s.price *= 1.25; s.hatch /= 4; }
  if (G.boosts.has("hatch30")) s.hatch *= 0.7;
  if (G.boosts.has("speed20")) s.spd *= 1.2;
  s.price = Math.max(1, Math.round(s.price));
  return s;
}
const antPrice = (key) => antStats(key).price;

function makeAnt(key, tile, layer) {
  const A = ANTS[key], s = antStats(key);
  const a = { id: NEXT_ID++, key, A, caste: A.castes[0], layer, x: uCx(tile) + rrange(-2, 2), y: uCy(tile) + rrange(-2, 2), tile, nt: -1,
    hp: s.hp, maxhp: s.hp, dmg: s.dmg, as: s.as, cc: s.cc, cdm: s.cd, spd: s.spd, str: s.str, vis: s.vis, dist: s.dist, range: s.range,
    size: A.size, cool: 0, stun: 0, alive: true, state: "idle", job: null, carry: null, tgt: null, squad: null, home: null, raid: null,
    born: G.t, wait: 0, th: R() * 6.28, moving: false, anim: R() * 10, revT: 0, fleeT: 0, bites: 0, goal: null };
  if (A.castes[0] === "loader") { if (G.perks.has("newborn")) a.newbornT = 120; if (G.perks.has("newbie")) a.bites = 5; }
  G.ants.push(a);
  return a;
}
function refreshStats(key) {
  for (const a of G.ants) if (a.alive && a.key === key) {
    const s = antStats(key), r = a.hp / a.maxhp;
    Object.assign(a, { maxhp: s.hp, dmg: s.dmg, as: s.as, cc: s.cc, cdm: s.cd, spd: s.spd, str: s.str, vis: s.vis, dist: s.dist, range: s.range });
    a.hp = Math.max(1, Math.round(s.hp * r));
  }
}

function spawnMob(type, base, role) {
  const M = MOBS[type], mul = G.mul;
  const m = { id: NEXT_ID++, type, M, isMob: true, layer: "S", x: base ? base.x + rrange(-14, 14) : 0, y: base ? base.y + rrange(-14, 14) : 0, tile: 0, nt: -1,
    hp: Math.round(M.hp * mul), maxhp: Math.round(M.hp * mul), dmg: M.dmg * (1 + (mul - 1) * 0.5), as: M.as, cc: M.cc || 0, cdm: M.cdm || M.dmg,
    spd: M.spd, vis: M.vis, size: M.size, base: base ? base.id : 0, role, cool: 0, stun: 0, alive: true, tgt: null, carry: 0,
    wx: 0, wy: 0, wait: 0, th: R() * 6.28, moving: false, anim: R() * 10, angry: 0, retT: 0 };
  if (base && role === "guard") base.guards.push(m.id);
  G.mobs.push(m);
  return m;
}
const baseById = (id) => G.bases.find((b) => b.id === id);

// ---------- movement ----------
function stepToward(u, tx, ty, dt, mul) {
  const sp = u.spd * SPD_PX * (mul || 1) * dt;
  const dx = tx - u.x, dy = ty - u.y, d = Math.hypot(dx, dy);
  if (d <= sp || d < 0.01) { u.x = tx; u.y = ty; u.moving = d > 0.01; return true; }
  u.x += (dx / d) * sp; u.y += (dy / d) * sp; u.th = Math.atan2(dy, dx); u.moving = true;
  return false;
}
// underground: walk tile by tile toward goal tile; returns true on arrival, "fail" when no way
function uWalk(u, goal, dt, mul) {
  const d = field(goal);
  const cur = uAt(u.x, u.y);
  if (u.nt < 0 || (!walkable(u.nt) && u.nt !== goal)) u.nt = -1;
  if (u.nt < 0) {
    if (d[cur] === 0) { return stepToward(u, uCx(goal) + (u.ox || 0), uCy(goal) + (u.oy || 0), dt, mul); }
    const n = nextStep(cur, d);
    if (n < 0) { u.moving = false; return d[cur] === 0 ? true : "fail"; }
    u.nt = n;
  }
  if (stepToward(u, uCx(u.nt), uCy(u.nt), dt, mul)) { u.tile = u.nt; u.nt = -1; }
  return false;
}
// go to (x, y) in a layer, switching layers through the nest entrance when needed
function goTo(u, layer, x, y, dt, mul) {
  if (u.layer !== layer) {
    if (u.layer === "U") {
      const r = uWalk(u, G.ent, dt, mul);
      if (r === true || (uAt(u.x, u.y) === G.ent && dist2(u.x, u.y, uCx(G.ent), uCy(G.ent)) < 4)) { u.layer = "S"; u.x = G.nest.x + rrange(-3, 3); u.y = G.nest.y + 3; u.nt = -1; }
      return false;
    }
    if (stepToward(u, G.nest.x, G.nest.y, dt, mul) || dist2(u.x, u.y, G.nest.x, G.nest.y) < 4) { u.layer = "U"; u.x = uCx(G.ent); u.y = uCy(G.ent); u.tile = G.ent; u.nt = -1; }
    return false;
  }
  if (layer === "S") return stepToward(u, x, y, dt, mul);
  const g = uAt(x, y);
  const r = uWalk(u, walkable(g) ? g : nearestWalk(g), dt, mul);
  return r === true;
}
function nearestWalk(i) {
  for (let r = 1; r < 6; r++) for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) {
    const x = uX(i) + dx, y = uY(i) + dy; if (x < 0 || y < 0 || x >= UND_W || y >= UND_H) continue;
    const n = uIdx(x, y); if (walkable(n) && reachable(n)) return n;
  }
  return G.ent;
}

// ---------- combat ----------
function reach(u, t) { return u.range || 3 + (u.size + t.size) * 2.2; }
function nearestFoe(u, r, pred) {
  const list = u.isMob ? G.ants : G.mobs;
  let best = null, bd = r * r;
  for (const e of list) {
    if (!e.alive || e.layer !== u.layer) continue;
    if (!u.isMob && e.role === "wander" && e.angry <= 0 && !pred) continue;   // leave the big wanderers alone unless they start it
    if (pred && !pred(e)) continue;
    const d = dist2(u.x, u.y, e.x, e.y);
    if (d < bd) { bd = d; best = e; }
  }
  return best;
}
function fight(u, t, dt, mul) {
  // move into reach and hit. returns false when the target is gone
  if (!t || !t.alive || t.layer !== u.layer) return false;
  const r = reach(u, t);
  if (dist2(u.x, u.y, t.x, t.y) > r * r) {
    if (u.layer === "S") stepToward(u, t.x, t.y, dt, mul);
    else { const g = uAt(t.x, t.y); if (uWalk(u, walkable(g) ? g : nearestWalk(g), dt, mul) === "fail") return false; if (uAt(u.x, u.y) === g) stepToward(u, t.x, t.y, dt, mul); }
    return true;
  }
  u.moving = false; u.th = Math.atan2(t.y - u.y, t.x - u.x);
  if (u.cool > 0) return true;
  if (u === G.queen && u.noFight) return true;
  let dmg = u.dmg, crit = false;
  if (u.cc && R() < u.cc) { dmg = Math.max(dmg, u.cdm || dmg * 2); crit = true; }
  u.cool = u.as;
  if (u.range) G.shots.push({ x: u.x, y: u.y, tx: t.x, ty: t.y, t: 0.25, layer: u.layer });
  if (u.A && u.A.splash) { for (const e of G.mobs) if (e.alive && e.layer === u.layer && dist2(e.x, e.y, t.x, t.y) < 100) hurt(e, dmg, u); }
  else hurt(t, dmg, u);
  if (crit) { G.fx.push({ x: t.x, y: t.y - 4, t: 0.5, txt: "!", col: "#ffcf4a", layer: u.layer }); if (u.A && u.A.stun) t.stun = 1; }
  return true;
}
function hurt(t, dmg, from) {
  if (!t.alive) return;
  t.hp -= dmg;
  t.hitT = 0.15;
  if (t.isMob) { t.angry = 4; if (!t.tgt || !t.tgt.alive) t.tgt = from; }
  else {
    if (!t.tgt && from && from.alive) t.tgt = from;
    if (t === G.queen) {
      G.st.queenHit = true;
      if (!G.qAlarm || G.t - G.qAlarm > 8) { G.qAlarm = G.t; G.st.queenHits++; toast("QUEEN UNDER ATTACK!", "#ff5d4a"); alertAt(t.x, t.y, "U"); }
    }
  }
  if (t.hp <= 0) kill(t, from);
}
function kill(t, from) {
  t.alive = false;
  if (t.isMob) {
    G.st.kills++;
    if (from && !from.isMob && (from.caste === "loader" || from.caste === "scout")) G.st.workerKills++;
    const M = t.M;
    const food = M.food + (t.carry || 0);
    if (food > 0) dropAt(t.layer, t.x, t.y, "drop", Math.round(food * (0.7 + R() * 0.3)));
    let ch = M.chitin; if (G.perks.has("valuable")) ch *= 1.35;
    if (R() < ch) dropAt(t.layer, t.x + 3, t.y + 2, "chitin", 1 + (M.boss ? 2 : 0));
    const b = baseById(t.base);
    if (b) b.guards = b.guards.filter((id) => id !== t.id);
    G.fx.push({ x: t.x, y: t.y, t: 0.6, ring: true, col: "#ffcf4a", layer: t.layer });
    seeAlmanac("mob", t.type);
  } else {
    if (t.carry && t.carry.food) dropAt(t.layer, t.x, t.y, "drop", t.carry.food);
    if (t.carry && t.carry.chitin) dropAt(t.layer, t.x, t.y, "chitin", t.carry.chitin);
    releaseJob(t);
    const dk = t.caste + "<" + (from ? (from.isMob ? from.type + ":" + from.role + ":" + from.layer : "ant") : "?");
    G.deaths[dk] = (G.deaths[dk] || 0) + 1;
    if (t.squad) { const s = G.squads.find((q) => q.id === t.squad); if (s) s.lost++; }
    G.fx.push({ x: t.x, y: t.y, t: 0.6, ring: true, col: "#ff5d4a", layer: t.layer });
    if (t === G.queen) endRun("lose", "The queen died");
  }
}
function dropAt(layer, x, y, kind, n) {
  if (n <= 0) return;
  if (layer === "U") { if (kind === "chitin") G.pile.chitin += n; else G.pile.food += n; return; }
  const near = G.foods.find((f) => f.layer === "S" && f.kind === kind && f.amt > 0 && dist2(f.x, f.y, x, y) < 64);
  if (near) { near.amt += n; near.max = Math.max(near.max, near.amt); return; }
  G.foods.push({ id: NEXT_ID++, kind, layer: "S", x, y, amt: n, max: n, known: seenS(x, y), drop: true });
}

// ---------- the queen, eggs, queue ----------
function eggCount(i) { let n = 0; for (const e of G.eggs) if (e.tile === i) n++; return n; }
function eggSpot() {
  // a queen room tile with space for an egg, closest to the queen first
  let best = -1, bd = 1e9;
  for (const b of G.builds) if (b.kind === "qs" && b.done) for (const i of b.tiles) {
    if (eggCount(i) >= G.Q.qsEgg) continue;
    const d = Math.abs(uX(i) - uX(G.queenTile)) + Math.abs(uY(i) - uY(G.queenTile));
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
function layEgg(key, free) {
  const tile = eggSpot();
  if (tile < 0) return null;
  const s = antStats(key);
  let tmax = (s.hatch * HATCH_K) / (G.queen ? G.queen.hsMul || 1 : QUEEN_RB[G.qcard.rar].hs);
  const e = { key, tile, t: tmax, tmax, id: NEXT_ID++ };
  G.eggs.push(e);
  if (!free) G.st.eggsMade++;
  G.st.maxEggs = Math.max(G.st.maxEggs, G.eggs.length);
  return e;
}
function nearQueen(i) { return Math.abs(uX(i) - uX(G.queenTile)) <= 2 && Math.abs(uY(i) - uY(G.queenTile)) <= 1; }
function barracksRoom(key) {
  // how many more of this warrior fit in the barracks
  const A = ANTS[key];
  if (A.bigBarracks) { const cap = doneOf("bb").length; let used = 0; for (const a of G.ants) if (a.alive && ANTS[a.key].bigBarracks) used++; for (const e of G.eggs) if (ANTS[e.key].bigBarracks) used++; return cap - used; }
  if (A.barracks) { const cap = doneOf("br").reduce((n, b) => n + b.tiles.length, 0); let used = 0; for (const a of G.ants) if (a.alive && ANTS[a.key].barracks) used += ANTS[a.key].barracks; for (const e of G.eggs) used += ANTS[e.key].barracks || 0; return Math.floor((cap - used) / A.barracks + 1e-6); }
  if (A.maxSpace) { const cap = lvlv(A.maxSpace, G.lvls[key] || 1); let used = 0; for (const a of G.ants) if (a.alive && a.key === key) used++; for (const e of G.eggs) if (e.key === key) used++; return cap - used; }
  return 99;
}
function queued(key) { let n = 0; for (const s of G.queue) if (s.key === key) n += s.n; return n; }
function canQueue(key) {
  const A = ANTS[key];
  if (A.barracks || A.bigBarracks || A.maxSpace) {
    const room = barracksRoom(key) - queued(key);
    if (room <= 0) return A.bigBarracks ? "Build a big barracks first" : A.maxSpace ? "No space for more" : "Build more barracks first";
  }
  return null;
}
function enqueue(key) {
  const why = canQueue(key);
  if (why) { toast(why, "#ff9a3c"); return false; }
  let s = G.queue.find((q) => q.key === key && q.n < 4 && !q.loop);
  if (!s) {
    if (G.queue.length >= queueSlots()) { toast("Queue full: build more queen rooms", "#ff9a3c"); return false; }
    s = { key, n: 0, loop: false }; G.queue.push(s);
  }
  s.n++;
  return true;
}
function queueCount() { let n = 0; for (const s of G.queue) n += s.loop ? 4 : s.n; return n; }
function curPrice() { return G.queue.length ? antPrice(G.queue[0].key) : 0; }
function queenStep(dt) {
  const q = G.queen;
  if (!q || !q.alive) return;
  // stay in the queen room; walk to a new room when moved
  if (G.queenGoto != null && !(buildingAt(G.queenGoto) && buildingAt(G.queenGoto).kind === "qs")) G.queenGoto = null;
  if (G.queenGoto != null) {
    const r = uWalk(q, G.queenGoto, dt);
    if (r === true || uAt(q.x, q.y) === G.queenGoto) { G.queenTile = G.queenGoto; G.queenGoto = null; toast("The queen moved", "#ffcf4a"); if (!G.qgroup0.includes(G.queenTile)) G.st.queenMoved = true; }
  } else {
    const foe = q.noFight ? null : nearestFoe(q, 14);
    if (foe) fight(q, foe, dt);
    else { q.moving = false; if (dist2(q.x, q.y, uCx(G.queenTile), uCy(G.queenTile)) > 2) stepToward(q, uCx(G.queenTile), uCy(G.queenTile), dt); }
  }
  q.hp = Math.min(q.maxhp, q.hp + dt * 0.4);
  // lay eggs: skip slots that cannot go now (no barracks), keep turns between slots
  for (let k = 0; k < G.queue.length; k++) {
    const s = G.queue[0];
    if (s.n <= 0 && !s.loop) { G.queue.shift(); k--; continue; }
    const price = antPrice(s.key);
    if (G.fed < price) break;
    if (eggSpot() < 0) break;
    const A = ANTS[s.key];
    if ((A.barracks || A.bigBarracks || A.maxSpace) && barracksRoom(s.key) <= 0) { G.queue.push(G.queue.shift()); continue; }
    G.fed -= price;
    layEgg(s.key);
    if (!s.loop) s.n--;
    G.queue.push(G.queue.shift());
    if (s.n <= 0 && !s.loop) G.queue.splice(G.queue.indexOf(s), 1);
    break;
  }
  // eggs hatch
  for (let k = G.eggs.length - 1; k >= 0; k--) {
    const e = G.eggs[k];
    e.t -= dt * (G.perks.has("mother") && nearQueen(e.tile) ? 1.5 : 1);
    if (e.t <= 0) {
      G.eggs.splice(k, 1);
      const a = makeAnt(e.key, e.tile, "U");
      if (a.caste === "scout") G.st.scouts++;
      if (a.caste === "loader") G.st.loaders++;
      seeAlmanac("ant", e.key);
      joinSquadOnHatch(a);
    }
  }
  // workshop crafting
  if (G.upgrade) {
    const ws = doneOf("ws")[0];
    if (ws) {
      if (G.wsT <= 0 && G.wsChitin >= 4 && G.wsPlate + G.plates < G.upgrade.need) { G.wsChitin -= 4; G.wsT = 18; }
      if (G.wsT > 0) { G.wsT -= dt; if (G.wsT <= 0) { G.wsT = 0; G.wsPlate++; } }
    }
    if (G.plates >= G.upgrade.need) {
      const key = G.upgrade.key;
      G.lvls[key]++; G.plates -= G.upgrade.need; G.upgrade = null;
      G.st.maxLvl = Math.max(G.st.maxLvl, G.lvls[key]);
      refreshStats(key);
      toast(ANTS[key].name + " is now level " + G.lvls[key], "#7fd46b");
    }
  }
}

// ---------- builders (underground jobs) ----------
function foodSources() {
  // tiles (or the pile) where food can be taken
  const out = [];
  if (G.pile.food > 0) out.push(G.ent);
  for (const b of G.builds) if (b.done) for (const i of b.tiles) if (G.sf[i] > 0) out.push(i);
  return out;
}
function takeFood(tile, n) {
  if (tile === G.ent && G.pile.food > 0) { const k = Math.min(n, G.pile.food); G.pile.food -= k; return k; }
  const k = Math.min(n, G.sf[tile]); G.sf[tile] -= k; return k;
}
function takeChitin(tile, n) {
  if (tile === G.ent && G.pile.chitin > 0) { const k = Math.min(n, G.pile.chitin); G.pile.chitin -= k; return k; }
  const k = Math.min(n, G.sc[tile]); G.sc[tile] -= k; return k;
}
function closestTile(from, tiles) {
  const d = field(from);
  let best = -1, bd = 1e9;
  for (const t of tiles) { let dd = d[t]; if (dd < 0) { dd = field(t)[from]; if (dd < 0) continue; } if (dd < bd) { bd = dd; best = t; } }
  return best;
}
function storeSpot(kind, from) {
  const tiles = [];
  for (const b of G.builds) if (b.done) for (const i of b.tiles) {
    if (kind === "food" && G.sf[i] + (G.resv[i] || 0) < foodCap(i)) tiles.push(i);
    if (kind === "chitin" && G.sc[i] + (G.resv[i] || 0) < chitinCap(i)) tiles.push(i);
  }
  return tiles.length ? closestTile(from, tiles) : -1;
}
function freeFood() { return totalFood() - (G.foodResv || 0); }
function releaseJob(a) {
  const j = a.job; if (!j) return;
  if (j.foodResv) G.foodResv -= j.foodResv;
  if (j.site) { const b = G.builds.find((x) => x.id === j.site); if (b) b.coming -= j.n || 0; }
  if (j.dest === "queen") G.feedComing -= j.n || 0;
  if (j.dest === "ws") G.wsComing -= j.n || 0;
  if (j.resv != null) G.resv[j.resv] = Math.max(0, (G.resv[j.resv] || 0) - (j.n || 0));
  if (j.type === "dig" || j.type === "fill") G.digResv.delete(j.tile);
  if (j.type === "plate") G.plateComing--;
  a.job = null;
}
function pickJob(a) {
  const P = G.prio, cands = [];
  const ff = freeFood();
  const price = curPrice();
  if (P.feed && G.queue.length && ff > 0 && G.fed + G.feedComing < price) cands.push({ p: P.feed, k: 0, f: () => haulFood(a, "queen", Math.min(a.str, price - G.fed - G.feedComing)) });
  if (P.build && ff > 0) { const b = G.builds.find((x) => !x.done && x.got + x.coming < x.need); if (b) cands.push({ p: P.build, k: 1, f: () => haulFood(a, "site", Math.min(a.str, b.need - b.got - b.coming), b) }); }
  if (P.dig && G.digList.length) cands.push({ p: P.dig, k: 2, f: () => digJob(a) });
  if (P.store && (G.pile.food > 0 || G.pile.chitin > 0)) cands.push({ p: P.store, k: 3, f: () => storeJob(a) });
  if (P.ws && G.upgrade && doneOf("ws").length) {
    if (G.wsPlate > G.plateComing) cands.push({ p: P.ws + 0.5, k: 4, f: () => { G.plateComing++; a.job = { type: "plate", phase: 0 }; return true; } });
    else if (G.wsChitin + G.wsComing < 4 && totalChitin() > 0 && G.wsT <= 0) cands.push({ p: P.ws, k: 4, f: () => haulChitinWs(a) });
  }
  if (P.fill && G.fillList.length && ff > 0) cands.push({ p: P.fill, k: 5, f: () => fillJob(a) });
  cands.sort((x, y) => y.p - x.p || x.k - y.k);
  for (const c of cands) if (c.f()) return true;
  return false;
}
function haulFood(a, dest, n, site) {
  if (n <= 0) return false;
  const srcs = foodSources(); if (!srcs.length) return false;
  const src = closestTile(uAt(a.x, a.y), srcs); if (src < 0) return false;
  const j = { type: "haul", kind: "food", src, dest, n, phase: 0, foodResv: n };
  G.foodResv = (G.foodResv || 0) + n;
  if (dest === "queen") G.feedComing += n;
  if (dest === "site") { j.site = site.id; site.coming += n; }
  a.job = j; return true;
}
function storeJob(a) {
  const kind = G.pile.food > 0 && foodSpace() > 0 ? "food" : G.pile.chitin > 0 ? "chitin" : null;
  if (!kind) return false;
  const to = storeSpot(kind, G.ent); if (to < 0) return false;
  const n = Math.min(a.str, kind === "food" ? G.pile.food - (G.pileResv || 0) : G.pile.chitin);
  if (n <= 0) return false;
  G.resv[to] = (G.resv[to] || 0) + n;
  a.job = { type: "haul", kind, src: G.ent, dest: "store", to, n, phase: 0, resv: to };
  return true;
}
function haulChitinWs(a) {
  const srcs = []; if (G.pile.chitin > 0) srcs.push(G.ent);
  for (const b of G.builds) if (b.done) for (const i of b.tiles) if (G.sc[i] > 0) srcs.push(i);
  const src = closestTile(uAt(a.x, a.y), srcs); if (src < 0) return false;
  const n = Math.min(a.str, 4 - G.wsChitin - G.wsComing); if (n <= 0) return false;
  G.wsComing += n;
  a.job = { type: "haul", kind: "chitin", src, dest: "ws", n, phase: 0 };
  return true;
}
function digJob(a) {
  const from = uAt(a.x, a.y), d = field(from);
  let best = -1, bs = -1, bd = 1e9;
  for (const i of G.digList) {
    if (G.digResv.has(i)) continue;
    const st = digStand(i); if (st < 0) continue;
    const dd = d[st]; if (dd < 0) continue;
    if (dd < bd) { bd = dd; best = i; bs = st; }
  }
  if (best < 0) return false;
  G.digResv.add(best);
  a.job = { type: "dig", tile: best, stand: bs, phase: 0 };
  return true;
}
function fillJob(a) {
  for (const i of G.fillList) {
    if (G.digResv.has(i)) continue;
    if (!canFill(i)) continue;
    const srcs = foodSources(); if (!srcs.length) return false;
    const src = closestTile(uAt(a.x, a.y), srcs); if (src < 0) return false;
    G.digResv.add(i);
    G.foodResv = (G.foodResv || 0) + 1;
    a.job = { type: "fill", tile: i, src, n: 1, phase: 0, foodResv: 1 };
    return true;
  }
  return false;
}
function builderStep(a, dt) {
  // fight intruders close by
  // builders keep away from intruders (losing all builders ends the game); warriors and loaders defend
  const near = nearestFoe(a, 22);
  if (near && a.layer === "U") {
    if (!a.flee || a.flee.t < G.t) {
      let best = -1, bd = -1;
      for (const b of G.builds) if (b.done) for (const i of b.tiles) { const d = dist2(uCx(i), uCy(i), near.x, near.y); if (d > bd && reachable(i)) { bd = d; best = i; } }
      a.flee = { tile: best >= 0 ? best : G.ent, t: G.t + 1.5 };
    }
    if (a.job && a.job.type === "dig") releaseJob(a);
    uWalk(a, a.flee.tile, dt, 1.2);
    return;
  }
  a.tgt = null;
  if (a.layer !== "U") { goTo(a, "U", uCx(G.queenTile), uCy(G.queenTile), dt); return; }
  if (!a.job) {
    a.wait -= dt;
    if (a.wait > 0) { idleWander(a, dt); return; }
    if (!pickJob(a)) { a.wait = 0.6 + R() * 0.6; idleWander(a, dt); return; }
  }
  const j = a.job, spd = 1;
  const digRate = 2.2 * (G.perks.has("diggers") ? 2 : 1) * (1 + 0.15 * ((G.lvls[a.key] || 1) - 1));
  if (j.type === "dig") {
    if (G.ut[j.tile] === U_EMPTY || !G.um[j.tile]) { releaseJob(a); return; }
    if (j.phase === 0) { const r = uWalk(a, j.stand, dt); if (r === "fail") { releaseJob(a); a.wait = 1; return; } if (r === true) j.phase = 1; return; }
    a.moving = false; a.th = Math.atan2(uCy(j.tile) - a.y, uCx(j.tile) - a.x); a.digging = 0.2;
    G.uhp[j.tile] -= digRate * dt;
    if (G.uhp[j.tile] <= 0) { digTile(j.tile); G.digList = G.digList.filter((t) => t !== j.tile); G.dustAt = j.tile; releaseJob(a); }
    return;
  }
  if (j.type === "fill") {
    if (j.phase === 0) { const r = uWalk(a, j.src, dt); if (r === "fail") { releaseJob(a); return; } if (r === true) { if (takeFood(j.src, 1) < 1) { releaseJob(a); return; } G.foodResv -= 1; j.foodResv = 0; a.carry = { food: 1 }; j.phase = 1; j.stand = digStand(j.tile) >= 0 ? nearStand(j.tile) : -1; } return; }
    if (j.phase === 1) {
      if (j.stand < 0 || !canFill(j.tile)) { G.pile.food += 1; a.carry = null; releaseJob(a); return; }
      const r = uWalk(a, j.stand, dt); if (r === "fail") { G.pile.food += 1; a.carry = null; releaseJob(a); return; }
      if (r === true) { j.phase = 2; j.prog = 8; }
      return;
    }
    j.prog -= digRate * dt; a.digging = 0.2;
    if (j.prog <= 0) { if (canFill(j.tile)) { fillTile(j.tile); G.fillList = G.fillList.filter((t) => t !== j.tile); } else G.pile.food += 1; a.carry = null; releaseJob(a); }
    return;
  }
  if (j.type === "haul") {
    if (j.phase === 0) {
      const r = uWalk(a, j.src, dt);
      if (r === "fail") { releaseJob(a); a.wait = 1; return; }
      if (r !== true) return;
      const got = j.kind === "food" ? takeFood(j.src, j.n) : takeChitin(j.src, j.n);
      if (j.foodResv) { G.foodResv -= j.foodResv; j.foodResv = 0; }
      if (got <= 0) { releaseJob(a); return; }
      if (got < j.n) { const miss = j.n - got; if (j.dest === "queen") G.feedComing -= miss; if (j.site) { const b = G.builds.find((x) => x.id === j.site); if (b) b.coming -= miss; } if (j.dest === "ws") G.wsComing -= miss; if (j.resv != null) G.resv[j.resv] -= miss; j.n = got; }
      a.carry = j.kind === "food" ? { food: got } : { chitin: got };
      j.phase = 1;
      return;
    }
    let goal;
    if (j.dest === "queen") goal = G.queenTile;
    else if (j.dest === "site") { const b = G.builds.find((x) => x.id === j.site); if (!b) { dropCarry(a); releaseJob(a); return; } goal = b.tiles[0]; }
    else if (j.dest === "store") goal = j.to;
    else if (j.dest === "ws") { const w = doneOf("ws")[0]; if (!w) { dropCarry(a); releaseJob(a); return; } goal = w.tiles[0]; }
    const r = uWalk(a, goal, dt);
    if (r === "fail") { dropCarry(a); releaseJob(a); a.wait = 1; return; }
    if (r !== true) return;
    const n = j.n;
    if (j.dest === "queen") { G.fed += n; G.feedComing -= n; }
    else if (j.dest === "site") { const b = G.builds.find((x) => x.id === j.site); b.coming -= n; b.got += n; if (b.got >= b.need && !b.done) finishBuilding(b); }
    else if (j.dest === "store") { G.resv[j.to] -= n; if (j.kind === "food") { const room = foodCap(j.to) - G.sf[j.to]; const k = Math.min(room, n); G.sf[j.to] += k; G.pile.food += n - k; } else { const room = chitinCap(j.to) - G.sc[j.to]; const k = Math.min(room, n); G.sc[j.to] += k; G.pile.chitin += n - k; } }
    else if (j.dest === "ws") { G.wsChitin += n; G.wsComing -= n; }
    a.carry = null; j.n = 0; j.site = null; j.resv = null; j.dest = null; a.job = null;
    return;
  }
  if (j.type === "plate") {
    if (j.phase === 0) { const w = doneOf("ws")[0]; if (!w || G.wsPlate <= 0) { releaseJob(a); return; } const r = uWalk(a, w.tiles[0], dt); if (r === "fail") { releaseJob(a); return; } if (r === true) { G.wsPlate--; a.carry = { plate: 1 }; j.phase = 1; } return; }
    const r = uWalk(a, G.queenTile, dt);
    if (r === "fail") { G.wsPlate++; a.carry = null; releaseJob(a); return; }
    if (r === true) { G.plates++; a.carry = null; G.plateComing--; a.job = null; }
  }
}
function nearStand(i) {
  const x = uX(i), y = uY(i), d = field(G.ent);
  const ns = []; if (x > 0) ns.push(i - 1); if (x < UND_W - 1) ns.push(i + 1); if (y > 0) ns.push(i - UND_W); if (y < UND_H - 1) ns.push(i + UND_W);
  for (const n of ns) if (walkable(n) && d[n] >= 0) return n;
  return -1;
}
function dropCarry(a) { if (!a.carry) return; if (a.carry.food) G.pile.food += a.carry.food; if (a.carry.chitin) G.pile.chitin += a.carry.chitin; a.carry = null; }
function finishBuilding(b) {
  b.done = true;
  toast(BUILDS[b.kind].name + " built", "#7fd46b");
  if (b.kind === "ws") G.st.wsBuilt = true;
  G.uver++;
  for (const i of b.tiles) markUndRedraw(i);
}
function idleWander(a, dt) {
  // stay near the queen room, step around a little
  if (!a.goal || a.goal.t < G.t) {
    const room = G.builds.filter((b) => b.kind === "qs" && b.done);
    const pool = room.length ? room[rnd(room.length)].tiles : [G.queenTile];
    a.goal = { tile: pool[rnd(pool.length)], t: G.t + 3 + R() * 4 };
  }
  if (a.layer === "U") { const r = uWalk(a, a.goal.tile, dt, 0.5); if (r === true || r === "fail") a.moving = false; }
  else goTo(a, "U", uCx(G.queenTile), uCy(G.queenTile), dt);
}

// ---------- loaders and scouts (food) ----------
function nearBase(f, r) { if (f.layer !== "S") return false; for (const b of G.bases) if (b.alive && dist2(b.x, b.y, f.x, f.y) < r * r) return true; return false; }
function foodTargets(a) {
  const out = [];
  for (const f of G.foods) {
    if (f.amt <= 0 || !f.known) continue;
    if (nearBase(f, 50)) continue;   // too close to an enemy base: guards kill the workers
    if (f.kind === "chitin" && !a.str) continue;
    out.push(f);
  }
  return out;
}
function pickFood(a) {
  const list = foodTargets(a);
  if (!list.length) return null;
  let best = null, bs = 1e18;
  for (const f of list) {
    let d;
    if (f.layer === "S") d = Math.hypot(f.x - G.nest.x, f.y - G.nest.y) + (a.layer === "S" ? Math.hypot(f.x - a.x, f.y - a.y) * 0.5 : 0);
    else d = 30 + (field(G.ent)[f.tile] || 0) * T;
    const crowd = G.ants.filter((o) => o.alive && o.food === f).length;
    const s = d + crowd * 26 + (f.kind === "chitin" ? -30 : 0) + (nearBase(f, 95) ? 150 : 0);
    if (s < bs) { bs = s; best = f; }
  }
  return best;
}
function biteTime(a) { return 1.2 + (G.perks.has("heavy") && a.caste === "loader" ? 2 : 0); }
function takeBite(a, f) {
  let n = a.str;
  if (f.kind !== "chitin") {
    n *= FOOD_K;
    if (a.A.solid2 && FOODS[f.kind] && FOODS[f.kind].solid) n *= 2;
    if (a.caste === "loader" && G.perks.has("heavy") && R() < 0.5) n += 1;
    if (a.newbornT > 0) n += 1;
    if (a.bites > 0) { n += 1; a.bites--; }
    if (a.caste === "scout" && G.perks.has("lucky") && R() < 0.5) n *= 2;
  }
  n = Math.min(n, f.amt);
  f.amt -= n;
  if (f.amt <= 0 && f.drop) G.foods.splice(G.foods.indexOf(f), 1);
  a.carry = f.kind === "chitin" ? { chitin: n } : { food: n };
}
function deliver(a, dt) {
  // carry home: into a stock with space, else onto the pile at the entrance
  const kind = a.carry.food ? "food" : "chitin";
  if (a.layer === "S") { goTo(a, "U", 0, 0, dt); return; }
  if (a.dTo == null) { a.dTo = storeSpot(kind, uAt(a.x, a.y)); if (a.dTo >= 0) G.resv[a.dTo] = (G.resv[a.dTo] || 0) + (a.carry.food || a.carry.chitin); else a.dTo = G.ent; }
  const r = uWalk(a, a.dTo, dt);
  if (r === "fail" || r === true) {
    const n = a.carry.food || a.carry.chitin;
    if (a.dTo !== G.ent) {
      G.resv[a.dTo] -= n;
      if (kind === "food") { const k = r === true ? Math.min(n, foodCap(a.dTo) - G.sf[a.dTo]) : 0; G.sf[a.dTo] += k; G.pile.food += n - k; }
      else { const k = r === true ? Math.min(n, chitinCap(a.dTo) - G.sc[a.dTo]) : 0; G.sc[a.dTo] += k; G.pile.chitin += n - k; }
    } else { if (kind === "food") G.pile.food += n; else G.pile.chitin += n; }
    if (kind === "food") G.st.food += n; else G.chitinGot += n;
    a.carry = null; a.dTo = null;
  }
}
function forage(a, dt) {
  // go to a food source, bite, bring it home. returns false when there is nothing to do
  if (a.carry) { deliver(a, dt); return true; }
  if (!a.food || a.food.amt <= 0 || !G.foods.includes(a.food)) { a.food = pickFood(a); a.biteT = 0; }
  const f = a.food;
  if (!f) return false;
  if (f.layer === "U") {
    if (goTo(a, "U", f.x, f.y, dt) || (a.layer === "U" && uAt(a.x, a.y) === f.tile)) { a.moving = false; a.biteT = (a.biteT || 0) + dt; if (a.biteT >= biteTime(a)) { takeBite(a, f); a.food = null; } }
    return true;
  }
  const arrived = a.layer === "S" && dist2(a.x, a.y, f.x, f.y) < 25;
  if (!arrived) { goTo(a, "S", f.x + Math.cos(a.id) * 3, f.y + Math.sin(a.id) * 3, dt); return true; }
  a.moving = false; a.biteT = (a.biteT || 0) + dt;
  if (a.biteT >= biteTime(a)) { takeBite(a, f); a.food = null; }
  return true;
}
function patrolNear(a, cx, cy, r, dt, mul) {
  if (a.layer !== "S") { goTo(a, "S", cx, cy, dt, mul); return; }
  if (!a.goal || a.goal.t < G.t || (Math.abs(a.x - a.goal.x) < 1 && Math.abs(a.y - a.goal.y) < 1 && (a.goal.t = Math.min(a.goal.t, G.t + 0.8)) && false)) {
    const ang = R() * 6.283, d = Math.sqrt(R()) * r;
    a.goal = { x: clamp(cx + Math.cos(ang) * d, 4, SURF_W * T - 4), y: clamp(cy + Math.sin(ang) * d, 4, SURF_H * T - 4), t: G.t + 2 + R() * 3 };
  }
  if (stepToward(a, a.goal.x, a.goal.y, dt, (mul || 1) * 0.6)) a.moving = false;
}
function loaderStep(a, dt) {
  if (a.newbornT > 0) a.newbornT -= dt;
  const mul = a.newbornT > 0 ? 1.4 : 1;
  // defend: enemies near the nest, or the one that hit me
  if (a.tgt && a.tgt.alive && a.tgt.layer === a.layer && dist2(a.x, a.y, a.tgt.x, a.tgt.y) < 40 * 40) { fight(a, a.tgt, dt, mul); return; }
  a.tgt = null;
  // hurt loaders run home, and nobody fights a giant alone; healthy ones guard the nest area
  if (a.layer === "S") { const big = nearestFoe(a, 30, (e) => e.maxhp > a.maxhp * 3 || a.hp < a.maxhp * 0.45); if (big) { a.tgt = null; goTo(a, "U", 0, 0, dt, 1.2); return; } }
  if (a.layer === "U" && a.hp < a.maxhp) a.hp = Math.min(a.maxhp, a.hp + dt * 0.5);
  const foe = nearestFoe(a, a.vis * VIS_PX, (e) => a.layer === "U" || dist2(e.x, e.y, G.nest.x, G.nest.y) < 70 * 70);
  if (foe && !a.carry) { a.tgt = foe; fight(a, foe, dt, mul); return; }
  if (forage(a, dt * mul)) return;
  patrolNear(a, G.nest.x, G.nest.y, 34 + a.dist * 6, dt);
}
function scoutRadius(a) { return 95 + a.dist * 14; }
function exploreGoal(a) {
  const R0 = scoutRadius(a);
  let best = null, bd = 1e18;
  for (let k = 0; k < 24; k++) {
    const ang = R() * 6.283, d = 30 + Math.sqrt(R()) * R0;
    const x = clamp(G.nest.x + Math.cos(ang) * d, 6, SURF_W * T - 6), y = clamp(G.nest.y + Math.sin(ang) * d * 1.2, 6, SURF_H * T - 6);
    if (seenS(x, y)) continue;
    const s = dist2(x, y, a.x, a.y);
    if (s < bd) { bd = s; best = { x, y }; }
  }
  return best;
}
function scoutStep(a, dt) {
  const flees = a.A.flees;
  if (flees) {
    const foe = nearestFoe(a, 26);
    if (foe) a.fleeT = 2.2;
    if (a.fleeT > 0) { a.fleeT -= dt; if (a.layer === "S") { const dx = a.x - (foe ? foe.x : G.nest.x), dy = a.y - (foe ? foe.y : G.nest.y); const d = Math.hypot(dx, dy) || 1; stepToward(a, G.nest.x * 0.5 + (a.x + (dx / d) * 30) * 0.5, G.nest.y * 0.5 + (a.y + (dy / d) * 30) * 0.5, dt, 1.2); } return; }
  } else {
    const big = a.layer === "S" ? nearestFoe(a, 30, (e) => e.maxhp > a.maxhp * 3) : null;
    if (big) { a.tgt = null; a.fleeT = 1.5; }
    else if (a.tgt && a.tgt.alive && a.tgt.layer === a.layer && dist2(a.x, a.y, a.tgt.x, a.tgt.y) < 50 * 50) { fight(a, a.tgt, dt); return; }
    a.tgt = null;
    if (a.fleeT > 0) { a.fleeT -= dt; goTo(a, "U", 0, 0, dt, 1.2); return; }
    const foe = nearestFoe(a, 30);
    if (foe) { a.tgt = foe; fight(a, foe, dt); return; }
  }
  if (a.carry) { deliver(a, dt); return; }
  // food seen close by: grab it (seekers and adventurers)
  if (a.str > 0 && a.layer === "S") {
    const near = G.foods.find((f) => f.layer === "S" && f.known && f.amt > 0 && dist2(f.x, f.y, a.x, a.y) < 30 * 30 && !nearBase(f, 50));
    if (near) { if (a.food !== near) { a.food = near; a.biteT = 0; } forage(a, dt); return; }
  }
  if (!a.exp || seenS(a.exp.x, a.exp.y) || a.exp.t < G.t) { const g = exploreGoal(a); a.exp = g ? { x: g.x, y: g.y, t: G.t + 25 } : null; }
  if (a.exp) { if (goTo(a, "S", a.exp.x, a.exp.y, dt)) a.exp = null; return; }
  // everything near is explored: forage like a loader, or walk the edge
  if (a.str > 0 && forage(a, dt)) return;
  patrolNear(a, G.nest.x, G.nest.y, scoutRadius(a), dt);
}
// ---------- warriors, guardians ----------
function homeTile(a) {
  if (a.home) { const b = G.builds.find((x) => x.id === a.home); if (b && b.done) return b.tiles[0]; }
  const kind = a.A.bigBarracks ? "bb" : "br";
  const list = doneOf(kind);
  if (list.length) { const b = list[a.id % list.length]; a.home = b.id; return b.tiles[0]; }
  return G.queenTile;
}
function raidStep(a, dt) {
  const b = baseById(a.raid);
  const mul = G.perks.has("blitz") ? 3 : G.perks.has("platoon") ? 0.7 : 1;
  if (!b || !b.alive) { a.raid = null; return false; }
  const foe = nearestFoe(a, a.vis * VIS_PX * (a.layer === "S" ? 1 : 0.5));
  if (foe && (a.layer === "U" || dist2(foe.x, foe.y, b.x, b.y) < 70 * 70 || dist2(foe.x, foe.y, a.x, a.y) < 24 * 24)) { fight(a, foe, dt, mul); return true; }
  if (a.layer !== "S" || dist2(a.x, a.y, b.x, b.y) > 14 * 14) { goTo(a, "S", b.x + Math.cos(a.id * 1.7) * 9, b.y + Math.sin(a.id * 1.7) * 9, dt, mul); return true; }
  a.moving = false; a.th = Math.atan2(b.y - a.y, b.x - a.x);
  if (a.cool <= 0) {
    a.cool = a.as;
    let d = a.dmg * (G.perks.has("siege") ? 2 : 1);
    b.hp -= d; b.hitT = 0.15;
    if (b.hp <= 0) destroyBase(b);
  }
  return true;
}
function destroyBase(b) {
  if (!b.alive) return;
  b.alive = false; b.hp = 0;
  const B = BASES[b.kind];
  const [s0, s1] = SUGAR_TIER[B.tier];
  const sugar = s0 + rnd(s1 - s0 + 1);
  G.sugar += sugar;
  dropAt("S", b.x - 6, b.y, "drop", 20 + B.tier * 20);
  dropAt("S", b.x + 6, b.y + 4, "drop", 12 + B.tier * 10);
  dropAt("S", b.x, b.y - 6, "chitin", 2 + B.tier * 2);
  // card drop: common 50 %, rare 20 %, epic 5 %
  const roll = R();
  const pool = Object.keys(ANTS).filter((k) => casteOf(k) !== "queen");
  if (roll < 0.05 + B.tier * 0.03) G.cardsWon.push([pool[rnd(pool.length)], 2]);
  else if (roll < 0.25) G.cardsWon.push([pool[rnd(pool.length)], 1]);
  else if (roll < 0.75) G.cardsWon.push([pool[rnd(pool.length)], 0]);
  toast(B.name + " destroyed! +" + sugar + " sugar", "#ffcf4a");
  G.fx.push({ x: b.x, y: b.y, t: 1.2, ring: true, big: true, col: "#ffcf4a", layer: "S" });
  for (const m of G.mobs) if (m.alive && m.base === b.id) { m.role = "wander"; m.base = 0; }
  if (G.bases.every((x) => !x.alive)) { G.winT = 1.5; }
}
function warriorStep(a, dt) {
  if (a.raid && raidStep(a, dt)) return;
  // defend the nest: intruders underground, or enemies near the entrance
  const under = G.mobs.find((m) => m.alive && m.layer === "U");
  if (under) { a.tgt = under; fight(a, under, dt); return; }
  if (a.tgt && a.tgt.alive && a.tgt.layer === "S" && dist2(a.tgt.x, a.tgt.y, G.nest.x, G.nest.y) < 80 * 80) { if (a.layer === "U") goTo(a, "S", G.nest.x, G.nest.y, dt); else fight(a, a.tgt, dt); return; }
  a.tgt = null;
  const near = G.mobs.find((m) => m.alive && m.layer === "S" && m.role !== "wander" && dist2(m.x, m.y, G.nest.x, G.nest.y) < 50 * 50 && seenS(m.x, m.y));
  if (near) { a.tgt = near; return; }
  // rest at home
  const h = homeTile(a);
  if (a.layer !== "U") { goTo(a, "U", uCx(h), uCy(h), dt); return; }
  const r = uWalk(a, h, dt, 0.8);
  if (r === true || uAt(a.x, a.y) === h) { a.moving = false; a.hp = Math.min(a.maxhp, a.hp + dt * 1); }
}
function guardianStep(a, dt) {
  a.hp = Math.min(a.maxhp, a.hp + dt * 0.25);
  const isDef = a.key === "defender";
  // enemies inside the nest come first
  const under = G.mobs.find((m) => m.alive && m.layer === "U");
  if (under && isDef) { a.tgt = under; fight(a, under, dt); return; }
  const leashX = isDef ? G.nest.x : a.px || G.nest.x, leashY = isDef ? G.nest.y : a.py || G.nest.y;
  const leash = isDef ? 95 : 60;
  if (a.tgt && a.tgt.alive && a.tgt.layer === a.layer && dist2(a.tgt.x, a.tgt.y, leashX, leashY) < leash * leash) { fight(a, a.tgt, dt); return; }
  a.tgt = null;
  const thief = isDef ? nearestFoe(a, a.vis * VIS_PX * 1.3, (e) => e.M.thief) : null;
  const foe = thief || nearestFoe(a, a.vis * VIS_PX, (e) => (e.role !== "wander" || e.angry > 0) && dist2(e.x, e.y, leashX, leashY) < leash * leash);
  if (foe) { a.tgt = foe; fight(a, foe, dt); return; }
  if (isDef) { patrolNear(a, G.nest.x, G.nest.y, 22 + a.dist * 8, dt); return; }
  // bodyguard: walk along a food trail
  if (!a.trail || a.trail.t < G.t) {
    const trails = G.foods.filter((f) => f.layer === "S" && f.known && f.amt > 0);
    if (trails.length) { const f = trails[rnd(trails.length)]; const k = 0.3 + R() * 0.7; a.px = G.nest.x + (f.x - G.nest.x) * k; a.py = G.nest.y + (f.y - G.nest.y) * k; }
    else { a.px = G.nest.x; a.py = G.nest.y; }
    a.trail = { t: G.t + 8 + R() * 6 };
  }
  patrolNear(a, a.px, a.py, 20, dt);
}

// ---------- squads ----------
// order: { kind: "patrol"|"guard"|"path"|"raid"|"queen"|"free", x, y, r, base }
function squadById(id) { return G.squads.find((s) => s.id === id); }
function freeAnts(key) { return G.ants.filter((a) => a.alive && a.key === key && !a.squad && !a.raid); }
function squadMembers(s) { return G.ants.filter((a) => a.alive && a.squad === s.id); }
function fillSquad(s) {
  for (const key in s.want) {
    let have = squadMembers(s).filter((a) => a.key === key).length;
    for (const a of freeAnts(key)) { if (have >= s.want[key]) break; a.squad = s.id; a.tgt = null; a.goal = null; have++; }
    // too many: let the extra go
    if (have > s.want[key]) for (const a of squadMembers(s).filter((x) => x.key === key).slice(s.want[key])) a.squad = null;
  }
}
function joinSquadOnHatch(a) {
  for (const s of G.squads) {
    if (!s.refill || !s.want[a.key]) continue;
    const have = squadMembers(s).filter((x) => x.key === a.key).length;
    if (have < s.want[a.key]) { a.squad = s.id; return; }
  }
}
function squadStep(a, s, dt) {
  const o = s.order, m = squadMembers(s), idx = m.indexOf(a), n = m.length;
  if (o.kind === "free") return false;
  if (o.kind === "raid") { if (!a.raid) { const b = baseById(o.base); if (!b || !b.alive) { s.order = { kind: "free" }; toast(s.name + ": target gone, back to normal", "#c08bff"); return false; } a.raid = o.base; } return raidStep(a, dt); }
  if (o.kind === "queen") {
    const foe = nearestFoe(a, 60);
    if (foe && foe.layer === "U") { fight(a, foe, dt); return true; }
    const ang = (idx / Math.max(1, n)) * 6.283;
    const gx = uCx(G.queenTile) + Math.cos(ang) * 6, gy = uCy(G.queenTile) + Math.sin(ang) * 4;
    if (goTo(a, "U", gx, gy, dt)) a.moving = false;
    return true;
  }
  // surface orders: fight what comes inside the zone
  let cx = o.x, cy = o.y, leash = 40;
  if (o.kind === "patrol") leash = o.r + 30;
  if (o.kind === "path") { cx = (o.x + G.nest.x) / 2; cy = (o.y + G.nest.y) / 2; leash = Math.hypot(o.x - G.nest.x, o.y - G.nest.y) / 2 + 30; }
  if (a.tgt && a.tgt.alive && a.tgt.layer === "S" && dist2(a.tgt.x, a.tgt.y, cx, cy) < leash * leash) { fight(a, a.tgt, dt); return true; }
  a.tgt = null;
  if (a.layer === "S") {
    const foe = nearestFoe(a, a.vis * VIS_PX, (e) => (e.role !== "wander" || e.angry > 0 || m.length >= 5) && dist2(e.x, e.y, cx, cy) < leash * leash);
    if (foe) { a.tgt = foe; fight(a, foe, dt); return true; }
    // squad mates in a fight pull the others in
    const mate = m.find((x) => x !== a && x.tgt && x.tgt.alive && x.layer === "S" && dist2(x.x, x.y, a.x, a.y) < 60 * 60);
    if (mate) { a.tgt = mate.tgt; return true; }
  }
  if (o.kind === "guard") {
    const ang = (idx / Math.max(1, n)) * 6.283 + 0.4;
    if (goTo(a, "S", o.x + Math.cos(ang) * (4 + n), o.y + Math.sin(ang) * (4 + n), dt)) { a.moving = false; a.th = ang; }
    return true;
  }
  if (o.kind === "patrol") {
    // the squad walks together: one shared waypoint, each ant a little apart
    if (!s.wp || s.wpT < G.t || (m[0] === a && a.layer === "S" && dist2(a.x, a.y, s.wp.x, s.wp.y) < 9)) {
      if (!s.wp || s.wpT < G.t || m[0] === a) { const ang = R() * 6.283, d = Math.sqrt(R()) * o.r; s.wp = { x: clamp(o.x + Math.cos(ang) * d, 4, SURF_W * T - 4), y: clamp(o.y + Math.sin(ang) * d, 4, SURF_H * T - 4) }; s.wpT = G.t + 4 + R() * 3; }
    }
    const ang = (idx / Math.max(1, n)) * 6.283;
    goTo(a, "S", s.wp.x + Math.cos(ang) * 5, s.wp.y + Math.sin(ang) * 5, dt, 0.8);
    return true;
  }
  if (o.kind === "path") {
    if (s.leg == null) s.leg = 1;
    const tx = s.leg ? o.x : G.nest.x + (o.x - G.nest.x) * 0.15, ty = s.leg ? o.y : G.nest.y + (o.y - G.nest.y) * 0.15;
    const ang = (idx / Math.max(1, n)) * 6.283;
    if (goTo(a, "S", tx + Math.cos(ang) * 5, ty + Math.sin(ang) * 5, dt, 0.8) && m[0] === a) s.leg = 1 - s.leg;
    return true;
  }
  return false;
}

// ---------- enemies ----------
function mobStep(m, dt) {
  if (m.stun > 0) { m.stun -= dt; m.moving = false; return; }
  if (m.angry > 0) m.angry -= dt;
  const b = baseById(m.base);
  // pick a target
  if (m.tgt && (!m.tgt.alive || m.tgt.layer !== m.layer)) m.tgt = null;
  m.retT -= dt;
  if (m.retT <= 0) {
    m.retT = 0.4;
    if ((!m.M.thief || m.angry > 0) && !(m.role === "raid" && m.layer === "U" && m.angry <= 0)) { const t = nearestFoe(m, m.vis * VIS_PX * (m.role === "wander" && m.angry <= 0 ? 0.4 : 1)); if (t && (!m.tgt || dist2(m.x, m.y, t.x, t.y) < dist2(m.x, m.y, m.tgt.x, m.tgt.y))) m.tgt = t; }
  }
  if (m.role === "guard") {
    if (!b || !b.alive) { m.role = "wander"; return; }
    if (m.tgt && dist2(m.tgt.x, m.tgt.y, b.x, b.y) < (m.angry > 0 ? 75 : 55) ** 2) { fight(m, m.tgt, dt); return; }
    m.tgt = null;
    wanderAround(m, b.x, b.y, 26, dt);
    return;
  }
  if (m.role === "wander") {
    if (m.tgt) { fight(m, m.tgt, dt); if (m.tgt && (dist2(m.x, m.y, m.tgt.x, m.tgt.y) > 60 * 60 || dist2(m.x, m.y, G.nest.x, G.nest.y) < 70 * 70)) m.tgt = null; return; }
    if (m.layer === "U") { m.role = "raid"; return; }
    if (!m.wgoal || m.wgoal.t < G.t) m.wgoal = { x: rrange(20, SURF_W * T - 20), y: rrange(20, SURF_H * T - 20), t: G.t + 20 };
    if (dist2(m.wgoal.x, m.wgoal.y, G.nest.x, G.nest.y) < 110 * 110) m.wgoal.t = 0;
    if (stepToward(m, m.wgoal.x, m.wgoal.y, dt, 0.5)) m.wgoal.t = 0;
    return;
  }
  if (m.role === "raid") {
    if (m.tgt) { if (!fight(m, m.tgt, dt)) m.tgt = null; return; }
    // into the nest and on to the queen
    if (m.layer === "S") { if (!m.warned && dist2(m.x, m.y, G.nest.x, G.nest.y) < 110 * 110) { m.warned = true; if (!G.raidAlarm || G.t - G.raidAlarm > 10) { G.raidAlarm = G.t; toast("Enemies near the nest!", "#ff5d4a"); alertAt(m.x, m.y, "S"); } } goTo(m, "U", 0, 0, dt); return; }
    // inside: go for the queen; hit back only at ants that hit it
    if (m.tgt && m.tgt.alive && m.tgt.layer === "U" && m.angry > 0) { fight(m, m.tgt, dt); return; }
    if (G.queen && G.queen.alive) { m.tgt = G.queen; fight(m, G.queen, dt); }
    return;
  }
  if (m.role === "thief") {
    if (m.tgt && m.angry > 0) { fight(m, m.tgt, dt); return; }
    if (m.carry > 0) {
      // run home with the food
      if (m.layer === "U") { goTo(m, "S", 0, 0, dt, 1.1); return; }
      const hx = b && b.alive ? b.x : m.x < G.nest.x ? 0 : SURF_W * T, hy = b && b.alive ? b.y : m.y;
      if (stepToward(m, hx, hy, dt, 1.1)) { m.alive = false; if (b) b.stash = (b.stash || 0) + m.carry; }
      return;
    }
    if (m.layer === "S") { goTo(m, "U", 0, 0, dt); return; }
    // find food inside
    if (m.stealAt == null || (m.stealAt !== G.ent && G.sf[m.stealAt] <= 0)) { const s = foodSources(); m.stealAt = s.length ? closestTile(uAt(m.x, m.y), s) : -2; }
    if (m.stealAt === -2) { m.carry = 0.0001; return; }
    const r = uWalk(m, m.stealAt, dt);
    if (r === true || r === "fail") {
      const got = r === true ? takeFood(m.stealAt, 3) : 0;
      m.carry = got || 0.0001; m.stealAt = null;
      if (got) { toast("A thief took " + got + " food!", "#ff9a3c"); alertAt(uCx(G.ent), uCy(G.ent), "U"); }
    }
  }
}
function wanderAround(m, cx, cy, r, dt) {
  if (m.layer !== "S") { goTo(m, "S", cx, cy, dt); return; }
  if (!m.wgoal || m.wgoal.t < G.t) { const a = R() * 6.283, d = R() * r; m.wgoal = { x: cx + Math.cos(a) * d, y: cy + Math.sin(a) * d, t: G.t + 2 + R() * 3 }; }
  if (stepToward(m, m.wgoal.x, m.wgoal.y, dt, 0.4)) m.moving = false;
}
function basesStep(dt) {
  for (const b of G.bases) {
    if (!b.alive) continue;
    if (b.hitT > 0) b.hitT -= dt;
    // guards come back over time
    if (b.guards.length < b.glist.length) { b.respawnT -= dt; if (b.respawnT <= 0) { const have = b.guards.map((id) => G.mobs.find((m) => m.id === id)).filter(Boolean).map((m) => m.type); const need = b.glist.slice(); for (const h of have) { const k = need.indexOf(h); if (k >= 0) need.splice(k, 1); } if (need.length) spawnMob(need[0], b, "guard"); b.respawnT = 40; } }
    // the base heals slowly when nobody hits it
    if (!b.hitT || b.hitT <= 0) b.hp = Math.min(b.maxhp, b.hp + dt * 0.3);
    const B = BASES[b.kind];
    b.waveT -= dt;
    if (b.waveT <= 0) {
      const n = Math.min(5, 1 + Math.floor(G.t / 360) + (B.tier >= 2 ? 1 : 0));
      for (let k = 0; k < n; k++) spawnMob(B.raid, b, "raid");
      b.waveT = (150 + rnd(70)) / Math.sqrt(G.mul) + G.bases.filter((x) => x.alive).length * 15;
    }
    if (B.thief) { b.thiefT -= dt; if (b.thiefT <= 0) { spawnMob(B.thief, b, "thief"); b.thiefT = 80 + rnd(50); } }
  }
}

// ---------- the main step ----------
function antStep(a, dt) {
  if (a.stun > 0) { a.stun -= dt; return; }
  if (a.squad) { const s = squadById(a.squad); if (!s) a.squad = null; else if (squadStep(a, s, dt)) return; }
  switch (a.caste) {
    case "queen": return;
    case "builder": return builderStep(a, dt);
    case "loader": return a.A.fights || a.A.flees ? scoutStep(a, dt) : loaderStep(a, dt);
    case "scout": return scoutStep(a, dt);
    case "warrior": case "shooter": return warriorStep(a, dt);
    case "guardian": return guardianStep(a, dt);
  }
}
function step(dt) {
  if (!G || G.over) return;
  G.t += dt;
  for (const a of G.ants) {
    if (!a.alive) continue;
    if (a.cool > 0) a.cool -= dt;
    if (a.hitT > 0) a.hitT -= dt;
    if (a.digging > 0) a.digging -= dt;
    a.anim += dt * (a.moving ? 10 : 0);
    antStep(a, dt);
    // eyes on the surface
    if (a.layer === "S") {
      a.revT -= dt;
      if (a.revT <= 0) { a.revT = 0.25; revealCircle(a.x, a.y, a.vis * VIS_PX * (a.caste === "scout" ? 1 : 0.6)); }
      for (const s of G.sugars) if (!s.got && dist2(s.x, s.y, a.x, a.y) < 100) { s.got = true; G.sugar += s.n; G.sugarFound += s.n; toast("Sugar +" + s.n + "  (" + G.sugarFound + "/" + G.sugarTotal + ")", "#f4e9df"); G.fx.push({ x: s.x, y: s.y, t: 0.8, ring: true, col: "#ffffff", layer: "S" }); }
    } else if (a.layer === "U") G.seen[uAt(a.x, a.y)] = 1;
  }
  queenStep(dt);
  for (const m of G.mobs) {
    if (!m.alive) continue;
    if (m.cool > 0) m.cool -= dt;
    if (m.hitT > 0) m.hitT -= dt;
    m.anim += dt * (m.moving ? 9 : 0);
    mobStep(m, dt);
  }
  basesStep(dt);
  // tidy
  if (G.ants.some((a) => !a.alive)) G.ants = G.ants.filter((a) => a.alive);
  if (G.mobs.some((m) => !m.alive)) G.mobs = G.mobs.filter((m) => m.alive);
  for (const s of G.shots) s.t -= dt; G.shots = G.shots.filter((s) => s.t > 0);
  for (const f of G.fx) f.t -= dt; G.fx = G.fx.filter((f) => f.t > 0);
  for (const t of G.toasts) t.t -= dt; G.toasts = G.toasts.filter((t) => t.t > 0);
  for (const t of G.alerts) t.t -= dt; G.alerts = G.alerts.filter((t) => t.t > 0);
  const n = G.ants.length; G.st.maxAnts = Math.max(G.st.maxAnts, n);
  if (G.over) return;
  if (G.winT != null) { G.winT -= dt; if (G.winT <= 0) endRun("win"); }
  const builders = G.ants.filter((a) => a.caste === "builder").length + G.eggs.filter((e) => casteOf(e.key) === "builder").length;
  if (builders === 0 && G.t > 10) endRun("lose", "No builders left");
}
function endRun(kind, why) { if (G.over) return; G.over = kind; G.overWhy = why || ""; G.overT = 0; if (typeof onRunOver === "function") onRunOver(); }

// lists kept for the builders (designations)
function initJobs() { G.digList = []; G.fillList = []; G.digResv = new Set(); G.resv = {}; G.foodResv = 0; G.feedComing = 0; G.wsComing = 0; G.plateComing = 0; G.qgroup0 = G.builds.filter((b) => b.kind === "qs").flatMap((b) => b.tiles); }
function markDig(i) {
  if (G.ut[i] === U_STONE || G.ut[i] === U_EMPTY) return false;
  if (G.um[i] === 1) { G.um[i] = 0; G.digList = G.digList.filter((t) => t !== i); return true; }
  G.um[i] = 1; G.digList.push(i); return true;
}
function markFill(i) {
  if (G.um[i] === 2) { G.um[i] = 0; G.fillList = G.fillList.filter((t) => t !== i); return true; }
  if (!canFill(i)) return false;
  G.um[i] = 2; G.fillList.push(i); return true;
}
function startRaid(base, n) {
  const cap = 10 + (G.perks.has("blitz") ? -5 : 0) + (G.perks.has("platoon") ? 5 : 0);
  const pool = G.ants.filter((a) => a.alive && (a.caste === "warrior" || a.caste === "shooter") && !a.raid && !a.squad);
  const go = pool.slice(0, Math.min(n, cap));
  for (const a of go) { a.raid = base.id; a.tgt = null; }
  G.st.raidMax = Math.max(G.st.raidMax, go.length);
  return go.length;
}
function recallRaid(base) { for (const a of G.ants) if (a.raid === base.id && !a.squad) a.raid = null; }
function raidCount(base) { return G.ants.filter((a) => a.alive && a.raid === base.id).length; }
