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
void drawHome() {
  scrollY = constrain(scrollY, 0, homeMaxScroll());
  spr.setClipRect(0, HDR_H, W, CONT_H);
  // today line: how many goals are done, one dot per activity that has a goal
  {
    int y = HDR_H + 4 - scrollY, goals = 0, done = 0;
    bool anyToday = !todayEv.empty();   // a "max" goal counts as done only on a day you used the board (same as the Garden)
    for (auto& a : acts) if (a.goal > 0 && a.goalType) { goals++; int g = goalState(a, measure(a, sumFor(a.id))); if (g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && anyToday)) done++; }
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
  const int tw = tileW();
  for (size_t i = 0; i < acts.size(); i++) {
    int x, y; homeTileRect(i, x, y);
    if (y > FTR_Y || y + TILE_H < HDR_H) continue;
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
  txt(FB, fitText(FB, a.name, g.panelW), 8, 38, a.color);
  spr.fillRoundRect(8, 60, g.panelW, 40, 10, C(CARD));
  spr.drawRoundRect(8, 60, g.panelW, 40, 10, C(a.color));
  String unit = isUnitAct(a) ? a.unit : String("times");
  if (land()) {
    txt(FL, kpVal.length() ? kpVal : String("0"), 8 + g.panelW - 10, 80, INK, D_MR);
    txt(FS, unit, 12, 108, SOFT);
    txt(FS, fitText(FS, "Type a number", g.panelW), 12, 130, SOFT);
  } else {
    txt(FL, kpVal.length() ? kpVal : String("0"), 176, 80, INK, D_MR);
    txt(FS, unit, 184, 80, SOFT, D_ML);
  }
  for (int k = 0; k < 12; k++) {
    int x = g.x0 + (k % 3) * g.px, y = g.y0 + (k / 3) * g.py;
    spr.fillRoundRect(x, y, g.kw, g.kh, 8, C(CARD));
    navAdd(x, y, g.kw, g.kh);
    spr.drawRoundRect(x, y, g.kw, g.kh, 8, C(LINE));
    txt(k == 11 ? FB : FL, KP_KEYS[k], x + g.kw / 2, y + g.kh / 2, INK, D_MC);
  }
}
void drawKeypadFooter() {
  int bw = (W - 18) / 2;
  spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(PAPER));
  spr.fillRoundRect(6, FTR_Y + 2, bw, 26, 8, C(KEYBG));
  navAdd(6, FTR_Y + 2, bw, 26, NK_BACK);          // Cancel
  navAdd(12 + bw, FTR_Y + 2, bw, 26);             // Save
  txt(FB, "Cancel", 6 + bw / 2, FTR_Y + 15, INK, D_MC);
  spr.fillRoundRect(12 + bw, FTR_Y + 2, bw, 26, 8, C(INK));
  txt(FB, "Save", 12 + bw + bw / 2, FTR_Y + 15, ONINK, D_MC);
}
