#pragma once
// v12 / v13: extra parts on the I2C wires (all optional, found at start):
//   - small screen (OLED) 1.3" SH1106 128x64, address 0x3C, above the big screen (v13; a 0.96" SSD1306 128x64 also works:
//     Settings > Screen > Small screen size). It shows the clock, the date, Home/Uni, Wi-Fi, the battery, the next reminder
//     and the tabs (Log / Apps / Settings: the open one is lit). The big screen then has no top bar.
//     Big screen off: only the clock, the date and a reminder, dimmed.
//   - PCF8574 + NA011 button board (low level in joystick.h): the buttons below. The big screen then has no tabs.
//       A  = press (like the stick press), hold = long press. v13: A also works in the games.
//       B  = next tab (Log > Apps > Settings)        D = back (on a tab page: the tab before)
//       C  = go to Log                               E = +1 for the reminder shown ("Time for ...")
//       F  = small screen: clock <-> today's goals   hold F 1 s = Home <-> Uni
//     In a pocket (big screen off) any button only wakes the screen.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- the small screen ----------------
#define OLED_ADDR 0x3C
const int OW = 128, OH = 64, OPG = OH / 8;
LGFX_Sprite ospr;                 // 128x64, 1 byte a pixel (8 KB); turned into the screen's 1-bit pages when sent
uint8_t oledSent[OPG][OW];        // what the screen shows now
uint8_t oledKnown = 0;            // bit p: page p of oledSent is really on the screen (then only the changed part is sent)
uint8_t oledContrast = 0, oledFlipNow = 0xFF;
uint8_t oledChip = 0;             // Settings > Screen: 0 = 1.3" SH1106 (its picture starts at column 2), 1 = 0.96" SSD1306
int oledPage = 0;                 // F: 0 clock, 1 today's goals
uint32_t oledT = 0, oledBytes = 0;   // (oledBytes: picture bytes sent, the tests read it)

bool oledCmd(std::initializer_list<uint8_t> c) {
  uint8_t b[32]; uint8_t n = 0; b[n++] = 0x00;   // 0x00 = commands follow
  for (uint8_t v : c) if (n < sizeof b) b[n++] = v;
  return i2cWriteRaw(OLED_ADDR, b, n);
}
// The setup works for both chips (the SH1106 skips the SSD1306-only 0x8D / 0x20 / 0x2E). The screen stays dark:
// it is switched on after the first picture is in (its memory holds noise after power on).
bool oledInit() {
  bool flip = oledMode == 1;
  oledFlipNow = oledMode;
  oledKnown = 0; oledContrast = 0;
  bool ok = oledCmd({0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x02,
                     (uint8_t)(flip ? 0xA0 : 0xA1), (uint8_t)(flip ? 0xC0 : 0xC8),
                     0xDA, 0x12, 0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E});
  if (ok && oledChip == 0) oledCmd({0xAD, 0x8B});   // SH1106: its own power on
  return ok;
}
// one page (8 rows), columns x0..x1. Page mode on both chips: page, then the column (SH1106: 2 more), then the bytes.
bool oledSendRange(int p, const uint8_t* row, int x0, int x1) {
  int c = x0 + (oledChip == 0 ? 2 : 0);
  if (!oledCmd({(uint8_t)(0xB0 | p), (uint8_t)(c & 0x0F), (uint8_t)(0x10 | (c >> 4))})) return false;
  uint8_t b[OW + 1]; b[0] = 0x40;   // 0x40 = picture bytes follow
  int n = x1 - x0 + 1; memcpy(b + 1, row + x0, n);
  if (!i2cWriteRaw(OLED_ADDR, b, n + 1)) return false;
  oledBytes += n;
  return true;
}
// send only what changed (the colon blinking = a few bytes, not the whole screen: no hitch in the games)
bool oledPush(uint8_t pages[OPG][OW]) {
  for (int p = 0; p < OPG; p++) {
    int x0 = 0, x1 = OW - 1;
    if (oledKnown & (1 << p)) {
      while (x0 < OW && pages[p][x0] == oledSent[p][x0]) x0++;
      if (x0 == OW) continue;
      while (x1 > x0 && pages[p][x1] == oledSent[p][x1]) x1--;
    }
    if (!oledSendRange(p, pages[p], x0, x1)) { oledKnown &= ~(1 << p); return false; }   // a loose wire: sent again next time
    memcpy(oledSent[p] + x0, pages[p] + x0, x1 - x0 + 1);
    oledKnown |= 1 << p;
  }
  return true;
}
// 1 byte a pixel (RGB332) -> 8 pages of 8 rows, bit 0 at the top. A pixel counts as lit from half brightness.
void oledToPages(uint8_t pages[OPG][OW]) {
  memset(pages, 0, OPG * OW);
  const uint8_t* px = (const uint8_t*)ospr.getBuffer();
  if (!px) return;
  for (int y = 0; y < OH; y++) {
    const uint8_t* r = px + y * OW; uint8_t bit = 1 << (y & 7); uint8_t* pg = pages[y >> 3];
    for (int x = 0; x < OW; x++) if (((r[x] >> 2) & 7) >= 4) pg[x] |= bit;
  }
}

// text on the small screen: English in sharp fonts, Thai names in the Thai font
// size: 0 = 6x8 letters, 1 = medium bold, 2 = big bold (numbers), 3 = large bold
const lgfx::IFont* ofont(const String& s, int size) {
  if (hasThai(s)) return &fS;
  return size == 3 ? (const lgfx::IFont*)&fonts::FreeSansBold18pt7b : size == 2 ? (const lgfx::IFont*)&fonts::FreeSansBold12pt7b
       : size == 1 ? (const lgfx::IFont*)&fonts::FreeSansBold9pt7b : (const lgfx::IFont*)&fonts::Font0;
}
void otxt(const String& s, int x, int y, lgfx::textdatum_t d, int size = 0, bool ink = true) {
  ospr.setFont(ofont(s, size)); ospr.setTextDatum(d); ospr.setTextColor(ink ? TFT_WHITE : TFT_BLACK); ospr.drawString(s, x, y);
}
int otw(const String& s, int size = 0) { ospr.setFont(ofont(s, size)); return ospr.textWidth(s); }
String ofit(const String& s, int w, int size = 0) {   // cut to w pixels (never in the middle of a Thai letter)
  if (otw(s, size) <= w) return s;
  String t = s;
  while (t.length()) {
    int i = t.length() - 1; while (i > 0 && (t[i] & 0xC0) == 0x80) i--;
    t.remove(i);
    if (otw(t + ".", size) <= w) return t + ".";
  }
  return t;
}
String oUp(String s) { if (!hasThai(s)) s.toUpperCase(); return s; }
// One line of 6x8 English letters around a name that may be Thai, all on one baseline. The Thai letters have marks up
// to 15 dots above the baseline and 3 below (English: 7 above), so a line with a Thai name needs a taller row.
// Only the name is cut. how: 0 = from x, 1 = centred on x, 2 = from x with post at the right end (x + w).
void oLine(const String& pre, const String& name, const String& post, int x, int w, int base, bool ink, int how = 0) {
  int pw = otw(pre), qw = otw(post);
  String n = ofit(name, max(0, w - pw - qw));
  int nw = otw(n), x0 = how == 1 ? x - (pw + nw + qw) / 2 : x;
  const lgfx::textdatum_t BL = lgfx::textdatum::baseline_left;
  otxt(pre, x0, base, BL, 0, ink); otxt(n, x0 + pw, base, BL, 0, ink);
  otxt(post, how == 2 ? x + w - qw : x0 + pw + nw, base, BL, 0, ink);
}
void oBar(int x, int y, int w, int h, float f) {   // a thin bar, filled by f (0..1)
  ospr.drawRect(x, y, w, h, TFT_WHITE);
  int fw = (int)((w - 2) * constrain(f, 0.0f, 1.0f));
  if (fw > 0) ospr.fillRect(x + 1, y + 1, fw, h - 2, TFT_WHITE);
}

void oledSplash() {   // at start, before the screen is switched on
  ospr.fillScreen(TFT_BLACK);
  otxt("SOMUDTICK", OW / 2, 26, lgfx::textdatum::baseline_center, 1);
  otxt(String("FIRMWARE ") + FW_VERSION, OW / 2, 40, D_TC);
  ospr.drawFastHLine(24, 52, 80, TFT_WHITE);
}
// the name of the page on screen, short (for the lit tab when it is not the tab's first page)
const char* oPageName() {
  switch (scr) {
    case S_HOME: return "LOG"; case S_STATS: return "STATS"; case S_KEYPAD: return "TYPE"; case S_HEAT: return "STATS";
    case S_APPS: return "APPS"; case S_GAMES: return "GAMES"; case S_FILES: return "FILES"; case S_AC: return "AC";
    case S_NET: return "NET"; case S_DECK: return "DECK"; case S_USB: return "USB"; case S_GAME: return "SWIM";
    case S_SUDOKU: return "SUDOKU"; case S_SAND: return "SAND"; case S_ANTS: return "ANTS"; case S_MAZE: return "MAZE";
    case S_BLOCKS: return "BLOCKS"; case S_GBLIST: case S_GB: return "GB"; case S_REMOTE: return "REMOTE";
    case S_WIFI: return "WI-FI"; case S_BT: return "BT"; case S_KBD: return "TYPE";
    case S_SET: { static const char* P[4] = {"SET", "SCREEN", "WI-FI", "ABOUT"}; return P[constrain(setPage, 0, 3)]; }
    default: return "";
  }
}
// the tabs at the bottom (rows 53..63): LOG  APPS  SET, the open one lit (white with black letters)
// (the picture moves right by 0..2 dots every few minutes against wear: everything is drawn in columns 0..125 + sx)
const int ORX = 125;   // the right edge before that move
void oledTabs(int sx) {
  const char* T[3] = {"LOG", "APPS", "SET"};
  int cur = tabOf(scr);
  for (int i = 0; i < 3; i++) {
    int x = i * 42 + sx, w = 41;
    if (i == cur) { ospr.fillRect(x, 53, w, 11, TFT_WHITE); otxt(oPageName(), x + w / 2, 55, D_TC, 0, false); }
    else { otxt(T[i], x + w / 2, 55, D_TC); ospr.drawFastHLine(x + 4, 63, w - 8, TFT_WHITE); }
  }
}
// the next reminder: its name and "25M" (false when nothing is coming today)
bool oledNextRemind(String& name, String& in) {
  if (timeApprox) return false;
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  if (tm.tm_hour >= REMIND_TO_H) return false;
  struct tm s = tm; s.tm_hour = REMIND_FROM_H; s.tm_min = 0; s.tm_sec = 0;
  uint32_t base = (uint32_t)mktime(&s), now = (uint32_t)n, best = 0xFFFFFFFF; int bi = -1;
  for (size_t i = 0; i < acts.size(); i++) {
    if (!acts[i].remind) continue;
    uint32_t last = lastSeen.count(acts[i].id) ? lastSeen[acts[i].id] : 0;
    if (last < base) last = base;
    uint32_t due = last + (uint32_t)acts[i].remind * 60;
    if (due > now && due - now < best) { best = due - now; bi = i; }
  }
  if (bi < 0) return false;
  uint32_t m = (best + 59) / 60;
  in = m >= 60 ? String(m / 60) + "H" + (m % 60 ? String(m % 60) + "M" : String("")) : String(m) + "M";
  name = oUp(acts[bi].name);
  return true;
}
// today's goals: how many are reached (the same rule as the Log page)
int oledGoalsDone(int& goals) {
  int done = 0; bool any = !todayEv.empty(); goals = 0;
  for (auto& a : acts) if (a.goal > 0 && a.goalType) { goals++; int g = goalState(a, measure(a, sumFor(a.id))); if (g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && any)) done++; }
  return done;
}
void oledBattery(int rx, int y) {   // rx = right edge: [===] 65%
  int p = batPct();
  String t = p < 0 ? String("USB") : String(p) + "%";
  otxt(t, rx, y, D_TR);
  int x = rx - otw(t) - 15;
  ospr.drawRect(x, y, 12, 8, TFT_WHITE); ospr.fillRect(x + 12, y + 2, 2, 4, TFT_WHITE);
  int seg = p < 0 ? 3 : (p + 20) / 34;
  for (int k = 0; k < 3; k++) if (k < seg) ospr.fillRect(x + 2 + k * 3, y + 2, 2, 4, TFT_WHITE);
}
void oledClock(int sx, int sy, bool blink) {   // the clock page
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  // row 3 first: what is next (a reminder that is due is lit, blinking)
  String pre, name, post;
  bool due = remindAct >= 0 && remindAct < (int)acts.size(), lit = due && (tm.tm_sec & 1);
  if (due) { pre = "TIME FOR "; name = oUp(acts[remindAct].name); post = padOk ? " E:+1" : ""; }
  else if (timeApprox) pre = "SET THE TIME: WI-FI";
  else if (oledNextRemind(name, post)) { pre = "NEXT "; post = " IN " + post; }
  else { int goals, done = oledGoalsDone(goals); if (goals) pre = "GOALS " + String(done) + "/" + goals; }
  // A Thai name needs a taller row 3 (21 dots): then the clock is smaller and the date is on 2 lines, nothing goes away
  bool th = hasThai(name);
  int big = th ? 2 : 3, base = th ? 17 : 26, r2 = th ? 20 : 30;
  String t = hhmm(n);
  otxt(t, sx, base + sy, lgfx::textdatum::baseline_left, big);
  if (blink && !timeApprox && (tm.tm_sec & 1)) {   // the colon blinks: you see at a glance that it runs (only the colon is
    int cx = sx + otw(t.substring(0, 2), big), cw = otw(":", big);   // painted over, so the digits stay put and few bytes are sent)
    ospr.fillRect(cx, sy, cw, base + 2, TFT_BLACK);
  }
  // right: day, date, month (or "TIME NOT SET")
  int rx = ORX + sx;
  String dow = EN_DOW[tm.tm_wday], mon = EN_MON[tm.tm_mon]; dow.toUpperCase(); mon.toUpperCase();
  if (th) {
    otxt(timeApprox ? String("TIME") : dow, rx, 1 + sy, D_TR);
    otxt(timeApprox ? String("NOT SET") : String(tm.tm_mday) + " " + mon, rx, 10 + sy, D_TR);
  } else {
    otxt(timeApprox ? String("TIME") : dow, rx, 1 + sy, D_TR);
    otxt(timeApprox ? String("NOT") : String(tm.tm_mday), rx, 10 + sy, D_TR);
    otxt(timeApprox ? String("SET") : mon, rx, 19 + sy, D_TR);
  }
  // row 2: [HOME] Wi-Fi bars ........ battery
  String pl = place == 'H' ? "HOME" : "UNI";
  int pw_ = otw(pl) + 6;
  ospr.fillRect(sx, r2 + sy, pw_, 10, TFT_WHITE); otxt(pl, sx + 3, r2 + 1 + sy, D_TL, 0, false);
  if (staOn) for (int k = 0; k < 3; k++) { int h = 2 + k * 3; if (staOk() || k == 0) ospr.fillRect(sx + pw_ + 5 + k * 4, r2 + 9 + sy - h, 3, h, TFT_WHITE); else ospr.drawRect(sx + pw_ + 5 + k * 4, r2 + 9 + sy - h, 3, h, TFT_WHITE); }
  oledBattery(rx, r2 + 1 + sy);
  // row 3 (only the name is cut, so "IN 25M" / "E:+1" stay in sight)
  int top = th ? 31 : 42, h = th ? 21 : 10;
  if (lit) ospr.fillRect(sx, top + sy, ORX + 1, h, TFT_WHITE);
  oLine(pre, name, post, sx + (due ? 1 : 0), ORX - (due ? 1 : 0), (th ? 48 : 50) + sy, !lit, due ? 2 : 0);
}
void oledGoals(int sx) {   // F: today's goals, "GOALS 2/3" and the activities (a line under each = how far to the goal)
  int goals, done = oledGoalsDone(goals);
  ospr.fillRect(sx, 0, ORX + 1, 10, TFT_WHITE);
  otxt(goals ? "GOALS " + String(done) + "/" + goals : String("TODAY"), sx + 2, 1, D_TL, 0, false);
  otxt(timeApprox ? String("--:--") : hhmm(nowT()), sx + ORX - 1, 1, D_TR, 0, false);   // the time stays in sight
  // the activities with a goal first, then the rest
  std::vector<int> order;
  for (size_t i = 0; i < acts.size(); i++) if (acts[i].goal > 0 && acts[i].goalType) order.push_back(i);
  for (size_t i = 0; i < acts.size(); i++) if (!(acts[i].goal > 0 && acts[i].goalType)) order.push_back(i);
  int y = 12;   // rows of 10 dots (4 fit above the tabs), a Thai name: 20
  for (size_t k = 0; k < order.size(); k++) {
    Act& a = acts[order[k]]; Sum s = sumFor(a.id);
    String nm = oUp(a.name); bool th = hasThai(nm);
    int h = th ? 20 : 10; if (y + h > 54) break;
    String v = isUnitAct(a) ? fmtNum(s.sum) : String(s.count);
    bool goal = a.goal > 0 && a.goalType;
    if (goal) v += (a.goalType == 1 ? "/" : " MAX ") + fmtNum(a.goal);
    int base = y + (th ? 15 : 7);
    otxt(v, sx + ORX, base, lgfx::textdatum::baseline_right);
    oLine("", nm, "", sx, ORX - otw(v) - 6, base, true);
    if (goal) { float f = measure(a, s) / a.goal; int w = (int)(ORX * min(1.0f, f)); if (w > 0) ospr.drawFastHLine(sx, y + h - 2, w, TFT_WHITE); }
    y += h;
  }
}
void oledPop(int sx, uint32_t ms) {   // just saved: "+1 WATER", today's number and a bar to the goal
  Act& a = acts[popAct]; Sum s = sumFor(a.id);
  String nm = oUp(a.name);
  if (hasThai(nm)) {   // "+1" big, the Thai name in the Thai font on the same line
    int pw = otw(popText + " ", 2);
    otxt(popText, sx, 20, lgfx::textdatum::baseline_left, 2);
    oLine("", nm, "", sx + pw, 126 - pw, 20, true);
  } else otxt(ofit(popText + " " + nm, 126, 2), sx, 20, lgfx::textdatum::baseline_left, 2);
  bool goal = a.goal > 0 && a.goalType;
  String g = goal ? (a.goalType == 1 ? " / " : " MAX ") + fmtNum(a.goal) : String("");
  if (isUnitAct(a)) oLine("TODAY " + fmtNum(s.sum) + " ", oUp(a.unit), g, sx, 127, 34, true);   // (the unit may be Thai)
  else oLine("TODAY " + String(s.count), "", g, sx, 127, 34, true);
  if (goal) oBar(sx, 39, 127, 7, measure(a, s) / a.goal);
  (void)ms;
}
// a notice over the screen: big word + a small line (name = a name in it, maybe Thai: then the line sits lower)
void oledNotice(const String& big, const String& small, bool inverted, const String& name = "", const String& post = "") {
  if (inverted) ospr.fillRect(0, 0, OW, 52, TFT_WHITE);
  otxt(ofit(big, 126, 2), OW / 2, 24, lgfx::textdatum::baseline_center, 2, !inverted);
  oLine(small, name, post, OW / 2, 126, hasThai(name) ? 48 : 40, !inverted, 1);
}
void oledOff(int sx, int sy) {   // the big screen is off (in a pocket / on the desk): the clock, the date and a reminder, dim
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  otxt(hhmm(n), OW / 2 + sx, 28 + sy, lgfx::textdatum::baseline_center, 3);
  String d = timeApprox ? String("TIME NOT SET") : String(EN_DOW[tm.tm_wday]) + " " + tm.tm_mday + " " + EN_MON[tm.tm_mon];
  otxt(oUp(d), OW / 2 + sx, 34 + sy, D_TC);
  if (remindAct >= 0 && remindAct < (int)acts.size()) {
    String nm = oUp(acts[remindAct].name); bool th = hasThai(nm);   // (a Thai name: a taller bar)
    ospr.fillRect(0, (th ? 43 : 50) + sy, OW, th ? 21 : 10, TFT_WHITE);
    oLine("TIME FOR ", nm, "", OW / 2, 124, (th ? 59 : 58) + sy, false, 1);
  }
}
bool irLearning(); String irLearnLeft();   // app_ir.h (v13)
void oledTask() {
  if (!oledOk) return;
  uint32_t ms = millis();
  if (ms - oledT < 200) return;   // 5 times a second is enough (and only the changed bytes are sent)
  oledT = ms;
  if (oledFlipNow != oledMode) {   // Settings changed: turned over / off / on
    if (oledMode == 2) { oledCmd({0xAE}); oledFlipNow = 2; return; }
    oledInit(); oledCmd({0xAF});   // (the old picture is still in its memory: no noise; the new one follows below)
  }
  if (oledMode == 2) return;
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  // brightness: low at night and while the big screen is off (an OLED left on the same picture wears in)
  uint8_t want = (pw == P_OFF || tm.tm_hour >= 23 || tm.tm_hour < 6) ? 0x01 : 0xCF;
  if (want != oledContrast && oledCmd({0x81, want})) oledContrast = want;
  // the picture moves by a pixel every few minutes, for the same reason
  int mnt = tm.tm_min, sx = mnt % 3, sy = (mnt / 3) % 2 ? 1 : 0;   // (v13: right only: moving left cut the first letter)
  bool game = scr == S_GAME || scr == S_MAZE || scr == S_BLOCKS || scr == S_GB || scr == S_SAND;
  ospr.fillScreen(TFT_BLACK); ospr.setTextColor(TFT_WHITE);
  bool popNow = popAct >= 0 && popAct < (int)acts.size() && ms - popMs < 2500;
  bool burstNow = burstNoticeMs && ms - burstNoticeMs < 4000;
  if (logFull) {
    oledNotice("LOG FULL", "LOGS ARE NOT SAVED", tm.tm_sec & 1);
    oledTabs(0);
  } else if (irLearning()) {   // v13: learning a remote key
    oledNotice("LEARN IR", "PRESS A KEY  " + irLearnLeft(), false);
    oledTabs(0);
  } else if (pw == P_OFF) oledOff(sx, sy);
  else if (burstNow) {
    bool btnMsg = burstName.startsWith("Button ");   // a loose button (padChatter) or too many logs in a minute
    if (btnMsg) oledNotice("IGNORED", oUp(burstName), false);
    else oledNotice("TOO FAST", "", false, oUp(burstName), " NOT SAVED");
    oledTabs(0);
  } else if (popNow) { oledPop(0, ms); oledTabs(0); }
  else if (oledPage == 1) { oledGoals(sx); oledTabs(sx); }
  else { oledClock(sx, sy, !game); oledTabs(sx); }   // (in a game the colon does not blink: fewer sends)
  static uint8_t pages[OPG][OW];
  oledToPages(pages);
  oledPush(pages);
}

void extBegin() {
  oledMode = prefs.getUChar("oledMode", 0); if (oledMode > 2) oledMode = 0;
  oledChip = prefs.getUChar("oledChip", 0) ? 1 : 0;
  barsMode = prefs.getUChar("barsMode", 0) ? 1 : 0;
  oledOk = oledInit();
  if (oledOk) {
    ospr.setColorDepth(8);
    if (!ospr.createSprite(OW, OH)) oledOk = false;
    else {   // v13: the first picture goes in before the screen is switched on (v12 switched it on first = noise during the start)
      static uint8_t pg[OPG][OW];
      oledSplash(); oledToPages(pg); oledPush(pg);
      if (oledMode != 2) oledCmd({0xAF});
    }
  }
  padBegin();
  joySwIo14 = !padOk;   // v13: with the button board the stick press comes from K, and IO14 is free for the IR receiver
  Serial.printf("small screen: %s, button board: %s (0x%02X)\n", oledOk ? "yes" : "no", padOk ? "yes" : "no", padAddr);
}

// ---------------- the button board: menu actions ----------------
const Screen PAD_TABS[3] = {S_HOME, S_APPS, S_SET};
bool padTabsOk() {   // the pages you may leave by a tab (the same ones that have the tabs; Ants / Deck / USB leave by their Exit)
  return !askUpdate && scr != S_USB && scr != S_DECK && scr != S_ANTS && scr != S_KBD && scr != S_KEYPAD && scr != S_SUDOKU;
}
bool padTopPage() { return scr == S_HOME || scr == S_APPS || (scr == S_SET && setPage == 0); }
void padTab(int d) {
  if (!padTabsOk()) { sfx(SFX_ERR); return; }
  int t = (tabOf(scr) + d + 3) % 3;
  goScreen(PAD_TABS[t]); sfx(SFX_TAP);
}
void padQuickLog() {   // E: +1 for the activity of the reminder shown (the same as [+1] on the reminder bar)
  int i = remindAct;
  if (i < 0 || i >= (int)acts.size()) { sfx(SFX_ERR); return; }
  remindDoneAct = i; remindDoneMs = millis(); remindDoneId = acts[i].id;
  logDefault(i);   // (clears the reminder; the small screen shows "+1 WATER")
  remindAct = -1; dirty = true;
}
void navBackAny();   // nav.h: Back, also when the stick is not used in the menus
void padPress(uint8_t b) {
  switch (b) {
    case PB_A: case PB_K: navHandle(JE_PRESS); break;
    case PB_B: padTab(1); break;
    case PB_C: if (padTabsOk()) { goScreen(S_HOME); sfx(SFX_TAP); } else sfx(SFX_ERR); break;
    case PB_D: if (padTopPage()) padTab(-1); else if (scr == S_SUDOKU) sudokuClose(); else navBackAny(); break;   // (Sudoku has no ring)
    case PB_E: padQuickLog(); break;
    case PB_F: oledPage = 1 - oledPage; oledT = 0; sfx(SFX_TAP); break;
    default: break;
  }
}
void padHold(uint8_t b) {
  if (b == PB_A || b == PB_K) navHandle(JE_HOLD);
  else if (b == PB_F) { place = place == 'H' ? 'U' : 'H'; prefs.putChar("place", place); ledFlash(0xFFFFFF, 120); sfx(SFX_KEY); dirty = true; oledT = 0; }
}
// one press = one action, on letting go (a hold does its own thing once). A button that is still down from before,
// or that woke the screen, does nothing more until it is let go. A stuck button can't repeat anything.
// A loose button that chatters (the Toilet 97 story, v11.9) makes presses no hand makes: the same button 10 times in
// 5 s, or 40 times in a minute (a slow chatter; not counted on the keyboard / number pad pages, where you type fast).
// It is then ignored for 30 s (then 1 min, 2 min .. 32 min if it goes on) and the small screen / the notice bar says so.
// (simulated in tools: a loose contact makes at most ~60 actions an hour, a hand is never stopped)
const int PAD_CHAT_N = 40; const uint32_t PAD_MUTE_MS = 30000;
uint32_t padPressT[PB_N][PAD_CHAT_N]; uint8_t padPressI[PB_N]; uint32_t padMuteUntil[PB_N], padLastT[PB_N]; uint8_t padMutes[PB_N];
bool padChatter(uint8_t b, uint32_t ms) {   // true = this press is ignored
  if (padLastT[b] && ms - padLastT[b] > 120000UL) padMutes[b] = 0;   // quiet for 2 min: forgiven
  padLastT[b] = ms; if (!padLastT[b]) padLastT[b] = 1;
  if (padMuteUntil[b] && (int32_t)(ms - padMuteUntil[b]) < 0) return true;
  padMuteUntil[b] = 0;
  uint8_t& i = padPressI[b];
  uint32_t back10 = padPressT[b][(i + PAD_CHAT_N - 9) % PAD_CHAT_N], back40 = padPressT[b][i];   // the press 10 / 40 back
  padPressT[b][i] = ms; i = (i + 1) % PAD_CHAT_N;
  bool typing = scr == S_KBD || scr == S_KEYPAD;
  bool caught = padMutes[b] > 0;   // it chattered before (and was not quiet for 2 min since): 10 in 30 s is enough
  if ((back10 && ms - back10 < (caught ? 30000UL : 5000UL)) || (!typing && back40 && ms - back40 < 60000UL)) {
    for (auto& t : padPressT[b]) t = 0;   // (after the pause it starts counting again)
    padMuteUntil[b] = ms + (PAD_MUTE_MS << min((int)padMutes[b], 6)); if (!padMuteUntil[b]) padMuteUntil[b] = 1;   // 30 s, 1 min .. 32 min
    if (padMutes[b] < 6) padMutes[b]++;
    static const char* NM[PB_N] = {"A", "B", "C", "D", "E", "F", "K"};
    burstNoticeMs = ms; if (!burstNoticeMs) burstNoticeMs = 1; burstName = String("Button ") + NM[b] + " loose?"; dirty = true;
    sfx(SFX_ERR);
    return true;
  }
  return false;
}
uint8_t padWas = 0, padHeld = 0, padIgnore = 0; uint32_t padDownT[PB_N];
uint32_t padTaskMs = 0;
// v13: a button still held from a game or a window that waited for its own buttons does nothing more when it is let go
// (it used to count as a new press here = a stray tap on the page you came back to)
void padResync() { padPoll(); padWas = padBits; padIgnore |= padBits; padHeld &= padBits; }
void padTask() {   // from loop(), menus only (the games read the buttons themselves)
  if (!padOk) return;
  uint32_t ms = millis();
  if (padTaskMs && ms - padTaskMs > 300) padResync();   // not called for a while (a game, a window): start from what is held now
  padTaskMs = ms ? ms : 1;
  padPoll();
  uint8_t now = padBits & (joyOk ? 0x3F : 0x7F);   // K: read by the stick code when the stick is there
  for (uint8_t b = 0; b < PB_N; b++) {
    uint8_t m = 1 << b;
    bool d = now & m, was = padWas & m;
    if (d && !was) {
      padDownT[b] = ms; padHeld &= ~m; padIgnore &= ~m;
      if (pw == P_OFF || tDown) { padIgnore |= m; if (pw == P_OFF) { wake(); joyNavReset(); } }   // pocket: only wakes
      else lastTouchMs = ms;
    } else if (d && was) {
      lastTouchMs = ms;
      if (!(padIgnore & m) && !(padHeld & m) && ms - padDownT[b] >= (b == PB_F ? 1000UL : 650UL)) { padHeld |= m; wake(); padHold(b); }
    } else if (!d && was) {
      if (!(padIgnore & m) && !(padHeld & m) && ms - padDownT[b] >= 30 && !padChatter(b, ms)) { wake(); padPress(b); }
      padHeld &= ~m; padIgnore &= ~m;
    }
  }
  padWas = now;
}
// the windows that wait in their own loop (photo viewer, "are you sure?", after a video): A = press, D = back, hold A = hold
JoyEv padWaitEvent() {
  if (!padOk) return JE_NONE;
  static uint8_t was = 0, ign = 0; static uint32_t downT = 0, lastMs = 0;
  uint32_t ms = millis();
  padPoll();
  const uint8_t OK = (1 << PB_A) | (joyOk ? 0 : (1 << PB_K)), BK = 1 << PB_D;   // (K: the stick code reads it when there is a stick)
  uint8_t now = padBits & (OK | BK);
  if (!lastMs || ms - lastMs > 300) { was = now; ign = now; }   // a new window: what is held now does nothing until let go
  lastMs = ms;
  JoyEv e = JE_NONE;
  if ((now & OK) && !(was & OK)) downT = ms;
  else if ((now & OK) && !(ign & OK) && ms - downT >= 650) { ign |= now & OK; e = JE_HOLD; }   // held: once
  else if (!(now & OK) && (was & OK) && !(ign & OK) && ms - downT >= 30) e = JE_PRESS;
  if ((was & BK) && !(now & BK) && !(ign & BK)) e = JE_BACK;
  ign &= now;   // a button let go is not ignored any more
  was = now;
  return e;
}
