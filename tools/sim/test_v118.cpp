#include "sim_common.h"
// v11.8 tests: the HUD theme (Settings > Screen > Theme > HUD), the Ant Colony game (was Habit Garden),
// the SYS BOOT start screen, and the 3 small bugs found when v11.7.2 was checked.
// On v11.7.2 these print FAIL (the parts don't exist yet); on v11.8 every line must print PASS.

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void hold(int ms = 1500) { g_simDigital[JOY_SW] = LOW; run(ms); g_simDigital[JOY_SW] = HIGH; run(150); }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static void finger(int x, int y, int ms = 60) { g_simTouch = true; g_simTouchX = x; g_simTouchY = y; run(ms); g_simTouch = false; run(60); }
static int logsToday() { return (int)todayEv.size(); }

#ifdef HAS_ANTS
// every dug pixel must be reachable from the door (ants can't dig closed caves)
static bool antsConnected(int& open, int& reach) {
  std::vector<uint8_t> seen(AF_W * AF_H, 0); std::vector<int> q;
  for (int x = 0; x < AF_W; x++) if (anPx(x, AF_SKY)) { seen[AF_SKY * AF_W + x] = 1; q.push_back(AF_SKY * AF_W + x); }
  for (size_t h = 0; h < q.size(); h++) {
    int i = q[h], x = i % AF_W, y = i / AF_W; const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& d : D) { int nx = x + d[0], ny = y + d[1]; if (ny < AF_SKY || !anPx(nx, ny) || seen[ny * AF_W + nx]) continue; seen[ny * AF_W + nx] = 1; q.push_back(ny * AF_W + nx); }
  }
  open = 0; for (int y = AF_SKY; y < AF_H; y++) for (int x = 0; x < AF_W; x++) open += anPx(x, y);
  reach = (int)q.size();
  return reach == open;
}
static uint32_t antsSum() { uint32_t s = 0; for (int i = 0; i < AN_BITS; i++) s = s * 31 + anBits[i]; return s; }
#endif

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t118"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;   // 24 Sep, 20:15 in Thailand
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  prefs.putUInt("gdPts", 42);        // an old Garden, from v11.7.2
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3; g_simOffline = true; WiFi.connected = true; remindAct = -1; joyNavOn = true;

  // ---------- 1 version ----------
  printf("1  version %s %s\n", FW_VERSION, R(!strcmp(FW_VERSION, "v11.8")));

#ifdef THEME_HUD
  // ---------- 2 theme: HUD on / off, kept in prefs, HUD fonts and colours ----------
  { themeSet(2); bool on = themeHud && prefs.getUChar("hud", 0) == 1 && FS == (const lgfx::IFont*)&hS && FL == (const lgfx::IFont*)&hL && INK == HUDCOLS[hudCol].main;
    themeSet(1); bool dark = !themeHud && themeDark && prefs.getUChar("hud", 1) == 0 && FS == (const lgfx::IFont*)&fonts::FreeSans9pt7b;
    themeSet(0); bool light = !themeHud && !themeDark && PAPER == 0xF2F2F2;
    themeSet(2);
    printf("2  theme: HUD on=%d, Dark back=%d, Light back=%d %s\n", on, dark, light, R(on && dark && light)); }

  // ---------- 3 Settings > Screen: tap HUD, tap a HUD colour ----------
  { themeSet(0); goScreen(S_SET); setOpenPage(1); dirty = true; render();
    // the buttons in drawing order: screen off (4), direction (2 + 2), theme (3)
    auto T = navT; bool okN = T.size() >= 11;
    if (okN) onTap(T[10].vx + T[10].vw / 2, T[10].vy + T[10].vh / 2);   // "HUD"
    bool hud = themeHud == 1;
    setScroll = 1000; dirty = true; render(); render(); T = navT;       // the 4 HUD colours are under the theme row (scroll down)
    std::vector<NavT> sw; for (auto& t : T) if (t.w == 40 && t.h == 34) sw.push_back(t);
    if (sw.size() == 4) onTap(sw[1].vx + sw[1].vw / 2, sw[1].vy + sw[1].vh / 2);   // Amber
    bool amber = hudCol == 1 && prefs.getUChar("hudCol", 0) == 1 && INK == HUDCOLS[1].main;
    hudCol = 0; prefs.putUChar("hudCol", 0); applyTheme();
    printf("3  Settings: tap HUD=%d, tap Amber=%d %s\n", hud, amber, R(okN && hud && amber)); setOpenPage(0); }

  // ---------- 4 HUD Log: every button is where it is on Light (touch and joystick the same) ----------
  { scr = S_HOME; scrollY = 0; themeSet(0); dirty = true; render(); auto L = navT;
    themeSet(2); dirty = true; render(); auto Hn = navT;
    bool same = L.size() == Hn.size();
    for (size_t i = 0; same && i < L.size(); i++) { same = L[i].x == Hn[i].x && L[i].y == Hn[i].y && L[i].w == Hn[i].w && L[i].h == Hn[i].h;
      if (!same) printf("   button %d: light %d,%d %dx%d  hud %d,%d %dx%d\n", (int)i, L[i].x, L[i].y, L[i].w, L[i].h, Hn[i].x, Hn[i].y, Hn[i].w, Hn[i].h); }
    int tx, ty; homeTileRect(0, tx, ty); int n0 = logsToday();
    onTap(tx + tileW() - 20, ty + TILE_H - 16); bool plus = logsToday() == n0 + 1;
    onTap(tx + 15, ty + TILE_H - 16); bool minus = logsToday() == n0;
    printf("4  HUD Log: %d buttons in the same places=%d, [+1]=%d, [-]=%d %s\n", (int)Hn.size(), same, plus, minus, R(same && plus && minus)); }

  // ---------- 5 pocket rule on HUD: screen off, a tap on [+1] only wakes ----------
  { scr = S_HOME; pw = P_OFF; lowPower = true; int tx, ty; homeTileRect(0, tx, ty); int n0 = logsToday();
    finger(tx + tileW() - 20, ty + TILE_H - 16); bool ok = logsToday() == n0 && pw == P_ON;
    printf("5  HUD screen off: tap [+1] only wakes=%d %s\n", ok, R(ok)); }

  // ---------- 6 HUD top bar: Home/Uni chip changes only with a long press ----------
  { scr = S_APPS; dirty = true; render(); char p0 = place; int cx = (hdrChipX0 + hdrChipX1) / 2;
    bool drawn = hdrChipX1 > hdrChipX0 && hdrChipX1 <= W;
    onTap(cx, 15); bool tapNo = place == p0;
    tHandled = false; onLongPress(cx, 15); bool held = place != p0;
    place = p0; prefs.putChar("place", place);
    printf("6  HUD chip: drawn=%d, tap does nothing=%d, hold switches=%d %s\n", drawn, tapNo, held, R(drawn && tapNo && held)); }

  // ---------- 7 the SYS BOOT start screen (HUD only) ----------
  { themeSet(2); spr.fillScreen(0); bootHud(6); bool drawn = spr.readPixel(W / 2, H / 2) != 0 || spr.readPixel(20, H - 26) != 0;
    savePng(spr, "t118_boot", W, H);
    themeSet(0); spr.fillScreen(C(0x123456)); bootHud(6); bool lightNo = spr.readPixel(W / 2, H / 2) == C(0x123456);
    themeSet(2);
    printf("7  start screen: HUD draws SYS BOOT=%d, Light keeps the plain one=%d %s\n", drawn, lightNo, R(drawn && lightNo)); }

  // ---------- 8 Apps and Games: no cut words (v11.7.2 showed "Weather, n.." and "Shortcut ke..") ----------
  { bool ok = true; String bad;
    for (int th = 0; th < 3; th++) for (int r = 0; r < 2; r++) {
      themeSet(th); rot = r; applyRotation(); scr = S_APPS; dirty = true; render();
      for (int i = 0; i < N_APPS; i++) { int x, y, w, h; appTileRect(i, x, y, w, h); String s = APP_SUBS[i];
        spr.setFont(pickFont(FS, s)); if ((int)spr.textWidth(hudUp(s)) > w - 6 && s.indexOf(',') > 0) s = s.substring(0, s.indexOf(','));
        spr.setFont(pickFont(FS, s)); if ((int)spr.textWidth(hudUp(s)) > w - 6) { ok = false; bad = s; } } }
    themeSet(2); rot = 0; applyRotation();
    printf("8  app tiles: every small line fits (3 themes, tall + wide)%s%s %s\n", ok ? "" : ", cut: ", bad.c_str(), R(ok)); }
#else
  for (int k = 2; k <= 8; k++) printf("%d  HUD theme: not in this version %s\n", k, R(false));
#endif

#ifdef HAS_ANTS
  // ---------- 9 a new colony: queen, 3 workers, a shaft from the door to her room ----------
  { LittleFS.remove("/ants.bin"); anLoaded = false; anSyncedFor = "";
    antOpen(); render();
    bool start = an.workers >= 3 && an.nRooms >= 1 && an.room[0].kind == AK_QUEEN && an.room[0].prog == 8;
    int open, reach; bool conn = antsConnected(open, reach);
    bool file = LittleFS.exists("/ants.bin");
    savePng(spr, "t118_ants_new", W, H);
    printf("9  new colony: queen room dug=%d, workers=%u, room joined to the door=%d (%d/%d px), saved=%d %s\n", start, an.workers, conn, reach, open, file, R(start && conn && file)); }

  // ---------- 10 goals -> eggs -> 8 days -> workers ----------
  { // the first count used the last 7 days of logs: eggs = goals reached on each of those days
    uint32_t eggs = 0; for (int i = 0; i < AN_HATCH; i++) eggs += an.brood[i];
    uint32_t want = 0;
    for (int d = 7; d >= 1; --d) { std::map<String, Sum> m; bool any = false; forEachEvent(dayKey(nowT() - (time_t)d * 86400), [&](const Ev& e) { Sum& s = m[e.id]; s.count++; s.sum += e.v; any = true; }); int dn, g; anScore(m, any, dn, g); want += dn; }
    uint32_t w0 = an.workers; want += anToday; eggs += anToday;   // (today's eggs too: they are counted when today ends)
    antLeave();
    setNow(T0 + 9 * 86400); run(1500);   // 9 days later (no logs on those days): every egg has hatched
    antOpen(); render();
    uint32_t left = 0; for (int i = 0; i < AN_HATCH; i++) left += an.brood[i];
    bool ok = eggs == want && eggs > 0 && an.workers == w0 + eggs && left == 0;
    printf("10 eggs from goals: %u (want %u), 9 days later workers %u -> %u, eggs left %u %s\n", eggs, want, w0, an.workers, left, R(ok)); }

  // ---------- 11 digging while the page is open: tunnels grow, all joined to the door, soil carried out ----------
  { an.workers = 30; anFields(); anSpawnAnts();
    uint32_t d0 = an.dug, m0 = an.mound;
    run(40000);
    int open, reach; bool conn = antsConnected(open, reach);
    bool moved = an.dug > d0, carried = an.mound > m0;
    savePng(spr, "t118_ants_dig", W, H);
    printf("11 digging on screen 40 s: dug %u -> %u, soil carried out %u -> %u, all joined to the door=%d %s\n", d0, an.dug, m0, an.mound, conn, R(moved && carried && conn)); }

  // ---------- 12 digging while nobody looks (10 h), never more than the colony wants ----------
  { antLeave(); uint32_t d0 = an.dug; an.lastT -= 10 * 3600;
    antOpen(); uint32_t d1 = an.dug;
    int open, reach; bool conn = antsConnected(open, reach);
    antLeave(); an.lastT -= 200 * 86400; antOpen(); uint32_t d2 = an.dug;   // a very long time away
    bool capped = d2 <= anDigGoal() + 200;
    printf("12 while away: 10 h dug %u -> %u, joined=%d; 200 days: %u (goal %u) %s\n", d0, d1, conn, d2, anDigGoal(), R(d1 > d0 + 200 && conn && capped)); }

  // ---------- 13 save and load: the same colony after a restart ----------
  { antLeave(); uint32_t w = an.workers, dug = an.dug, sum = antsSum(); uint16_t nr = an.nRooms;
    anLoaded = false; memset(&an, 0, sizeof an); memset(anBits, 0, AN_BITS); anLoad();
    bool ok = an.workers == w && an.dug == dug && an.nRooms == nr && antsSum() == sum;
    printf("13 restart: workers %u, rooms %u, dug %u, map the same=%d %s\n", an.workers, an.nRooms, an.dug, antsSum() == sum, R(ok)); }

  // ---------- 14 joystick: hold 1 s = back to Games, the memory is given back ----------
  { antOpen(); run(300); hold(1500);
    bool out = scr == S_GAMES, freed = !anDist && !anFace && !anFoodF && !anBgOk && anAnts.empty();
    printf("14 hold the stick 1 s: Games=%d, memory given back=%d %s\n", out, freed, R(out && freed)); }

  // ---------- 15 screen off: a tap on the farm only wakes, the ants wait ----------
  { antOpen(); run(500); bool lab = anLabels; pw = P_OFF; lowPower = true; float x0 = anAnts.empty() ? 0 : anAnts[0].x;
    run(2000); bool still = anAnts.empty() || anAnts[0].x == x0;
    AnBox S = anScene(); finger(S.x + S.w / 2, S.y + S.h / 2);
    bool ok = pw == P_ON && anLabels == lab && still;
    printf("15 screen off on the farm: ants wait=%d, tap only wakes=%d %s\n", still, pw == P_ON && anLabels == lab, R(ok)); }

  // ---------- 16 wide screen: the farm and the numbers fit ----------
  { rot = 1; applyRotation(); dirty = true; render(); AnBox S = anScene();
    bool ok = S.y + S.h <= FTR_Y && S.x + S.w <= W - 70;
    savePng(spr, "t118_ants_wide", W, H);
    rot = 0; applyRotation();
    printf("16 wide screen: farm %dx%d at y %d, ends above the tabs (%d)=%d %s\n", S.w, S.h, S.y, FTR_Y, ok, R(ok)); }

  // ---------- 17 millis() wraps (49.7 days on): the ants go on ----------
  { g_simMs = 0xFFFFFFFFull - 2000; anStepMs = anDigMs = anFoodMs = millis(); float x0 = anAnts.empty() ? 0 : anAnts[0].x, y0 = anAnts.empty() ? 0 : anAnts[0].y;
    run(6000); bool moved = anAnts.empty() || anAnts[0].x != x0 || anAnts[0].y != y0 || anAnts[0].wait;
    printf("17 millis() wraps: ants still move=%d %s\n", moved, R(moved)); antLeave(); }

  // ---------- 18 the old Garden numbers are kept (nothing lost) ----------
  { bool ok = prefs.getUInt("gdPts", 0) == 42;
    printf("18 old Garden data kept in prefs=%d %s\n", ok, R(ok)); }
#else
  for (int k = 9; k <= 18; k++) printf("%d Ant Colony: not in this version %s\n", k, R(false));
#endif

  // ---------- 19 bug 1: after [+1] on the reminder bar, the Deck's bottom keys work again soon ----------
  { themeSet(2); deckOpen(); remindAct = 0; dirty = true; render();
    uint32_t s0 = deckSent;
    onTap(W - 20, remindBarY + 15);   // [+1]
    dirty = true; render();
    int kx, ky, kw, kh; deckKeyRect(DECK_PER_PAGE - 1, kx, ky, kw, kh);   // bottom right key, under the bar
    onTap(kx + kw / 2, remindBarY + 15); bool blocked = deckSent == s0;   // at once: the "saved" bar (no double tap)
    run(700); dirty = true; render();
    onTap(kx + kw / 2, remindBarY + 15); bool works = deckSent == s0 + 1;
    printf("19 Deck after [+1]: tap at once blocked=%d, after 0.7 s the key works=%d %s\n", blocked, works, R(blocked && works));
    deckLeave(); }

  // ---------- 20 bug 3: the "saved" bar keeps the right name when the list changes ----------
  { run(2000); scr = S_APPS; remindAct = 1; dirty = true; render(); String nm = acts[1].name;
    onTap(W - 20, remindBarY + 15);
    std::swap(acts[0], acts[1]);   // the web page changed the order right then
    dirty = true; render();
    String shown = remindDoneAct >= 0 ? acts[remindDoneAct].name : String("-");
    bool ok = shown == nm;
    std::swap(acts[0], acts[1]);
    printf("20 saved bar after the list changed: shows %s (want %s) %s\n", shown.c_str(), nm.c_str(), R(ok)); }

  themeSet(0);
  return 0;
}
