#pragma once
// ============================================================================
//  Game: "Pixel Swim" (back in v11.9, the Dragon of v11.6-v11.8 is gone). You are a glowing ball
//  in water made of pixels. Push the joystick to swim: the water moves out of the way, white like foam.
//  Eat the glowing dot -> you get bigger. Grow big enough and you reach the next level (you start small again).
//  v11.9: about twice as many water pixels (finer water), ripples and a splash when you eat, sounds, a light trail.
//  Joystick wiring: VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (!), GND -> GND
//  Included from SomudTick.ino.
// ============================================================================
#define HAS_PIXSWIM 2          // (the simulator tests look for it)
const int NP_MAX = 3400;       // water pixels (v11.5 had 1400)
struct Pix { float x, y, vx, vy; int16_t hx, hy; };
Pix* pix = nullptr; int NP = 0;
const int NFOOD = 1;           // the dot to eat
struct Food { float x, y; uint32_t col; };
Food food[NFOOD];
float px_, py_, pvx, pvy, pr;  // player position, speed, size (radius)
int score = 0, best = 0, level = 1;
uint32_t levelMsgT = 0;
enum GState { G_READY, G_PLAY, G_PAUSE, G_OVER };
GState gs = G_READY;
uint32_t lastFrame = 0;
const int GTOP = 24;            // score bar height
const float PR0 = 5;            // start size
const float PR_MAX = 34;        // this big = next level
struct Ripple { float x, y; uint32_t t; uint32_t col; };
Ripple rip[4]; int ripN = 0;    // rings on the water after eating
const int TRAILN = 10; float trX[TRAILN], trY[TRAILN]; int trI = 0;   // a soft light trail behind you
const uint32_t WATER = 0x041220;

float gRand(float a, float b) { return a + (b - a) * (random(10000) / 10000.0f); }
void placeFood(Food& f) {
  const uint32_t cols[] = {0xFFD23F, 0xFF9CEE, 0x6BE3FF, 0xFFFFFF, 0xB4FF6B};
  for (int t = 0; t < 20; t++) {
    f.x = gRand(10, W - 10); f.y = gRand(GTOP + 10, H - 10);
    if (hypotf(f.x - px_, f.y - py_) > pr + 30) break;
  }
  f.col = cols[random(5)];
}
void gameLevel(bool first) {
  pr = PR0; px_ = W / 2; py_ = (H + GTOP) / 2; pvx = pvy = 0;
  for (int i = 0; i < TRAILN; i++) { trX[i] = px_; trY[i] = py_; }
  for (auto& f : food) placeFood(f);
  if (!first) levelMsgT = millis();
}
void gameReset() {
  if (!pix) pix = (Pix*)heap_caps_malloc(sizeof(Pix) * NP_MAX, MALLOC_CAP_SPIRAM);
  if (!pix) return;
  // a grid about 5 px apart (tall 48 x 59, wide 64 x 43), each pixel a little off its place so it looks like water
  int cols = W / 5, rows = (H - GTOP) / 5;
  while (cols * rows > NP_MAX) rows--;
  NP = cols * rows;
  float gx = (float)W / cols, gy = (float)(H - GTOP) / rows;
  for (int i = 0; i < NP; i++) {
    int c = i % cols, r = i / cols;
    pix[i].hx = (int16_t)(c * gx + gx / 2 + random(-1, 2));
    pix[i].hy = (int16_t)(GTOP + r * gy + gy / 2 + random(-1, 2));
    pix[i].x = pix[i].hx; pix[i].y = pix[i].hy; pix[i].vx = pix[i].vy = 0;
  }
  score = 0; level = 1; levelMsgT = 0; ripN = 0;
  gameLevel(true);
}
void grow() { pr += 1.2f; }   // each dot makes you a bit bigger
JoyBtn gBtn;   // the stick's button: short press = start / pause, hold 1 s = leave
void gameOpen() {
  scr = S_GAME;
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  best = prefs.getInt("best", 0);
  joyMap = prefs.getUChar("joyMap", 0);
  joyCenter();
  joyBtnReset(gBtn);
  gameReset();
  gs = G_READY;
  lastFrame = 0;
}
// A game left alone (v11.5): it pauses when the screen dims (the Settings screen timer), or after 5 minutes when the
// timer is "Never". Then the screen dims and turns off as usual. A press wakes the screen, the next press plays on.
const uint32_t GAME_IDLE_NEVER_MS = 300000UL;
bool gameIdle() { return pw != P_ON || (!OFF_MS[offIdx] && millis() - lastTouchMs > GAME_IDLE_NEVER_MS); }
// the small buttons on the black top bar of the games: [Pause] [Exit]. Returns where they start (x).
const int GB_EXIT_W = 46, GB_PAUSE_W = 54;
int gameBarButtons(int barH, bool pause) {
  int x = W - 4 - GB_EXIT_W;
  auto b = [&](int bx, int bw, const char* t) {
    spr.drawRoundRect(bx, 3, bw, barH - 6, 6, C(0x5A6470));
    spr.setFont(FS); spr.setTextColor(C(0xDDE3EA)); spr.setTextDatum(D_MC); spr.drawString(t, bx + bw / 2, barH / 2);
  };
  b(x, GB_EXIT_W, "Exit");
  if (pause) { x -= GB_PAUSE_W + 4; b(x, GB_PAUSE_W, "Pause"); }
  return x;
}
int gameBarHit(int x) { return x >= W - 4 - GB_EXIT_W ? 1 : x >= W - 8 - GB_EXIT_W - GB_PAUSE_W ? 2 : 0; }   // 1 Exit, 2 Pause, 0 nothing
void gameStep() {
  if (!pix) return;
  float jx, jy; joyVec(jx, jy);   // follows the screen direction
  if (fabsf(jx) + fabsf(jy) > 0.15f) lastTouchMs = millis();   // swimming = using the board (keeps the screen on)
  float speed = 3.4f * powf(PR0 / pr, 0.35f);   // bigger = a bit slower
  pvx = pvx * 0.80f + jx * speed * 0.40f;
  pvy = pvy * 0.80f + jy * speed * 0.40f;
  px_ = constrain(px_ + pvx, pr, W - pr);
  py_ = constrain(py_ + pvy, GTOP + pr, H - pr);
  trI = (trI + 1) % TRAILN; trX[trI] = px_; trY[trI] = py_;
  // water: pushed away by you (and by a new ripple), then slowly flows back home
  const float R = pr + 16, R2 = R * R;
  for (int i = 0; i < NP; i++) {
    Pix& p = pix[i];
    float dx = p.x - px_, dy = p.y - py_, d2 = dx * dx + dy * dy;
    if (d2 < R2 && d2 > 0.01f) { float d = sqrtf(d2), f = (R - d) / R * 2.2f; p.vx += dx / d * f; p.vy += dy / d * f; }
    p.vx += (p.hx - p.x) * 0.02f; p.vy += (p.hy - p.y) * 0.02f;
    p.vx *= 0.86f; p.vy *= 0.86f;
    p.x += p.vx; p.y += p.vy;
  }
  for (int k = 0; k < ripN; k++) {   // a ring that grows for 0.3 s pushes the water once
    uint32_t age = millis() - rip[k].t; if (age > 300) continue;
    float rr = 8 + age * 0.12f;
    for (int i = 0; i < NP; i++) { Pix& p = pix[i]; float dx = p.x - rip[k].x, dy = p.y - rip[k].y, d = sqrtf(dx * dx + dy * dy);
      if (d > 0.5f && fabsf(d - rr) < 4) { p.vx += dx / d * 0.9f; p.vy += dy / d * 0.9f; } }
  }
  // eat the dot
  for (auto& f : food) if (hypotf(px_ - f.x, py_ - f.y) < pr + 3) {
    score++; grow(); ledFlash(f.col, 100); sfx(SFX_EAT);
    rip[ripN < 4 ? ripN++ : (int)(millis() % 4)] = Ripple{f.x, f.y, millis(), f.col};
    placeFood(f);
    if (score > best) {   // new best: keep it even if the board is switched off mid-game (at most every 10 s)
      best = score;
      static uint32_t savedT = 0;
      if (millis() - savedT > 10000) { savedT = millis(); prefs.putInt("best", best); }
    }
  }
  if (pr >= PR_MAX) { level++; score += 5; if (score > best) best = score; prefs.putInt("best", best); sfx(SFX_LEVEL); gameLevel(false); }   // big enough -> next level
  int w = 0; for (int k = 0; k < ripN; k++) if (millis() - rip[k].t < 900) rip[w++] = rip[k]; ripN = w;
}
void gameDraw() {
  spr.fillScreen(C(WATER));
  if (pix) for (int i = 0; i < NP; i++) {
    Pix& p = pix[i];
    float moved = fabsf(p.x - p.hx) + fabsf(p.y - p.hy);   // pushed pixels turn white like foam
    uint32_t c = blend(0x245F8E, 0xEAF7FF, min(1.0f, moved / 16.0f));
    spr.fillRect((int)p.x, (int)p.y, 2, 2, C(c));
  }
  for (int k = 0; k < ripN; k++) {   // rings after eating
    uint32_t age = millis() - rip[k].t; float t = age / 900.0f;
    spr.drawCircle(rip[k].x, rip[k].y, 6 + age * 0.06f, C(blend(rip[k].col, WATER, t)));
    if (age < 500) spr.drawCircle(rip[k].x, rip[k].y, 3 + age * 0.1f, C(blend(0xFFFFFF, WATER, min(1.0f, t * 1.6f))));
  }
  for (auto& f : food) {   // glowing dot, gently pulsing
    float g = 7 + 2 * sinf(millis() / 180.0f);
    spr.fillCircle(f.x, f.y, g + 3, C(blend(WATER, f.col, 0.15f)));
    spr.fillCircle(f.x, f.y, g, C(blend(WATER, f.col, 0.35f)));
    spr.fillCircle(f.x, f.y, 4, C(f.col));
  }
  for (int k = 1; k < TRAILN; k++) {   // light trail: older = smaller and darker
    int i = (trI + k) % TRAILN; float t = (float)k / TRAILN;
    spr.fillCircle(trX[i], trY[i], max(1.0f, pr * 0.45f * t), C(blend(WATER, 0xFFF4C8, 0.25f * t)));
  }
  spr.fillCircle(px_, py_, pr + 2, C(blend(WATER, 0xFFF4C8, 0.35f)));   // glow
  spr.fillCircle(px_, py_, pr, C(0xFFF4C8));
  spr.drawCircle(px_, py_, pr, C(0xFFFFFF));
  spr.fillCircle(px_ - pr * 0.35f, py_ - pr * 0.35f, max(1.0f, pr * 0.22f), C(0xFFFFFF));   // shine
  float el = min(pr * 0.5f, 1.0f + fabsf(pvx) + fabsf(pvy));
  spr.fillCircle(px_ + (pvx >= 0 ? 1 : -1) * min(fabsf(pvx), 1.0f) * el, py_ + (pvy >= 0 ? 1 : -1) * min(fabsf(pvy), 1.0f) * el, max(1.5f, pr / 4), C(WATER));   // eye looks where you swim
  // score bar: Score, level with a bar to the next level, [Pause] [Exit]
  spr.fillRect(0, 0, W, GTOP, C(0x000000));
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML);
  String sc = "Score " + String(score);
  spr.drawString(sc, 6, GTOP / 2);
  int x1 = 6 + spr.textWidth(sc) + 8;
  int bx = gameBarButtons(GTOP, true) - 6;
  spr.setFont(FS); spr.setTextColor(C(0x9AA7B4)); spr.setTextDatum(D_ML);
  String lv = "Lv" + String(level);
  if ((int)spr.textWidth(lv) + 30 <= bx - x1) {
    spr.drawString(lv, x1, GTOP / 2);
    int gx = x1 + spr.textWidth(lv) + 5, gw = bx - gx; float f = (pr - PR0) / (PR_MAX - PR0);
    if (gw > 12) { spr.drawRect(gx, GTOP / 2 - 3, gw, 6, C(0x33414F)); spr.fillRect(gx + 1, GTOP / 2 - 2, (int)((gw - 2) * constrain(f, 0.0f, 1.0f)), 4, C(0xFFD23F)); }
  }
  spr.drawFastHLine(0, GTOP - 1, W, C(0x1E3A55));
  if (levelMsgT && millis() - levelMsgT < 1800 && gs == G_PLAY) {
    float t = (millis() - levelMsgT) / 1800.0f;
    spr.setFont(FL); spr.setTextColor(C(blend(0xFFFFFF, WATER, t * t))); spr.setTextDatum(D_MC);
    spr.drawString("Level " + String(level) + "!", W / 2, (H + GTOP) / 2 - 40 - t * 20);
  }
  if (gs != G_PLAY) {
    static uint32_t stickT = 0; static String stickS;
    if (millis() - stickT > 300) { stickT = millis(); stickS = "Stick X " + String(joyRawX()) + "  Y " + String(joyRawY()); }
    int bw = W - 16, bh = 180, bx2 = 8, by = (H - bh) / 2;
    spr.fillRoundRect(bx2, by, bw, bh, 12, C(0x000000));
    spr.drawRoundRect(bx2, by, bw, bh, 12, C(0x6BE3FF));
    spr.setTextDatum(D_MC); spr.setTextColor(C(0xFFFFFF));
    spr.setFont(FL);
    spr.drawString(gs == G_PAUSE ? "PAUSED" : "PIXEL SWIM", W / 2, by + 22);
    auto line = [&](const String& t, int y, uint32_t col) { spr.setFont(FS); spr.setTextColor(C(col)); spr.setTextDatum(D_MC); spr.drawString(fitText(FS, t, bw - 12), W / 2, y); };
    line("Eat the dot to grow.", by + 50, 0xFFFFFF);
    line("Grow big = next level.", by + 72, 0xFFFFFF);
    line(gs == G_PAUSE ? "Press: go on  Hold: exit" : "Press: start  Hold: exit", by + 96, 0xFFE9A8);
    line("Best " + String(best), by + 120, 0x9AA7B4);
    line(stickS, by + 142, 0x9AA7B4);
    line(W > H ? "Wrong way? Settings > Joystick" : "Wrong way? See Settings", by + 164, 0x9AA7B4);
  }
  spr.pushSprite(0, 0);
}
// called from loop() while the game screen is open
void gameLoop() {
  if (millis() - lastFrame < 33) return;   // about 30 frames per second
  lastFrame = millis();
  int b = joyBtnRead(gBtn);
  if (b && pw != P_ON) { wake(); b = 0; }   // screen dim or off (paused in a pocket): a press only wakes it
  if (b == 2) {   // hold the press 1 s = back to Games (the same in every game)
    if (best > prefs.getInt("best", 0)) prefs.putInt("best", best);
    gs = G_READY; scr = S_GAMES; dirty = true; joyNavReset(); return;
  }
  if (b == 1) {
    lastTouchMs = millis();
    if (gs == G_READY) { joyCenter(); gs = G_PLAY; }
    else if (gs == G_PLAY) gs = G_PAUSE;
    else if (gs == G_PAUSE) gs = G_PLAY;
    else if (gs == G_OVER) { gameReset(); joyCenter(); gs = G_PLAY; }
  }
  if (gs == G_PLAY && gameIdle()) gs = G_PAUSE;   // left alone: pause (the screen dims and turns off as usual)
  if (pw == P_OFF) return;                         // nothing to draw on a dark screen
  if (gs == G_PLAY) gameStep();
  gameDraw();
}
void gameTap(int x, int y) {
  if (y < GTOP + 10) {   // the top bar: [Pause] [Exit]
    int h = gameBarHit(x);
    if (h == 1) {
      if (best > prefs.getInt("best", 0)) prefs.putInt("best", best);
      gs = G_READY; scr = S_GAMES; dirty = true; joyNavReset();
    } else if (h == 2 && gs == G_PLAY) gs = G_PAUSE;
    return;
  }
  if (gs == G_READY || gs == G_PAUSE) { lastTouchMs = millis(); gs = G_PLAY; }   // no stick: a tap starts / goes on
}

// ---------------- Joystick direction setup ----------------
// Asks you to push UP and then RIGHT, and remembers how the stick is turned.
void joyMsg(const String& a, const String& b, const String& c) {
  lcd.fillScreen(TFT_WHITE);
  lcd.setTextDatum(D_MC); lcd.setTextColor(TFT_BLACK);
  int cx = lcd.width() / 2, cy = lcd.height() / 2;
  lcd.setFont(FL); lcd.drawString(a, cx, cy - 40);
  lcd.setFont(FS); lcd.drawString(b, cx, cy - 6); lcd.drawString(c, cx, cy + 18);
  lcd.setTextColor(0x8410); lcd.drawString("Tap the screen to cancel", cx, lcd.height() - 20);
}
bool joyTapped() { lgfx::touch_point_t tp; if (lcd.getTouch(&tp)) { while (lcd.getTouch(&tp)) delay(5); return true; } return false; }
// wait until the stick is pushed far; returns false on cancel/timeout. dx, dy are -1..1
bool joyWaitPush(float& dx, float& dy) {
  uint32_t t = millis();
  while (millis() - t < 20000) {
    dx = (joyRawX() - joyCX) / 2048.0f; dy = (joyRawY() - joyCY) / 2048.0f;
    if (fabsf(dx) > 0.6f || fabsf(dy) > 0.6f) { delay(150); return true; }
    if (joyTapped()) return false;
    delay(20);
  }
  return false;
}
bool joyWaitCenter() {
  uint32_t t = millis();
  while (millis() - t < 20000) {
    if (abs(joyRawX() - joyCX) < 400 && abs(joyRawY() - joyCY) < 400) { delay(300); return true; }
    if (joyTapped()) return false;
    delay(20);
  }
  return false;
}
void joySetup() {
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  lcd.setBrightness(BRIGHT[brightIdx]);
  joyMsg("Joystick", "Let go of the stick.", "Wait 2 seconds...");
  delay(1500);
  joyCenter();
  float ux, uy, rx, ry;
  bool ok = false;
  joyMsg("Push UP", "Push the stick UP", "and hold it there.");
  if (joyWaitPush(ux, uy)) {
    joyMsg("Let go", "Let the stick go back", "to the middle.");
    if (joyWaitCenter()) {
      joyMsg("Push RIGHT", "Push the stick RIGHT", "and hold it there.");
      if (joyWaitPush(rx, ry)) ok = true;
    }
  }
  if (ok) {
    uint8_t m = 0;
    bool swap = fabsf(ux) > fabsf(uy);          // "up" moved the X reading -> the stick is turned sideways
    if (swap) m |= 1;
    float upV = swap ? ux : uy;                 // up should give a negative number
    float rightV = swap ? ry : rx;              // right should give a positive number
    if (upV > 0) m |= 4;
    if (rightV < 0) m |= 2;
    joyMap = m; prefs.putUChar("joyMap", joyMap);
    joyRot = rot; prefs.putUChar("joyRot", joyRot);   // the directions are for this screen direction (others are turned from it)
    joyOk = true;
    joyMsg("Done!", "The stick direction is saved.", "Try it in Apps > Games.");
  } else {
    joyMsg("Not saved", "No stick movement found.", "Check the wires (VRX IO2, VRY IO3).");
  }
  delay(2000);
  lcd.setRotation(rot);
  tDown = false; tHandled = true; lastTouchMs = millis(); pw = P_ON;
  dirty = true;
}
