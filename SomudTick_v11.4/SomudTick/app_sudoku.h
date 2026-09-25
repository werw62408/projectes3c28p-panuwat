#pragma once
// ============================================================================
//  Sudoku app. Play by touch OR by joystick:
//    touch: tap a box -> a number pad pops up -> tap a number.
//    joystick: push to move the orange box, press the stick = open the pad,
//              push to pick a number, press = put it in. Hold the press 1 s = back to Games.
//  Notes = small pencil numbers. Hint = fill the selected box.
//  Wrong numbers turn red and count as a mistake.
//  The game is saved when you leave, so you can continue later.
//  Included from SomudTick.ino.
// ============================================================================
#include "sudoku_core.h"

struct SdkSave {
  uint8_t sol[81], puz[81], cur[81];
  uint16_t notes[81];
  uint8_t level, mistakes, hints, done;
  uint32_t secs;
};
SdkSave sg;
bool sdkHave = false;
int sdkSel = -1;          // selected box
bool sdkPad = false;      // number pad open
bool sdkNotes = false;    // notes mode
bool sdkNewMenu = false;  // choosing level for a new game
uint32_t sdkStartMs = 0;  // when this play session started
bool sdkJoy = false;      // joystick found (checked when the app opens)
bool sdkJoyUsed = false;  // show the joystick cursor on the pad / menu
JoyBtn sdkBtn;            // the stick's button (short press / hold 1 s = leave)
int sdkPadSel = 4;        // joystick cursor on the pad (0..11)
int sdkMenuSel = 0;       // joystick cursor on the level menu (0..2)
const char* SDK_LEVEL_N[3] = {"Easy", "Medium", "Hard"};
const int SDK_KEEP[3] = {40, 32, 26};
const uint32_t SDK_BLUE = 0x2F5FD0, SDK_RED = 0xD03030;

uint32_t sdkSecs() { return sg.secs + (sdkStartMs && !sg.done ? (millis() - sdkStartMs) / 1000 : 0); }
bool sdkDirty = false; uint32_t sdkDirtyMs = 0;   // a change not saved yet (saved every 30 s, on leaving, and when the screen turns off)
void sdkSave() {
  sdkDirty = false;
  if (!sdkHave) return;
  sg.secs = sdkSecs(); if (sdkStartMs) sdkStartMs = millis();
  prefs.putBytes("sdk", &sg, sizeof sg);
}
void sdkNew(int level) {
  sg.level = level;
  sdk::make(sg.sol, sg.puz, SDK_KEEP[level]);
  memcpy(sg.cur, sg.puz, 81);
  memset(sg.notes, 0, sizeof sg.notes);
  sg.mistakes = sg.hints = sg.done = 0; sg.secs = 0;
  sdkHave = true; sdkSel = -1; sdkPad = false; sdkNotes = false; sdkNewMenu = false;
  sdkStartMs = millis();
  sdkSave();
}
void sudokuOpen() {
  scr = S_SUDOKU;
  sdkJoy = joyOk; sdkJoyUsed = false;   // the stick found at start (checking again here fails if the stick is held while opening)
  joyBtnReset(sdkBtn);
  if (!sdkHave) sdkHave = prefs.getBytes("sdk", &sg, sizeof sg) == sizeof sg;
  if (!sdkHave) sdkNewMenu = true;
  sdkStartMs = millis();
  dirty = true;
}
void sudokuClose() { sdkSave(); sdkStartMs = 0; scr = S_GAMES; dirty = true; joyNavReset(); }   // (a press still held from the game does nothing in the menu)

// board size and place:
//   tall screen: tool bar on top, board under it
//   wide screen: no top bar, board uses the full height, buttons on the right side
struct SdkGeo { int x0, y0, cs; };
SdkGeo sdkGeo() {
  if (land()) { int cs = (H - 6) / 9; return {3, 3, cs}; }
  int top = HDR_H + 32;
  int cs = min((W - 6) / 9, (H - top - 4) / 9);
  return {(W - 9 * cs) / 2, top, cs};
}
struct SdkPad { int px, py, bw, bh, gap, pw, ph; };
SdkPad sdkPadGeo() {
  SdkPad p; p.bw = 56; p.bh = land() ? 38 : 40; p.gap = 5;
  p.pw = p.bw * 3 + p.gap * 4; p.ph = p.bh * 4 + p.gap * 5 + 20;
  p.px = land() ? 3 + (sdkGeo().cs * 9 - p.pw) / 2 : (W - p.pw) / 2;   // wide: over the board, buttons stay visible
  p.py = land() ? (H - p.ph) / 2 : max(HDR_H + 32, (H - p.ph) / 2);
  return p;
}
struct SdkMenu { int px, py, pw, ph, by; };
SdkMenu sdkMenuGeo() {
  SdkMenu m; m.pw = min(W - 30, 220); m.ph = sg.done ? 170 : 150; m.px = land() ? 3 + (sdkGeo().cs * 9 - m.pw) / 2 : (W - m.pw) / 2; m.py = (H - m.ph) / 2;
  m.by = m.py + 10 + ((sg.done && !sdkNewMenu) ? 54 : 26);
  return m;
}
bool sdkSolved() { for (int i = 0; i < 81; i++) if (sg.cur[i] != sg.sol[i]) return false; return true; }
void sdkClearNotes(int i, int v) {   // remove note v from the same row, column and 3x3 block
  int r = i / 9, c = i % 9, br = r / 3 * 3, bc = c / 3 * 3;
  uint16_t m = ~(1 << v);
  for (int k = 0; k < 9; k++) { sg.notes[r * 9 + k] &= m; sg.notes[k * 9 + c] &= m; }
  for (int y = 0; y < 3; y++) for (int x = 0; x < 3; x++) sg.notes[(br + y) * 9 + bc + x] &= m;
}
void sdkPlace(int v) {   // v = 0 erase
  int i = sdkSel;
  if (i < 0 || sg.puz[i] || sg.done) return;
  if (v == 0) { sg.cur[i] = 0; sg.notes[i] = 0; return; }
  if (sdkNotes) { if (!sg.cur[i]) sg.notes[i] ^= (1 << v); return; }
  sg.cur[i] = v; sg.notes[i] = 0;
  if (v != sg.sol[i]) sg.mistakes++;
  else sdkClearNotes(i, v);
  if (sdkSolved()) {
    sg.secs = sdkSecs(); sg.done = 1; sdkPad = false;
    char k[8]; snprintf(k, sizeof k, "sdkB%d", sg.level);
    uint32_t b = prefs.getUInt(k, 0);
    if (!b || sg.secs < b) prefs.putUInt(k, sg.secs);
    ledFlash(0x40FF60, 600);
    sdkSave(); return;   // solved: save right away
  }
  if (!sdkDirty) sdkDirtyMs = millis();
  sdkDirty = true;   // saving the whole board to flash on every number wore the flash out
}
String mmss(uint32_t s) { char b[12]; snprintf(b, sizeof b, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60)); return b; }

void sdkHint() {
  if (sdkSel >= 0 && !sg.puz[sdkSel] && sg.cur[sdkSel] != sg.sol[sdkSel] && !sg.done) {
    bool n = sdkNotes; sdkNotes = false; sg.hints++;
    uint8_t before = sg.mistakes; sdkPlace(sg.sol[sdkSel]); sg.mistakes = before; sdkNotes = n;
  }
}
// tool bar. Tall: one row on top [Back] time mistakes [Hint] [New]. Wide: a column on the right.
void sdkToolbar(bool draw, int tx, int ty) {
  if (land()) {
    SdkGeo g = sdkGeo();
    int sx = g.x0 + g.cs * 9 + 6, sw = W - sx - 4;
    auto hitB = [&](int y) { return !draw && hitR(tx, ty, sx, y - 2, sw, 34); };
    if (draw) {
      spr.fillRoundRect(sx, 4, sw, 30, 8, C(KEYBG));
      spr.fillTriangle(sx + 8, 19, sx + 15, 13, sx + 15, 25, C(INK));
      txt(FS, "Back", sx + 19, 19, INK, D_ML);
      txt(FB, mmss(sdkSecs()), sx + sw / 2, 50, INK, D_MC);
      txt(FS, fitText(FS, String(sg.mistakes) + " wrong", sw), sx + sw / 2, 70, sg.mistakes ? SDK_RED : SOFT, D_MC);
      txt(FS, SDK_LEVEL_N[sg.level], sx + sw / 2, 90, SOFT, D_MC);
      btn(sx, 108, sw, 30, "Hint", false);
      btn(sx, 144, sw, 30, "New", sdkNewMenu);
      char k[8]; snprintf(k, sizeof k, "sdkB%d", sg.level);
      uint32_t bst = prefs.getUInt(k, 0);
      txt(FS, fitText(FS, bst ? "Best " + mmss(bst) : String("No best"), sw), sx + sw / 2, 190, SOFT, D_MC);
      spr.drawFastHLine(sx + 8, 204, sw - 16, C(LINE));   // below the line: the clock (the top bar is hidden here)
      spr.drawCircle(sx + sw / 2 - 26, 218, 5, C(SOFT)); spr.drawFastVLine(sx + sw / 2 - 26, 214, 4, C(SOFT)); spr.drawFastHLine(sx + sw / 2 - 26, 218, 3, C(SOFT));   // small clock
      txt(FS, hhmm(nowT()), sx + sw / 2 + 6, 218, SOFT, D_MC);
      return;
    }
    if (hitB(4)) { sudokuClose(); return; }
    if (hitB(108)) sdkHint();
    else if (hitB(144)) sdkNewMenu = !sdkNewMenu;
    dirty = true;
    return;
  }
  int y = HDR_H + 3, h = 26;
  int nw = 46, hw = 46;
  int xNew = W - 4 - nw, xHint = xNew - 4 - hw;
  if (draw) {
    spr.fillRoundRect(6, y, 64, h, 8, C(KEYBG));   // same Back button as every other app
    spr.fillTriangle(16, y + 13, 24, y + 7, 24, y + 19, C(INK));
    txt(FS, "Back", 29, y + 13, INK, D_ML);
    String tm = mmss(sdkSecs());
    txt(FB, tm, 76, y + 13, INK, D_ML);
    spr.setFont(FB); int tw = spr.textWidth(tm);
    txt(FS, "x" + String(sg.mistakes), 76 + tw + 6, y + 13, sg.mistakes ? SDK_RED : SOFT, D_ML);   // x = wrong numbers
    btn(xHint, y, hw, h, "Hint", false);
    btn(xNew, y, nw, h, "New", sdkNewMenu);
    return;
  }
  if (!hitR(tx, ty, 0, y - 3, W, h + 5)) return;
  if (tx < 72) { sudokuClose(); return; }
  if (tx >= xHint && tx < xHint + hw) sdkHint();
  else if (tx >= xNew) sdkNewMenu = !sdkNewMenu;
  dirty = true;
}
bool sdkInToolbar(int x, int y) {
  if (land()) { SdkGeo g = sdkGeo(); return x >= g.x0 + g.cs * 9 + 4; }
  return y < HDR_H + 32 && y >= HDR_H;
}

void drawSudoku() {
  sdkToolbar(true, -1, -1);
  SdkGeo g = sdkGeo();
  int bs = g.cs * 9;
  spr.fillRect(g.x0, g.y0, bs, bs, C(CARD));
  int sel = sdkSel, sv = sel >= 0 ? sg.cur[sel] : 0;
  for (int i = 0; i < 81; i++) {
    int r = i / 9, c = i % 9, x = g.x0 + c * g.cs, y = g.y0 + r * g.cs;
    if (sel >= 0) {   // highlight row, column, block and same numbers
      int sr = sel / 9, sc = sel % 9;
      bool rel = r == sr || c == sc || (r / 3 == sr / 3 && c / 3 == sc / 3);
      if (i == sel) spr.fillRect(x, y, g.cs, g.cs, C(blend(CARD, SDK_BLUE, 0.35f)));
      else if (sv && sg.cur[i] == sv) spr.fillRect(x, y, g.cs, g.cs, C(blend(CARD, SDK_BLUE, 0.2f)));
      else if (rel) spr.fillRect(x, y, g.cs, g.cs, C(blend(CARD, INK, 0.07f)));
    }
    uint8_t v = sg.cur[i];
    if (v) {
      uint32_t col = sg.puz[i] ? INK : (v == sg.sol[i] ? SDK_BLUE : SDK_RED);
      txt(g.cs >= 22 ? (const lgfx::IFont*)&fonts::FreeSans12pt7b : FS, String(v), x + g.cs / 2, y + g.cs / 2 + 1, col, D_MC);   // plain font; given numbers are dark, yours are blue
    } else if (sg.notes[i]) {
      spr.setFont(&fonts::Font0); spr.setTextDatum(D_MC); spr.setTextColor(C(SOFT));
      int t = g.cs / 3;
      for (int n = 1; n <= 9; n++) if (sg.notes[i] & (1 << n))
        spr.drawString(String(n), x + ((n - 1) % 3) * t + t / 2 + 1, y + ((n - 1) / 3) * t + t / 2 + 1);
    }
  }
  for (int k = 0; k <= 9; k++) {   // grid lines, thick every 3
    uint32_t col = k % 3 ? LINE : INK;
    int w = k % 3 ? 1 : 2;
    spr.fillRect(g.x0 + k * g.cs - (w == 2 ? 1 : 0), g.y0, w, bs + 1, C(col));
    spr.fillRect(g.x0, g.y0 + k * g.cs - (w == 2 ? 1 : 0), bs + 1, w, C(col));
  }
  if (sel >= 0) {   // frame around the chosen box: blue by finger, orange by stick (the same orange as every other screen)
    int x = g.x0 + (sel % 9) * g.cs, y = g.y0 + (sel / 9) * g.cs;
    uint32_t fc = sdkJoyUsed ? NAV_RING_C : SDK_BLUE;
    spr.drawRect(x, y, g.cs + 1, g.cs + 1, C(fc));
    spr.drawRect(x + 1, y + 1, g.cs - 1, g.cs - 1, C(fc));
  }
  // number pad pop-up
  if (sdkPad && sdkSel >= 0) {
    SdkPad P = sdkPadGeo();
    int bw = P.bw, bh = P.bh, gap = P.gap, pw_ = P.pw, ph = P.ph, px = P.px, py = P.py;
    spr.fillRoundRect(px - 2, py - 2, pw_ + 4, ph + 4, 12, C(INK));
    spr.fillRoundRect(px, py, pw_, ph, 10, C(PAPER));
    txt(FS, sdkNotes ? "Notes mode" : "Pick a number", px + pw_ / 2, py + 11, SOFT, D_MC);
    for (int k = 0; k < 12; k++) {
      int x = px + gap + (k % 3) * (bw + gap), y = py + 20 + gap + (k / 3) * (bh + gap);
      String lab = k < 9 ? String(k + 1) : (k == 9 ? String("Erase") : k == 10 ? String("Notes") : String("Close"));
      bool on = (k == 10 && sdkNotes) || (k < 9 && sdkNotes && (sg.notes[sdkSel] & (1 << (k + 1))));
      btn(x, y, bw, bh, lab, on);
      if (k < 9) txt(&fonts::FreeSans12pt7b, lab, x + bw / 2, y + bh / 2 + 1, on ? ONINK : INK, D_MC);
      if (sdkJoyUsed && k == sdkPadSel) {   // joystick cursor
        spr.drawRoundRect(x - 2, y - 2, bw + 4, bh + 4, 10, C(NAV_RING_C));
        spr.drawRoundRect(x - 3, y - 3, bw + 6, bh + 6, 11, C(NAV_RING_C));
      }
    }
  }
  if (sdkNewMenu || sg.done) {
    SdkMenu M = sdkMenuGeo();
    int pw_ = M.pw, ph = M.ph, px = M.px, py = M.py;
    spr.fillRoundRect(px - 2, py - 2, pw_ + 4, ph + 4, 12, C(INK));
    spr.fillRoundRect(px, py, pw_, ph, 10, C(PAPER));
    int y = py + 10;
    if (sg.done && !sdkNewMenu) {
      txt(FL, "Solved!", px + pw_ / 2, y + 12, INK, D_MC); y += 30;
      txt(FS, fitText(FS, "Time " + mmss(sg.secs) + "   Mistakes " + String(sg.mistakes), pw_ - 10), px + pw_ / 2, y + 8, SOFT, D_MC); y += 24;
    } else { txt(FB, "New game", px + pw_ / 2, y + 10, INK, D_MC); y += 26; }
    for (int l = 0; l < 3; l++) {
      btn(px + 10, y, pw_ - 20, 30, SDK_LEVEL_N[l], sdkJoyUsed && l == sdkMenuSel);
      y += 36;
    }
  }
}
void sdkPadKey(int k) {   // a key on the number pad was chosen (touch or joystick)
  if (k < 9) { sdkPlace(k + 1); if (!sdkNotes) sdkPad = false; }
  else if (k == 9) { sdkPlace(0); sdkPad = false; }
  else if (k == 10) sdkNotes = !sdkNotes;
  else sdkPad = false;
}
void sdkOpenPad() {
  if (sdkSel < 0 || sg.puz[sdkSel] || sg.done) return;   // fixed numbers can only be selected
  sdkPad = true;
  sdkPadSel = sg.cur[sdkSel] ? sg.cur[sdkSel] - 1 : 4;   // stick cursor starts on the current number (or 5)
}
void sudokuTap(int x, int y) {
  sdkJoyUsed = false;
  if (sdkInToolbar(x, y)) { sdkToolbar(false, x, y); return; }
  if (sdkNewMenu || sg.done) {   // level menu
    SdkMenu M = sdkMenuGeo();
    int yy = M.by;
    for (int l = 0; l < 3; l++) { if (hitR(x, y, M.px + 10, yy, M.pw - 20, 30)) { sdkNew(l); dirty = true; return; } yy += 36; }
    if (!hitR(x, y, M.px, M.py, M.pw, M.ph) && sdkHave && !sg.done) sdkNewMenu = false;
    dirty = true; return;
  }
  if (sdkPad) {
    SdkPad P = sdkPadGeo();
    if (!hitR(x, y, P.px, P.py, P.pw, P.ph)) { sdkPad = false; dirty = true; return; }
    int c = (x - P.px - P.gap) / (P.bw + P.gap), r = (y - P.py - 20 - P.gap) / (P.bh + P.gap);
    if (c < 0 || c > 2 || r < 0 || r > 3) return;
    sdkPadKey(r * 3 + c);
    dirty = true; return;
  }
  SdkGeo g = sdkGeo();
  int c = (x - g.x0) / g.cs, r = (y - g.y0) / g.cs;
  if (x < g.x0 || y < g.y0 || c > 8 || r > 8) return;
  sdkSel = r * 9 + c;
  sdkOpenPad();
  dirty = true;
}

// ---------------- joystick ----------------
// called from loop() while Sudoku is open
void sdkJoyMove(int dx, int dy) {
  if (sdkNewMenu || sg.done) { sdkMenuSel = constrain(sdkMenuSel + dy, 0, 2); return; }
  if (sdkPad) {
    int c = sdkPadSel % 3 + dx, r = sdkPadSel / 3 + dy;
    sdkPadSel = constrain(r, 0, 3) * 3 + constrain(c, 0, 2);
    return;
  }
  if (sdkSel < 0) { sdkSel = 40; return; }   // first push: start in the middle
  int c = (sdkSel % 9 + dx + 9) % 9, r = (sdkSel / 9 + dy + 9) % 9;   // wraps around the edges
  sdkSel = r * 9 + c;
}
void sdkJoyPress() {
  if (sdkNewMenu || sg.done) { sdkNew(sdkMenuSel); return; }
  if (sdkPad) { sdkPadKey(sdkPadSel); return; }
  if (sdkSel < 0) { sdkSel = 40; return; }
  sdkOpenPad();
}
void sudokuJoyTask() {
  if (sdkDirty && millis() - sdkDirtyMs > 30000) sdkSave();
  if (!sdkJoy) return;
  static uint32_t nextT = 0, readT = 0; static int lastDir = 0;
  if (millis() - readT < 20) return;
  readT = millis();
  float jx, jy; joyVec(jx, jy);
  int dir = 0;   // 1 left, 2 right, 3 up, 4 down
  if (fabsf(jx) > 0.55f || fabsf(jy) > 0.55f) dir = fabsf(jx) > fabsf(jy) ? (jx < 0 ? 1 : 2) : (jy < 0 ? 3 : 4);
  static int seenDir = 0, seenN = 0;   // the same direction must be read 3 times in a row (stops jumps from noise)
  if (dir == seenDir) seenN++; else { seenDir = dir; seenN = 1; }
  if (dir && seenN < 3) dir = lastDir == dir ? dir : 0;
  int b = joyBtnRead(sdkBtn);
  bool press = b == 1;
  bool move = false;
  if (!dir) lastDir = 0;
  else if (dir != lastDir) { move = true; lastDir = dir; nextT = millis() + 380; }   // first step right away
  else if (millis() >= nextT) { move = true; nextT = millis() + 140; }              // hold = keep moving
  if (pw == P_OFF) { if (b) wake(); return; }   // screen off (in a pocket): only a press wakes it, and does nothing else
  if (b == 2) { sudokuClose(); return; }        // hold the press 1 s = back to Games
  if (!move && !press) return;
  if (pw != P_ON) { wake(); return; }   // dimmed: the first use only brightens the screen
  lastTouchMs = millis();
  sdkJoyUsed = true;
  if (move) sdkJoyMove(dir == 1 ? -1 : dir == 2 ? 1 : 0, dir == 3 ? -1 : dir == 4 ? 1 : 0);
  if (press) sdkJoyPress();
  dirty = true;
}

// line under each tile on the Games page
String gardenSub();   // in app_garden.h
String mazeSub(); String blocksSub();   // app_maze.h, app_blocks.h
String gameSub(int i) {
  if (i == 0) { int b = prefs.getInt("best", 0); return b ? "Best " + String(b) : String("Joystick"); }   // short: 4 tiles share the page
  if (i == 2) return "Pour & relax";
  if (i == 3) return gardenSub();
  if (i == 4) return mazeSub();
  if (i == 5) return blocksSub();
  if (!sdkHave) sdkHave = prefs.getBytes("sdk", &sg, sizeof sg) == sizeof sg;
  if (sdkHave && !sg.done) return "Go on " + mmss(sg.secs);
  return "New game";
}
