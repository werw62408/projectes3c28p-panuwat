#include "sim_common.h"
// v11.3 tests: the joystick moves around every screen, Tilt Maze, Blocks

static void run(int ms) { for (int t = 0; t < ms; t += 25) { g_simMs += 20; loop(); } }   // loop() itself adds 5 ms
static void stickRaw(int x, int y) { g_simAnalog[JOY_X] = x; g_simAnalog[JOY_Y] = y; }
// a push in screen directions (for joyMap = 0 and the screen direction the setup was made on)
static void push(int dx, int dy, int holdMs = 120) { stickRaw(2048 + dx * 1900, 2048 + dy * 1900); run(holdMs); stickRaw(2048, 2048); run(120); }
static void press(int holdMs = 80) { g_simDigital[JOY_SW] = LOW; run(holdMs); g_simDigital[JOY_SW] = HIGH; run(80); }
static String focusName() { int i = navFind(); if (i < 0) return "none"; auto& t = navT[i]; char b[48]; snprintf(b, sizeof b, "%d,%d %dx%d", t.x, t.y, t.w, t.h); return b; }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t113"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  setNow(T0); setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };
  offIdx = 3;   // screen never turns off during the tests

  // ---------- joystick around the screens ----------
  { printf("J0 stick found at start: %d %s\n", joyOk, R(joyOk)); }
  // J1 first push only shows the ring, second moves it
  { scr = S_HOME; scrollY = 0; dirty = true; loop(); push(0, 1);
    bool shown = navShow; String f1 = focusName(); push(0, 1); String f2 = focusName();
    printf("J1 first push shows the ring=%d (at %s), next push moves it (to %s) %s\n", shown, f1.c_str(), f2.c_str(), R(shown && f1 != f2)); }
  // J2 press on a +1 button = a log
  { int i = navFind(); auto t = navT[i]; int tile = homeTileAt(t.vx + t.vw / 2, t.vy + t.vh / 2);
    // make sure the ring is on a +1 (right part of a tile)
    int tx, ty; homeTileRect(tile, tx, ty); if (t.x < tx + 40) push(1, 0);
    int c0 = sumFor(acts[tile].id).count; press(); int c1 = sumFor(acts[tile].id).count;
    printf("J2 press on +1 of %s: %d -> %d %s\n", acts[tile].name.c_str(), c0, c1, R(c1 == c0 + 1)); }
  // J3 hold the press on a tile = number pad, type 3, go down to Save
  { int i = navFind(); int tile = homeTileAt(navT[i].vx + navT[i].vw / 2, navT[i].vy + navT[i].vh / 2);
    int c0 = sumFor(acts[tile].id).count;
    press(800); bool pad = scr == S_KEYPAD;
    push(1, 0); push(1, 0); press();                  // 1 -> 2 -> 3, press
    String typed = kpVal;
    for (int k = 0; k < 4; k++) push(0, 1);           // 6, 9, Del, Save
    press(); int c1 = sumFor(acts[tile].id).count;
    printf("J3 hold = number pad=%d, typed \"%s\", Save: %d -> %d %s\n", pad, typed.c_str(), c0, c1, R(pad && typed == "3" && scr == S_HOME && (isUnitAct(acts[tile]) || c1 == c0 + 3))); }
  // J4 down to the tabs, right = Apps, press
  { scr = S_HOME; scrollY = 0; dirty = true; loop();
    for (int k = 0; k < 8; k++) push(0, 1);
    bool onTab = navT[navFind()].kind == NK_FOOTER;
    push(1, 0); press();
    printf("J4 down to the tabs=%d, right + press = Apps: %d %s\n", onTab, scr == S_APPS, R(onTab && scr == S_APPS)); }
  // J5 into Games, hold left 1 s = Back
  { dirty = true; loop(); push(0, 1); press();   // Files (top left) -> down = Games
    bool games = scr == S_GAMES; dirty = true; loop();
    stickRaw(2048 - 1900, 2048); run(1200); stickRaw(2048, 2048); run(150);
    printf("J5 Games opened=%d, hold left 1 s -> back to Apps=%d %s\n", games, scr == S_APPS, R(games && scr == S_APPS)); }
  // J6 the direction turns with the screen
  { bool ok = true; joyMap = 0; joyRot = 0; stickRaw(2048 + 1900, 2048);   // stick pushed to the right of the case
    float x, y; rot = 0; joyVec(x, y); ok &= x > 0.5f && fabsf(y) < 0.1f;
    rot = 1; joyVec(x, y); ok &= y < -0.5f && fabsf(x) < 0.1f;   // screen turned a quarter: the same push is "up" on screen
    rot = 2; joyVec(x, y); ok &= x < -0.5f;
    rot = 3; joyVec(x, y); ok &= y > 0.5f;
    rot = 0; stickRaw(2048, 2048);
    // check against the touch screen's own turning: a point moved right on the panel moves the same way on screen
    printf("J6 stick direction follows the screen direction (4 ways) %s\n", R(ok)); }
  // J7 pocket: screen off, pushes do nothing, a press only wakes
  { scr = S_HOME; dirty = true; loop(); pw = P_OFF; lowPower = true; int c0 = (int)todayEv.size();
    push(0, 1); push(1, 0); bool stillOff = pw == P_OFF;
    press(); bool woke = pw == P_ON; bool noLog = (int)todayEv.size() == c0;
    printf("J7 screen off: push keeps it off=%d, press wakes=%d, nothing logged=%d %s\n", stillOff, woke, noLog, R(stillOff && woke && noLog)); }
  // J8 Settings: long list scrolls under the ring
  { goScreen(S_SET); setScroll = 0; dirty = true; loop(); push(0, 1);
    for (int k = 0; k < 14; k++) push(0, 1);
    int i = navFind(); auto t = navT[i];
    bool visible = t.vy >= HDR_H && t.vy + t.vh <= FTR_Y + 30;
    printf("J8 Settings: scrolled to %d, ring on a visible button=%d %s\n", setScroll, visible, R(setScroll > 0 && visible));
    dirty = true; render(); savePng(spr, "t113_ring_settings", W, H); }
  // J9 Files: hold a row = menu; hold left = closes the menu
  { curDir = "/photos"; filesOpen(); dirty = true; loop(); push(0, 1); push(0, 1);
    press(800); bool menu = fmUi == FU_MENU; dirty = true; loop(); savePng(spr, "t113_ring_files_menu", W, H);
    stickRaw(2048 - 1900, 2048); run(1200); stickRaw(2048, 2048); run(150);
    printf("J9 Files: hold = menu=%d, hold left = menu closed=%d %s\n", menu, fmUi == FU_LIST, R(menu && fmUi == FU_LIST)); }
  // J10 switched off in Settings: the stick does nothing in menus
  { joyNavOn = false; navShow = false; scr = S_HOME; dirty = true; loop(); push(0, 1);
    printf("J10 menus OFF: no ring=%d %s\n", !navShow, R(!navShow)); joyNavOn = true; }
  // J11 finger hides the ring
  { scr = S_HOME; dirty = true; loop(); push(0, 1); bool on = navShow; navShow = true;
    tDown = false; g_simTouch = true; g_simTouchX = 120; g_simTouchY = 150; loop(); g_simTouch = false; loop();
    printf("J11 ring shown=%d, a touch hides it=%d %s\n", on, !navShow, R(on && !navShow)); }

  // ---------- Tilt Maze ----------
  { bool ok = true; int levels = 0;
    for (int lv = 1; lv <= 14; lv++) {
      mzLevel = lv; mzBuild(); levels++;
      // every cell can be reached (a real maze: one way between any two cells)
      std::vector<int> seen(MZ_MAXC * MZ_MAXR, 0), q{0}; seen[0] = 1; size_t qi = 0;
      const int DC[4] = {0, 1, 0, -1}, DR[4] = {-1, 0, 1, 0};
      while (qi < q.size()) { int cur = q[qi++], c = cur % MZ_MAXC, r = cur / MZ_MAXC;
        for (int d = 0; d < 4; d++) if (!(mzWall[r][c] & (1 << d))) { int n = (r + DR[d]) * MZ_MAXC + c + DC[d]; if (!seen[n]) { seen[n] = 1; q.push_back(n); } } }
      if ((int)q.size() != mzCols * mzRows) { ok = false; printf("   level %d: %d of %d cells reached\n", lv, (int)q.size(), mzCols * mzRows); }
      if (mzGoalC == 0 && mzGoalR == 0) ok = false;
    }
    printf("M1 mazes 1-14: every cell reachable, goal away from start %s\n", R(ok)); }
  // M2 the ball never goes through a wall (random tilting for 60 s of game time)
  { mzLevel = 6; mzBuild(); float worst = 0; bool out = false;
    std::mt19937 rng(3);
    for (int i = 0; i < 1800; i++) {
      float ax = (rng() % 2001) / 1000.0f - 1, ay = (rng() % 2001) / 1000.0f - 1;
      for (int k = 0; k < 4; k++) mzStep(ax, ay, 0.033f / 4);
      for (auto& w : mzRects) { float cx = constrain(mzBX, (float)w.x, (float)(w.x + w.w)), cy = constrain(mzBY, (float)w.y, (float)(w.y + w.h));
        float pen = mzR - hypotf(mzBX - cx, mzBY - cy); if (pen > worst) worst = pen; }
      if (mzBX < mzX0 || mzBY < mzY0 || mzBX > mzX0 + mzCols * mzCell || mzBY > mzY0 + mzRows * mzCell) out = true;
    }
    printf("M2 ball vs walls: deepest overlap %.2f px, left the board=%d %s\n", worst, out, R(worst < 1.0f && !out)); }
  // M3 the maze can really be played to the end: steer along the path with the stick
  { bool ok = true; float worstT = 0;
    for (int lv : {1, 4, 9}) {
      mzLevel = lv; mzBuild();
      // path from start to goal
      std::vector<int> from(MZ_MAXC * MZ_MAXR, -2), q{0}; from[0] = -1; size_t qi = 0; const int DC[4] = {0, 1, 0, -1}, DR[4] = {-1, 0, 1, 0};
      while (qi < q.size()) { int cur = q[qi++], c = cur % MZ_MAXC, r = cur / MZ_MAXC;
        for (int d = 0; d < 4; d++) if (!(mzWall[r][c] & (1 << d))) { int n = (r + DR[d]) * MZ_MAXC + c + DC[d]; if (from[n] == -2) { from[n] = cur; q.push_back(n); } } }
      std::vector<int> path; for (int cur = mzGoalR * MZ_MAXC + mzGoalC; cur >= 0; cur = from[cur]) path.insert(path.begin(), cur);
      size_t wp = 1; float t = 0; bool won = false;
      while (t < 240 && !won) {
        int cur = path[min(wp, path.size() - 1)];
        float tx = mzCellX(cur % MZ_MAXC), ty = mzCellY(cur / MZ_MAXC);
        float ax = constrain((tx - mzBX) / 25.0f - mzVX / 250.0f, -1.0f, 1.0f), ay = constrain((ty - mzBY) / 25.0f - mzVY / 250.0f, -1.0f, 1.0f);
        for (int k = 0; k < 4; k++) mzStep(ax, ay, 0.033f / 4);
        t += 0.033f;
        if (hypotf(tx - mzBX, ty - mzBY) < mzCell * 0.25f && wp + 1 < path.size()) wp++;
        for (auto& tr : mzTraps) if (hypotf(mzBX - tr.x, mzBY - tr.y) < mzCell * 0.34f * 0.7f) { ok = false; }
        if (hypotf(mzBX - mzCellX(mzGoalC), mzBY - mzCellY(mzGoalR)) < mzCell * 0.34f * 0.7f) won = true;
      }
      if (!won) { ok = false; printf("   level %d not finished (path %d cells)\n", lv, (int)path.size()); }
      worstT = max(worstT, t);
    }
    printf("M3 levels 1, 4, 9 played to the goal by steering (longest %.0f s), no red hole on the way %s\n", worstT, R(ok)); }
  // M4 the game screens, pause with a long press
  { mazeOpen(); run(100); savePng(spr, "t113_maze_ready", W, H);
    press(); bool playing = mzState == MZ_PLAY;
    stickRaw(2048 + 1500, 2048 + 900); run(700); stickRaw(2048, 2048); savePng(spr, "t113_maze_play", W, H);
    g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(80);
#ifdef FW_VERSION   // v11.5+: holding the press leaves the game (the same in every game)
    printf("M4 start by press=%d, hold 1 s = back to Games=%d (v11.5+) %s\n", playing, scr == S_GAMES, R(playing && scr == S_GAMES)); }
#else
    bool paused = mzState == MZ_PAUSE;
    savePng(spr, "t113_maze_pause", W, H);
    push(0, 1); push(0, 1); press();   // Back to Games
    printf("M4 start by press=%d, hold 1 s = pause=%d, menu Back = Games=%d %s\n", playing, paused, scr == S_GAMES, R(playing && paused && scr == S_GAMES)); }
#endif
  { mzLevel = 3; mzBuild(); mzTimeMs = 23400; mzBX = mzCellX(mzGoalC); mzBY = mzCellY(mzGoalR); mzWin(); scr = S_MAZE; mazeDraw(); savePng(spr, "t113_maze_win", W, H);
    printf("M5 level done saves the best time and opens the next level: best %u, max level %d %s\n", mzBest(3), mzMaxLevel, R(mzBest(3) == 234 && mzMaxLevel >= 4)); }

  // ---------- Blocks ----------
  { bool ok = true; for (int p = 0; p < 7; p++) for (int r = 0; r < 4; r++) { int n = 0; for (int k = 0; k < 16; k++) if (BL_SHAPE[p][r] & (1 << k)) n++; if (n != 4) ok = false; }
    printf("B1 every piece has 4 squares in every turn %s\n", R(ok)); }
  { blocksOpen(); blBagN = 0; int cnt[7] = {0}; for (int i = 0; i < 14; i++) cnt[blTakeFromBag()]++;
    bool ok = true; for (int i = 0; i < 7; i++) if (cnt[i] != 2) ok = false;
    printf("B2 7-bag: 14 pieces = each piece twice %s\n", R(ok)); }
  { blNew(); blState = BL_PLAY; for (int c = 0; c < BL_W; c++) if (c != 4) blBoard[BL_H - 1][c] = 3;
    blPiece = 0; blRot = 1; blX = 2; blY = 0;   // I standing up: its column is x + 2 = 4
    long s0 = blScore; blHardDrop(); blFinishClear();
    bool rowGone = true; for (int c = 0; c < BL_W; c++) if (c != 4 && blBoard[BL_H - 1][c] == 3) rowGone = false;
    printf("B3 one row: lines %d, score %ld -> %ld, row cleared=%d %s\n", blLines, s0, blScore, rowGone, R(blLines == 1 && blScore - s0 >= 100 && rowGone)); }
  { blNew(); blState = BL_PLAY; for (int r = BL_H - 4; r < BL_H; r++) for (int c = 0; c < BL_W; c++) if (c != 9) blBoard[r][c] = 5;
    blPiece = 0; blRot = 1; blX = 7; blY = 0; long s0 = blScore; blHardDrop(); blFinishClear();
    bool empty = true; for (int r = 0; r < BL_H; r++) for (int c = 0; c < BL_W; c++) if (blBoard[r][c]) empty = false;
    printf("B4 four rows at once: +%ld points (800 + drop), board empty=%d %s\n", blScore - s0, empty, R(blScore - s0 >= 800 && empty)); }
  { blNew(); blState = BL_PLAY; for (int r = 0; r < BL_H; r++) for (int c = 0; c < BL_W; c++) blBoard[r][c] = (c + r) % 2 ? 2 : 0;
    blBest = 0; blScore = 1234; blSpawn();
    printf("B5 no room at the top = game over=%d, best saved %u %s\n", blState == BL_OVER, prefs.getUInt("blBest", 0), R(blState == BL_OVER && prefs.getUInt("blBest", 0) == 1234)); }
  { blocksOpen(); run(60); press(); bool play = blState == BL_PLAY; savePng(spr, "t113_blocks_start", W, H);
    blPiece = 5; blRot = 0; blX = 3; blY = 2; blLanded = false;
    int x0 = blX; push(1, 0); int x1 = blX;
    int r0 = blRot; press(); int r1 = blRot;
    int placed0 = 0; for (int r = 0; r < BL_H; r++) for (int c = 0; c < BL_W; c++) if (blBoard[r][c]) placed0++;
    stickRaw(2048, 2048 - 1900); run(120); stickRaw(2048, 2048); run(120);   // flick up = drop
    int placed1 = 0; for (int r = 0; r < BL_H; r++) for (int c = 0; c < BL_W; c++) if (blBoard[r][c]) placed1++;
    printf("B6 stick: start=%d, right %d->%d, press turns %d->%d, flick up drops (%d -> %d squares) %s\n", play, x0, x1, r0, r1, placed0, placed1, R(play && x1 == x0 + 1 && r1 == ((r0 + 1) & 3) && placed1 == placed0 + 4)); }
  { blNew(); blState = BL_PLAY;
    for (int i = 0; i < 6; i++) { blX = (i * 3) % 7; blHardDrop(); if (blState == BL_CLEAR) blFinishClear(); if (blState != BL_PLAY) break; }
    blocksDraw(); savePng(spr, "t113_blocks_play", W, H);
    g_simDigital[JOY_SW] = LOW; run(1500); g_simDigital[JOY_SW] = HIGH; run(80);
#ifdef FW_VERSION   // v11.5+: holding the press leaves the game
    printf("B7 hold 1 s = back to Games=%d (v11.5+) %s\n", scr == S_GAMES, R(scr == S_GAMES)); }
#else
    bool paused = blState == BL_PAUSE; savePng(spr, "t113_blocks_pause", W, H);
    push(0, 1); push(0, 1); press();
    printf("B7 hold 1 s = pause=%d, menu Back = Games=%d %s\n", paused, scr == S_GAMES, R(paused && scr == S_GAMES)); }
#endif

  // pictures: Games menu, ring on the Log page, wide screen
  { scr = S_GAMES; navShow = false; dirty = true; render(); savePng(spr, "t113_games_tall", W, H);
    scr = S_HOME; scrollY = 0; navShow = true; navPage = -1; dirty = true; render(); savePng(spr, "t113_ring_log", W, H);
    rot = 1; applyRotation(); scr = S_GAMES; navShow = false; dirty = true; render(); savePng(spr, "t113_games_wide", W, H);
    mazeOpen(); mzLevel = 5; mzBuild(); mzSetState(MZ_PLAY); mzStartMs = millis(); mazeDraw(); savePng(spr, "t113_maze_wide", W, H);
    blocksOpen(); blLayout(); blState = BL_PLAY; for (int i = 0; i < 6; i++) blHardDrop(); blocksDraw(); savePng(spr, "t113_blocks_wide", W, H);
    rot = 0; applyRotation(); }
  return 0;
}
