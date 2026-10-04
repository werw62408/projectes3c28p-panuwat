#pragma once
// Drawing: colours, theme, screen direction, text and button helpers, header, footer, app title bar.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- สี ----------------
static inline uint16_t C(uint32_t c) { return lgfx::color565((c >> 16) & 255, (c >> 8) & 255, c & 255); }
static uint32_t blend(uint32_t a, uint32_t b, float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  auto ch = [&](int s) { return (uint32_t)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) & 255; };
  return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

// fonts (FS small, FB bold, FL large, FXL numbers only on HUD)
const lgfx::IFont* FS = &fonts::FreeSans9pt7b;
const lgfx::IFont* FB = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* FL = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* FXL = &fonts::FreeSansBold24pt7b;

// ---------------- Theme / rotation ----------------
uint32_t HUDB = 0xFFFFFF, HUDD = 0x6F7C87, HUDW = 0xFF6A2B;   // HUD: bright, dim and warning colour (set in applyTheme)
void applyTheme() {
  if (themeHud) {   // v11.8: black + one colour
    const HudCol& h = HUDCOLS[hudCol];
    PAPER = 0x06080A; CARD = 0x0B0F13; SOFT = h.dim; LINE = blend(PAPER, h.main, 0.32f); KEYBG = 0x141A20; MUTE = blend(PAPER, h.main, 0.45f);
    HDRBG = PAPER; HDRFG = h.main; HDRSOFT = h.dim; ONINK = 0x000000; INK = h.main;
    HUDB = h.bright; HUDD = h.dim; HUDW = h.warn;
    FS = &hS; FB = &hB; FL = &hL; FXL = &hXL;
  } else {
    if (!themeDark) {
      PAPER = 0xF2F2F2; CARD = 0xFFFFFF; SOFT = 0x6E6E6E; LINE = 0xD0D0D0; KEYBG = 0xE8E8E8; MUTE = 0xBDBDBD;
      HDRBG = 0x111111; HDRFG = 0xFFFFFF; HDRSOFT = 0xBDBDBD; ONINK = 0xFFFFFF;
    } else {
      PAPER = 0x000000; CARD = 0x161616; SOFT = 0x9A9A9A; LINE = 0x333333; KEYBG = 0x2A2A2A; MUTE = 0x5A5A5A;
      HDRBG = 0x262626; HDRFG = 0xFFFFFF; HDRSOFT = 0x9A9A9A; ONINK = 0x000000;
    }
    INK = themeDark ? TEXTCOLS[textColIdx].dark : TEXTCOLS[textColIdx].light;
    FS = &fonts::FreeSans9pt7b; FB = &fonts::FreeSansBold9pt7b; FL = &fonts::FreeSansBold12pt7b; FXL = &fonts::FreeSansBold24pt7b;
  }
  dirty = true;
}
// Settings: 0 Light, 1 Dark, 2 HUD
int themeNow() { return themeHud ? 2 : themeDark; }
void themeSet(int t) {
  themeHud = t == 2; if (t < 2) themeDark = t;
  prefs.putUChar("hud", themeHud); prefs.putUChar("dark", themeDark);
  applyTheme();
}
// v12: with the small screen above (clock, date, battery) the top bar goes away, and with the button board
// (B / D = next / previous tab) the bottom tabs go away: the page gets the whole screen (260 -> up to 320 px).
// Only on the pages that place everything from HDR_H / FTR_Y; the others (number pad, keyboard, Sudoku, Deck,
// USB drive, games) keep their bars as before.
uint8_t oledMode = 0;   // Settings > Screen: 0 normal, 1 flipped (upside down in the case), 2 off
uint8_t barsMode = 0;   // Settings > Screen: 0 auto (hide what the extra parts replace), 1 always show the bars
bool barsFreePage(Screen s) {
  switch (s) {
    case S_HOME: case S_STATS: case S_HEAT: case S_APPS: case S_GAMES: case S_SET: case S_FILES: case S_AC:
    case S_NET: case S_WIFI: case S_BT: case S_GBLIST: case S_REMOTE: return true;
    default: return false;
  }
}
void layoutUpdate() {
  bool bare = barsMode == 0 && barsFreePage(scr);
  int hh = (bare && oledOk && oledMode != 2) ? 0 : 30;
  int fy = (bare && padOk) ? H : H - 30;
  if (hh != HDR_H || fy != FTR_Y) { HDR_H = hh; FTR_Y = fy; dirty = true; }
  CONT_H = FTR_Y - HDR_H;
}
void applyRotation() {
  lcd.setRotation(rot);
  W = lcd.width(); H = lcd.height();
  FTR_Y = H - 30; CONT_H = FTR_Y - HDR_H;
  layoutUpdate();
  spr.deleteSprite();
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("sprite alloc failed (is OPI PSRAM enabled?)");
  scrollY = heatScroll = setScroll = filesScroll = acScroll = 0;
  dirty = true;
}

// ---------------- Drawing helpers ----------------
// English text uses sharp built-in fonts. Text with Thai letters (e.g. a Thai activity name) falls back to the Thai font.
// v11.8: on the HUD theme these point to the HUD fonts (applyTheme)
bool hasThai(const String& s) { for (size_t i = 0; i < s.length(); i++) if ((uint8_t)s[i] >= 0x80) return true; return false; }
bool forceTh = false;   // true = draw with the Thai font even if this line has no Thai (keeps wrapped lines the same size)
const lgfx::IFont* pickFont(const lgfx::IFont* f, const String& s) {
  if (!hasThai(s) && (!forceTh || (f != FS && f != FB))) return f;
  return f == FS ? (const lgfx::IFont*)&fS : (const lgfx::IFont*)&fB;
}
void txt(const lgfx::IFont* f, const String& s, int x, int y, uint32_t col, lgfx::textdatum_t d) {
  spr.setFont(pickFont(f, s)); spr.setTextColor(C(col)); spr.setTextDatum(d); spr.drawString(s, x, y);
}
String fitText(const lgfx::IFont* f, String s, int w) {
  spr.setFont(pickFont(f, s));
  if (spr.textWidth(s) <= w) return s;
  while (s.length()) {
    int i = s.length() - 1;
    while (i > 0 && (s[i] & 0xC0) == 0x80) i--;
    s.remove(i);
    if (spr.textWidth(s + "..") <= w) return s + "..";
  }
  return s;
}
// ---------------- v11.8 HUD look: cut corners, tick rulers, segment bars ----------------
String hudUp(const String& s) { if (!themeHud || hasThai(s)) return s; String u = s; u.toUpperCase(); return u; }   // HUD words are in capitals
// a box with the top-left and bottom-right corners cut off. fill / edge < 0: not drawn
void hudShape(int x, int y, int w, int h, int c, int32_t fill, int32_t edge) {
  if (w <= 2 || h <= 2) return;
  c = min(c, min(w, h) / 2);
  if (fill >= 0) for (int r = 0; r < h; r++) {
    int l = x + max(0, c - r), rr = x + w - 1 - max(0, r - (h - 1 - c));
    if (rr >= l) spr.drawFastHLine(l, y + r, rr - l + 1, C(fill));
  }
  if (edge >= 0) {
    uint16_t e = C(edge);
    spr.drawFastHLine(x + c, y, w - c, e); spr.drawFastVLine(x + w - 1, y, h - c, e);
    spr.drawLine(x + w - 1, y + h - 1 - c, x + w - 1 - c, y + h - 1, e);
    spr.drawFastHLine(x, y + h - 1, w - c, e); spr.drawFastVLine(x, y + c, h - c, e);
    spr.drawLine(x, y + c, x + c, y, e);
  }
}
// corner marks just outside a box (the "target" look of the HUD tiles)
void hudBrackets(int x, int y, int w, int h, uint32_t col) {
  uint16_t k = C(col); const int L = 7;
  spr.drawFastHLine(x - 2, y + h + 1, L, k); spr.drawFastVLine(x - 2, y + h + 2 - L, L, k);   // bottom left
  spr.drawFastHLine(x + w + 2 - L, y - 2, L, k); spr.drawFastVLine(x + w + 1, y - 2, L, k);    // top right
}
// a ruler line with small ticks (under the top bar, above the tabs). down = ticks hang down
void hudTicks(int y, bool down, uint32_t col) {
  uint16_t k = C(col);
  spr.drawFastHLine(0, y, W, k);
  for (int x = 4; x < W; x += 8) { int t = (x % 40 == 4) ? 4 : 2; spr.drawFastVLine(x, down ? y : y - t + 1, t, k); }
}
// n slanted segments, the first "on" of them lit
void hudSegs(int x, int y, int w, int h, int n, int on, uint32_t lit, uint32_t off) {
  int gap = 3, sw = (w - gap * (n - 1)) / n, sl = min(4, h / 2);
  for (int i = 0; i < n; i++) {
    int sx = x + i * (sw + gap);
    uint32_t c = i < on ? lit : off;
    for (int r = 0; r < h; r++) spr.drawFastHLine(sx + sl - r * sl / max(1, h - 1), y + r, sw - sl, C(c));
  }
}
// a ring gauge (like a car dial): 270 degrees, filled by f (0..1), a number in the middle
void hudGauge(int cx, int cy, int r, float f, const String& mid, uint32_t col) {
  f = constrain(f, 0.0f, 1.0f);
  spr.fillArc(cx, cy, r, r - 4, 135, 405, C(blend(PAPER, col, 0.22f)));
  if (f > 0.005f) spr.fillArc(cx, cy, r, r - 4, 135, 135 + 270 * f, C(col));
  spr.drawCircle(cx, cy, r - 7, C(blend(PAPER, col, 0.45f)));
  txt((const lgfx::IFont*)&fonts::Font0, mid, cx, cy, col, D_MC);
}
// a card: rounded on Light / Dark, cut corners with a faint edge on HUD
void card(int x, int y, int w, int h, int r, uint32_t fill) {
  if (themeHud) hudShape(x, y, w, h, min(r, 9), fill, LINE); else spr.fillRoundRect(x, y, w, h, r, C(fill));
}
void backBtn() {   // [< Back] at the top left (screens that draw their own title row)
  if (themeHud) { hudShape(6, HDR_H + 4, 64, 26, 6, CARD, INK); spr.fillTriangle(14, HDR_H + 17, 21, HDR_H + 12, 21, HDR_H + 22, C(INK)); txt(&hS, "BACK", 25, HDR_H + 17, INK, D_ML); }
  else { spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG)); spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK)); txt(FS, "Back", 29, HDR_H + 17, INK, D_ML); }
  navAdd(6, HDR_H + 4, 64, 26, NK_BACK);
}
void btn(int x, int y, int w, int h, const String& label0, bool on, uint32_t onCol) {
  String label = hudUp(label0);
  navAdd(x, y, w, h);   // every button can be reached with the joystick
  if (themeHud) {   // cut corners; lit = filled with the HUD colour
    hudShape(x, y, w, h, min(7, h / 4), on ? onCol : CARD, on ? onCol : LINE);
    spr.setFont(pickFont(FB, label));
    bool small = spr.textWidth(label) > w - 8;
    txt(small ? FS : FB, small ? fitText(FS, label, w - 4) : label, x + w / 2, y + h / 2, on ? ONINK : INK, D_MC);
    return;
  }
  spr.fillRoundRect(x, y, w, h, 8, C(on ? onCol : CARD));
  spr.drawRoundRect(x, y, w, h, 8, C(on ? onCol : LINE));
  spr.setFont(pickFont(FB, label));
  bool small = spr.textWidth(label) > w - 8;   // narrow button -> smaller font
  txt(small ? FS : FB, small ? fitText(FS, label, w - 4) : label, x + w / 2, y + h / 2, on ? ONINK : INK, D_MC);
}
void scrollBar(int top, int viewH, int pos, int maxPos) {
  navArea(top, viewH, pos, maxPos);   // the joystick can scroll this area (nav.h)
  if (maxPos <= 0) return;
  int bh = viewH * viewH / (viewH + maxPos);
  int by = top + (viewH - bh) * pos / maxPos;
  if (themeHud) { spr.drawFastVLine(W - 2, top, viewH, C(LINE)); spr.fillRect(W - 3, by, 3, bh, C(INK)); return; }
  spr.fillRoundRect(W - 4, by, 3, bh, 1, C(MUTE));
}
// LovyanGFX drawWideLine() removes the clip box when it finishes (library bug),
// so later drawing could spill over the title. This keeps the clip box.
void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) {
  int32_t cx, cy, cw, ch; spr.getClipRect(&cx, &cy, &cw, &ch);
  spr.drawWideLine(x0, y0, x1, y1, r, c);
  spr.setClipRect(cx, cy, cw, ch);
}
bool hitR(int tx, int ty, int x, int y, int w, int h) { return tx >= x && tx < x + w && ty >= y && ty < y + h; }

int hdrChipX0 = 0, hdrChipX1 = 0;   // where the Home/Uni chip is (for taps)
void drawHudHeader();
void drawHeader() {
  if (HDR_H <= 0) return;   // v12: the clock is on the small screen
  if (themeHud) { drawHudHeader(); return; }
  spr.fillRect(0, 0, W, HDR_H, C(HDRBG));
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  // left: clock
  String ts = (timeApprox ? "~" : "") + hhmm(n);
  txt(FB, ts, 5, HDR_H / 2, timeApprox ? HDRSOFT : HDRFG, D_ML);
  spr.setFont(FB); int left = 5 + spr.textWidth(ts) + 7;
  // right: battery, Wi-Fi dot, place chip (measured, so nothing overlaps)
  int p = batPct();
  String bs = p < 0 ? String("USB") : String(p) + "%";
  txt(FS, bs, W - 4, HDR_H / 2, (p >= 0 && p < 15) ? HDRSOFT : HDRFG, D_MR);
  spr.setFont(FS); int x = W - 4 - spr.textWidth(bs) - 9;
  if (staOk()) spr.fillCircle(x, 15, 3, C(HDRFG)); else if (staOn) spr.drawCircle(x, 15, 3, C(HDRSOFT));
  x -= 8;
  String chip = String(place == 'H' ? "Home" : "Uni") + (autoPlace ? " A" : "");
  spr.setFont(FS); int cw = spr.textWidth(chip) + 14;
  hdrChipX0 = x - cw; hdrChipX1 = x;
  spr.fillRoundRect(hdrChipX0, 5, cw, 20, 10, C(HDRFG));
  txt(FS, chip, hdrChipX0 + cw / 2, 15, HDRBG, D_MC);
  // middle: date (only when the clock is right)
  // the longest text that fits: "Thu 24 Sep" -> "24 Sep" -> "24/9" (tall screen has little room)
  int room = hdrChipX0 - 6 - left;
  String opts[3];
  if (timeApprox) { opts[0] = "time not set"; opts[1] = "set time"; opts[2] = "time?"; }
  else { opts[0] = String(EN_DOW[tm.tm_wday]) + " " + tm.tm_mday + " " + EN_MON[tm.tm_mon]; opts[1] = String(tm.tm_mday) + " " + EN_MON[tm.tm_mon]; opts[2] = String(tm.tm_mday) + "/" + (tm.tm_mon + 1); }
  spr.setFont(FS);
  for (auto& o : opts) if ((int)spr.textWidth(o) <= room) { txt(FS, o, left, HDR_H / 2, timeApprox ? HDRFG : HDRSOFT, D_ML); break; }
}

// v11.8 HUD top bar:  20:15  24 SEP     [HOME] .il 65 [===]   and a ruler under it
void hudBattery(int x, int y, int p, uint32_t col) {   // x = right edge
  spr.drawRect(x - 18, y - 5, 16, 10, C(col)); spr.fillRect(x - 2, y - 2, 2, 4, C(col));
  int seg = p < 0 ? 3 : (p + 20) / 34;   // 0..3 bars
  for (int k = 0; k < 3; k++) if (k < seg) spr.fillRect(x - 16 + k * 4, y - 3, 3, 6, C(col));
}
void hudSignal(int x, int y, bool ok, uint32_t col, uint32_t dim) {   // x = left, y = bottom. Not joined yet: one bar
  for (int k = 0; k < 4; k++) { int h = 3 + k * 2; spr.fillRect(x + k * 3, y - h, 2, h, C(ok || k == 0 ? col : dim)); }
}
void drawHudHeader() {
  spr.fillRect(0, 0, W, HDR_H, C(HDRBG));
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  String ts = (timeApprox ? "~" : "") + hhmm(n);
  txt(&hM, ts, 5, HDR_H / 2 - 1, timeApprox ? HDRSOFT : HUDB, D_ML);
  spr.setFont(&hM); int left = 5 + spr.textWidth(ts) + 7;
  int p = batPct();
  int x = W - 4;
  uint32_t bc = (p >= 0 && p < 15) ? HUDW : HDRFG;
  hudBattery(x, HDR_H / 2 - 1, p, bc); x -= 22;
  String bs = p < 0 ? String("USB") : String(p);
  txt(&hS, bs, x, HDR_H / 2 - 1, bc, D_MR);
  spr.setFont(&hS); x -= spr.textWidth(bs) + 4;
  if (staOn) { hudSignal(x - 11, HDR_H / 2 + 5, staOk(), HDRFG, blend(HDRBG, HDRSOFT, 0.5f)); x -= 15; }
  String chip = String(place == 'H' ? "HOME" : "UNI") + (autoPlace ? " A" : "");
  spr.setFont(&hS); int cw = spr.textWidth(chip) + 14;
  hdrChipX0 = x - cw; hdrChipX1 = x;
  hudShape(hdrChipX0, 5, cw, 20, 5, HDRBG, HDRFG);
  txt(&hS, chip, hdrChipX0 + cw / 2, 15, HUDB, D_MC);
  int room = hdrChipX0 - 6 - left;
  String opts[2];
  if (timeApprox) { opts[0] = "SET TIME"; opts[1] = "TIME?"; }
  else { String mon = EN_MON[tm.tm_mon]; mon.toUpperCase(); opts[0] = String(tm.tm_mday) + " " + mon; opts[1] = String(tm.tm_mday) + "/" + (tm.tm_mon + 1); }
  spr.setFont(&hS);
  for (auto& o : opts) if ((int)spr.textWidth(o) <= room) { txt(&hS, o, left, HDR_H / 2, timeApprox ? HDRFG : HDRSOFT, D_ML); break; }
  hudTicks(HDR_H - 1, true, LINE);
  int mk = W / 3;   // a small marker on the ruler
  spr.fillTriangle(mk - 4, HDR_H - 1, mk + 4, HDR_H - 1, mk, HDR_H + 3, C(HDRFG));
}
// v10: 3 tabs. Stats opens from a button on the Log page, so it belongs to the Log tab.
const int N_TABS = 3;
int tabOf(Screen s) {
  switch (s) { case S_HOME: case S_KEYPAD: case S_STATS: return 0; case S_SET: case S_WIFI: case S_BT: case S_KBD: return 2; default: return 1; }
}
const char* FTR_TABS[N_TABS] = {"Log", "Apps", "Settings"};
void footerTabs(int* x0) {   // x0[0..N_TABS] = tab edges
  x0[0] = 0;
  for (int i = 1; i <= N_TABS; i++) x0[i] = W * i / N_TABS;
}
void drawFooter() {
  if (FTR_Y >= H) return;   // v12: the buttons B / D change the tab
  int x0[N_TABS + 1]; footerTabs(x0);
  int cur = tabOf(scr);
  if (themeHud) {   // v11.8: ruler + capital tabs, the open one filled
    spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(PAPER));
    hudTicks(FTR_Y, false, LINE);
    for (int i = 0; i < N_TABS; i++) {
      bool on = cur == i; int w = x0[i + 1] - x0[i];
      if (on) hudShape(x0[i] + 3, FTR_Y + 4, w - 6, 23, 6, INK, INK);
      navAdd(x0[i] + 3, FTR_Y + 2, w - 6, 26, NK_FOOTER);
      String t = FTR_TABS[i]; t.toUpperCase();
      spr.setFont(&hB); const lgfx::IFont* f = (int)spr.textWidth(t) <= w - 14 ? (const lgfx::IFont*)&hB : (const lgfx::IFont*)&hS;
      txt(f, t, x0[i] + w / 2, FTR_Y + 15, on ? ONINK : SOFT, D_MC);
    }
    return;
  }
  spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(CARD));
  spr.drawFastHLine(0, FTR_Y, W, C(LINE));
  for (int i = 0; i < N_TABS; i++) {
    bool on = cur == i;
    int w = x0[i + 1] - x0[i];
    if (on) spr.fillRoundRect(x0[i] + 3, FTR_Y + 4, w - 6, 22, 8, C(INK));
    navAdd(x0[i] + 3, FTR_Y + 2, w - 6, 26, NK_FOOTER);
    txt(FS, FTR_TABS[i], x0[i] + w / 2, FTR_Y + 15, on ? ONINK : SOFT, D_MC);
  }
}
// small title bar with a Back button, used by app screens
void drawAppTitle(const String& title0, const String& right0, bool hasBtn) {
  String title = hudUp(title0), right = hudUp(right0);
  if (themeHud) {   // [< BACK]  TITLE ..... right
    hudShape(6, HDR_H + 4, 64, 26, 6, CARD, INK);
    spr.fillTriangle(14, HDR_H + 17, 21, HDR_H + 12, 21, HDR_H + 22, C(INK));
    txt(&hS, "BACK", 25, HDR_H + 17, INK, D_ML);
    navAdd(6, HDR_H + 4, 64, 26, NK_BACK);
    int rw = 0;
    if (right.length()) { spr.setFont(pickFont(FS, right)); rw = spr.textWidth(right) + 10; txt(FS, right, W - 8, HDR_H + 17, SOFT, D_MR); }
    txt(FB, fitText(FB, title, W - 8 - 78 - (rw ? rw : hasBtn ? 66 : 0)), 78, HDR_H + 17, HUDB, D_ML);
    return;
  }
  spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG));
  spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK));
  txt(FS, "Back", 29, HDR_H + 17, INK, D_ML);
  navAdd(6, HDR_H + 4, 64, 26, NK_BACK);
  int rw = 0;
  if (right.length()) { spr.setFont(pickFont(FS, right)); rw = spr.textWidth(right) + 10; txt(FS, right, W - 8, HDR_H + 17, SOFT, D_MR); }
  txt(FB, fitText(FB, title, W - 8 - 78 - (rw ? rw : hasBtn ? 66 : 0)), 78, HDR_H + 17, INK, D_ML);   // hasBtn: leave room for a button on the right
}
bool backHit(int x, int y) { return y >= HDR_H && y < HDR_H + 34 && x < 74; }
