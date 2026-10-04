#include "sim_common.h"
// v11.5 tests: the bugs found in the v11.4 review (items 1-16) and the owner's wishes after the board videos (17-24).
// Every test here printed FAIL on v11.4 (the bug was there) and must print PASS on v11.5.
// New things (FW_VERSION, beepCount, oilSort, ...) do not exist in v11.4: there the test prints FAIL without building them.

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }   // by the clock (loop() adds 1-40 ms itself)
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void hold(int ms = 1500) { g_simDigital[JOY_SW] = LOW; run(ms); g_simDigital[JOY_SW] = HIGH; run(150); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static bool pixIs(int x, int y, uint32_t col) { uint16_t p = spr.readPixel(x, y), c = C(col); return p == c || p == (uint16_t)((c >> 8) | (c << 8)); }
static int logsToday() { return (int)todayEv.size(); }

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t115"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;   // 20:15 in Thailand: inside the reminder hours (08-22)
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = false; WiFi.connected = true; remindAct = -1;

  // ---------- 17 SD card text in About (first: later tests must not add files) ----------
  {
#ifdef FW_VERSION
    setAboutPage(); String used = sdCardText(), files = sdFilesText();
    bool ok = used.indexOf("GB") > 0 && files == "6 photos, 1 clip";
    printf("17 About SD card: \"%s\", \"%s\" %s\n", used.c_str(), files.c_str(), R(ok));
#else
    printf("17 About SD card: only whole MB, no number of files %s\n", R(false));
#endif
  }

  // ---------- 1 a reminder must not turn the screen off in the same loop pass ----------
  { scr = S_APPS; offIdx = 1; pw = P_ON; lowPower = false;
    g_simMs += 1; lastTouchMs = millis() + 2;   // wake() ran 2 ms after loop() read the clock (80 MHz + mktime on the board)
    loop(); bool stayedOn = pw == P_ON;
    // the whole reminder, from a dark screen
    pw = P_OFF; lowPower = true; remindedFor.clear(); remindAct = -1; lastSeen["water"] = (uint32_t)nowT() - 3 * 3600;
#ifdef FW_VERSION
    uint32_t b0 = beepCount;
#endif
    lastTouchMs = millis() - 200000; run(1500);
    bool woke = pw == P_ON && remindAct >= 0;
#ifdef FW_VERSION
    bool beeped = beepCount == b0 + 1;
    volIdx = -1; remindedFor.clear(); lastSeen["water"] = (uint32_t)nowT() - 3 * 3600; uint32_t b1 = beepCount; run(1500); bool quiet = beepCount == b1; volIdx = 2;
#else
    bool beeped = false, quiet = false;
#endif
    printf("1  reminder: wake 2 ms late keeps the screen on=%d, from dark: screen on + bar=%d, beep=%d, no beep when sound off=%d %s\n",
           stayedOn, woke, beeped, quiet, R(stayedOn && woke && beeped && quiet));
    remindAct = -1; offIdx = 3; lastTouchMs = millis(); }

  // ---------- 2 pocket: the joystick ring must not stay on [+1] while the screen is off ----------
  { scr = S_HOME; scrollY = 0; offIdx = 0; pw = P_ON; lowPower = false; lastTouchMs = millis(); remindAct = -1;
    dirty = true; loop(); navShow = true; navPage = -1; dirty = true; loop();
    int tx, ty; homeTileRect(0, tx, ty);
    for (auto& t : navT) if (t.x == tx + 41 && t.y == ty + TILE_H - 28) navFocus(t);   // the ring on Water's [+1]
    dirty = true; loop();
    int n0 = logsToday();
    run(40000); bool off = pw == P_OFF;   // 30 s screen timer
    press(); press();                     // squeezed twice in a pocket
    bool noLog = logsToday() == n0;
    int i = navFind(); bool notPlus = i < 0 || !(navT[i].x == tx + 41 && navT[i].y == ty + TILE_H - 28);
    printf("2  pocket: screen went off=%d, 2 presses: no log=%d, ring not left on [+1]=%d %s\n", off, noLog, notPlus, R(off && noLog && notPlus));
    offIdx = 3; lastTouchMs = millis(); if (pw != P_ON) wake(); }

  // ---------- 3 wide Sudoku: the top row and Back can be tapped ----------
  { rot = 1; applyRotation(); sudokuOpen(); dirty = true; loop(); if (sdkNewMenu) sdkNew(0); sdkPad = false; sdkSel = -1;
    SdkGeo g = sdkGeo(); onTap(g.x0 + 4 * g.cs + g.cs / 2, g.y0 + g.cs / 2); bool row0 = sdkSel == 4;
    sdkPad = false; int sx = g.x0 + g.cs * 9 + 6, sw = W - sx - 4; onTap(sx + sw / 2, 19); bool back = scr == S_GAMES;
    printf("3  wide Sudoku: tap a box in the top row=%d, tap the middle of Back=%d %s\n", row0, back, R(row0 && back));
    rot = 0; applyRotation(); scr = S_HOME; }

  // ---------- 4 USB drive page: the empty strip at the bottom does nothing ----------
  { scr = S_USB; usbAsk = false; dirty = true; render(); onTap(40, FTR_Y + 12); bool stay = scr == S_USB && !usbAsk;
    printf("4  USB drive page: tap under 'Leave USB drive' stays on the page=%d %s\n", stay, R(stay)); scr = S_HOME; }

  // ---------- 5 web PIN lock after millis() wraps (49.7 days) ----------
  { uint64_t keep = g_simMs; server.headers.clear(); server.simClient.local = WiFi.localIP(); apOn = true;
    g_simMs = 4000000000ULL; pinLockUntil = millis() + 60000;   // 5 wrong PINs on day 46
    g_simMs = 0x100000000ULL + 1000;                            // day 49.7: millis() starts again from 0
    server.headers["Cookie"] = std::string("stpin=") + webPin.c_str(); bool ok = webAuth();
    g_simMs = keep; pinLockUntil = 0; server.headers.clear();
    printf("5  PIN lock is over after millis() wraps (right PIN works)=%d %s\n", ok, R(ok)); }

  // ---------- 6 on-screen keyboard: '_' and '<' can be typed ----------
  { kbdOpen("Password", "", nullptr); dirty = true; render();
    auto typeChar = [](char ch) {
      const int layers = sizeof(KB_ROWS) / sizeof(KB_ROWS[0]);
      for (int L = 0; L < layers; L++) for (int r = 0; r < 3; r++) {
        const char* row = KB_ROWS[L][r]; const char* p = strchr(row, ch); if (!p) continue;
        kbdLayer = L; KbGeo g = kbGeo(); int n = strlen(row), off = (W - n * g.kw) / 2;
        onTap(off + (p - row) * g.kw + g.kw / 2, g.y0 + r * g.kh + g.kh / 2); return kbdText.endsWith(String(ch));
      }
      return false;
    };
    bool under = typeChar('_'), lt = typeChar('<');
    // the "123" key: 123 -> #+= -> 123, the "abc" key goes back
    kbdLayer = 0; KbGeo g = kbGeo(); int by = g.y0 + 3 * g.kh + g.kh / 2;
    onTap((int)(2.2f * g.kw), by); int l1 = kbdLayer; onTap((int)(2.2f * g.kw), by); int l2 = kbdLayer; onTap((int)(0.7f * g.kw), by); int l3 = kbdLayer;
    dirty = true; render(); savePng(spr, "t115_keyboard", W, H);
    printf("6  keyboard: '_' typed=%d, '<' typed=%d, 123 key: %d -> %d, abc key -> %d %s\n", under, lt, l1, l2, l3, R(under && lt && l1 == 2 && l2 == 3 && l3 == 0));
    scr = S_HOME; }

  // ---------- 7 a game left running pauses by itself, the screen dims and goes off ----------
  { offIdx = 1; lastTouchMs = millis();
    gameOpen(); run(100); press(); bool play = gs == G_PLAY;
    run(65000); bool swimPaused = gs == G_PAUSE && pw != P_ON && scr == S_GAME;
    press(); bool wokeOnly = pw == P_ON && gs == G_PAUSE; press(); bool goOn = gs == G_PLAY;
    hold(); bool swimOut = scr == S_GAMES;
    mazeOpen(); run(100); press(); bool mzPlay = mzState == MZ_PLAY;
    run(65000); bool mazePaused = mzState == MZ_PAUSE && pw != P_ON && scr == S_MAZE;
    press(); bool mzWoke = pw == P_ON && mzState == MZ_PAUSE && scr == S_MAZE;
    mazeLeave();
    blocksOpen(); run(100); press(); bool blPlay = blState == BL_PLAY;
    run(65000); bool blocksPaused = blState == BL_PAUSE && pw != P_ON && scr == S_BLOCKS;
    press(); bool blWoke = pw == P_ON && blState == BL_PAUSE; blocksLeave();
    // screen timer "Never": pause after 5 minutes without touching anything
    offIdx = 3; lastTouchMs = millis(); gameOpen(); run(100); press(); g_simMs += 301000; run(500); bool never = gs == G_PAUSE;
    hold(); lastTouchMs = millis();
    printf("7  games pause when left: Swim %d/%d (press wakes only %d, press plays %d, hold leaves %d), Maze %d/%d (wakes only %d), Blocks %d/%d (wakes only %d), 'Never' pauses after 5 min=%d %s\n",
           play, swimPaused, wokeOnly, goOn, swimOut, mzPlay, mazePaused, mzWoke, blPlay, blocksPaused, blWoke, never,
           R(play && swimPaused && wokeOnly && goOn && swimOut && mzPlay && mazePaused && mzWoke && blPlay && blocksPaused && blWoke && never));
    scr = S_GAMES; if (pw != P_ON) wake(); }

  // ---------- 7b a paused game left open with the screen off must not stop the reminders ----------
  { offIdx = 1; lastTouchMs = millis(); if (pw != P_ON) wake();
    mazeOpen(); run(100); press(); run(65000);   // paused by itself, screen off, still in the game
    bool dark = pw == P_OFF && scr == S_MAZE;
    remindedFor.clear(); remindAct = -1; lastSeen["water"] = (uint32_t)nowT() - 3 * 3600;
#ifdef FW_VERSION
    uint32_t b0 = beepCount;
#endif
    run(1500);
    bool woke = pw == P_ON && remindAct >= 0;
#ifdef FW_VERSION
    bool beep = beepCount == b0 + 1;
#else
    bool beep = false;
#endif
    printf("7b reminder while a paused game is open (screen off=%d): screen on + reminder=%d, beep=%d %s\n", dark, woke, beep, R(dark && woke && beep));
    if (scr == S_MAZE) mazeLeave(); offIdx = 3; lastTouchMs = millis(); remindAct = -1; scr = S_GAMES; }

  // ---------- 8 Maze, Blocks, Garden: hold the press 1 s = back to Games; [Pause] on the top bar ----------
  { offIdx = 3; lastTouchMs = millis();
    mazeOpen(); run(100); press(); run(300); hold(); bool mz = scr == S_GAMES;
    blocksOpen(); run(100); press(); run(300); hold(); bool bl = scr == S_GAMES;
    
#ifdef HAS_ANTS
    antOpen(); dirty = true; loop(); hold(); bool gd = scr == S_GAMES;   // v11.8: Ants took the Garden's place
#else
    gardenOpen(); dirty = true; loop(); hold(); bool gd = scr == S_GAMES;
#endif

    mazeOpen(); run(100); press(); run(200); mazeTapAt(W - 80, 10); bool mp = mzState == MZ_PAUSE && scr == S_MAZE;
    dirty = true; mazeDraw(); savePng(spr, "t115_maze_pause_btn", W, H); mazeLeave();
    blocksOpen(); run(100); press(); run(200); blTapAt(W - 80, 10); bool bp = blState == BL_PAUSE && scr == S_BLOCKS;
    blocksDraw(); savePng(spr, "t115_blocks_pause_btn", W, H); blocksLeave();
    printf("8  hold 1 s = Games: Maze=%d Blocks=%d Garden=%d, [Pause] button: Maze=%d Blocks=%d %s\n", mz, bl, gd, mp, bp, R(mz && bl && gd && mp && bp));
    scr = S_HOME; }

  // ---------- 11 upload refused while the PIN is locked ----------
  { server.headers.clear(); server.args.clear(); server.simClient.local = WiFi.localIP();
    server.headers["Cookie"] = std::string("stpin=") + webPin.c_str(); pinLockUntil = millis() + 60000;
    HTTPUpload& u = server.upload(); u.status = UPLOAD_FILE_START; u.filename = "evil.jpg"; u.currentSize = 0; server.args["dir"] = "/photos";
    apiUploadData(); bool refused = upErr.length() > 0; u.status = UPLOAD_FILE_END; apiUploadData();
    bool noFile = !SD_MMC.exists("/photos/evil.jpg");
    pinLockUntil = 0; server.headers.clear(); server.args.clear();
    printf("11 upload while the PIN is locked: refused=%d, no file written=%d %s\n", refused, noFile, R(refused && noFile)); }

  // ---------- 12 CSV: a unit with a comma stays in one column ----------
  { Act a; a.id = "run"; a.name = "Run"; a.unit = "km,ok"; acts.push_back(a); logEvent(acts.size() - 1, 2);
    server.args.clear(); server.args["days"] = "1"; server.lastBody = ""; apiExport();
    bool quoted = server.lastBody.indexOf("\"km,ok\"") >= 0;
    acts.pop_back(); server.args.clear();
    printf("12 CSV export: unit with a comma is quoted=%d %s\n", quoted, R(quoted)); }

  // ---------- 13 Bluetooth gives its memory back after a scan ----------
  { g_simBle = {{"Mi Smart Band 8", "c8:47:8c:1a:22:90", -52}}; g_simBleOn = false; scr = S_BT;
    btScan(); dirty = true; render(); bool listed = bdevs.size() == 1, freed = !g_simBleOn;
    btScan(); dirty = true; render(); bool again = bdevs.size() == 1 && !g_simBleOn;
    printf("13 Bluetooth: device listed=%d, BLE memory given back after the scan=%d, scan again works=%d %s\n", listed, freed, again, R(listed && freed && again));
    scr = S_HOME; }

  // ---------- 14 a download that cannot start does not leave "Loading..." for ever ----------
  { scr = S_NET; netTab = 1; newsCat = 1; newsOpen = -1; netBusy = false; netDone = false; netHasNext = false; WiFi.connected = true;
    g_simTaskFail = true; netLoad(true); bool notStuck = !netBusy, told = netMsg.length() > 0; g_simTaskFail = false;
    printf("14 download task cannot start: not stuck busy=%d, message shown=%d %s\n", notStuck, told, R(notStuck && told));
    netMsg = ""; scr = S_HOME; }

  // ---------- 15 a long unit on the number pad stays inside the box ----------
  { rot = 0; applyRotation(); Act a; a.id = "walk"; a.name = "Walk"; a.unit = "kilometres"; acts.push_back(a);
    kpAct = acts.size() - 1; kpVal = "12"; scr = S_KEYPAD; dirty = true; render();
    bool clean = true; for (int y = 66; y < 94; y++) for (int x = W - 8; x < W; x++) if (!pixIs(x, y, PAPER)) clean = false;
    savePng(spr, "t115_keypad_unit", W, H);
    acts.pop_back(); kpAct = -1; scr = S_HOME;
    printf("15 number pad: unit 'kilometres' stays inside the box=%d %s\n", clean, R(clean)); }

  // ---------- 16 About counts logs whose time is not sure ----------
  {
#ifdef FW_VERSION
    File f = logFs().open(logPath(curDay), "a"); f.print(String((unsigned long)nowT()) + ",water,1,H,0\n"); f.close(); logRev++;
    int n = unsureLogCount();
    printf("16 About: logs with an unsure time counted=%d %s\n", n, R(n == 1));
#else
    printf("16 About: logs with an unsure time are not counted %s\n", R(false));
#endif
  }

  // ---------- 18 switching tabs does not show the old error message ----------
  { scr = S_NET; netTab = 1; newsOpen = -1; wx.ok = false; remindAct = -1;
    netMsg = "Could not get the news. Tap Reload to try again."; g_simOffline = true; dirty = true; render();
    int ty = netTabY(), w = netTabW(); onTap(netTabX() + w / 2, ty + 10);   // tap Weather: spr = the picture drawn right after the tap
    bool clean = pixIs(12, netTop() + 16, PAPER) && netTab == 0;
    g_simOffline = false; netMsg = ""; scr = S_HOME;
    printf("18 News -> Weather: no old error box on the new tab=%d %s\n", clean, R(clean)); }

  // ---------- 19 version ----------
  {
#ifdef FW_VERSION
    scr = S_SET; setPage = 0; setScroll = 9999; dirty = true; render(); setScroll = 9999; dirty = true; render(); savePng(spr, "t115_settings_version", W, H);
    printf("19 version %s shown on the Settings page (see t115_settings_version.png) %s\n", FW_VERSION, R(FW_VERSION[0] == 'v' && (atoi(FW_VERSION + 1) > 11 || (atoi(FW_VERSION + 1) == 11 && atof(FW_VERSION + 4) >= 5))));   // v11.5 or newer
    setScroll = 0; scr = S_HOME;
#else
    printf("19 version only far down in About %s\n", R(false));
#endif
  }

  // ---------- 20 oil prices: petrol first (98, 95, 91, E20, E85), then diesel ----------
  {
#ifdef FW_VERSION
    const char* N[] = {"ดีเซล B20", "ไฮดีเซล S", "ไฮ พรีเมียม ดีเซล พลัส", "ไฮ พรีเมียม 98 พลัส",
                       "แก๊สโซฮอล์ E85 S EVO", "แก๊สโซฮอล์ E20 S EVO", "แก๊สโซฮอล์ 91 S EVO", "แก๊สโซฮอล์ 95 S EVO"};   // Bangchak, 26 Sep 2026
    std::vector<Oil> v; for (auto n : N) { Oil o; o.name = n; o.today = 40; o.tomorrow = 40; o.dif = 0; v.push_back(o); }
    oilSort(v);
    String got; for (auto& o : v) { String g, r; oilSplit(o.name, g, r); got += (g.length() ? g : r) + "|"; }
    String want = "98|95|91|E20|E85|Hi Diesel|B20|Hi Premium Diesel Plus|";
    printf("20 oil order: %s %s\n", got.c_str(), R(got == want));
#else
    printf("20 oil order: as Bangchak sends it (B20 first) %s\n", R(false));
#endif
  }

  // ---------- 22 air con: one message per tap ----------
  { int m0 = g_simIrMsgs; scr = S_AC; acSend(); int sent = g_simIrMsgs - m0; scr = S_HOME;
    printf("22 AC: one tap sends %d message (strong pin kept: %d) %s\n", sent, g_simDriveCap[21], R(sent == 1 && g_simDriveCap[21] == GPIO_DRIVE_CAP_3)); }
  return 0;
}
