#pragma once
// ============================================================================
//  Game: "Blocks". Falling blocks: fill a whole row and it disappears.
//  Stick: left / right = move, down = faster, flick up = drop, press = turn, hold the press 1 s = back to Games (v11.5).
//  [Pause] on the top bar (or a tap beside the board) = the pause menu. Left alone it pauses by itself (v11.5).
//  No stick: tap the left / right part of the board = move, the middle = turn, swipe down = drop.
//  Every level (10 rows cleared) the blocks fall faster.
//  Included from SomudTick.ino.
// ============================================================================

const int BL_TOP = 24;
const int BL_W = 10, BL_H = 20;
uint8_t blBoard[BL_H][BL_W];   // 0 empty, 1..7 piece colour
// the 7 pieces, 4 turns each, as 4x4 pictures (bit 15 = top-left)
const uint16_t BL_SHAPE[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},   // I
  {0x8E00, 0x6440, 0x0E20, 0x44C0},   // J
  {0x2E00, 0x4460, 0x0E80, 0xC440},   // L
  {0x6600, 0x6600, 0x6600, 0x6600},   // O
  {0x6C00, 0x4620, 0x06C0, 0x8C40},   // S
  {0x4E00, 0x4640, 0x0E40, 0x4C40},   // T
  {0xC600, 0x2640, 0x0C60, 0x4C80},   // Z
};
const uint32_t BL_COL[8] = {0, 0x3BC6E8, 0x3B6FE8, 0xF0A030, 0xF2D23B, 0x4CC45A, 0xA15CE0, 0xE84B4B};
int blPiece = 0, blRot = 0, blX = 3, blY = -1, blNext = 0;
uint8_t blBag[7]; int blBagN = 0;
long blScore = 0, blBest = 0; int blLines = 0, blLevel = 1;
enum BlState : uint8_t { BL_READY, BL_PLAY, BL_PAUSE, BL_OVER, BL_CLEAR };
BlState blState = BL_READY;
uint32_t blFallT = 0, blLockT = 0, blFrameT = 0, blClearT = 0, blMoveT = 0, blDownT = 0;
int blLockMoves = 0; bool blLanded = false;
uint8_t blClearRows[4]; int blClearN = 0;
int blMenuSel = 0; int blMoveDir = 0; bool blUpArmed = true;
bool blSwWas = false, blHoldDone = false; uint32_t blSwT = 0;
bool blWakeHold = false;   // the touch / press that woke the screen is still down: it does nothing
bool blTouchOn = false; int blTX0, blTY0, blTX, blTY;
int blCell = 14, blBX = 8, blBY = 28;   // board place on screen

bool blCellOf(uint16_t m, int r, int c) { return m & (0x8000 >> (r * 4 + c)); }
bool blFits(int p, int rot, int x, int y) {
  uint16_t m = BL_SHAPE[p][rot & 3];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
    if (!blCellOf(m, r, c)) continue;
    int bx = x + c, by = y + r;
    if (bx < 0 || bx >= BL_W || by >= BL_H) return false;
    if (by >= 0 && blBoard[by][bx]) return false;
  }
  return true;
}
int blTakeFromBag() {   // "7-bag": every 7 pieces contain each piece once (no long waits for the long I)
  if (!blBagN) { for (int i = 0; i < 7; i++) blBag[i] = i; for (int i = 6; i > 0; i--) { int j = random(i + 1); uint8_t t = blBag[i]; blBag[i] = blBag[j]; blBag[j] = t; } blBagN = 7; }
  return blBag[--blBagN];
}
int blDropMs() { return max(70, (int)(800 * powf(0.83f, blLevel - 1))); }
void blSpawn() {
  blPiece = blNext; blNext = blTakeFromBag(); blRot = 0; blX = 3; blY = blPiece == 0 ? -1 : 0;
  blLanded = false; blLockMoves = 0; blFallT = millis();
  if (!blFits(blPiece, blRot, blX, blY)) {   // no room at the top: game over
    blState = BL_OVER; blMenuSel = 0;
    if (blScore > blBest) { blBest = blScore; prefs.putUInt("blBest", (uint32_t)blBest); }
    ledFlash(0xD03030, 400);
  }
}
void blNew() {
  memset(blBoard, 0, sizeof blBoard);
  blScore = 0; blLines = 0; blLevel = 1; blBagN = 0;
  blNext = blTakeFromBag(); blSpawn();
}
void blLayout() {
  if (W > H) { blCell = (H - BL_TOP - 8) / BL_H; blBX = (W - blCell * BL_W) / 2; }
  else { blCell = min((H - BL_TOP - 8) / BL_H, (W - 92) / BL_W); blBX = 8; }
  blBY = BL_TOP + 4;
}
void blocksOpen() {
  blBest = prefs.getUInt("blBest", 0);
  blLayout(); randomSeed(esp_random()); blNew(); blState = BL_READY;
  if (joyOk) joyCenter();
  blSwWas = joyOk && joyDown(); blHoldDone = blSwWas; blTouchOn = false; blUpArmed = true; blWakeHold = false;
  scr = S_BLOCKS;
}
void blocksLeave() {
  if (blScore > blBest) { blBest = blScore; prefs.putUInt("blBest", (uint32_t)blBest); }
  scr = S_GAMES; dirty = true; joyNavReset();
}
bool blMove(int dx) {
  if (!blFits(blPiece, blRot, blX + dx, blY)) return false;
  blX += dx;
  if (blLanded && blLockMoves < 15) { blLockT = millis(); blLockMoves++; }   // moving on the floor gives a little more time
  return true;
}
void blTurn() {
  const int KX[6] = {0, -1, 1, 0, -2, 2}, KY[6] = {0, 0, 0, -1, 0, 0};   // try beside / above if the wall is in the way
  int nr = (blRot + 1) & 3;
  for (int k = 0; k < 6; k++) if (blFits(blPiece, nr, blX + KX[k], blY + KY[k])) {
    blRot = nr; blX += KX[k]; blY += KY[k];
    if (blLanded && blLockMoves < 15) { blLockT = millis(); blLockMoves++; }
    return;
  }
}
void blLock() {
  uint16_t m = BL_SHAPE[blPiece][blRot];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++)
    if (blCellOf(m, r, c) && blY + r >= 0) blBoard[blY + r][blX + c] = blPiece + 1;
  blClearN = 0;
  for (int r = 0; r < BL_H; r++) { bool full = true; for (int c = 0; c < BL_W; c++) if (!blBoard[r][c]) full = false; if (full && blClearN < 4) blClearRows[blClearN++] = r; }
  if (blClearN) { blState = BL_CLEAR; blClearT = millis(); ledFlash(0xFFFFFF, 120); return; }
  blSpawn();
}
void blFinishClear() {
  for (int k = 0; k < blClearN; k++) {   // rows are in top-to-bottom order: removing one moves the ones above down
    int row = blClearRows[k];
    for (int r = row; r > 0; r--) memcpy(blBoard[r], blBoard[r - 1], BL_W);
    memset(blBoard[0], 0, BL_W);
  }
  const int PTS[5] = {0, 100, 300, 500, 800};
  blScore += PTS[blClearN] * blLevel;
  blLines += blClearN; blLevel = blLines / 10 + 1;
  blClearN = 0; blState = BL_PLAY;
  blSpawn();
}
void blHardDrop() {
  int n = 0; while (blFits(blPiece, blRot, blX, blY + 1)) { blY++; n++; }
  blScore += 2 * n; blLock();
}
bool blSoftStep() {   // one row down (true if it moved)
  if (blFits(blPiece, blRot, blX, blY + 1)) { blY++; blLanded = false; return true; }
  if (!blLanded) { blLanded = true; blLockT = millis(); }
  return false;
}
int blGhostY() { int y = blY; while (blFits(blPiece, blRot, blX, y + 1)) y++; return y; }
void blDrawCell(int x, int y, uint32_t col, bool ghost = false) {
  int s = blCell;
  if (ghost) { spr.drawRoundRect(x + 1, y + 1, s - 2, s - 2, 2, C(blend(CARD, col, 0.7f))); return; }
  spr.fillRoundRect(x, y, s - 1, s - 1, 2, C(col));
  spr.drawFastHLine(x + 1, y + 1, s - 3, C(blend(col, 0xFFFFFF, 0.45f)));   // a little shine on top
}
void blDrawMini(int p, int x, int y, int s) {   // the next piece, small
  uint16_t m = BL_SHAPE[p][0];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (blCellOf(m, r, c)) spr.fillRoundRect(x + c * s, y + r * s, s - 1, s - 1, 2, C(BL_COL[p + 1]));
}
void blocksDraw() {
  spr.fillScreen(C(PAPER));
  int bw = blCell * BL_W, bh = blCell * BL_H;
  spr.fillRoundRect(blBX - 3, blBY - 3, bw + 6, bh + 6, 6, C(INK));
  spr.fillRect(blBX, blBY, bw, bh, C(CARD));
  for (int c = 1; c < BL_W; c++) spr.drawFastVLine(blBX + c * blCell - 1, blBY, bh, C(blend(CARD, LINE, 0.5f)));
  bool flash = blState == BL_CLEAR && ((millis() - blClearT) / 60) % 2 == 0;
  for (int r = 0; r < BL_H; r++) {
    bool clearing = false; for (int k = 0; k < blClearN; k++) if (blClearRows[k] == r) clearing = true;
    for (int c = 0; c < BL_W; c++) if (blBoard[r][c]) blDrawCell(blBX + c * blCell, blBY + r * blCell, clearing && flash ? 0xFFFFFF : BL_COL[blBoard[r][c]]);
  }
  if (blState == BL_PLAY || blState == BL_PAUSE) {
    uint16_t m = BL_SHAPE[blPiece][blRot];
    int gy = blGhostY();
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (blCellOf(m, r, c)) {
      if (gy + r >= 0 && gy != blY) blDrawCell(blBX + (blX + c) * blCell, blBY + (gy + r) * blCell, BL_COL[blPiece + 1], true);
      if (blY + r >= 0) blDrawCell(blBX + (blX + c) * blCell, blBY + (blY + r) * blCell, BL_COL[blPiece + 1]);
    }
  }
  // side: next piece and numbers (tall: right of the board; wide: left and right of it)
  bool wide = W > H;
  int px = wide ? 6 : blBX + bw + 10, pw_ = wide ? blBX - 14 : W - px - 6, py = blBY;
  auto box = [&](int x, int y, int w, const String& k, const String& v) {
    spr.fillRoundRect(x, y, w, 40, 8, C(CARD));
    txt(FS, k, x + w / 2, y + 11, SOFT, D_MC);
    txt(FB, fitText(FB, v, w - 6), x + w / 2, y + 29, INK, D_MC);
  };
  spr.fillRoundRect(px, py, pw_, 64, 8, C(CARD));
  txt(FS, "Next", px + pw_ / 2, py + 10, SOFT, D_MC);
  { int s = min(12, (pw_ - 10) / 4); blDrawMini(blNext, px + (pw_ - 4 * s) / 2 + (blNext == 3 ? s / 2 : 0), py + 24 + (blNext == 0 ? -s / 2 : 0), s); }
  int qx = wide ? blBX + bw + 8 : px, qw = wide ? W - qx - 6 : pw_, qy = wide ? blBY : py + 70;
  box(qx, qy, qw, "Score", String(blScore));
  box(qx, qy + 46, qw, "Lines", String(blLines));
  box(qx, qy + 92, qw, "Level", String(blLevel));
  if (wide) box(px, py + 70, pw_, "Best", String(blBest)); else box(px, qy + 138, pw_, "Best", String(blBest));
  // top bar
  spr.fillRect(0, 0, W, BL_TOP, C(0x000000));
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML); spr.drawString("Blocks", 6, BL_TOP / 2);
  gameBarButtons(BL_TOP, blState == BL_PLAY);   // [Pause] [Exit]
  // windows
  if (blState == BL_READY || blState == BL_PAUSE || blState == BL_OVER) {
    int ww = min(W - 24, 220), wh = blState == BL_PAUSE || blState == BL_READY ? 176 : 150, wx = (W - ww) / 2, wy = (H - wh) / 2 + 6;
    spr.fillRoundRect(wx - 2, wy - 2, ww + 4, wh + 4, 14, C(INK));
    spr.fillRoundRect(wx, wy, ww, wh, 12, C(PAPER));
    auto line = [&](const lgfx::IFont* f, const String& t, int y, uint32_t col) { txt(f, fitText(f, t, ww - 14), W / 2, y, col, D_MC); };
    if (blState == BL_READY) {
      line(FL, "BLOCKS", wy + 22, INK);
      // one thing per line (v11.5: two per line were cut on the screen)
      const char* J[5] = {"Left / right: move", "Press: turn", "Down: faster", "Flick up: drop", "Hold the press: exit"};
      const char* T[5] = {"Tap left / right: move", "Tap the middle: turn", "Swipe down: drop", "Fill a row: it goes away", ""};
      for (int k = 0; k < 5; k++) line(FS, joyOk ? J[k] : T[k], wy + 48 + k * 19, SOFT);
      line(FB, joyOk ? "Press the stick to start" : "Tap here to start", wy + 155, INK);
    } else if (blState == BL_OVER) {
      line(FL, "Game over", wy + 24, INK);
      line(FB, "Score " + String(blScore), wy + 58, INK);
      line(FS, blScore >= blBest && blScore > 0 ? "New best!" : "Best " + String(blBest), wy + 82, SOFT);
      line(FB, joyOk ? "Press: play again" : "Tap: play again", wy + 118, INK);
    } else {
      line(FL, "PAUSED", wy + 22, INK);
      const char* M[3] = {"Resume", "New game", "Back to Games"};
      for (int i = 0; i < 3; i++) {
        int y = wy + 44 + i * 42;
        btn(wx + 12, y, ww - 24, 36, M[i], false);
        if (i == blMenuSel) for (int k = 0; k < 3; k++) spr.drawRoundRect(wx + 12 - 3 + k, y - 3 + k, ww - 24 + 6 - 2 * k, 42 - 2 * k, 10, C(NAV_RING_C));
      }
    }
  }
  spr.pushSprite(0, 0);
}
void blMenuDo(int i) {
  if (i == 0) { blState = BL_PLAY; blFallT = millis(); }
  else if (i == 1) { if (blScore > blBest) { blBest = blScore; prefs.putUInt("blBest", (uint32_t)blBest); } blNew(); blState = BL_PLAY; }
  else blocksLeave();
}
void blTapAt(int x, int y) {
  if (y < BL_TOP + 6) {   // the top bar: [Pause] [Exit]
    int h = gameBarHit(x);
    if (h == 1) blocksLeave();
    else if (h == 2 && blState == BL_PLAY) { blState = BL_PAUSE; blMenuSel = 0; }
    return;
  }
  if (blState == BL_READY || blState == BL_OVER) { if (blState == BL_OVER) blNew(); blState = BL_PLAY; blFallT = millis(); return; }
  if (blState == BL_PAUSE) {
    int wh = 176, wy = (H - wh) / 2 + 6;
    for (int i = 0; i < 3; i++) if (y >= wy + 44 + i * 42 && y < wy + 80 + i * 42) { blMenuDo(i); return; }
    return;
  }
  if (blState != BL_PLAY) return;
  int bw = blCell * BL_W;
  if (x < blBX || x > blBX + bw) { blState = BL_PAUSE; blMenuSel = 0; return; }   // beside the board = pause
  if (x < blBX + bw / 3) blMove(-1); else if (x > blBX + bw * 2 / 3) blMove(1); else blTurn();
}
void blocksLoop() {
  uint32_t now = millis();
  // finger
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (pw == P_OFF || blWakeHold) {   // screen off (in a pocket): a touch or a press only wakes it, and does nothing else
    bool any = down || (joyOk && joyDown());
    if (pw == P_OFF) { if (any) { wake(); blWakeHold = true; } return; }
    if (any) return;   // wait until it is let go
    blWakeHold = false; blTouchOn = false; blSwWas = false; blHoldDone = false; return;
  }
  if (down) { lastTouchMs = now; if (pw != P_ON) wake(); }
  if (down && !blTouchOn) { blTouchOn = true; blTX0 = blTX = tp.x; blTY0 = blTY = tp.y; }
  else if (down) { blTX = tp.x; blTY = tp.y; }
  else if (!down && blTouchOn) {
    blTouchOn = false;
    if (blState == BL_PLAY && blTY - blTY0 > 40 && abs(blTX - blTX0) < 40) blHardDrop();   // swipe down = drop
    else blTapAt(blTX0, blTY0);
    if (scr != S_BLOCKS) return;
  }
  // stick
  if (joyOk) {
    bool sw = joyDown();
    if (sw && !blSwWas) {
      blSwT = now; blHoldDone = false; lastTouchMs = now;
      if (pw != P_ON) { wake(); blHoldDone = true; }   // dimmed: the press only brightens the screen
      else if (blState == BL_PLAY) blTurn();          // turn at once when pressed (no waiting)
    }
    if (sw && !blHoldDone && now - blSwT > 1000) { blHoldDone = true; blocksLeave(); return; }   // hold 1 s = back to Games
    if (!sw && blSwWas && !blHoldDone && now - blSwT > 30) {
      if (blState == BL_READY) { blState = BL_PLAY; blFallT = now; }
      else if (blState == BL_OVER) { blNew(); blState = BL_PLAY; }
      else if (blState == BL_PAUSE) { blMenuDo(blMenuSel); if (scr != S_BLOCKS) return; }
    }
    blSwWas = sw;
    float jx, jy; joyVec(jx, jy);
    if (fabsf(jx) > 0.2f || fabsf(jy) > 0.2f) { lastTouchMs = now; if (pw != P_ON) wake(); }
    if (blState == BL_PLAY) {
      int dir = jx < -0.5f ? -1 : jx > 0.5f ? 1 : 0;
      if (dir != blMoveDir) { blMoveDir = dir; if (dir) { blMove(dir); blMoveT = now + 170; } }   // first step at once, then repeat
      else if (dir && (int32_t)(now - blMoveT) >= 0) { blMove(dir); blMoveT = now + 50; }
      if (jy > 0.5f && fabsf(jy) > fabsf(jx) && now - blDownT > 40) { blDownT = now; if (blSoftStep()) { blScore++; blFallT = now; } }   // down = faster
      if (jy < -0.7f && fabsf(jy) > 1.5f * fabsf(jx)) { if (blUpArmed) { blUpArmed = false; blHardDrop(); } }   // flick up = drop
      else if (jy > -0.3f) blUpArmed = true;
    } else if (blState == BL_PAUSE) {
      static uint32_t mT = 0;
      if (fabsf(jy) > 0.6f && now - mT > 220) { blMenuSel = constrain(blMenuSel + (jy > 0 ? 1 : -1), 0, 2); mT = now; }
    }
  }
  if (now - blFrameT < 33) return;
  blFrameT = now;
  if (blState == BL_PLAY && gameIdle()) { blState = BL_PAUSE; blMenuSel = 0; }   // left alone: pause (the screen dims and turns off as usual)
  if (blState == BL_PLAY) {
    if (now - blFallT >= (uint32_t)blDropMs()) { blFallT = now; blSoftStep(); }
    if (blLanded && !blFits(blPiece, blRot, blX, blY + 1) && now - blLockT > 450) blLock();
    else if (blLanded && blFits(blPiece, blRot, blX, blY + 1)) blLanded = false;   // moved off the edge: falls again
  } else if (blState == BL_CLEAR) {
    if (now - blClearT > 240) blFinishClear();
  }
  blocksDraw();
}
String blocksSub() { uint32_t b = prefs.getUInt("blBest", 0); return b ? "Best " + String(b) : String("Joystick"); }
