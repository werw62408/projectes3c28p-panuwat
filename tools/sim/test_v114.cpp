#include "sim_common.h"
// v11.4 tests: the stick scrolls every screen (no more stuck pages), the games all leave with "hold 1 s",
// no PIN on the board's own Wi-Fi, stronger IR sending

static void run(int ms) { for (int t = 0; t < ms; t += 25) { g_simMs += 20; loop(); } }   // loop() itself adds 5 ms
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static bool isOrange(int x, int y) {
  uint16_t p = spr.readPixel(x, y), o = C(NAV_RING_C);
  return p == o || p == (uint16_t)((o >> 8) | (o << 8));
}
static bool ringOnFooter() { int i = navFind(); return !navOnArea && i >= 0 && navT[i].kind == NK_FOOTER; }
static bool ringVisible() {   // the ring's button is fully on screen (or the ring is around the area)
  if (navOnArea) return true;
  int i = navFind(); if (i < 0) return false;
  return navT[i].vh >= navT[i].h - 1;
}
// push down until the bottom tabs, then up until the top: every part must be reached, and back
struct Sweep { int downSteps, reached, maxScroll, upScroll; bool allVisible; };
static Sweep sweep() {
  dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
  Sweep s{0, 0, 0, 0, true};
  int* sv = scrollVar();
  int k;
  for (k = 0; k < 80; k++) { push(0, 1); s.allVisible &= ringVisible(); if (ringOnFooter()) break; }
  s.downSteps = k; s.reached = sv ? *sv : 0;
  if (sv) { int keep = *sv; *sv = 99999; dirty = true; render(); s.maxScroll = *sv; *sv = keep; dirty = true; render(); }
  for (k = 0; k < 80; k++) { push(0, -1); s.allVisible &= ringVisible(); int i = navFind(); if (!navOnArea && i >= 0 && !navInArea(navT[i]) && navT[i].kind != NK_FOOTER && (!sv || *sv == 0)) break; }
  s.upScroll = sv ? *sv : 0;
  return s;
}

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t114"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3;   // screen never turns off during the tests
  g_simOffline = false; WiFi.connected = true;

  // ---------- N: the stick on screens that scroll ----------
  // N1 the page from the video: News > oil prices. Down from Tech used to scroll once and get stuck
  { netTab = 1; newsCat = 0; scr = S_NET; netScroll = 0; newsOpen = -1; netLoad(true); netPoll(); dirty = true; loop();
    push(0, 1);   // show the ring (on the first button: Tech)
    int start = netScroll;
    for (int k = 0; k < 40 && !ringOnFooter(); k++) push(0, 1);
    int bottom = netScroll, mx = netMax;
    bool backToChips = false;
    for (int k = 0; k < 40; k++) { push(0, -1); int i = navFind(); if (!navOnArea && i >= 0 && navInArea(navT[i]) && netScroll == 0) { backToChips = true; break; } }
    printf("N1 News oil: from scroll %d down to %d of %d, up again to the Tech/AI/Robots chips=%d (scroll %d) %s\n",
           start, bottom, mx, backToChips, netScroll, R(bottom == mx && backToChips && netScroll == 0)); }
  // N2 every screen that scrolls: down reaches the end, up comes back to the top, the ring is always fully seen
  { struct { const char* n; std::function<void()> go; } pages[] = {
      {"Log", [] { scr = S_HOME; scrollY = 0; }},
      {"Stats", [] { scr = S_STATS; statView = 0; statScroll = 0; }},
      {"Settings", [] { scr = S_SET; setPage = 0; setScroll = 0; }},
      {"Settings > Screen", [] { scr = S_SET; setPage = 1; setScroll = 0; }},
      {"Settings > Wi-Fi", [] { scr = S_SET; setPage = 2; setScroll = 0; }},
      {"Settings > About", [] { scr = S_SET; setPage = 3; setScroll = 0; }},
      {"AC Remote", [] { scr = S_AC; acScroll = 0; }},
      {"Weather", [] { netTab = 0; scr = S_NET; netScroll = 0; newsOpen = -1; for (int k = 0; k < 3; k++) { netLoad(true); netPoll(); dirty = true; render(); if (netMax > 0) break; } }},
      {"News: oil", [] { netTab = 1; newsCat = 0; scr = S_NET; netScroll = 0; newsOpen = -1; netLoad(true); netPoll(); }},
      {"News: AI", [] { netTab = 1; newsCat = 1; scr = S_NET; netScroll = 0; newsOpen = -1; netLoad(true); netPoll(); }},
      {"News: robots", [] { netTab = 1; newsCat = 2; scr = S_NET; netScroll = 0; newsOpen = -1; netLoad(true); netPoll(); }},
      {"A news story", [] { netTab = 1; newsCat = 1; scr = S_NET; netScroll = 0; newsOpen = 0; }},
    };
    for (auto& p : pages) {
      p.go(); Sweep s = sweep();
      bool ok = s.reached >= s.maxScroll && s.upScroll == 0 && s.allVisible;
      printf("N2 %-18s down %2d pushes, scrolled %4d of %4d, back up to %d, ring always fully seen=%d %s\n", p.n, s.downSteps, s.reached, s.maxScroll, s.upScroll, s.allVisible, R(ok));
    }
  }
  // N3 a page with no buttons in it (Weather): the ring goes around the reading area, drawn in orange
  { netTab = 0; scr = S_NET; netScroll = 0;
    for (int k = 0; k < 3; k++) { netLoad(true); netPoll(); dirty = true; render(); if (netMax > 0) break; }   // (weather comes from the internet: try again if it failed)
    dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
    for (int k = 0; k < 6 && !navOnArea; k++) push(0, 1);
    dirty = true; render(); savePng(spr, "t114_area_ring", W, H);
    bool edge = isOrange(1, navArY + navArH / 2) || isOrange(2, navArY + navArH / 2);
    int s0 = netScroll; push(0, 1); int s1 = netScroll;
    printf("N3 Weather: ring around the area=%d, orange edge drawn=%d, a push scrolls it %d -> %d %s\n", navOnArea, edge, s0, s1, R(navOnArea && edge && s1 > s0)); }
  // N4 a news list: each push shows the next story whole
  { netTab = 1; newsCat = 1; scr = S_NET; netScroll = 0; newsOpen = -1; netLoad(true); netPoll(); dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
    int whole = 0, n = 0, prevY = -9999; bool forward = true;
    for (int k = 0; k < 10; k++) { push(0, 1); int i = navFind(); if (i < 0 || navOnArea || !navInArea(navT[i])) continue;
      n++; if (navT[i].vh >= navT[i].h - 1) whole++; int y = navT[i].y + netScroll; if (y < prevY) forward = false; prevY = y; }
    dirty = true; render(); savePng(spr, "t114_news_list", W, H);
    printf("N4 News list: %d of %d stories fully seen when the ring is on them, always forward=%d %s\n", whole, n, forward, R(n >= 5 && whole == n && forward)); }
  // N5 touch the screen, then the stick again: the ring comes back where it was (not the first button)
  { scr = S_SET; setPage = 0; setScroll = 0; dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
    for (int k = 0; k < 4; k++) push(0, 1);
    int16_t fx = navFX, fy = navFY;
    g_simTouch = true; g_simTouchX = W - 4; g_simTouchY = HDR_H / 2; run(40); g_simTouch = false; run(60);   // touch the top bar (does nothing)
    bool hidden = !navShow; push(0, 1); bool same = navFX == fx && navFY == fy;
    printf("N5 touch hides the ring=%d, next push shows it on the same button=%d %s\n", hidden, same, R(hidden && same && navShow)); }
  // N6 pocket: screen off, pushes do nothing, a press only wakes (still true after the changes)
  { scr = S_NET; dirty = true; loop(); pw = P_OFF; lowPower = true; int s0 = netScroll;
    push(0, 1); push(0, 1); bool stillOff = pw == P_OFF && netScroll == s0;
    press(); bool woke = pw == P_ON;
    printf("N6 screen off: pushes keep it off and scroll nothing=%d, press wakes=%d %s\n", stillOff, woke, R(stillOff && woke)); }

  // ---------- G: every game leaves with "hold the press 1 s" ----------
  // G1 Sudoku: orange cursor, short press = pad, hold 1 s = back to Games
  { sudokuOpen(); dirty = true; loop();
    if (sdkNewMenu) { press(); run(50); }   // no saved game: first press starts one
    push(0, 1); dirty = true; render();
    SdkGeo g = sdkGeo(); int cx = g.x0 + (sdkSel % 9) * g.cs, cy = g.y0 + (sdkSel / 9) * g.cs;
    bool orange = isOrange(cx, cy + g.cs / 2) || isOrange(cx + 1, cy + g.cs / 2);
    savePng(spr, "t114_sudoku_cursor", W, H);
    press(); bool pad = sdkPad;
    dirty = true; render(); int pk = sdkPadSel; (void)pk; savePng(spr, "t114_sudoku_pad", W, H);
    press(); run(50);   // put a number in (closes the pad)
    g_simDigital[JOY_SW] = LOW; run(1200); g_simDigital[JOY_SW] = HIGH; run(100);
    printf("G1 Sudoku: stick cursor orange=%d, press opens the pad=%d, hold 1 s = back to Games=%d %s\n", orange, pad, scr == S_GAMES, R(orange && pad && scr == S_GAMES)); }
  // G2 Sudoku with the screen off: a push does not wake, a press only wakes (no pad)
  { sudokuOpen(); dirty = true; loop(); if (sdkNewMenu) { press(); run(50); } sdkPad = false; int sel0 = sdkSel;
    pw = P_OFF; lowPower = true;
    push(1, 0); push(0, 1); bool stillOff = pw == P_OFF && sdkSel == sel0;
    press(); bool woke = pw == P_ON; bool noPad = !sdkPad;
    printf("G2 Sudoku screen off: pushes keep it off=%d, press only wakes=%d (no pad=%d) %s\n", stillOff, woke, noPad, R(stillOff && woke && noPad)); }
  // G3 Sudoku opened while the stick is still pressed (from the menu): the stick still works, the held press does nothing
  { scr = S_GAMES; dirty = true; loop();
    g_simDigital[JOY_SW] = LOW; sudokuOpen(); run(200); g_simDigital[JOY_SW] = HIGH; run(100);
    bool nothing = !sdkPad && scr == S_SUDOKU;
    int s0 = sdkSel; push(1, 0); bool moves = sdkSel != s0;
    printf("G3 Sudoku opened with the stick held: stick found=%d, held press did nothing=%d, stick moves the box=%d %s\n", sdkJoy, nothing, moves, R(sdkJoy && nothing && moves));
    sudokuClose(); }
  // G4 Pixel Swim: short press starts, hold 1 s = back to Games
  { gameOpen(); run(100); press(); bool playing = gs == G_PLAY; run(300);
    shotLcd("t114_swim");
    g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(100);   // (the game loop is slower in the simulator)
    printf("G4 Pixel Swim: press starts=%d, hold 1 s = back to Games=%d %s\n", playing, scr == S_GAMES, R(playing && scr == S_GAMES)); }
  // G5 Sand & Water: short press empties, screen off = only a press wakes, hold 1 s = back to Games
  { sandOpen(); run(100); uint32_t dt0 = saDrainT; press(); bool drain = saDrainT != dt0;   // (an empty box closes again at once)
    pw = P_OFF; lowPower = true; bool d1 = saDrain;
    push(1, 0); bool stillOff = pw == P_OFF;
    press(); bool woke = pw == P_ON && saDrain == d1;
    g_simDigital[JOY_SW] = LOW; run(1200); g_simDigital[JOY_SW] = HIGH; run(100);
    printf("G5 Sand: press empties=%d, screen off: push keeps it off=%d, press only wakes=%d, hold 1 s = Games=%d %s\n", drain, stillOff, woke, scr == S_GAMES, R(drain && stillOff && woke && scr == S_GAMES)); }
  // G7 after leaving with the hold, letting go of the press does not open a game from the menu (ring on a tile)
  { scr = S_GAMES; dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
    bool ok = true;
    for (int g = 0; g < 3; g++) {
      if (g == 0) gameOpen(); else if (g == 1) sudokuOpen(); else sandOpen();
      run(150); if (g == 1 && sdkNewMenu) { press(); run(50); }
      g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(300);
      if (scr != S_GAMES) { printf("   game %d: after letting go the screen is %d\n", g, scr); ok = false; scr = S_GAMES; }
    }
    printf("G7 leave by holding, then let go: stays on the Games menu (Pixel Swim, Sudoku, Sand) %s\n", R(ok)); }
  // G6 Tilt Maze and Blocks still open their menus with the same hold (unchanged from v11.3)
  { mazeOpen(); run(100); press(); run(300); g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(100);
    bool mz = mzState == MZ_PAUSE;
    mzSetState(MZ_PLAY); scr = S_GAMES; dirty = true; loop();
    printf("G6 Tilt Maze: hold 1 s = pause menu=%d %s\n", mz, R(mz)); }

  // ---------- W: web page PIN ----------
  { server.headers.clear(); apOn = true;
    server.simClient.local = WiFi.softAPIP(); bool hot = pinOk();
    server.simClient.local = WiFi.localIP(); bool homeNo = !pinOk();
    server.headers["Cookie"] = std::string("stpin=") + webPin.c_str(); bool homeYes = pinOk();
    server.headers["Cookie"] = "stpin=0000"; if (webPin == "0000") server.headers["Cookie"] = "stpin=1111"; bool wrong = !pinOk();
    server.headers.clear(); apOn = false; server.simClient.local = WiFi.softAPIP(); bool apOffAsks = !pinOk(); apOn = true;
    printf("W1 PIN: board's Wi-Fi no PIN=%d, home Wi-Fi asks=%d, right PIN ok=%d, wrong PIN refused=%d, hotspot off = asks=%d %s\n",
           hot, homeNo, homeYes, wrong, apOffAsks, R(hot && homeNo && homeYes && wrong && apOffAsks)); }

  // ---------- I: air con IR ----------
  { int m0 = g_simIrMsgs; scr = S_AC; acSend(); int sent = g_simIrMsgs - m0;
    printf("I1 AC: one tap sends the message %d times, IO21 drive strength %d (3 = strongest) %s\n", sent, g_simDriveCap[21], R(sent == 2 && g_simDriveCap[21] == GPIO_DRIVE_CAP_3)); }

  // ---------- V: version ----------
  { scr = S_SET; setPage = 3; setScroll = 9999; dirty = true; render(); setScroll = 9999; dirty = true; render(); savePng(spr, "t114_about", W, H);
    bool v = std::string(INDEX_HTML).find("requestVideoFrameCallback") != std::string::npos;
    printf("V1 About shows v11.4 (see t114_about.png), phone page has the new clip converter=%d %s\n", v, R(v)); }
  return 0;
}
