#include "sim_common.h"
#include <chrono>
// v11.7 tests: the 6 fixes from the v11.6 review, the Game Boy app, Deck (USB + Bluetooth keys).
// Played like a person: taps, stick pushes and presses; plus extreme cases.

static void run(int ms) { for (int t = 0; t < ms; t += 25) { g_simMs += 20; loop(); } }   // loop() itself adds 5 ms
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void runFor(uint32_t ms) { uint32_t t = millis(); while (millis() - t < ms) { g_simMs += 20; loop(); } }   // by the clock (game loops are slower)
static void holdPress(uint32_t ms) { g_simDigital[JOY_SW] = LOW; runFor(ms); g_simDigital[JOY_SW] = HIGH; run(100); }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void ftap(int x, int y, int holdMs = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(holdMs); g_simTouch = false; run(60); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static const uint32_t DAY = 86400000UL;
static std::string readAll(const std::string& p) { FILE* f = fopen(p.c_str(), "rb"); if (!f) return ""; std::string s; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n); fclose(f); return s; }
static uint32_t fbHash() { uint32_t h = 2166136261u; if (!gbFb) return 0; for (int i = 0; i < GB_DW * GB_DH; i++) h = (h ^ gbFb[i]) * 16777619u; return h; }
static void tileTap(int i) { int x, y, w, h; gamesTileRect(i, x, y, w, h); ftap(x + w / 2, y + h / 2); }
static std::string serialOut;
static void serTx(struct gb_s*, const uint8_t b) { serialOut += (char)b; }
static enum gb_serial_rx_ret_e serRx(struct gb_s*, uint8_t* b) { *b = 0xFF; return GB_SERIAL_RX_NO_CONNECTION; }

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t117"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  // games on the card: two real ones (free), and three bad files
  SD_MMC.mkdir("/roms");
  writeFile(SD_MMC.host("/roms/2048.gb"), readAll("roms/2048.gb"));
  writeFile(SD_MMC.host("/roms/cpu_instrs.gb"), readAll("roms/cpu_instrs.gb"));
  { std::string c = readAll("roms/2048.gb"); c[0x143] = (char)0xC0; writeFile(SD_MMC.host("/roms/color_only.gb"), c); }
  writeFile(SD_MMC.host("/roms/tiny.gb"), std::string(100, 'x'));
  writeFile(SD_MMC.host("/roms/notes.txt"), "not a game");
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3;
  g_simOffline = false; WiFi.connected = true;

  // ================= the 6 fixes =================
  // F1 reminder sound: after the last note there is at least 90 ms of silence (what the sound chip still holds)
  { i2sOn = true; codecOk = true; volIdx = 2; g_simI2S.clear();
    remindBeep();
    int last = -1; for (int i = 0; i < (int)g_simI2S.size(); i++) if (g_simI2S[i]) last = i;
    int tail = (int)g_simI2S.size() - 1 - last;
    printf("F1 reminder sound: %d samples of silence after the last note (need >= 1440 = 90 ms) %s\n", tail, R(tail >= 1440)); }
  // F2 Dragon after 26 days switched on: no dash by itself, a press dashes
  { g_simMs = 26ULL * DAY; gameOpen(); run(100); press(); run(300); bool before = dashing(); press(); run(40); bool during = dashing(); run(1500);
    printf("F2 Dragon after 26 days on: dashing without a press=%d, a press dashes=%d %s\n", before, during, R(!before && during));
    g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(200); g_simMs = DAY / 2; }
  // F3 Bluetooth: the scan ends while another page is open -> the memory is given back at once
  { btPageOpen(); dirty = true; run(100); btScan(); goScreen(S_APPS); dirty = true; run(100); btDone = true; run(500);
    printf("F3 Bluetooth scan ends on another page: Bluetooth off again=%d %s\n", !BLEDevice::getInitialized(), R(!BLEDevice::getInitialized())); }
  // F4 About after 3 years of logs: only the new day is read again after a log
  { for (int d = 15; d < 3 * 365; d++) { time_t day = T0 - (time_t)d * 86400; std::string out;
      for (int k = 0; k < 30; k++) { char b[64]; snprintf(b, sizeof b, "%u,water,1,H,%d\n", (unsigned)(day - 43200 + k * 600), k % 7 ? 1 : 0); out += b; }
      writeFile(logFs().host("/log/" + dayKey(day) + ".csv"), out); }
    unsureAllRead = false;
    int first = unsureLogCount();
    timeApprox = true; logEvent(0, 1); timeApprox = false;   // a log on a guessed time
    auto t = std::chrono::steady_clock::now(); int second = unsureLogCount();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
    printf("F4 Time unsure: %d, after one more guessed log %d (+1 expected), counted again in %.2f ms (only today read) %s\n", first, second, ms, R(second == first + 1 && ms < 5)); }
  // F5 hotspot rests 10 min after the screen goes off, also on News (1 min longer there)
  { offIdx = 1; apOn = true; apAsleep = false; scr = S_NET; netTab = 0; dirty = true; wake(); run(100);
    uint32_t offAt = 0, apAt = 0; for (int s = 0; s < 14 * 60 && !apAt; s++) { run(1000); if (!offAt && pw == P_OFF) offAt = millis(); if (apAsleep) apAt = millis(); }
    float after = apAt ? (apAt - offAt) / 60000.0f : -1;
    printf("F5 News page: hotspot rests %.1f min after the screen went off (10 expected) %s\n", after, R(after >= 9.95f && after <= 10.6f));
    wake(); apAsleep = false; offIdx = 3; scr = S_HOME; dirty = true; run(100); }
  // F6 Sudoku clock: pausing often keeps every millisecond
  { sudokuOpen(); run(200); if (sdkNewMenu) { press(); run(100); }
    uint32_t a = sdkSecs(); for (int k = 0; k < 200; k++) { sdkNewMenu = !sdkNewMenu; run(300); } sdkNewMenu = false; run(100);
    uint32_t b = sdkSecs();
    printf("F6 Sudoku clock with 200 quick pauses over 60 s: +%u s (about 30 expected) %s\n", b - a, R(b - a >= 28 && b - a <= 32));
    sudokuClose(); run(100); }

  // ================= Game Boy =================
  scr = S_GAMES; dirty = true; run(100);
  // G1 the list: only real games, sorted; opened from the Games page by a tap
  { tileTap(6); run(200); std::string names; for (auto& r : gbRoms) names += std::string(r.name.c_str()) + " ";
    shot("t117_gb_list");
    printf("G1 Games > Game Boy: list \"%s\" (4 .gb files, not the .txt) %s\n", names.c_str(), R(scr == S_GBLIST && gbRoms.size() == 4)); }
  auto openRom = [&](const char* name) { for (size_t i = 0; i < gbRoms.size(); i++) if (gbRoms[i].name == name) { int top = gbListTop(); ftap(W / 2, top + (int)i * GB_ROW_H + 18); return; } };
  // G2 bad files: a message, no crash, still on the list
  { openRom("color_only"); bool m1 = gbMsg.indexOf("Color") >= 0 && scr == S_GBLIST;
    openRom("tiny"); bool m2 = gbMsg.indexOf("Not a Game Boy") >= 0 && scr == S_GBLIST;
    dirty = true; render(); savePng(spr, "t117_gb_badfile", W, H);
    printf("G2 bad games: Color-only says so=%d, too small says so=%d, still on the list %s\n", m1, m2, R(m1 && m2)); }
  // G3 the CPU test game: runs at the right speed and passes every test (a free test program for Game Boy emulators)
  { openRom("cpu_instrs"); bool on = scr == S_GB; serialOut.clear(); if (gbCore) gb_init_serial(gbCore, serTx, serRx);
    uint32_t f0 = gbFrames; uint32_t t0 = millis();
    for (int s = 0; s < 70 && serialOut.find("Passed all tests") == std::string::npos && serialOut.find("Failed") == std::string::npos; s++) { run(1000); lastTouchMs = millis(); }
    float fps = (gbFrames - f0) * 1000.0f / (millis() - t0);
    shotLcd("t117_gb_cpu_instrs");
    bool passed = serialOut.find("Passed all tests") != std::string::npos;
    printf("G3 cpu_instrs: started=%d, %.1f frames a second (59.7 wanted), result: %s %s\n", on, fps, passed ? "Passed all tests" : serialOut.substr(0, 60).c_str(), R(on && passed && fps > 58 && fps < 61.5f));
    holdPress(2200);   // hold 2 s: pause menu
    bool menu = gbState == GB_PAUSE; push(0, 1); press(); bool back = scr == S_GBLIST;
    printf("G4 hold the press 2 s = pause menu=%d, down + press = Save and exit, back on the list=%d %s\n", menu, back, R(menu && back)); }
  // G5 2048 with the stick only: press = A starts, pushes slide the tiles (the picture changes), the game's save is written
  { openRom("2048"); run(3000); lastTouchMs = millis();
    uint32_t h0 = fbHash(); press(); run(1500); uint32_t h1 = fbHash();   // title -> game
    int changes = 0; uint32_t prev = h1;
    const int dirs[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 0}, {0, 1}, {-1, 0}, {0, 1}};
    for (auto& d : dirs) { push(d[0], d[1], 150); run(600); lastTouchMs = millis(); uint32_t h = fbHash(); if (h != prev) changes++; prev = h; }
    shotLcd("t117_gb_2048"); gbDrawChrome(); savePng(spr, "t117_gb_2048_full", W, H);
    printf("G5 2048 by stick only: press starts=%d, %d of 8 pushes moved the tiles %s\n", h1 != h0, changes, R(h1 != h0 && changes >= 5));
    // G6 top bar [Exit] saves the game's RAM (2048 keeps its best score in battery RAM)
    bool hadRam = gbRamSize > 0; ftap(W - 10, 10); bool left = scr == S_GBLIST;
    bool sav = SD_MMC.exists("/roms/2048.sav");
    printf("G6 [Exit] on the top bar: back to the list=%d, save file written=%d (game has battery RAM=%d) %s\n", left, sav, hadRam, R(left && (!hadRam || sav))); }
  // G7 pocket: screen off during a game, the stick pushed and pressed: nothing runs, a press only wakes
  { offIdx = 0; openRom("2048"); run(2000); press(); run(500);
    run(45000); bool paused = gbState == GB_PAUSE, off = pw == P_OFF;
    uint32_t f0 = gbFrames, h0 = fbHash();
    for (int k = 0; k < 6; k++) push(k % 2 ? 1 : -1, k % 3 - 1);
    bool still = gbFrames == f0 && pw == P_OFF;
    press(); bool woke = pw == P_ON && scr == S_GB && gbState == GB_PAUSE && fbHash() == h0;
    printf("G7 pocket: left alone it pauses=%d and turns off=%d, pushes run nothing=%d, a press only wakes (still paused)=%d %s\n", paused, off, still, woke, R(paused && off && still && woke));
    press(); bool resumed = gbState == GB_PLAY;   // next press on "Resume"
    printf("G8 then a press on Resume plays on=%d %s\n", resumed, R(resumed));
    offIdx = 3; ftap(W - 10, 10); run(100); }
  // G9 wide screen: the picture in the middle, the touch buttons at the sides (tap START / A)
  { rot = 1; applyRotation(); openRom("2048"); run(1500); lastTouchMs = millis();
    GbKey k[4]; gbKeys(k); bool sides = k[0].x == 0 && k[2].x == W - 40 && gbX0() == 40;
    uint32_t h0 = fbHash(); g_simTouch = true; g_simTouchX = k[3].x + 20; g_simTouchY = k[3].y + 40; run(200); g_simTouch = false; run(1500); bool aWorks = fbHash() != h0;
    gbDrawChrome(); savePng(spr, "t117_gb_wide", W, H);   // (the simulator screen does not turn: the picture is taken from the drawing)
    printf("G9 wide screen: picture at x=%d with buttons at the sides=%d, touch A starts the game=%d %s\n", gbX0(), sides, aWorks, R(sides && aWorks));
    ftap(W - 10, 10); rot = 0; applyRotation(); run(100); }
  // G10 extreme: the SD card is pulled out while playing (the game is in memory): it plays on, leaving can't save but doesn't crash
  { openRom("2048"); run(1000); press(); run(500); SD_MMC.present = false; sdOk = false; gbRamDirty = true;
    lastTouchMs = millis(); uint32_t f0 = gbFrames; runFor(2000); bool plays = gbFrames > f0 + 110;   // 2 s = about 119 frames
    ftap(W - 10, 10); bool back = scr == S_GBLIST;
    printf("G10 SD card pulled mid-game: plays on=%d, [Exit] still goes back (no crash)=%d %s\n", plays, back, R(plays && back));
    SD_MMC.present = true; sdMount(); scr = S_GAMES; dirty = true; run(100); }
  // G11 extreme: 200 fast taps on the list + stick mashing in the game: nothing sticks, memory is given back each time
  { gbListOpen(); run(100); size_t freeCnt = 0;
    for (int k = 0; k < 5; k++) { openRom("2048"); for (int j = 0; j < 20; j++) { push(j % 2 ? 1 : -1, 0, 40); press(30); } ftap(W - 10, 10); if (!gbCore && !gbRom && !gbFb) freeCnt++; }
    printf("G11 open / mash / exit 5 times: memory freed each time=%zu of 5, back on the list=%d %s\n", freeCnt, scr == S_GBLIST, R(freeCnt == 5 && scr == S_GBLIST)); }

  // ================= Deck =================
  auto keyTap = [&](int i) { int x, y, w, h; deckKeyRect(i % DECK_PER_PAGE, x, y, w, h); if (deckPage != i / DECK_PER_PAGE) ftap(W - 38, HDR_H + 48); ftap(x + w / 2, y + h / 2); };
  auto downs = [&](size_t from) { int d = 0; for (size_t i = from; i < g_simDeck.size(); i++) if (g_simDeck[i].down) d++; return d; };
  auto ups = [&](size_t from) { int u = 0; for (size_t i = from; i < g_simDeck.size(); i++) if (!g_simDeck[i].down) u++; return u; };
  // D1 Apps > Deck
  { goScreen(S_APPS); dirty = true; run(100); int x, y, w, h; appTileRect(4, x, y, w, h); ftap(x + w / 2, y + h / 2); run(100);
    shot("t117_deck");
    printf("D1 Apps > Deck: opened=%d, USB mode, status \"%s\" %s\n", scr == S_DECK, deckStatus().c_str(), R(scr == S_DECK && deckVia == DV_USB && deckReady())); }
  // D2 media key over USB: press and let go
  { size_t n0 = g_simDeck.size(); keyTap(0); bool ok = g_simDeck.size() == n0 + 2 && g_simDeck[n0].via == 'U' && g_simDeck[n0].media == 0xCD && g_simDeck[n0].down && !g_simDeck[n0 + 1].down;
    printf("D2 tap Play/Pause: USB media key 0xCD down + up %s\n", R(ok)); }
  // D3 page 2, Copy = Ctrl+C
  { size_t n0 = g_simDeck.size(); keyTap(12); bool ok = g_simDeck.size() == n0 + 2 && g_simDeck[n0].mod == DM_CTRL && g_simDeck[n0].key == 0x06 && g_simDeck[n0 + 1].key == 0;
    shot("t117_deck_page2");
    printf("D3 page 2, tap Copy: Ctrl + C then released=%d, page shown %d/2 %s\n", ok, deckPage + 1, R(ok && deckPage == 1)); }
  // D4 a text key types each letter (Shift for capitals and "!")
  { size_t n0 = g_simDeck.size(); keyTap(23); int typed = downs(n0); int shifted = 0; for (size_t i = n0; i < g_simDeck.size(); i++) if (g_simDeck[i].down && g_simDeck[i].mod == DM_SHIFT) shifted++;
    printf("D4 tap Hello: %d keys typed for \"%s\" (%d with Shift: H, S, T, !) %s\n", typed, deckKeys[23].text.c_str(), shifted, R(typed == (int)deckKeys[23].text.length() && shifted == 4)); }
  // D5 joystick only: move the ring to a key and press
  { deckPage = 0; dirty = true; run(100); size_t n0 = g_simDeck.size(); navShow = false;
    push(0, 1); push(0, 1); push(0, 1); press();
    printf("D5 joystick: 3 pushes + press sent a key=%d %s\n", downs(n0) == 1, R(downs(n0) == 1)); }
  // D6 Bluetooth: not paired -> nothing sent + a note; paired -> sent over Bluetooth
  { ftap(W - 38, HDR_H + 17); bool bt = deckVia == DV_BT && deckBleOn;
    size_t n0 = g_simDeck.size(); keyTap(1); bool none = g_simDeck.size() == n0 && deckNote.indexOf("Not paired") >= 0;
    shot("t117_deck_bt_notpaired");
    deckBleLinked = true; keyTap(1); bool sent = g_simDeck.size() == n0 + 2 && g_simDeck[n0].via == 'B' && g_simDeck[n0].media == 0xB5;
    printf("D6 Bluetooth: switched=%d, not paired: nothing sent + note=%d, paired: Next sent by Bluetooth=%d %s\n", bt, none, sent, R(bt && none && sent)); }
  // D7 a Bluetooth scan while Deck uses Bluetooth: refused, Deck stays connected
  { btScan(); bool refused = !btBusy; btDone = true; btCollect(); bool still = BLEDevice::getInitialized() && deckBleOn;
    printf("D7 Bluetooth scan during Deck: refused=%d, Deck still on=%d %s\n", refused, still, R(refused && still)); btDone = false; }
  // D8 pocket: screen off in Deck: a tap / a press only wakes, nothing is sent
  { size_t n0 = g_simDeck.size(); pw = P_OFF; lowPower = true; keyTap(0); bool w1 = pw == P_ON; pw = P_OFF; press(); bool w2 = pw == P_ON;
    printf("D8 screen off: tap wakes=%d, press wakes=%d, nothing sent=%d %s\n", w1, w2, g_simDeck.size() == n0, R(w1 && w2 && g_simDeck.size() == n0)); }
  // D9 leave Deck by a bottom tab: Bluetooth off again (memory back for news)
  { int x0[N_TABS + 1]; footerTabs(x0); ftap((x0[0] + x0[1]) / 2, FTR_Y + 10); run(100);
    printf("D9 left Deck by the Log tab: on Log=%d, Bluetooth off=%d %s\n", scr == S_HOME, !deckBleOn && !BLEDevice::getInitialized(), R(scr == S_HOME && !deckBleOn)); }
  // D10 the phone web page: save keys, bad input is refused or cleaned, empty = defaults, kept after a restart
  { server.args["plain"] = R"([{"label":"My mute","kind":1,"mod":5,"key":16},{"label":"This label is far too long","kind":9,"key":153},{"label":"T","kind":3,"text":"hi"}])";
    apiDeckPost(); bool ok1 = server.lastCode == 200 && deckKeys[0].label == "My mute" && deckKeys[0].mod == 5 && deckKeys[1].label.length() == 16 && deckKeys[1].kind == DK_NONE && deckKeys[2].kind == DK_TEXT && deckKeys[5].kind == DK_NONE;
    deckDefaults(); deckLoad(); bool kept = deckKeys[0].label == "My mute";
    server.args["plain"] = "{broken"; apiDeckPost(); bool bad1 = server.lastCode == 400 && deckKeys[0].label == "My mute";
    std::string many = "["; for (int i = 0; i < 30; i++) many += std::string(i ? "," : "") + "{\"label\":\"x\",\"kind\":1,\"key\":4}"; many += "]";
    server.args["plain"] = many; apiDeckPost(); bool bad2 = server.lastCode == 400;
    server.args["plain"] = "[]"; apiDeckPost(); bool reset = deckKeys[0].label == "Play Pause";
    printf("D10 web page: saved + cleaned=%d, kept after restart=%d, broken JSON refused=%d, 30 keys refused=%d, [] = defaults=%d %s\n", ok1, kept, bad1, bad2, reset, R(ok1 && kept && bad1 && bad2 && reset)); }
  // D11 extreme: 60 fast taps -> 60 keys, every key let go (nothing stuck); cable pulled -> a note, nothing sent
  { deckOpen(); deckVia = DV_USB; deckStartVia(); run(100); size_t n0 = g_simDeck.size();
    for (int k = 0; k < 60; k++) keyTap(k % 12 == 11 ? 3 : k % 12);
    bool allUp = downs(n0) == ups(n0);
    g_simUsbHost = false; size_t n1 = g_simDeck.size(); keyTap(0); bool noSend = g_simDeck.size() == n1 && deckNote.indexOf("USB") >= 0; g_simUsbHost = true;
    printf("D11 60 fast taps: %d keys sent, every one let go=%d; cable out: nothing sent + note=%d %s\n", downs(n0), allUp, noSend, R(downs(n0) >= 55 && allUp && noSend)); }
  // D12 wide screen: 4 x 3 keys fit
  { rot = 1; applyRotation(); deckPage = 0; dirty = true; run(100); int x, y, w, h; deckKeyRect(11, x, y, w, h); shot("t117_deck_wide");
    printf("D12 wide screen: last key ends at x=%d y=%d (screen %dx%d, tabs at %d) %s\n", x + w, y + h, W, H, FTR_Y, R(x + w <= W && y + h <= FTR_Y)); rot = 0; applyRotation(); goScreen(S_HOME); dirty = true; run(100); }

  return 0;
}
