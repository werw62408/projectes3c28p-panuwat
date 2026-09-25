#pragma once
// ============================================================================
//  Game: "Tilt Maze". Tilt the board with the joystick so the steel ball rolls to the green hole.
//  - every level is a new maze (level N is always the same maze, so best times can be compared)
//  - the more you push the stick, the more the board tilts (the ball speeds up, bumps off walls)
//  - from level 3: red holes. Fall in = back to the start
//  - hold the stick button 1 s = pause (Resume / Restart / Back). No stick: hold a finger where the ball should go
//  Included from SomudTick.ino.
// ============================================================================

const int MZ_TOP = 24;          // score bar
const int MZ_MAXC = 20, MZ_MAXR = 24;
uint8_t mzWall[MZ_MAXR][MZ_MAXC];   // bits: 1 north, 2 east, 4 south, 8 west (1 = wall)
int mzCols = 0, mzRows = 0, mzCell = 40, mzX0 = 0, mzY0 = 0, mzLevel = 1, mzMaxLevel = 1;
int mzGoalC = 0, mzGoalR = 0;
struct MzRect { int16_t x, y, w, h; };
std::vector<MzRect> mzRects;          // every wall as a rectangle (for drawing and bumping)
struct MzTrap { float x, y; };
std::vector<MzTrap> mzTraps;
float mzBX, mzBY, mzVX, mzVY, mzR;    // ball
enum MzState : uint8_t { MZ_READY, MZ_PLAY, MZ_PAUSE, MZ_WIN, MZ_FALL };
MzState mzState = MZ_READY;
uint32_t mzFrameT = 0, mzStartMs = 0, mzTimeMs = 0, mzStateT = 0;
int mzMenuSel = 0;
bool mzTouchOn = false; int mzTX = 0, mzTY = 0;
bool mzSwWas = false; uint32_t mzSwT = 0; bool mzHoldDone = false;
uint32_t mzRng = 1;
uint32_t mzRand() { mzRng ^= mzRng << 13; mzRng ^= mzRng >> 17; mzRng ^= mzRng << 5; return mzRng; }

float mzCellX(int c) { return mzX0 + c * mzCell + mzCell / 2.0f; }
float mzCellY(int r) { return mzY0 + r * mzCell + mzCell / 2.0f; }
uint16_t mzBest(int lv) { return prefs.getUShort(("mzB" + String(min(lv, 99))).c_str(), 0); }   // tenths of a second, 0 = none
void mzBuild() {   // a new maze for mzLevel
  int areaW = W - 8, areaH = H - MZ_TOP - 8;
  mzCell = max(18, 46 - 3 * (mzLevel - 1));
  mzCols = min(MZ_MAXC, areaW / mzCell); mzRows = min(MZ_MAXR, areaH / mzCell);
  mzX0 = (W - mzCols * mzCell) / 2; mzY0 = MZ_TOP + 4 + (areaH - mzRows * mzCell) / 2;
  // maze: "recursive backtracker" (a random walk that carves passages, going back when stuck) -> one path between any two cells
  mzRng = 0x9E3779B9u * (uint32_t)mzLevel + 12345;
  for (int r = 0; r < mzRows; r++) for (int c = 0; c < mzCols; c++) mzWall[r][c] = 15;
  static uint8_t seen[MZ_MAXR][MZ_MAXC]; memset(seen, 0, sizeof seen);
  std::vector<int> stack; stack.push_back(0); seen[0][0] = 1;
  const int DC[4] = {0, 1, 0, -1}, DR[4] = {-1, 0, 1, 0};
  while (!stack.empty()) {
    int cur = stack.back(), c = cur % MZ_MAXC, r = cur / MZ_MAXC;
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) { int nc = c + DC[d], nr = r + DR[d]; if (nc >= 0 && nr >= 0 && nc < mzCols && nr < mzRows && !seen[nr][nc]) opts[n++] = d; }
    if (!n) { stack.pop_back(); continue; }
    int d = opts[mzRand() % n], nc = c + DC[d], nr = r + DR[d];
    mzWall[r][c] &= ~(1 << d); mzWall[nr][nc] &= ~(1 << ((d + 2) & 3));
    seen[nr][nc] = 1; stack.push_back(nr * MZ_MAXC + nc);
  }
  // goal = the cell farthest from the start (walk distance), so the whole maze is used
  static int16_t dist[MZ_MAXR][MZ_MAXC]; static int16_t from[MZ_MAXR][MZ_MAXC];
  for (int r = 0; r < mzRows; r++) for (int c = 0; c < mzCols; c++) { dist[r][c] = -1; from[r][c] = -1; }
  std::vector<int> q; q.push_back(0); dist[0][0] = 0; size_t qi = 0; int far = 0;
  while (qi < q.size()) {
    int cur = q[qi++], c = cur % MZ_MAXC, r = cur / MZ_MAXC;
    if (dist[r][c] > dist[far / MZ_MAXC][far % MZ_MAXC]) far = cur;
    for (int d = 0; d < 4; d++) {
      if (mzWall[r][c] & (1 << d)) continue;
      int nc = c + DC[d], nr = r + DR[d];
      if (dist[nr][nc] < 0) { dist[nr][nc] = dist[r][c] + 1; from[nr][nc] = cur; q.push_back(nr * MZ_MAXC + nc); }
    }
  }
  mzGoalC = far % MZ_MAXC; mzGoalR = far / MZ_MAXC;
  // red holes (level 3+): in dead ends that are not on the way to the goal
  static uint8_t onPath[MZ_MAXR][MZ_MAXC]; memset(onPath, 0, sizeof onPath);
  for (int cur = far; cur >= 0; cur = from[cur / MZ_MAXC][cur % MZ_MAXC]) onPath[cur / MZ_MAXC][cur % MZ_MAXC] = 1;
  mzTraps.clear();
  int want = mzLevel >= 3 ? min(mzLevel - 2, 6) : 0;
  std::vector<int> ends;
  for (int r = 0; r < mzRows; r++) for (int c = 0; c < mzCols; c++) {
    int open = 0; for (int d = 0; d < 4; d++) if (!(mzWall[r][c] & (1 << d))) open++;
    if (open == 1 && !onPath[r][c] && (r || c) && dist[r][c] > 2) ends.push_back(r * MZ_MAXC + c);
  }
  for (int k = 0; k < want && !ends.empty(); k++) {
    int i = mzRand() % ends.size(); int cur = ends[i]; ends.erase(ends.begin() + i);
    mzTraps.push_back({mzCellX(cur % MZ_MAXC), mzCellY(cur / MZ_MAXC)});
  }
  // walls as rectangles: north and west of every cell, plus the outer south and east edges
  int t = max(3, mzCell / 8);
  mzRects.clear();
  for (int r = 0; r < mzRows; r++) for (int c = 0; c < mzCols; c++) {
    int x = mzX0 + c * mzCell, y = mzY0 + r * mzCell;
    if (mzWall[r][c] & 1) mzRects.push_back({(int16_t)(x - t / 2), (int16_t)(y - t / 2), (int16_t)(mzCell + t), (int16_t)t});
    if (mzWall[r][c] & 8) mzRects.push_back({(int16_t)(x - t / 2), (int16_t)(y - t / 2), (int16_t)t, (int16_t)(mzCell + t)});
    if (r == mzRows - 1 && (mzWall[r][c] & 4)) mzRects.push_back({(int16_t)(x - t / 2), (int16_t)(y + mzCell - t / 2), (int16_t)(mzCell + t), (int16_t)t});
    if (c == mzCols - 1 && (mzWall[r][c] & 2)) mzRects.push_back({(int16_t)(x + mzCell - t / 2), (int16_t)(y - t / 2), (int16_t)t, (int16_t)(mzCell + t)});
  }
  mzR = mzCell * 0.28f;
  mzBX = mzCellX(0); mzBY = mzCellY(0); mzVX = mzVY = 0; mzTimeMs = 0;
}
// one small physics step (dt seconds). ax, ay = how much the board tilts (-1..1)
void mzStep(float ax, float ay, float dt) {
  const float G = 520.0f * mzCell / 40.0f;       // bigger cells = a bigger board = faster ball
  mzVX += ax * G * dt; mzVY += ay * G * dt;
  float fr = 1.0f - 1.6f * dt; mzVX *= fr; mzVY *= fr;   // rolling friction
  float vmax = mzR * 0.8f / dt;                  // never jump more than most of its own size in one step (no going through walls)
  float sp = sqrtf(mzVX * mzVX + mzVY * mzVY);
  if (sp > vmax) { mzVX *= vmax / sp; mzVY *= vmax / sp; }
  mzBX += mzVX * dt; mzBY += mzVY * dt;
  for (int pass = 0; pass < 2; pass++)
    for (auto& w : mzRects) {
      float cx = constrain(mzBX, (float)w.x, (float)(w.x + w.w)), cy = constrain(mzBY, (float)w.y, (float)(w.y + w.h));
      float dx = mzBX - cx, dy = mzBY - cy, d2 = dx * dx + dy * dy;
      if (d2 >= mzR * mzR) continue;
      float d = sqrtf(d2), nx, ny;
      if (d < 0.001f) {   // centre inside the wall: push out the short way
        float l = mzBX - w.x, r = w.x + w.w - mzBX, u = mzBY - w.y, b = w.y + w.h - mzBY, m = min(min(l, r), min(u, b));
        nx = m == l ? -1 : m == r ? 1 : 0; ny = m == u ? -1 : m == b ? 1 : 0; d = 0;
        if (nx && ny) ny = 0;
      } else { nx = dx / d; ny = dy / d; }
      mzBX += nx * (mzR - d); mzBY += ny * (mzR - d);
      float vn = mzVX * nx + mzVY * ny;
      if (vn < 0) { mzVX -= 1.35f * vn * nx; mzVY -= 1.35f * vn * ny; if (vn < -120) ledFlash(0x606870, 30); }   // bump (a bit bouncy)
    }
}
void mzSetState(MzState s) { mzState = s; mzStateT = millis(); mzMenuSel = 0; }
void mazeOpen() {
  mzMaxLevel = max(1, (int)prefs.getUChar("mzMax", 1));
  mzLevel = constrain((int)prefs.getUChar("mzCur", 1), 1, mzMaxLevel);
  if (joyOk) joyCenter();
  mzBuild(); mzSetState(MZ_READY);
  mzSwWas = joyOk && joyDown(); mzHoldDone = mzSwWas; mzTouchOn = false; mzFrameT = 0;
  scr = S_MAZE;
}
void mazeLeave() { prefs.putUChar("mzCur", mzLevel); scr = S_GAMES; dirty = true; joyNavReset(); }
String mzClock(uint32_t ms) { uint32_t s = ms / 1000; return String(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + String(s % 60); }
void mzWin() {
  uint16_t t = (uint16_t)min((uint32_t)65000, (uint32_t)(mzTimeMs / 100)), b = mzBest(mzLevel);
  if (!b || t < b) prefs.putUShort(("mzB" + String(min(mzLevel, 99))).c_str(), t);
  if (mzLevel >= mzMaxLevel && mzLevel < 250) { mzMaxLevel = mzLevel + 1; prefs.putUChar("mzMax", mzMaxLevel); }
  ledFlash(0x2E9E5B, 400);
  mzSetState(MZ_WIN);
}
void mzMenuDo(int i) {   // pause menu: 0 Resume, 1 Restart, 2 Back
  if (i == 0) { mzSetState(MZ_PLAY); mzStartMs = millis() - mzTimeMs; }
  else if (i == 1) { mzBuild(); mzSetState(MZ_PLAY); mzStartMs = millis(); }
  else mazeLeave();
}
void mazeDraw() {
  spr.fillScreen(C(PAPER));
  uint32_t board = CARD, wall = blend(CARD, INK, 0.78f);
  spr.fillRoundRect(mzX0 - 4, mzY0 - 4, mzCols * mzCell + 8, mzRows * mzCell + 8, 6, C(board));
  // goal and red holes
  float gx = mzCellX(mzGoalC), gy = mzCellY(mzGoalR), hr = mzCell * 0.34f;
  spr.fillCircle(gx, gy, hr + 3, C(0x2E9E5B)); spr.fillCircle(gx, gy, hr, C(0x14301F));
  for (auto& t : mzTraps) { spr.fillCircle(t.x, t.y, hr + 2, C(0xD03030)); spr.fillCircle(t.x, t.y, hr - 1, C(0x1A1010)); }
  spr.drawCircle(mzCellX(0), mzCellY(0), mzCell * 0.32f, C(blend(board, INK, 0.25f)));   // start
  for (auto& w : mzRects) spr.fillRect(w.x, w.y, w.w, w.h, C(wall));
  // the ball: shadow, steel, light
  float br = mzR;
  if (mzState == MZ_FALL) br = mzR * max(0.2f, 1.0f - (millis() - mzStateT) / 500.0f);   // falls into the hole
  spr.fillCircle(mzBX + 2, mzBY + 2, br, C(blend(board, 0x000000, 0.25f)));
  spr.fillCircle(mzBX, mzBY, br, C(0x8F99A3));
  spr.fillCircle(mzBX - br * 0.15f, mzBY - br * 0.15f, br * 0.72f, C(0xB9C2CB));
  spr.fillCircle(mzBX - br * 0.38f, mzBY - br * 0.38f, max(1.5f, br * 0.25f), C(0xFFFFFF));
  // top bar
  spr.fillRect(0, 0, W, MZ_TOP, C(0x000000));
  uint32_t tms = mzState == MZ_PLAY ? millis() - mzStartMs : mzTimeMs;
  String l = "Level " + String(mzLevel) + "  " + mzClock(tms);
  spr.setFont(FB); int lw = spr.textWidth(l);
  uint16_t b = mzBest(mzLevel);
  String rt = String(b ? "Best " + mzClock(b * 100UL) + "  " : String("")) + "[Exit]";
  spr.setFont(FS); if ((int)spr.textWidth(rt) > W - 18 - lw) rt = "[Exit]";   // narrow: the best time is also in the Level done window
  spr.setTextColor(C(0x9AA7B4)); spr.setTextDatum(D_MR); spr.drawString(rt, W - 6, MZ_TOP / 2);
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML); spr.drawString(l, 6, MZ_TOP / 2);
  // windows
  if (mzState == MZ_READY || mzState == MZ_PAUSE || mzState == MZ_WIN) {
    int bw = W - 24, bh = mzState == MZ_PAUSE ? 176 : 150, bx = 12, by = (H - bh) / 2 + 6;
    spr.fillRoundRect(bx - 2, by - 2, bw + 4, bh + 4, 14, C(INK));
    spr.fillRoundRect(bx, by, bw, bh, 12, C(PAPER));
    auto line = [&](const lgfx::IFont* f, const String& t, int y, uint32_t col) { txt(f, fitText(f, t, bw - 16), W / 2, y, col, D_MC); };
    if (mzState == MZ_READY) {
      line(FL, "TILT MAZE", by + 22, INK);
      line(FB, (mzLevel > 1 ? "< " : "  ") + String("Level ") + mzLevel + (mzLevel < mzMaxLevel ? " >" : "  "), by + 54, INK);
      line(FS, joyOk ? "Roll it into the green hole" : "Hold a finger where it goes", by + 80, SOFT);
      line(FS, mzLevel >= 3 ? "Red holes: back to start" : "Left / right: pick a level", by + 100, SOFT);
      line(FB, joyOk ? "Press the stick to start" : "Tap here to start", by + 128, INK);
    } else if (mzState == MZ_WIN) {
      line(FL, "Level " + String(mzLevel) + " done!", by + 24, 0x2E9E5B);
      line(FB, "Time " + mzClock(mzTimeMs), by + 58, INK);
      uint16_t bb = mzBest(mzLevel);
      line(FS, bb * 100UL >= mzTimeMs - 99 ? "New best time!" : "Best " + mzClock(bb * 100UL), by + 82, SOFT);
      line(FB, joyOk ? "Press: next level" : "Tap: next level", by + 118, INK);
    } else {
      line(FL, "PAUSED", by + 22, INK);
      const char* M[3] = {"Resume", "Restart level", "Back to Games"};
      for (int i = 0; i < 3; i++) {
        int y = by + 44 + i * 42;
        btn(bx + 12, y, bw - 24, 36, M[i], false);
        if (i == mzMenuSel) for (int k = 0; k < 3; k++) spr.drawRoundRect(bx + 12 - 3 + k, y - 3 + k, bw - 24 + 6 - 2 * k, 42 - 2 * k, 10, C(NAV_RING_C));
      }
    }
  }
  spr.pushSprite(0, 0);
}
void mazeTapAt(int x, int y) {   // a finished tap (not a hold on the board)
  if (y < MZ_TOP + 6) { if (mzState == MZ_PLAY) mzTimeMs = millis() - mzStartMs; mazeLeave(); return; }   // [Exit]
  if (mzState == MZ_READY) {
    int by = (H - 150) / 2 + 6;
    if (y > by + 40 && y < by + 68) { if (x < W / 3 && mzLevel > 1) { mzLevel--; mzBuild(); return; } if (x > W * 2 / 3 && mzLevel < mzMaxLevel) { mzLevel++; mzBuild(); return; } }
    mzSetState(MZ_PLAY); mzStartMs = millis(); return;
  }
  if (mzState == MZ_WIN) { mzLevel++; mzBuild(); mzSetState(MZ_PLAY); mzStartMs = millis(); return; }
  if (mzState == MZ_PAUSE) {
    int bh = 176, by = (H - bh) / 2 + 6;
    for (int i = 0; i < 3; i++) if (y >= by + 44 + i * 42 && y < by + 80 + i * 42) { mzMenuDo(i); return; }
    return;
  }
}
void mazeLoop() {
  uint32_t now = millis();
  // finger
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (down) { lastTouchMs = now; if (pw != P_ON) wake(); }
  if (down && !mzTouchOn) { mzTouchOn = true; mzTX = tp.x; mzTY = tp.y; }
  else if (down) { mzTX = tp.x; mzTY = tp.y; }
  else if (!down && mzTouchOn) {
    mzTouchOn = false;
    if (mzState != MZ_PLAY || mzTY < MZ_TOP + 6) mazeTapAt(mzTX, mzTY);
    if (scr != S_MAZE) return;
  }
  // stick button: short press = start / next / choose; hold 1 s = pause
  if (joyOk) {
    bool sw = joyDown();
    if (sw && !mzSwWas) { mzSwT = now; mzHoldDone = false; }
    if (sw && !mzHoldDone && now - mzSwT > 1000 && mzState == MZ_PLAY) { mzHoldDone = true; mzTimeMs = now - mzStartMs; mzSetState(MZ_PAUSE); }
    if (!sw && mzSwWas && !mzHoldDone && now - mzSwT > 30) {
      lastTouchMs = now;
      if (mzState == MZ_READY) { mzSetState(MZ_PLAY); mzStartMs = now; }
      else if (mzState == MZ_WIN) { mzLevel++; mzBuild(); mzSetState(MZ_PLAY); mzStartMs = now; }
      else if (mzState == MZ_PAUSE) { mzMenuDo(mzMenuSel); if (scr != S_MAZE) return; }
    }
    mzSwWas = sw;
    // menus: up / down (pause), left / right (level on the start window)
    static uint32_t moveT = 0;
    if ((mzState == MZ_PAUSE || mzState == MZ_READY) && now - moveT > 220) {
      float jx, jy; joyVec(jx, jy);
      if (mzState == MZ_PAUSE && fabsf(jy) > 0.6f) { mzMenuSel = constrain(mzMenuSel + (jy > 0 ? 1 : -1), 0, 2); moveT = now; lastTouchMs = now; }
      if (mzState == MZ_READY && fabsf(jx) > 0.6f) {
        int nl = constrain(mzLevel + (jx > 0 ? 1 : -1), 1, mzMaxLevel);
        if (nl != mzLevel) { mzLevel = nl; mzBuild(); }
        moveT = now + 130; lastTouchMs = now;
      }
    }
  }
  if (now - mzFrameT < 33) return;   // about 30 pictures a second
  float dt = mzFrameT ? min(0.05f, (now - mzFrameT) / 1000.0f) : 0.033f;
  mzFrameT = now;
  if (mzState == MZ_PLAY) {
    lastTouchMs = now;   // keep the screen on while playing
    float ax = 0, ay = 0;
    if (mzTouchOn && mzTY > MZ_TOP + 6) { ax = constrain((mzTX - mzBX) / 70.0f, -1.0f, 1.0f); ay = constrain((mzTY - mzBY) / 70.0f, -1.0f, 1.0f); }
    else if (joyOk) joyVec(ax, ay);
    for (int k = 0; k < 4; k++) mzStep(ax, ay, dt / 4);
    float hr = mzCell * 0.34f;
    for (auto& t : mzTraps) if (hypotf(mzBX - t.x, mzBY - t.y) < hr * 0.7f) { mzBX = t.x; mzBY = t.y; mzVX = mzVY = 0; mzSetState(MZ_FALL); ledFlash(0xD03030, 300); }
    if (mzState == MZ_PLAY && hypotf(mzBX - mzCellX(mzGoalC), mzBY - mzCellY(mzGoalR)) < hr * 0.7f) { mzTimeMs = now - mzStartMs; mzBX = mzCellX(mzGoalC); mzBY = mzCellY(mzGoalR); mzWin(); }
  } else if (mzState == MZ_FALL) {
    if (now - mzStateT > 700) { mzBX = mzCellX(0); mzBY = mzCellY(0); mzVX = mzVY = 0; mzState = MZ_PLAY; }   // back to the start, the clock keeps going
  } else if (OFF_MS[offIdx] && now - lastTouchMs > OFF_MS[offIdx]) {   // left waiting: back to Games so the screen can turn off
    mazeLeave(); return;
  }
  mazeDraw();
}
String mazeSub() { int m = max(1, (int)prefs.getUChar("mzMax", 1)); return m > 1 ? "Level " + String(m) : String("Joystick"); }
