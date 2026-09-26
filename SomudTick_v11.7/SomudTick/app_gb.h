#pragma once
// ============================================================================
//  Game Boy app (v11.7): plays original Game Boy games (.gb) from the SD card.
//  The emulator is Peanut-GB (peanut_gb.h, MIT license, github.com/deltabeard/Peanut-GB).
//  - games: .gb files in /roms on the card (the phone web page: รูป/คลิป > ส่งเกม)
//  - stick = D-pad, press the stick = A (held as long as you hold it)
//  - touch buttons under the picture (at the sides on a wide screen): B, A, SELECT, START
//  - hold the stick press 2 s = the pause menu (Resume / Save and exit). [Pause] [Exit] on the top bar too.
//    (2 s, not 1 s like the other games: many Game Boy games hold A for a while)
//  - the game's own save (battery RAM) is kept in <game>.sav next to the game: written when you leave,
//    and every minute while it changes (switching off loses at most the last minute)
//  - left alone it pauses and the screen turns off as usual; with the screen off a press only wakes it
//  - no sound yet; Game Boy Color-only games don't start (games made for both work, in 4 shades)
//  Included from SomudTick.ino.
// ============================================================================
#pragma GCC push_options
#pragma GCC optimize("O2")   // the emulator runs about twice as fast at -O2 as at the usual -Os
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "peanut_gb.h"
#pragma GCC pop_options

const char* GB_DIR = "/roms";
const int GB_SW = 160, GB_SH = 144;          // Game Boy screen
const int GB_DW = 240, GB_DH = 216;          // on our screen: 1.5 x
const int GB_TOP = 24;                        // black top bar
const uint32_t GB_FRAME_US = 16743;           // 59.73 pictures a second
const uint32_t GB_HOLD_MENU_MS = 2000;
const uint32_t GB_SHADES[4] = {0xE0F8D0, 0x88C070, 0x346856, 0x081820};   // the classic green screen

// ---------------- the list of games ----------------
struct GbRom { String path, name; uint32_t size; };
std::vector<GbRom> gbRoms;
int gbListScroll = 0, gbListMax = 0;
String gbMsg;                                 // shown on the list (a game that could not start)
void gbListLoad() {
  gbRoms.clear();
  if (!sdMount()) return;
  if (!SD_MMC.exists(GB_DIR)) SD_MMC.mkdir(GB_DIR);
  File d = SD_MMC.open(GB_DIR);
  if (!d) return;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    String n = baseOf(f.path());
    if (!f.isDirectory() && n[0] != '.' && lowerExt(n) == "gb") gbRoms.push_back({joinPath(GB_DIR, n), n.substring(0, n.length() - 3), (uint32_t)f.size()});
    f.close();
    if (gbRoms.size() >= 200) break;
  }
  d.close();
  std::sort(gbRoms.begin(), gbRoms.end(), [](const GbRom& a, const GbRom& b) { return a.name.compareTo(b.name) < 0; });
}
void gbListOpen() { scr = S_GBLIST; gbListScroll = 0; gbListLoad(); dirty = true; }
const int GB_ROW_H = 44;
int gbListTop() { return HDR_H + 36 + (gbMsg.length() ? 40 : 0); }
void drawGbList() {
  drawAppTitle("Game Boy", gbRoms.size() ? String(gbRoms.size()) + " games" : String(""));
  if (gbMsg.length()) {
    spr.fillRoundRect(6, HDR_H + 36, W - 12, 36, 8, C(0xFCE3E0));
    txt(FS, fitText(FS, gbMsg, W - 24), 14, HDR_H + 54, 0xB02020, D_ML);
  }
  int top = gbListTop();
  if (gbRoms.empty()) {
    const char* L[] = {sdOk ? "No games yet." : "No SD card.", "Put .gb files in the folder", "/roms on the SD card.",
                       "Phone web page: รูป/คลิป >", "ส่งเกม (.gb)", "Free to try: 2048 (Game Boy)"};
    for (int k = 0; k < 6; k++) txt(k == 0 ? FB : FS, fitText(FS, L[k], W - 24), W / 2, top + 16 + k * 24, k == 0 ? INK : SOFT, D_MC);
    gbListMax = 0; scrollBar(top, FTR_Y - top, 0, 0);
    return;
  }
  gbListScroll = constrain(gbListScroll, 0, gbListMax);
  spr.setClipRect(0, top, W, FTR_Y - top);
  int y = top - gbListScroll;
  for (auto& r : gbRoms) {
    navAdd(6, y, W - 12, GB_ROW_H - 4);
    spr.fillRoundRect(6, y, W - 12, GB_ROW_H - 4, 8, C(CARD));
    spr.fillRoundRect(14, y + 9, 16, 22, 3, C(0x8B8FA0)); spr.fillRect(17, y + 12, 10, 8, C(0xB8D8A8));   // a little cartridge
    txt(FB, fitText(FB, r.name, W - 12 - 44 - 60), 40, y + (GB_ROW_H - 4) / 2, INK, D_ML);
    txt(FS, bytesText(r.size), W - 14, y + (GB_ROW_H - 4) / 2, SOFT, D_MR);
    y += GB_ROW_H;
  }
  gbListMax = max(0, (int)gbRoms.size() * GB_ROW_H - (FTR_Y - top));
  scrollBar(top, FTR_Y - top, gbListScroll, gbListMax);
  spr.clearClipRect();
}
void gbStart(const GbRom& r);
void gbListTap(int x, int y) {
  if (backHit(x, y)) { scr = S_GAMES; gbMsg = ""; dirty = true; return; }
  int top = gbListTop();
  if (y < top || y >= FTR_Y) return;
  int i = (y - top + gbListScroll) / GB_ROW_H;
  if (i >= 0 && i < (int)gbRoms.size() && (y - top + gbListScroll) % GB_ROW_H < GB_ROW_H - 4) gbStart(gbRoms[i]);
}

// ---------------- the emulator ----------------
struct gb_s* gbCore = nullptr;
uint8_t* gbRom = nullptr; uint32_t gbRomSize = 0;
uint8_t* gbRam = nullptr; size_t gbRamSize = 0;
bool gbRamDirty = false; uint32_t gbRamSavedMs = 0;
String gbSavPath, gbName;
uint16_t* gbFb = nullptr;                     // the last picture, 240 x 216 (to draw the menu on top of it)
uint8_t gbXmap[GB_DW];                        // our x -> Game Boy x
uint32_t gbNextUs = 0, gbFrames = 0, gbSlowFrames = 0;
enum GbState : uint8_t { GB_PLAY, GB_PAUSE };
GbState gbState = GB_PLAY;
int gbMenuSel = 0;
bool gbWakeHold = false, gbSwWas = false, gbHoldDone = false; uint32_t gbSwT = 0;
bool gbTouchWas = false;
bool gbBarWas = false, gbMenuTWas = false; int gbBarX = 0, gbMenuTX = 0, gbMenuTY = 0; uint32_t gbMoveT = 0;   // taps in progress
bool gbChromeDirty = true;
uint16_t gbPal[4];
int gbX0() { return (W - GB_DW) / 2; }       // where the picture starts (tall: 0, wide: 40)

uint8_t gbRomRead(struct gb_s*, const uint_fast32_t a) { return a < gbRomSize ? gbRom[a] : 0xFF; }
uint8_t gbRamRead(struct gb_s*, const uint_fast32_t a) { return a < gbRamSize ? gbRam[a] : 0xFF; }
void gbRamWrite(struct gb_s*, const uint_fast32_t a, const uint8_t v) { if (a < gbRamSize && gbRam[a] != v) { gbRam[a] = v; gbRamDirty = true; } }
volatile int gbErr = -1;
void gbError(struct gb_s*, const enum gb_error_e e, const uint16_t) { gbErr = (int)e; }
// one Game Boy line -> 1 or 2 lines on our screen (1.5 x), straight to the screen (and kept in gbFb)
void gbDrawLine(struct gb_s*, const uint8_t px[160], const uint_fast8_t ly) {
  int y0 = ly * 3 / 2, y1 = (ly + 1) * 3 / 2;   // rows y0 .. y1-1 (1 or 2 rows)
  uint16_t* row = gbFb + y0 * GB_DW;
  for (int x = 0; x < GB_DW; x++) row[x] = gbPal[px[gbXmap[x]] & 3];
  for (int y = y0 + 1; y < y1; y++) memcpy(gbFb + y * GB_DW, row, GB_DW * 2);
  lcd.pushImage(gbX0(), GB_TOP + y0, GB_DW, y1 - y0, (const lgfx::rgb565_t*)row);
}
void gbSaveRam() {
  if (!gbRamSize || !gbRam || !gbRamDirty || !sdMount()) return;
  File f = SD_MMC.open(gbSavPath, FILE_WRITE);
  if (f) { f.write(gbRam, gbRamSize); f.close(); gbRamDirty = false; }
  gbRamSavedMs = millis();
}
void gbFree() {
  if (gbCore) { free(gbCore); gbCore = nullptr; }
  if (gbRom) { free(gbRom); gbRom = nullptr; }
  if (gbRam) { free(gbRam); gbRam = nullptr; }
  if (gbFb) { free(gbFb); gbFb = nullptr; }
  gbRomSize = 0; gbRamSize = 0;
}
void gbLeave() {   // save, free the memory, back to the list
  gbSaveRam();
  gbFree();
  scr = S_GBLIST; gbListLoad(); dirty = true; joyNavReset();
}
void gbStart(const GbRom& r) {
  gbMsg = "";
  gbFree();
  if (r.size < 0x8000 || r.size > 4UL * 1048576) { gbMsg = r.size < 0x8000 ? "Not a Game Boy game (too small)." : "Too big (more than 4 MB)."; dirty = true; return; }
  gbRom = (uint8_t*)heap_caps_malloc(r.size, MALLOC_CAP_SPIRAM);
  gbCore = (struct gb_s*)malloc(sizeof(struct gb_s));
  gbFb = (uint16_t*)heap_caps_malloc(GB_DW * GB_DH * 2, MALLOC_CAP_SPIRAM);
  if (!gbRom || !gbCore || !gbFb) { gbFree(); gbMsg = "Not enough memory. Restart the board."; dirty = true; return; }
  File f = SD_MMC.open(r.path, "r");
  size_t got = f ? f.read(gbRom, r.size) : 0;
  if (f) f.close();
  if (got != r.size) { gbFree(); gbMsg = "Can't read the game from the card."; dirty = true; return; }
  gbRomSize = r.size;
  if (gbRom[0x143] == 0xC0) { gbFree(); gbMsg = "Game Boy Color only: can't play it."; dirty = true; return; }
  gbErr = -1;
  if (gb_init(gbCore, gbRomRead, gbRamRead, gbRamWrite, gbError, nullptr) != GB_INIT_NO_ERROR) { gbFree(); gbMsg = "This game type is not supported."; dirty = true; return; }
  size_t rs = 0; gb_get_save_size_s(gbCore, &rs);
  gbRamSize = rs;
  if (gbRamSize) {
    gbRam = (uint8_t*)heap_caps_malloc(gbRamSize, MALLOC_CAP_SPIRAM);
    if (!gbRam) { gbFree(); gbMsg = "Not enough memory. Restart the board."; dirty = true; return; }
    memset(gbRam, 0xFF, gbRamSize);
  }
  gbName = r.name;
  gbSavPath = r.path.substring(0, r.path.length() - 3) + ".sav";
  if (gbRamSize && SD_MMC.exists(gbSavPath)) { File s = SD_MMC.open(gbSavPath, "r"); if (s) { s.read(gbRam, gbRamSize); s.close(); } }
  gbRamDirty = false; gbRamSavedMs = millis();
  gb_init_lcd(gbCore, gbDrawLine);
  gbCore->direct.frame_skip = 1;   // emulate 60 pictures a second, draw every 2nd one (30 a second: the screen can't take more)
  gbCore->direct.interlace = 0;
  { time_t t = nowT(); struct tm tm; localtime_r(&t, &tm); gb_set_rtc(gbCore, &tm); }   // games with a clock (MBC3)
  for (int x = 0; x < GB_DW; x++) gbXmap[x] = x * GB_SW / GB_DW;
  for (int k = 0; k < 4; k++) gbPal[k] = lgfx::color565((GB_SHADES[k] >> 16) & 255, (GB_SHADES[k] >> 8) & 255, GB_SHADES[k] & 255);
  for (int i = 0; i < GB_DW * GB_DH; i++) gbFb[i] = gbPal[0];
  gbState = GB_PLAY; gbMenuSel = 0; gbFrames = gbSlowFrames = 0;
  gbSwWas = joyOk && joyDown(); gbHoldDone = gbSwWas; gbWakeHold = false; gbTouchWas = false;
  gbBarWas = gbMenuTWas = false;
  gbNextUs = micros(); gbChromeDirty = true;
  scr = S_GB; lastTouchMs = millis();
}
// ---------------- the screen around the picture ----------------
struct GbKey { int x, y, w, h; uint8_t bit; const char* t; };
int gbKeys(GbKey* k) {   // the touch buttons
  if (W > H) {   // wide: 40 px on each side
    int h = (H - GB_TOP) / 2;
    k[0] = {0, GB_TOP, 40, h, JOYPAD_SELECT, "SEL"}; k[1] = {0, GB_TOP + h, 40, h, JOYPAD_B, "B"};
    k[2] = {W - 40, GB_TOP, 40, h, JOYPAD_START, "STA"}; k[3] = {W - 40, GB_TOP + h, 40, h, JOYPAD_A, "A"};
  } else {       // tall: a row under the picture
    int y = GB_TOP + GB_DH + 4, h = H - y - 4, w = (W - 20) / 4;
    k[0] = {4, y, w, h, JOYPAD_B, "B"}; k[1] = {8 + w, y, w, h, JOYPAD_A, "A"};
    k[2] = {12 + 2 * w, y, w, h, JOYPAD_SELECT, "SELECT"}; k[3] = {16 + 3 * w, y, w, h, JOYPAD_START, "START"};
  }
  return 4;
}
void gbDrawChrome() {   // everything but the picture (the picture comes from gbFb), plus the pause menu
  spr.fillScreen(C(0x10141C));
  spr.fillRect(0, 0, W, GB_TOP, C(0x000000));
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML);
  int bx = gameBarButtons(GB_TOP, gbState == GB_PLAY);
  spr.drawString(fitText(FB, gbName, bx - 12), 6, GB_TOP / 2);
  GbKey k[4]; int n = gbKeys(k);
  for (int i = 0; i < n; i++) {
    spr.fillRoundRect(k[i].x + 2, k[i].y + 2, k[i].w - 4, k[i].h - 4, 10, C(0x2A3140));
    txt(strlen(k[i].t) > 1 ? FS : FB, k[i].t, k[i].x + k[i].w / 2, k[i].y + k[i].h / 2, 0xDDE3EA, D_MC);
  }
  if (gbFb) spr.pushImage(gbX0(), GB_TOP, GB_DW, GB_DH, (const lgfx::rgb565_t*)gbFb);
  if (gbState == GB_PAUSE) {
    int ww = min(W - 24, 210), wh = 150, wx = (W - ww) / 2, wy = GB_TOP + (GB_DH - wh) / 2;
    spr.fillRoundRect(wx - 2, wy - 2, ww + 4, wh + 4, 14, C(INK));
    spr.fillRoundRect(wx, wy, ww, wh, 12, C(PAPER));
    txt(FL, "PAUSED", W / 2, wy + 22, INK, D_MC);
    const char* it[2] = {"Resume", "Save and exit"};
    for (int i = 0; i < 2; i++) {
      btn(wx + 12, wy + 46 + i * 44, ww - 24, 36, it[i], false);
      if (joyOk && i == gbMenuSel) for (int r = 0; r < 2; r++) spr.drawRoundRect(wx + 10 - r, wy + 44 + i * 44 - r, ww - 20 + 2 * r, 40 + 2 * r, 10, C(NAV_RING_C));
    }
    txt(FS, gbRamSize ? "The game's own save is kept." : "This game has no save.", W / 2, wy + wh - 12, SOFT, D_MC);
  }
  spr.pushSprite(0, 0);
  gbChromeDirty = false;
}
void gbPause() { if (gbState == GB_PLAY) { gbState = GB_PAUSE; gbMenuSel = 0; gbChromeDirty = true; gbSaveRam(); } }
void gbResume() { gbState = GB_PLAY; gbChromeDirty = true; gbNextUs = micros(); lastTouchMs = millis(); }
void gbMenuDo(int i) { if (i == 0) gbResume(); else gbLeave(); }

// ---------------- from loop() while a game is open ----------------
void gbLoop() {
  uint32_t now = millis();
  lgfx::touch_point_t tp;
  bool touch = lcd.getTouch(&tp) > 0;
  bool sw = joyOk && joyDown();
  if (pw == P_OFF || gbWakeHold) {   // screen off (in a pocket): a touch or a press only wakes it, and does nothing else
    if (pw == P_OFF) { if (touch || sw) { wake(); gbWakeHold = true; gbChromeDirty = true; } return; }
    if (touch || sw) return;   // wait until it is let go
    gbWakeHold = false; gbSwWas = false; gbHoldDone = false; gbTouchWas = false; return;
  }
  if (gbChromeDirty) gbDrawChrome();
  // the top bar: [Pause] [Exit] (on a tap, when the finger is lifted)
  if (touch && !gbTouchWas) { gbTouchWas = true; lastTouchMs = now; if (pw != P_ON) { wake(); gbHoldDone = true; } }
  else if (!touch && gbTouchWas) gbTouchWas = false;
  bool bar = touch && tp.y < GB_TOP;
  if (bar) gbBarX = tp.x;
  if (!bar && gbBarWas) {
    int h = gameBarHit(gbBarX);
    if (h == 1) { gbBarWas = false; gbLeave(); return; }
    if (h == 2) gbPause();
  }
  gbBarWas = bar;
  // the stick button: A while playing; held 2 s = the pause menu
  if (sw && !gbSwWas) { gbSwT = now; gbHoldDone = false; lastTouchMs = now; if (pw != P_ON) { wake(); gbHoldDone = true; } }
  if (sw && !gbHoldDone && now - gbSwT >= GB_HOLD_MENU_MS && gbState == GB_PLAY) { gbHoldDone = true; gbPause(); }
  bool released = !sw && gbSwWas && !gbHoldDone && now - gbSwT > 30;
  gbSwWas = sw;
  float jx = 0, jy = 0; if (joyOk) joyVec(jx, jy);
  if (fabsf(jx) > 0.3f || fabsf(jy) > 0.3f) { lastTouchMs = now; if (pw != P_ON) { wake(); return; } }
  if (gbState == GB_PAUSE) {
    if (fabsf(jy) > 0.6f && now - gbMoveT > 220) { gbMenuSel = jy > 0 ? 1 : 0; gbMoveT = now; gbChromeDirty = true; }
    if (released) { gbMenuDo(gbMenuSel); return; }
    if (touch && tp.y >= GB_TOP) { gbMenuTWas = true; gbMenuTX = tp.x; gbMenuTY = tp.y; }
    else if (!touch && gbMenuTWas) {
      gbMenuTWas = false; int tx = gbMenuTX, ty = gbMenuTY;
      int ww = min(W - 24, 210), wh = 150, wx = (W - ww) / 2, wy = GB_TOP + (GB_DH - wh) / 2;
      for (int i = 0; i < 2; i++) if (hitR(tx, ty, wx + 12, wy + 46 + i * 44, ww - 24, 36)) { gbMenuDo(i); return; }
    }
    return;
  }
  if (gameIdle()) { gbPause(); return; }   // left alone: pause (the screen dims and turns off as usual)
  // the Game Boy buttons (0 = pressed)
  uint8_t pad = 0xFF;
  if (jx > 0.45f) pad &= ~JOYPAD_RIGHT; else if (jx < -0.45f) pad &= ~JOYPAD_LEFT;
  if (jy > 0.45f) pad &= ~JOYPAD_DOWN; else if (jy < -0.45f) pad &= ~JOYPAD_UP;
  if (sw && !gbHoldDone) pad &= ~JOYPAD_A;
  if (touch && tp.y >= GB_TOP) { GbKey k[4]; int n = gbKeys(k); for (int i = 0; i < n; i++) if (hitR(tp.x, tp.y, k[i].x, k[i].y, k[i].w, k[i].h)) pad &= ~k[i].bit; }
  gbCore->direct.joypad = pad;
  // run the frames that are due (at most 3 at once: a slow board plays slower instead of jumping)
  uint32_t us = micros();
  if ((int32_t)(us - gbNextUs) < 0) return;
  lcd.startWrite();
  for (int n = 0; n < 3 && (int32_t)(us - gbNextUs) >= 0; n++) {
    gb_run_frame(gbCore);
    gbFrames++; gbNextUs += GB_FRAME_US;
    if (gbFrames % 60 == 0) gb_tick_rtc(gbCore);   // the game's clock (MBC3), once a second
    if (gbErr >= 0) break;
  }
  lcd.endWrite();
  if ((int32_t)(micros() - gbNextUs) > 100000) { gbNextUs = micros(); gbSlowFrames++; }   // far behind: start counting again
  if (gbErr >= 0) { gbMsg = "The game stopped (emulator error " + String(gbErr) + ")."; gbLeave(); return; }
  if (gbRamDirty && now - gbRamSavedMs > 60000) gbSaveRam();   // the game's save, at most once a minute
}
String gbSub() { return "Game Boy games"; }
