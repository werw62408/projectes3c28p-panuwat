"use strict";
// ===== Outside the game: save, home map, deck, forge, perks, shop, almanac, queen path, results =====

const SAVE_KEY = "antraid.v1";
let META = null;
const APP = { mode: "home", msg: null };

function freshMeta() {
  const cards = {};
  for (const k of Object.keys(ANTS)) cards[k] = [0, 0, 0, 0];
  cards.uqueen[1] = 1;
  for (const k of ["builder", "loader", "adventurer", "seeker", "attacker", "defender", "bodyguard"]) cards[k][0] = 1;
  return {
    v: 1, sugar: 20, cards,
    deck: [{ key: "uqueen", rar: 1 }, { key: "builder", rar: 0 }, { key: "loader", rar: 0 }, { key: "seeker", rar: 0 }, { key: "attacker", rar: 0 }, { key: "bodyguard", rar: 0 }],
    perksOn: [], perksGot: [], stars: {}, open: { 0: 0 }, qp: [], loc: 0, lvl: 0,
    stats: { wins: 0, games: 0, kills: 0, workerKills: 0, scouts: 0, loaders: 0, queenHits: 0, bestFood: 0, bestRaid: 0, bestEggs: 0, bestDig: 0, bestEggsMade: 0 },
    alm: { ant: ["uqueen", "builder", "loader"], mob: [] }, daily: { day: "", card: false, sugar: false },
  };
}
function loadMeta() {
  let m = null;
  try { const s = localStorage.getItem(SAVE_KEY); if (s) m = JSON.parse(s); } catch (_) { m = null; }
  const f = freshMeta();
  if (!m || m.v !== 1) m = f;
  // fill anything missing (older saves, new cards)
  for (const k of Object.keys(f)) if (m[k] === undefined) m[k] = f[k];
  for (const k of Object.keys(ANTS)) if (!m.cards[k]) m.cards[k] = [0, 0, 0, 0];
  for (const k of Object.keys(f.stats)) if (m.stats[k] === undefined) m.stats[k] = 0;
  m.deck = m.deck.filter((c) => ANTS[c.key] && (m.cards[c.key][c.rar] || 0) > 0);
  if (!m.deck.some((c) => casteOf(c.key) === "queen")) m.deck = f.deck.filter((c) => (m.cards[c.key][c.rar] || 0) > 0);
  if (/[?&]all\b/.test(location.search)) unlockAll(m);
  META = m;
}
function saveMeta() { try { localStorage.setItem(SAVE_KEY, JSON.stringify(META)); } catch (_) {} }
function unlockAll(m) {
  for (const k of Object.keys(ANTS)) m.cards[k] = [3, 3, 1, 1];
  m.sugar = Math.max(m.sugar, 500);
  for (const P of PERKS) if (!m.perksGot.includes(P.id)) m.perksGot.push(P.id);
  LOCS.forEach((L, i) => { m.open[i] = L.levels.length - 1; });
}
const totalStars = () => Object.values(META.stars).reduce((n, a) => n + a.filter(Boolean).length, 0);
function bestRar(key) { const c = META.cards[key]; for (let r = 3; r >= 0; r--) if (c[r] > 0) return r; return 0; }
function deckSlots(deck) { const q = deck.find((c) => casteOf(c.key) === "queen"); return 5 + (q ? QUEEN_RB[q.rar].slots : 0); }
function deckProblem(deck) {
  if (deck.length > deckSlots(deck)) return "Too many cards for this queen";
  for (const c of NEED_CASTES) if (!deck.some((d) => ANTS[d.key].castes.includes(c))) return "Needs a " + CASTES[c].name + " card";
  if (deck.filter((d) => casteOf(d.key) === "queen").length > 1) return "Only one queen";
  return null;
}
function seeAlmanac(kind, key) { const a = META.alm[kind]; if (!a.includes(key)) { a.push(key); saveMeta(); } }
function today() { const d = new Date(); return d.getFullYear() + "-" + (d.getMonth() + 1) + "-" + d.getDate(); }
function perkUnlocked(P) { return META.perksGot.includes(P.id) || (P.need && (META.stats[P.need[0]] || 0) >= P.need[1]); }

// ---------- home (the location map) ----------
function openHome() {
  APP.mode = "home";
  G = null; bgS = null; bgU = null; fogC = null;
  UI.clear();
  UI.push(livePanel(() => {
    const L = LOCS[META.loc], open = META.open[META.loc];
    const lvlOpen = open != null;
    const st = META.stars[META.loc + "-" + META.lvl] || [];
    return {
      full: true, back: false, bg: "#140f0b",
      header: { h: 116, draw: (x, y, w) => drawHomeHeader(x, y, w, L, st, lvlOpen && META.lvl <= open) },
      items: [
        { t: "PLAY", col: lvlOpen && META.lvl <= open ? GREEN : "#6a5a4a", r: "", dis: !(lvlOpen && META.lvl <= open), why: "Win the level before first", act: () => startFlow() },
        { t: "Location", r: "< " + L.name + " >", adj: (d) => { META.loc = (META.loc + d + LOCS.length) % LOCS.length; META.lvl = Math.min(META.lvl, (META.open[META.loc] ?? 0)); if (META.open[META.loc] == null) META.lvl = 0; saveMeta(); } },
        { t: "Level", r: "< " + (META.lvl + 1) + " >", adj: (d) => { META.lvl = clamp(META.lvl + d, 0, L.levels.length - 1); saveMeta(); } },
        { t: "Deck", r: META.deck.length + "/" + deckSlots(META.deck), act: () => openDeck() },
        { t: "Forge (merge cards)", act: () => openForge() },
        { t: "Perks", r: META.perksOn.length + "/3 on", act: () => openPerks() },
        { t: "Shop", r: META.daily.day !== today() || !META.daily.card || !META.daily.sugar ? "FREE!" : "", rcol: GREEN, act: () => openShop() },
        { t: "Queen path", r: totalStars() + " stars", act: () => openQPath() },
        { t: "Almanac", act: () => openAlmanac() },
        { t: "How to play", act: () => openHelp() },
      ],
      foot: "Stick: choose  A: ok  left/right: change",
    };
  }));
}
function drawHomeHeader(x, y, w, L, st, open) {
  // title, sugar, a small map card of the location like the original's level panel
  txt("ANT RAID", x + 2, y + 2, GOLD, 16, "left", true);
  icon("sugar", x + w - 40, y + 6); txt(String(META.sugar), x + w - 2, y + 6, INK, 8, "right");
  const cy = y + 24, ch = 88;
  const g = ctx.createLinearGradient(x, cy, x, cy + ch);
  g.addColorStop(0, shade(L.ground[1], 0.75)); g.addColorStop(1, shade(L.ground[2], 0.45));
  box(x, cy, w, ch, null); ctx.fillStyle = g; ctx.fillRect(x, cy, w, ch); box(x, cy, w, ch, null, open ? GOLD : LINE);
  txt(L.name, x + 6, cy + 4, "#fff5e6", 8, "left", true);
  txt("LEVEL " + (META.lvl + 1) + (open ? "" : "  (locked)"), x + 6, cy + 14, open ? "#fff5e6" : RED, 16, "left", true);
  for (let k = 0; k < 3; k++) icon(st[k] ? "star" : "star0", x + w - 34 + k * 10, cy + 5);
  const LV = L.levels[META.lvl];
  txt("TARGETS", x + 6, cy + 34, "#fff5e6");
  LV.bases.forEach((b, k) => { box(x + 54 + k * 12, cy + 33, 9, 9, BASES[b].col, "#120b07"); });
  LV.wander.forEach((m, k) => { box(x + 54 + (LV.bases.length + k) * 12, cy + 33, 9, 9, MOBS[m].col, "#ffffff"); });
  txt("QUESTS", x + 6, cy + 46, "#fff5e6");
  const qs = ["win", ...LV.q];
  qs.forEach((q, k) => { icon(st[k] ? "star" : "star0", x + 6, cy + 56 + k * 10); txt(fitTxt(QUESTS[q].txt, w - 22), x + 17, cy + 56 + k * 10, st[k] ? "#fff5e6" : "#d8c8b0"); });
  if (APP.msg && performance.now() < APP.msg.until) { box(x, cy + ch - 12, w, 12, "rgba(90,30,20,0.95)"); txt(fitTxt(APP.msg.t, w - 6), x + w / 2, cy + ch - 10, INK, 8, "center"); }
}

// ---------- start: deck check -> boosts -> game ----------
function startFlow() {
  const why = deckProblem(META.deck);
  // the deck screen like the original: YOUR DECK, Change / Play now
  UI.push(livePanel(() => {
    const slots = deckSlots(META.deck);
    const items = [];
    for (let k = 0; k < Math.max(slots, META.deck.length); k++) {
      const c = META.deck[k];
      items.push({ t: c ? ANTS[c.key].short : "", drawCell: (x, y, w, h, sel) => c ? drawCard(x, y, w, h, c.key, c.rar, sel) : box(x, y, w, h, "#2a201a", sel ? GOLD : LINE), act: () => openDeck() });
    }
    const cols = 4;
    while (items.length % cols) items.push({ t: "", drawCell: (x, y, w, h) => box(x, y, w, h, "#1a1410", LINE), act: () => openDeck() });
    items.push({ t: "CHANGE", drawCell: (x, y, w, h, sel) => btnCell(x, y, w * 2 + 4, h, "CHANGE", sel, PANEL2), act: () => openDeck() });
    items.push({ t: "", skip: true, drawCell: () => {} });
    items.push({ t: "PLAY NOW", drawCell: (x, y, w, h, sel) => btnCell(x, y, w * 2 + 4, h, "PLAY NOW", sel, "#3d6a2a"), act: () => { const p = deckProblem(META.deck); if (p) { APP.msg = { t: p, until: performance.now() + 2500 }; toastUI(p); return; } UI.pop(); pickBoosts(); } });
    items.push({ t: "", skip: true, drawCell: () => {} });
    return { kind: "grid", cols, cellH: 62, title: "YOUR DECK", titleR: META.deck.length + "/" + slots, items, foot: deckProblem(META.deck) || "A: play  D: back" };
  }, { sel: Math.ceil(Math.max(deckSlots(META.deck), META.deck.length) / 4) * 4 + 2 }));
  if (why) APP.msg = { t: why, until: performance.now() + 2500 };
}
function btnCell(x, y, w, h, label, sel, col) { const hh = 22; const yy = y + 4; box(x, yy, w, hh, col, sel ? GOLD : LINE); txt(label, x + w / 2, yy + 7, sel ? GOLD : INK, 8, "center", true); }
function pickBoosts() {
  const chosen = [];
  let refresh = 1;
  const roll = () => { const pool = BOOSTS.filter((b) => !chosen.includes(b.id)); const out = []; while (out.length < 3 && pool.length) out.push(pool.splice(Math.floor(Math.random() * pool.length), 1)[0]); return out; };
  let cards = roll();
  const p = livePanel(() => ({
    kind: "grid", cols: 3, cellH: 128, full: true, back: () => { UI.pop(); }, title: "CHOOSE YOUR BOOSTS (" + (chosen.length + 1) + "/2)!",
    header: { h: 10, draw: (x, y, w) => txt("BUT FIRST...!", x + w / 2, y, DIM, 8, "center") },
    items: [
      ...cards.map((b) => ({ t: b.name, drawCell: (x, y, w, h, sel) => drawBoostCard(x, y, w, h, b, sel), act: () => { chosen.push(b.id); if (chosen.length >= 2) { UI.pop(); beginGame(chosen); } else { cards = roll(); } } })),
      { t: "REFRESH", dis: refresh <= 0, why: "No refresh left", drawCell: (x, y, w, h, sel) => btnCell(x + w / 2 + 2, y + 6, w * 2 - w + 0, 30, refresh > 0 ? "REFRESH" : "NO REFRESH", sel, "#3a3f4a"), act: () => { if (refresh > 0) { refresh--; cards = roll(); } } },
    ],
    foot: "Pick 2. One refresh.",
  }));
  UI.push(p);
}
function beginGame(boosts) {
  UI.clear();
  const seed = (Date.now() ^ (Math.random() * 1e9)) >>> 0;
  newRun({ loc: META.loc, lvl: META.lvl, deck: META.deck.slice(), perks: META.perksOn.slice(), boosts, seed });
  initJobs();
  bgS = null; bgU = null; fogC = null; fogVerDrawn = -1;
  G.zoomV = { S: 2, U: 1 };
  centerOn("S", G.nest.x, G.nest.y);
  centerOn("U", uCx(G.queenTile), uCy(G.queenTile));
  APP.mode = "game";
  toast("Find and destroy " + G.bases.length + " enemy bases!", GOLD);
}

// ---------- deck editor ----------
function openDeck() {
  UI.push(livePanel(() => {
    const items = [];
    const slots = deckSlots(META.deck);
    items.push({ t: "IN THE DECK " + META.deck.length + "/" + slots + "  (A: take out / change)", skip: true, col: GOLD });
    META.deck.forEach((c, k) => items.push({ t: ANTS[c.key].name, r: RAR[c.rar], rcol: RAR_COL[c.rar], sub: ANTS[c.key].castes.map((x) => CASTES[x].name).join(" / "), icon: (x, y) => casteIcon(casteOf(c.key), x, y),
      act: () => cardMenu(c.key, k) }));
    items.push({ t: "", skip: true, line: true, h: 6 });
    items.push({ t: "YOUR CARDS (A: put in)", skip: true, col: GOLD });
    for (const key of Object.keys(ANTS)) {
      const own = META.cards[key]; if (!own.some((n) => n > 0)) continue;
      const inDeck = META.deck.find((c) => c.key === key);
      items.push({ t: ANTS[key].name + (inDeck ? "  (in deck)" : ""), r: own.map((n, r) => (n ? RAR[r][0] + n : "")).filter(Boolean).join(" "), sub: (ANTS[key].skills || []).join(". "), icon: (x, y) => casteIcon(casteOf(key), x, y), col: inDeck ? DIM : INK,
        act: () => cardMenu(key, inDeck ? META.deck.indexOf(inDeck) : -1) });
    }
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: "DECK", titleR: deckProblem(META.deck) ? "!" : "OK", items, foot: deckProblem(META.deck) || "Needs Queen, Builder, Loader, Scout, Warrior" };
  }));
}
function cardMenu(key, deckIdx) {
  const own = META.cards[key];
  const A = ANTS[key];
  UI.push(livePanel(() => {
    const items = [];
    const s = A.hp === undefined ? null : A;
    items.push({ t: A.castes.map((x) => CASTES[x].name).join(" / "), skip: true, col: CASTES[A.castes[0]].col });
    if (casteOf(key) !== "queen") items.push({ t: "HP " + lvlv(A.hp, 1) + "  DMG " + lvlv(A.dmg, 1) + "  SPD " + lvlv(A.spd, 1) + "  CARRY " + lvlv(A.str, 1), skip: true }, { t: "PRICE " + lvlv(A.price, 1) + " food   HATCH " + Math.round(A.hatch * HATCH_K) + " s", skip: true });
    else items.push({ t: "Slots: 5 + rarity (Rare 6, Epic 7, Legend 8)", skip: true });
    for (const t of A.skills || []) items.push({ t: "- " + t, skip: true, col: DIM });
    for (let r = 0; r < 4; r++) if (own[r] > 0) {
      const inDeck = deckIdx >= 0 && META.deck[deckIdx].rar === r;
      items.push({ t: (inDeck ? "In deck: " : "Use ") + RAR[r] + " (x" + own[r] + ")", col: RAR_COL[r], dis: inDeck, why: "Already in the deck",
        act: () => {
          if (deckIdx >= 0) META.deck[deckIdx] = { key, rar: r };
          else if (casteOf(key) === "queen") { const qi = META.deck.findIndex((c) => casteOf(c.key) === "queen"); if (qi >= 0) META.deck[qi] = { key, rar: r }; else META.deck.unshift({ key, rar: r }); }
          else { if (META.deck.length >= deckSlots(META.deck)) { toastUI("Deck full: take a card out first"); return; } META.deck.push({ key, rar: r }); }
          saveMeta(); UI.pop();
        } });
    }
    if (deckIdx >= 0 && casteOf(key) !== "queen") items.push({ t: "Take out of the deck", col: RED, act: () => { META.deck.splice(deckIdx, 1); saveMeta(); UI.pop(); } });
    items.push({ t: "Back", act: () => UI.pop() });
    return { small: false, title: A.name, items };
  }));
}

// ---------- forge ----------
const FORGE = [[3, 30], [6, 60], [9, 90]];   // cards needed, sugar
function openForge() {
  UI.push(livePanel(() => {
    const items = [{ t: "3 same cards + sugar = next rarity", skip: true, col: DIM }];
    for (const key of Object.keys(ANTS)) {
      const own = META.cards[key];
      for (let r = 0; r < 3; r++) {
        if (!own[r]) continue;
        const [need, sugar] = FORGE[r];
        const ok = own[r] >= need && META.sugar >= sugar;
        items.push({ t: ANTS[key].short + " " + RAR[r] + " -> " + RAR[r + 1], r: own[r] + "/" + need + "  " + sugar + "s", rcol: ok ? GREEN : DIM, icon: (x, y) => box(x, y, 7, 7, RAR_COL[r + 1]), dis: !ok, why: own[r] < need ? "Need " + need + " cards" : "Need " + sugar + " sugar",
          act: () => { own[r] -= need; own[r + 1]++; META.sugar -= sugar; seeAlmanac("ant", key); saveMeta(); toastUI(ANTS[key].name + " is now " + RAR[r + 1] + "!"); } });
      }
    }
    if (items.length === 1) items.push({ t: "No cards to merge yet", skip: true });
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: "FORGE", titleR: META.sugar + " sugar", items, foot: "Rare 3 cards/30, Epic 6/60, Legend 9/90" };
  }));
}

// ---------- perks ----------
function openPerks() {
  UI.push(livePanel(() => ({ title: "PERKS", titleR: META.perksOn.length + "/3", items: [
    ...PERKS.map((P) => {
      const un = perkUnlocked(P), on = META.perksOn.includes(P.id);
      return { t: P.name, r: on ? "ON" : un ? "" : "LOCKED", rcol: on ? GREEN : RED, sub: un ? P.desc : (P.qp ? "Queen path reward" : P.needTxt + " (" + (META.stats[P.need[0]] || 0) + "/" + P.need[1] + ")"), col: un ? INK : "#6a5a4a",
        dis: !un, why: "Locked", act: () => { if (on) META.perksOn = META.perksOn.filter((x) => x !== P.id); else if (META.perksOn.length < 3) META.perksOn.push(P.id); else { toastUI("Only 3 perks at once"); return; } saveMeta(); } };
    }),
    { t: "Back", act: () => UI.pop() },
  ], foot: "Up to 3 perks. They change the next game" })));
}

// ---------- shop (free daily things, no money) ----------
function openShop() {
  if (META.daily.day !== today()) META.daily = { day: today(), card: false, sugar: false };
  UI.push(livePanel(() => ({ title: "SHOP", titleR: META.sugar + " sugar", items: [
    { t: "Free daily card", r: META.daily.card ? "taken" : "FREE", rcol: META.daily.card ? DIM : GREEN, sub: "A random common card", dis: META.daily.card, why: "Come back tomorrow",
      act: () => { const pool = Object.keys(ANTS).filter((k) => casteOf(k) !== "queen"); const k = pool[Math.floor(Math.random() * pool.length)]; META.cards[k][0]++; META.daily.card = true; seeAlmanac("ant", k); saveMeta(); toastUI("Got " + ANTS[k].name + "!"); } },
    { t: "Free daily sugar", r: META.daily.sugar ? "taken" : "+15", rcol: META.daily.sugar ? DIM : GREEN, sub: "Sugar is for the forge", dis: META.daily.sugar, why: "Come back tomorrow",
      act: () => { META.sugar += 15; META.daily.sugar = true; saveMeta(); toastUI("+15 sugar"); } },
    { t: "Rare card pack", r: "60 sugar", sub: "One random Rare card", dis: META.sugar < 60, why: "Need 60 sugar",
      act: () => { const pool = Object.keys(ANTS).filter((k) => casteOf(k) !== "queen"); const k = pool[Math.floor(Math.random() * pool.length)]; META.cards[k][1]++; META.sugar -= 60; seeAlmanac("ant", k); saveMeta(); toastUI("Got Rare " + ANTS[k].name + "!"); } },
    { t: "Back", act: () => UI.pop() },
  ] })));
}

// ---------- queen path ----------
function openQPath() {
  UI.push(livePanel(() => {
    const st = totalStars();
    return { title: "QUEEN PATH", titleR: st + " stars", items: [
      ...QPATH.map((q, k) => {
        const got = META.qp.includes(k), can = st >= q.stars && !got;
        return { t: q.stars + " stars: " + q.txt, r: got ? "OK" : can ? "TAKE" : "", rcol: got ? DIM : GREEN, icon: (x, y) => icon(st >= q.stars ? "star" : "star0", x, y), dis: !can, why: got ? "Already taken" : "Need " + q.stars + " stars",
          act: () => { claimQP(q.give); META.qp.push(k); saveMeta(); toastUI("Got: " + q.txt); } };
      }),
      { t: "Back", act: () => UI.pop() },
    ], foot: "Stars come from level quests" };
  }));
}
function claimQP(g) {
  if (g.sugar) META.sugar += g.sugar;
  if (g.card) META.cards[g.card[0]][g.card[1]]++;
  if (g.cards) for (const [k, r] of g.cards) META.cards[k][r]++;
  if (g.perk) META.perksGot.push(g.perk);
  if (g.perks) META.perksGot.push(...g.perks);
}

// ---------- almanac ----------
function openAlmanac() {
  UI.push(livePanel(() => {
    const items = [{ t: "ANTS", skip: true, col: GOLD }];
    for (const k of Object.keys(ANTS)) {
      const seen = META.alm.ant.includes(k);
      const A = ANTS[k];
      items.push({ t: seen ? A.name : "???", icon: (x, y) => casteIcon(seen ? casteOf(k) : "builder", x, y), sub: seen ? (casteOf(k) === "queen" ? A.skills.join(". ") : "HP " + lvlv(A.hp, 1) + "-" + lvlv(A.hp, 5) + "  DMG " + lvlv(A.dmg, 1) + "-" + lvlv(A.dmg, 5) + "  " + A.skills[0]) : "Not seen yet", col: seen ? INK : "#6a5a4a" });
    }
    items.push({ t: "ENEMIES", skip: true, col: GOLD });
    for (const k of Object.keys(MOBS)) {
      const seen = META.alm.mob.includes(k), M = MOBS[k];
      items.push({ t: seen ? M.name : "???", icon: (x, y) => box(x, y, 7, 7, seen ? M.col : "#3a2a1e"), sub: seen ? "HP " + M.hp + "  DMG " + M.dmg + "  food " + M.food + (M.thief ? "  THIEF" : "") + (M.boss ? "  BOSS" : "") : "Kill one to learn about it", col: seen ? INK : "#6a5a4a" });
    }
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: "ALMANAC", items, sel: 1 };
  }));
}

// ---------- end of a run ----------
function onRunOver() {
  const won = G.over === "win";
  const key = G.loc + "-" + G.lvl;
  const st = META.stars[key] || [false, false, false];
  const qs = ["win", ...G.LV.q];
  const done = qs.map((q) => won && questDone(q));
  const newStars = done.map((d, k) => d && !st[k]);
  META.stars[key] = st.map((s, k) => s || done[k]);
  // rewards: sugar from bases and the map is kept even when you lose
  META.sugar += G.sugar;
  for (const [k, r] of G.cardsWon) { META.cards[k][r]++; seeAlmanac("ant", k); }
  const S = META.stats, s = G.st;
  S.games++; if (won) S.wins++;
  S.kills += s.kills; S.workerKills += s.workerKills; S.scouts += s.scouts; S.loaders += s.loaders; S.queenHits += s.queenHits;
  S.bestFood = Math.max(S.bestFood, s.food); S.bestRaid = Math.max(S.bestRaid, s.raidMax); S.bestEggs = Math.max(S.bestEggs, s.maxEggs); S.bestDig = Math.max(S.bestDig, s.dug); S.bestEggsMade = Math.max(S.bestEggsMade, s.eggsMade);
  const before = new Set(PERKS.filter((P) => META.perksGot.includes(P.id)).map((P) => P.id));
  const newPerks = PERKS.filter((P) => P.need && (S[P.need[0]] || 0) >= P.need[1] && !before.has(P.id));
  for (const P of newPerks) META.perksGot.push(P.id);
  if (won) { const L = LOCS[G.loc]; if (G.lvl + 1 < L.levels.length) META.open[G.loc] = Math.max(META.open[G.loc] ?? 0, G.lvl + 1); else if (G.loc + 1 < LOCS.length && META.open[G.loc + 1] == null) META.open[G.loc + 1] = 0; }
  saveMeta();
  const lines = [
    { t: won ? "All bases destroyed!" : G.overWhy || "The colony fell", skip: true, col: won ? GREEN : RED },
    { t: "Time " + fmtTime(G.t) + "   Ants (most) " + s.maxAnts + "   Kills " + s.kills, skip: true },
    { t: "Food " + s.food + "   Tiles dug " + s.dug + "   Eggs " + s.eggsMade, skip: true },
    { t: "Sugar +" + G.sugar + "  (map " + G.sugarFound + "/" + G.sugarTotal + ")", skip: true, col: INK },
    ...qs.map((q, k) => ({ t: QUESTS[q].txt, icon: (x, y) => icon(META.stars[key][k] ? "star" : "star0", x, y), r: newStars[k] ? "NEW!" : "", rcol: GOLD, skip: true })),
    ...G.cardsWon.map(([k, r]) => ({ t: "Card: " + ANTS[k].name, r: RAR[r], rcol: RAR_COL[r], skip: true })),
    ...newPerks.map((P) => ({ t: "Perk unlocked: " + P.name, skip: true, col: BLUE })),
    { t: "CONTINUE", col: GOLD, act: () => openHome() },
  ];
  UI.clear();
  UI.push({ title: won ? "VICTORY" : "DEFEAT", titleCol: won ? GREEN : RED, items: lines, back: false, sel: lines.length - 1 });
}
function questDone(q) {
  const s = G.st;
  switch (q) {
    case "win": return true;
    case "build_ws": return s.wsBuilt;
    case "ants30": return s.maxAnts >= 30;
    case "ants40": return s.maxAnts >= 40;
    case "ants50": return s.maxAnts >= 50;
    case "time25": return G.t < 25 * 60;
    case "time30": return G.t < 30 * 60;
    case "sugar10": return G.sugarFound >= 10;
    case "lvl3": return s.maxLvl >= 3;
    case "noqueen": return !s.queenHit;
    case "queen2": return s.queenMoved;
  }
  return false;
}
