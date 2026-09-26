#pragma once
// ============================================================================
//  Game: "Dragon" (v11.6, was "Pixel Swim"). A small black-and-white dragon flies through a night sky full of
//  star pixels. Push the joystick to fly (the stars move out of the way), press it to dash (a short, fast rush).
//  Crash into the glowing coloured dots -> you get bigger. Big enough = next level (you start small again).
//  Hold the press 1 s = back to Games. [Pause] / [Exit] on the top bar.
//  Joystick wiring: VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (!), GND -> GND
//  Included from SomudTick.ino.
// ============================================================================
// (joystick reading is in joystick.h)
#define HAS_DRAGON 1           // (the simulator tests look for it)
const int NP = 2000;           // star pixels (more than Pixel Swim had)
struct Pix { float x, y, vx, vy; int16_t hx, hy; };
Pix* pix = nullptr;
const int NFOOD = 3;           // glowing dots to crash into
struct Food { float x, y; uint32_t col; };
Food food[NFOOD];
float px_, py_, pvx, pvy, pr;  // the dragon's head: position, speed, size (grows from PR0 to PR_MAX)
int score = 0, best = 0, level = 1;
uint32_t levelMsgT = 0;
enum GState { G_READY, G_PLAY, G_PAUSE, G_OVER };
GState gs = G_READY;
uint32_t lastFrame = 0;
const int GTOP = 24;            // score bar height
const float PR0 = 5;            // start size
const float PR_MAX = 34;        // this big = next level
// the body follows the path the head flew: a ring of past head positions, one every few pixels
const int TRAIL = 80, NSEG = 26;
float trX[TRAIL], trY[TRAIL]; int trHead = 0;
float dAng = 0;                 // where the head looks (radians)
uint32_t dashUntil = 0, dashReadyAt = 0;   // dash: a short rush, then a short rest
const uint32_t DASH_MS = 380, DASH_REST_MS = 900;
const uint32_t SKY = 0x04060D;  // night sky

float gRand(float a, float b) { return a + (b - a) * (random(10000) / 10000.0f); }
float dragonR() { return 3.0f + (pr - PR0) * 0.36f; }   // head size in pixels (3 .. about 13)
void placeFood(Food& f) {
  const uint32_t cols[] = {0xFFD23F, 0xFF9CEE, 0x6BE3FF, 0xFF6B5B, 0xB4FF6B};
  for (int t = 0; t < 20; t++) {
    f.x = gRand(10, W - 10); f.y = gRand(GTOP + 10, H - 10);
    if (hypotf(f.x - px_, f.y - py_) > dragonR() * 4 + 30) break;
  }
  f.col = cols[random(5)];
}
void trailReset() {   // a straight dragon looking right: point n behind the head is n steps to the left
  float st = max(1.0f, dragonR() * 0.25f);
  for (int i = 0; i < TRAIL; i++) { trX[i] = px_ - ((TRAIL - i) % TRAIL) * st; trY[i] = py_; }
  trHead = 0; dAng = 0;
}
void gameLevel(bool first) {
  pr = PR0; px_ = W / 2; py_ = (H + GTOP) / 2; pvx = pvy = 0;
  trailReset();
  for (auto& f : food) placeFood(f);
  if (!first) levelMsgT = millis();
}
void gameReset() {
  if (!pix) pix = (Pix*)heap_caps_malloc(sizeof(Pix) * NP, MALLOC_CAP_SPIRAM);
  for (int i = 0; i < NP; i++) {   // stars spread over the sky (a loose grid + a little randomness)
    int cols = W > H ? 54 : 42, c = i % cols, r = i / cols, rows = NP / cols;
    float gx = (float)W / cols, gy = (float)(H - GTOP) / rows;
    pix[i].hx = (int16_t)(c * gx + gRand(0, gx)); pix[i].hy = (int16_t)(GTOP + r * gy + gRand(0, gy));
    pix[i].x = pix[i].hx; pix[i].y = pix[i].hy; pix[i].vx = pix[i].vy = 0;
  }
  score = 0; level = 1; levelMsgT = 0; dashUntil = dashReadyAt = 0;
  gameLevel(true);
}
void grow() { pr += 1.2f; }   // each dot makes you a bit bigger
JoyBtn gBtn;   // the stick's button: short press = start / dash / go on, hold 1 s = leave
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
bool dashing() { return (int32_t)(dashUntil - millis()) > 0; }
void dragonDash() {   // press = a short rush in the direction the dragon looks
  if ((int32_t)(millis() - dashReadyAt) < 0) return;   // still resting from the last one
  dashUntil = millis() + DASH_MS; dashReadyAt = dashUntil + DASH_REST_MS;
  float sp = 4.5f; pvx += cosf(dAng) * sp; pvy += sinf(dAng) * sp;
  ledFlash(0xFFFFFF, 60);
}
// a point on the body: k = 0 the neck .. NSEG-1 the tail tip (follows the flown path)
void bodyPt(int k, float& x, float& y) {
  int i = (trHead - (k + 1) * 2 + TRAIL * 4) % TRAIL;   // every 2nd path point (about half a head apart)
  x = trX[i]; y = trY[i];
}
void gameStep() {
  float jx, jy; joyVec(jx, jy);   // follows the screen direction
  if (fabsf(jx) + fabsf(jy) > 0.15f) lastTouchMs = millis();   // flying = using the board (keeps the screen on)
  float speed = 3.4f * powf(PR0 / pr, 0.30f);   // bigger = a bit slower
  bool dash = dashing();
  float damp = dash ? 0.93f : 0.80f;
  pvx = pvx * damp + jx * speed * 0.40f;
  pvy = pvy * damp + jy * speed * 0.40f;
  float hr = dragonR();
  px_ = constrain(px_ + pvx, hr + 2, W - hr - 2);
  py_ = constrain(py_ + pvy, GTOP + hr + 2, H - hr - 2);
  // the head turns smoothly toward where it flies
  float v = hypotf(pvx, pvy);
  if (v > 0.25f) { float want = atan2f(pvy, pvx), d = want - dAng; while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI; dAng += d * 0.3f; }
  // the path for the body: a new point every so many pixels
  float step = max(1.0f, hr * 0.25f);
  if (hypotf(px_ - trX[trHead], py_ - trY[trHead]) >= step) { trHead = (trHead + 1) % TRAIL; trX[trHead] = px_; trY[trHead] = py_; }
  // stars: pushed away by the dragon (head and body), then they slowly float back home
  float hk = dash ? 3.4f : 2.2f;
  float bx[4], by[4]; for (int s = 0; s < 4; s++) bodyPt(3 + s * 6, bx[s], by[s]);
  for (int i = 0; i < NP; i++) {
    Pix& p = pix[i];
    auto push = [&](float cx, float cy, float R, float k) {
      float dx = p.x - cx, dy = p.y - cy, d2 = dx * dx + dy * dy;
      if (d2 < R * R && d2 > 0.01f) { float d = sqrtf(d2), f = (R - d) / R * k; p.vx += dx / d * f; p.vy += dy / d * f; }
    };
    push(px_, py_, hr * 2.2f + 14, hk);
    for (int s = 0; s < 4; s++) push(bx[s], by[s], hr * 1.2f + 6, 1.0f);
    p.vx += (p.hx - p.x) * 0.02f; p.vy += (p.hy - p.y) * 0.02f;
    p.vx *= 0.86f; p.vy *= 0.86f;
    p.x += p.vx; p.y += p.vy;
  }
  // crash into a dot
  for (auto& f : food) if (hypotf(px_ - f.x, py_ - f.y) < hr + 6) {
    score++; grow(); ledFlash(f.col, 100); placeFood(f);
    if (score > best) {   // new best: keep it even if the board is switched off mid-game (at most every 10 s)
      best = score;
      static uint32_t savedT = 0;
      if (millis() - savedT > 10000) { savedT = millis(); prefs.putInt("best", best); }
    }
  }
  if (pr >= PR_MAX) { level++; score += 5; if (score > best) best = score; prefs.putInt("best", best); gameLevel(false); }   // big enough -> next level
}
// ---- the dragon, drawn with simple shapes: white body, black ink lines (like a pencil sketch) ----
const uint32_t INKD = 0x151515, BONE = 0xF2F2EE, SHADE = 0xB0B0AA;
// a bat wing: root (rx, ry) on the shoulder, n = out to the side, f = forward, span = length, lift 0..1 (folded..open)
void dragonWing(float rx, float ry, float nx, float ny, float fx, float fy, float span, float lift, uint32_t skin) {
  auto P = [&](float out, float fwd, float& x, float& y) { x = rx + nx * span * out * lift + fx * span * fwd; y = ry + ny * span * out * lift + fy * span * fwd; };
  float ex, ey, tx, ty, ax, ay, bx, by, cx, cy;
  P(0.42f, 0.22f, ex, ey);    // elbow
  P(1.00f, 0.05f, tx, ty);    // tip of the long finger
  P(0.78f, -0.42f, ax, ay);   // second finger
  P(0.40f, -0.70f, bx, by);   // third finger
  cx = rx - fx * span * 0.55f; cy = ry - fy * span * 0.55f;   // where the skin meets the body
  // skin between the fingers, each edge pulled in a little (the scalloped edge of a bat wing)
  auto skinTri = [&](float x0, float y0, float x1, float y1, float px, float py) {
    float mx = (x0 + x1) / 2 * 0.75f + px * 0.25f, my = (y0 + y1) / 2 * 0.75f + py * 0.25f;
    spr.fillTriangle(px, py, x0, y0, mx, my, C(skin)); spr.fillTriangle(px, py, mx, my, x1, y1, C(skin));
    spr.drawLine(x0, y0, mx, my, C(INKD)); spr.drawLine(mx, my, x1, y1, C(INKD));
  };
  skinTri(tx, ty, ax, ay, ex, ey);
  skinTri(ax, ay, bx, by, ex, ey);
  spr.fillTriangle(rx, ry, ex, ey, bx, by, C(skin));
  skinTri(bx, by, cx, cy, rx, ry);
  float bw = max(0.8f, span * 0.035f);
  wideLine(rx, ry, ex, ey, bw * 1.4f, C(INKD));   // the arm
  wideLine(ex, ey, tx, ty, bw, C(INKD));          // three finger bones
  spr.drawLine(ex, ey, ax, ay, C(INKD)); spr.drawLine(ex, ey, bx, by, C(INKD));
  spr.fillTriangle(ex, ey, ex + nx * span * 0.12f * lift + fx * span * 0.1f, ey + ny * span * 0.12f * lift + fy * span * 0.1f, ex + fx * span * 0.05f, ey + fy * span * 0.05f, C(INKD));   // the claw on the elbow
}
void dragonDraw() {
  float hr = dragonR(), t = millis() / 1000.0f;
  float fx = cosf(dAng), fy = sinf(dAng);                         // the head looks this way
  float nx = -fy, ny = fx; if (ny > 0) { nx = -nx; ny = -ny; }    // the head's "up" side (horns)
  // the body: points along the flown path, head first
  float X[NSEG + 1], Y[NSEG + 1]; X[0] = px_; Y[0] = py_;
  for (int k = 0; k < NSEG; k++) bodyPt(k, X[k + 1], Y[k + 1]);
  auto rad = [&](int k) { return hr * (0.78f - 0.58f * k / NSEG); };   // thick at the neck, thin at the tail
  // wings behind the body, at the shoulders (body point 3)
  int sh = 4;
  float bfx = X[sh - 2] - X[sh + 2], bfy = Y[sh - 2] - Y[sh + 2], bl = hypotf(bfx, bfy);
  if (bl < 0.1f) { bfx = fx; bfy = fy; bl = 1; }
  bfx /= bl; bfy /= bl;
  float wnx = -bfy, wny = bfx;
  float flap = 0.5f + 0.5f * sinf(t * (dashing() ? 20 : 10));
  float span = hr * 5.4f;
  dragonWing(X[sh], Y[sh], wnx, wny, bfx, bfy, span * 0.85f, 0.35f + 0.65f * (1 - flap), SHADE);   // far wing (in the shadow)
  // body: one smooth tube, a dark outline first, the white body on top
  for (int k = NSEG; k >= 1; --k) wideLine(X[k - 1], Y[k - 1], X[k], Y[k], rad(k) + 1.2f, C(INKD));
  for (int k = NSEG; k >= 1; --k) wideLine(X[k - 1], Y[k - 1], X[k], Y[k], rad(k), C(BONE));
  for (int k = 2; k < NSEG; k++) {   // belly scales across the body, spikes along the back
    float dx = X[k - 1] - X[k + 1], dy = Y[k - 1] - Y[k + 1], l = hypotf(dx, dy); if (l < 0.1f) continue;
    dx /= l; dy /= l; float px = -dy, py = dx;
    float r = rad(k);
    if (k % 2 == 0) spr.drawLine(X[k] + px * r * 0.85f, Y[k] + py * r * 0.85f, X[k] - px * r * 0.85f, Y[k] - py * r * 0.85f, C(SHADE));
    if (k % 3 == 0 && r > 1.2f) {
      float sx = (py > 0) ? -px : px, sy = (py > 0) ? -py : py;   // the spikes stand on the upper side
      float ax = X[k] + sx * r * 0.8f + dx * r * 0.5f, ay = Y[k] + sy * r * 0.8f + dy * r * 0.5f, bx = X[k] + sx * r * 0.8f - dx * r * 0.7f, by = Y[k] + sy * r * 0.8f - dy * r * 0.7f;
      float cx = X[k] + sx * (r + hr * 0.5f) - dx * r * 1.0f, cy = Y[k] + sy * (r + hr * 0.5f) - dy * r * 1.0f;
      spr.fillTriangle(ax, ay, bx, by, cx, cy, C(BONE)); spr.drawLine(ax, ay, cx, cy, C(INKD)); spr.drawLine(bx, by, cx, cy, C(INKD));
    }
  }
  { int k = NSEG; float dx = X[k - 1] - X[k], dy = Y[k - 1] - Y[k], l = hypotf(dx, dy);   // the tail ends in a spade
    if (l > 0.1f) { dx /= l; dy /= l; float s = hr * 0.55f;
      spr.fillTriangle(X[k] - dx * s * 1.6f, Y[k] - dy * s * 1.6f, X[k] - dy * s * 0.7f, Y[k] + dx * s * 0.7f, X[k] + dy * s * 0.7f, Y[k] - dx * s * 0.7f, C(BONE));
      spr.drawLine(X[k] - dx * s * 1.6f, Y[k] - dy * s * 1.6f, X[k] - dy * s * 0.7f, Y[k] + dx * s * 0.7f, C(INKD));
      spr.drawLine(X[k] - dx * s * 1.6f, Y[k] - dy * s * 1.6f, X[k] + dy * s * 0.7f, Y[k] - dx * s * 0.7f, C(INKD)); } }
  dragonWing(X[sh], Y[sh], -wnx, -wny, bfx, bfy, span, 0.35f + 0.65f * flap, BONE);   // near wing, over the body
  // the head: skull, long snout, open jaw with teeth, two horns bending back, an angry eye
  float hx = px_, hy = py_, H1 = hr * 1.3f;
  float snX = hx + fx * H1 * 2.1f + nx * H1 * 0.15f, snY = hy + fy * H1 * 2.1f + ny * H1 * 0.15f;   // nose tip
  float jaw = 0.45f + 0.3f * sinf(t * 6);                                                           // the mouth opens and closes
  float jwX = hx + fx * H1 * 1.8f - nx * H1 * (0.55f + jaw), jwY = hy + fy * H1 * 1.8f - ny * H1 * (0.55f + jaw);
  spr.fillCircle(hx, hy, H1 + 1, C(INKD));
  spr.fillTriangle(hx + nx * (H1 + 1), hy + ny * (H1 + 1), hx - nx * H1 * 0.2f, hy - ny * H1 * 0.2f, snX + fx, snY + fy, C(INKD));
  spr.fillTriangle(hx - nx * (H1 * 0.2f), hy - ny * (H1 * 0.2f), hx - nx * (H1 + 1), hy - ny * (H1 + 1), jwX + fx, jwY + fy, C(INKD));
  spr.fillCircle(hx, hy, H1, C(BONE));
  spr.fillTriangle(hx + nx * H1 * 0.9f, hy + ny * H1 * 0.9f, hx - nx * H1 * 0.1f, hy - ny * H1 * 0.1f, snX, snY, C(BONE));   // upper jaw
  spr.fillTriangle(hx - nx * H1 * 0.25f, hy - ny * H1 * 0.25f, hx - nx * H1 * 0.9f, hy - ny * H1 * 0.9f, jwX, jwY, C(BONE));   // lower jaw
  spr.fillTriangle(hx + fx * H1 * 0.6f - nx * H1 * 0.15f, hy + fy * H1 * 0.6f - ny * H1 * 0.15f, snX - nx * H1 * 0.25f, snY - ny * H1 * 0.25f, jwX + nx * H1 * 0.1f, jwY + ny * H1 * 0.1f, C(INKD));   // inside of the mouth
  if (H1 >= 4.5f) for (int k = 1; k <= 3; k++) {   // teeth, top and bottom
    float q = 0.75f + k * 0.35f, tx = hx + fx * H1 * q - nx * H1 * 0.12f, ty = hy + fy * H1 * q - ny * H1 * 0.12f;
    spr.fillTriangle(tx, ty, tx + fx * H1 * 0.14f, ty + fy * H1 * 0.14f, tx - nx * H1 * 0.28f, ty - ny * H1 * 0.28f, C(BONE));
  }
  for (int k = 0; k < 2; k++) {   // horns
    float bx = hx + nx * H1 * 0.75f - fx * H1 * (0.1f + k * 0.45f), by = hy + ny * H1 * 0.75f - fy * H1 * (0.1f + k * 0.45f);
    float mx = bx + nx * H1 * 0.55f - fx * H1 * 0.7f, my = by + ny * H1 * 0.55f - fy * H1 * 0.7f;
    float ex = mx + nx * H1 * 0.15f - fx * H1 * 0.75f, ey = my + ny * H1 * 0.15f - fy * H1 * 0.75f;
    float w = max(0.7f, H1 * (k ? 0.13f : 0.17f));
    wideLine(bx, by, mx, my, w + 0.8f, C(INKD)); wideLine(mx, my, ex, ey, w * 0.7f + 0.8f, C(INKD));
    wideLine(bx, by, mx, my, w, C(BONE)); wideLine(mx, my, ex, ey, w * 0.7f, C(BONE));
  }
  float ex = hx + fx * H1 * 0.55f + nx * H1 * 0.35f, ey = hy + fy * H1 * 0.55f + ny * H1 * 0.35f;
  spr.fillCircle(ex, ey, max(1.0f, H1 * 0.18f), C(INKD));   // eye
  wideLine(ex - fx * H1 * 0.45f + nx * H1 * 0.4f, ey - fy * H1 * 0.45f + ny * H1 * 0.4f, ex + fx * H1 * 0.4f + nx * H1 * 0.12f, ey + fy * H1 * 0.4f + ny * H1 * 0.12f, max(0.5f, H1 * 0.07f), C(INKD));   // angry brow
  if (H1 >= 5) for (int k = 0; k < 2; k++) {   // a few scale lines on the neck
    float q = 0.9f + k * 0.35f; spr.drawLine(hx - fx * H1 * q + nx * H1 * 0.5f, hy - fy * H1 * q + ny * H1 * 0.5f, hx - fx * H1 * q - nx * H1 * 0.5f, hy - fy * H1 * q - ny * H1 * 0.5f, C(SHADE));
  }
}
void gameDraw() {
  spr.fillScreen(C(SKY));
  for (int i = 0; i < NP; i++) {   // stars: pushed ones light up white
    Pix& p = pix[i];
    float moved = fabsf(p.x - p.hx) + fabsf(p.y - p.hy);
    uint8_t h = (uint8_t)(((uint32_t)i * 2654435761u) >> 24);   // a fixed "random" brightness per star (no stripes)
    bool big = h < 26;
    uint32_t base = big ? 0x7A86A8 : h < 100 ? 0x3C4866 : 0x232C44;   // a few bright stars, most dim
    uint32_t c = blend(base, 0xFFFFFF, min(1.0f, moved / 16.0f));
    spr.fillRect((int)p.x, (int)p.y, big ? 2 : 1, big ? 2 : 1, C(c));
  }
  for (auto& f : food) {   // glowing dots, gently pulsing
    float g = 7 + 2 * sinf(millis() / 180.0f + f.x);
    spr.fillCircle(f.x, f.y, g, C(blend(SKY, f.col, 0.28f)));
    spr.fillCircle(f.x, f.y, 4, C(f.col));
  }
  if (dashing()) {   // wind lines behind the dragon while it rushes
    float fx = cosf(dAng), fy = sinf(dAng), hr = dragonR();
    for (int k = -1; k <= 1; k++) {
      float ox = -fy * k * hr * 0.9f, oy = fx * k * hr * 0.9f;
      spr.drawLine(px_ + ox - fx * hr * 2, py_ + oy - fy * hr * 2, px_ + ox - fx * hr * (5 + 2 * abs(k)), py_ + oy - fy * hr * (5 + 2 * abs(k)), C(0xAFC4FF));
    }
  }
  dragonDraw();
  // score bar
  spr.fillRect(0, 0, W, GTOP, C(0x000000));
  spr.setFont(FB); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_ML);
  String sc = "Score " + String(score);
  spr.drawString(sc, 6, GTOP / 2);
  int x1 = 6 + spr.textWidth(sc) + 10;
  int x2 = gameBarButtons(GTOP, gs == G_PLAY) - 8;   // [Pause] [Exit]
  spr.setFont(FS); spr.setTextColor(C(0x9AA7B4));
  String lv = "Lv " + String(level);
  if ((int)spr.textWidth(lv) <= x2 - x1) { spr.setTextDatum(D_ML); spr.drawString(lv, x1, GTOP / 2); }
  if (levelMsgT && millis() - levelMsgT < 1800 && gs == G_PLAY) {
    spr.setFont(FL); spr.setTextColor(C(0xFFFFFF)); spr.setTextDatum(D_MC);
    spr.drawString("Level " + String(level) + "!", W / 2, (H + GTOP) / 2 - 40);
  }
  if (gs != G_PLAY) {
    static uint32_t stickT = 0; static String stickS;
    if (millis() - stickT > 300) { stickT = millis(); stickS = "Stick X " + String(joyRawX()) + "  Y " + String(joyRawY()); }
    int bw = W - 16, bh = 178, bx = 8, by = (H - bh) / 2 + 6;
    spr.fillRoundRect(bx, by, bw, bh, 12, C(0x000000));
    spr.drawRoundRect(bx, by, bw, bh, 12, C(0xFFFFFF));
    spr.setTextDatum(D_MC); spr.setTextColor(C(0xFFFFFF));
    spr.setFont(FL);
    spr.drawString(gs == G_PAUSE ? "PAUSED" : "DRAGON", W / 2, by + 22);
    auto line = [&](const String& t, int y, uint32_t col) { spr.setFont(FS); spr.setTextColor(C(col)); spr.setTextDatum(D_MC); spr.drawString(fitText(FS, t, bw - 12), W / 2, y); };
    line("Fly with the stick.", by + 50, 0xFFFFFF);
    line("Crash into dots to grow.", by + 70, 0xFFFFFF);
    line(gs == G_PAUSE ? "Press: go on  Hold: exit" : "Press: start  Hold: exit", by + 94, 0xFFE9A8);
    line("In the game, press: dash", by + 114, 0xFFE9A8);
    line("Best " + String(best), by + 136, 0x9AA7B4);
    line(stickS, by + 156, 0x9AA7B4);
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
    else if (gs == G_PLAY) dragonDash();   // v11.6: a press is a dash; pause = [Pause] on the top bar
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
