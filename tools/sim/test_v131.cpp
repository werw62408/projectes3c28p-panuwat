#include "sim_common.h"
// v13.1 tests: the fixes from the full check of v13 (the owner picked items 1-8 and the two odd pictures).
//  - A pressed fast on purpose (Del on the keyboard, Vol+ on Remotes / Deck, Sudoku) is not "Button A loose?"
//  - the joystick ring never sits on a button under the bottom bar, and a list scrolls its last row above the bar
//  - no reminder bar over a window (a tap there went to the bar)
//  - D in Sudoku closes the number pad / the New game window first
//  - the small screen: "NEXT ... IN" only for a reminder that will come (before 22:00), the clock keeps going in the
//    Joystick direction window, LOG FULL with the big screen off is one quiet line
//  - the button board found late (IO14 becomes the IR receiver), a remote's pulses on IO14 are not A in the Game Boy
//  - E / [+1] on the reminder bar: a refused log (too many in a minute) keeps the reminder
//  - the AC page says "Set from remote" in its title (no note over the page, no half words around it)
//  - HUD Log tiles: the "T+01:03" line ends above the - / +1 buttons (found while checking the pictures)
// Deck Bluetooth on NimBLE (item 1) can't run here (no Bluetooth stack in the simulator): it is checked by building
// the real firmware with the board's core (NimBLE is what the ESP32-S3 core 3.3.12 uses).

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static void btnDown(int b) { g_simPadDown |= 1 << b; }
static void btnUp(int b) { g_simPadDown &= ~(1 << b); }
static void pad(int b, int holdMs = 100) { btnDown(b); run(holdMs); btnUp(b); run(120); }
static void finger(int x, int y, int ms = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(ms); g_simTouch = false; run(60); }
static int countOf(const char* id) { int n = 0; for (auto& e : todayEv) n += e.id == id; return n; }
static bool oledLit(int x, int y) { return ospr.readPixel(x, y) != 0; }
static void padClear() { for (int b = 0; b < PB_N; b++) { padMuteUntil[b] = 0; padMutes[b] = 0; padLastT[b] = 0; padPressI[b] = 0; for (auto& t : padPressT[b]) t = 0; } burstNoticeMs = 0; }
static bool clearOfBar() {   // no place for the ring overlaps the bar (the bar's own buttons aside)
  if (!remindBarShown) return true;
  int top = remindBarY, bot = remindBarY + 30;
  for (auto& t : navT) { if (t.kind == NK_OVER) continue; if (t.y == remindBarY && t.h == 30) continue; if (t.vy < bot && t.vy + t.vh > top) return false; }
  return true;
}
static int lastRow(int w, int h) {   // the nav entry of the lowest full-width row of this size
  int best = -1; for (size_t i = 0; i < navT.size(); i++) if (navT[i].w == w && navT[i].h == h && (best < 0 || navT[i].y > navT[best].y)) best = i; return best;
}
static int simCount = 0;

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t131"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;   // Thu 24 Sep 2026, 20:15
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  g_simOledOn = true; g_simPadOn = true; g_simPadAddr = 0x20;   // plugged in before the start
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = true; WiFi.connected = true; remindAct = -1; animOn = false;
  for (auto& a : acts) if (a.id != "water") a.remind = 0;   // (only Water reminds, so no other bar turns up by itself)

  printf("1  version %s %s\n", FW_VERSION, R(atof(FW_VERSION + 1) >= 13.05));

  // ================= 2: A pressed fast on purpose is not "loose" =================
  { auto burst = [&](Screen s, uint8_t b, int n, int gapMs) {
      padClear(); scr = s; uint32_t t = millis() + 1000; int ignored = 0;
      for (int k = 0; k < n; k++) { if (padChatter(b, t)) ignored++; t += gapMs; }
      return ignored; };
    int home = burst(S_HOME, PB_A, 12, 300);     // 12 presses ~3 a second on the Log page: still caught (as in v13)
    int remote = burst(S_REMOTE, PB_A, 15, 300); // Vol+ 15 times on Remotes: a hand
    int kbd = burst(S_KBD, PB_A, 20, 250);       // Del 20 times on the keyboard
    int deck = burst(S_DECK, PB_A, 15, 300);     // Vol+ on Deck
    int sdk = burst(S_SUDOKU, PB_A, 14, 300);    // a quick Sudoku player
    int chat = burst(S_REMOTE, PB_A, 40, 90);    // a chattering contact on Remotes: still caught
    int e = burst(S_REMOTE, PB_E, 12, 300);      // E (= +1) keeps the normal rule everywhere
    padClear(); scr = S_HOME;
    bool ok = home > 0 && remote == 0 && kbd == 0 && deck == 0 && sdk == 0 && chat > 0 && e > 0;
    printf("2  fast A: Log page caught=%d; not caught on Remotes=%d, keyboard=%d, Deck=%d, Sudoku=%d; chatter still caught=%d, E normal rule=%d %s\n",
           home > 0, remote == 0, kbd == 0, deck == 0, sdk == 0, chat > 0, e > 0, R(ok)); }
  { for (auto& k : irKeys) k = IrKey();
    irKeys[0].used = true; irKeys[0].name = "Vol +"; irKeys[0].type = NEC; irKeys[0].bits = 32; irKeys[0].value = 0x20DF40BF;
    remoteOpen(); navShow = true; dirty = true; render();
    int x, y, w, h; irKeyRect(0, x, y, w, h); for (auto& t : navT) if (t.x == x && t.y == y) navFocus(t);
    size_t o0 = g_simIrOut.size(); padClear(); run(300);
    for (int k = 0; k < 12; k++) pad(PB_A, 90);   // 12 presses in ~2.5 s
    int sent = (int)(g_simIrOut.size() - o0); bool noLoose = !burstNoticeMs;
    remoteLeave(); run(100); padClear();
    printf("3  Remotes: A on a learned Vol+ 12 times in 2.5 s = %d sends (12 wanted), no \"Button A loose?\"=%d %s\n", sent, noLoose, R(sent == 12 && noLoose)); }

  // ================= 4-6: the joystick ring and the bottom bar =================
  { int wi = actIndexOf("water"); bool ok = true; std::string bad;
    for (int r : {0, 1}) for (int bm : {0, 1}) {
      std::string tag = std::string(r ? " wide" : " tall") + (bm ? "+bars" : "");
      barsMode = bm; rot = r; applyRotation();
      goScreen(S_SET); setOpenPage(0); remindAct = wi; remindId = "water"; navShow = true; navPage = -1; setScroll = 0; dirty = true; render();
      if (!remindBarShown) { ok = false; bad += tag + " no bar"; }
      bool clear = clearOfBar();
      for (int k = 0; k < 40; k++) { navHandle(JE_DOWN); dirty = true; render(); if (!clearOfBar()) clear = false; }
      if (!clear) { ok = false; bad += tag + " ring under the bar"; }
      setScroll = 9999; dirty = true; render(); setScroll = 9999; dirty = true; render();
      int a = lastRow(W - 16, 38);   // "About", the last row of Settings
      bool seen = a >= 0 && navT[a].vy + navT[a].vh <= remindBarY && navT[a].vh >= navT[a].h - 1;
      int n0 = countOf("water");
      if (seen) { navFocus(navT[a]); navPress(); }
      bool about = seen && setPage == 3 && countOf("water") == n0 && remindAct == wi;
      if (!about) { ok = false; bad += tag + " About under the bar"; }
      if (r == 0 && bm == 0) savePng(spr, "t131_settings_bottom_remind", W, H);
      scr = S_AC; acScroll = 9999; dirty = true; render(); acScroll = 9999; dirty = true; render();
      int f = lastRow(W - 16, 32);   // "Follow real remote", the last button of the AC page
      if (!(f >= 0 && navT[f].vy + navT[f].vh <= remindBarY)) { ok = false; bad += tag + " AC last button under the bar"; }
    }
    barsMode = 0; rot = 0; applyRotation(); remindAct = -1; goScreen(S_HOME); run(100);
    printf("4  reminder bar: the ring never on a button under it (Settings, every screen shape), the last rows scroll above it (About opens, no log; AC)%s %s\n", bad.c_str(), R(ok)); }
  { int wi = actIndexOf("water");
    for (auto& k : irKeys) k = IrKey();
    irKeys[0].used = true; irKeys[0].name = "TV"; irKeys[0].type = NEC; irKeys[0].bits = 32; irKeys[0].value = 0x20DF10EF;
    remoteOpen(); remindAct = wi; remindId = "water"; irMenuKey = 0; irMenu = 2; dirty = true; render();
    bool noBar = !remindBarShown;
    int n0 = countOf("water"); finger(W - 30, FTR_Y - 19);   // where the bar's [+1] would be
    bool noLog = countOf("water") == n0 && scr == S_REMOTE;
    irMenu = 0; remoteLeave(); remindAct = -1; run(100);
    printf("5  a window is open: no reminder bar over it=%d, a tap there does not log / leave=%d %s\n", noBar, noLog, R(noBar && noLog)); }
  { int wi = actIndexOf("water"); goScreen(S_HOME); logFull = true; remindAct = -1; navShow = true; scrollY = 0; dirty = true; render();
    bool notice = remindBarShown && remindBarIsNotice, clear = clearOfBar();
    for (int k = 0; k < 20; k++) { navHandle(JE_DOWN); dirty = true; render(); if (!clearOfBar()) clear = false; }
    logFull = false; (void)wi; scrollY = 0; dirty = true; run(100);
    printf("6  Log page with a notice (Log FULL): the ring never on a +1 under it=%d (notice shown=%d) %s\n", clear, notice, R(notice && clear)); }

  // ================= 7: D in Sudoku =================
  { sudokuOpen(); run(100); if (sdkNewMenu || !sdkHave || sg.done) { sdkNew(0); sdkNewMenu = false; }
    int cell = -1; for (int i = 0; i < 81; i++) if (!sg.puz[i] && !sg.cur[i]) { cell = i; break; }
    sdkSel = cell; sdkOpenPad(); bool open = sdkPad; dirty = true; run(100);
    pad(PB_D); bool closedPad = !sdkPad && scr == S_SUDOKU;
    sdkNewMenu = true; dirty = true; run(100); pad(PB_D); bool closedMenu = !sdkNewMenu && scr == S_SUDOKU;
    pad(PB_D); bool left = scr == S_GAMES;
    printf("7  Sudoku, D: closes the number pad first=%d, then the New game window=%d, then leaves=%d %s\n", closedPad, closedMenu, left, R(open && closedPad && closedMenu && left)); }

  // ================= 8-10: the small screen =================
  { const uint32_t T21 = T0 + 45 * 60, T19 = T0 - 75 * 60;   // 21:00 and 19:00 the same day
    String nm, in;
    setNow(T21); lastSeen["water"] = T21 - 15 * 60; bool late = oledNextRemind(nm, in);    // due 22:15: never comes
    setNow(T19); lastSeen["water"] = T19 - 15 * 60; bool early = oledNextRemind(nm, in);  // due 20:15
    String at = in;
    setNow(T0); recomputeLast();
    printf("8  small screen NEXT: a reminder after 22:00 is not shown=%d, one at 20:15 is (in \"%s\")=%d %s\n", !late, at.c_str(), early && at == "1H15M", R(!late && early && at == "1H15M")); }
  { goScreen(S_SET); run(100); long b0 = g_simOled.dataBytes;
    simCount = 0; g_simDelayHook = [] { simCount++; if (simCount == 300) g_simTouch = true; if (simCount == 306) g_simTouch = false; };   // cancel in "Push UP"
    joySetup(); g_simDelayHook = nullptr; g_simTouch = false;
    long sent = g_simOled.dataBytes - b0; run(100);
    printf("9  Joystick direction window: the small screen keeps its clock (%ld bytes sent while it waited) %s\n", sent, R(sent > 0)); }
  { logFull = true; pw = P_OFF; powerLow(true); remindAct = -1;
    if (!(time(nullptr) & 1)) g_simEpoch += 1;   // an odd second: v13 drew the whole screen white here
    oledT = 0; oledTask();
    int lit = 0; for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) lit += oledLit(x, y);
    bool said = false; for (int y = 51; y < 60; y++) for (int x = 30; x < 98; x++) if (oledLit(x, y)) said = true;
    bool quiet = lit < 128 * 64 / 5 && g_simOled.contrast == 0x01;
    oledPng("t131_oled_logfull_off");
    logFull = false; wake(); oledT = 0; oledTask();
    printf("10 big screen off + LOG FULL: the small screen stays dim and quiet (%d dots lit)=%d, says LOG FULL=%d %s\n", lit, quiet, said, R(quiet && said)); }

  // ================= 11-12: the button board found late, the Game Boy and IO14 =================
  { padOk = false; joySwIo14 = true; irOn = false; g_simIrRxOn = false; g_simPadOn = false;
    run(31000); bool old = !padOk && joySwIo14 && !irOn;   // the old wiring (no board): nothing found, nothing changes
    g_simPadOn = true; run(31000); bool found = padOk && !joySwIo14 && irOn && g_simIrRxOn && FTR_Y == H;
    printf("11 button board missed at the start: old wiring stays as it is=%d; plugged in later: found, IO14 = IR receiver, no tabs=%d %s\n", old, found, R(old && found)); }
  { bool wasPad = padOk; padOk = false; joySwIo14 = true; irOn = false;   // the old wiring: the stick press on IO14
    gbListOpen(); gbStart(gbRoms[0]); bool started = scr == S_GB; lastTouchMs = millis();
    auto aDown = [] { return !(gbCore->direct.joypad & JOYPAD_A); };
    bool pulseA = false;
    for (int k = 0; k < 6; k++) {   // a remote's start pulses on IO14: ~9 ms low, then high
      g_simDigital[JOY_SW] = LOW; for (int s = 0; s < 3; s++) { g_simMs += 4; gbLoop(); if (started && aDown()) pulseA = true; }
      g_simDigital[JOY_SW] = HIGH; for (int s = 0; s < 10; s++) { g_simMs += 10; gbLoop(); if (started && aDown()) pulseA = true; }
    }
    g_simDigital[JOY_SW] = LOW; bool realA = false; for (int s = 0; s < 10; s++) { g_simMs += 10; gbLoop(); if (started && aDown()) realA = true; }   // a real press, 100 ms
    g_simDigital[JOY_SW] = HIGH; for (int s = 0; s < 5; s++) { g_simMs += 10; gbLoop(); }
    if (scr == S_GB) gbLeave(); padOk = wasPad; joySwIo14 = !padOk; irOn = padOk; scr = S_GAMES; dirty = true; run(100);
    printf("12 Game Boy, old wiring: a remote's short pulses on IO14 are not A=%d, a real press is=%d %s\n", !pulseA, realA, R(started && !pulseA && realA)); }

  // ================= 13: E keeps the reminder when the log is refused =================
  { int wi = actIndexOf("water"); scr = S_APPS; burstT.clear(); padClear();
    for (int k = 0; k < 12; k++) logDefault(wi);   // 12 in a minute: the next one is refused ("Too fast")
    remindAct = wi; remindId = "water"; int n0 = countOf("water"); dirty = true; run(100);
    pad(PB_E); bool kept = countOf("water") == n0 && remindAct == wi;
    finger(W - 30, FTR_Y - 19); bool keptBar = countOf("water") == n0 && remindAct == wi;   // [+1] on the bar: the same
    g_simMs += 61000; burstNoticeMs = 0; run(100); pad(PB_E); bool logged = countOf("water") == n0 + 1 && remindAct == -1;
    goScreen(S_HOME); run(100); padClear();
    printf("13 too many in a minute: E keeps the reminder=%d, the bar's [+1] too=%d; a minute later E logs and clears it=%d %s\n", kept, keptBar, logged, R(kept && keptBar && logged)); }

  // ================= 14: the AC page's "Set from the remote" note =================
  { bool covered = false, said = true, fits = true;
    for (int th : {0, 2}) for (int r : {0, 1}) {
      themeSet(th); rot = r; applyRotation(); scr = S_AC; acScroll = 0;
      acSyncMs = 0; acSentMs = millis() - 5000; dirty = true; render();
      std::vector<uint16_t> plain; for (int y = HDR_H + 36; y < FTR_Y; y++) for (int x = 0; x < W; x++) plain.push_back(spr.readPixel(x, y));
      std::vector<uint16_t> topPlain; for (int y = HDR_H; y < HDR_H + 34; y++) for (int x = 78; x < W; x++) topPlain.push_back(spr.readPixel(x, y));
      acSyncMs = millis(); dirty = true; render();
      size_t k = 0; for (int y = HDR_H + 36; y < FTR_Y; y++) for (int x = 0; x < W; x++) if (spr.readPixel(x, y) != plain[k++]) covered = true;   // nothing over the page
      k = 0; bool changed = false; for (int y = HDR_H; y < HDR_H + 34; y++) for (int x = 78; x < W; x++) if (spr.readPixel(x, y) != topPlain[k++]) changed = true;
      if (!changed) said = false;
      { String t = hudUp(acSyncTitle()); spr.setFont(pickFont(FB, t)); if ((int)spr.textWidth(t) > W - 8 - 78) fits = false; }   // whole words in the title
      if (th == 0 && r == 0) savePng(spr, "t131_ac_synced", W, H);
      if (th == 2 && r == 1) savePng(spr, "t131_ac_synced_hud_wide", W, H);
    }
    themeSet(0); rot = 0; applyRotation(); acSyncMs = 0; goScreen(S_HOME); run(100);
    printf("14 AC \"Set from remote\": said in the title=%d (fits=%d), nothing drawn over the page=%d (Light / HUD, tall / wide) %s\n", said, fits, !covered, R(said && fits && !covered)); }

  // ================= 15: HUD Log tile: the time line clear of the buttons (found while checking the v13.1 pictures) =================
  { int hits = 0;
    for (int r : {0, 1}) {
      themeSet(2); rot = r; applyRotation(); goScreen(S_HOME); scrollY = 0; logFull = false; dirty = true; render();
      for (int i = 0; i < 2; i++) {
        int x, y; homeTileRect(i, x, y); int row = y + TILE_H - 29;   // the row just above the - / +1 buttons
        uint16_t bg = spr.readPixel(x + 70, row);
        for (int xx = x + 8; xx < x + 60; xx++) if (spr.readPixel(xx, row) != bg) hits++;
      }
      if (r == 0) savePng(spr, "t131_hud_log", W, H);
    }
    themeSet(0); rot = 0; applyRotation(); goScreen(S_HOME); run(100);
    printf("15 HUD Log tiles: \"T+01:03\" ends above the - / +1 buttons (%d dots in the row above them) %s\n", hits, R(hits == 0)); }
  return 0;
}
