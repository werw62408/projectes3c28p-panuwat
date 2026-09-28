#include "sim_common.h"
#include <chrono>
// Review of v11.6: real use and extreme cases. A FAIL line here = a problem found in v11.6 (not a broken test).
//   ./build.sh review_v11.6.cpp <v11.6 sketch folder> rev116 && ./bin/rev116

static void run(int ms) { for (int t = 0; t < ms; t += 25) { g_simMs += 20; loop(); } }   // loop() itself adds 5 ms
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void ftap(int x, int y, int holdMs = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(holdMs); g_simTouch = false; run(60); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static const uint32_t DAY = 86400000UL;
static void sleepScreen() { pw = P_OFF; lowPower = true; }   // as if the screen timer ran out

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_rev116"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3;
  g_simOffline = false; WiFi.connected = true;

  // ---------- X1 Dragon after a long time switched on (board left on USB) ----------
  for (uint32_t days : {1u, 26u, 48u}) {
    g_simMs = days * DAY;
    gameOpen(); run(100); press(); run(300);   // start
    bool before = dashing();                   // nobody pressed: must not be dashing
    press(); run(40); bool during = dashing();
    run(1500); bool after = dashing();
    printf("X1 Dragon after %2u days on: dashing before a press=%d, a press dashes=%d, stops after=%d %s\n", days, before, during, !after, R(!before && during && !after));
    g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(100);   // leave
  }
  g_simMs = DAY / 2;

  // ---------- X2 millis() wraps to 0 (49.7 days) while the board is used ----------
  { offIdx = 0;   // screen off after 30 s
    g_simMs = 0xFFFFFFFFu - 20000; scr = S_HOME; dirty = true; wake(); run(200);
    run(15000); bool stillOn = pw == P_ON;           // 15 s: on (crosses the wrap at ~20 s)
    run(12000); bool dim = pw == P_DIM;              // 27 s: dim (dims 20 s before off? offMs 30 s -> dim at 20 s)
    run(8000); bool off = pw == P_OFF;               // 35 s: off
    ftap(120, 150); bool woke = pw == P_ON;
    printf("X2 across the 49.7-day wrap: on at 15 s=%d, dim/off in time=%d/%d, a tap wakes=%d %s\n", stillOn, dim || off, off, woke, R(stillOn && off && woke));
    offIdx = 3; }

  // ---------- X3 hotspot rest on the News page (screen off 1 min + 1 min extra there) ----------
  { offIdx = 1; apOn = true; apAsleep = false; scr = S_NET; netTab = 0; netScroll = 0; dirty = true; wake(); run(100);
    uint32_t t0 = millis(), offAt = 0, apAt = 0;
    for (int s = 0; s < 14 * 60 && !apAt; s++) { run(1000); if (!offAt && pw == P_OFF) offAt = millis(); if (apAsleep) apAt = millis(); }
    float offMin = (offAt - t0) / 60000.0f, apAfter = apAt ? (apAt - offAt) / 60000.0f : -1;
    printf("X3 News page: screen off after %.1f min, hotspot rests %.1f min after that (should be 10) %s\n", offMin, apAfter, R(apAfter >= 9.9f));
    wake(); apAsleep = false; offIdx = 3; scr = S_HOME; dirty = true; run(100); }

  // ---------- X4 in a pocket: screen off, the stick is pushed and pressed, the screen is touched, in every game ----------
  { struct G { const char* n; std::function<void()> open; Screen s; } games[] = {
      {"Dragon", [] { gameOpen(); }, S_GAME}, {"Sudoku", [] { sudokuOpen(); }, S_SUDOKU}, {"Sand", [] { sandOpen(); }, S_SAND},
      {"Garden", [] { gardenOpen(); }, S_GARDEN}, {"Tilt Maze", [] { mazeOpen(); }, S_MAZE}, {"Blocks", [] { blocksOpen(); }, S_BLOCKS}};
    offIdx = 0;
    for (auto& g : games) {
      scr = S_GAMES; dirty = true; wake(); run(100);
      g.open(); run(200);
      if (g.s == S_SUDOKU && sdkNewMenu) { press(); run(100); }
      int score0 = score, lv0 = level; uint32_t sdk0 = sdkSecs(); bool drain0 = saDrain;
      int waitMs = g.s == S_SUDOKU ? 100000 : 45000;   // (Sudoku gets 1 min more before the screen turns off)
      run(waitMs);   // left alone: pauses, dims, turns off
      bool off = pw == P_OFF;
      score0 = score; lv0 = level; sdk0 = sdkSecs(); drain0 = saDrain;   // the state when it went dark
      // pocket: random pushes, short presses, long presses, a touch
      for (int k = 0; k < 6; k++) { push(k % 2 ? 1 : -1, k % 3 - 1); }
      bool offAfterPush = pw == P_OFF;
      bool same = g.s == S_GAME ? (score == score0 && level == lv0) : g.s == S_SUDOKU ? sdkSecs() == sdk0 : g.s == S_SAND ? saDrain == drain0 : true;   // nothing moved while dark
      press(); bool wokeByPress = pw != P_OFF; bool stayed1 = scr == g.s;
      run(waitMs); press(1300); bool stayed2 = scr == g.s;   // a long press in the pocket: only wakes, does not leave
      run(waitMs); ftap(W / 2, H / 2); bool stayed3 = scr == g.s;
      printf("X4 pocket in %-9s: off by itself=%d, pushes keep it off=%d, press wakes only=%d, long press stays=%d, tap stays=%d, nothing changed=%d %s\n",
             g.n, off, offAfterPush, wokeByPress && stayed1, stayed2, stayed3, same, R(off && offAfterPush && wokeByPress && stayed1 && stayed2 && stayed3 && same));
      wake(); run(100);
      g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(200);   // leave with the hold
      if (scr != S_GAMES) { scr = S_GAMES; dirty = true; }
    }
    offIdx = 3; }

  // ---------- X5 Sudoku clock: counts only while playing ----------
  { sudokuOpen(); run(200); if (sdkNewMenu) { press(); run(100); }
    uint32_t a = sdkSecs(); run(10000); uint32_t b = sdkSecs();          // playing: +10 s
    sdkNewMenu = true; dirty = true; run(600000); uint32_t c = sdkSecs(); // New game window open 10 min: no change
    sdkNewMenu = false; run(3000); uint32_t d = sdkSecs();
    for (int k = 0; k < 200; k++) { sdkNewMenu = !sdkNewMenu; run(300); }   // open / close quickly 200 times (60 s)
    sdkNewMenu = false; run(100); uint32_t e = sdkSecs();
    printf("X5 Sudoku clock: 10 s play=+%u, 10 min in New game window=+%u, then 3 s=+%u, 200 quick open/close over 60 s=+%u (about 30 expected) %s\n",
           b - a, c - b, d - c, e - d, R(b - a >= 9 && b - a <= 11 && c - b <= 1 && e - d >= 25));
    sudokuClose(); run(100); }

  // ---------- X6 the Bluetooth memory: scan, then leave the page before the scan ends ----------
  { btPageOpen(); dirty = true; run(100); int d0 = BLEDevice::deinits();
    btScan(); bool busy = btBusy;
    goScreen(S_APPS); dirty = true; run(100);
    btDone = true;   // the 5 s scan ends while another page is open
    run(3000);
    int freedAway = BLEDevice::deinits() - d0; bool stillOn = BLEDevice::getInitialized();
    btPageOpen(); dirty = true; run(200); int freedBack = BLEDevice::deinits() - d0;
    printf("X6 Bluetooth scan, page left during the scan: memory given back while away=%d (still on=%d), after coming back=%d %s\n",
           freedAway, stillOn, freedBack, R(freedAway >= 1)); }

  // ---------- X7 no memory for a download job ----------
  { scr = S_NET; netTab = 0; dirty = true; run(100); g_simTaskFail = true; netLoad(true); run(300);
    bool msg = netMsg.indexOf("memory") >= 0; bool free_ = !netBusy;
    g_simTaskFail = false; netMsg = ""; netLoad(true); netPoll(); run(300);
    printf("X7 download job can't start (no memory): message shown=%d, not stuck on Loading=%d, next try works=%d %s\n", msg, free_, !netBusy, R(msg && free_ && !netBusy)); }

  // ---------- X8 SD card pulled out ----------
  { SD_MMC.present = false; sdOk = false;
    scr = S_SET; setAboutPage(); dirty = true; run(300); render();
    String sdl = sdCardText();
    filesOpen(); dirty = true; run(300);
    bool noCrash = true;
    backupNow(); run(100);
    printf("X8 SD card out: About says \"%s\", Files opens, backup says \"%s\", no crash=%d %s\n", sdl.c_str(), bkErr.c_str(), noCrash, R(sdl == "no card"));
    SD_MMC.present = true; sdMount(); scr = S_HOME; dirty = true; run(100); }

  // ---------- X9 huge data: 3 years of logs + 2500 files on the card: opening About ----------
  { // 3 years of day files, 30 logs a day
    for (int d = 15; d < 3 * 365; d++) {
      time_t day = T0 - (time_t)d * 86400; std::string out;
      for (int k = 0; k < 30; k++) { char b[64]; snprintf(b, sizeof b, "%u,water,1,H,%d\n", (unsigned)(day - 86400 / 2 + k * 600), k % 7 ? 1 : 0); out += b; }
      writeFile(logFs().host("/log/" + dayKey(day) + ".csv"), out);
    }
    SD_MMC.mkdir("/photos/many");
    for (int i = 0; i < 2500; i++) writeFile(SD_MMC.host("/photos/many/p" + std::to_string(i) + ".jpg"), "x");
    logRev++;
    auto ms = [](std::function<void()> f) { auto t = std::chrono::steady_clock::now(); f(); return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); };
    double tLogs = ms([] { unsureLogCount(); });
    double tCard = ms([] { sdCountFiles(); });
    double tCached = ms([] { unsureLogCount(); });
    logEvent(0, 1);   // one more log: the whole count again
    double tAgain = ms([] { unsureLogCount(); });
    scr = S_SET; setAboutPage(); dirty = true; render();
    printf("X9 3 years of logs (%d files read) + 2500 photos, on the PC: count logs %.0f ms, count card %.0f ms, cached %.1f ms, after 1 more log %.0f ms again %s\n",
           3 * 365, tLogs, tCard, tCached, tAgain, R(tAgain < 5));
    savePng(spr, "rev116_about_huge", W, H); }

  // ---------- X10 number pad: the longest number with a long unit ----------
  { int ai = -1; for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == "water") ai = i;
    String keepU = acts[ai].unit; acts[ai].unit = "ml of cold water";
    kpAct = ai; scr = S_KEYPAD; kpVal = "88888.88"; dirty = true; render(); savePng(spr, "rev116_keypad_long", W, H);
    spr.setFont(FS); String u = fitText(FS, acts[ai].unit, 100); int ux = 8 + kpGeo().panelW - 10 - (int)spr.textWidth(u);
    spr.setFont(FL); int left = ux - 8 - (int)spr.textWidth(kpVal);
    printf("X10 number pad \"%s\" + unit \"%s\": number starts at x=%d (panel starts at 8) %s\n", kpVal.c_str(), u.c_str(), left, R(left >= 8));
    acts[ai].unit = keepU; scr = S_HOME; dirty = true; run(100); }

  // ---------- X11 first start: nothing saved, no card, no stick, no Wi-Fi ----------
  { prefs.clear(); SD_MMC.present = false; joyOk = false; WiFi.connected = false; g_simOffline = true;
    const Screen all[] = {S_HOME, S_STATS, S_APPS, S_GAMES, S_SET, S_FILES, S_AC, S_NET, S_WIFI, S_BT, S_GARDEN, S_SUDOKU};
    for (auto s : all) { scr = s; dirty = true; render(); }
    for (int p = 0; p < 4; p++) { scr = S_SET; setOpenPage(p); dirty = true; render(); }
    printf("X11 empty board (no settings, no card, no stick, no Wi-Fi): all screens drew without a crash PASS\n");
    SD_MMC.present = true; sdMount(); joyOk = true; WiFi.connected = true; g_simOffline = false; }

  // ---------- X12 the reminder sound is cut (worked out from the code: the sound chip keeps 90 ms) ----------
  { const int RATE = 16000, DMA = 6 * 240;                // ESP_I2S in core 3.3.12: 6 x 240 frames queued
    int note2 = RATE * 160 / 1000, tail = 2 * 160;          // second note, then 2 x 160 silent frames
    int cut = max(0, DMA - tail); float pct = 100.0f * cut / note2;
    printf("X12 reminder sound: amp off with %d frames still queued, %d of them the note: last %.0f ms (%.0f%%) of the high note is cut %s\n",
           DMA, cut, cut * 1000.0f / RATE, pct, R(cut == 0)); }
  return 0;
}
