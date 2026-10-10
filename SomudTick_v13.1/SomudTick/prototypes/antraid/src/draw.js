"use strict";
// ===== Drawing: world (surface, underground), sprites, top bar, bottom bar =====
// Everything is drawn in screen pixels of the board (240 x 320). The canvas is 2x for sharp text.

const PX = 2;
let cv, ctx;
let bgS = null, bgU = null, fogC = null, fogVerDrawn = -1, bgUfull = false;
const FONT = "Silkscreen, 'Courier New', monospace";
const INK = "#f4e9df", DIM = "#b39c8b", GOLD = "#ffcf4a", RED = "#ff5d4a", GREEN = "#7fd46b", BLUE = "#5ec8ff", PANEL = "#1c1511", PANEL2 = "#2a201a", LINE = "#4a3a2c";

function hashf(x, y) { let h = (Math.imul(x | 0, 374761393) + Math.imul(y | 0, 668265263)) >>> 0; h = Math.imul(h ^ (h >>> 13), 1274126177) >>> 0; return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }
function hex(c) { const n = parseInt(c.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; }
function shade(c, k) { const [r, g, b] = hex(c); const f = (v) => clamp(Math.round(v * k), 0, 255); return "rgb(" + f(r) + "," + f(g) + "," + f(b) + ")"; }

function initCanvas(el) {
  cv = el; cv.width = SCR_W * PX; cv.height = SCR_H * PX;
  ctx = cv.getContext("2d");
  ctx.imageSmoothingEnabled = false;
}
function txt(s, x, y, col, size, align, bold) {
  ctx.font = (bold ? "700 " : "") + (size || 8) + "px " + FONT;
  ctx.fillStyle = col || INK; ctx.textAlign = align || "left"; ctx.textBaseline = "top";
  ctx.fillText(s, x, y);
}
function txtW(s, size, bold) { ctx.font = (bold ? "700 " : "") + (size || 8) + "px " + FONT; return ctx.measureText(s).width; }
function fitTxt(s, w, size) { if (txtW(s, size) <= w) return s; while (s.length > 1 && txtW(s + "..", size) > w) s = s.slice(0, -1); return s + ".."; }
function box(x, y, w, h, fill, stroke) { if (fill) { ctx.fillStyle = fill; ctx.fillRect(x, y, w, h); } if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = 1; ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1); } }
function bar(x, y, w, h, f, col, bg) { box(x, y, w, h, bg || "#000"); box(x, y, Math.max(0, Math.round(w * clamp(f, 0, 1))), h, col); }

// ---------- backgrounds ----------
function buildSurfaceBg() {
  const W = SURF_W * T, H = SURF_H * T;
  bgS = document.createElement("canvas"); bgS.width = W; bgS.height = H;
  const c = bgS.getContext("2d");
  const pal = G.L.ground;
  const img = c.createImageData(W, H);
  const P = pal.map(hex);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const big = hashf(x >> 4, y >> 4), mid = hashf(x >> 2, (y >> 2) + 999), sm = hashf(x, y + 5000);
    let k = big < 0.3 ? 2 : big > 0.75 ? 3 : 0;
    if (mid > 0.82) k = 1;
    let [r, g, b] = P[k];
    const d = (sm - 0.5) * 14;
    const o = (y * W + x) * 4;
    img.data[o] = clamp(r + d, 0, 255); img.data[o + 1] = clamp(g + d, 0, 255); img.data[o + 2] = clamp(b + d, 0, 255); img.data[o + 3] = 255;
  }
  c.putImageData(img, 0, 0);
  // decorations
  for (const d of G.deco) drawDeco(c, d);
  // nest mound
  const nx = G.nest.x, ny = G.nest.y;
  c.fillStyle = shade(G.L.soil[0], 1.15); ell(c, nx, ny + 1, 13, 9);
  c.fillStyle = G.L.soil[0]; ell(c, nx, ny, 10, 7);
  c.fillStyle = shade(G.L.soil[1], 0.8); ell(c, nx, ny, 6, 4);
  c.fillStyle = "#120b07"; ell(c, nx, ny + 1, 3.5, 2.5);
}
function ell(c, x, y, rx, ry) { c.beginPath(); c.ellipse(x, y, rx, ry, 0, 0, 6.283); c.fill(); }
function drawDeco(c, d) {
  const desert = G.L.deco === "desert";
  const s = d.s;
  if (d.k === 0 || d.k === 1) { // rock
    c.fillStyle = "#6f6a62"; ell(c, d.x, d.y, 4 * s, 3 * s); c.fillStyle = "#8d877d"; ell(c, d.x - 1, d.y - 1, 2.6 * s, 1.8 * s);
  } else if (d.k === 2 || d.k === 3) { // plant
    c.fillStyle = desert ? "#b98f4a" : "#2f5a24";
    for (let k = 0; k < 4; k++) c.fillRect(Math.round(d.x + (k - 1.5) * 2 * s), Math.round(d.y - 3 * s - (k % 2) * 2), 1, Math.round(4 * s + (k % 2) * 2));
  } else if (d.k === 4) { // twig / dry coral
    c.strokeStyle = desert ? "#c89a52" : "#6a4a2a"; c.lineWidth = 1; c.beginPath(); c.moveTo(d.x - 5 * s, d.y + 2); c.lineTo(d.x + 5 * s, d.y - 2); c.moveTo(d.x, d.y); c.lineTo(d.x + 2, d.y - 4 * s); c.stroke();
  } else { // pebbles
    c.fillStyle = desert ? "#9c8a6a" : "#5d6a50"; c.fillRect(Math.round(d.x), Math.round(d.y), 2, 1); c.fillRect(Math.round(d.x + 3), Math.round(d.y + 2), 1, 1);
  }
}
function buildUnderBg() {
  bgU = document.createElement("canvas"); bgU.width = UND_W * T; bgU.height = UND_H * T;
  for (let i = 0; i < UND_W * UND_H; i++) drawUTile(i);
  bgUfull = true;
}
function drawUTile(i) {
  const c = bgU.getContext("2d");
  const x0 = uX(i) * T, y0 = uY(i) * T, k = G.ut[i];
  const soil = G.L.soil;
  const cave = G.caves.find((cv) => cv.tiles.includes(i));
  const hiddenCave = cave && !cave.food.known;
  for (let y = 0; y < T; y++) for (let x = 0; x < T; x++) {
    const px = x0 + x, py = y0 + y, h = hashf(px, py + 77);
    let col;
    if (k === U_EMPTY && !hiddenCave) {
      col = shade(soil[2], 0.55 + h * 0.12);
      // tunnel walls: darker near solid neighbours
    } else if (k === U_HARD) col = shade(soil[2], 0.85 + h * 0.2 + ((px + py) % 5 === 0 ? 0.12 : 0));
    else if (k === U_STONE) col = h > 0.5 ? "#77726a" : "#6a655d";
    else col = shade(soil[0], 0.85 + h * 0.22 + (hashf(px >> 2, py >> 2) > 0.8 ? 0.1 : 0));
    c.fillStyle = col; c.fillRect(px, py, 1, 1);
  }
  if (k === U_STONE) { c.fillStyle = "#8d877d"; c.fillRect(x0 + 2, y0 + 2, 3, 2); }
  if (k === U_EMPTY && !hiddenCave) {
    // soft edge where the tunnel meets soil
    c.fillStyle = "rgba(0,0,0,0.25)";
    if (uY(i) > 0 && G.ut[i - UND_W] !== U_EMPTY) c.fillRect(x0, y0, T, 1);
    if (uX(i) > 0 && G.ut[i - 1] !== U_EMPTY) c.fillRect(x0, y0, 1, T);
    if (uX(i) < UND_W - 1 && G.ut[i + 1] !== U_EMPTY) c.fillRect(x0 + T - 1, y0, 1, T);
    if (uY(i) < UND_H - 1 && G.ut[i + UND_W] !== U_EMPTY) c.fillRect(x0, y0 + T - 1, T, 1);
  }
}
function flushUnder() {
  if (!bgU) return;
  if (!undDirty.length) return;
  const set = new Set();
  for (const i of undDirty) { set.add(i); if (uX(i) > 0) set.add(i - 1); if (uX(i) < UND_W - 1) set.add(i + 1); if (i >= UND_W) set.add(i - UND_W); if (i + UND_W < UND_W * UND_H) set.add(i + UND_W); }
  for (const i of set) drawUTile(i);
  undDirty = [];
}
function buildFog() {
  if (!fogC) { fogC = document.createElement("canvas"); fogC.width = SURF_W; fogC.height = SURF_H; }
  const c = fogC.getContext("2d");
  const img = c.createImageData(SURF_W, SURF_H);
  for (let i = 0; i < SURF_W * SURF_H; i++) { img.data[i * 4 + 3] = G.fog[i] ? 0 : 235; img.data[i * 4] = 12; img.data[i * 4 + 1] = 9; img.data[i * 4 + 2] = 8; }
  c.putImageData(img, 0, 0);
  fogVerDrawn = G.fogVer;
}

// ---------- sprites ----------
function drawBug(x, y, th, size, col, moving, anim, look, hit) {
  // a little bug seen from above: abdomen, thorax, head, legs
  const s = size;
  const cx = Math.cos(th), sy = Math.sin(th);
  const at = (d, side) => [x + cx * d - sy * side, y + sy * d + cx * side];
  ctx.fillStyle = hit ? "#ffffff" : col;
  const dark = hit ? "#ffffff" : shade(col, 0.6);
  // legs
  ctx.strokeStyle = dark; ctx.lineWidth = Math.max(0.6, s * 0.35);
  const wig = moving ? Math.sin(anim) * 0.9 : 0;
  ctx.beginPath();
  for (let k = -1; k <= 1; k++) for (const side of [-1, 1]) {
    const [bx, by] = at(k * s * 0.9, 0);
    const [ex, ey] = at(k * s * 1.2 + (k === 0 ? wig * side : -wig * side) * s * 0.6, side * s * 2.0);
    ctx.moveTo(bx, by); ctx.lineTo(ex, ey);
  }
  ctx.stroke();
  const blob = (d, r1, r2, c) => { const [px, py] = at(d, 0); ctx.fillStyle = c; ctx.beginPath(); ctx.ellipse(px, py, r1, r2, th, 0, 6.283); ctx.fill(); };
  if (look === "bug" || look === "beetle") {
    blob(-s * 0.3, s * 2.0, s * 1.6, col);
    blob(s * 1.6, s * 0.8, s * 0.9, dark);
    if (look === "bug" && !hit) { ctx.fillStyle = "#1a1210"; for (const [d, sd] of [[0, 0.8], [0, -0.8], [-1, 0.5], [-1, -0.5]]) { const [px, py] = at(d * s, sd * s); ctx.fillRect(px - 0.5, py - 0.5, 1, 1); } }
    if (look === "beetle" && !hit) { ctx.strokeStyle = "rgba(255,255,255,0.25)"; ctx.lineWidth = 0.6; const [a1, b1] = at(s * 1.2, 0), [a2, b2] = at(-s * 1.9, 0); ctx.beginPath(); ctx.moveTo(a1, b1); ctx.lineTo(a2, b2); ctx.stroke(); }
    return;
  }
  if (look === "bee") {
    blob(-s * 1.0, s * 1.5, s * 1.1, col); blob(s * 0.4, s * 0.8, s * 0.8, dark); blob(s * 1.3, s * 0.6, s * 0.6, dark);
    if (!hit) { ctx.fillStyle = "#1a1210"; const [p1, q1] = at(-s * 1.0, 0); ctx.fillRect(p1 - 0.5, q1 - s, 1, s * 2); }
    ctx.fillStyle = "rgba(255,255,255,0.55)"; const [w1, v1] = at(0, s * 1.4), [w2, v2] = at(0, -s * 1.4); ctx.beginPath(); ctx.ellipse(w1, v1, s * 0.9, s * 0.5, th, 0, 6.283); ctx.ellipse(w2, v2, s * 0.9, s * 0.5, th, 0, 6.283); ctx.fill();
    return;
  }
  if (look === "scorp") {
    blob(0, s * 1.4, s * 1.0, col); blob(s * 1.3, s * 0.7, s * 0.7, dark);
    // claws and tail
    ctx.strokeStyle = col; ctx.lineWidth = s * 0.5; ctx.beginPath();
    for (const side of [-1, 1]) { const [a, b] = at(s * 1.6, side * s * 0.6), [c2, d2] = at(s * 2.8, side * s * 1.2); ctx.moveTo(a, b); ctx.lineTo(c2, d2); }
    const [t1, u1] = at(-s * 1.2, 0), [t2, u2] = at(-s * 2.6, wig * 0.3 * s), [t3, u3] = at(-s * 2.0, s * 0.9); ctx.moveTo(t1, u1); ctx.lineTo(t2, u2); ctx.lineTo(t3, u3); ctx.stroke();
    return;
  }
  if (look === "spider") {
    ctx.strokeStyle = dark; ctx.lineWidth = s * 0.3; ctx.beginPath();
    for (let k = 0; k < 4; k++) for (const side of [-1, 1]) { const [bx, by] = at(s * 0.2, 0); const [ex, ey] = at((k - 1.5) * s * 1.1 + wig * s * 0.3 * side, side * s * 2.6); ctx.moveTo(bx, by); ctx.lineTo(ex, ey); }
    ctx.stroke(); blob(-s * 0.7, s * 1.2, s * 1.0, col); blob(s * 0.8, s * 0.8, s * 0.7, dark);
    return;
  }
  if (look === "grub") { blob(-s * 0.5, s * 2.0, s * 1.3, col); blob(s * 1.4, s * 0.7, s * 0.7, dark); return; }
  // ant (and termite)
  blob(-s * 1.15, s * 1.15, s * 0.85, col);
  blob(0, s * 0.7, s * 0.5, col);
  blob(s * 0.95, s * 0.65, s * 0.6, dark);
}
function antColor(a) { return a === G.queen ? "#7a4a22" : a.caste === "warrior" || a.caste === "shooter" ? "#4a1e14" : "#2e1c14"; }
function drawAnt(a, ox, oy, z) {
  const x = (a.x - ox) * z, y = (a.y - oy) * z + VIEW_Y;
  if (x < -10 || y < VIEW_Y - 10 || x > SCR_W + 10 || y > VIEW_Y + VIEW_H + 10) return;
  const s = a.size * 1.1 * z;
  drawBug(x, y, a.th, s, antColor(a), a.moving, a.anim, "ant", a.hitT > 0);
  // caste mark
  const cc = CASTES[a.caste].col;
  ctx.fillStyle = cc;
  const bx = x - Math.cos(a.th) * s * 1.1, by = y - Math.sin(a.th) * s * 1.1;
  ctx.fillRect(Math.round(bx - z * 0.5), Math.round(by - z * 0.5), Math.max(1, z), Math.max(1, z));
  if (a.squad) { ctx.fillStyle = "#c08bff"; ctx.fillRect(Math.round(x - 0.5), Math.round(y - s * 2.2), 1, 1); }
  if (a.carry) {
    const hx = x + Math.cos(a.th) * s * 1.8, hy = y + Math.sin(a.th) * s * 1.8;
    ctx.fillStyle = a.carry.food ? "#e8b04a" : a.carry.chitin ? "#9fd0ff" : "#cfe8ff";
    ctx.fillRect(Math.round(hx - z), Math.round(hy - z), Math.max(2, 2 * z), Math.max(2, 2 * z));
  }
  if (a === G.queen) { ctx.fillStyle = GOLD; for (let k = -1; k <= 1; k++) ctx.fillRect(Math.round(x + k * 2 * z), Math.round(y - s * 2.4 - (k === 0 ? z : 0)), Math.max(1, z), Math.max(1, z) * 2); }
  if (a.hp < a.maxhp && a.maxhp > 0) bar(Math.round(x - 5), Math.round(y - s * 2 - 3), 10, 1, a.hp / a.maxhp, a.hp / a.maxhp > 0.4 ? GREEN : RED);
  if (a.digging > 0 && Math.floor(G.t * 8) % 2) { ctx.fillStyle = "#c9a26a"; ctx.fillRect(Math.round(x + Math.cos(a.th) * 4 * z), Math.round(y + Math.sin(a.th) * 4 * z), 1, 1); }
}
function drawMob(m, ox, oy, z) {
  const x = (m.x - ox) * z, y = (m.y - oy) * z + VIEW_Y;
  if (x < -14 || y < VIEW_Y - 14 || x > SCR_W + 14 || y > VIEW_Y + VIEW_H + 14) return;
  drawBug(x, y, m.th, m.size * 1.05 * z, m.M.col, m.moving, m.anim, m.M.look, m.hitT > 0);
  if (m.hp < m.maxhp) bar(Math.round(x - 6), Math.round(y - m.size * 2.4 * z - 3), 12, 1, m.hp / m.maxhp, RED);
  if (m.carry > 0.5) { ctx.fillStyle = "#e8b04a"; ctx.fillRect(Math.round(x - 1), Math.round(y - 1), 2, 2); }
  if (m.stun > 0) txt("z", x + 3, y - 10, "#9fd0ff", 8);
}
function drawFood(f, ox, oy, z) {
  const x = (f.x - ox) * z, y = (f.y - oy) * z + VIEW_Y;
  if (x < -14 || y < VIEW_Y - 14 || x > SCR_W + 14 || y > VIEW_Y + VIEW_H + 14) return;
  const F = FOODS[f.kind] || FOODS.drop;
  const k = clamp(f.amt / Math.max(1, f.max), 0.25, 1);
  if (f.kind === "chitin") { ctx.fillStyle = "#9fd0ff"; for (let j = 0; j < Math.min(5, f.amt); j++) ctx.fillRect(Math.round(x + (j % 3) * 2 * z - 2), Math.round(y + Math.floor(j / 3) * 2 * z), Math.max(1, z), Math.max(1, z)); return; }
  if (f.kind === "drop") { ctx.fillStyle = F.col; for (let j = 0; j < Math.min(9, Math.ceil(f.amt / 3)); j++) { const a = j * 2.4, r = (j % 3) * 1.4 * z; ctx.fillRect(Math.round(x + Math.cos(a) * r), Math.round(y + Math.sin(a) * r), Math.max(1, z), Math.max(1, z)); } return; }
  const s = (3 + 4 * k) * z;
  if (f.kind === "mushroom" || f.kind === "cave") {
    ctx.fillStyle = F.col2; ctx.fillRect(Math.round(x - s * 0.25), Math.round(y - s * 0.1), Math.round(s * 0.5), Math.round(s * 0.7));
    ctx.fillStyle = F.col; ctx.beginPath(); ctx.ellipse(x, y - s * 0.2, s * 0.8, s * 0.55, 0, Math.PI, 0); ctx.fill();
    ctx.fillStyle = "#fff4e0"; ctx.fillRect(Math.round(x - s * 0.3), Math.round(y - s * 0.55), Math.max(1, z), Math.max(1, z)); ctx.fillRect(Math.round(x + s * 0.25), Math.round(y - s * 0.45), Math.max(1, z), Math.max(1, z));
  } else if (f.kind === "berry") {
    ctx.fillStyle = F.col; for (let j = 0; j < 3 + Math.round(k * 3); j++) { const a = j * 2.1; ctx.beginPath(); ctx.arc(x + Math.cos(a) * s * 0.4, y + Math.sin(a) * s * 0.35, s * 0.3, 0, 6.283); ctx.fill(); }
    ctx.fillStyle = F.col2; ctx.fillRect(Math.round(x - 1), Math.round(y - 1), 1, 1);
  } else if (f.kind === "seed") {
    for (let j = 0; j < 3 + Math.round(k * 4); j++) { ctx.fillStyle = j % 2 ? F.col : F.col2; const a = j * 1.9, r = (j % 3) * s * 0.22; ctx.beginPath(); ctx.ellipse(x + Math.cos(a) * r, y + Math.sin(a) * r, s * 0.22, s * 0.14, a, 0, 6.283); ctx.fill(); }
  } else if (f.kind === "cactus") {
    ctx.fillStyle = F.col2; ctx.fillRect(Math.round(x - s * 0.2), Math.round(y - s * 0.8), Math.round(s * 0.4), Math.round(s * 1.2));
    ctx.fillStyle = F.col; for (let j = 0; j < 1 + Math.round(k * 3); j++) ctx.fillRect(Math.round(x - s * 0.5 + j * s * 0.35), Math.round(y - s * 1.0 + (j % 2) * 2), Math.max(2, 2 * z), Math.max(2, 2 * z));
  }
}
function drawBase(b, ox, oy, z) {
  const x = (b.x - ox) * z, y = (b.y - oy) * z + VIEW_Y;
  if (x < -30 || y < VIEW_Y - 30 || x > SCR_W + 30 || y > VIEW_Y + VIEW_H + 30) return;
  const B = BASES[b.kind], s = z;
  if (!b.alive) { ctx.fillStyle = "#4a3a2c"; ctx.beginPath(); ctx.ellipse(x, y + 2 * s, 12 * s, 6 * s, 0, 0, 6.283); ctx.fill(); ctx.fillStyle = "#2a1f18"; ctx.fillRect(Math.round(x - 6 * s), Math.round(y), Math.round(3 * s), Math.round(2 * s)); ctx.fillRect(Math.round(x + 2 * s), Math.round(y + 2 * s), Math.round(4 * s), Math.round(2 * s)); return; }
  const col = b.hitT > 0 ? "#ffffff" : B.col;
  if (B.look === "hive") {
    ctx.fillStyle = shade(B.col, 0.6); ctx.beginPath(); ctx.ellipse(x, y + 8 * s, 12 * s, 4 * s, 0, 0, 6.283); ctx.fill();
    for (let k = 0; k < 4; k++) { ctx.fillStyle = k % 2 ? col : shade(B.col, 0.85); ctx.beginPath(); ctx.ellipse(x, y + 4 * s - k * 4 * s, (10 - k * 1.8) * s, 3 * s, 0, 0, 6.283); ctx.fill(); }
    ctx.fillStyle = "#1a1210"; ctx.beginPath(); ctx.ellipse(x, y + 3 * s, 2 * s, 1.5 * s, 0, 0, 6.283); ctx.fill();
  } else if (B.look === "tower") {
    ctx.fillStyle = shade(B.col, 0.7); ctx.beginPath(); ctx.moveTo(x - 11 * s, y + 8 * s); ctx.lineTo(x - 4 * s, y - 14 * s); ctx.lineTo(x + 1 * s, y - 6 * s); ctx.lineTo(x + 5 * s, y - 16 * s); ctx.lineTo(x + 12 * s, y + 8 * s); ctx.fill();
    ctx.fillStyle = col; ctx.beginPath(); ctx.moveTo(x - 8 * s, y + 8 * s); ctx.lineTo(x - 4 * s, y - 10 * s); ctx.lineTo(x + 5 * s, y - 12 * s); ctx.lineTo(x + 9 * s, y + 8 * s); ctx.fill();
    ctx.fillStyle = "#1a1210"; ctx.fillRect(Math.round(x - 1.5 * s), Math.round(y + 3 * s), Math.round(3 * s), Math.round(4 * s));
  } else if (B.look === "rock") {
    ctx.fillStyle = "#5e5a54"; ctx.beginPath(); ctx.ellipse(x, y, 14 * s, 9 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = "#7a756c"; ctx.beginPath(); ctx.ellipse(x - 2 * s, y - 2 * s, 10 * s, 6 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = col; ctx.beginPath(); ctx.ellipse(x + 2 * s, y + 5 * s, 6 * s, 3.5 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = "#120b07"; ctx.beginPath(); ctx.ellipse(x + 2 * s, y + 5 * s, 3.5 * s, 2 * s, 0, 0, 6.283); ctx.fill();
  } else if (B.look === "hole") {
    ctx.fillStyle = shade(B.col, 1.2); ctx.beginPath(); ctx.ellipse(x, y, 12 * s, 8 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = col; ctx.beginPath(); ctx.ellipse(x, y, 8 * s, 5 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = "#0e0806"; ctx.beginPath(); ctx.ellipse(x, y + 1 * s, 5 * s, 3 * s, 0, 0, 6.283); ctx.fill();
    ctx.strokeStyle = "rgba(255,255,255,0.35)"; ctx.lineWidth = 0.6; ctx.beginPath(); for (let k = 0; k < 6; k++) { const a = k * 1.05; ctx.moveTo(x, y); ctx.lineTo(x + Math.cos(a) * 11 * s, y + Math.sin(a) * 7 * s); } ctx.stroke();
  } else { // mound with leaves
    ctx.fillStyle = shade(B.col, 0.75); ctx.beginPath(); ctx.ellipse(x, y + 2 * s, 13 * s, 8 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = col; ctx.beginPath(); ctx.ellipse(x, y, 10 * s, 6 * s, 0, 0, 6.283); ctx.fill();
    ctx.fillStyle = "#3d7a2e"; for (let k = 0; k < 5; k++) { const a = k * 1.3; ctx.beginPath(); ctx.ellipse(x + Math.cos(a) * 8 * s, y + Math.sin(a) * 5 * s, 3 * s, 1.5 * s, a, 0, 6.283); ctx.fill(); }
    ctx.fillStyle = "#120b07"; ctx.beginPath(); ctx.ellipse(x, y + 1 * s, 3 * s, 2 * s, 0, 0, 6.283); ctx.fill();
  }
  const f = b.hp / b.maxhp;
  if (f < 1) bar(Math.round(x - 12), Math.round(y - 20 * s), 24, 2, f, RED);
  const rc = raidCount(b);
  if (rc) txt("x" + rc, x + 13 * s, y - 18 * s, RED, 8);
}

// ---------- the world view ----------
function camFor(layer) { return G.cam[layer]; }
function worldSize(layer) { return layer === "S" ? [SURF_W * T, SURF_H * T] : [UND_W * T, UND_H * T]; }
function clampCam(layer) {
  const c = G.cam[layer], [W, H] = worldSize(layer), z = G.zoomV[layer];
  const vw = SCR_W / z, vh = VIEW_H / z;
  c.x = W <= vw ? (W - vw) / 2 : clamp(c.x, 0, W - vw);
  c.y = H <= vh ? (H - vh) / 2 : clamp(c.y, 0, H - vh);
}
function drawWorld() {
  const L = G.view, c = G.cam[L], z = G.zoomV[L];
  clampCam(L);
  ctx.save();
  ctx.beginPath(); ctx.rect(0, VIEW_Y, SCR_W, VIEW_H); ctx.clip();
  ctx.fillStyle = "#0c0907"; ctx.fillRect(0, VIEW_Y, SCR_W, VIEW_H);
  const ox = c.x, oy = c.y;
  if (L === "S") {
    if (!bgS) buildSurfaceBg();
    ctx.drawImage(bgS, -ox * z, VIEW_Y - oy * z, bgS.width * z, bgS.height * z);
    // trails: raids and squad orders
    for (const b of G.bases) if (b.alive && raidCount(b)) dashed((G.nest.x - ox) * z, (G.nest.y - oy) * z + VIEW_Y, (b.x - ox) * z, (b.y - oy) * z + VIEW_Y, "rgba(255,93,74,0.6)");
    for (const s of G.squads) drawSquadMark(s, ox, oy, z);
    for (const f of G.foods) if (f.layer === "S" && f.amt > 0 && seenS(f.x, f.y)) drawFood(f, ox, oy, z);
    for (const s of G.sugars) if (!s.got && seenS(s.x, s.y)) { const x = (s.x - ox) * z, y = (s.y - oy) * z + VIEW_Y; box(Math.round(x - 2 * z), Math.round(y - 2 * z), Math.round(4 * z), Math.round(4 * z), "#ffffff"); box(Math.round(x - 2 * z), Math.round(y + z), Math.round(4 * z), Math.max(1, z), "#d8d0c8"); }
    for (const b of G.bases) if (b.known || seenS(b.x, b.y)) drawBase(b, ox, oy, z);
    for (const a of G.ants) if (a.layer === "S") drawAnt(a, ox, oy, z);
    for (const m of G.mobs) if (m.layer === "S" && seenS(m.x, m.y)) drawMob(m, ox, oy, z);
    if (fogVerDrawn !== G.fogVer) buildFog();
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(fogC, -ox * z - T * z * 0.5, VIEW_Y - oy * z - T * z * 0.5, SURF_W * T * z + T * z, SURF_H * T * z + T * z);
    ctx.imageSmoothingEnabled = false;
  } else {
    if (!bgU) buildUnderBg();
    flushUnder();
    ctx.drawImage(bgU, -ox * z, VIEW_Y - oy * z, bgU.width * z, bgU.height * z);
    drawUnderStuff(ox, oy, z);
    for (const a of G.ants) if (a.layer === "U") drawAnt(a, ox, oy, z);
    for (const m of G.mobs) if (m.layer === "U") drawMob(m, ox, oy, z);
  }
  for (const s of G.shots) if (s.layer === L) { ctx.fillStyle = "#ffcf4a"; const k = 1 - s.t / 0.25; ctx.fillRect(Math.round((s.x + (s.tx - s.x) * k - ox) * z), Math.round((s.y + (s.ty - s.y) * k - oy) * z + VIEW_Y), 2, 2); }
  for (const f of G.fx) if (f.layer === L) {
    const x = (f.x - ox) * z, y = (f.y - oy) * z + VIEW_Y;
    if (f.ring) { ctx.strokeStyle = f.col; ctx.globalAlpha = clamp(f.t * 2, 0, 1); ctx.lineWidth = 1; ctx.beginPath(); ctx.arc(x, y, (f.big ? 30 : 8) * (1 - f.t / (f.big ? 1.2 : 0.6)) + 2, 0, 6.283); ctx.stroke(); ctx.globalAlpha = 1; }
    if (f.txt) txt(f.txt, x, y - (0.5 - f.t) * 10, f.col, 8, "center");
  }
  drawCursorAndPick(ox, oy, z);
  ctx.restore();
  drawAlertsEdge(ox, oy, z);
}
function dashed(x1, y1, x2, y2, col) { ctx.strokeStyle = col; ctx.lineWidth = 1; ctx.setLineDash([3, 3]); ctx.lineDashOffset = -G.t * 8; ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); ctx.setLineDash([]); }
function drawSquadMark(s, ox, oy, z) {
  const o = s.order; if (!o || o.x == null) return;
  const x = (o.x - ox) * z, y = (o.y - oy) * z + VIEW_Y;
  const col = SQUAD_COL[(s.id - 1) % SQUAD_COL.length];
  ctx.strokeStyle = col; ctx.globalAlpha = 0.7; ctx.lineWidth = 1;
  if (o.kind === "patrol") { ctx.setLineDash([2, 3]); ctx.beginPath(); ctx.arc(x, y, o.r * z, 0, 6.283); ctx.stroke(); ctx.setLineDash([]); }
  if (o.kind === "path") dashed((G.nest.x - ox) * z, (G.nest.y - oy) * z + VIEW_Y, x, y, col);
  ctx.globalAlpha = 1;
  // flag
  box(Math.round(x), Math.round(y - 9), 1, 9, "#e8e0d8");
  box(Math.round(x + 1), Math.round(y - 9), 6, 4, col);
  txt(String(s.id), x + 2, y - 9, "#120b07", 8);
}
const SQUAD_COL = ["#c08bff", "#5ec8ff", "#ffb02e", "#7fd46b"];
function drawUnderStuff(ox, oy, z) {
  const tz = T * z;
  // buildings
  for (const b of G.builds) {
    const B = BUILDS[b.kind];
    for (const i of b.tiles) {
      const x = Math.round((uX(i) * T - ox) * z), y = Math.round((uY(i) * T - oy) * z + VIEW_Y);
      ctx.fillStyle = b.done ? shade(B.col, 0.55) : "rgba(0,0,0,0.25)";
      ctx.fillRect(x + z, y + z, tz - 2 * z, tz - 2 * z);
      if (!b.done) { ctx.strokeStyle = B.col; ctx.setLineDash([2, 2]); ctx.strokeRect(x + 1.5, y + 1.5, tz - 3, tz - 3); ctx.setLineDash([]); }
      else { ctx.fillStyle = shade(B.col, 0.75); ctx.fillRect(x + z, y + tz - 2 * z, tz - 2 * z, z); }
    }
    const x0 = Math.round((uX(b.tiles[0]) * T - ox) * z), y0 = Math.round((uY(b.tiles[0]) * T - oy) * z + VIEW_Y);
    if (!b.done) bar(x0 + 1, y0 + tz - 2, (B.size === 2 ? 2 : 1) * tz - 2, 1, b.got / b.need, GOLD);
    if (b.kind === "br" || b.kind === "bb") { ctx.fillStyle = shade(B.col, 1.2); ctx.fillRect(x0 + 2 * z, y0 + 2 * z, 4 * z, z); }
    if (b.kind === "ws" && b.done) {
      ctx.fillStyle = "#9aa8b4"; ctx.fillRect(x0 + 4 * z, y0 + 6 * z, 8 * z, 3 * z); ctx.fillRect(x0 + 6 * z, y0 + 9 * z, 4 * z, 3 * z);
      if (G.wsT > 0) bar(x0 + 2, y0 + 2 * tz - 3, 2 * tz - 4, 1, 1 - G.wsT / 18, BLUE);
      for (let k = 0; k < G.wsChitin; k++) { ctx.fillStyle = "#9fd0ff"; ctx.fillRect(x0 + (2 + k * 2) * z, y0 + 2 * z, z, z); }
      for (let k = 0; k < G.wsPlate; k++) { ctx.fillStyle = "#cfe8ff"; ctx.fillRect(x0 + (10 + k * 3) * z, y0 + 2 * z, 2 * z, 2 * z); }
    }
  }
  // stored food, chitin, eggs
  for (let i = 0; i < UND_W * UND_H; i++) {
    const sf = G.sf[i], sc = G.sc[i];
    if (!sf && !sc) continue;
    const x = (uX(i) * T - ox) * z, y = (uY(i) * T - oy) * z + VIEW_Y;
    for (let k = 0; k < sf; k++) { ctx.fillStyle = k % 2 ? "#e8b04a" : "#d8863a"; ctx.fillRect(Math.round(x + (1 + (k % 4) * 1.6) * z), Math.round(y + (4.5 + Math.floor(k / 4) * 1.6) * z), Math.max(1, z), Math.max(1, z)); }
    for (let k = 0; k < sc; k++) { ctx.fillStyle = "#9fd0ff"; ctx.fillRect(Math.round(x + (1 + (k % 4) * 1.6) * z), Math.round(y + 2 * z), Math.max(1, z), Math.max(1, z)); }
  }
  for (const e of G.eggs) {
    const n = G.eggs.filter((q) => q.tile === e.tile).indexOf(e);
    const x = (uCx(e.tile) - ox + (n ? 2 : -1)) * z, y = (uCy(e.tile) - oy - 1) * z + VIEW_Y;
    ctx.fillStyle = "#fff5e6"; ctx.beginPath(); ctx.ellipse(x, y, 1.4 * z, 1.9 * z, 0.3, 0, 6.283); ctx.fill();
    ctx.fillStyle = CASTES[casteOf(e.key)].col; ctx.fillRect(Math.round(x - 0.5), Math.round(y + 1.2 * z), Math.max(1, z), Math.max(1, z * 0.6));
  }
  // the pile at the entrance
  if (G.pile.food || G.pile.chitin) {
    const x = (uCx(G.ent) - ox) * z, y = (uCy(G.ent) - oy) * z + VIEW_Y;
    for (let k = 0; k < Math.min(12, G.pile.food); k++) { ctx.fillStyle = "#e8b04a"; ctx.fillRect(Math.round(x - 3 * z + (k % 4) * 1.6 * z), Math.round(y + 1 * z - Math.floor(k / 4) * 1.4 * z), Math.max(1, z), Math.max(1, z)); }
    for (let k = 0; k < Math.min(6, G.pile.chitin); k++) { ctx.fillStyle = "#9fd0ff"; ctx.fillRect(Math.round(x + 2 * z + (k % 2) * 1.6 * z), Math.round(y - 2 * z + Math.floor(k / 2) * 1.4 * z), Math.max(1, z), Math.max(1, z)); }
  }
  // underground food sources (caves)
  for (const f of G.foods) if (f.layer === "U" && f.known && f.amt > 0) drawFood(f, ox, oy, z);
  // designations
  for (const i of G.digList) { const x = Math.round((uX(i) * T - ox) * z), y = Math.round((uY(i) * T - oy) * z + VIEW_Y); ctx.strokeStyle = "rgba(255,207,74,0.9)"; ctx.lineWidth = 1; ctx.strokeRect(x + 1.5, y + 1.5, tz - 3, tz - 3); ctx.beginPath(); ctx.moveTo(x + 2, y + tz - 2); ctx.lineTo(x + tz - 2, y + 2); ctx.stroke(); if (G.uhp[i] < U_HP[G.ut[i]]) bar(x + 1, y + tz - 2, tz - 2, 1, 1 - G.uhp[i] / U_HP[G.ut[i]], GOLD); }
  for (const i of G.fillList) { const x = Math.round((uX(i) * T - ox) * z), y = Math.round((uY(i) * T - oy) * z + VIEW_Y); ctx.strokeStyle = "rgba(94,200,255,0.9)"; ctx.strokeRect(x + 1.5, y + 1.5, tz - 3, tz - 3); }
  // entrance light
  const ex = (uCx(G.ent) - ox) * z, ey = (uY(G.ent) * T - oy) * z + VIEW_Y;
  ctx.fillStyle = "rgba(255,230,160,0.35)"; ctx.fillRect(Math.round(ex - 3 * z), Math.round(ey), Math.round(6 * z), Math.round(2 * z));
}
function drawCursorAndPick(ox, oy, z) {
  if (!G.cur || UI.top()) return;
  const L = G.view, c = G.cur[L];
  const x = (c.x - ox) * z, y = (c.y - oy) * z + VIEW_Y;
  if (L === "U" && (G.build || UI.pick)) {
    const i = uAt(c.x, c.y), tz = T * z;
    const tx = Math.round((uX(i) * T - ox) * z), ty = Math.round((uY(i) * T - oy) * z + VIEW_Y);
    let ok = true, size = 1;
    if (G.build) {
      const tool = BUILD_TOOLS[G.build.tool];
      if (tool.b) { const tiles = canPlace(tool.b, i); ok = !!tiles; size = BUILDS[tool.b].size; }
      else if (tool.id === "dig") ok = G.ut[i] !== U_EMPTY && G.ut[i] !== U_STONE;
      else if (tool.id === "fill") ok = G.ut[i] === U_EMPTY && !G.ub[i] && i !== G.ent;
      else if (tool.id === "remove") ok = !!G.ub[i];
    }
    ctx.strokeStyle = ok ? "#ffffff" : RED; ctx.lineWidth = 1;
    ctx.strokeRect(tx + 0.5, ty + 0.5, tz * size - 1, tz * size - 1);
    return;
  }
  const blink = Math.floor(performance.now() / 400) % 2 ? 1 : 0.7;
  ctx.globalAlpha = blink;
  ctx.fillStyle = "#000"; ctx.fillRect(Math.round(x - 5), Math.round(y), 11, 1); ctx.fillRect(Math.round(x), Math.round(y - 5), 1, 11);
  ctx.fillStyle = "#fff"; ctx.fillRect(Math.round(x - 4), Math.round(y), 3, 1); ctx.fillRect(Math.round(x + 2), Math.round(y), 3, 1); ctx.fillRect(Math.round(x), Math.round(y - 4), 1, 3); ctx.fillRect(Math.round(x), Math.round(y + 2), 1, 3);
  ctx.globalAlpha = 1;
  if (UI.pick && UI.pick.r) { ctx.strokeStyle = "rgba(192,139,255,0.9)"; ctx.setLineDash([2, 2]); ctx.beginPath(); ctx.arc(x, y, UI.pick.r * z, 0, 6.283); ctx.stroke(); ctx.setLineDash([]); }
  // what is under the cursor
  const h = hoverThing();
  if (h && h.label) { const w = txtW(h.label) + 6; const bx = clamp(Math.round(x + 8), 2, SCR_W - w - 2), by = clamp(Math.round(y - 14), VIEW_Y + 2, VIEW_Y + VIEW_H - 12); box(bx, by, w, 11, "rgba(18,13,10,0.85)"); txt(h.label, bx + 3, by + 2, h.col || INK); }
}
function drawAlertsEdge(ox, oy, z) {
  for (const a of G.alerts) {
    if (a.layer !== G.view) continue;
    const x = (a.x - ox) * z, y = (a.y - oy) * z + VIEW_Y;
    if (x > 0 && x < SCR_W && y > VIEW_Y && y < VIEW_Y + VIEW_H) { if (Math.floor(G.t * 4) % 2) { ctx.strokeStyle = RED; ctx.beginPath(); ctx.arc(x, y, 9, 0, 6.283); ctx.stroke(); } continue; }
    const cx = SCR_W / 2, cy = VIEW_Y + VIEW_H / 2, ang = Math.atan2(y - cy, x - cx);
    const ex = clamp(cx + Math.cos(ang) * 200, 8, SCR_W - 8), ey = clamp(cy + Math.sin(ang) * 200, VIEW_Y + 8, VIEW_Y + VIEW_H - 8);
    ctx.fillStyle = RED; ctx.beginPath(); ctx.moveTo(ex + Math.cos(ang) * 6, ey + Math.sin(ang) * 6); ctx.lineTo(ex + Math.cos(ang + 2.4) * 5, ey + Math.sin(ang + 2.4) * 5); ctx.lineTo(ex + Math.cos(ang - 2.4) * 5, ey + Math.sin(ang - 2.4) * 5); ctx.fill();
  }
}

// ---------- icons ----------
function icon(kind, x, y, s) {
  s = s || 1;
  const r = (a, b, w, h, c) => { ctx.fillStyle = c; ctx.fillRect(Math.round(x + a * s), Math.round(y + b * s), Math.max(1, Math.round(w * s)), Math.max(1, Math.round(h * s))); };
  switch (kind) {
    case "ant": r(1, 4, 2, 2, "#c9b5a0"); r(3, 4, 2, 2, "#c9b5a0"); r(5, 3, 3, 3, "#c9b5a0"); r(0, 2, 1, 1, "#c9b5a0"); r(2, 6, 1, 2, "#8d7a68"); r(5, 6, 1, 2, "#8d7a68"); break;
    case "food": r(2, 1, 4, 3, "#d8443a"); r(1, 2, 6, 2, "#d8443a"); r(3, 4, 2, 3, "#f3e2c8"); r(3, 2, 1, 1, "#fff"); break;
    case "chitin": r(1, 2, 6, 4, "#9fd0ff"); r(2, 1, 4, 1, "#cfe8ff"); r(2, 3, 1, 2, "#5e8db0"); break;
    case "sugar": r(1, 1, 6, 6, "#ffffff"); r(1, 5, 6, 2, "#d8d0c8"); r(5, 1, 2, 6, "#e8e0d8"); break;
    case "egg": r(2, 1, 4, 6, "#fff5e6"); r(1, 2, 6, 4, "#fff5e6"); break;
    case "target": r(1, 3, 6, 4, "#8a5a3e"); r(2, 1, 4, 2, "#a8743e"); r(3, 4, 2, 2, "#1a1210"); break;
    case "star": r(3, 0, 2, 8, GOLD); r(0, 3, 8, 2, GOLD); r(1, 1, 6, 6, GOLD); break;
    case "star0": r(3, 0, 2, 8, "#5a4a3a"); r(0, 3, 8, 2, "#5a4a3a"); r(1, 1, 6, 6, "#5a4a3a"); break;
    case "pause": r(1, 1, 2, 6, INK); r(5, 1, 2, 6, INK); break;
    case "play": r(2, 1, 1, 6, INK); r(3, 2, 1, 4, INK); r(4, 3, 1, 2, INK); break;
    default: r(1, 1, 6, 6, kind || DIM);
  }
}
function casteIcon(c, x, y) { box(x, y, 6, 6, CASTES[c].col); box(x + 2, y + 2, 2, 2, "#120b07"); }

// ---------- top bar and side counters ----------
function drawTopBar() {
  box(0, 0, SCR_W, TOP_H, "#120d0a");
  box(0, TOP_H - 1, SCR_W, 1, LINE);
  let x = 2;
  const item = (ic, s, col) => { icon(ic, x, 3); txt(s, x + 10, 3, col || INK); x += 12 + txtW(s) + 6; };
  item("ant", String(G.ants.length - 1));
  item("food", String(totalFood()), foodSpace() <= 0 ? GOLD : INK);
  item("chitin", String(totalChitin()));
  item("sugar", String(G.sugar));
  // targets
  const alive = G.bases.filter((b) => b.alive).length;
  txt((G.bases.length - alive) + "/" + G.bases.length, SCR_W - 44, 3, alive ? INK : GREEN, 8, "right");
  icon("target", SCR_W - 42, 3);
  // speed / pause
  box(SCR_W - 30, 1, 14, 12, G.speed > 1 ? "#4a3a2c" : "#2a201a");
  txt(G.speed > 1 ? "x" + G.speed : ">", SCR_W - 23, 3, G.speed > 1 ? GOLD : INK, 8, "center");
  box(SCR_W - 15, 1, 14, 12, G.paused || G.build ? "#5a2a1e" : "#2a201a");
  icon(G.paused || G.build ? "play" : "pause", SCR_W - 12, 3);
}
function drawSide() {
  if (!UI.side) return;
  const groups = [["builder", "builder"], ["loader", "loader"], ["scout", "scout"], ["warrior", "warrior"], ["guardian", "guardian"]];
  let y = VIEW_Y + 3;
  box(1, y - 1, 26, groups.length * 9 + 12, "rgba(18,13,10,0.7)");
  for (const [c] of groups) {
    const n = G.ants.filter((a) => a.caste === c || (c === "warrior" && a.caste === "shooter")).length;
    casteIcon(c, 3, y + 1); txt(String(n), 11, y, INK); y += 9;
  }
  icon("egg", 3, y); txt(String(G.eggs.length), 11, y + 1, INK);
}
function drawToasts() {
  let y = VIEW_Y + 4;
  for (const t of G.toasts) {
    const w = Math.min(SCR_W - 8, txtW(t.txt) + 10);
    ctx.globalAlpha = clamp(t.t * 2, 0, 1);
    box(Math.round((SCR_W - w) / 2), y, w, 12, "rgba(18,13,10,0.88)", LINE);
    txt(fitTxt(t.txt, w - 8), SCR_W / 2, y + 2, t.col, 8, "center");
    ctx.globalAlpha = 1;
    y += 13;
  }
}
// bottom bar: in game 5 buttons; in build mode the tools
const BUILD_TOOLS = [
  { id: "dig", name: "DIG", hint: "Soft 4 s, hard 15 s. Stone: no" },
  { id: "fill", name: "FILL", hint: "1 food per tile. Keeps the queen's way open" },
  { id: "qs", name: "QUEEN", b: "qs" },
  { id: "fs", name: "FOOD", b: "fs" },
  { id: "cs", name: "CHITIN", b: "cs" },
  { id: "br", name: "BARRACK", b: "br" },
  { id: "bb", name: "BIG BAR", b: "bb" },
  { id: "ws", name: "WORKSHP", b: "ws" },
  { id: "remove", name: "REMOVE", hint: "Remove a room (food is not given back)" },
];
function barButtons() {
  if (G.build) {
    const n = BUILD_TOOLS.length, sel = G.build.tool, w = 48;
    const first = clamp(sel - 2, 0, n - 5);
    const out = [];
    for (let k = 0; k < 5; k++) { const i = first + k; const t = BUILD_TOOLS[i]; out.push({ x: k * w, w, label: t.name, sub: t.b ? String(buildCost(t.b)) : "", on: i === sel, act: () => { G.build.tool = i; } }); }
    return out;
  }
  const w = 48;
  return [
    { x: 0, w, label: "ANTS", sub: queueCount() + "/" + queueSlots() * 4, key: "F", act: () => openQueen() },
    { x: w, w, label: "BUILD", key: "C", act: () => toggleBuild() },
    { x: w * 2, w, label: "SQUADS", sub: G.squads.length ? String(G.squads.length) : "", key: "E", act: () => openSquads() },
    { x: w * 3, w, label: G.view === "S" ? "NEST" : "SURFACE", key: "B", act: () => toggleView(), alert: G.alerts.some((a) => a.layer !== G.view) },
    { x: w * 4, w, label: "MENU", key: "D", act: () => openGameMenu() },
  ];
}
function buildCost(kind) { return kind === "qs" ? Math.round(G.Q.qsCost * (1 + QUEEN_RB[G.qcard.rar].qs)) : BUILDS[kind].cost; }
function drawBottomBar() {
  const y0 = SCR_H - BAR_H;
  box(0, y0, SCR_W, BAR_H, "#120d0a");
  box(0, y0, SCR_W, 1, LINE);
  for (const b of barButtons()) {
    box(b.x + 1, y0 + 2, b.w - 2, BAR_H - 4, b.on ? "#5a3a1e" : PANEL2, b.on ? GOLD : LINE);
    txt(b.label, b.x + b.w / 2, y0 + 6, b.on ? GOLD : INK, 8, "center");
    if (b.sub) txt(b.sub, b.x + b.w / 2, y0 + 15, DIM, 8, "center");
    if (b.key) txt(b.key, b.x + 4, y0 + 15, "#6a5a4a", 8);
    if (b.alert && Math.floor(G.t * 3) % 2) box(b.x + b.w - 7, y0 + 4, 4, 4, RED);
  }
  if (G.build) {
    const t = BUILD_TOOLS[G.build.tool];
    const hint = t.b ? BUILDS[t.b].name + " - " + buildCost(t.b) + " food. " + BUILDS[t.b].desc : t.hint;
    box(0, VIEW_Y, SCR_W, 22, "rgba(18,13,10,0.88)");
    txt("BUILD (time stopped)  E/F tool  A use  D done", 3, VIEW_Y + 2, GOLD);
    txt(fitTxt(hint, SCR_W - 6), 3, VIEW_Y + 12, INK);
  }
}
