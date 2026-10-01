#pragma once
// v12: extra parts on the I2C wires (all optional, found at start):
//   - OLED 0.91" SSD1306 128x32 (address 0x3C) above the big screen: the clock, date, battery, Home/Uni,
//     reminders ("TIME FOR WATER  E=+1") and what was just saved. The big screen then has no top bar.
//   - PCF8574 + NA011 button board (low level in joystick.h): the buttons below. The big screen then has no tabs.
//       A  = press (like the stick press), hold = long press
//       B  = next tab (Log > Apps > Settings)        D = back (on a tab page: the tab before)
//       C  = go to Log                               E = +1 for the reminder shown ("Time for ...")
//       F  = small screen: clock <-> today's goals   hold F 1 s = Home <-> Uni
//     In a pocket (big screen off) any button only wakes the screen.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- the small screen ----------------
#define OLED_ADDR 0x3C
LGFX_Sprite ospr;              // 128x32, 1 byte a pixel (4 KB); turned into the screen's 1-bit pages when sent
uint8_t oledSent[512];         // what the screen shows now (only changes are sent)
bool oledSentOk = false;
uint8_t oledContrast = 0, oledFlipNow = 0xFF;
int oledPage = 0;              // F: 0 clock, 1 today's goals
uint32_t oledT = 0;

bool oledCmd(std::initializer_list<uint8_t> c) {
  uint8_t b[32]; uint8_t n = 0; b[n++] = 0x00;   // 0x00 = commands follow
  for (uint8_t v : c) if (n < sizeof b) b[n++] = v;
  return i2cWriteRaw(OLED_ADDR, b, n);
}
bool oledInit() {
  bool flip = oledMode == 1;
  oledFlipNow = oledMode;
  oledSentOk = false; oledContrast = 0;
  bool ok = oledCmd({0xAE, 0xD5, 0x80, 0xA8, 0x1F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
                     (uint8_t)(flip ? 0xA0 : 0xA1), (uint8_t)(flip ? 0xC0 : 0xC8),
                     0xDA, 0x02, 0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E});
  if (ok && oledMode != 2) oledCmd({0xAF});   // on (set to "Off": it stays dark, its memory would show noise)
  return ok;
}
void oledSend(const uint8_t* pages) {
  oledCmd({0x21, 0, 127, 0x22, 0, 3});   // the whole screen, left to right, top to bottom
  uint8_t b[129]; b[0] = 0x40;            // 0x40 = picture bytes follow
  for (int p = 0; p < 4; p++) {
    memcpy(b + 1, pages + p * 128, 128);
    if (!i2cWriteRaw(OLED_ADDR, b, 129)) { oledSentOk = false; return; }
  }
  memcpy(oledSent, pages, 512); oledSentOk = true;
}
void extBegin() {
  oledMode = prefs.getUChar("oledMode", 0); if (oledMode > 2) oledMode = 0;
  barsMode = prefs.getUChar("barsMode", 0) ? 1 : 0;
  oledOk = oledInit();
  if (oledOk) {
    ospr.setColorDepth(8);
    if (!ospr.createSprite(128, 32)) oledOk = false;
    else { ospr.fillScreen(TFT_BLACK); ospr.setTextColor(TFT_WHITE); ospr.setTextDatum(D_MC); ospr.setFont(&fonts::FreeSansBold9pt7b); ospr.drawString("SOMUDTICK", 64, 16); }
  }
  padBegin(oledOk ? OLED_ADDR : 0);
  Serial.printf("small screen: %s, button board: %s (0x%02X)\n", oledOk ? "yes" : "no", padOk ? "yes" : "no", padAddr);
}

// text on the small screen: English in sharp fonts, Thai names in the Thai font
void otxt(const String& s, int x, int y, lgfx::textdatum_t d, bool big = false) {
  const lgfx::IFont* f = hasThai(s) ? (const lgfx::IFont*)&fS : big ? (const lgfx::IFont*)&fonts::FreeSansBold9pt7b : (const lgfx::IFont*)&fonts::Font0;
  ospr.setFont(f); ospr.setTextDatum(d); ospr.drawString(s, x, y);
}
String ofit(const String& s, int w, bool big = false) {   // cut to w pixels (never in the middle of a Thai letter)
  const lgfx::IFont* f = hasThai(s) ? (const lgfx::IFont*)&fS : big ? (const lgfx::IFont*)&fonts::FreeSansBold9pt7b : (const lgfx::IFont*)&fonts::Font0;
  ospr.setFont(f);
  if (ospr.textWidth(s) <= w) return s;
  String t = s;
  while (t.length()) {
    int i = t.length() - 1; while (i > 0 && (t[i] & 0xC0) == 0x80) i--;
    t.remove(i);
    if (ospr.textWidth(t + ".") <= w) return t + ".";
  }
  return t;
}
String oUp(String s) { if (!hasThai(s)) s.toUpperCase(); return s; }

void oledClock(int sx, int sy) {
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  String t = hhmm(n);
  if (timeApprox) t = "~" + t;
  else if ((tm.tm_sec & 1) && !(scr == S_GAME || scr == S_MAZE || scr == S_BLOCKS || scr == S_GB || scr == S_SAND))
    t.setCharAt(2, ' ');   // the colon blinks: you see at a glance that it runs (not in a game: a frame costs ~13 ms = a hitch)
  ospr.setFont(&fonts::FreeSansBold18pt7b); ospr.setTextDatum(D_ML);
  ospr.drawString(t, sx, 16 + sy);
  // right: day, date, battery + Home/Uni (6x8 letters)
  String dow = EN_DOW[tm.tm_wday]; dow.toUpperCase();
  String mon = EN_MON[tm.tm_mon]; mon.toUpperCase();
  int p = batPct();
  String l3 = String(place == 'H' ? "H " : "U ") + (p < 0 ? String("USB") : String(p) + "%");
  int rx = 127 + sx;
  otxt(timeApprox ? String("TIME?") : dow, rx, 0 + sy, D_TR);
  otxt(timeApprox ? String("") : String(tm.tm_mday) + " " + mon, rx, 11 + sy, D_TR);
  otxt(l3, rx, 22 + sy, D_TR);
  if (staOk()) for (int k = 0; k < 3; k++) ospr.fillRect(rx - 42 + k * 3, 29 + sy - k * 2, 2, 2 + k * 2, TFT_WHITE);   // Wi-Fi bars
}
void oledGoals() {   // F: today's goals, "GOALS 2/3" and the first activities with a goal
  int goals = 0, done = 0; bool anyToday = !todayEv.empty();
  String row;
  for (auto& a : acts) {
    if (!(a.goal > 0 && a.goalType)) continue;
    goals++;
    float m = measure(a, sumFor(a.id)); int g = goalState(a, m);
    if (g == 2 || (a.goalType == 2 && (g == 3 || g == 1) && anyToday)) done++;
  }
  otxt(goals ? "GOALS " + String(done) + "/" + goals : String("TODAY"), 0, 0, D_TL);
  int y = 11;
  for (size_t i = 0; i < acts.size() && y <= 22; i++) {
    Act& a = acts[i]; Sum s = sumFor(a.id);
    String v = isUnitAct(a) ? fmtNum(s.sum) : String(s.count);
    if (a.goal > 0 && a.goalType) v += "/" + fmtNum(a.goal);
    otxt(v, 127, y, D_TR);
    ospr.setFont(&fonts::Font0); int vw = ospr.textWidth(v);
    otxt(ofit(oUp(a.name), 127 - vw - 6), 0, y, D_TL);
    y += 11;
  }
}
void oledTask() {
  if (!oledOk) return;
  uint32_t ms = millis();
  if (ms - oledT < 200) return;   // 5 times a second is enough (a whole screen takes ~13 ms on the wires)
  oledT = ms;
  if (oledFlipNow != oledMode) {   // Settings changed: turned over / off
    if (oledMode == 2) { oledCmd({0xAE}); oledFlipNow = 2; oledSentOk = false; return; }
    oledInit();
  }
  if (oledMode == 2) return;
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  // brightness: low at night and while the big screen is off (an OLED left on the same picture wears in)
  uint8_t want = (pw == P_OFF || tm.tm_hour >= 23 || tm.tm_hour < 6) ? 0x01 : 0xCF;
  if (want != oledContrast && oledCmd({0x81, want})) oledContrast = want;
  // the picture moves by a pixel every few minutes, for the same reason
  int mnt = tm.tm_min, sx = mnt % 3 - 1, sy = (mnt / 3) % 2 ? 1 : 0;
  ospr.fillScreen(TFT_BLACK); ospr.setTextColor(TFT_WHITE);
  bool popNow = popAct >= 0 && popAct < (int)acts.size() && ms - popMs < 2500;
  bool burstNow = burstNoticeMs && ms - burstNoticeMs < 4000;
  bool remindNow = remindAct >= 0 && remindAct < (int)acts.size();
  if (logFull) {
    if (tm.tm_sec & 1) { ospr.fillRect(0, 0, 128, 32, TFT_WHITE); ospr.setTextColor(TFT_BLACK); }
    otxt("LOG FULL", 64, 9, D_MC, true); otxt("logs are not saved", 64, 25, D_MC);
  } else if (burstNow) {
    bool btnMsg = burstName.startsWith("Button ");   // a loose button (padChatter) or too many logs in a minute
    otxt(btnMsg ? "IGNORED" : "TOO FAST", 64, 9, D_MC, true);
    otxt(ofit(oUp(burstName) + (btnMsg ? "" : " not saved"), 126), 64, 25, D_MC);
  } else if (popNow) {   // just saved: "+1 WATER" and today's number
    Act& a = acts[popAct]; Sum s = sumFor(a.id);
    otxt(ofit(popText + " " + oUp(a.name), 126, true), 0, 10, D_ML, true);
    String v = "TODAY " + (isUnitAct(a) ? fmtNum(s.sum) + " " + a.unit : String(s.count));
    if (a.goal > 0 && a.goalType) v += (a.goalType == 1 ? " / " : " MAX ") + fmtNum(a.goal);
    otxt(ofit(v, 126), 0, 25, D_ML);
  } else if (remindNow && (tm.tm_sec / 3) % 2 == 0) {   // every other 3 s: the reminder, then the clock
    otxt("TIME FOR", 0, 0, D_TL);
    otxt(padOk ? "E = +1" : "", 127, 0, D_TR);
    otxt(ofit(oUp(acts[remindAct].name), 126, true), 0, 21, D_ML, true);
  } else if (oledPage == 1) oledGoals();
  else oledClock(sx, sy);
  // 1 byte a pixel (RGB332) -> 4 pages of 8 rows, bit 0 at the top. A pixel counts as lit from half brightness.
  static uint8_t pages[512];
  memset(pages, 0, sizeof pages);
  const uint8_t* px = (const uint8_t*)ospr.getBuffer();
  if (!px) return;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 128; x++)
      if (((px[y * 128 + x] >> 2) & 7) >= 4) pages[(y >> 3) * 128 + x] |= 1 << (y & 7);
  if (!oledSentOk || memcmp(pages, oledSent, 512)) oledSend(pages);
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
void padPress(uint8_t b) {
  switch (b) {
    case PB_A: case PB_K: navHandle(JE_PRESS); break;
    case PB_B: padTab(1); break;
    case PB_C: if (padTabsOk()) { goScreen(S_HOME); sfx(SFX_TAP); } else sfx(SFX_ERR); break;
    case PB_D: if (padTopPage()) padTab(-1); else navHandle(JE_BACK); break;
    case PB_E: padQuickLog(); break;
    case PB_F: oledPage = 1 - oledPage; sfx(SFX_TAP); break;
    default: break;
  }
}
void padHold(uint8_t b) {
  if (b == PB_A || b == PB_K) navHandle(JE_HOLD);
  else if (b == PB_F) { place = place == 'H' ? 'U' : 'H'; prefs.putChar("place", place); ledFlash(0xFFFFFF, 120); sfx(SFX_KEY); dirty = true; }
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
void padTask() {   // from loop(), menus only (the games read the buttons themselves)
  if (!padOk) return;
  padPoll();
  uint8_t now = padBits & (joyOk ? 0x3F : 0x7F);   // K: read by the stick code when the stick is there
  uint32_t ms = millis();
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
