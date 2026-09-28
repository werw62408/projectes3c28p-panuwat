#include "sim_common.h"
// v11.7.2 tests: the Deck page in its own black + neon look, no bottom bar, ways out.
// Played like a person: taps, stick pushes and presses; plus extreme cases.

static void run(int ms) { for (int t = 0; t < ms; t += 25) { g_simMs += 20; loop(); } }   // loop() itself adds 5 ms
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void runFor(uint32_t ms) { uint32_t t = millis(); while (millis() - t < ms) { g_simMs += 20; loop(); } }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void ftap(int x, int y, int holdMs = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(holdMs); g_simTouch = false; run(60); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static uint32_t sprHash() { uint32_t h = 2166136261u; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) h = (h ^ spr.readPixel(x, y)) * 16777619u; return h; }
static void openDeck() { goScreen(S_APPS); dirty = true; run(100); int x, y, w, h; appTileRect(4, x, y, w, h); ftap(x + w / 2, y + h / 2); run(100); }
// the same choice as deckLabel(): big, small, small on two lines; false = it would be cut
static bool labelFits(const String& s, int w, int h) {
  int room = w - 8;
  spr.setFont(pickFont(FB, s)); if ((int)spr.textWidth(s) <= room) return true;
  spr.setFont(pickFont(FS, s)); if ((int)spr.textWidth(s) <= room) return true;
  int sp = -1, best = 999;
  for (int i = 1; i + 1 < (int)s.length(); i++) if (s[i] == ' ' && abs(i - (int)s.length() / 2) < best) { best = abs(i - (int)s.length() / 2); sp = i; }
  if (sp <= 0 || h < 30) return false;
  spr.setFont(pickFont(FS, s));
  return (int)spr.textWidth(s.substring(0, sp)) <= room && (int)spr.textWidth(s.substring(sp + 1)) <= room;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t1172"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3;
  auto downs = [&](size_t from) { int d = 0; for (size_t i = from; i < g_simDeck.size(); i++) if (g_simDeck[i].down) d++; return d; };

  // N1 the same look on the light and the dark theme (and any text colour)
  { openDeck(); navShow = false;
    themeDark = 0; textColIdx = 0; applyTheme(); render(); uint32_t a = sprHash(); savePng(spr, "preview_v11.7.2_tall", W, H);
    themeDark = 1; textColIdx = 2; applyTheme(); render(); uint32_t b = sprHash();
    themeDark = 0; textColIdx = 0; applyTheme(); render();
    int x, y, w, h; deckKeyRect(0, x, y, w, h);
    bool dark = spr.readPixel(x + w + DECK_GAP / 2, y + h / 2 + 1) == C(DN_BG) || spr.readPixel(x + w + DECK_GAP / 2, y + h / 2 + 3) == C(DN_BG);
    printf("N1 Deck looks the same on light and dark theme=%d, black between the keys=%d %s\n", a == b, dark, R(scr == S_DECK && a == b && dark)); }
  // N2 the top bar is neon too: black, a cyan line under it
  { bool blk = spr.readPixel(W / 2 + 3, 2) == C(0x000000), line = spr.readPixel(W / 2, HDR_H - 1) == C(DN_CYAN);
    printf("N2 top bar: black=%d, cyan line under it=%d %s\n", blk, line, R(blk && line)); }
  // N3 no bottom bar: no tab for the joystick, a tap under the keys does nothing, a tap where "Log" was hits the key there
  { bool noTab = true; for (auto& t : navT) if (t.kind == NK_FOOTER) noTab = false;
    size_t n0 = g_simDeck.size(); ftap(W / 2, H - 3); bool stay = scr == S_DECK && g_simDeck.size() == n0;
    int x0[N_TABS + 1]; footerTabs(x0); ftap((x0[0] + x0[1]) / 2, FTR_Y + 12); bool key = scr == S_DECK && downs(n0) == 1;
    printf("N3 no bottom bar: no tab for the stick=%d, tap under the keys: stays + nothing sent=%d, tap at the old Log tab: stays, sends that key=%d %s\n", noTab, stay, key, R(noTab && stay && key)); }
  // N4 Exit (top left) leaves, Bluetooth is off again
  { ftap(W - 38, HDR_H + 17); bool bt = deckVia == DV_BT && deckBleOn;
    shot("t1172_deck_bt");
    ftap(30, HDR_H + 17); run(100);
    printf("N4 Exit tap (on Bluetooth): on Apps=%d, Bluetooth off=%d %s\n", scr == S_APPS, !deckBleOn && !BLEDevice::getInitialized(), R(bt && scr == S_APPS && !deckBleOn && !BLEDevice::getInitialized())); }
  // N5 hold the stick left 1 s = Back (as on every page)
  { openDeck(); stickRaw(2048 - 1900, 2048); runFor(1300); stickRaw(2048, 2048); run(200);
    printf("N5 hold stick left 1 s: left Deck=%d (on Apps) %s\n", scr == S_APPS, R(scr == S_APPS)); }
  // N6 stick only: walk the ring to Exit and press
  { openDeck(); navShow = false; for (int k = 0; k < 5; k++) push(0, -1); for (int k = 0; k < 4; k++) push(-1, 0); press(); run(100);
    printf("N6 stick only: ring to Exit + press: on Apps=%d %s\n", scr == S_APPS, R(scr == S_APPS)); }
  // N7 pocket: screen off in Deck. A tap on Exit only wakes; holding the stick left does not wake and does not leave
  { openDeck(); pw = P_OFF; lowPower = true; ftap(30, HDR_H + 17); bool w1 = pw == P_ON && scr == S_DECK;
    pw = P_OFF; lowPower = true; stickRaw(2048 - 1900, 2048); runFor(1500); stickRaw(2048, 2048); run(200); bool w2 = pw == P_OFF && scr == S_DECK;
    press(); bool w3 = pw == P_ON && scr == S_DECK;
    printf("N7 screen off: tap on Exit only wakes=%d, stick held left: still off + still Deck=%d, press only wakes=%d %s\n", w1, w2, w3, R(w1 && w2 && w3)); }
  // N8 the Home/Uni chip in the neon top bar: a short tap does nothing, a long press switches (pocket rule)
  { run(100); char p0 = place; int cx = (hdrChipX0 + hdrChipX1) / 2; ftap(cx, 15); bool shortNo = place == p0;
    ftap(cx, 15, 900); bool longYes = place != p0; ftap(cx, 15, 900);
    printf("N8 Home/Uni chip on Deck: short tap no change=%d, long press switches=%d, back=%d %s\n", shortNo, longYes, place == p0, R(shortNo && longYes && place == p0)); }
  // N9 a reminder on Deck: the bar is in neon colours near the bottom, inside the screen; a tap goes to Log
  { remindAct = 0; dirty = true; run(100); shot("t1172_deck_remind");
    bool in = remindBarShown && remindBarY + 30 <= H && remindBarY > deckTop();
    ftap(W / 2, remindBarY + 15); run(100);
    printf("N9 reminder on Deck: bar at y=%d (screen %d) shown inside=%d, tap -> Log=%d %s\n", remindBarY, H, in, scr == S_HOME, R(in && scr == S_HOME)); remindAct = -1; }
  // N10 turn the screen while a key is lit: 4 x 3 keys, all inside, under the status row, not touching; back again
  { openDeck(); int x, y, w, h; deckKeyRect(4, x, y, w, h); ftap(x + w / 2, y + h / 2, 30);
    rot = 1; applyRotation(); dirty = true; run(50); navShow = false; render(); savePng(spr, "preview_v11.7.2_wide", W, H);
    bool ok = true; int lx = 0, ly = 0;
    for (int s = 0; s < DECK_PER_PAGE; s++) { deckKeyRect(s, x, y, w, h); if (x < 2 || y < HDR_H + 60 || x + w > W - 2 || y + h > H - 2 || h < 30) ok = false;
      if (s % 4) { if (x - lx < 4) ok = false; } lx = x + w; if (s >= 4) { int ax, ay, aw, ah; deckKeyRect(s - 4, ax, ay, aw, ah); if (y - (ay + ah) < 4) ok = false; } ly = y + h; }
    printf("N10 turn the screen on Deck: wide %dx%d, 12 keys inside with room between=%d (last ends x=%d y=%d) %s\n", W, H, ok, lx, ly, R(ok && scr == S_DECK && W == 320));
    rot = 0; applyRotation(); dirty = true; run(100); }
  // N11 every default name fits its key (not cut), tall and wide, both pages
  { int bad = 0; String which;
    for (int r = 0; r < 2; r++) { rot = r; applyRotation();
      for (int i = 0; i < DECK_N; i++) { int x, y, w, h; deckKeyRect(i % DECK_PER_PAGE, x, y, w, h); if (!labelFits(deckKeys[i].label, w, h)) { bad++; which += deckKeys[i].label + (r ? "(wide) " : "(tall) "); } } }
    rot = 0; applyRotation(); deckPage = 1; dirty = true; run(100); navShow = false; render(); savePng(spr, "preview_v11.7.2_page2", W, H); deckPage = 0;
    printf("N11 all 24 names fit their keys tall + wide: cut=%d %s %s\n", bad, which.c_str(), R(bad == 0)); }
  // N12 on for 49.7 days (millis() wraps; a tap wakes the screen first): a key still sends, lights up, and goes dark again
  { if (deckVia != DV_USB) ftap(W - 38, HDR_H + 17);
    g_simMs = 0xFFFFFFFFULL - 230; dirty = true; run(50); ftap(W / 2, H - 2); size_t n0 = g_simDeck.size();
    int x, y, w, h; deckKeyRect(0, x, y, w, h); ftap(x + w / 2, y + h / 2, 30); bool sent = downs(n0) == 1;
    run(1000); bool wrapped = millis() < 2000; bool dark = !(deckFlash >= 0 && millis() - deckFlashMs < 300);
    printf("N12 millis() wraps while the key is lit: key sent=%d, wrapped=%d, light off again=%d %s\n", sent, wrapped, dark, R(sent && wrapped && dark)); }
  // N13 extreme: in and out 20 times by Exit on Bluetooth: Bluetooth off every time; 40 fast taps on Exit + tile never stuck
  { int leaks = 0;
    for (int k = 0; k < 20; k++) { openDeck(); if (deckVia != DV_BT) ftap(W - 38, HDR_H + 17); ftap(30, HDR_H + 17, 30); if (deckBleOn || BLEDevice::getInitialized() || scr != S_APPS) leaks++; }
    printf("N13 20 times in + out on Bluetooth: Bluetooth left on=%d times %s\n", leaks, R(leaks == 0)); }

  // ================= reminder bar: its own [+1] button =================
  auto actIdx = [&](const char* id) { for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == id) return (int)i; return -1; };
  auto cnt = [&](int i) { int c = 0; for (auto& e : todayEv) if (e.id == acts[i].id) c++; return c; };
  auto sum = [&](int i) { float v = 0; for (auto& e : todayEv) if (e.id == acts[i].id) v += e.v; return v; };
  auto showRemind = [&](int i) { remindAct = i; remindDoneAct = -1; wake(); dirty = true; run(100); };
  auto plusTap = [&](int hold = 60) { ftap(W - 20, remindBarY + 15, hold); };
  int wa = actIdx("water");
  // R1 on Apps: tap [+1] -> one log, still on Apps, the reminder is gone, "Water +1 saved" shows
  { goScreen(S_APPS); showRemind(wa); shot("t1172_remind_plus"); bool btn = remindBarShown && remindPlusX > W / 2;
    int c0 = cnt(wa); plusTap(); render(); bool saved = remindBarIsDone; shot("t1172_remind_saved");
    printf("R1 Apps: [+1] in the bar=%d, tap -> logged %d (1 expected), still on Apps=%d, reminder gone=%d, \"saved\" shown=%d %s\n", btn, cnt(wa) - c0, scr == S_APPS, remindAct < 0, saved, R(btn && cnt(wa) - c0 == 1 && scr == S_APPS && remindAct < 0 && saved)); }
  // R2 a tap on "saved" does nothing (no 2nd log, no page change); it goes away after 1.5 s
  { int c0 = cnt(wa); plusTap(); ftap(40, remindBarY + 15); bool none = cnt(wa) == c0 && scr == S_APPS;
    runFor(1600); render(); bool gone = !remindBarShown;
    printf("R2 taps on \"saved\": nothing logged + stays=%d, gone after 1.5 s=%d %s\n", none, gone, R(none && gone)); }
  // R3 tap the words: go to Log, nothing logged (as before)
  { showRemind(wa); int c0 = cnt(wa); ftap(60, remindBarY + 15);
    printf("R3 tap \"Time for Water\": on Log=%d, nothing logged=%d %s\n", scr == S_HOME, cnt(wa) == c0, R(scr == S_HOME && cnt(wa) == c0)); }
  // R4 an amount activity: the button shows its step (like the tile's [+]) and logs that amount
  { int fu = actIdx("fuel"); goScreen(S_APPS); float st = acts[fu].step; showRemind(fu); String lab = remindPlusLabel(fu);
    float s0 = sum(fu); plusTap(); float added = sum(fu) - s0;
    printf("R4 amount activity \"%s\" (%s): button \"%s\", logged %.0f (%.0f expected) %s\n", acts[fu].name.c_str(), acts[fu].unit.c_str(), lab.c_str(), added, st, R(isUnitAct(acts[fu]) && lab == "+" + fmtNum(st) && fabsf(added - st) < 0.01f)); runFor(1600); }
  // R5 stick only: ring to [+1] + press -> logged
  { goScreen(S_APPS); showRemind(wa); navShow = false; int c0 = cnt(wa);
    for (int k = 0; k < 4; k++) push(0, 1); for (int k = 0; k < 3; k++) push(1, 0); bool onPlus = navFX == remindPlusX; press();
    printf("R5 stick only (Apps): 4 pushes down + right: ring on [+1]=%d, press: logged %d, still on Apps=%d %s\n", onPlus, cnt(wa) - c0, scr == S_APPS, R(onPlus && cnt(wa) - c0 == 1 && scr == S_APPS)); runFor(1600); }
  { openDeck(); showRemind(wa); navShow = false; int c0 = cnt(wa); size_t n0 = g_simDeck.size();
    for (int k = 0; k < 6; k++) push(0, 1); for (int k = 0; k < 3; k++) push(1, 0); bool onPlus = navFX == remindPlusX; press();
    printf("R5b stick only (Deck, no bottom bar): down + right: ring on [+1]=%d, press: logged %d, still on Deck=%d, no key sent=%d %s\n", onPlus, cnt(wa) - c0, scr == S_DECK, g_simDeck.size() == n0, R(onPlus && cnt(wa) - c0 == 1 && scr == S_DECK && g_simDeck.size() == n0)); runFor(1600); }
  // R6 pocket: screen off with the bar: a tap on [+1] only wakes, nothing logged; stick pushes log nothing
  { goScreen(S_APPS); showRemind(wa); int c0 = cnt(wa); pw = P_OFF; lowPower = true; plusTap(); bool w = pw == P_ON;
    pw = P_OFF; lowPower = true; for (int k = 0; k < 6; k++) push(k % 2 ? 1 : 0, k % 2 ? 0 : 1);
    printf("R6 screen off: tap on [+1] only wakes=%d, pushes keep it off=%d, nothing logged=%d %s\n", w, pw == P_OFF, cnt(wa) == c0, R(w && pw == P_OFF && cnt(wa) == c0)); press(); remindAct = -1; }
  // R7 fast double / triple taps on [+1]: only one log
  { goScreen(S_APPS); showRemind(wa); int c0 = cnt(wa); for (int k = 0; k < 3; k++) ftap(W - 20, remindBarY + 15, 20);
    printf("R7 3 fast taps on [+1]: logged %d (1 expected) %s\n", cnt(wa) - c0, R(cnt(wa) - c0 == 1)); runFor(1600); }
  // R8 on Deck (neon, no bottom bar): [+1] logs, stays on Deck, no key sent
  { openDeck(); showRemind(wa); shot("t1172_deck_remind_plus"); int c0 = cnt(wa); size_t n0 = g_simDeck.size(); plusTap();
    printf("R8 Deck: logged %d, still on Deck=%d, no key sent=%d %s\n", cnt(wa) - c0, scr == S_DECK, g_simDeck.size() == n0, R(cnt(wa) - c0 == 1 && scr == S_DECK && g_simDeck.size() == n0)); runFor(1600); }
  // R9 a long Thai name on a wide screen: the name is shortened, [+1] stays whole inside the bar
  { String old = acts[wa].name; acts[wa].name = "ดื่มน้ำเปล่าแก้วใหญ่มากหลังออกกำลังกายตอนเย็น";
    rot = 1; applyRotation(); goScreen(S_APPS); showRemind(wa); render(); savePng(spr, "t1172_remind_wide_thai", W, H);
    bool ok = remindPlusX > W / 2 && remindPlusX + 44 <= W - 6; int c0 = cnt(wa); plusTap();
    printf("R9 wide + long Thai name: [+1] at x=%d inside=%d, logged=%d %s\n", remindPlusX, ok, cnt(wa) - c0, R(ok && cnt(wa) - c0 == 1));
    acts[wa].name = old; runFor(1600); rot = 0; applyRotation(); }
  // R10 millis() wraps while "saved" shows: it still goes away
  { goScreen(S_APPS); g_simMs = 0xFFFFFFFFULL - 500; showRemind(wa); plusTap(); runFor(1700); render();
    printf("R10 millis() wraps under \"saved\": gone=%d, now %u ms %s\n", !remindBarShown, millis(), R(!remindBarShown && millis() < 5000)); }

  return 0;
}
