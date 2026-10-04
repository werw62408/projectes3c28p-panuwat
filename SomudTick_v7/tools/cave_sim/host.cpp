// Cave Swarm on a computer: a bot plays the whole run and saves screenshots.
//   ./build.sh && ./cave_host [tall|wide] [seed]
// This does not run on the board. It only checks the game logic and the look.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using std::min; using std::max;
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define CAVE_HOST
#define D_ML lgfx::textdatum::middle_left
#define D_MR lgfx::textdatum::middle_right
#define D_MC lgfx::textdatum::middle_center

static uint32_t simMs = 0;
uint32_t millis() { return simMs; }
LGFX_Sprite spr;
int W = 240, H = 320;
const lgfx::IFont* FS = &fonts::FreeSans9pt7b;
const lgfx::IFont* FB = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* FL = &fonts::FreeSansBold12pt7b;
static inline uint16_t C(uint32_t c) { return lgfx::color565((c >> 16) & 255, (c >> 8) & 255, c & 255); }
// same as SomudTick.ino
static uint32_t blend(uint32_t a, uint32_t b, float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  auto ch = [&](int s) { return (uint32_t)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) & 255; };
  return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}
struct { int getInt(const char*, int d) { return d; } void putInt(const char*, int) {} } prefs;
int ledFlashes = 0, dmgContact = 0, dmgShot = 0;
void ledFlash(uint32_t, uint32_t) { ledFlashes++; }
enum Screen { S_GAMES, S_CAVE };
Screen scr = S_CAVE; bool dirty = false;

#include "../../SomudTick/app_cave.h"

void savePng(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6 %d %d 255\n", W, H);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    uint16_t c = spr.readPixel(x, y);
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
// simple player: run from bugs and acid, go for crystals and the pod, dash through bullets
void bot() {
  memset(&csIn, 0, sizeof csIn);
  float fx = 0, fy = 0;
  for (int i = 0; i < CS_NE; i++) if (csE[i].on) {
    float dx = csX - csE[i].x, dy = csY - csE[i].y, d2 = dx * dx + dy * dy + 1;
    float w = (csE[i].type == E_BOSS ? 9000 : 1400) / d2; fx += dx * w / sqrtf(d2); fy += dy * w / sqrtf(d2);
  }
  bool danger = false;
  for (int i = 0; i < CS_NS; i++) if (csS[i].on) {
    float dx = csX - csS[i].x, dy = csY - csS[i].y, d2 = dx * dx + dy * dy + 1;
    if (d2 < 22 * 22) danger = true;
    float w = 500 / d2; fx += dx * w / sqrtf(d2); fy += dy * w / sqrtf(d2);
  }
  float tx = CS_WORLD_W / 2, ty = CS_WORLD_H / 2, best = 1e9;
  for (int i = 0; i < CS_NG; i++) if (csG[i].on) { float d = hypotf(csG[i].x - csX, csG[i].y - csY); if (d < best) { best = d; tx = csG[i].x; ty = csG[i].y; } }
  if (csPodOn) { tx = csPodX; ty = csPodY; }
  float gx = tx - csX, gy = ty - csY, gd = hypotf(gx, gy) + 1;
  float pull = csPodOn ? 2.5f : 0.6f;
  fx += gx / gd * pull; fy += gy / gd * pull;
  // pick the free direction closest to where we want to go (don't walk into walls)
  float m = hypotf(fx, fy);
  if (m < 0.05f) return;
  float bestS = -1e9, bx = 0, by = 0;
  for (int k = 0; k < 16; k++) {
    float a = k * 6.2832f / 16, dx = cosf(a), dy = sinf(a);
    float sc = (dx * fx + dy * fy) / m;
    for (int st = 1; st <= 3; st++) if (csRockXY(csX + dx * 10 * st, csY + dy * 10 * st)) { sc -= 1.2f / st; break; }
    if (sc > bestS) { bestS = sc; bx = dx; by = dy; }
  }
  csIn.jx = bx; csIn.jy = by;
  csIn.dash = danger;
}
int main(int argc, char** argv) {
  bool wide = argc > 1 && !strcmp(argv[1], "wide");
  if (wide) { W = 320; H = 240; }
  csSeed = argc > 2 ? atoi(argv[2]) : 12345;
  spr.setColorDepth(16); spr.createSprite(W, H);
  csAlloc(); caveNewRun(); csState = CS_TITLE;
  const char* pre = wide ? "wide" : "tall";
  char fn[64];
  caveDraw(); snprintf(fn, sizeof fn, "%s_00_title.ppm", pre); savePng(fn);
  csState = CS_PLAY;
  int shotAt[] = {20, 125, 200, 306, 330, 575};
  const char* shotName[] = {"01_early", "02_swarm", "03_mid", "04_boss", "05_boss2", "06_pod"};
  int nextShot = 0, levelups = 0, maxEnemies = 0, maxShots = 0;
  bool savedLevel = false, bossSeen = false;
  long frames = 0;
  while (csState != CS_OVER && frames < CS_FPS * 700) {
    simMs += 33; frames++;
    if (csState == CS_LEVELUP) {
      if (!savedLevel && levelups >= 2) { caveDraw(); snprintf(fn, sizeof fn, "%s_07_levelup.ppm", pre); savePng(fn); savedLevel = true; }
      memset(&csIn, 0, sizeof csIn); simMs += 400;
      // like a player would: level up guns you have, then hearts/armor, then new guns
      auto score = [](uint8_t u) { if (u >= U_N) return 0; int sc = csUp[u] ? 30 - csUp[u] : 10;
        if (u == U_HEART || u == U_ARMOR) sc += 8; if (u == U_DRILLS || u == U_ZAP) sc += 4; return sc; };
      int pick = 0; for (int i = 1; i < csNChoice; i++) if (score(csChoice[i]) > score(csChoice[pick])) pick = i;
      csSel = pick; csIn.dash = true; caveUpdate(); levelups++; continue;
    }
    bot();
    int hp0 = csHp; bool near = false;
    for (int i = 0; i < CS_NE; i++) if (csE[i].on && hypotf(csE[i].x - csX, csE[i].y - csY) < csEnemyR(csE[i].type) + 8) near = true;
    caveUpdate();
    if (csHp < hp0) { if (near) dmgContact += hp0 - csHp; else dmgShot += hp0 - csHp; }
    if (csFrame % (CS_FPS * 30) == 0) { int n = 0; for (int i = 0; i < CS_NE; i++) n += csE[i].on; printf("  t=%d:%02d lv=%d hp=%d alive=%d\n", csT() / 60, csT() % 60, csLvl, csHp, n); }
    int ne = 0, ns = 0; for (int i = 0; i < CS_NE; i++) ne += csE[i].on; for (int i = 0; i < CS_NS; i++) ns += csS[i].on;
    maxEnemies = max(maxEnemies, ne); maxShots = max(maxShots, ns);
    if (csBossIdx >= 0) bossSeen = true;
    if (nextShot < 6 && csT() >= shotAt[nextShot]) { caveDraw(); snprintf(fn, sizeof fn, "%s_%s.ppm", pre, shotName[nextShot]); savePng(fn); nextShot++; }
  }
  caveDraw(); snprintf(fn, sizeof fn, "%s_08_end.ppm", pre); savePng(fn);
  printf("result=%s time=%d:%02d level=%d kills=%d gold=%d hp=%d/%d levelups=%d maxEnemies=%d maxEnemyShots=%d boss=%s bossDone=%d\n",
         csWon ? "ESCAPED" : csHp <= 0 ? "DIED" : "LEFT_BEHIND", csT() / 60, csT() % 60, csLvl, csKills, csGold, csHp, csMaxHp,
         levelups, maxEnemies, maxShots, bossSeen ? "yes" : "no", csBossDone);
  printf("damage taken: contact=%d shots=%d\n", dmgContact, dmgShot);
  printf("upgrades:"); for (int i = 0; i < U_N; i++) printf(" %s=%d", CS_UPG_NAME[i], csUp[i]); printf("\n");
}
