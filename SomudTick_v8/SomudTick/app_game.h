#pragma once
// ============================================================================
//  Game: "Pixel Swim". You are a dot swimming in water made of pixels.
//  Push the joystick to swim, the water moves out of the way.
//  Eat the glowing dot -> you get bigger.
//  Grow big enough and you reach the next level (you start small again).
//  Joystick wiring: VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (!), GND -> GND
//  Included from SomudTick.ino.
// ============================================================================
#define JOY_X  2
#define JOY_Y  3
#define JOY_SW 14

const int NP = 1400;           // water pixels (dense background)
struct Pix { float x, y, vx, vy; int16_t hx, hy; };
Pix* pix = nullptr;
const int NFOOD = 1;           // dot to eat
struct Food { float x, y; uint32_t col; };
Food food[NFOOD];
float px_, py_, pvx, pvy, pr;  // player position, speed, size (radius)
int score = 0, best = 0, level = 1;
uint32_t levelMsgT = 0;
enum GState { G_READY, G_PLAY, G_PAUSE, G_OVER };
GState gs = G_READY;
int joyCX = 2048, joyCY = 2048;
uint8_t joyMap = 0;   // bit0 = swap X/Y, bit1 = flip left-right, bit2 = flip up-down (set by "Joystick direction")
bool swPrev = true; uint32_t swT = 0;
uint32_t lastFrame = 0;
const int GTOP = 24;            // score bar height
const float PR0 = 5;            // start size
const float PR_MAX = 34;        // this big = next level

int joyRawX() { return analogRead(JOY_X); }
int joyRawY() { return analogRead(JOY_Y); }
void joyCenter() {   // remember where the stick rests
  long sx = 0, sy = 0;
  for (int i = 0; i < 16; i++) { sx += joyRawX(); sy += joyRawY(); delay(2); }
  joyCX = sx / 16; joyCY = sy / 16;
}
float joyAxis(int raw, int c) {   // -1 .. +1 with a small dead zone
  float v = raw >= c ? (raw - c) / float(max(1, 4095 - c)) : (raw - c) / float(max(1, c));
  if (fabsf(v) < 0.12f) return 0;
  return constrain(v, -1.0f, 1.0f);
}
bool joyPressed() {   // true once per press
  bool v = digitalRead(JOY_SW);
  bool fired = false;
  if (v != swPrev && millis() - swT > 40) { swT = millis(); swPrev = v; if (!v) fired = true; }
  return fired;
}
// Is a joystick plugged in? A free (unplugged) pin gives jumpy readings,
// a real stick at rest gives a steady middle value.
bool joyDetect() {
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  joyMap = prefs.getUChar("joyMap", 0);
  int mnx = 4095, mxx = 0, mny = 4095, mxy = 0; long sx = 0, sy = 0;
  for (int i = 0; i < 24; i++) {
    int x = joyRawX(), y = joyRawY();
    mnx = min(mnx, x); mxx = max(mxx, x); mny = min(mny, y); mxy = max(mxy, y); sx += x; sy += y;
    delay(1);
  }
  int cx = sx / 24, cy = sy / 24;
  bool ok = mxx - mnx < 250 && mxy - mny < 250 && cx > 900 && cx < 3200 && cy > 900 && cy < 3200;
  if (ok) { joyCX = cx; joyCY = cy; }
  swPrev = digitalRead(JOY_SW);
  return ok;
}
float gRand(float a, float b) { return a + (b - a) * (random(10000) / 10000.0f); }
void placeFood(Food& f) {
  const uint32_t cols[] = {0xFFD23F, 0xFF9CEE, 0x6BE3FF, 0xFFFFFF, 0xB4FF6B};
  for (int t = 0; t < 20; t++) {
    f.x = gRand(8, W - 8); f.y = gRand(GTOP + 8, H - 8);
    if (hypotf(f.x - px_, f.y - py_) > pr + 30) break;
  }
  f.col = cols[random(5)];
}
void gameLevel(bool first) {
  pr = PR0; px_ = W / 2; py_ = (H + GTOP) / 2; pvx = pvy = 0;
  for (auto& f : food) placeFood(f);
  if (!first) levelMsgT = millis();
}
void gameReset() {
  if (!pix) pix = (Pix*)heap_caps_malloc(sizeof(Pix) * NP, MALLOC_CAP_SPIRAM);
  int cols = W > H ? 46 : 36, rows = NP / cols;
  float gx = (float)W / cols, gy = (float)(H - GTOP) / rows;
  for (int i = 0; i < NP; i++) {
    int c = i % cols, r = i / cols;
    pix[i].hx = (int16_t)(c * gx + gx / 2 + random(-2, 3));
    pix[i].hy = (int16_t)(GTOP + r * gy + gy / 2 + random(-2, 3));
    pix[i].x = pix[i].hx; pix[i].y = pix[i].hy; pix[i].vx = pix[i].vy = 0;
  }
  score = 0; level = 1; levelMsgT = 0;
  gameLevel(true);
}
void grow() { pr += 1.2f; }   // each dot makes you a bit bigger
void gameOpen() {
  scr = S_GAME;
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  best = prefs.getInt("best", 0);
  joyMap = prefs.getUChar("joyMap", 0);
  joyCenter();
  gameReset();
  gs = G_READY;
  lastFrame = 0;
}
void gameStep() {
  float jx = joyAxis(joyRawX(), joyCX), jy = joyAxis(joyRawY(), joyCY);
  if (joyMap & 1) { float t = jx; jx = jy; jy = t; }
  if (joyMap & 2) jx = -jx;
  if (joyMap & 4) jy = -jy;
  float speed = 3.4f * powf(PR0 / pr, 0.35f);   // bigger = a bit slower
  pvx = pvx * 0.80f + jx * speed * 0.40f;
  pvy = pvy * 0.80f + jy * speed * 0.40f;
  px_ = constrain(px_ + pvx, pr, W - pr);
  py_ = constrain(py_ + pvy, GTOP + pr, H - pr);
  // water: pushed away by you, then slowly flows back home
  for (int i = 0; i < NP; i++) {
    Pix& p = pix[i];
    auto push = [&](float cx, float cy, float R, float k) {
      float dx = p.x - cx, dy = p.y - cy, d2 = dx * dx + dy * dy;
      if (d2 < R * R && d2 > 0.01f) { float d = sqrtf(d2), f = (R - d) / R * k; p.vx += dx / d * f; p.vy += dy / d * f; }
    };
    push(px_, py_, pr + 16, 2.2f);
    p.vx += (p.hx - p.x) * 0.02f; p.vy += (p.hy - p.y) * 0.02f;
    p.vx *= 0.86f; p.vy *= 0.86f;
    p.x += p.vx; p.y += p.vy;
  }
  // eat the dot
  for (auto& f : food) if (hypotf(px_ - f.x, py_ - f.y) < pr + 3) {
    score++; grow(); ledFlash(f.col, 100); placeFood(f);
    if (score > best) best = score;
  }
  if (pr >= PR_MAX) { level++; score += 5; if (score > best) best = score; prefs.putInt("best", best); gameLevel(false); }   // big enough -> next level
}
void gameDraw() {
  const uint32_t WATER = 0x06121F;
  spr.fillScreen(C(WATER));
  for (int i = 0; i < NP; i++) {
    Pix& p = pix[i];
    float moved = fabsf(p.x - p.hx) + fabsf(p.y - p.hy);   // pushed pixels turn white like foam
    uint32_t c = blend(0x2E6A9E, 0xE8F6FF, min(1.0f, moved / 18.0f));
    spr.fillRect((int)p.x, (int)p.y, 2, 2, C(c));
  }
  for (auto& f : food) {   // glowing dot, gently pulsing
    float g = 7 + 2 * sinf(millis() / 180.0f);
    spr.fillCircle(f.x, f.y, g, C(blend(WATER, f.col, 0.3f)));
    spr.fillCircle(f.x, f.y, 4, C(f.col));
  }
  spr.fillCircle(px_, py_, pr, C(0xFFF4C8));
  spr.drawCircle(px_, py_, pr, C(0xFFFFFF));
  float el = min(pr * 0.5f, 1.0f + fabsf(pvx) + fabsf(pvy));
  spr.fillCircle(px_ + (pvx >= 0 ? 1 : -1) * min(fabsf(pvx), 1.0f) * el, py_ + (pvy >= 0 ? 1 : -1) * min(fabsf(pvy), 1.0f) * el, max(1.5f, pr / 4), C(0x06121F));   // eye looks where you swim
  // score bar
  spr.fillRect(0, 0, W, GTOP, C(0x000000));
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML);
  String sc = "Score " + String(score);
  spr.drawString(sc, 6, GTOP / 2);
  int x1 = 6 + spr.textWidth(sc) + 10;
  spr.setFont(FS); spr.setTextColor(C(0x9AA7B4));
  String rt = "Best " + String(best) + "  [Exit]";
  spr.setTextDatum(D_MR); spr.drawString(rt, W - 6, GTOP / 2);
  int x2 = W - 6 - spr.textWidth(rt) - 8;
  String lv = "Lv " + String(level);
  if (spr.textWidth(lv) <= x2 - x1) { spr.setTextDatum(D_ML); spr.drawString(lv, x1, GTOP / 2); }
  if (levelMsgT && millis() - levelMsgT < 1800 && gs == G_PLAY) {
    spr.setFont(FL); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_MC);
    spr.drawString("Level " + String(level) + "!", W / 2, (H + GTOP) / 2 - 40);
  }
  if (gs != G_PLAY) {
    static uint32_t stickT = 0; static String stickS;
    if (millis() - stickT > 300) { stickT = millis(); stickS = "Stick X " + String(joyRawX()) + "  Y " + String(joyRawY()); }
    int bw = W - 16, bh = 170, bx = 8, by = (H - bh) / 2;
    spr.fillRoundRect(bx, by, bw, bh, 12, C(0x000000));
    spr.drawRoundRect(bx, by, bw, bh, 12, C(0xFFFFFF));
    spr.setTextDatum(D_MC); spr.setTextColor(C(0xFFFFFF));
    spr.setFont(FL);
    spr.drawString(gs == G_PAUSE ? "PAUSED" : "PIXEL SWIM", W / 2, by + 22);
    auto line = [&](const String& t, int y, uint32_t col) { spr.setFont(FS); spr.setTextColor(C(col)); spr.setTextDatum(D_MC); spr.drawString(fitText(FS, t, bw - 12), W / 2, y); };
    line("Eat the dot to grow.", by + 50, 0xFFFFFF);
    line("Grow big = next level.", by + 72, 0xFFFFFF);
    line(gs == G_PAUSE ? "Press joystick to go on" : "Press joystick to start", by + 96, 0xFFE9A8);
    line(stickS, by + 122, 0x9AA7B4);
    line(W > H ? "Wrong way? Settings > Joystick" : "Wrong way? See Settings", by + 144, 0x9AA7B4);
  }
  spr.pushSprite(0, 0);
}
// called from loop() while the game screen is open
void gameLoop() {
  if (millis() - lastFrame < 33) return;   // about 30 frames per second
  lastFrame = millis();
  lastTouchMs = millis();                  // keep the screen on while playing
  if (joyPressed()) {
    if (gs == G_READY) { joyCenter(); gs = G_PLAY; }
    else if (gs == G_PLAY) gs = G_PAUSE;
    else if (gs == G_PAUSE) gs = G_PLAY;
    else if (gs == G_OVER) { gameReset(); joyCenter(); gs = G_PLAY; }
  }
  if (gs == G_PLAY) gameStep();
  gameDraw();
}
void gameTap(int x, int y) {
  if (y < GTOP + 10) {   // tap the top bar = exit
    if (best > prefs.getInt("best", 0)) prefs.putInt("best", best);
    gs = G_READY; scr = S_GAMES; dirty = true;
  }
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
    joyMsg("Done!", "The stick direction is saved.", "Try it in Apps > Games.");
  } else {
    joyMsg("Not saved", "No stick movement found.", "Check the wires (VRX IO2, VRY IO3).");
  }
  delay(2000);
  lcd.setRotation(rot);
  tDown = false; tHandled = true; lastTouchMs = millis(); pw = P_ON;
  dirty = true;
}
