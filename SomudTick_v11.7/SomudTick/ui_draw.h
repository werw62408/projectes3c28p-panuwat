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

// ---------------- Theme / rotation ----------------
void applyTheme() {
  if (!themeDark) {
    PAPER = 0xF2F2F2; CARD = 0xFFFFFF; SOFT = 0x6E6E6E; LINE = 0xD0D0D0; KEYBG = 0xE8E8E8; MUTE = 0xBDBDBD;
    HDRBG = 0x111111; HDRFG = 0xFFFFFF; HDRSOFT = 0xBDBDBD; ONINK = 0xFFFFFF;
  } else {
    PAPER = 0x000000; CARD = 0x161616; SOFT = 0x9A9A9A; LINE = 0x333333; KEYBG = 0x2A2A2A; MUTE = 0x5A5A5A;
    HDRBG = 0x262626; HDRFG = 0xFFFFFF; HDRSOFT = 0x9A9A9A; ONINK = 0x000000;
  }
  INK = themeDark ? TEXTCOLS[textColIdx].dark : TEXTCOLS[textColIdx].light;
  dirty = true;
}
void applyRotation() {
  lcd.setRotation(rot);
  W = lcd.width(); H = lcd.height();
  FTR_Y = H - 30; CONT_H = FTR_Y - HDR_H;
  spr.deleteSprite();
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("sprite alloc failed (is OPI PSRAM enabled?)");
  scrollY = heatScroll = setScroll = filesScroll = acScroll = 0;
  dirty = true;
}

// ---------------- Drawing helpers ----------------
// English text uses sharp built-in fonts. Text with Thai letters (e.g. a Thai activity name) falls back to the Thai font.
const lgfx::IFont* FS = &fonts::FreeSans9pt7b;
const lgfx::IFont* FB = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* FL = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* FXL = &fonts::FreeSansBold24pt7b;
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
void btn(int x, int y, int w, int h, const String& label, bool on, uint32_t onCol) {
  navAdd(x, y, w, h);   // every button can be reached with the joystick
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
void drawHeader() {
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
  int x0[N_TABS + 1]; footerTabs(x0);
  int cur = tabOf(scr);
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
void drawAppTitle(const String& title, const String& right, bool hasBtn) {
  spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG));
  spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK));
  txt(FS, "Back", 29, HDR_H + 17, INK, D_ML);
  navAdd(6, HDR_H + 4, 64, 26, NK_BACK);
  int rw = 0;
  if (right.length()) { spr.setFont(pickFont(FS, right)); rw = spr.textWidth(right) + 10; txt(FS, right, W - 8, HDR_H + 17, SOFT, D_MR); }
  txt(FB, fitText(FB, title, W - 8 - 78 - (rw ? rw : hasBtn ? 66 : 0)), 78, HDR_H + 17, INK, D_ML);   // hasBtn: leave room for a button on the right
}
bool backHit(int x, int y) { return y >= HDR_H && y < HDR_H + 34 && x < 74; }
