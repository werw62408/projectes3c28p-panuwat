"use strict";
// ===== Main loop, keyboard, on-screen pad, test hooks =====

const STEP = 1 / 30;
let last = 0, acc = 0, dirHeld = null, dirT = 0;

function render() {
  ctx.setTransform(PX, 0, 0, PX, 0, 0);
  ctx.imageSmoothingEnabled = false;
  if (APP.mode === "game" && G) {
    drawWorld();
    drawSide();
    drawToasts();
    drawTopBar();
    drawBottomBar();
    if (UI.pick) { box(0, VIEW_Y, SCR_W, 12, "rgba(40,20,60,0.9)"); txt(fitTxt(UI.pick.hint, SCR_W - 6), 3, VIEW_Y + 2, "#e0c8ff"); }
    const stopped = G.paused || G.build || UI.top() || UI.pick;
    if (stopped && !UI.top() && !G.build) txt("PAUSED", SCR_W / 2, VIEW_Y + VIEW_H - 12, GOLD, 8, "center");
  } else {
    drawHomeBg();
  }
  for (const p of UI.stack) drawPanel(p);
  if (APP.mode === "game" && UI.top() && UI.top().small !== undefined) drawToasts();
}
function drawHomeBg() {
  // soil layers with a few tunnels, like looking into an ant farm
  ctx.fillStyle = "#140f0b"; ctx.fillRect(0, 0, SCR_W, SCR_H);
  const t = performance.now() / 1000;
  for (let y = 0; y < SCR_H; y += 4) for (let x = 0; x < SCR_W; x += 4) {
    const h = hashf(x >> 2, y >> 2);
    ctx.fillStyle = h > 0.92 ? "#2a1e16" : h > 0.6 ? "#1d1510" : "#18110d";
    ctx.fillRect(x, y, 4, 4);
  }
  for (let k = 0; k < 7; k++) {
    const x = (k * 53 + t * (8 + k * 3)) % (SCR_W + 40) - 20, y = 250 + Math.sin(k * 1.7 + t * 0.5) * 30;
    drawBug(x, y, 0, 1.6, "#2e1c14", true, t * 12 + k, "ant", false);
  }
}
function frame(now) {
  const dt = Math.min(0.05, (now - (last || now)) / 1000);
  last = now;
  // held direction: repeat for menus and tile steps
  if (dirHeld) { dirT -= dt; if (dirT <= 0) { dirPress(dirHeld); dirT = 0.09; } }
  moveCursor(dt);
  if (APP.mode === "game" && G && !G.over) {
    const stopped = G.paused || G.build || UI.top() || UI.pick;
    if (!stopped) {
      acc += dt * G.speed;
      let n = 0;
      while (acc >= STEP && n < 12) { step(STEP); acc -= STEP; n++; }
      if (n >= 12) acc = 0;
    } else acc = 0;
  }
  if (UI.top()) refreshPanel(UI.top());
  render();
  requestAnimationFrame(frame);
}
function holdDir(d, on) {
  UI.held[d] = on ? 1 : 0;
  if (on) { dirHeld = d; dirT = 0.32; dirPress(d); }
  else if (dirHeld === d) dirHeld = null;
}
const KEYMAP = { ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right" };
const BTNMAP = { z: "A", Z: "A", " ": "A", Enter: "A", x: "D", X: "D", Escape: "D", Backspace: "D", b: "B", B: "B", c: "C", C: "C", e: "E", E: "E", f: "F", F: "F", k: "K", K: "K" };
function onKey(e, down) {
  if (e.target && (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA")) return;
  const d = KEYMAP[e.key];
  if (d) { e.preventDefault(); if (down && !e.repeat) holdDir(d, true); if (!down) holdDir(d, false); return; }
  if (!down) return;
  const b = BTNMAP[e.key];
  if (b) { e.preventDefault(); if (!e.repeat) press(b); return; }
  if (e.key === "+" || e.key === "=") { if (G) { G.zoomV[G.view] = 2; followCursor(); } }
  if (e.key === "-") { if (G) { G.zoomV[G.view] = 1; followCursor(); } }
}
function wirePad() {
  document.querySelectorAll("[data-dir]").forEach((el) => {
    const d = el.dataset.dir;
    el.addEventListener("pointerdown", (e) => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (_) {} holdDir(d, true); });
    const up = () => holdDir(d, false);
    el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up); el.addEventListener("lostpointercapture", up);
  });
  document.querySelectorAll("[data-btn]").forEach((el) => {
    el.addEventListener("pointerdown", (e) => { e.preventDefault(); press(el.dataset.btn); });
  });
}
function boot() {
  initCanvas(document.getElementById("cv"));
  cv.addEventListener("pointerdown", onPointerDown);
  cv.addEventListener("pointermove", onPointerMove);
  cv.addEventListener("pointerup", onPointerUp);
  cv.addEventListener("pointercancel", () => { ptr = null; });
  cv.addEventListener("wheel", onWheel, { passive: false });
  window.addEventListener("keydown", (e) => onKey(e, true));
  window.addEventListener("keyup", (e) => onKey(e, false));
  window.addEventListener("blur", () => { for (const d in UI.held) UI.held[d] = 0; dirHeld = null; });
  wirePad();
  loadMeta();
  openHome();
  requestAnimationFrame(frame);
}
// test hooks (used by the headless tests)
window.AR = {
  get G() { return G; }, get META() { return META; }, UI, APP, press, dirPress, tap, step, holdDir, openHome, beginGame, newRun, initJobs,
  enqueue, markDig, startRaid, newSquad, setOrder, fillSquad, squadMembers, canPlace, addBuilding, uIdx, uX, uY, render, BUILD_TOOLS,
  fast(sec, dt) { dt = dt || STEP; for (let t = 0; t < sec && G && !G.over; t += dt) step(dt); },
};
if (document.fonts && document.fonts.load) document.fonts.load("8px Silkscreen").finally(boot); else boot();
