#include "sim_common.h"
// v11.6 tests: Sudoku clock, weather place, the Dragon game (was Pixel Swim), SD info on the phone page,
// the clip converter for iPhone. Every line printed FAIL on v11.5 and must print PASS on v11.6.

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }   // by the clock
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void hold(int ms = 1500) { g_simDigital[JOY_SW] = LOW; run(ms); g_simDigital[JOY_SW] = HIGH; run(150); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t116"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = true; WiFi.connected = true; remindAct = -1;

  // ---------- 1 Sudoku: the clock stops while the New game window is open (seen in the owner's video) ----------
  { sudokuOpen(); run(100); if (sdkNewMenu) sdkNew(0); run(3000);
    uint32_t a = sdkSecs();
    sudokuTap(W - 20, HDR_H + 10); bool menu = sdkNewMenu;   // [New]
    run(5000); uint32_t b = sdkSecs();
    sudokuTap(10, H - 10); bool closed = !sdkNewMenu;          // tap outside the window = close it
    run(3000); uint32_t c = sdkSecs();
    run(90000); uint32_t d = sdkSecs();                         // thinking 1.5 minutes without touching: still counts
    printf("1  Sudoku clock: playing %us, New game window open 5 s: %us -> %us (stopped=%d), closed=%d then %us, thinking 90 s -> %us %s\n",
           a, a, b, b == a, closed, c, d, R(a >= 2 && menu && b == a && closed && c >= b + 2 && d >= c + 88));
    sudokuClose(); }

  // ---------- 2 weather place: Sathorn, Bangkok (RMUTK) ----------
  { bool ok = !strcmp(WX_LAT, "13.71") && !strcmp(WX_LON, "100.54") && String(WX_PLACE).indexOf("Sathorn") >= 0;
    printf("2  weather place: %s (%s, %s) %s\n", WX_PLACE, WX_LAT, WX_LON, R(ok)); }

  // ---------- 3 Dragon (was Pixel Swim): press = dash, [Pause] on the top bar, hold = back to Games ----------
  {
#ifdef HAS_DRAGON
    gameOpen(); run(100); bool ready = gs == G_READY;
    press(); bool play = gs == G_PLAY;
    press(); bool dash = dashing() && gs == G_PLAY;
    run(300); gameTap(W - 80, 10); bool paused = gs == G_PAUSE && scr == S_GAME;
    press(); bool goOn = gs == G_PLAY;
    int s0 = score; food[0].x = px_ + 1; food[0].y = py_; run(200); bool ate = score == s0 + 1;
    gameDraw(); savePng(spr, "t116_dragon", W, H);
    hold(); bool out = scr == S_GAMES;
    printf("3  Dragon: ready=%d, press starts=%d, press dashes=%d, [Pause]=%d, press goes on=%d, crash into a dot=%d, hold = Games=%d %s\n",
           ready, play, dash, paused, goOn, ate, out, R(ready && play && dash && paused && goOn && ate && out));
#else
    printf("3  Dragon: not in this version (Pixel Swim) %s\n", R(false));
#endif
    scr = S_HOME; }

  // ---------- 4 phone page: SD card with photo / clip numbers ----------
  { server.args.clear(); server.args["dir"] = "/"; server.headers.clear(); server.simClient.local = WiFi.softAPIP(); apOn = true;
    server.lastBody = ""; apiSdList();
    bool ok = server.lastBody.indexOf("\"photos\":6") >= 0 && server.lastBody.indexOf("\"clips\":1") >= 0;
    bool page = std::string(INDEX_HTML).find("sz(d.used)") != std::string::npos;
    printf("4  phone page SD info: numbers sent=%d, shown like the board=%d %s\n", ok, page, R(ok && page)); server.args.clear(); }

  // ---------- 5 clip converter for iPhone: the clip player is put on the page and started at the tap ----------
  { std::string h(INDEX_HTML);
    bool onPage = h.find("document.body.appendChild(v)") != std::string::npos, unlock = h.find("const un=v.play()") != std::string::npos;
    printf("5  clip converter: player on the page=%d, played at the tap=%d (full test: tools/web/test_video.js, scenario ios) %s\n", onPage, unlock, R(onPage && unlock)); }

  // ---------- 6 version ----------
  { printf("6  version %s %s\n", FW_VERSION, R(atof(FW_VERSION + 1) >= 11.6f)); }   // (this version or a newer one)
  return 0;
}
