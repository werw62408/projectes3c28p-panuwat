#pragma once
// Log screen (activity tiles, +1 / -, goals line) and the number pad.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Log (home) ----------------
// Tiles: 2 across on a tall screen, 3 across on a wide screen.
//  ● Name              ✓
//  12 /8
//  2h ago
//  [ - ][   + 1       ]   <- the + button fills up with colour as you get near your goal
const int SUM_H = 30;   // "today" line on top
int homeCols() { return land() ? 3 : 2; }
int tileW() { return (W - 12 - 6 * (homeCols() - 1)) / homeCols(); }
const int TILE_H = 100, TILE_GAP = 6;
void homeTileRect(int i, int& x, int& y) {
  x = 6 + (i % homeCols()) * (tileW() + TILE_GAP);
  y = HDR_H + 4 + SUM_H + (i / homeCols()) * (TILE_H + TILE_GAP) - scrollY;
}
int homeMaxScroll() {
  int rows = ((int)acts.size() + homeCols() - 1) / homeCols();
  int m = SUM_H + rows * (TILE_H + TILE_GAP) + 4 - CONT_H + (logNotice() ? 36 : 0);   // room to scroll above a notice bar
  return m > 0 ? m : 0;
}
int homeTileAt(int x, int y) {
  for (size_t i = 0; i < acts.size(); i++) { int tx, ty; homeTileRect(i, tx, ty); if (hitR(x, y, tx, ty, tileW(), TILE_H)) return i; }
  return -1;
}
void drawCheck(int x, int y, uint32_t c) { wideLine(x, y + 4, x + 3, y + 7, 1.2f, C(c)); wideLine(x + 3, y + 7, x + 9, y, 1.2f, C(c)); }

// ---------------- v11.8 HUD Log: the same places for every button, drawn like a cockpit ----------------
String hudSince(uint32_t last) {   // time since the last log: T+00:42 (hours:minutes), T+3D after 4 days
  if (!last) return "T+--:--";
  uint32_t n = (uint32_t)nowT(); uint32_t d = n > last ? n - last : 0;
  if (d >= 96 * 3600UL) return "T+" + String(d / 86400UL) + "D";
  char b[16]; snprintf(b, sizeof b, "T+%02u:%02u", (unsigned)(d / 3600), (unsigned)(d / 60 % 60)); return b;
}
void hudDiamond(int cx, int cy, int r, uint32_t col, bool fill) {
  if (fill) { spr.fillTriangle(cx - r, cy, cx, cy - r, cx + r, cy, C(col)); spr.fillTriangle(cx - r, cy, cx, cy + r, cx + r, cy, C(col)); }
  else { spr.drawLine(cx - r, cy, cx, cy - r, C(col)); spr.drawLine(cx, cy - r, cx + r, cy, C(col)); spr.drawLine(cx + r, cy, cx, cy + r, C(col)); spr.drawLine(cx, cy + r, cx - r, cy, C(col)); }
}
void drawHudGoals(int y, int done, int goals, bool anyToday) {
  txt(&hS, goals ? "GOALS" : "TODAY", 8, y + SUM_H / 2 - 2, SOFT, D_ML);
  spr.setFont(&hS); int dx = 8 + spr.textWidth(goals ? "GOALS" : "TODAY") + 6;
  if (goals) {
    String t = String(done) + "/" + goals;
    txt(&hM, t, dx, y + SUM_H / 2 - 2, HUDB, D_ML);
    spr.setFont(&hM); dx += spr.textWidth(t) + 10;
  }
  // Stats button: cut corners, a small bar chart
  hudShape(W - 70, y + 1, 64, 24, 6, PAPER, INK);
  navAdd(W - 70, y + 1, 64, 24);   // (the same place as on Light / Dark)
  for (int k = 0; k < 3; k++) spr.fillRect(W - 63 + k * 4, y + 17 - k * 4, 3, 4 + k * 4, C(INK));
  txt(&hS, "STATS", W - 47, y + 13, HUDB, D_ML);
  for (auto& a : acts) {
    if (!(a.goal > 0 && a.goalType) || dx > W - 86) continue;
    int g = goalState(a, measure(a, sumFor(a.id)));
    bool ok = g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && anyToday);
    int cy = y + SUM_H / 2 - 2;
    if (g == 4) { spr.drawLine(dx - 4, cy - 4, dx + 4, cy + 4, C(HUDW)); spr.drawLine(dx - 4, cy + 4, dx + 4, cy - 4, C(HUDW)); }   // over the limit
    else hudDiamond(dx, cy, 5, a.color, ok);
    dx += 15;
  }
}
void drawHudTile(int i, int x, int y, int tw) {
  Act& a = acts[i];
  Sum s = sumFor(a.id);
  float m = measure(a, s);
  int gs = goalState(a, m);
  bool od = overdue(a), over = gs == 4, done = gs == 2;
  bool flash = (int)i == flashIdx && millis() < flashUntil;
  uint32_t edge = over || od ? HUDW : done ? INK : LINE;
  hudShape(x, y, tw, TILE_H, 9, flash ? blend(CARD, INK, 0.25f) : CARD, edge);
  if (done || od || over) hudBrackets(x, y, tw, TILE_H, edge);
  // name, with the activity colour as a small square; a code (A01) or OK on the right
  spr.fillRect(x + 8, y + 10, 5, 5, C(a.color));
  // the name first; the code (A01) only if there is room, "OK" when the goal is reached
  String nm = hudUp(a.name);
  String code = done ? String("OK") : (i < 9 ? "A0" : "A") + String(i + 1);
  spr.setFont(&hS); int cw = spr.textWidth(code) + (done ? 10 : 0);
  spr.setFont(pickFont(FS, nm)); int nw = spr.textWidth(nm);
  bool showCode = nw <= tw - 25 - cw - 6;   // the name comes first (a reached goal still shows by its bright frame)
  if (showCode && done) { hudShape(x + tw - 7 - cw, y + 5, cw, 16, 4, INK, INK); txt(&hS, code, x + tw - 7 - cw / 2, y + 13, ONINK, D_MC); }
  else if (showCode) txt(&hS, code, x + tw - 7, y + 13, SOFT, D_MR);
  txt(FS, fitText(FS, nm, tw - 25 - (showCode ? cw + 6 : 0)), x + 17, y + 13, HUDB, D_ML);
  // the big number, what it is compared to, the time since the last one
  String big = isUnitAct(a) ? fmtNum(s.sum) : String(s.count);
  const lgfx::IFont* bf = &hL; spr.setFont(bf);
  bool gauge = a.goal > 0 && a.goalType;
  int room = tw - 12 - (gauge ? 38 : 4);
  if ((int)spr.textWidth(big) > room) { bf = &hM; spr.setFont(bf); }
  txt(bf, fitText(bf, big, room), x + 8, y + 36, over ? HUDW : HUDB, D_ML);
  String l2;
  if (gauge) l2 = (a.goalType == 1 ? "TGT " : "MAX ") + fmtNum(a.goal);
  if (isUnitAct(a) && a.unit.length()) { String u = hudUp(a.unit); spr.setFont(pickFont(FS, l2 + " " + u)); if (!l2.length() || (int)spr.textWidth(l2 + " " + u) <= room) l2 += (l2.length() ? " " : "") + u; }
  txt(FS, fitText(FS, l2, room), x + 8, y + 55, SOFT, D_ML);
  txt(&hS, fitText(&hS, hudSince(lastSeen.count(a.id) ? lastSeen[a.id] : 0), tw - 16), x + 8, y + 68, od ? HUDW : SOFT, D_ML);
  if (gauge) {
    float f = a.goal > 0 ? m / a.goal : 0;
    int pc = min(999, (int)roundf(f * 100));
    hudGauge(x + tw - 22, y + 44, 16, f, String(pc), over ? HUDW : (done ? HUDB : INK));
  }
  // buttons: the same places as on the other themes ([-] 32 wide, [+] the rest)
  int by = y + TILE_H - 28, bh = 24;
  bool can = canUndo(a, s);
  hudShape(x + 5, by, 32, bh, 5, PAPER, can ? INK : LINE);
  navAdd(x + 5, by, 32, bh, NK_BTN, x + tw / 2, y + 30);
  spr.fillRect(x + 15, by + bh / 2 - 1, 12, 3, C(can ? INK : LINE));
  int px = x + 41, pw_ = tw - 46;
  hudShape(px, by, pw_, bh, 6, INK, INK);
  navAdd(px, by, pw_, bh, NK_BTN, x + tw / 2, y + 30);
  String plus = "+" + fmtNum(isUnitAct(a) ? a.step : 1);
  txt(&hB, fitText(&hB, plus, pw_ - 6), px + pw_ / 2, by + bh / 2, ONINK, D_MC);
}
void drawHome() {
  scrollY = constrain(scrollY, 0, homeMaxScroll());
  spr.setClipRect(0, HDR_H, W, CONT_H);
  // today line: how many goals are done, one dot per activity that has a goal
  {
    int y = HDR_H + 4 - scrollY, goals = 0, done = 0;
    bool anyToday = !todayEv.empty();   // a "max" goal counts as done only on a day you used the board (same as the Garden)
    for (auto& a : acts) if (a.goal > 0 && a.goalType) { goals++; int g = goalState(a, measure(a, sumFor(a.id))); if (g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && anyToday)) done++; }
    if (themeHud) drawHudGoals(y, done, goals, anyToday); else {
    String t = goals ? String("Goals ") + done + " / " + goals : String("Today");
    txt(FB, t, 8, y + SUM_H / 2 - 2, INK, D_ML);
    spr.setFont(FB); int dx = 8 + spr.textWidth(t) + 10;
    // Stats button (Stats used to be a tab)
    spr.fillRoundRect(W - 70, y + 1, 64, 24, 8, C(INK));
    navAdd(W - 70, y + 1, 64, 24);   // Stats button
    for (int k = 0; k < 3; k++) spr.fillRect(W - 64 + k * 4, y + 17 - k * 4, 3, 4 + k * 4, C(ONINK));   // tiny bar chart
    txt(FS, "Stats", W - 50, y + 13, ONINK, D_ML);
    for (auto& a : acts) {
      if (!(a.goal > 0 && a.goalType) || dx > W - 84) continue;
      int g = goalState(a, measure(a, sumFor(a.id)));
      bool ok = g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && anyToday);
      if (g == 4) { spr.fillCircle(dx, y + SUM_H / 2 - 2, 5, C(INK)); spr.fillRect(dx - 3, y + SUM_H / 2 - 3, 7, 2, C(PAPER)); }   // over the limit
      else if (ok) spr.fillCircle(dx, y + SUM_H / 2 - 2, 5, C(a.color));
      else spr.drawCircle(dx, y + SUM_H / 2 - 2, 5, C(a.color));
      dx += 14;
    }
    }
  }
  const int tw = tileW();
  for (size_t i = 0; i < acts.size(); i++) {
    int x, y; homeTileRect(i, x, y);
    if (y > FTR_Y || y + TILE_H < HDR_H) continue;
    if (themeHud) { drawHudTile(i, x, y, tw); continue; }
    Act& a = acts[i];
    Sum s = sumFor(a.id);
    float m = measure(a, s);
    int gs = goalState(a, m);
    bool od = overdue(a), over = gs == 4;
    uint32_t bg = over ? INK : (gs == 2 ? blend(CARD, a.color, 0.18f) : CARD);
    if ((int)i == flashIdx && millis() < flashUntil) bg = blend(CARD, a.color, 0.45f);
    uint32_t fg = over ? ONINK : INK, fg2 = over ? blend(INK, ONINK, 0.7f) : SOFT;
    spr.fillRoundRect(x, y, tw, TILE_H, 12, C(bg));
    uint32_t bd = od ? INK : ((gs == 2 || gs == 3) ? a.color : LINE);
    spr.drawRoundRect(x, y, tw, TILE_H, 12, C(bd));
    if (od || gs == 3) spr.drawRoundRect(x + 1, y + 1, tw - 2, TILE_H - 2, 11, C(bd));
    // name with a colour dot
    spr.fillCircle(x + 12, y + 14, 4, C(a.color));
    bool mark = gs == 2;
    txt(FB, fitText(FB, a.name, tw - 26 - (mark ? 14 : 0)), x + 21, y + 14, fg, D_ML);
    if (mark) drawCheck(x + tw - 18, y + 10, a.color);
    // number + goal / unit
    String big = isUnitAct(a) ? fmtNum(s.sum) : String(s.count);
    txt(FL, big, x + 9, y + 27, over ? ONINK : a.color);
    spr.setFont(FL); int bw = spr.textWidth(big);
    String suf;
    if (a.goal > 0 && a.goalType) suf = (a.goalType == 1 ? "/" : "max ") + fmtNum(a.goal);
    if (isUnitAct(a)) suf += (suf.length() ? " " : "") + a.unit;
    if (suf.length()) {   // "max 500 ml" -> "max 500" -> "/500": never cut the number off
      spr.setFont(FS);
      if ((int)spr.textWidth(suf) > tw - bw - 20 && a.goal > 0 && a.goalType) suf = (a.goalType == 1 ? "/" : "max ") + fmtNum(a.goal);
      if ((int)spr.textWidth(suf) > tw - bw - 20 && a.goal > 0 && a.goalType) suf = "/" + fmtNum(a.goal);
      txt(FS, fitText(FS, suf, tw - bw - 20), x + 13 + bw, y + 34, fg2);
    }
    // goal bar (thin line): fills up to the goal; for a "max" goal it turns dark when you go over
    if (a.goal > 0 && a.goalType) {
      float r = min(1.0f, m / a.goal);
      int bx = x + 9, bw2 = tw - 18, byy = y + 47;
      spr.fillRoundRect(bx, byy, bw2, 4, 2, C(over ? blend(INK, ONINK, 0.35f) : blend(CARD, a.color, 0.25f)));
      if (r > 0) spr.fillRoundRect(bx, byy, max(4, (int)(bw2 * r)), 4, 2, C(over ? ONINK : a.color));
    }
    // time since last
    uint32_t last = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
    {   // "1h 3m ago" -> "1h 3m" -> "1h": no cut words on a narrow tile
      String ag = (od ? "! " : "") + agoStr(last);
      spr.setFont(FS);
      if ((int)spr.textWidth(ag) > tw - 16 && ag.endsWith(" ago")) ag = ag.substring(0, ag.length() - 4);
      if ((int)spr.textWidth(ag) > tw - 16 && ag.indexOf('h') > 0) ag = ag.substring(0, ag.indexOf('h') + 1);
      txt(FS, fitText(FS, ag, tw - 16), x + 9, y + 56, od ? fg : fg2);
    }
    // buttons: [-] undo, [+] add
    int by = y + TILE_H - 28, bh = 24;
    bool can = canUndo(a, s);
    uint32_t mb = over ? blend(INK, ONINK, 0.2f) : KEYBG;
    spr.fillRoundRect(x + 5, by, 32, bh, 8, C(can ? mb : bg));
    navAdd(x + 5, by, 32, bh, NK_BTN, x + tw / 2, y + 30);   // [-]  (hold = type a number, like holding the tile)
    spr.drawRoundRect(x + 5, by, 32, bh, 8, C(over ? blend(INK, ONINK, 0.35f) : LINE));
    spr.fillRect(x + 15, by + bh / 2 - 1, 12, 3, C(can ? fg : (over ? blend(INK, ONINK, 0.35f) : LINE)));
    int px = x + 41, pw_ = tw - 46;
    spr.fillRoundRect(px, by, pw_, bh, 8, C(a.color));
    navAdd(px, by, pw_, bh, NK_BTN, x + tw / 2, y + 30);     // [+1]
    String plus = "+" + fmtNum(isUnitAct(a) ? a.step : 1);
    txt(FB, fitText(FB, plus, pw_ - 6), px + pw_ / 2, by + bh / 2, 0xFFFFFF, D_MC);
  }
  if (acts.empty()) txt(FS, "No activities. Add them on the web page.", W / 2, HDR_H + 60, SOFT, D_MC);
  scrollBar(HDR_H, CONT_H, scrollY, homeMaxScroll());
  spr.clearClipRect();
}

// ---------------- Number pad (tall: keys below / wide: keys on the right) ----------------
const char* KP_KEYS[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "Del"};
KpGeo kpGeo() {
  if (land()) return {140, 36, 56, 38, 60, 43, 124};
  return {6, 108, 72, 40, 78, 45, W - 16};
}
void drawKeypad() {
  if (kpAct < 0 || kpAct >= (int)acts.size()) { scr = S_HOME; return; }
  Act& a = acts[kpAct];
  KpGeo g = kpGeo();
  txt(FB, fitText(FB, hudUp(a.name), g.panelW), 8, 38, themeHud ? HUDB : a.color);
  if (themeHud) hudShape(8, 60, g.panelW, 40, 8, CARD, a.color); else {
  spr.fillRoundRect(8, 60, g.panelW, 40, 10, C(CARD));
  spr.drawRoundRect(8, 60, g.panelW, 40, 10, C(a.color)); }
  String unit = isUnitAct(a) ? a.unit : String("times");
  if (land()) {
    txt(FL, kpVal.length() ? kpVal : String("0"), 8 + g.panelW - 10, 80, INK, D_MR);
    txt(FS, unit, 12, 108, SOFT);
    txt(FS, fitText(FS, "Type a number", g.panelW), 12, 130, SOFT);
  } else {
    // the unit on the right inside the box, the number just left of it (v11.5: a long unit ran off the screen)
    String u = fitText(FS, unit, 100); spr.setFont(pickFont(FS, u)); int ux = 8 + g.panelW - 10 - (int)spr.textWidth(u);
    txt(FS, u, ux, 80, SOFT, D_ML);
    txt(FL, kpVal.length() ? kpVal : String("0"), ux - 8, 80, INK, D_MR);
  }
  for (int k = 0; k < 12; k++) {
    int x = g.x0 + (k % 3) * g.px, y = g.y0 + (k / 3) * g.py;
    navAdd(x, y, g.kw, g.kh);
    if (themeHud) hudShape(x, y, g.kw, g.kh, 7, CARD, LINE); else {
    spr.fillRoundRect(x, y, g.kw, g.kh, 8, C(CARD));
    spr.drawRoundRect(x, y, g.kw, g.kh, 8, C(LINE)); }
    txt(k == 11 ? FB : FL, KP_KEYS[k], x + g.kw / 2, y + g.kh / 2, INK, D_MC);
  }
}
void drawKeypadFooter() {
  int bw = (W - 18) / 2;
  spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(PAPER));
  if (themeHud) {
    hudShape(6, FTR_Y + 2, bw, 26, 6, CARD, LINE); navAdd(6, FTR_Y + 2, bw, 26, NK_BACK); txt(FB, "CANCEL", 6 + bw / 2, FTR_Y + 15, INK, D_MC);
    hudShape(12 + bw, FTR_Y + 2, bw, 26, 6, INK, INK); navAdd(12 + bw, FTR_Y + 2, bw, 26); txt(FB, "SAVE", 12 + bw + bw / 2, FTR_Y + 15, ONINK, D_MC);
    return;
  }
  spr.fillRoundRect(6, FTR_Y + 2, bw, 26, 8, C(KEYBG));
  navAdd(6, FTR_Y + 2, bw, 26, NK_BACK);          // Cancel
  navAdd(12 + bw, FTR_Y + 2, bw, 26);             // Save
  txt(FB, "Cancel", 6 + bw / 2, FTR_Y + 15, INK, D_MC);
  spr.fillRoundRect(12 + bw, FTR_Y + 2, bw, 26, 8, C(INK));
  txt(FB, "Save", 12 + bw + bw / 2, FTR_Y + 15, ONINK, D_MC);
}
