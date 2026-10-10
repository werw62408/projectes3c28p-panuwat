"use strict";
// ===== UI: menu panels, input (joystick + 7 buttons, keyboard, touch), in-game menus =====

const UI = {
  stack: [], pick: null, side: true, held: { up: 0, down: 0, left: 0, right: 0 }, rep: 0, curV: 0,
  top() { return this.stack[this.stack.length - 1] || null; },
  push(p) { p.sel = p.sel || 0; p.scroll = 0; fixSel(p, 1); this.stack.push(p); return p; },
  pop() { const p = this.stack.pop(); if (p && p.onClose) p.onClose(); return p; },
  clear() { while (this.stack.length) this.pop(); },
  replace(p) { this.stack.pop(); return this.push(p); },
};
function fixSel(p, dir) {
  const n = p.items.length; if (!n) return;
  p.sel = clamp(p.sel, 0, n - 1);
  for (let k = 0; k < n && (p.items[p.sel].skip); k++) p.sel = (p.sel + (dir || 1) + n) % n;
}
function refreshPanel(p) { if (p && p.build) { const s = p.sel; const np = p.build(); p.items = np.items; if (np.title) p.title = np.title; if (np.foot !== undefined) p.foot = np.foot; p.sel = s; fixSel(p, 1); } }
// a panel that rebuilds its items from a function (so numbers stay fresh)
function livePanel(fn, extra) { const p = Object.assign({ build: fn }, fn(), extra || {}); return p; }

// ---------- panel geometry and drawing ----------
function panelRect(p) {
  if (p.full) return { x: 0, y: 0, w: SCR_W, h: SCR_H };
  if (p.small) { const h = Math.min(VIEW_H - 8, 20 + (p.header ? p.header.h : 0) + p.items.reduce((s, it) => s + rowH(it), 0) + (p.foot ? 12 : 0) + 4); return { x: 6, y: VIEW_Y + VIEW_H - h - 4, w: SCR_W - 12, h }; }
  return { x: 4, y: VIEW_Y + 2, w: SCR_W - 8, h: VIEW_H - 4 };
}
function rowH(it) { return it.h || (it.sub ? 22 : 14); }
function drawPanel(p) {
  const r = panelRect(p);
  if (!p.full) { ctx.fillStyle = "rgba(0,0,0,0.45)"; ctx.fillRect(0, 0, SCR_W, SCR_H); }
  box(r.x, r.y, r.w, r.h, p.bg || PANEL, p.full ? null : LINE);
  let y = r.y + 3;
  if (p.title) { txt(fitTxt(p.title, r.w - 40), r.x + 6, y + 1, p.titleCol || GOLD, 8, "left", true); if (p.titleR) txt(p.titleR, r.x + r.w - 6, y + 1, DIM, 8, "right"); y += 14; box(r.x + 4, y - 2, r.w - 8, 1, LINE); }
  if (p.header) { p.header.draw(r.x + 4, y, r.w - 8); y += p.header.h; }
  const footH = p.foot ? 12 : 0;
  const top = y, bottom = r.y + r.h - 3 - footH;
  p._rows = [];
  if (p.kind === "grid") { drawGrid(p, r, top, bottom); }
  else {
    // scroll so the selected row is visible
    let yy = 0; const pos = p.items.map((it) => { const o = yy; yy += rowH(it); return o; });
    const total = yy, view = bottom - top;
    const sy = pos[p.sel] || 0, sh = p.items[p.sel] ? rowH(p.items[p.sel]) : 0;
    if (sy - p.scroll < 0) p.scroll = sy;
    if (sy + sh - p.scroll > view) p.scroll = sy + sh - view;
    p.scroll = clamp(p.scroll, 0, Math.max(0, total - view));
    ctx.save(); ctx.beginPath(); ctx.rect(r.x, top, r.w, view); ctx.clip();
    p.items.forEach((it, i) => {
      const iy = top + pos[i] - p.scroll, h = rowH(it);
      if (iy + h < top || iy > bottom) return;
      p._rows.push({ i, x: r.x + 3, y: iy, w: r.w - 6, h });
      drawRow(p, it, i === p.sel, r.x + 3, iy, r.w - 6, h);
    });
    ctx.restore();
    if (total > view) { const f = view / total; box(r.x + r.w - 3, top + (p.scroll / total) * view, 2, Math.max(6, view * f), "#6a5a4a"); }
  }
  if (p.foot) txt(fitTxt(p.foot, r.w - 10), r.x + 6, r.y + r.h - 12, DIM);
}
function drawRow(p, it, sel, x, y, w, h) {
  if (it.skip && !it.icon && !it.r && !it.sub) { if (it.t) txt(fitTxt(it.t, w - 6), x + 3, y + 3, it.col || DIM); if (it.line) box(x + 2, y + h / 2, w - 4, 1, LINE); return; }
  if (it.skip) sel = false;
  if (sel) box(x, y, w, h - 1, "#3a2a1e", GOLD);
  else if (it.bg) box(x, y, w, h - 1, it.bg);
  let tx = x + 4;
  if (it.icon) { it.icon(tx, y + (it.sub ? 3 : 3)); tx += 10; }
  const right = typeof it.r === "function" ? it.r() : it.r;
  const rw = right ? txtW(right) + 8 : 0;
  txt(fitTxt(it.t, w - (tx - x) - rw - 4), tx, y + 3, it.dis ? "#6a5a4a" : it.col || INK);
  if (right) txt(right, x + w - 4, y + 3, it.rcol || (it.adj ? GOLD : DIM), 8, "right");
  if (it.adj && sel) { txt("<", x + w - rw - 2, y + 3, GOLD, 8, "right"); }
  if (it.sub) txt(fitTxt(it.sub, w - (tx - x) - 4), tx, y + 12, it.subCol || DIM);
}
function drawGrid(p, r, top, bottom) {
  const cols = p.cols || 3, gap = 4;
  const cw = Math.floor((r.w - 8 - gap * (cols - 1)) / cols), ch = p.cellH || Math.round(cw * 1.4);
  const rowsVis = Math.max(1, Math.floor((bottom - top + gap) / (ch + gap)));
  const selRow = Math.floor(p.sel / cols);
  if (selRow - (p.scroll || 0) >= rowsVis) p.scroll = selRow - rowsVis + 1;
  if (selRow < (p.scroll || 0)) p.scroll = selRow;
  p.items.forEach((it, i) => {
    const row = Math.floor(i / cols) - (p.scroll || 0), col = i % cols;
    if (row < 0 || row >= rowsVis) return;
    const x = r.x + 4 + col * (cw + gap), y = top + row * (ch + gap);
    if (it.wide) { /* a full-width button row */ }
    p._rows.push({ i, x, y, w: cw, h: ch });
    if (it.drawCell) it.drawCell(x, y, cw, ch, i === p.sel);
    else { box(x, y, cw, ch, i === p.sel ? "#3a2a1e" : PANEL2, i === p.sel ? GOLD : LINE); txt(fitTxt(it.t, cw - 4), x + cw / 2, y + ch / 2 - 4, INK, 8, "center"); }
  });
}

// ---------- card art (drawn, not images) ----------
function drawCard(x, y, w, h, key, rar, sel, extra) {
  const A = ANTS[key];
  const col = RAR_COL[rar || 0];
  box(x, y, w, h, shade(col, 0.35), sel ? GOLD : col);
  if (sel) box(x + 1, y + 1, w - 2, h - 2, null, GOLD);
  // picture
  const px = x + 3, py = y + 3, pw = w - 6, ph = Math.round(h * 0.55);
  const c0 = A.castes[0];
  const grad = ctx.createLinearGradient(px, py, px, py + ph);
  grad.addColorStop(0, shade(CASTES[c0].col, 0.55)); grad.addColorStop(1, shade(CASTES[c0].col, 0.25));
  ctx.fillStyle = grad; ctx.fillRect(px, py, pw, ph);
  const s = Math.min(pw, ph) / 9 * Math.min(1.25, A.size * 0.75 + 0.35);
  drawBug(px + pw / 2, py + ph / 2 + 1, -Math.PI / 2, s, c0 === "queen" ? "#7a4a22" : "#2e1c14", false, 0, "ant", false);
  if (c0 === "queen") { ctx.fillStyle = GOLD; for (let k = -1; k <= 1; k++) ctx.fillRect(Math.round(px + pw / 2 + k * 3 - 1), Math.round(py + 3 - (k ? 0 : 1)), 2, 3); }
  // caste chips
  A.castes.forEach((c, k) => box(px + 2 + k * 7, py + 2, 5, 5, CASTES[c].col));
  txt(fitTxt(A.short, w - 4), x + w / 2, py + ph + 3, INK, 8, "center");
  if (h > 60) txt(RAR[rar || 0], x + w / 2, py + ph + 12, col, 8, "center");
  if (extra) txt(extra, x + w / 2, y + h - 10, GOLD, 8, "center");
}
function drawBoostCard(x, y, w, h, bst, sel) {
  box(x, y, w, h, "#1d6fb8", sel ? GOLD : "#0f4a80");
  if (sel) box(x + 1, y + 1, w - 2, h - 2, null, GOLD);
  box(x + 3, y + 3, w - 6, h - 6, null, "#3a8ad0");
  const cx = x + w / 2, cy = y + h * 0.33;
  const ic = bst.icon;
  if (ic === "food") { ctx.fillStyle = "#d8443a"; ctx.beginPath(); ctx.ellipse(cx, cy, 10, 7, 0, Math.PI, 0); ctx.fill(); box(cx - 3, cy, 6, 8, "#f3e2c8"); }
  else if (ic === "queen" || ic === "builder" || ic === "loader" || ic === "scout") { drawBug(cx, cy + 2, -Math.PI / 2, ic === "queen" ? 4 : 3.2, "#2e1c14", false, 0, "ant", false); if (ic === "queen") { ctx.fillStyle = GOLD; ctx.fillRect(cx - 4, cy - 14, 8, 3); } }
  else if (ic === "barracks") { box(cx - 11, cy - 8, 22, 16, "#8a5a3a"); box(cx - 6, cy - 4, 12, 12, "#b85440"); box(cx - 2, cy, 4, 8, "#3a1e14"); }
  else if (ic === "egg") { ctx.fillStyle = "#fff5e6"; ctx.beginPath(); ctx.ellipse(cx, cy, 6, 8, 0, 0, 6.283); ctx.fill(); }
  else if (ic === "speed") { ctx.fillStyle = INK; for (let k = 0; k < 2; k++) { ctx.beginPath(); ctx.moveTo(cx - 10 + k * 9, cy - 7); ctx.lineTo(cx - 1 + k * 9, cy); ctx.lineTo(cx - 10 + k * 9, cy + 7); ctx.fill(); } }
  else if (ic === "chitin") { box(cx - 8, cy - 5, 16, 10, "#9fd0ff"); box(cx - 6, cy - 7, 12, 2, "#cfe8ff"); }
  else if (ic === "sword") { box(cx - 1, cy - 10, 3, 16, "#d8d8e0"); box(cx - 5, cy + 4, 11, 2, "#8a5a3a"); }
  // green arrow like a plus
  box(x + w - 14, y + 6, 8, 3, "#7fd46b"); box(x + w - 11, y + 3, 2, 9, "#7fd46b");
  const words = bst.name.split(" "); const lines = []; let cur = "";
  for (const wd of words) { const t = cur ? cur + " " + wd : wd; if (txtW(t) > w - 8 && cur) { lines.push(cur); cur = wd; } else cur = t; }
  if (cur) lines.push(cur);
  lines.slice(0, 4).forEach((l, k) => txt(l, x + w / 2, y + h * 0.62 + k * 9, "#fff5e6", 8, "center"));
}

// ---------- input ----------
const DIRS = { up: [0, -1], down: [0, 1], left: [-1, 0], right: [1, 0] };
function press(btn) {
  if (typeof onAnyInput === "function") onAnyInput();
  const p = UI.top();
  if (p) return panelKey(p, btn);
  if (APP.mode !== "game" || !G) return;
  gameKey(btn);
}
function dirPress(d) {
  const p = UI.top();
  if (p) {
    const [dx, dy] = DIRS[d];
    if (p.kind === "grid") {
      const cols = p.cols || 3, n = p.items.length;
      let s = p.sel + dx + dy * cols;
      if (dy && (s < 0 || s >= n)) { if (p.onEdge && p.onEdge(dy)) return; s = clamp(s, 0, n - 1); }
      p.sel = clamp(s, 0, n - 1); fixSel(p, dx + dy || 1); return;
    }
    if (dy) { const n = p.items.length; let s = p.sel; for (let k = 0; k < n; k++) { s = (s + dy + n) % n; if (!p.items[s].skip) break; } p.sel = s; return; }
    const it = p.items[p.sel];
    if (it && it.adj) { it.adj(dx); refreshPanel(p); }
    else if (dx < 0 && p.leftBack !== false) { /* stick left = back, like the device */ }
    return;
  }
  if (APP.mode !== "game" || !G) return;
  if ((G.build || UI.pick) && G.view === "U") { const c = G.cur.U; c.x = clamp(c.x + DIRS[d][0] * T, 2, UND_W * T - 2); c.y = clamp(c.y + DIRS[d][1] * T, 2, UND_H * T - 2); followCursor(); if (G.build && G.build.paint != null) applyTool(true); }
}
function panelKey(p, btn) {
  if (btn === "A" || btn === "K") { const it = p.items[p.sel]; if (it && !it.skip && it.act && !it.dis) { it.act(); refreshPanel(UI.top()); } else if (it && it.dis && it.why) toastUI(it.why); return; }
  if (btn === "D") { if (p.back === false) return; if (typeof p.back === "function") p.back(); else UI.pop(); refreshPanel(UI.top()); return; }
  if (p.keys && p.keys[btn]) { p.keys[btn](); refreshPanel(UI.top()); }
}
function toastUI(t) { if (G && APP.mode === "game") toast(t, "#ff9a3c"); else APP.msg = { t, until: performance.now() + 2200 }; }
function gameKey(btn) {
  if (UI.pick) {
    if (btn === "A" || btn === "K") { const c = G.cur[G.view]; const pk = UI.pick; UI.pick = null; pk.done(c.x, c.y, pk.r); return; }
    if (btn === "D") { UI.pick = null; return; }
    if (UI.pick.r && (btn === "E" || btn === "F")) { UI.pick.r = clamp(UI.pick.r + (btn === "F" ? 6 : -6), 12, 90); return; }
    if (btn === "B" && UI.pick.layerFree) toggleView();
    return;
  }
  if (G.build) {
    if (btn === "A" || btn === "K") { G.build.paint = null; applyTool(false); return; }
    if (btn === "D" || btn === "C") { toggleBuild(); return; }
    if (btn === "E") { G.build.tool = (G.build.tool + BUILD_TOOLS.length - 1) % BUILD_TOOLS.length; return; }
    if (btn === "F") { G.build.tool = (G.build.tool + 1) % BUILD_TOOLS.length; return; }
    return;
  }
  if (btn === "A" || btn === "K") return interact();
  if (btn === "B") return toggleView();
  if (btn === "C") return toggleBuild();
  if (btn === "E") return openSquads();
  if (btn === "F") return openQueen();
  if (btn === "D") return openGameMenu();
}
function centerOn(L, x, y) { const z = G.zoomV[L]; G.cam[L].x = x - SCR_W / 2 / z; G.cam[L].y = y - VIEW_H / 2 / z; clampCam(L); }
function followCursor() {
  const L = G.view, c = G.cur[L], cam = G.cam[L], z = G.zoomV[L];
  const vw = SCR_W / z, vh = VIEW_H / z, m = 28 / z;
  if (c.x < cam.x + m) cam.x = c.x - m;
  if (c.x > cam.x + vw - m) cam.x = c.x - vw + m;
  if (c.y < cam.y + m) cam.y = c.y - m;
  if (c.y > cam.y + vh - m) cam.y = c.y - vh + m;
  clampCam(L);
}
function moveCursor(dt) {
  // continuous stick move on the map
  if (!G || UI.top() || APP.mode !== "game") { UI.curV = 0; return; }
  const h = UI.held, dx = (h.right ? 1 : 0) - (h.left ? 1 : 0), dy = (h.down ? 1 : 0) - (h.up ? 1 : 0);
  if (!dx && !dy) { UI.curV = 0; return; }
  if ((G.build || UI.pick) && G.view === "U") return;   // tile steps (dirPress repeats)
  UI.curV = Math.min(1, UI.curV + dt * 1.6);
  const sp = (50 + 130 * UI.curV) / G.zoomV[G.view];
  const L = G.view, c = G.cur[L], [W, H] = worldSize(L);
  c.x = clamp(c.x + dx * sp * dt, 1, W - 1); c.y = clamp(c.y + dy * sp * dt, 1, H - 1);
  followCursor();
}
function toggleView() {
  G.view = G.view === "S" ? "U" : "S";
  if (G.view === "U") { const c = G.cur.U; if (!c) G.cur.U = { x: uCx(G.queenTile), y: uCy(G.queenTile) }; }
  followCursor();
}
function toggleBuild() {
  if (G.build) { G.build = null; return; }
  G.build = { tool: G.lastTool || 0, paint: null };
  if (G.view !== "U") toggleView();
  const c = G.cur.U; c.x = Math.floor(c.x / T) * T + 4; c.y = Math.floor(c.y / T) * T + 4;
}
function applyTool(painting) {
  const i = uAt(G.cur.U.x, G.cur.U.y), tool = BUILD_TOOLS[G.build.tool];
  G.lastTool = G.build.tool;
  if (tool.id === "dig") {
    if (G.ut[i] === U_EMPTY || G.ut[i] === U_STONE) { if (!painting) toast(G.ut[i] === U_STONE ? "Stone: can't dig" : "Already open", "#ff9a3c"); return; }
    const want = painting ? G.build.paint : G.um[i] !== 1;
    if (painting && want === (G.um[i] === 1)) return;
    if ((G.um[i] === 1) !== want) markDig(i);
    G.build.paint = want;
    return;
  }
  if (tool.id === "fill") {
    if (G.ut[i] !== U_EMPTY) { if (!painting) toast("Nothing to fill", "#ff9a3c"); return; }
    const want = painting ? G.build.paint : G.um[i] !== 2;
    if (painting && want === (G.um[i] === 2)) return;
    if ((G.um[i] === 2) !== want) { if (!markFill(i) && !painting) toast("Can't fill: the queen would be cut off", "#ff9a3c"); }
    G.build.paint = want;
    return;
  }
  if (tool.id === "remove") {
    const b = buildingAt(i);
    if (!b) { toast("No room here", "#ff9a3c"); return; }
    if (b.kind === "qs" && b.tiles.includes(G.queenTile)) { toast("The queen lives here", "#ff9a3c"); return; }
    removeBuilding(b); G.uver++; return;
  }
  if (painting) return;
  const tiles = canPlace(tool.b, i);
  if (!tiles) { const B = BUILDS[tool.b]; toast(B.needs && !G.builds.some((b) => b.kind === B.needs && b.done) ? "Build a chitin stock first" : "Can't build here (needs open tiles, 1 tile from other rooms)", "#ff9a3c"); return; }
  addBuilding(tool.b, tiles);
}

// ---------- what is under the cursor ----------
function hoverThing() {
  if (!G || !G.cur) return null;
  const L = G.view, c = G.cur[L];
  if (L === "S") {
    let best = null, bd = 12 * 12;
    const consider = (o, kind, r) => { const d = dist2(o.x, o.y, c.x, c.y); if (d < Math.max(bd, 0) && d < r * r) { bd = d; best = { kind, o }; } };
    for (const b of G.bases) if (b.known) consider(b, "base", 16);
    for (const m of G.mobs) if (m.layer === "S" && seenS(m.x, m.y)) consider(m, "mob", 10);
    for (const a of G.ants) if (a.layer === "S") consider(a, "ant", 8);
    for (const f of G.foods) if (f.layer === "S" && f.amt > 0 && seenS(f.x, f.y)) consider(f, "food", 10);
    if (dist2(G.nest.x, G.nest.y, c.x, c.y) < 100 && !best) best = { kind: "nest", o: G.nest };
    if (!best) return null;
    best.label = labelOf(best);
    return best;
  }
  const i = uAt(c.x, c.y);
  let best = null, bd = 7 * 7;
  for (const m of G.mobs) if (m.layer === "U") { const d = dist2(m.x, m.y, c.x, c.y); if (d < bd) { bd = d; best = { kind: "mob", o: m }; } }
  for (const a of G.ants) if (a.layer === "U") { const d = dist2(a.x, a.y, c.x, c.y); if (d < bd) { bd = d; best = { kind: "ant", o: a }; } }
  if (!best && i === G.ent) best = { kind: "nest", o: G.nest };
  if (!best && G.ub[i]) best = { kind: "room", o: buildingAt(i) };
  if (!best) { const f = G.foods.find((f) => f.layer === "U" && f.known && f.amt > 0 && (f.tile === i || f.tile + 1 === i)); if (f) best = { kind: "food", o: f }; }
  if (!best) { const eg = G.eggs.find((e) => e.tile === i); if (eg) best = { kind: "egg", o: eg }; }
  if (!best) return null;
  best.label = labelOf(best);
  return best;
}
function labelOf(h) {
  const o = h.o;
  switch (h.kind) {
    case "base": return BASES[o.kind].name + (o.alive ? " " + Math.ceil(o.hp) + "/" + o.maxhp : " (gone)");
    case "mob": return o.M.name + " " + Math.ceil(o.hp) + "/" + o.maxhp;
    case "ant": return (o === G.queen ? "QUEEN" : ANTS[o.key].short) + " L" + (G.lvls[o.key] || 1) + " " + Math.ceil(o.hp) + "/" + o.maxhp;
    case "food": return (FOODS[o.kind] ? FOODS[o.kind].name : "Chitin") + " " + o.amt;
    case "nest": return G.view === "S" ? "Nest (A: go in)" : "Entrance";
    case "room": return o ? BUILDS[o.kind].name + (o.done ? "" : " " + o.got + "/" + o.need) : "";
    case "egg": return "Egg: " + ANTS[o.key].short + " " + Math.ceil(o.t) + "s";
  }
  return "";
}
function interact() {
  const h = hoverThing();
  const c = G.cur[G.view];
  if (!h) { if (G.view === "S") hereMenu(c.x, c.y); return; }
  const o = h.o;
  if (h.kind === "base") return baseMenu(o);
  if (h.kind === "ant" && o === G.queen) return openQueen();
  if (h.kind === "ant") return antMenu(o);
  if (h.kind === "mob") return UI.push({ small: true, title: o.M.name, items: [{ t: "Health " + Math.ceil(o.hp) + "/" + o.maxhp + "   Damage " + o.dmg.toFixed(1), skip: true }, { t: o.role === "thief" ? "Thief: steals food" : o.role === "raid" ? "Coming for the queen!" : o.role === "guard" ? "Guards its base" : "Walks around", skip: true }, { t: "OK", act: () => UI.pop() }] });
  if (h.kind === "food") return UI.push({ small: true, title: labelOf(h), items: [{ t: "Ants on it: " + G.ants.filter((a) => a.food === o).length, skip: true }, { t: "Bodyguard squad: walk this trail", act: () => { UI.pop(); squadOrderPick({ kind: "path", x: o.x, y: o.y }); } }, { t: "OK", act: () => UI.pop() }] });
  if (h.kind === "nest") { toggleView(); return; }
  if (h.kind === "room") return roomMenu(o);
  if (h.kind === "egg") return UI.push({ small: true, title: "Egg", items: [{ t: ANTS[o.key].name + " hatches in " + Math.ceil(o.t) + " s", skip: true }, { t: "OK", act: () => UI.pop() }] });
}
function baseMenu(b) {
  const B = BASES[b.kind];
  let n = Math.max(1, idleWarriors().length);
  UI.push(livePanel(() => ({
    small: true, title: B.name, titleR: b.alive ? "" : "DESTROYED",
    items: b.alive ? [
      { t: "Health " + Math.ceil(b.hp) + "/" + b.maxhp + "   Guards " + b.guards.length, skip: true },
      { t: "Reward: " + SUGAR_TIER[B.tier][0] + "-" + SUGAR_TIER[B.tier][1] + " sugar, food, chitin", skip: true },
      { t: "Raid with", r: () => n + " / " + idleWarriors().length + " free", adj: (d) => { n = clamp(n + d, 1, Math.max(1, idleWarriors().length)); } },
      { t: "SEND RAID", col: RED, dis: !idleWarriors().length, why: "No free warriors (make attackers in barracks)", act: () => { const k = startRaid(b, n); toast(k + " ants go to raid " + B.name, RED); UI.pop(); } },
      { t: "Send a squad here", act: () => { UI.pop(); squadOrderPick({ kind: "raid", base: b.id }); } },
      raidCount(b) ? { t: "Call the raid back (" + raidCount(b) + ")", act: () => { recallRaid(b); UI.pop(); } } : { t: "", skip: true, h: 4 },
      { t: "Close", act: () => UI.pop() },
    ] : [{ t: "Nothing left here", skip: true }, { t: "Close", act: () => UI.pop() }],
  })));
}
function idleWarriors() { return G.ants.filter((a) => (a.caste === "warrior" || a.caste === "shooter") && !a.raid && !a.squad); }
function antMenu(a) {
  const s = a.squad ? squadById(a.squad) : null;
  const what = a.raid ? "Raiding" : a.job ? a.job.type.toUpperCase() : a.carry ? "Carrying home" : a.state;
  UI.push({ small: true, title: ANTS[a.key].name + "  L" + (G.lvls[a.key] || 1), items: [
    { t: "Health " + Math.ceil(a.hp) + "/" + a.maxhp + "  Damage " + a.dmg + "  Speed " + Math.round(a.spd), skip: true },
    { t: "Doing: " + what + (s ? "   Squad: " + s.name : ""), skip: true },
    ...(ANTS[a.key].skills || []).map((t) => ({ t: "- " + t, skip: true, col: DIM })),
    { t: "OK", act: () => UI.pop() },
  ] });
}
function roomMenu(b) {
  if (!b) return;
  const B = BUILDS[b.kind];
  const items = [{ t: B.desc, skip: true }];
  if (!b.done) items.push({ t: "Building: " + b.got + "/" + b.need + " food brought", skip: true });
  if (b.kind === "qs" && b.done && !b.tiles.includes(G.queenTile)) items.push({ t: "Move the queen here", act: () => { G.queenGoto = b.tiles[0]; UI.pop(); toast("The queen is moving", GOLD); } });
  if (b.kind === "qs" && b.tiles.includes(G.queenTile)) items.push({ t: "Open the queen (make ants)", act: () => { UI.pop(); openQueen(); } });
  if (b.kind === "ws" && b.done) items.push({ t: "Workshop: level up ants", act: () => { UI.pop(); openWorkshop(); } });
  if (!(b.kind === "qs" && b.tiles.includes(G.queenTile))) items.push({ t: "Remove (food not given back)", col: RED, act: () => { removeBuilding(b); G.uver++; UI.pop(); } });
  items.push({ t: "OK", act: () => UI.pop() });
  UI.push({ small: true, title: B.name + (b.done ? "" : " (building)"), items });
}
function hereMenu(x, y) {
  if (!G.squads.length) return;
  const items = [{ t: "Squads: order to this spot", skip: true, col: DIM }];
  for (const s of G.squads) {
    items.push({ t: s.name + ": patrol here", icon: (ix, iy) => box(ix, iy, 7, 7, SQUAD_COL[(s.id - 1) % 4]), act: () => { setOrder(s, { kind: "patrol", x, y, r: 30 }); UI.pop(); } });
    items.push({ t: s.name + ": guard here", icon: (ix, iy) => box(ix, iy, 7, 7, SQUAD_COL[(s.id - 1) % 4]), act: () => { setOrder(s, { kind: "guard", x, y }); UI.pop(); } });
  }
  items.push({ t: "Close", act: () => UI.pop() });
  UI.push({ small: true, title: "Here", items });
}

// ---------- the queen (make ants) ----------
function openQueen() {
  UI.push(livePanel(() => {
    const slots = queueSlots(), items = [];
    items.push({ t: "QUEUE " + G.queue.length + "/" + slots + " slots    fed " + G.fed + "/" + (curPrice() || "-"), skip: true, col: GOLD });
    G.queue.forEach((s, k) => items.push({
      t: (k + 1) + ". " + ANTS[s.key].name, r: s.loop ? "REPEAT" : "x" + s.n, rcol: s.loop ? GREEN : INK,
      icon: (x, y) => casteIcon(casteOf(s.key), x, y),
      act: () => slotMenu(s),
    }));
    if (!G.queue.length) items.push({ t: "Nothing in the queue", skip: true, col: DIM });
    items.push({ t: "", skip: true, line: true, h: 6 });
    items.push({ t: "ADD AN EGG (A)", skip: true, col: GOLD });
    for (const c of G.deck) {
      if (casteOf(c.key) === "queen") continue;
      const s = antStats(c.key), why = canQueue(c.key);
      items.push({
        t: ANTS[c.key].name + " L" + (G.lvls[c.key] || 1), r: s.price + " food", sub: Math.round(s.hatch * HATCH_K / (G.queen.hsMul || 1)) + "s  " + ANTS[c.key].castes.map((x) => CASTES[x].name).join("/") + (queued(c.key) ? "  queued " + queued(c.key) : "") + (why ? "  - " + why : ""),
        subCol: why ? "#ff9a3c" : DIM, icon: (x, y) => casteIcon(casteOf(c.key), x, y),
        act: () => enqueue(c.key),
      });
    }
    return { title: "QUEEN - MAKE ANTS", titleR: "eggs " + G.eggs.length, items, foot: "2 queen room tiles = 1 slot, 4 eggs per slot" };
  }));
}
function slotMenu(s) {
  UI.push(livePanel(() => ({ small: true, title: ANTS[s.key].name, items: [
    { t: "Count", r: s.loop ? "-" : "x" + s.n, adj: (d) => { if (s.loop) return; if (d > 0) { if (s.n < 4 && !canQueue(s.key)) s.n++; } else { s.n--; if (s.n <= 0) { G.queue.splice(G.queue.indexOf(s), 1); UI.pop(); } } } },
    { t: "Repeat forever", r: s.loop ? "ON" : "OFF", rcol: s.loop ? GREEN : DIM, act: () => { s.loop = !s.loop; if (!s.loop && s.n < 1) s.n = 1; } },
    { t: "Remove slot", col: RED, act: () => { const k = G.queue.indexOf(s); if (k >= 0) G.queue.splice(k, 1); UI.pop(); } },
    { t: "OK", act: () => UI.pop() },
  ] })));
}

// ---------- squads ----------
let SQUAD_TYPES = () => G.deck.map((c) => c.key).filter((k) => { const c = ANTS[k].castes; return c.includes("warrior") || c.includes("guardian") || c.includes("shooter") || ANTS[k].fights; });
function openSquads() {
  UI.push(livePanel(() => {
    const items = [];
    for (const s of G.squads) {
      const m = squadMembers(s);
      items.push({ t: s.name + "  " + m.length + "/" + wantTotal(s), r: orderName(s.order), sub: Object.keys(s.want).filter((k) => s.want[k]).map((k) => ANTS[k].short + " " + m.filter((a) => a.key === k).length + "/" + s.want[k]).join("  ") || "empty: add ants",
        icon: (x, y) => box(x, y, 7, 7, SQUAD_COL[(s.id - 1) % 4]), act: () => squadMenu(s) });
    }
    if (G.squads.length < 4) items.push({ t: "+ NEW SQUAD", col: GREEN, act: () => { const s = newSquad(); squadMenu(s); } });
    items.push({ t: "", skip: true, h: 4 });
    items.push({ t: "Close", act: () => UI.pop() });
    return { title: "SQUADS", titleR: G.squads.length + "/4", items, foot: "Group ants and give them a job: patrol, guard, trail, raid" };
  }));
}
function wantTotal(s) { let n = 0; for (const k in s.want) n += s.want[k]; return n; }
function newSquad() {
  let id = 1; while (G.squads.some((s) => s.id === id)) id++;
  const s = { id, name: "Squad " + id, want: {}, order: { kind: "free" }, refill: true, lost: 0 };
  // start with what fits: 3 guardians or warriors if there are free ones
  const types = SQUAD_TYPES();
  const guard = types.find((k) => casteOf(k) === "guardian" && freeAnts(k).length) || types.find((k) => freeAnts(k).length);
  if (guard) s.want[guard] = Math.min(3, freeAnts(guard).length);
  G.squads.push(s);
  fillSquad(s);
  return s;
}
function orderName(o) { return { free: "FREE", patrol: "PATROL", guard: "GUARD", path: "TRAIL", raid: "RAID", queen: "QUEEN" }[o.kind] || "-"; }
function setOrder(s, o) { s.order = o; s.wp = null; s.leg = 1; for (const a of squadMembers(s)) { a.tgt = null; a.goal = null; a.raid = o.kind === "raid" ? o.base : null; } toast(s.name + ": " + orderName(o), SQUAD_COL[(s.id - 1) % 4]); }
function squadMenu(s) {
  UI.push(livePanel(() => {
    const items = [];
    const m = squadMembers(s);
    items.push({ t: "ANTS (left/right to change)", skip: true, col: GOLD });
    for (const k of SQUAD_TYPES()) {
      const have = m.filter((a) => a.key === k).length, want = s.want[k] || 0;
      items.push({ t: ANTS[k].name, r: have + "/" + want + "  (" + freeAnts(k).length + " free)", icon: (x, y) => casteIcon(casteOf(k), x, y),
        adj: (d) => { s.want[k] = clamp(want + d, 0, 12); fillSquad(s); } });
    }
    items.push({ t: "", skip: true, line: true, h: 6 });
    items.push({ t: "ORDER", r: orderName(s.order), act: () => ordersMenu(s) });
    items.push({ t: "Auto refill with new ants", r: s.refill ? "ON" : "OFF", rcol: s.refill ? GREEN : DIM, act: () => { s.refill = !s.refill; } });
    if (s.order.x != null) items.push({ t: "Show on map", act: () => { UI.clear(); G.view = "S"; G.cur.S.x = s.order.x; G.cur.S.y = s.order.y; followCursor(); } });
    items.push({ t: "Disband", col: RED, act: () => { for (const a of squadMembers(s)) { a.squad = null; a.raid = null; } G.squads.splice(G.squads.indexOf(s), 1); UI.pop(); } });
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: s.name.toUpperCase(), titleR: m.length + " ants", items, foot: s.lost ? "Lost so far: " + s.lost : "" };
  }));
}
function ordersMenu(s) {
  const n = G.nest, off = 60;
  const preset = (t, dx, dy) => ({ t, act: () => { UI.pop(); setOrder(s, { kind: "patrol", x: clamp(n.x + dx, 10, SURF_W * T - 10), y: clamp(n.y + dy, 10, SURF_H * T - 10), r: 26 }); } });
  UI.push({ title: s.name + ": ORDER", items: [
    { t: "Patrol an area (pick on map)", sub: "Walk around inside a circle, fight what comes in", act: () => { UI.pop(); pickOnMap(s, "patrol"); } },
    { t: "Guard a spot (pick on map)", sub: "Stand there and fight what comes close", act: () => { UI.pop(); pickOnMap(s, "guard"); } },
    { t: "Walk a trail (pick the far end)", sub: "Go back and forth between the nest and a spot", act: () => { UI.pop(); pickOnMap(s, "path"); } },
    { t: "Raid a base (pick a base)", sub: "Attack it until it falls", act: () => { UI.pop(); pickOnMap(s, "raid"); } },
    { t: "Guard the queen", sub: "Stay with the queen inside the nest", act: () => { UI.pop(); setOrder(s, { kind: "queen" }); } },
    { t: "Free (normal work)", sub: "No order: each ant does its usual job", act: () => { UI.pop(); setOrder(s, { kind: "free" }); } },
    { t: "QUICK PATROL", skip: true, col: GOLD },
    preset("Left of the nest", -off, 0), preset("Right of the nest", off, 0), preset("Above the nest", 0, -off), preset("Below the nest", 0, off),
    preset("Top-left corner", -off, -off), preset("Top-right corner", off, -off), preset("Bottom-left corner", -off, off), preset("Bottom-right corner", off, off),
    { t: "Back", act: () => UI.pop() },
  ] });
}
function pickOnMap(s, kind) {
  UI.clear();
  G.view = "S";
  const c = G.cur.S;
  if (s.order.x != null) { c.x = s.order.x; c.y = s.order.y; }
  followCursor();
  UI.pick = {
    kind, r: kind === "patrol" ? 30 : 0,
    hint: kind === "patrol" ? "PATROL: move, E/F size, A set, D cancel" : kind === "raid" ? "RAID: put the cursor on a base, A set" : kind === "path" ? "TRAIL: pick the far end, A set" : "GUARD: pick the spot, A set",
    done: (x, y, r) => {
      if (kind === "raid") {
        const b = G.bases.filter((b) => b.alive && b.known).sort((p, q) => dist2(p.x, p.y, x, y) - dist2(q.x, q.y, x, y))[0];
        if (!b || dist2(b.x, b.y, x, y) > 30 * 30) { toast("No known base there", "#ff9a3c"); return; }
        setOrder(s, { kind: "raid", base: b.id, x: b.x, y: b.y });
        return;
      }
      setOrder(s, { kind, x, y, r });
    },
  };
}
function squadOrderPick(order) {
  // from a base or food menu: choose which squad gets the order
  if (!G.squads.length) { const s = newSquad(); setOrder(s, order); if (!wantTotal(s)) squadMenu(s); return; }
  UI.push({ small: true, title: "Which squad?", items: [...G.squads.map((s) => ({ t: s.name + " (" + squadMembers(s).length + ")", icon: (x, y) => box(x, y, 7, 7, SQUAD_COL[(s.id - 1) % 4]), act: () => { UI.pop(); if (order.kind === "raid") { const b = baseById(order.base); order.x = b.x; order.y = b.y; } setOrder(s, order); } })),
    ...(G.squads.length < 4 ? [{ t: "+ New squad", col: GREEN, act: () => { UI.pop(); const s = newSquad(); setOrder(s, order); squadMenu(s); } }] : []),
    { t: "Cancel", act: () => UI.pop() }] });
}

// ---------- game menu ----------
const PRIO_NAMES = ["OFF", "LOW", "MID", "HIGH"];
function openGameMenu() {
  UI.push(livePanel(() => ({ title: "MENU", titleR: G.L.name + " " + (G.lvl + 1) + "  " + fmtTime(G.t), items: [
    { t: "Resume", act: () => UI.pop() },
    { t: "Speed", r: "x" + G.speed, adj: (d) => { G.speed = clamp(G.speed + d, 1, 3); } },
    { t: "Zoom (" + (G.view === "S" ? "surface" : "nest") + ")", r: "x" + G.zoomV[G.view], adj: (d) => { G.zoomV[G.view] = clamp(G.zoomV[G.view] + d, 1, 2); followCursor(); } },
    { t: "Pause", r: G.paused ? "ON" : "OFF", act: () => { G.paused = !G.paused; } },
    { t: "Workers priority", sub: "What builders do first", act: () => openPrio() },
    { t: "Workshop (level up)", sub: doneOf("ws").length ? (G.upgrade ? "Working: " + ANTS[G.upgrade.key].short + " plates " + G.plates + "/" + G.upgrade.need : "Pick an ant to level up") : "Build a chitin stock, then a workshop", act: () => openWorkshop() },
    { t: "Add a card to the deck", sub: "Cards can be added during a game, not removed", act: () => openAddCard() },
    { t: "Active perks", sub: [...G.perks].map((p) => PERKS.find((x) => x.id === p).name).join(", ") || "none", act: () => UI.push({ title: "ACTIVE PERKS", items: [...[...G.perks].map((p) => { const P = PERKS.find((x) => x.id === p); return { t: P.name, sub: P.desc, skip: true }; }), ...[...G.boosts].map((b) => ({ t: "Boost: " + BOOSTS.find((x) => x.id === b).name, skip: true, col: BLUE })), { t: "Back", act: () => UI.pop() }] }) },
    { t: "Side counters", r: UI.side ? "ON" : "OFF", act: () => { UI.side = !UI.side; } },
    { t: "Almanac", act: () => openAlmanac() },
    { t: "How to play", act: () => openHelp() },
    { t: "Give up (back to map)", col: RED, act: () => UI.push({ small: true, title: "Give up this game?", items: [{ t: "Yes, give up", col: RED, act: () => { UI.clear(); endRun("lose", "You gave up"); } }, { t: "No", act: () => UI.pop() }] }) },
  ] })));
}
function fmtTime(t) { const m = Math.floor(t / 60), s = Math.floor(t % 60); return m + ":" + (s < 10 ? "0" : "") + s; }
function openPrio() {
  const rows = [["feed", "Feed the queen"], ["build", "Build rooms"], ["dig", "Dig tunnels"], ["store", "Store food from the pile"], ["ws", "Workshop (chitin, plates)"], ["fill", "Fill tunnels"]];
  UI.push(livePanel(() => ({ title: "WORKERS PRIORITY", items: [
    ...rows.map(([k, t]) => ({ t, r: PRIO_NAMES[G.prio[k]], rcol: [DIM, INK, GOLD, GREEN][G.prio[k]], adj: (d) => { G.prio[k] = clamp(G.prio[k] + d, 0, 3); }, act: () => { G.prio[k] = (G.prio[k] + 1) % 4; } })),
    { t: "", skip: true, h: 4 },
    { t: "Builders: " + G.ants.filter((a) => a.caste === "builder").length + "   busy: " + G.ants.filter((a) => a.caste === "builder" && a.job).length, skip: true, col: DIM },
    { t: "Back", act: () => UI.pop() },
  ], foot: "Left/right to change. HIGH jobs are done first" })));
}
function openWorkshop() {
  if (!doneOf("ws").length) { UI.push({ small: true, title: "WORKSHOP", items: [{ t: "Build a CHITIN stock first,", skip: true }, { t: "then a WORKSHOP (2x2, 40 food).", skip: true }, { t: "Chitin drops from enemies.", skip: true, col: DIM }, { t: "OK", act: () => UI.pop() }] }); return; }
  UI.push(livePanel(() => {
    const items = [];
    if (G.upgrade) items.push({ t: "Now: " + ANTS[G.upgrade.key].name + " -> L" + (G.lvls[G.upgrade.key] + 1), sub: "chitin " + G.wsChitin + "/4  plates " + (G.plates) + "/" + G.upgrade.need + (G.wsT > 0 ? "  making " + Math.round((1 - G.wsT / 18) * 100) + "%" : "") + "  (have " + totalChitin() + " chitin)", skip: true, col: GOLD, subCol: INK });
    if (G.upgrade) items.push({ t: "Stop this upgrade", col: RED, act: () => { G.upgrade = null; G.plates = 0; } });
    for (const c of G.deck) {
      if (casteOf(c.key) === "queen") continue;
      const l = G.lvls[c.key] || 1;
      items.push({ t: ANTS[c.key].name + "  L" + l, r: l >= 5 ? "MAX" : "needs " + l + " plate" + (l > 1 ? "s" : ""), icon: (x, y) => casteIcon(casteOf(c.key), x, y), dis: l >= 5 || !!G.upgrade, why: G.upgrade ? "One upgrade at a time" : "Already max",
        act: () => { G.upgrade = { key: c.key, need: l }; G.plates = 0; } });
    }
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: "WORKSHOP", items, foot: "4 chitin = 1 plate (18 s). Builders carry plates to the queen" };
  }));
}
function openAddCard() {
  const slots = deckSlots(G.deck);
  UI.push(livePanel(() => {
    const items = [{ t: "Deck " + G.deck.length + "/" + slots, skip: true, col: GOLD }];
    for (const key of Object.keys(META.cards)) {
      const own = META.cards[key]; if (!own.some((n) => n > 0)) continue;
      if (G.deck.some((c) => c.key === key) || casteOf(key) === "queen") continue;
      const rar = bestRar(key);
      items.push({ t: ANTS[key].name, r: RAR[rar], rcol: RAR_COL[rar], dis: G.deck.length >= slots, why: "Deck is full", icon: (x, y) => casteIcon(casteOf(key), x, y),
        act: () => { G.deck.push({ key, rar }); G.lvls[key] = 1; toast(ANTS[key].name + " added", GREEN); UI.pop(); } });
    }
    if (items.length === 1) items.push({ t: "No other cards", skip: true, col: DIM });
    items.push({ t: "Back", act: () => UI.pop() });
    return { title: "ADD A CARD", items };
  }));
}
function openHelp() {
  const lines = [
    "GOAL: destroy every enemy base.", "You lose if the queen dies", "or no builders are left.", "",
    "BUTTONS (board / keyboard)", "Stick / arrows: move cursor", "A / Z / Space: use, pick", "D / X / Esc: back, menu", "B: surface <-> nest", "C: build mode", "E: squads   F: queen (ants)", "",
    "THE NEST", "Builders dig, build rooms,", "feed the queen. Food makes eggs.", "Queen rooms: 2 tiles = 1 slot.", "Barracks: 1 attacker each.", "",
    "OUTSIDE", "Scouts clear the fog and find", "food and sugar. Loaders bring", "food home. Defenders guard the", "entrance, bodyguards the trails.", "",
    "RAIDS", "Point at a base, A, SEND RAID.", "Bases give sugar, food, chitin", "and sometimes a card.", "",
    "SQUADS (new)", "Make a group (e.g. 3 bodyguards)", "and give it a job: patrol an", "area, guard a spot, walk a trail,", "raid a base or guard the queen.", "Auto refill keeps it full.",
  ];
  UI.push({ title: "HOW TO PLAY", items: [...lines.map((t) => ({ t, col: t === t.toUpperCase() && t ? GOLD : INK, h: 11, read: true })), { t: "Back", act: () => UI.pop() }], sel: 0 });
}

// ---------- pointer (touch / mouse) ----------
let ptr = null;
function canvasPos(e) { const r = cv.getBoundingClientRect(); return { x: ((e.clientX - r.left) / r.width) * SCR_W, y: ((e.clientY - r.top) / r.height) * SCR_H }; }
function onPointerDown(e) {
  e.preventDefault();
  cv.focus();
  if (typeof onAnyInput === "function") onAnyInput();
  const p = canvasPos(e);
  ptr = { x0: p.x, y0: p.y, x: p.x, y: p.y, drag: false, t0: performance.now(), id: e.pointerId };
  try { cv.setPointerCapture(e.pointerId); } catch (_) {}
  if (APP.mode === "game" && G && !UI.top() && G.build && p.y > VIEW_Y && p.y < VIEW_Y + VIEW_H) {
    const tool = BUILD_TOOLS[G.build.tool];
    if (tool.id === "dig" || tool.id === "fill") { setCursorScreen(p.x, p.y); G.build.paint = null; applyTool(false); ptr.paint = true; }
  }
}
function onPointerMove(e) {
  if (!ptr || e.pointerId !== ptr.id) return;
  const p = canvasPos(e);
  const dx = p.x - ptr.x, dy = p.y - ptr.y;
  if (Math.abs(p.x - ptr.x0) + Math.abs(p.y - ptr.y0) > 5) ptr.drag = true;
  ptr.x = p.x; ptr.y = p.y;
  if (!ptr.drag) return;
  const top = UI.top();
  if (top) {
    // dragging a list moves the selection one row per 14 px (like the stick)
    ptr.acc = (ptr.acc || 0) + dy;
    while (ptr.acc <= -14) { ptr.acc += 14; dirPress("down"); }
    while (ptr.acc >= 14) { ptr.acc -= 14; dirPress("up"); }
    return;
  }
  if (APP.mode !== "game" || !G) return;
  if (ptr.paint) { setCursorScreen(p.x, p.y); applyTool(true); return; }
  const z = G.zoomV[G.view], cam = G.cam[G.view];
  cam.x -= dx / z; cam.y -= dy / z; clampCam(G.view);
  const c = G.cur[G.view]; c.x = cam.x + SCR_W / 2 / z; c.y = cam.y + VIEW_H / 2 / z;
}
function onPointerUp(e) {
  if (!ptr || e.pointerId !== ptr.id) return;
  const p = ptr; ptr = null;
  if (p.drag || p.paint) { if (G && G.build) G.build.paint = null; return; }
  tap(p.x0, p.y0);
}
function setCursorScreen(x, y) { const z = G.zoomV[G.view], cam = G.cam[G.view]; const c = G.cur[G.view]; const [W, H] = worldSize(G.view); c.x = clamp(cam.x + x / z, 1, W - 1); c.y = clamp(cam.y + (y - VIEW_Y) / z, 1, H - 1); }
function tap(x, y) {
  const top = UI.top();
  if (top) {
    const hit = (top._rows || []).find((r) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
    if (hit) {
      const it = top.items[hit.i];
      if (it.skip) return;
      const same = top.sel === hit.i;
      top.sel = hit.i;
      if (it.adj && !it.act) { const r = panelRect(top); it.adj(x > r.x + r.w * 0.6 ? 1 : -1); refreshPanel(top); return; }
      if (it.adj && it.act && x > panelRect(top).x + panelRect(top).w * 0.6) { it.adj(1); refreshPanel(top); return; }
      if (top.kind === "grid" && !same && top.tapSelect) return;
      panelKey(top, "A");
      return;
    }
    const r = panelRect(top);
    if (!(x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) && !top.full) panelKey(top, "D");
    return;
  }
  if (APP.mode !== "game" || !G) return;
  if (y < TOP_H) {
    if (x > SCR_W - 15) { if (G.build) toggleBuild(); else G.paused = !G.paused; }
    else if (x > SCR_W - 30) G.speed = G.speed >= 3 ? 1 : G.speed + 1;
    return;
  }
  if (y >= SCR_H - BAR_H) {
    for (const b of barButtons()) if (x >= b.x && x < b.x + b.w) { b.act(); return; }
    return;
  }
  if (UI.pick && UI.pick.hintRect && y < VIEW_Y + 14) { press("D"); return; }
  setCursorScreen(x, y);
  if (UI.pick) return press("A");
  if (G.build) { applyTool(false); return; }
  interact();
}
function onWheel(e) {
  if (APP.mode !== "game" || !G || UI.top()) return;
  e.preventDefault();
  G.zoomV[G.view] = clamp(G.zoomV[G.view] + (e.deltaY < 0 ? 1 : -1), 1, 2); followCursor();
}
