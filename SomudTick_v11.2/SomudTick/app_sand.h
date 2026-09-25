#pragma once
// ============================================================================
//  Sand & Water: a calm toy. Drag your finger to pour coloured sand or water.
//  Sand piles up, water flows and fills the gaps, rocks stay where you put them.
//  No score, no time, nothing to lose.
//    Tools (top bar): Sand (tap again = next colour, the last one is rainbow),
//                     Water, Rock, Erase, Empty (the floor opens and everything
//                     falls out).
//    Joystick (optional): tilt left/right = wind, press = Empty.
//  The picture stays while the board is on (it is not saved to memory).
//  Included from SomudTick.ino.
// ============================================================================

const int SA_CELL = 3;            // one grain = 3x3 pixels
const int SA_BAR = 34;            // tool bar height (same as the title bar of other apps)
enum SaTool { ST_SAND, ST_WATER, ST_ROCK, ST_ERASE, ST_EMPTY };
// one byte per cell: bit7 moved this step | bit6-5 type | bit4-2 colour | bit1-0 shade
enum SaType { SAT_NONE = 0, SAT_SAND = 1, SAT_WATER = 2, SAT_ROCK = 3 };
uint8_t* saG = nullptr;           // the grid (in PSRAM)
int saGW = 0, saGH = 0;
int saTool = ST_SAND, saColor = 0;   // saColor 0..7 = one colour, 8 = rainbow
bool saDrain = false; uint32_t saDrainT = 0;
bool saHint = true;               // "how to play" box until the first pour
bool saJoy = false;
uint32_t saFrameT = 0, saFrameN = 0, saRnd = 12345;
uint16_t saPal[128];              // colour of every (type, colour, shade), already byte-swapped for the sprite
const int SA_NCOL = 8;
// sand colours: soft ones on the dark theme, a bit deeper on the light theme (so they show on white)
const uint32_t SA_COLS_DARK[SA_NCOL] = {0xE8C88A, 0xF08A6C, 0xF4A6C8, 0xB8A6F0, 0x8FE0B8, 0x8CC8F0, 0xF2D060, 0xF2F2F2};
const uint32_t SA_COLS_LIGHT[SA_NCOL] = {0xD4A65A, 0xE06A4C, 0xE27BA8, 0x8C73D6, 0x49B487, 0x4A9CD6, 0xE0B030, 0x8E8E8E};
const uint32_t* SA_COLS = SA_COLS_LIGHT;
uint32_t SA_BG = 0xFFFFFF;   // the box you pour in = the card colour of the theme

static inline uint32_t saRand() { saRnd ^= saRnd << 13; saRnd ^= saRnd >> 17; saRnd ^= saRnd << 5; return saRnd; }
static inline uint8_t saType(uint8_t c) { return (c >> 5) & 3; }
static inline uint8_t saMake(uint8_t t, uint8_t col, uint8_t shade) { return (t << 5) | ((col & 7) << 2) | (shade & 3); }
static inline uint16_t saSwap(uint16_t c) { return (c >> 8) | (c << 8); }

void saMakePalette() {
  SA_COLS = themeDark ? SA_COLS_DARK : SA_COLS_LIGHT;
  SA_BG = CARD;
  const float SH[4] = {0.80f, 0.90f, 1.0f, 1.10f};
  const uint32_t WATER[4] = {0x2A63C4, 0x3474D6, 0x4486E4, 0x5A9CF0};
  const uint32_t ROCK[4] = {0x5E636B, 0x6B7079, 0x787E88, 0x868C96};
  for (int i = 0; i < 128; i++) {
    int t = (i >> 5) & 3, col = (i >> 2) & 7, sh = i & 3;
    uint32_t c = SA_BG;
    if (t == SAT_SAND) {
      uint32_t b = SA_COLS[col];
      auto ch = [&](int s) { return (uint32_t)min(255, (int)(((b >> s) & 255) * SH[sh])); };
      c = (ch(16) << 16) | (ch(8) << 8) | ch(0);
    } else if (t == SAT_WATER) c = WATER[sh];
    else if (t == SAT_ROCK) c = ROCK[sh];
    saPal[i] = saSwap(C(c));
  }
}
bool saEmpty() { for (int i = 0; i < saGW * saGH; i++) if (saG[i]) return false; return true; }
void saClear() { if (saG) memset(saG, 0, saGW * saGH); }

void sandOpen() {
  int gw = W / SA_CELL, gh = (H - SA_BAR) / SA_CELL;
  if (!saG || gw != saGW || gh != saGH) {   // first time, or the screen was turned
    if (saG) free(saG);
    saG = (uint8_t*)heap_caps_malloc(gw * gh, MALLOC_CAP_SPIRAM);
    saGW = gw; saGH = gh;
    saClear();
    saHint = true;
  }
  saMakePalette();
  saJoy = joyDetect();
  saDrain = false;
  saRnd ^= esp_random() | 1;
  scr = S_SAND;
  saFrameT = 0;
}
void sandClose() { saDrain = false; scr = S_GAMES; dirty = true; }

// ---------------- pouring ----------------
int saLastGX = -1, saLastGY = -1;
void saBrushAt(int gx, int gy) {
  int r = saTool == ST_ERASE ? 4 : saTool == ST_ROCK ? 2 : 3;
  int n = saTool == ST_SAND ? 7 : saTool == ST_WATER ? 9 : 0;   // grains per frame (sand/water are sprinkled)
  uint8_t col = saColor < SA_NCOL ? saColor : (millis() / 350) % SA_NCOL;
  if (n) {
    for (int k = 0; k < n; k++) {
      int dx = (int)(saRand() % (2 * r + 1)) - r, dy = (int)(saRand() % (2 * r + 1)) - r;
      if (dx * dx + dy * dy > r * r) continue;
      int x = gx + dx, y = gy + dy;
      if (x < 0 || y < 0 || x >= saGW || y >= saGH || saG[y * saGW + x]) continue;
      saG[y * saGW + x] = saTool == ST_SAND ? saMake(SAT_SAND, col, saRand() & 3) : saMake(SAT_WATER, 0, saRand() & 3);
    }
    return;
  }
  for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {   // rock / erase: a solid round brush
    if (dx * dx + dy * dy > r * r) continue;
    int x = gx + dx, y = gy + dy;
    if (x < 0 || y < 0 || x >= saGW || y >= saGH) continue;
    saG[y * saGW + x] = saTool == ST_ROCK ? saMake(SAT_ROCK, 0, saRand() & 3) : 0;
  }
}
// screen point -> brush; rock and erase draw a line from the last point so fast strokes have no gaps
void saPour(int px, int py) {
  int gx = px / SA_CELL, gy = (py - SA_BAR) / SA_CELL;
  if (saTool >= ST_ROCK && saLastGX >= 0) {
    int steps = max(abs(gx - saLastGX), abs(gy - saLastGY));
    for (int s = 1; s <= steps; s++) saBrushAt(saLastGX + (gx - saLastGX) * s / steps, saLastGY + (gy - saLastGY) * s / steps);
  } else saBrushAt(gx, gy);
  saLastGX = gx; saLastGY = gy;
  saHint = false;
}

// ---------------- physics ----------------
// wind: -1..1 (joystick). Sand falls and slides off piles, water also flows sideways.
void saStep(float wind) {
  const int GW = saGW, GH = saGH;
  saFrameN++;
  auto side = [&]() { float r = (saRand() % 1000) / 1000.0f; return r < 0.5f + wind * 0.45f ? 1 : -1; };
  for (int y = GH - 1; y >= 0; y--) {
    bool ltr = (y + saFrameN) & 1;
    uint8_t* row = saG + y * GW;
    for (int i = 0; i < GW; i++) {
      int x = ltr ? i : GW - 1 - i;
      uint8_t c = row[x];
      if (!c || (c & 0x80)) continue;
      uint8_t t = saType(c);
      if (t == SAT_ROCK) continue;
      if (y == GH - 1) { if (saDrain) row[x] = 0; continue; }   // bottom: falls out when the floor is open
      uint8_t* below = row + GW;
      if (t == SAT_SAND) {
        uint8_t b = below[x];
        if (!b) {   // fall (2 cells when there is room: looks less slow at 30 pictures a second)
          if (y + 2 < GH && !below[GW + x]) { below[GW + x] = c | 0x80; row[x] = 0; } else { below[x] = c | 0x80; row[x] = 0; }
          continue;
        }
        if (saType(b) == SAT_WATER) { below[x] = c | 0x80; row[x] = b; continue; }   // sand sinks through water
        int d = side();
        for (int k = 0; k < 2; k++, d = -d) {
          int nx = x + d; if (nx < 0 || nx >= GW) continue;
          uint8_t b2 = below[nx];
          if ((!b2 || saType(b2) == SAT_WATER) && (!row[nx] || saType(row[nx]) == SAT_WATER)) { below[nx] = c | 0x80; row[x] = b2; break; }
        }
      } else {   // water
        uint8_t wc = saMake(SAT_WATER, 0, saRand() & 3) | 0x80;   // moving water sparkles a little
        if (!below[x]) { if (y + 2 < GH && !below[GW + x]) below[GW + x] = wc; else below[x] = wc; row[x] = 0; continue; }
        int d = side(); bool moved = false;
        for (int k = 0; k < 2 && !moved; k++, d = -d) {
          int nx = x + d; if (nx < 0 || nx >= GW) continue;
          if (!below[nx] && !row[nx]) { below[nx] = wc; row[x] = 0; moved = true; }
        }
        for (int k = 0; k < 2 && !moved; k++, d = -d) {   // flow sideways up to 3 cells
          int to = -1;
          for (int s = 1; s <= 3; s++) { int nx = x + d * s; if (nx < 0 || nx >= GW || row[nx]) break; to = nx; }
          if (to >= 0) { row[to] = wc; row[x] = 0; moved = true; }
        }
      }
    }
  }
  for (int i = 0; i < GW * GH; i++) saG[i] &= 0x7F;
}

// ---------------- drawing ----------------
int saBtnW() { return (W - 80) / 5; }
void saToolRect(int k, int& x, int& w) { w = saBtnW(); x = 76 + k * w; }   // k = tool 0..4 (after the Back button)
void saDrawTool(int k, int cx, int cy) {
  switch (k) {
    case ST_SAND:
      if (saColor < SA_NCOL) spr.fillCircle(cx, cy, 8, C(SA_COLS[saColor]));
      else for (int s = 0; s < 6; s++) spr.fillArc(cx, cy, 8, 0, s * 60, s * 60 + 60, C(SA_COLS[s + 1]));
      spr.fillCircle(cx - 3, cy - 3, 2, C(blend(CARD, 0xFFFFFF, 0.7f)));
      break;
    case ST_WATER:
      spr.fillCircle(cx, cy + 3, 6, C(0x4A8FEA));
      spr.fillTriangle(cx - 5, cy + 1, cx + 5, cy + 1, cx, cy - 9, C(0x4A8FEA));
      break;
    case ST_ROCK:
      spr.fillRoundRect(cx - 9, cy - 6, 18, 13, 4, C(0x7A808A));
      spr.drawFastHLine(cx - 5, cy - 1, 7, C(0x5E636B)); spr.drawFastHLine(cx - 2, cy + 3, 8, C(0x5E636B));
      break;
    case ST_ERASE:
      spr.drawCircle(cx, cy, 8, C(0xF4A6C8)); spr.drawCircle(cx, cy, 7, C(0xF4A6C8));
      wideLine(cx - 5, cy + 5, cx + 5, cy - 5, 1.2f, C(0xF4A6C8));
      break;
    case ST_EMPTY:   // floor with a gap and an arrow going down
      spr.fillRect(cx - 10, cy + 5, 6, 3, C(INK)); spr.fillRect(cx + 4, cy + 5, 6, 3, C(INK));
      spr.fillRect(cx - 1, cy - 8, 3, 9, C(INK));
      spr.fillTriangle(cx - 5, cy, cx + 5, cy, cx, cy + 6, C(INK));
      break;
  }
}
void sandDraw() {
  uint16_t* buf = (uint16_t*)spr.getBuffer();
  uint16_t bg = saSwap(C(SA_BG));
  const int GW = saGW, top = SA_BAR;
  for (int y = top; y < H; y++) { uint16_t* p = buf + y * W; for (int x = 0; x < W; x++) p[x] = bg; }
  for (int gy = 0; gy < saGH; gy++) {
    const uint8_t* row = saG + gy * GW;
    for (int gx = 0; gx < GW; gx++) {
      uint8_t c = row[gx]; if (!c) continue;
      uint16_t col = saPal[c & 0x7F];
      uint16_t* p = buf + (top + gy * SA_CELL) * W + gx * SA_CELL;
      for (int k = 0; k < SA_CELL; k++, p += W) { p[0] = col; p[1] = col; p[2] = col; }
    }
  }
  if (!saDrain) spr.fillRect(0, top + saGH * SA_CELL, W, H - top - saGH * SA_CELL, C(LINE));   // the floor
  // tool bar: same Back button as every other app
  spr.fillRect(0, 0, W, SA_BAR, C(PAPER));
  spr.drawFastHLine(0, SA_BAR - 1, W, C(LINE));
  spr.fillRoundRect(6, 4, 64, 26, 8, C(KEYBG));
  spr.fillTriangle(16, 17, 24, 11, 24, 23, C(INK));
  txt(FS, "Back", 29, 17, INK, D_ML);
  for (int k = 0; k < 5; k++) {
    int x, w; saToolRect(k, x, w);
    bool on = k == ST_EMPTY ? saDrain : k == saTool;
    if (on) { spr.fillRoundRect(x + 1, 4, w - 2, 26, 8, C(KEYBG)); spr.fillRect(x + w / 2 - 6, 28, 12, 2, C(INK)); }
    saDrawTool(k, x + w / 2, SA_BAR / 2);
  }
  if (saHint) {
    int bw = W - 24, bh = 150, bx = 12, by = SA_BAR + (H - SA_BAR - bh) / 2;
    spr.fillRoundRect(bx, by, bw, bh, 12, C(PAPER));
    spr.drawRoundRect(bx, by, bw, bh, 12, C(INK));
    auto line = [&](const lgfx::IFont* f, const String& s, int y, uint32_t col) { txt(f, fitText(f, s, bw - 16), W / 2, y, col, D_MC); };
    line(FL, "Sand & Water", by + 22, INK);
    line(FS, "Drag your finger to pour.", by + 52, INK);
    line(FS, "Tap Sand again: colour", by + 74, INK);
    line(FS, "Arrow: empty it all", by + 96, INK);
    line(FS, saJoy ? "Stick: tilt = wind" : "No score, no rush.", by + 124, SOFT);
  }
  spr.pushSprite(0, 0);
}

// ---------------- loop (called from loop() while Sand & Water is open) ----------------
bool saTouchOn = false, saTouchIgnore = false, saTouchCanvas = false;
void sandTap(int x, int y) {   // a touch that started on the tool bar
  if (x < 74) { sandClose(); return; }
  for (int k = 0; k < 5; k++) {
    int tx, tw; saToolRect(k, tx, tw);
    if (x < tx || x >= tx + tw) continue;
    if (k == ST_EMPTY) { saDrain = !saDrain; saDrainT = millis(); }
    else if (k == ST_SAND && saTool == ST_SAND) saColor = (saColor + 1) % (SA_NCOL + 1);
    else saTool = k;
    saHint = false;
  }
}
void sandLoop() {
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (down && !saTouchOn) {   // finger down
    saTouchOn = true; saTouchIgnore = pw == P_OFF; wake();
    saTouchCanvas = tp.y >= SA_BAR; saLastGX = -1;
    if (!saTouchIgnore && !saTouchCanvas) { sandTap(tp.x, tp.y); if (scr != S_SAND) return; }
  } else if (!down && saTouchOn) saTouchOn = false;
  if (down) lastTouchMs = millis();
  if (pw == P_OFF) return;   // screen off: the sand waits
  if (millis() - saFrameT < 33) return;   // about 30 pictures a second
  saFrameT = millis();
  if (down && !saTouchIgnore && saTouchCanvas && tp.y >= SA_BAR) saPour(tp.x, tp.y);
  float wind = 0;
  if (saJoy) {
    float jx = joyAxis(joyRawX(), joyCX), jy = joyAxis(joyRawY(), joyCY);
    if (joyMap & 1) jx = jy;
    if (joyMap & 2) jx = -jx;
    wind = jx;
    if (fabsf(jx) > 0.3f) { lastTouchMs = millis(); if (pw != P_ON) wake(); }
    if (joyPressed()) { saDrain = !saDrain; saDrainT = millis(); saHint = false; wake(); }
  }
  saStep(wind);
  if (saDrain && (millis() - saDrainT > 8000 || saEmpty())) saDrain = false;   // floor closes again by itself
  sandDraw();
}
