#pragma once
// Stats screen: All / Days / Hours.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Stats ----------------
void drawTri(int x, int y, bool left) {
  if (left) spr.fillTriangle(x + 10, y - 9, x + 10, y + 9, x, y, C(INK));
  else spr.fillTriangle(x, y - 9, x, y + 9, x + 10, y, C(INK));
}
String gapStr(double sec) {   // "1h 44m" (short, so the "Last" time fits next to it)
  long m = (long)(sec / 60);
  return (m >= 60 ? String(m / 60) + "h " : String("")) + String(m % 60) + "m";
}
// Stats has 3 views, picked with the buttons on top:
//   All     = every activity, one line each (tap a line to see Days)
//   Days    = one activity, last 7 days as bars
//   Hours   = what time of day you do things (was the "Hours" app)
int statView = 0, statScroll = 0, statMax = 0;
bool statTotalsOk = false;
const char* STAT_VIEWS[3] = {"All", "Days", "Hours"};
int statTop() { return HDR_H + 36; }
uint32_t stRev = 0; String stDay;   // the logs / day the numbers were counted for
void statsNeed(int days) {   // make sure the numbers for `days` days are ready (count again only if something changed)
  if (st.size() != acts.size() || stRev != logRev || stDay != curDay) statTotalsOk = false;
  if (st.size() == acts.size() && stDays == days && statTotalsOk) return;
  computeStats(days, !statTotalsOk);
  statTotalsOk = true; stRev = logRev; stDay = curDay;
}
// top row: [< Back] [All] [Days] [Hours]. Back goes to Log.
int statTabW() { return (W - 74 - 4 - 6) / 3; }   // 3 buttons after Back (gap 3)
void drawStatTabs() {
  spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG));
  navAdd(6, HDR_H + 4, 64, 26, NK_BACK);
  spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK));
  txt(FS, "Back", 29, HDR_H + 17, INK, D_ML);
  int w = statTabW();
  for (int i = 0; i < 3; i++) btn(74 + i * (w + 3), HDR_H + 4, w, 26, STAT_VIEWS[i], statView == i);
}
void drawStatSummary() {
  statsNeed(7);
  int top = statTop();
  const int RH = 46;
  statMax = max(0, (int)acts.size() * RH + 22 - (FTR_Y - top));
  statScroll = constrain(statScroll, 0, statMax);
  spr.setClipRect(0, top, W, FTR_Y - top);
  int y = top - statScroll;
  txt(FS, "Last 7 days", 10, y + 2, SOFT);
  txt(FS, "today", W - 12, y + 2, SOFT, D_TR);
  y += 22;
  const int bars = 7, bw = 6, bgap = 2, chartW = bars * (bw + bgap);
  for (size_t i = 0; i < acts.size(); i++, y += RH) {
    if (y > FTR_Y || y + RH < top) continue;
    Act& a = acts[i]; ActStat& s = st[i];
    auto val = [&](int di) { return isUnitAct(a) ? s.daily[di] : (float)s.dcnt[di]; };
    spr.fillRoundRect(6, y, W - 12, RH - 4, 10, C(CARD));
    navAdd(6, y, W - 12, RH - 4);   // tap = Days for this activity
    spr.fillRoundRect(6, y, 5, RH - 4, 2, C(a.color));
    float wk = 0, mx = a.goal > 0 ? a.goal : 1;
    for (int d = 0; d < stDays; d++) { wk += val(d); mx = max(mx, val(d)); }
    // right: today big, left of it: 7 small bars
    String td = fmtNum(val(stDays - 1));
    txt(FL, td, W - 14, y + (RH - 4) / 2, a.color, D_MR);
    spr.setFont(FL); int tdw = max(24, (int)spr.textWidth(td));
    int cx = W - 14 - tdw - 10 - chartW, ch = RH - 16;
    for (int d = 0; d < stDays; d++) {
      int h = max(1, (int)(ch * val(d) / mx));
      spr.fillRect(cx + d * (bw + bgap), y + 6 + ch - h, bw, h, C(d == stDays - 1 ? a.color : blend(CARD, a.color, 0.5f)));
    }
    int nameW = cx - 18 - 6;
    txt(FB, fitText(FB, a.name, nameW), 16, y + 5, INK);
    String sub = "avg " + fmtNum(wk / 7.0f) + (isUnitAct(a) ? " " + a.unit : String(""));
    txt(FS, fitText(FS, sub, nameW), 16, y + 23, SOFT);
  }
  scrollBar(top, FTR_Y - top, statScroll, statMax);
  spr.clearClipRect();
}
void drawStatDays() {
  statsNeed(7);
  statSel = constrain(statSel, 0, (int)acts.size() - 1);
  Act& a = acts[statSel];
  ActStat& s = st[statSel];
  const int top = statTop();
  const int selY = top + 12, boxY = top + 28, boxH = land() ? 42 : 52;
  const int infoY = FTR_Y - 20, chartTop = boxY + boxH + 6, chartBot = infoY - 6;
  drawTri(10, selY, true); drawTri(W - 20, selY, false);
  navAdd(0, selY - 12, 44, 24); navAdd(W - 44, selY - 12, 44, 24);   // previous / next activity
  txt(FB, fitText(FB, a.name, W - 70), W / 2, selY, a.color, D_MC);
  auto val = [&](int di) { return isUnitAct(a) ? s.daily[di] : (float)s.dcnt[di]; };
  float wk = 0;
  for (int i = 0; i < stDays; i++) wk += val(i);
  float tot = isUnitAct(a) ? s.total : s.totalCnt;
  // short words so they fit a narrow box: Week = last 7 days, Avg = average per day, Total = all time
  String boxes[4][2] = {{fmtNum(val(stDays - 1)), "Today"}, {fmtNum(wk), "Week"},
                        {fmtNum(wk / 7.0f), "Avg"}, {fmtNum(tot), "Total"}};
  // tall screen: 4 boxes in a row, chart under them. wide screen: 2x2 boxes on the left, chart on the right
  int bw = land() ? 60 : (W - 12) / 4, bh2 = boxH, chL = 6, chT = chartTop;
  if (land()) { bh2 = (chartBot - boxY - 4) / 2; chL = 6 + 2 * bw + 2; chT = boxY; }
  for (int i = 0; i < 4; i++) {
    int x = 6 + (land() ? (i % 2) : i) * bw, y = boxY + (land() ? (i / 2) * (bh2 + 4) : 0);
    spr.fillRoundRect(x, y, bw - 4, bh2, 8, C(CARD));
    int my = y + bh2 / 2;
    txt(FB, fitText(FB, boxes[i][0], bw - 8), x + (bw - 4) / 2, my - 18, a.color, D_TC);
    txt(FS, fitText(FS, boxes[i][1], bw - 5), x + (bw - 4) / 2, my + 3, SOFT, D_TC);
  }
  spr.fillRoundRect(chL, chT, W - 6 - chL, chartBot - chT, 10, C(CARD));
  int cx = chL + 8, cy = chT + 16, cw = W - 6 - chL - 16, ch = chartBot - 20 - cy;
  float mx = a.goal > 0 ? a.goal : 1;
  for (int i = 0; i < stDays; i++) mx = max(mx, val(i));
  int colW = cw / stDays;
  if (a.goal > 0 && a.goalType) {   // dashed goal line first: bars and numbers go on top of it
    int gy = cy + ch - (int)(ch * a.goal / mx);
    for (int x = cx; x < cx + cw; x += 8) spr.drawFastHLine(x, gy, 4, C(INK));
  }
  for (int i = 0; i < stDays; i++) {
    float v = val(i);
    int bh = (int)(ch * v / mx);
    int bx = cx + i * colW + 5, bwid = colW - 10;
    uint32_t col = a.color;
    if (a.goalType == 2 && a.goal > 0 && v > a.goal) col = INK;   // over limit = text-colour bar
    spr.fillRoundRect(bx, cy + ch - bh, bwid, max(bh, 1), 3, C(i == stDays - 1 ? col : blend(CARD, col, 0.6f)));
    if (v > 0) {
      spr.setFont(FS); int nw = spr.textWidth(fmtNum(v));
      spr.fillRect(bx + bwid / 2 - nw / 2 - 1, cy + ch - bh - 15, nw + 2, 13, C(CARD));
      txt(FS, fmtNum(v), bx + bwid / 2, cy + ch - bh - 1, SOFT, lgfx::textdatum::bottom_center);
    }
    struct tm tm = {}; int yy, mm, dd; sscanf(stKeys[i].c_str(), "%d-%d-%d", &yy, &mm, &dd);
    tm.tm_year = yy - 1900; tm.tm_mon = mm - 1; tm.tm_mday = dd; tm.tm_hour = 12; mktime(&tm);
    const char* D1[] = {"S", "M", "T", "W", "T", "F", "S"};   // narrow chart: one letter
    txt(FS, colW < 27 ? D1[tm.tm_wday] : EN_DOW2[tm.tm_wday], bx + bwid / 2, cy + ch + 3, SOFT, D_TC);
  }
  // bottom line: left "Avg gap 1 h 44 min", right "Last 19:12" (or "Last Tue"). Each gets its own half: no overlap
  uint32_t last = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
  String ls = "";
  if (last) { time_t lt = last; struct tm tl; localtime_r(&lt, &tl); ls = "Last " + (dayKey(lt) == curDay ? hhmm(last) : String(EN_DOW[tl.tm_wday])); }
  spr.setFont(FS); int lw = ls.length() ? spr.textWidth(ls) + 10 : 0;
  String gp = s.gapN ? gapStr(s.gapSum / s.gapN) : String("-");
  String gl = "Avg gap " + gp;
  if ((int)spr.textWidth(gl) > W - 20 - lw) gl = "Gap " + gp;   // tall screen: short word, keep the number
  txt(FS, fitText(FS, gl, W - 20 - lw), 10, infoY, SOFT);
  if (ls.length()) txt(FS, ls, W - 10, infoY, SOFT, D_TR);
}
void drawStatHours() {
  statsNeed(heatDays);
  int top = statTop();
  // 7 / 30 days switch on the right of the hour numbers
  spr.fillRoundRect(W - 78, top, 72, 22, 11, C(INK));
  navAdd(W - 78, top, 72, 22);
  txt(FS, String(heatDays) + " days", W - 42, top + 11, ONINK, D_MC);
  const int gx = 64, cw = (W - gx - 6) / 24, rh = land() ? 20 : 24, gtop = top + 42;
  const int hrs[] = {0, 6, 12, 18};
  for (int h : hrs) txt(FS, String(h), gx + h * cw, top + 24, SOFT, D_TL);
  int maxScroll = max(0, (int)acts.size() * rh - (FTR_Y - gtop));
  heatScroll = constrain(heatScroll, 0, maxScroll);
  spr.drawFastVLine(gx + 12 * cw - 1, gtop - 2, FTR_Y - gtop + 2, C(LINE));
  spr.setClipRect(0, gtop, W, FTR_Y - gtop);
  for (size_t i = 0; i < acts.size(); i++) {
    int y = gtop + i * rh - heatScroll;
    if (y > FTR_Y || y + rh < gtop) continue;
    uint16_t mxh = 1; for (int h = 0; h < 24; h++) mxh = max(mxh, st[i].hours[h]);
    txt(FS, fitText(FS, acts[i].name, gx - 8), 6, y + rh / 2 - 2, INK, D_ML);
    for (int h = 0; h < 24; h++) {
      uint16_t c = st[i].hours[h];
      uint32_t col = c ? blend(CARD, acts[i].color, 0.25f + 0.75f * c / mxh) : LINE;
      spr.fillRect(gx + h * cw, y + 2, cw - 1, rh - 8, C(col));
    }
  }
  scrollBar(gtop, FTR_Y - gtop, heatScroll, maxScroll);
  spr.clearClipRect();
}
void drawStats() {
  drawStatTabs();
  if (acts.empty()) { txt(FS, "No activities", W / 2, 120, SOFT, D_MC); return; }
  if (statView == 0) drawStatSummary();
  else if (statView == 1) drawStatDays();
  else drawStatHours();
}
void statsTap(int x, int y) {
  if (y < HDR_H + 32) {   // Back, then the view buttons
    if (x < 74) { goScreen(S_HOME); return; }
    int w = statTabW(), v = (x - 74) / (w + 3);
    if (v >= 0 && v < 3 && v != statView) { statView = v; statScroll = heatScroll = 0; }
    dirty = true; return;
  }
  if (acts.empty()) return;
  int top = statTop();
  if (statView == 0) {
    int i = (y - top - 22 + statScroll) / 46;
    if (y >= top + 22 && i >= 0 && i < (int)acts.size()) { statSel = i; statView = 1; dirty = true; }
  } else if (statView == 1) {
    if (y < top + 26) {
      if (x < 60) statSel = (statSel + acts.size() - 1) % acts.size();
      else if (x > W - 60) statSel = (statSel + 1) % acts.size();
      dirty = true;
    }
  } else if (y < top + 24 && x > W - 84) { heatDays = heatDays == 7 ? 30 : 7; dirty = true; }
}
