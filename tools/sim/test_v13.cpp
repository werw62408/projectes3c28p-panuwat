#include "sim_common.h"
// v13 tests: the 1.3" small screen (SH1106 128x64), the button board in every page and game, the IR receiver KY-022
// (learn / send / use as buttons / the AC page follows the remote / Signals), the review fixes (reminder by id,
// power-cut-safe saves, 49.7-day timers, cut texts), the Joy-Con range. Pressed like a hand would, plus extreme cases.
// The small screen and the button board are "plugged in" before the start (the simulator's I2C parts).

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static void btnDown(int b) { g_simPadDown |= 1 << b; }
static void btnUp(int b) { g_simPadDown &= ~(1 << b); }
static void pad(int b, int holdMs = 100) { btnDown(b); run(holdMs); btnUp(b); run(120); }
static void finger(int x, int y, int ms = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(ms); g_simTouch = false; run(60); }
static int countOf(const char* id) { int n = 0; for (auto& e : todayEv) n += e.id == id; return n; }
static void irPushR(decode_type_t t, uint64_t v, uint16_t bits, bool rep, int pulses) {
  SimIrMsg m; m.r.decode_type = t; m.r.value = v; m.r.bits = bits; m.r.repeat = rep;
  for (int k = 0; k < pulses; k++) m.raw.push_back(k == 0 ? 9000 : k == 1 ? 4500 : (k % 2 ? 560 : (k % 6 == 0 ? 1690 : 560)));
  g_simIrIn.push_back(m);
}
static void irNec(uint32_t v) { irPushR(NEC, v, 32, false, 68); }
static void irNecRepeat() { irPushR(NEC, kRepeat, 0, true, 4); }
static void irSony(uint32_t v) { irPushR(SONY, v, 12, false, 26); }
static void irAc(bool pwr, uint8_t mode, uint8_t temp, uint8_t fan, uint8_t model) {
  SimIrMsg m; m.r.decode_type = PANASONIC_AC; m.r.bits = 27 * 8;
  m.r.state[0] = 0x02; m.r.state[1] = 0x20; m.r.state[13] = pwr; m.r.state[14] = mode; m.r.state[15] = temp; m.r.state[16] = fan;
  m.r.state[17] = kPanasonicAcSwingVAuto; m.r.state[18] = 0; m.r.state[19] = model;
  for (int k = 0; k < 440; k++) m.raw.push_back(k % 2 ? 430 : 1300);
  g_simIrIn.push_back(m);
}
static bool oledSameAsPicture() {   // the screen's memory holds exactly what the firmware drew (SH1106: columns 2..129)
  static uint8_t pg[OPG][OW]; oledToPages(pg);
  int off = oledChip == 0 ? 2 : 0;
  for (int p = 0; p < OPG; p++) for (int x = 0; x < OW; x++) if (g_simOled.ram[p][x + off] != pg[p][x]) return false;
  return true;
}
static bool oledLit(int x, int y) { return ospr.readPixel(x, y) != 0; }
static int simScript = 0, simCount = 0;   // what g_simDelayHook does while a window waits in its own loop
static void scriptHook() {
  simCount++;
  if (simScript == 1) { if (simCount == 20) btnDown(PB_A); if (simCount == 30) btnUp(PB_A); if (simCount > 3000) g_simTouch = true; }    // A in the photo viewer
  if (simScript == 2) { if (simCount == 20 || simCount == 90) { irNec(0x00FF00FF); } if (simCount > 3000) g_simTouch = true; }   // a remote's OK (twice: ring, then press) in "are you sure?"
}

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t13"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;   // Thu 24 Sep 2026, 20:15
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  g_simOledOn = true; g_simPadOn = true; g_simPadAddr = 0x20;   // plugged in before the start
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = true; WiFi.connected = true; remindAct = -1; animOn = false;

  printf("1  version %s %s\n", FW_VERSION, R(!strcmp(FW_VERSION, "v13")));

  // ================= the small screen =================
  { bool ok = oledOk && padOk && !joySwIo14 && irOn && g_simIrRxOn;
    printf("2  start: small screen=%d, buttons=%d (0x%02X), IO14 = IR receiver=%d, receiver on=%d %s\n", oledOk, padOk, padAddr, !joySwIo14, g_simIrRxOn, R(ok)); }
  { bool clean = g_simOled.dispOn && !g_simOled.onBeforePicture;
    printf("3  start: the screen is switched on only after its first picture (v12: noise during the start)=%d %s\n", clean, R(clean)); }
  { scr = S_HOME; dirty = true; run(300); oledT = 0; oledTask();
    bool edges = true; for (int p = 0; p < 8; p++) if (g_simOled.ram[p][0] || g_simOled.ram[p][1] || g_simOled.ram[p][130] || g_simOled.ram[p][131]) edges = false;
    bool same = oledSameAsPicture();
    printf("4  SH1106: the picture is in columns 2..129 (nothing in 0, 1, 130, 131)=%d, memory = the picture=%d %s\n", edges, same, R(edges && same)); }
  { uint8_t a[8][132]; memcpy(a, g_simOled.ram, sizeof a);
    oledChip = 1; oledFlipNow = 0xFF; memset(g_simOled.ram, 0, sizeof g_simOled.ram); oledT = 0; oledTask();
    bool shifted = true; for (int p = 0; p < 8; p++) for (int x = 0; x < 128; x++) if (g_simOled.ram[p][x] != a[p][x + 2]) shifted = false;
    oledChip = 0; oledFlipNow = 0xFF; oledT = 0; oledTask();
    printf("5  0.96 inch (SSD1306) setting: the same picture from column 0=%d %s\n", shifted, R(shifted)); }
  { setNow(T0 + 120); oledT = 0; oledTask(); long b0 = g_simOled.dataBytes;
    setNow(T0 + 121); oledT = 0; oledTask(); long blink = g_simOled.dataBytes - b0;
    gameOpen(); run(100); oledT = 0; oledTask(); long g0 = g_simOled.dataBytes;
    for (int k = 0; k < 3; k++) { setNow(T0 + 122 + k); run(400); }
    long game = g_simOled.dataBytes - g0;
    gs = G_READY; scr = S_GAMES; dirty = true; run(100);
    printf("6  only the change is sent: colon blink %ld bytes (whole screen 1024), 3 s in a game %ld bytes %s\n", blink, game, R(blink > 0 && blink < 80 && game == 0)); }
  { time_t n = nowT(); struct tm tm; localtime_r(&n, &tm); int sx = tm.tm_min % 3;
    goScreen(S_APPS); run(100); oledT = 0; oledTask();
    bool apps = oledLit(42 + sx + 2, 54) && !oledLit(sx + 2, 54);
    deckOpen(); run(100); oledT = 0; oledTask(); bool deck = oledLit(42 + sx + 2, 54); deckLeave();
    goScreen(S_SET); run(100); oledT = 0; oledTask(); bool set = oledLit(84 + sx + 2, 54) && !oledLit(42 + sx + 2, 54);
    goScreen(S_HOME); run(100); oledT = 0; oledTask(); bool log = oledLit(sx + 2, 54);
    printf("7  tabs on the small screen follow the page: Log=%d Apps=%d Deck (in Apps)=%d Settings=%d %s\n", log, apps, deck, set, R(log && apps && deck && set)); }
  { pw = P_OFF; powerLow(true); remindAct = 0; remindId = acts[0].id; oledT = 0; oledTask();
    bool dim = g_simOled.contrast == 0x01, noTabs = !oledLit(60, 63) && !oledLit(20, 63), remind = oledLit(2, 52);
    wake(); oledT = 0; oledTask(); bool bright = g_simOled.contrast == 0xCF; remindAct = -1;
    printf("8  big screen off: small screen dim=%d, clock only (no tabs)=%d, the reminder shown=%d; on again: bright=%d %s\n", dim, noTabs, remind, bright, R(dim && noTabs && remind && bright)); }
  { g_simI2cFail = 3; setNow(T0 + 200); oledT = 0; oledTask(); bool known = oledKnown != 0xFF;
    for (int k = 0; k < 5; k++) { oledT = 0; oledTask(); } bool back = oledSameAsPicture();
    printf("9  loose wire to the small screen: no crash, parts not sent are known=%d, all sent again after=%d %s\n", known, back, R(known && back)); }

  // ================= the button board =================
  { goScreen(S_HOME); run(100);
    pad(PB_B); bool a = scr == S_APPS; pad(PB_B); bool s = scr == S_SET; pad(PB_D); bool d = scr == S_APPS; pad(PB_C); bool c = scr == S_HOME;
    printf("10 buttons: B next tab (Apps=%d, Settings=%d), D tab before (Apps=%d), C = Log=%d %s\n", a, s, d, c, R(a && s && d && c)); }
  { remindAct = 0; remindId = acts[0].id; int n0 = countOf(acts[0].id.c_str());
    pad(PB_E); bool logged = countOf(acts[0].id.c_str()) == n0 + 1 && remindAct == -1;
    int p0 = oledPage; pad(PB_F); bool page = oledPage != p0; pad(PB_F);
    char pl = place; pad(PB_F, 1200); bool hold = place != pl; pad(PB_F, 1200);
    printf("11 E = +1 for the reminder=%d, F = small screen page=%d, hold F 1 s = Home/Uni=%d %s\n", logged, page, hold, R(logged && page && hold)); }
  { goScreen(S_HOME); navShow = false; run(100);
    pad(PB_A); bool ring = navShow && scr == S_HOME;   // the first press only shows the ring
    pad(PB_A); bool stats = scr == S_STATS;         // the ring starts on Stats (the first button)
    pad(PB_D); bool back = scr == S_HOME;
    joyNavOn = false; goScreen(S_STATS); run(100); pad(PB_D); bool back2 = scr == S_HOME; joyNavOn = true;
    printf("12 A: first press shows the ring=%d, then taps (Stats)=%d; D = Back=%d, also with Joystick in menus OFF=%d %s\n", ring, stats, back, back2, R(ring && stats && back && back2)); }
  { goScreen(S_HOME); navShow = false; run(100); pw = P_OFF; powerLow(true); remindAct = 0; remindId = acts[0].id; int n0 = countOf(acts[0].id.c_str());
    pad(PB_A); bool woke = pw == P_ON && scr == S_HOME && !navShow;
    pw = P_OFF; powerLow(true); pad(PB_E); bool noLog = countOf(acts[0].id.c_str()) == n0 && pw == P_ON;
    remindAct = -1;
    printf("13 pocket: a button with the screen off only wakes it (A: no tap=%d, E: no log=%d) %s\n", woke, noLog, R(woke && noLog)); }

  // ================= A in the games =================
  { gameOpen(); run(100); pad(PB_A); bool play = gs == G_PLAY; btnDown(PB_A); run(1200); bool out = scr == S_GAMES; btnUp(PB_A); run(200);
    bool noStray = scr == S_GAMES;
    mazeOpen(); run(100); pad(PB_A); bool mz = mzState == MZ_PLAY; btnDown(PB_A); run(1200); bool mzOut = scr == S_GAMES; btnUp(PB_A); run(300);
    bool mzStray = scr == S_GAMES;
    blocksOpen(); run(100); pad(PB_A); bool bl = blState == BL_PLAY; blocksLeave();
    sandOpen(); run(100); uint32_t d0 = saDrainT; pad(PB_A); bool sa = saDrainT != d0; sandClose();   // (an empty box closes the floor again at once)
    antOpen(); run(300); btnDown(PB_A); run(1300); bool an = scr == S_GAMES; btnUp(PB_A); run(200);
    sudokuOpen(); run(100); if (sdkNewMenu) { pad(PB_A); run(100); } int sel0 = sdkSel; pad(PB_A); bool sdk = sdkSel != sel0 || sdkPad; sudokuClose();
    printf("14 A works like the stick press: Swim start=%d hold=exit=%d, Maze=%d exit=%d, Blocks=%d, Sand empty=%d, Ants exit=%d, Sudoku=%d %s\n",
           play, out, mz, mzOut, bl, sa, an, sdk, R(play && out && mz && mzOut && bl && sa && an && sdk));
    printf("15 the A still held after leaving a game does nothing on the Games page (Swim=%d, Maze=%d) %s\n", noStray, mzStray, R(noStray && mzStray)); }
  { gbListOpen(); gbStart(gbRoms[0]); bool started = scr == S_GB;
    btnDown(PB_A); run(2300); bool menu = gbState == GB_PAUSE; btnUp(PB_A); run(200);
    pad(PB_A); bool resumed = gbState == GB_PLAY;
    pw = P_OFF; powerLow(true); pad(PB_C); bool woke = pw == P_ON && scr == S_GB;
    if (scr == S_GB) gbLeave(); scr = S_GAMES; dirty = true; run(100);
    printf("16 Game Boy: hold A 2 s = pause menu=%d, A = Resume=%d, screen off: any button wakes=%d %s\n", menu, resumed, woke, R(started && menu && resumed && woke)); }
  { filesOpen(); curDir = "/photos"; filesLoad(); int img = -1; for (size_t i = 0; i < fList.size(); i++) if (fList[i].type == FT_IMG) { img = i; break; }
    simScript = 1; simCount = 0; g_simDelayHook = scriptHook;
    if (img >= 0) runImageViewer(img);
    g_simDelayHook = nullptr; bool byA = !g_simTouch && simCount < 3000; g_simTouch = false; simScript = 0; btnUp(PB_A); run(200);
    bool noStray = scr == S_FILES && !fmViewing;
    printf("17 photo viewer: A = back (it waited for the stick only)=%d, no stray tap after=%d %s\n", byA, noStray, R(img >= 0 && byA && noStray)); }

  // ================= the IR receiver: learn, send, use as buttons =================
  for (auto& k : irKeys) k = IrKey(); irKeysSave();
  { remoteOpen(); dirty = true; run(100);
    int x, y, w, h; irKeyRect(0, x, y, w, h); finger(x + w / 2, y + h / 2);
    bool learning = irLearning();
    irNec(0x20DF10EF); run(100);
    bool kbd = scr == S_KBD && irKeys[0].used && irKeys[0].type == NEC && irKeys[0].value == 0x20DF10EF;
    KbGeo g = kbGeo(); int ky = g.y0 + 3 * g.kh + g.kh / 2;
    for (int k = 0; k < 6; k++) finger((int)(7.7f * g.kw), ky);          // Del x6: "Key 1" gone
    auto key = [&](char ch) { const char* row = KB_ROWS[0][0]; for (int r = 0; r < 3; r++) { row = KB_ROWS[0][r]; const char* p = strchr(row, ch); if (p) { int n = strlen(row), off = (W - n * g.kw) / 2; finger(off + (p - row) * g.kw + g.kw / 2, g.y0 + r * g.kh + g.kh / 2); return; } } };
    key('t'); key('v'); finger((int)(9.2f * g.kw), ky);                  // "tv" + OK
    bool named = scr == S_REMOTE && irKeys[0].name == "tv";
    File f = LittleFS.open("/irkeys.json", "r"); String js; while (f && f.available()) js += (char)f.read(); if (f) f.close();
    bool saved = js.indexOf("\"tv\"") >= 0 && js.indexOf("20DF10EF") >= 0;
    irKeys[0] = IrKey(); irKeysLoad(); bool loaded = irKeys[0].used && irKeys[0].value == 0x20DF10EF && irKeys[0].name == "tv";
    printf("18 learn: tap an empty key=%d, a remote key comes in -> kept + name keyboard=%d, named \"tv\"=%d, saved=%d, loads again=%d %s\n",
           learning, kbd, named, saved, loaded, R(learning && kbd && named && saved && loaded)); }
  { int x, y, w, h; irKeyRect(0, x, y, w, h); size_t o0 = g_simIrOut.size(); uint32_t c0 = irCount;
    finger(x + w / 2, y + h / 2); bool sent = g_simIrOut.size() == o0 + 1 && g_simIrOut.back().type == NEC && g_simIrOut.back().value == 0x20DF10EF && g_simIrOut.back().bits == 32;
    irNec(0x20DF10EF); run(40); bool echo = irCount == c0;   // the board hears its own LED: skipped
    run(600); irNec(0x20DF10EF); run(40); bool later = irCount == c0 + 1;
    printf("19 tap a learned key: sent the same code=%d, its own echo is skipped=%d, a real press later counts=%d %s\n", sent, echo, later, R(sent && echo && later)); }
  { int x, y, w, h; irKeyRect(0, x, y, w, h);
    g_simTouch = true; g_simTouchX = x + w / 2; g_simTouchY = y + h / 2; run(800); g_simTouch = false; run(60);
    bool menu = irMenu == 1;
    FmBox b = irMenuBox(); int mx, my, mw; irMenuBtn(b, 1, mx, my, mw); finger(mx + mw / 2, my + 16);   // "Use as"
    bool list = irMenu == 2;
    IrUseGrid ug = irUseGrid(); int ux, uy, uw; irUseBtn(ug, IA_DOWN, ux, uy, uw); finger(ux + uw / 2, uy + 12);
    bool set = irKeys[0].act == IA_DOWN && irMenu == 0;
    printf("20 hold a key: menu=%d, Use as=%d, Down chosen=%d %s\n", menu, list, set, R(menu && list && set)); }
  { goScreen(S_SET); setScroll = 0; navShow = false; dirty = true; run(300);
    irNec(0x20DF10EF); run(100); bool ring = navShow;
    int y0 = navFY; run(300); irNec(0x20DF10EF); run(100); int y1 = navFY;
    printf("21 a learned key as Down (on Settings): first press shows the ring=%d, next one moves it down (%d -> %d) %s\n", ring, y0, y1, R(ring && y1 > y0)); }
  { goScreen(S_SET); setScroll = 0; navShow = true; dirty = true; run(200); navCheckPage();
    int f0 = navFY, moves = 0, last = navFY;
    irNec(0x20DF10EF); run(30);
    for (int k = 0; k < 10; k++) { run(78); irNecRepeat(); run(30); if (navFY != last) { moves++; last = navFY; } }
    if (navFY != f0 && !moves) moves = 1;
    printf("22 a held remote key (NEC repeats for 1.1 s): Down keeps stepping like a held stick, %d steps (3..8 wanted) %s\n", moves, R(moves >= 3 && moves <= 8)); }
  { irKeys[1].used = true; irKeys[1].name = "Ch +"; irKeys[1].type = SONY; irKeys[1].bits = 12; irKeys[1].value = 0x090; irKeys[1].act = IA_TAB;
    goScreen(S_HOME); run(600);
    irSony(0x090); run(45); irSony(0x090); run(45); irSony(0x090); run(300);
    bool once = scr == S_APPS;
    irCtl = false; run(300); irSony(0x090); run(300); bool off = scr == S_APPS; irCtl = true;
    run(300); pw = P_OFF; powerLow(true); irSony(0x090); run(300); bool pocket = pw == P_ON && scr == S_APPS;
    printf("23 Sony sends each press 3 times: one tab step=%d; Control OFF: nothing=%d; screen off: only wakes=%d %s\n", once, off, pocket, R(once && off && pocket)); }
  { irKeys[2].used = true; irKeys[2].name = "Red"; irKeys[2].type = NEC; irKeys[2].bits = 32; irKeys[2].value = 0x00FF00FF; irKeys[2].act = IA_PLUS;
    remindAct = 0; remindId = acts[0].id; int n0 = countOf(acts[0].id.c_str()); run(300);
    irNec(0x00FF00FF); run(200); bool plus = countOf(acts[0].id.c_str()) == n0 + 1;
    printf("24 a learned key as +1 logs the reminder's activity=%d %s\n", plus, R(plus)); }
  { scr = S_AC; acS.temp = 26; acS.mode = 0; acS.fan = 0; acS.model = 0; acFollow = true; uint32_t s0 = acSyncs; run(600);
    irAc(true, kPanasonicAcCool, 23, kPanasonicAcFanHigh, kPanasonicJke); run(200);
    bool synced = acSyncs == s0 + 1 && acS.power && acS.temp == 23 && AC_MODE_V[acS.mode] == kPanasonicAcCool && AC_FAN_V[acS.fan] == kPanasonicAcFanHigh && AC_MODEL_V[acS.model] == kPanasonicJke && prefs.getUChar("acT", 0) == 23;
    acFollow = false; irAc(false, kPanasonicAcDry, 28, kPanasonicAcFanLow, kPanasonicJke); run(200); bool off = acS.temp == 23 && acS.power;
    acFollow = true; acSend(); irAc(true, kPanasonicAcCool, 30, kPanasonicAcFanLow, kPanasonicJke); run(100); bool echo = acS.temp == 23;
    printf("25 AC page follows the real remote: temp/mode/fan/type=%d, Follow OFF: unchanged=%d, its own send not taken back=%d %s\n", synced, off, echo, R(synced && off && echo)); }
  { bool order = irLogN >= 3 && irLog[0].ms >= irLog[1].ms;
    irSigView = true; scr = S_REMOTE; dirty = true; render(); savePng(spr, "t13_signals", W, H); irSigView = false;
    printf("26 Signals view: %d signals kept, newest first=%d %s\n", irLogN, order, R(order)); }
  { scr = S_REMOTE; dirty = true; run(100); irLearnStart(5); run(16000);
    bool ended = !irLearning() && irNote == "Nothing came in";
    irLearnStart(6); irNec(0x20DF10EF); run(100); bool dup = !irKeys[6].used && irNote.startsWith("Already learned");
    printf("27 learning: nothing for 15 s -> stops with a note=%d; a key already learned is not learned twice=%d %s\n", ended, dup, R(ended && dup)); }
  { scr = S_REMOTE; irLearnStart(7);
    SimIrMsg m; m.r.decode_type = UNKNOWN; m.r.value = 0x8A1B2C3D; m.r.bits = 32; for (int k = 0; k < 40; k++) m.raw.push_back(k % 2 ? 600 : 1200 + k);
    std::vector<uint16_t> want = m.raw; g_simIrIn.push_back(m); run(100);
    if (scr == S_KBD) { scr = S_REMOTE; }
    bool raw = irKeys[7].used && irKeys[7].raw == want;
    irSendKey(7); bool sent = g_simIrOut.back().raw == want;
    printf("28 an unknown remote: its pulses are kept=%d and sent back the same=%d %s\n", raw, sent, R(raw && sent)); }
  { irKeysSave(); writeFile(LittleFS.host("/irkeys.json.tmp"), "[{\"s\":0,\"n\":\"half");   // a power cut while saving
    irKeysLoad(); bool kept = irKeys[0].used && irKeys[0].name == "tv" && !LittleFS.exists("/irkeys.json.tmp");
    writeFile(LittleFS.host("/irkeys.json"), "[{\"s\":0,\"n\":\"brok");   // a broken file: no crash, nothing learned
    irKeysLoad(); bool broken = !irKeys[0].used;
    irKeys[0].used = true; irKeys[0].name = "tv"; irKeys[0].type = NEC; irKeys[0].bits = 32; irKeys[0].value = 0x20DF10EF; irKeysSave();
    printf("29 learned keys after a power cut while saving: kept=%d; a broken file does not crash=%d %s\n", kept, broken, R(kept && broken)); }

  // ================= the review fixes =================
  { // the reminder bar keeps its activity when the list is changed on the web page
    std::vector<Act> keep = acts;
    int wi = actIndexOf("water"); remindAct = wi; remindId = "water";
    JsonDocument d; JsonArray a = d.to<JsonArray>();
    for (int i = (int)acts.size() - 1; i >= 0; --i) { JsonObject o = a.add<JsonObject>(); o["id"] = acts[i].id; o["name"] = acts[i].name; o["unit"] = acts[i].unit; o["color"] = colorHex(acts[i].color); o["step"] = acts[i].step; o["goal"] = acts[i].goal; o["type"] = acts[i].goalType; o["remind"] = acts[i].remind; }
    String body; serializeJson(d, body); server.args["plain"] = body.std(); apiActsPost();
    bool moved = remindAct >= 0 && acts[remindAct].id == "water" && remindAct != wi; int now = remindAct;
    int n0 = countOf("water"); scr = S_APPS; run(100); pad(PB_E); bool right = countOf("water") == n0 + 1;
    printf("30 reminder bar after the list was changed on the web: still Water (place %d -> %d)=%d, E logs Water=%d %s\n", wi, now, moved, right, R(moved && right)); }
  { antOpen(); run(500); uint32_t w0 = an.workers; uint16_t r0 = an.nRooms; anSave(); antLeave();
    writeFile(LittleFS.host("/ants.bin.tmp"), std::string(1000, 'x'));    // switched off while it was saving
    anLoaded = false; anLoad(); bool kept = an.workers == w0 && an.nRooms == r0 && !LittleFS.exists("/ants.bin.tmp");
    writeFile(LittleFS.host("/ants.bin"), std::string(500, 'y'));         // a broken file
    anLoaded = false; anLoad(); bool bad = LittleFS.exists("/ants.bad.bin");
    printf("31 ant farm: a save cut by a power cut keeps the colony=%d; a broken file is kept as ants.bad.bin=%d %s\n", kept, bad, R(kept && bad)); }
  { sdMount(); SD_MMC.mkdir("/roms"); std::string data(8192, 'S'); writeFile(SD_MMC.host("/roms/test.sav"), std::string(8192, 'O'));
    bool ok = writeBytesSafe(SD_MMC, "/roms/test.sav", (const uint8_t*)data.data(), data.size());
    File f = SD_MMC.open("/roms/test.sav", "r"); bool full = f && f.size() == 8192 && f.read() == 'S'; if (f) f.close();
    writeFile(SD_MMC.host("/roms/test.sav.ok"), std::string(8192, 'N')); SD_MMC.remove("/roms/test.sav");   // cut between the steps
    fileRepair(SD_MMC, "/roms/test.sav"); f = SD_MMC.open("/roms/test.sav", "r"); bool rep = f && f.size() == 8192 && f.read() == 'N'; if (f) f.close();
    printf("32 Game Boy save on the card: written whole=%d; cut between the steps: the finished copy is used=%d %s\n", ok && full, rep, R(ok && full && rep)); }
  { int lo = 0, hi = 4095, c = 2048;
    float wide = joyAxis(3500, c, lo, hi);
    int lo2 = 600, hi2 = 3500; float joy = joyAxis(3500, c, lo2, hi2), half = joyAxis(c + 726, c, lo2, hi2);
    joyAxis(3700, c, lo2, hi2); bool widened = hi2 == 3700;
    printf("33 Joy-Con range: a stick that stops at 3500 = %.2f before, %.2f after the range step (half way %.2f), went further = edge moves=%d %s\n",
           wide, joy, half, widened, R(wide < 0.8f && joy > 0.99f && half > 0.4f && half < 0.6f && widened)); }
  { // 49.7 days: millis() wraps. Sudoku: holding the stick keeps moving; the small screen and the receiver keep working
    g_simMs = 0x100000000ULL - 3000; setNow(T0 + 4000);
    sudokuOpen(); run(100); if (sdkNewMenu) sdkNew(0); sdkSel = 0; sdkJoyUsed = true; sdkPad = false;
    g_simAnalog[JOY_X] = 4000; int moved = 0, prev = sdkSel;
    for (int k = 0; k < 250; k++) { run(20); if (sdkSel != prev) { moved++; prev = sdkSel; } }   // 5 s held right, across the wrap
    g_simAnalog[JOY_X] = 2048; run(100);
    sudokuClose();
    long b0 = g_simOled.dataBytes; run(2500); bool oled = g_simOled.dataBytes > b0;
    irSendKey(0); irNec(0x20DF10EF); uint32_t c0 = irCount; run(40); bool quiet = irCount == c0; run(600); irNec(0x20DF10EF); run(40); bool hear = irCount == c0 + 1;
    printf("34 after 49.7 days on: Sudoku hold keeps moving (%d steps)=%d, small screen updates=%d, receiver skips its echo=%d and hears later=%d %s\n",
           moved, moved > 10, oled, quiet, hear, R(moved > 10 && oled && quiet && hear)); }
  { sdMount(); std::string v; for (int k = 0; k < 30; k++) { v += std::string("\xFF\xD8", 2); v += std::string(300, 'j'); v += std::string("\xFF\xD9", 2); }
    writeFile(SD_MMC.host("/videos/t13.mjpeg"), v);
    g_simMs = 0x100000000ULL - 900; uint64_t t0 = g_simMs;
    runVideo("/videos/t13.mjpeg"); uint64_t took = g_simMs - t0;
    printf("35 a clip playing across 49.7 days: ends normally in %.1f s (2 s of pictures; it could stop for 49 days) %s\n", took / 1000.0, R(took < 6000)); }
  { bool fits = true; String bad;
    for (int th : {0, 2}) for (int r : {0, 1}) {
      themeSet(th); rot = r; applyRotation(); layoutUpdate();
      auto chk = [&](const lgfx::IFont* f, const String& s, int w) { spr.setFont(pickFont(f, s)); if ((int)spr.textWidth(s) > w) { fits = false; bad += s + "|"; } };
      int CW = W - 16, seg3 = (CW - 12) / 3, seg2 = (CW - 6) / 2;
      for (const char* s : {"Normal", "Flip", "Off"}) chk(FS, hudUp(s), seg3 - 4);
      for (const char* s : {"1.3 inch", "0.96 inch"}) chk(FS, hudUp(s), seg2 - 4);
      chk(FS, hudUp("Control: OFF"), W - 82 - 4);
      chk(FS, hudUp("Follow real remote: OFF"), CW - 4);
      chk(FB, hudUp("Log FULL: not saved"), W - 50);
      chk(FB, hudUp("Button E: loose?"), W - 24);
      chk(FS, hudUp("// Screen off after"), CW);
    }
    themeSet(0); rot = 0; applyRotation();
    printf("36 texts that were cut now fit (Light, HUD, tall, wide)%s%s %s\n", fits ? "" : ": ", bad.c_str(), R(fits)); }
  { scr = S_APPS; remindAct = 0; remindId = acts[0].id; layoutUpdate(); dirty = true; render();
    bool clear = true; for (int i = 0; i < N_APPS; i++) { int x, y, w, h; appTileRect(i, x, y, w, h); if (y + h > remindBarY) clear = false; }
    savePng(spr, "t13_apps_remind", W, H); remindAct = -1;
    printf("37 Apps: the reminder bar no longer covers the last row of tiles=%d %s\n", clear, R(clear)); }
  { goScreen(S_SET); setOpenPage(1); setScroll = 9999; dirty = true; render(); setScroll = 9999; dirty = true; render();
    setScroll = 0; dirty = true; render(); int found = setSizeRowY;
    if (found > FTR_Y - 40) { setScroll = found - 100; dirty = true; render(); }
    bool before = oledChip == 0;
    finger(8 + (W - 16) * 3 / 4, setSizeRowY + 16);   // "0.96 inch"
    bool chip = oledChip == 1 && prefs.getUChar("oledChip", 0) == 1;
    setAboutPage(); dirty = true; render(); bool about = irAboutText().startsWith("OK");
    oledChip = 0; prefs.putUChar("oledChip", 0); oledFlipNow = 0xFF; setOpenPage(0);
    printf("38 Settings > Screen: small screen size 0.96 inch saved=%d (was 1.3=%d); About: IR receiver \"%s\" %s\n", chip, before, irAboutText().c_str(), R(chip && before && about)); (void)found; }
  { // a remote's OK / Back also works in the windows that wait in their own loop
    irKeys[2].used = true; irKeys[2].name = "OK"; irKeys[2].type = NEC; irKeys[2].bits = 32; irKeys[2].value = 0x00FF00FF; irKeys[2].act = IA_OK;
    simScript = 2; simCount = 0; g_simDelayHook = scriptHook;
    bool r = lcdConfirm("Move to Trash?", "test.jpg");   // the ring starts on nothing: the first OK selects Cancel, so this OK = "show the ring"
    g_simDelayHook = nullptr; bool byIr = !g_simTouch && simCount < 3000; g_simTouch = false; simScript = 0;
    printf("39 a learned OK key works in \"are you sure?\" (it waited for the stick only)=%d (answer %d) %s\n", byIr, r, R(byIr)); }
  { // the old wiring: no button board = IO14 is still the stick press (old tests use it), the receiver stays off
    joySwIo14 = true; g_simDigital[JOY_SW] = LOW; g_simMs += 20; bool down = joyDown(); g_simDigital[JOY_SW] = HIGH; g_simMs += 20; bool up = !joyDown(); joySwIo14 = false;
    printf("40 old wiring (no button board): IO14 low = the stick press=%d, high = not=%d %s\n", down, up, R(down && up)); }
  { // first start with nothing saved: the Remotes page and the small screen work with empty lists
    for (auto& k : irKeys) k = IrKey(); LittleFS.remove("/irkeys.json"); irKeysLoad();
    remoteOpen(); dirty = true; render(); bool empty = true; for (auto& k : irKeys) if (k.used) empty = false;
    irPage = 1; dirty = true; render(); bool page2 = irPerPage() * irPages() >= IR_KEYS;
    savePng(spr, "t13_remotes_empty", W, H); remoteLeave();
    printf("41 first start: Remotes page empty=%d, 24 keys over %d pages=%d %s\n", empty, irPages(), page2, R(empty && page2)); }
  { // the key windows stay between the bars on every screen shape (wide with both bars = only 172 px), and every button still works
    for (auto& k : irKeys) k = IrKey();
    irKeys[0].used = true; irKeys[0].name = "Vol +"; irKeys[0].type = NEC; irKeys[0].bits = 32; irKeys[0].value = 0x20DF40BF;
    irLogN = 0; for (int i = 0; i < 8; i++) { irNec(0x20DF0000 + i); run(300); }
    themeSet(0); bool fit = true, taps = true, rows = true; std::string bad;
    for (int r : {0, 1}) for (int bm : {0, 1}) {
      std::string tag = std::string(r ? " wide" : " tall") + (bm ? "+bars" : "");
      barsMode = bm; rot = r; applyRotation(); remoteOpen(); layoutUpdate();
      irMenuKey = 0; irMenu = 1; dirty = true; render();
      FmBox b = irMenuBox(); if (b.y < HDR_H || b.y + b.h > FTR_Y) { fit = false; bad += tag + " menu"; }
      if (r && bm) savePng(spr, "t13_menu_wide_bars", W, H);
      int x, y, w; irMenuBtn(b, 4, x, y, w); finger(x + w / 2, y + 16); if (irMenu != 0) { taps = false; bad += tag + " Cancel"; }
      irMenu = 2; dirty = true; render();
      IrUseGrid g = irUseGrid(); if (g.b.y < HDR_H || g.b.y + g.b.h > FTR_Y || g.step - 6 < 24) { fit = false; bad += tag + " Use as"; }
      if (r && bm) savePng(spr, "t13_useas_wide_bars", W, H);
      int ux, uy, uw; irUseBtn(g, IA_PLUS, ux, uy, uw); finger(ux + uw / 2, uy + (g.step - 6) / 2);
      spr.setFont(FS); if ((int)spr.textWidth(IA_NAMES[IA_PLUS]) > uw - 8) { fit = false; bad += tag + " \"+1 (remind)\" cut"; }
      if (irKeys[0].act != IA_PLUS || irMenu) { taps = false; bad += tag + " +1"; } irKeys[0].act = IA_NONE;
      irSigView = true; dirty = true; render();   // the Signals list: no half row under the bottom tabs
      if (spr.readPixel(20, FTR_Y - 3) != spr.readPixel(2, FTR_Y - 3)) { rows = false; bad += tag + " signals"; }
      if (r && bm) savePng(spr, "t13_signals_wide_bars", W, H);
      irSigView = false;
    }
    barsMode = 0; rot = 0; applyRotation(); remoteLeave();
    printf("42 Remotes windows fit between the bars (tall/wide, bars on/off)=%d, Cancel and \"+1\" taps work=%d, Signals shows whole rows only=%d%s %s\n",
           fit, taps, rows, bad.c_str(), R(fit && taps && rows)); }
  { // long names and Thai names on the small screen: cut to fit, never inside a Thai letter, the numbers / "E:+1" stay in sight
    String n0 = acts[0].name, n1 = acts[1].name;
    String th = "ดื่มน้ำเปล่าวันละแปดแก้วนะจ๊ะ", en = "Drink a big glass of water now please";
    auto utf8ok = [](const String& s) { for (size_t i = 0; i < s.length();) { uint8_t b = s[i]; int n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
                                          if (!n || i + n > s.length()) return false; for (int k = 1; k < n; k++) if (((uint8_t)s[i + k] & 0xC0) != 0x80) return false; i += n; } return true; };
    bool cut = true;
    for (int w : {20, 50, 80, 100}) { String c = ofit(th, w); if (otw(c) > w || !utf8ok(c)) cut = false; String e = ofit(oUp(en), w); if (otw(e) > w) cut = false; }
    acts[0].name = th; acts[1].name = en; pw = P_ON; wake(); scr = S_HOME; popAct = -1; burstNoticeMs = 0; logFull = false;
    remindAct = 0; remindId = acts[0].id;
    if (time(nullptr) & 1) g_simEpoch += 1;   // an even second: the reminder line is not inverted (it blinks)
    oledT = 0; oledTask(); oledPng("t13_oled_thai_remind");
    bool eShown = false; for (int x = 100; x < 126; x++) for (int y = 43; y < 51; y++) if (oledLit(x, y)) eShown = true;   // "E:+1" at the right end
    remindAct = 1; remindId = acts[1].id; oledT = 0; oledTask(); oledPng("t13_oled_long_remind");
    remindAct = -1; oledPage = 1; oledT = 0; oledTask(); oledPng("t13_oled_goals_long");
    bool nums = true; for (int r = 0; r < 2; r++) { bool lit = false; for (int x = 112; x < 126; x++) for (int y = 12 + r * 10; y < 20 + r * 10; y++) if (oledLit(x, y)) lit = true; if (!lit) nums = false; }
    oledPage = 0; popAct = 0; popMs = millis(); popText = "+1"; oledT = 0; oledTask(); oledPng("t13_oled_thai_plus1"); popAct = -1;
    pw = P_OFF; remindAct = 0; oledT = 0; oledTask(); oledPng("t13_oled_thai_off"); remindAct = -1; wake();
    acts[0].name = n0; acts[1].name = n1; oledT = 0; oledTask();
    printf("43 long / Thai names on the small screen: cut to fit and never inside a Thai letter=%d, \"E:+1\" still shown=%d, goal numbers shown=%d %s\n",
           cut, eShown, nums, R(cut && eShown && nums)); }
  return 0;
}
