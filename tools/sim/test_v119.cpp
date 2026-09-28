#include "sim_common.h"
// v11.9 tests: Pixel Swim back (finer water), the built-in Game Boy game, UI sounds + page wipes + Log animations,
// the log burst guard (Toilet 97 on the web page), undo many, the ant farm full screen (bigger, a v11.8 colony moved in),
// memory kept out of the small fast RAM, the crash note in About.
// On v11.8 these print FAIL; on v11.9 every line must print PASS.

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static void hold(int ms = 1500) { g_simDigital[JOY_SW] = LOW; run(ms); g_simDigital[JOY_SW] = HIGH; run(150); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static void finger(int x, int y, int ms = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(ms); g_simTouch = false; run(60); }
static int countOf(const char* id) { int n = 0; for (auto& e : todayEv) n += e.id == id; return n; }

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t119"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = true; WiFi.connected = true; remindAct = -1; joyNavOn = true;

  printf("1  version %s %s\n", FW_VERSION, R(atof(FW_VERSION + 1) >= 11.9f));

#ifdef HAS_PIXSWIM
  // ---------- 2 Pixel Swim: back, about twice the water pixels, eating = bigger + a sound + a ripple ----------
  { gameOpen(); run(100); bool ready = gs == G_READY && NP >= 2400;
    press(); bool play = gs == G_PLAY;
    uint32_t e0 = sfxCount[SFX_EAT]; float r0 = pr; int s0 = score;
    food[0].x = px_ + 1; food[0].y = py_; run(200);
    bool ate = score == s0 + 1 && pr > r0 && sfxCount[SFX_EAT] == e0 + 1 && ripN >= 1;
    gameDraw(); savePng(spr, "t119_swim", W, H);
    press(); bool paused = gs == G_PAUSE;
    hold(); bool out = scr == S_GAMES;
    printf("2  Pixel Swim: %d water pixels (v11.5: 1400), ready=%d, press plays=%d, eat: bigger + sound + ripple=%d, press pauses=%d, hold = Games=%d %s\n",
           NP, ready, play, ate, paused, out, R(ready && play && ate && paused && out)); }
#else
  printf("2  Pixel Swim: not in this version %s\n", R(false));
#endif

  // ---------- 3 Game Boy: a game is there without any file on the card ----------
  { gbListOpen(); bool has = !gbRoms.empty() && gbRoms[0].name == "2048";
#ifdef HAS_PIXSWIM
    bool built = has && gbRoms[0].built != nullptr;
    if (built) gbStart(gbRoms[0]);
    bool started = scr == S_GB && gbRomSize == 32768 && gbMsg.length() == 0;
    for (int k = 0; k < 20; k++) { g_simMs += 20; gbLoop(); }
    bool stillOk = scr == S_GB && gbErr < 0;
    if (scr == S_GB) gbLeave();
#else
    bool built = false, started = false, stillOk = false;
#endif
    printf("3  Game Boy: 2048 in the list with no SD file=%d, built in=%d, starts=%d, runs=%d %s\n", has, built, started, stillOk, R(has && built && started && stillOk));
    scr = S_HOME; }

  // ---------- 4 sounds: a tap ticks, +1 plays its own sound, a goal a tune, Sounds OFF = quiet, volume off = quiet ----------
  { scr = S_HOME; scrollY = 0; themeSet(0); dirty = true; render();
    uint32_t t0 = sfxCount[SFX_TAP], p0 = sfxCount[SFX_PLUS] + sfxCount[SFX_GOAL];
    finger(W - 20, HDR_H + 12);   // Stats button: a tap
    bool tick = sfxCount[SFX_TAP] == t0 + 1;
    scr = S_HOME; dirty = true; render();
    int tx, ty; homeTileRect(1, tx, ty); uint32_t t1 = sfxCount[SFX_TAP];
    finger(tx + tileW() - 20, ty + TILE_H - 16);   // +1 on Toilet
    bool plus = sfxCount[SFX_PLUS] + sfxCount[SFX_GOAL] == p0 + 1 && sfxCount[SFX_TAP] == t1;   // its own sound, no extra tick
    sfxOn = false; uint32_t t2 = sfxCount[SFX_TAP]; finger(W - 20, HDR_H + 12); bool off = sfxCount[SFX_TAP] == t2; sfxOn = true;
    scr = S_HOME; volIdx = -1; uint32_t t3 = sfxCount[SFX_TAP]; finger(W - 20, HDR_H + 12); bool mute = sfxCount[SFX_TAP] == t3; volIdx = 2;
    scr = S_HOME;
    printf("4  sounds: tap ticks=%d, +1 its own sound=%d, Sounds OFF quiet=%d, volume off quiet=%d %s\n", tick, plus, off, mute, R(tick && plus && off && mute)); }

  // ---------- 5 page wipe: a new page wipes in, a redraw of the same page does not; Motion OFF = no wipe ----------
  { animOn = true; scr = S_HOME; dirty = true; render(); uint32_t w0 = wipeCount;
    dirty = true; render(); bool same = wipeCount == w0;
    goScreen(S_APPS); render(); bool moved = wipeCount == w0 + 1;
    animOn = false; goScreen(S_SET); render(); bool off = wipeCount == w0 + 1; animOn = true;
    scr = S_HOME; dirty = true; render();
    printf("5  page wipe: same page none=%d, new page wipes=%d, Motion OFF none=%d %s\n", same, moved, off, R(same && moved && off)); }

  // ---------- 6 Log animations: +1 floats up for 0.7 s, reaching a goal = sparks + a tune ----------
  { scr = S_HOME; int wi = -1; for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == "water") wi = i;
    while (countOf("water") >= (int)acts[wi].goal && undoEvent(wi)) {}   // start below the goal
    run(61000);   // (away from the burst guard of the logs above)
    int need = (int)acts[wi].goal - countOf("water");
    for (int k = 0; k < need - 1; k++) logDefault(wi);
    uint32_t g0 = sfxCount[SFX_GOAL];
    run(1000); logDefault(wi);   // the one that reaches the goal
    bool pop = popAct == wi && popText == "+1";
    bool goal = goalAct == wi && sfxCount[SFX_GOAL] == g0 + 1;
    dirty = true; render(); savePng(spr, "t119_goal", W, H);
    run(1600); bool gone = millis() - popMs > POP_MS && millis() - goalMs > GOAL_MS;
    printf("6  Log: +1 floats up=%d, goal reached: sparks + tune=%d, both end=%d %s\n", pop, goal, gone, R(pop && goal && gone)); }

  // ---------- 7 burst guard: the same activity at most 12 times a minute (web, touch, stick, BOOT) ----------
  { run(61000); int ti = 1; int c0 = countOf("toilet");
    for (int k = 0; k < 30; k++) logDefault(ti);
    int got = countOf("toilet") - c0;
    bool notice = burstNoticeMs && millis() - burstNoticeMs < 4000;
    dirty = true; render(); savePng(spr, "t119_burst", W, H);
    // the web page gets "too fast"
    server.args.clear(); server.args["id"] = "toilet"; server.lastCode = 0; apiLog(); bool web = server.lastCode == 429;
    run(61000); int c1 = countOf("toilet"); logDefault(ti); bool again = countOf("toilet") == c1 + 1;
    printf("7  30 logs in a moment: %d saved (max 12), notice shown=%d, web gets 429=%d, a minute later works=%d %s\n", got, notice, web, again, R(got == 12 && notice && web && again)); }

  // ---------- 8 web: undo many at once ----------
  { int c0 = countOf("toilet");
    server.args.clear(); server.args["id"] = "toilet"; server.args["n"] = "10"; apiUndo();
    bool ok = countOf("toilet") == max(0, c0 - 10);
    std::string h(INDEX_HTML); bool page = h.find("undoMany(") != std::string::npos && h.find("data-plus") != std::string::npos && h.find("flex-wrap:wrap") != std::string::npos;
    printf("8  web: undo 10 at once %d -> %d=%d, page: +1 button, undo many, every tab seen=%d %s\n", c0, countOf("toilet"), ok, page, R(ok && page)); server.args.clear(); }

#if HAS_ANTS >= 2
  // ---------- 9 ant farm: full screen like Deck (no tabs, no clock bar), bigger ----------
  { themeSet(2); rot = 0; applyRotation(); antOpen(); run(1500); dirty = true; render();
    AnBox S = anScene();
    bool big = AF_H >= 290 && S.y + S.h == H && S.h >= 290;
    bool noTabs = true; for (auto& t : navT) if (t.kind == NK_FOOTER) noTabs = false;
    savePng(spr, "t119_ants_tall", W, H);
    finger(20, 12); bool exitTap = scr == S_GAMES;
    antOpen(); run(300); g_simAnalog[JOY_X] = 2048 - 1900; run(1300); g_simAnalog[JOY_X] = 2048; run(200); bool leftBack = scr == S_GAMES;
    printf("9  ant farm: %dx%d on screen (was 240x144)=%d, no tabs=%d, Exit tap=%d, stick left 1 s = back=%d %s\n", S.w, S.h, big, noTabs, exitTap, leftBack, R(big && noTabs && exitTap && leftBack)); }

  // ---------- 10 soil carried out to the pile, food brought in ----------
  { antOpen(); an.workers = 20; anFields(); anSpawnAnts(); uint32_t m0 = an.mound, d0 = an.dug;
    int outside = 0; for (int k = 0; k < 800; k++) { run(60); for (auto& a : anAnts) if (a.job == AJ_OUT && a.ty == AN_WALK_Y) { outside++; break; } }
    bool ok = an.mound > m0 && an.dug > d0 && outside > 0;
    printf("10 ants on screen 48 s: dug %u -> %u, pile %u -> %u, seen walking outside to the pile=%d %s\n", d0, an.dug, m0, an.mound, outside > 0, R(ok));
    antLeave(); }

  // ---------- 11 a v11.8 colony (240 x 144 file) is moved into the new farm, nothing lost ----------
  { LittleFS.remove("/ants.bin");
    AnSaveV2 o{}; o.magic = AN_MAGIC_V2; o.ver = 2; o.rng = 12345; strcpy(o.day, "2026-09-20"); o.workers = 57; o.streak = 4; o.best = 6; o.nRooms = 2;
    o.brood[0] = 3; o.brood[5] = 2; o.dug = 900; o.born = 60; o.lastT = (uint32_t)nowT(); o.mound = 222; o.foundT = (uint32_t)nowT() - 9 * 86400;
    o.room[0] = AnRoom{96, 46, 13, AK_QUEEN, 8, 7}; o.room[1] = AnRoom{160, 90, 12, AK_BROOD, 8, 9}; o.plan.on = 0; o.plan.room = -1;
    std::vector<uint8_t> ob(V2_W * V2_H / 8, 0);
    auto setb = [&](int x, int y) { int i = y * V2_W + x; ob[i >> 3] |= 1 << (i & 7); };
    for (int y = V2_SKY; y < 60; y++) for (int x = 79; x <= 81; x++) setb(x, y);   // a shaft
    for (int x = 80; x < 170; x++) for (int y = 88; y < 92; y++) setb(x, y);         // a tunnel
    File f = LittleFS.open("/ants.bin", "w"); f.write((uint8_t*)&o, sizeof o); f.write(ob.data(), ob.size()); f.close();
    anLoaded = false; anSyncedFor = ""; anLoad();
    bool kept = an.magic == AN_MAGIC && an.workers == 57 && an.streak == 4 && an.nRooms == 2 && an.brood[5] == 2 && an.mound == 222 && an.door == V2_DOOR;
    bool moved = anPx(120, 88 + AF_SKY - V2_SKY) && !anPx(120, 80 + AF_SKY - V2_SKY) && an.room[1].y == 90 + AF_SKY - V2_SKY;
    anLoaded = false; anLoad(); bool saved = an.magic == AN_MAGIC && an.workers == 57;   // written back in the new form
    printf("11 v11.8 colony moved in: numbers kept=%d, tunnels moved down %d rows=%d, saved as v11.9=%d %s\n", kept, AF_SKY - V2_SKY, moved, saved, R(kept && moved && saved)); }

  // ---------- 12 wide screen: the farm scrolls (drag / stick), the numbers on the right ----------
  { rot = 1; applyRotation(); antOpen(); run(300); dirty = true; render(); AnBox S = anScene();
    bool fits = S.y + S.h <= H && S.x + S.w <= W - 70 && anScrollMax > 0;
    int s0 = anScroll; g_simTouch = true; g_simTouchX = 120; g_simTouchY = 200; run(60); g_simTouchY = 120; run(100); g_simTouch = false; run(60);
    bool drag = anScroll > s0;
    savePng(spr, "t119_ants_wide", W, H);
    antLeave(); rot = 0; applyRotation();
    printf("12 wide farm: fits=%d, deeper part by dragging (%d -> %d)=%d %s\n", fits, s0, anScroll, drag, R(fits && drag)); }
#else
  for (int k = 9; k <= 12; k++) printf("%d ant farm v11.9: not in this version %s\n", k, R(false));
#endif

  // ---------- 13 memory: the ant farm keeps out of the small fast RAM (it took ~70 KB; with Bluetooth that crashed) ----------
  {
#ifdef SIM_HEAP_CHECK
#endif
    std::string src; { FILE* fp = fopen("../../SomudTick_v11.9/SomudTick/app_ants.h", "rb"); if (fp) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, fp)) > 0) src.append(b, n); fclose(fp); } }
    bool psram = src.size() > 0 && src.find("MALLOC_CAP_8BIT") == std::string::npos && src.find("std::vector<int16_t> q") == std::string::npos;
    printf("13 ant farm buffers in PSRAM (no MALLOC_CAP_8BIT, no big static vector)=%d %s\n", psram, R(psram)); }

  // ---------- 14 About: where a crash happened, and the lowest free memory ----------
  { crashScrWas = S_DECK; crashUpWas = 12; String w = crashWhere("crash"); crashScrWas = 0xFF;
    setAboutPage(); goScreen(S_SET); setOpenPage(3); dirty = true; render();
    bool ok = w == "crash in Deck, 12m";
    printf("14 About: crash note \"%s\"=%d %s\n", w.c_str(), ok, R(ok)); setOpenPage(0); }

  // ---------- 15 Deck: Bluetooth text says what to do (no cut words) ----------
  { themeSet(2); deckVia = DV_BT; deckOpen(); String st = deckStatus(); dirty = true; render();
    spr.setFont(FS); bool fits = (int)spr.textWidth(hudUp(st)) <= W - 28 - 78;
    deckLeave(); deckVia = DV_USB;
    printf("15 Deck BT text \"%s\" fits=%d %s\n", st.c_str(), fits, R(fits && st.indexOf("pair") >= 0)); }

  themeSet(0);
  return 0;
}
