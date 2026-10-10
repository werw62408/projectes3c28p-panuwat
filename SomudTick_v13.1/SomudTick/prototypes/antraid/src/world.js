"use strict";
// ===== Run state, map generation, underground grid, path finding =====

function mulberry(seed) {
  let s = seed >>> 0;
  return function () { s = (s + 0x6D2B79F5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}
let R = Math.random;                       // run RNG (seeded per run)
const rnd = (n) => Math.floor(R() * n);
const rrange = (a, b) => a + R() * (b - a);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const dist2 = (ax, ay, bx, by) => (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
const lvlv = (v, l) => (Array.isArray(v) ? v[Math.min(l, v.length) - 1] : v);

// tile kinds underground
const U_EMPTY = 0, U_SOFT = 1, U_HARD = 2, U_STONE = 3;
const U_HP = [0, 8, 32, 0];
const BKIND = [null, "qs", "fs", "cs", "br", "bb", "ws"];
const BCODE = { qs: 1, fs: 2, cs: 3, br: 4, bb: 5, ws: 6 };

let G = null;   // the running game
let NEXT_ID = 1;

function uIdx(x, y) { return y * UND_W + x; }
function uX(i) { return i % UND_W; }
function uY(i) { return (i / UND_W) | 0; }
function uCx(i) { return uX(i) * T + T / 2; }
function uCy(i) { return uY(i) * T + T / 2; }
function uAt(px, py) { const x = clamp(Math.floor(px / T), 0, UND_W - 1), y = clamp(Math.floor(py / T), 0, UND_H - 1); return uIdx(x, y); }
function sAt(px, py) { const x = clamp(Math.floor(px / T), 0, SURF_W - 1), y = clamp(Math.floor(py / T), 0, SURF_H - 1); return y * SURF_W + x; }

// ---------- new run ----------
function newRun(cfg) {
  // cfg: { loc, lvl, deck:[{key,rar}], perks:[ids], boosts:[ids], seed }
  const L = LOCS[cfg.loc], LV = L.levels[cfg.lvl];
  R = mulberry(cfg.seed || (Date.now() & 0xffffffff));
  NEXT_ID = 1;
  const qcard = cfg.deck.find((c) => ANTS[c.key].castes[0] === "queen");
  const Q = ANTS[qcard.key].q;
  G = {
    cfg, L, LV, loc: cfg.loc, lvl: cfg.lvl, mul: LV.mul, t: 0, speed: 1, paused: false, over: null, overT: 0,
    deck: cfg.deck.map((c) => ({ key: c.key, rar: c.rar })), qcard, Q,
    perks: new Set(cfg.perks), boosts: new Set(cfg.boosts),
    ut: new Uint8Array(UND_W * UND_H), uhp: new Float32Array(UND_W * UND_H), um: new Uint8Array(UND_W * UND_H),
    ub: new Uint8Array(UND_W * UND_H), ubid: new Int32Array(UND_W * UND_H), sf: new Int16Array(UND_W * UND_H), sc: new Int16Array(UND_W * UND_H),
    seen: new Uint8Array(UND_W * UND_H), uver: 1, ufields: new Map(),
    builds: [], fog: new Uint8Array(SURF_W * SURF_H), fogVer: 1,
    foods: [], sugars: [], bases: [], deco: [],
    ants: [], mobs: [], eggs: [], shots: [], fx: [], queen: null,
    queue: [], fed: 0, lvls: {}, upgrade: null, plates: 0, wsChitin: 0, wsT: 0, wsPlate: 0,
    pile: { food: 0, chitin: 0 }, sugar: 0, sugarFound: 0, sugarTotal: 0, chitinGot: 0,
    squads: [], prio: { dig: 2, fill: 1, build: 2, feed: 3, store: 2, ws: 1 },
    toasts: [], alerts: [], cardsWon: [], deaths: {},
    st: { food: 0, dug: 0, eggsMade: 0, kills: 0, workerKills: 0, queenHits: 0, maxAnts: 0, maxEggs: 0, raidMax: 0, scouts: 0, loaders: 0, queenHit: false, queenMoved: false, wsBuilt: false, maxLvl: 1 },
    nest: { x: (SURF_W * T) / 2, y: (SURF_H * T) / 2 }, ent: uIdx(15, 0), queenTile: 0, cam: { S: { x: 0, y: 0 }, U: { x: 0, y: 0 } }, view: "S",
  };
  for (const c of G.deck) G.lvls[c.key] = 1;
  genUnder();
  genSurface();
  startColony();
  return G;
}

function genUnder() {
  const L = G.L;
  // soil: soft near the top, hard patches deeper, stones
  for (let y = 0; y < UND_H; y++) for (let x = 0; x < UND_W; x++) {
    const i = uIdx(x, y);
    const n = Math.sin(x * 0.7 + y * 0.31 + G.cfg.seed % 7) + Math.sin(x * 0.23 - y * 0.57) + R() * 0.8;
    let k = U_SOFT;
    if (n > 1.15 - y * 0.025) k = U_HARD;
    G.ut[i] = k; G.uhp[i] = U_HP[k];
  }
  for (let s = 0; s < 14; s++) {   // stone clusters
    const cx = rnd(UND_W), cy = 6 + rnd(UND_H - 6), r = 1 + rnd(2);
    for (let y = -r; y <= r; y++) for (let x = -r; x <= r; x++) {
      if (x * x + y * y > r * r + 0.5 || R() < 0.25) continue;
      const tx = cx + x, ty = cy + y; if (tx < 0 || ty < 0 || tx >= UND_W || ty >= UND_H) continue;
      G.ut[uIdx(tx, ty)] = U_STONE;
    }
  }
  // the nest: entrance tunnel, then the queen room
  const ex = 15;
  for (let y = 0; y <= 5; y++) setEmpty(uIdx(ex, y));
  for (let x = ex - 4; x <= ex; x++) setEmpty(uIdx(x, 5));
  const qy = 6, qx0 = ex - 7;
  for (let y = qy; y <= qy + 1; y++) for (let x = qx0; x < qx0 + 6; x++) setEmpty(uIdx(x, y));
  for (let x = ex - 4; x <= ex - 2; x++) setEmpty(uIdx(x, 5));
  // keep the ring around the start area soft so the first digs are easy
  for (let y = 0; y <= 10; y++) for (let x = ex - 10; x <= ex + 4; x++) { const i = uIdx(x, y); if (G.ut[i] === U_STONE || G.ut[i] === U_HARD) { G.ut[i] = U_SOFT; G.uhp[i] = 8; } }
  G.qroom = []; for (let y = qy; y <= qy + 1; y++) for (let x = qx0; x < qx0 + 6; x++) G.qroom.push(uIdx(x, y));
  // underground mushroom caves (closed pockets, found by digging)
  G.caves = [];
  for (let c = 0; c < 4; c++) {
    let cx, cy, ok = false;
    for (let tries = 0; tries < 40 && !ok; tries++) { cx = 2 + rnd(UND_W - 4); cy = 12 + rnd(UND_H - 15); ok = true; }
    const tiles = [uIdx(cx, cy), uIdx(cx + 1, cy)];
    for (const i of tiles) { G.ut[i] = U_EMPTY; G.uhp[i] = 0; }
    const f = { id: NEXT_ID++, kind: "cave", layer: "U", tile: tiles[0], x: uCx(tiles[0]) + 4, y: uCy(tiles[0]), amt: 0, max: 0, known: false };
    f.amt = f.max = Math.round(rrange(...FOODS.cave.amt));
    G.foods.push(f); G.caves.push({ tiles, food: f });
  }
  G.uver++;
}
function setEmpty(i) { G.ut[i] = U_EMPTY; G.uhp[i] = 0; G.seen[i] = 1; }

function genSurface() {
  const L = G.L, LV = G.LV, nx = G.nest.x, ny = G.nest.y;
  const W = SURF_W * T, H = SURF_H * T;
  // decorations (rocks, plants): look only
  for (let k = 0; k < 90; k++) G.deco.push({ x: rnd(W), y: rnd(H), k: rnd(6), s: 0.6 + R() * 0.8 });
  // enemy bases spread around the nest
  const nb = LV.bases.length, a0 = R() * Math.PI * 2;
  LV.bases.forEach((kind, j) => {
    const a = a0 + (j / nb) * Math.PI * 2 + rrange(-0.35, 0.35);
    const d = 122 + rnd(40) + (j % 2) * 12 + G.lvl * 8;
    const x = clamp(nx + Math.cos(a) * d * 0.95, 24, W - 24), y = clamp(ny + Math.sin(a) * d * 1.15, 24, H - 24);
    const B = BASES[kind];
    const b = { id: NEXT_ID++, kind, x, y, hp: Math.round(B.hp * G.mul), maxhp: Math.round(B.hp * G.mul), alive: true, known: false,
      guards: [], respawnT: 30, waveT: 280 + BASES[kind].tier * 120 + rnd(100) + j * 40, thiefT: 200 + rnd(80) + j * 30, raidBy: 0 };
    G.bases.push(b);
  });
  // food sources: a few close to the nest, more further out
  const fk = L.food;
  const place = (dmin, dmax, kind) => {
    for (let tries = 0; tries < 30; tries++) {
      const a = R() * Math.PI * 2, d = rrange(dmin, dmax);
      const x = nx + Math.cos(a) * d, y = ny + Math.sin(a) * d * 1.3;
      if (x < 12 || y < 12 || x > W - 12 || y > H - 12) continue;
      if (G.bases.some((b) => dist2(b.x, b.y, x, y) < 40 * 40)) continue;
      if (G.foods.some((f) => f.layer === "S" && dist2(f.x, f.y, x, y) < 26 * 26)) continue;
      let amt = Math.round(rrange(...FOODS[kind].amt));
      if (G.Q.fieldBonus) amt = Math.round(amt * (1 + G.Q.fieldBonus));
      if (G.boosts.has("food10")) amt += 10;
      G.foods.push({ id: NEXT_ID++, kind, layer: "S", x, y, amt, max: amt, known: false });
      return;
    }
  };
  for (let k = 0; k < 3; k++) place(26, 55, fk[k % fk.length]);
  for (let k = 0; k < 13; k++) place(60, 250, fk[rnd(fk.length)]);
  // sugar cubes, hidden near rocks
  for (let k = 0; k < 8; k++) {
    const a = R() * Math.PI * 2, d = rrange(50, 240);
    const x = clamp(nx + Math.cos(a) * d, 10, W - 10), y = clamp(ny + Math.sin(a) * d * 1.3, 10, H - 10);
    const n = 1 + rnd(3);
    G.sugars.push({ x, y, n, got: false }); G.sugarTotal += n;
    G.deco.push({ x: x + 5, y: y + 2, k: 0, s: 1.2 });
  }
  // the fog: open a circle around the nest
  revealCircle(nx, ny, 44);
}

function revealCircle(px, py, r) {
  const x0 = Math.max(0, Math.floor((px - r) / T)), x1 = Math.min(SURF_W - 1, Math.floor((px + r) / T));
  const y0 = Math.max(0, Math.floor((py - r) / T)), y1 = Math.min(SURF_H - 1, Math.floor((py + r) / T));
  let ch = false;
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
    const i = y * SURF_W + x;
    if (G.fog[i]) continue;
    if (dist2(x * T + 4, y * T + 4, px, py) <= r * r) { G.fog[i] = 1; ch = true; }
  }
  if (ch) {
    G.fogVer++;
    for (const f of G.foods) if (!f.known && f.layer === "S" && G.fog[sAt(f.x, f.y)]) f.known = true;
    for (const b of G.bases) if (!b.known && G.fog[sAt(b.x, b.y)]) { b.known = true; toast(BASES[b.kind].name + " found", "#ffcf4a"); }
  }
}
const seenS = (x, y) => G.fog[sAt(x, y)] === 1;

function startColony() {
  const Q = G.Q, qcard = G.qcard;
  // queen rooms in the start room, plus food stocks / barracks for some queens
  const room = G.qroom.slice();
  const mk = (kind, tiles) => { const b = addBuilding(kind, tiles); b.done = true; b.got = b.need; return b; };
  let k = 0;
  const nQS = Q.startQS + (G.boosts.has("qroom2") ? 2 : 0);
  for (let j = 0; j < Math.min(nQS, room.length); j++) mk("qs", [room[k++]]);
  // extra rooms beside the start room when the boost adds more
  if (Q.startFS) { const tiles = carveRoom(uIdx(3, 6), Q.startFS); for (const i of tiles) mk("fs", [i]); }
  const nBR = Q.startBR + (G.boosts.has("bar4") ? 4 : 0);
  if (nBR) { const tiles = carveRoom(uIdx(20, 7), nBR); for (const i of tiles) mk("br", [i]); }
  if (G.boosts.has("chitin8")) { const tiles = carveRoom(uIdx(24, 10), 1); for (const i of tiles) { mk("cs", [i]); G.sc[i] = 4; } G.pile.chitin += 4; }
  // queen
  const qt = room[0] + 1;
  G.queenTile = qt;
  const q = makeAnt(qcard.key, qt, "U");
  G.queen = q;
  const qb = QUEEN_RB[qcard.rar];
  q.hsMul = qb.hs;
  if (G.boosts.has("queen3")) { q.maxhp *= 3; q.hp = q.maxhp; q.dmg += 2; }
  if (G.perks.has("carapace")) { q.maxhp *= 5; q.hp = q.maxhp; q.noFight = true; }
  // food: 12 pieces in the queen rooms
  let food = 12 + (G.boosts.has("store15") ? 15 : 0);
  food = storeFood(food);
  G.pile.food += food;
  // start eggs: builders, loader caste, scout caste (from the deck)
  const pick = (caste) => (G.deck.find((c) => ANTS[c.key].castes[0] === caste) || G.deck.find((c) => ANTS[c.key].castes.includes(caste)) || {}).key;
  const eggs = [pick("builder"), pick("builder"), pick("loader") || pick("scout"), pick("scout") || pick("loader"), pick("loader") || pick("builder")].slice(0, Q.eggs);
  if (G.boosts.has("build2")) eggs.push(pick("builder"), pick("builder"));
  if (G.boosts.has("scout3")) { const s = G.deck.find((c) => c.key === "seeker") ? "seeker" : pick("scout"); eggs.push(s, s, s); }
  if (qb.workers) for (let j = 0; j < qb.workers; j++) eggs.push(pick("builder"));
  eggs.forEach((key, j) => { if (key) { const e = layEgg(key, true); if (e) e.t = e.tmax = 4 + j * 1.5; } });
  G.cam.S.x = G.nest.x - SCR_W / 2; G.cam.S.y = G.nest.y - VIEW_H / 2;
  G.cam.U.x = 0; G.cam.U.y = 0;
  G.cur = { S: { x: G.nest.x, y: G.nest.y }, U: { x: uCx(G.queenTile), y: uCy(G.queenTile) } };
  // enemy guards
  for (const b of G.bases) {
    const B = BASES[b.kind];
    let gl = B.guard.slice();
    if (G.boosts.has("guard1") && gl.length > 1) gl.shift();
    b.glist = gl;
    for (const m of gl) spawnMob(m, b, "guard");
  }
  for (const w of G.LV.wander) {
    const a = R() * Math.PI * 2;
    const m = spawnMob(w, null, "wander");
    m.x = clamp(G.nest.x + Math.cos(a) * 170, 20, SURF_W * T - 20); m.y = clamp(G.nest.y + Math.sin(a) * 220, 20, SURF_H * T - 20);
  }
}

function carveRoom(near, n) {
  // dig a small room for n tiles beside a spot, joined by a tunnel to the queen room row
  const out = [];
  const y = uY(near);
  let x = uX(near);
  for (let k = 0; k < n; k++) {
    const tx = clamp(x + (k % 4), 1, UND_W - 2), ty = y + Math.floor(k / 4);
    const i = uIdx(tx, ty); setEmpty(i); out.push(i);
  }
  for (let tx = Math.min(15, x); tx <= Math.max(15, x); tx++) setEmpty(uIdx(tx, 5));
  for (let ty = 5; ty <= y; ty++) setEmpty(uIdx(x, ty));
  G.uver++;
  return out.filter((i) => !G.ub[i] && i !== uIdx(x, y - 1));
}
function freeTileNear(i0, r, kind) {
  // an empty tile close to i0 with no building next to it of another kind (and digs it if needed)
  const x0 = uX(i0), y0 = uY(i0);
  for (let d = 1; d <= 6; d++) for (let y = y0; y <= y0 + d + 2; y++) for (let x = x0 - d - 2; x <= x0 + d + 2; x++) {
    if (x < 1 || y < 1 || x >= UND_W - 1 || y >= UND_H - 1) continue;
    const i = uIdx(x, y);
    if (G.ut[i] !== U_EMPTY || G.ub[i] || i === G.ent) continue;
    if (!canPlace(kind || "fs", i)) continue;
    // keep the main tunnel free: only tiles with a free neighbour
    return i;
  }
  return -1;
}

// ---------- buildings ----------
function addBuilding(kind, tiles) {
  const B = BUILDS[kind];
  let cost = B.cost;
  if (kind === "qs") cost = Math.round(G.Q.qsCost * (1 + QUEEN_RB[G.qcard.rar].qs));
  const b = { id: NEXT_ID++, kind, tiles, need: cost, got: 0, coming: 0, done: false };
  G.builds.push(b);
  for (const i of tiles) { G.ub[i] = BCODE[kind]; G.ubid[i] = b.id; G.um[i] = 0; }
  return b;
}
function buildingAt(i) { if (!G.ub[i]) return null; const id = G.ubid[i]; return G.builds.find((b) => b.id === id) || null; }
function removeBuilding(b) {
  for (const i of b.tiles) {
    if (G.sf[i]) { G.pile.food += G.sf[i]; G.sf[i] = 0; }
    if (G.sc[i]) { G.pile.chitin += G.sc[i]; G.sc[i] = 0; }
    for (const e of G.eggs) if (e.tile === i) e.tile = -1;
    G.ub[i] = 0; G.ubid[i] = 0;
  }
  G.builds.splice(G.builds.indexOf(b), 1);
  for (const e of G.eggs) if (e.tile === -1) { const t = eggSpot(); e.tile = t >= 0 ? t : G.queenTile; }
}
function canPlace(kind, i) {
  // on a dug tile, not the entrance, not on a building, at least one tile away from other building kinds
  const B = BUILDS[kind];
  const tiles = B.size === 2 ? [i, i + 1, i + UND_W, i + UND_W + 1] : [i];
  if (B.size === 2 && uX(i) >= UND_W - 1) return null;
  for (const t of tiles) {
    if (t < 0 || t >= UND_W * UND_H) return null;
    if (G.ut[t] !== U_EMPTY || G.ub[t] || t === G.ent || uY(t) < 1) return null;
    if (G.caves.some((c) => c.tiles.includes(t) && c.food.amt > 0)) return null;
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      const x = uX(t) + dx, y = uY(t) + dy; if (x < 0 || y < 0 || x >= UND_W || y >= UND_H) continue;
      const n = uIdx(x, y);
      if (G.ub[n] && BKIND[G.ub[n]] !== kind) return null;
    }
  }
  if (B.needs && !G.builds.some((b) => b.kind === B.needs && b.done)) return null;
  // must not cut the queen off from the entrance once built? buildings are walkable, so fine.
  return tiles;
}
const doneOf = (kind) => G.builds.filter((b) => b.kind === kind && b.done);
function qsTiles() { let n = 0; for (const b of G.builds) if (b.kind === "qs" && b.done) n += b.tiles.length; return n; }
function queueSlots() { return Math.min(8, Math.floor(qsTiles() / 2)); }

// storage capacity of a tile
function foodCap(i) {
  const k = BKIND[G.ub[i]]; const b = k && buildingAt(i);
  if (!b || !b.done) return 0;
  if (k === "qs") return G.Q.qsFood;
  if (k === "fs") return 8;
  return 0;
}
function chitinCap(i) { const b = buildingAt(i); return b && b.done && b.kind === "cs" ? 4 : 0; }
function storeFood(n) {   // put food into storage at once (used at start); returns what did not fit
  for (const b of G.builds) if (b.done) for (const i of b.tiles) { const c = foodCap(i) - G.sf[i]; if (c > 0 && n > 0) { const k = Math.min(c, n); G.sf[i] += k; n -= k; } }
  return n;
}
function totalFood() { let n = G.pile.food; for (let i = 0; i < G.sf.length; i++) n += G.sf[i]; return n; }
function totalChitin() { let n = G.pile.chitin; for (let i = 0; i < G.sc.length; i++) n += G.sc[i]; return n; }
function foodSpace() { let n = 0; for (const b of G.builds) if (b.done) for (const i of b.tiles) n += Math.max(0, foodCap(i) - G.sf[i]); return n; }

// ---------- path finding underground (BFS on dug tiles) ----------
function walkable(i) { return G.ut[i] === U_EMPTY; }
function field(goal) {
  // steps from every tile to `goal` (a tile index, or an array of tiles). Cached until the tunnels change.
  const key = Array.isArray(goal) ? goal.join(",") : goal;
  let f = G.ufields.get(key);
  if (f && f.ver === G.uver) return f.d;
  const d = new Int16Array(UND_W * UND_H).fill(-1);
  const q = new Int32Array(UND_W * UND_H); let h = 0, t = 0;
  for (const g of Array.isArray(goal) ? goal : [goal]) { d[g] = 0; q[t++] = g; }
  while (h < t) {
    const i = q[h++], x = uX(i), y = uY(i), nd = d[i] + 1;
    if (x > 0 && d[i - 1] < 0 && walkable(i - 1)) { d[i - 1] = nd; q[t++] = i - 1; }
    if (x < UND_W - 1 && d[i + 1] < 0 && walkable(i + 1)) { d[i + 1] = nd; q[t++] = i + 1; }
    if (y > 0 && d[i - UND_W] < 0 && walkable(i - UND_W)) { d[i - UND_W] = nd; q[t++] = i - UND_W; }
    if (y < UND_H - 1 && d[i + UND_W] < 0 && walkable(i + UND_W)) { d[i + UND_W] = nd; q[t++] = i + UND_W; }
  }
  if (G.ufields.size > 300) G.ufields.clear();
  G.ufields.set(key, { ver: G.uver, d });
  return d;
}
function nextStep(from, d) {
  // the neighbour tile closer to the goal
  const x = uX(from), y = uY(from);
  let best = -1, bd = d[from] < 0 ? 1e9 : d[from];
  const tryN = (n) => { if (d[n] >= 0 && d[n] < bd) { bd = d[n]; best = n; } };
  if (x > 0) tryN(from - 1);
  if (x < UND_W - 1) tryN(from + 1);
  if (y > 0) tryN(from - UND_W);
  if (y < UND_H - 1) tryN(from + UND_W);
  return best;
}
function reachable(i) { return field(G.ent)[i] >= 0; }
// a dug, reachable tile next to a solid tile (where a builder stands to dig it)
function digStand(i) {
  const d = field(G.ent);
  const x = uX(i), y = uY(i);
  const ns = [];
  if (x > 0) ns.push(i - 1);
  if (x < UND_W - 1) ns.push(i + 1);
  if (y > 0) ns.push(i - UND_W);
  if (y < UND_H - 1) ns.push(i + UND_W);
  let best = -1;
  for (const n of ns) if (d[n] >= 0 && (best < 0 || d[n] < d[best])) best = n;
  return best;
}
function digTile(i) {
  G.ut[i] = U_EMPTY; G.uhp[i] = 0; G.um[i] = 0; G.seen[i] = 1; G.uver++; G.st.dug++;
  // opened a cave?
  for (const c of G.caves) if (!c.food.known && c.tiles.some((t) => near4(t, i) || t === i)) {
    c.food.known = true; for (const t of c.tiles) G.seen[t] = 1;
    toast("Mushroom cave found!", "#c9b8ff");
  }
  markUndRedraw(i);
}
function near4(a, b) { const dx = Math.abs(uX(a) - uX(b)), dy = Math.abs(uY(a) - uY(b)); return dx + dy === 1; }
function fillTile(i) {
  G.ut[i] = U_SOFT; G.uhp[i] = 8; G.um[i] = 0; G.uver++;
  markUndRedraw(i);
}
// filling must not cut the queen off from the entrance (and never the entrance itself)
function canFill(i) {
  if (G.ut[i] !== U_EMPTY || G.ub[i] || i === G.ent) return false;
  if (G.caves.some((c) => c.tiles.includes(i))) return false;
  for (const a of G.ants) if (a.layer === "U" && a.alive && a.tile === i && a === G.queen) return false;
  if (i === G.queenTile) return false;
  // test: with this tile closed, can the queen still reach the entrance? (own search, keeps the path cache)
  return connectedWithout(i, G.queenTile, G.ent);
}
function connectedWithout(block, from, to) {
  const seen = new Uint8Array(UND_W * UND_H), q = new Int32Array(UND_W * UND_H);
  let h = 0, t = 0; q[t++] = from; seen[from] = 1; seen[block] = 1;
  while (h < t) {
    const i = q[h++]; if (i === to) return true;
    const x = uX(i), y = uY(i);
    const go = (n) => { if (!seen[n] && walkable(n)) { seen[n] = 1; q[t++] = n; } };
    if (x > 0) go(i - 1); if (x < UND_W - 1) go(i + 1); if (y > 0) go(i - UND_W); if (y < UND_H - 1) go(i + UND_W);
  }
  return false;
}
let undDirty = [];
function markUndRedraw(i) { undDirty.push(i); }

// ---------- toasts / alerts ----------
function toast(txt, col) { if (!G) return; G.toasts.push({ txt, col: col || "#f4e9df", t: 3.2 }); if (G.toasts.length > 4) G.toasts.shift(); }
function alertAt(x, y, layer) { if (!G) return; G.alerts.push({ x, y, layer, t: 4 }); if (G.alerts.length > 6) G.alerts.shift(); }
