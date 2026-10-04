#pragma once
// ============================================================================
//  Game: "Cave Swarm". A miner in a cave full of bugs. Survive for 10 minutes.
//  - Move with the joystick (or drag a finger on the screen). Your guns fire by
//    themselves at the closest bug.
//  - Press the joystick (or tap the screen) = dash. You can't be hit while dashing.
//  - Walk into rock to dig it. Gold ore = score, red crystal = heal.
//  - Bugs drop blue crystals (XP). Level up = pick 1 of 3 upgrades.
//  - 5:00 a big boss comes. 9:00 the drop pod lands: reach it to win.
//  Uses the joystick helpers from app_game.h. Included from SomudTick.ino.
// ============================================================================

// ---------------- sizes ----------------
const int CS_CELL = 16, CS_MW = 48, CS_MH = 48;            // map = 48 x 48 cells
const int CS_WORLD_W = CS_CELL * CS_MW, CS_WORLD_H = CS_CELL * CS_MH;
const int CS_HUD = 22;                                       // top bar height
const int CS_FPS = 30;
const int CS_RUN_S = 600;                                    // 10 minutes
const int CS_BOSS_S = 300;                                   // boss at 5:00
const int CS_POD_CALL_S = 540, CS_POD_LAND_S = 570, CS_POD_STAY_S = 45;
const uint8_t CS_BEDROCK = 255;

const int CS_NE = 70, CS_NB = 100, CS_NS = 170, CS_NG = 120, CS_NP = 90, CS_NZ = 10;

// ---------------- data ----------------
enum CsState { CS_TITLE, CS_PLAY, CS_PAUSE, CS_LEVELUP, CS_OVER };
enum CsEType : uint8_t { E_GRUNT, E_SWARM, E_SPIT, E_TANK, E_BOSS };
enum CsUpg : uint8_t { U_RIFLE, U_SHOTGUN, U_DRILLS, U_GRENADE, U_ZAP,
                       U_ARMOR, U_BOOTS, U_MAGNET, U_OVERCLOCK, U_HEART, U_N,
                       U_HEAL = 100, U_GOLD };
const uint8_t CS_NWEAP = 5;
const char* CS_UPG_NAME[U_N] = {"Rifle", "Shotgun", "Drills", "Grenade", "Arc Zap",
                                "Armor", "Boots", "Magnet", "Overclock", "Big Heart"};
const char* CS_UPG_INFO[U_N] = {"Shoots closest bug", "Pellets, close range", "Spin around you",
                                "Blast, breaks rock", "Jumps bug to bug",
                                "Take less damage", "Move faster", "Grab from further",
                                "Guns fire faster", "+2 HP, heal faster"};
const uint32_t CS_UPG_COL[U_N] = {0xFFD23F, 0xFF8C42, 0x9AD1FF, 0x7CE38B, 0xB28DFF,
                                  0xB0B8C0, 0x7FE0D0, 0x6BB5FF, 0xFF6B9A, 0xFF5050};

struct CsEnemy { float x, y, vx, vy; int16_t hp, maxhp, cd; uint8_t type, hitT, flash; bool on; };
struct CsBullet { float x, y, vx, vy, tx, ty; int16_t life; uint8_t kind, dmg; int8_t pierce; bool on; };   // kind 0 rifle, 1 pellet, 2 grenade
struct CsShot { float x, y, vx, vy; int16_t life; uint8_t kind; bool on; };                                // enemy bullet
struct CsGem { float x, y, vx, vy; uint8_t kind; uint8_t val; bool home, on; };                         // kind 0 xp, 1 gold, 2 heal
struct CsPart { float x, y, vx, vy; int16_t life; uint32_t col; };
struct CsZap { float x1, y1, x2, y2; int16_t life; };

uint8_t* csMap = nullptr;     // 0 = floor, 1..254 = rock hit points, 255 = bedrock
uint8_t* csOre = nullptr;     // 0 none, 1 gold, 2 heal crystal
CsEnemy* csE = nullptr; CsBullet* csB = nullptr; CsShot* csS = nullptr; CsGem* csG = nullptr; CsPart* csP = nullptr;
CsZap csZ[CS_NZ];
int16_t csSpeck[260][2];      // floor dots, just for looks

CsState csState = CS_TITLE;
float csX, csY, csVX, csVY, csFaceX = 1, csFaceY = 0;
int csHp, csMaxHp, csIfr, csDashT, csDashCd, csMineT;
int csLvl, csXp, csXpNeed, csKills, csGold, csFrame, csShake;
uint8_t csUp[U_N];            // level of each upgrade (0 = not owned)
int16_t csWcd[CS_NWEAP];      // weapon cooldowns
uint8_t csChoice[3]; int csNChoice, csSel; uint32_t csMenuT;
int csBossIdx = -1; bool csBossDone, csWon, csPodOn; float csPodX, csPodY;
char csBanner[40]; int csBannerT;
int csBestS = 0, csWins = 0;
bool csJoy = false;           // joystick plugged in?
uint32_t csSeed = 1, csLastFrame = 0;
int csSwarmNext;

// input for one frame
struct CsInput { float jx, jy; bool dash; bool tap; int tx, ty; bool up, down; } csIn;

// ---------------- helpers ----------------
uint32_t csRnd() { csSeed ^= csSeed << 13; csSeed ^= csSeed >> 17; csSeed ^= csSeed << 5; return csSeed; }
float csRf(float a, float b) { return a + (b - a) * ((csRnd() & 0xFFFF) / 65535.0f); }
int csRi(int n) { return n > 0 ? (int)(csRnd() % (uint32_t)n) : 0; }
float csSec() { return csFrame / (float)CS_FPS; }
int csT() { return csFrame / CS_FPS; }
inline uint8_t& csCell(int cx, int cy) { return csMap[cy * CS_MW + cx]; }
bool csRockAt(int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= CS_MW || cy >= CS_MH) return true;
  return csMap[cy * CS_MW + cx] != 0;
}
bool csRockXY(float x, float y) { return csRockAt((int)floorf(x / CS_CELL), (int)floorf(y / CS_CELL)); }
void csText(const lgfx::IFont* f, const char* s, int x, int y, uint32_t col, lgfx::textdatum_t d) {
  spr.setFont(f); spr.setTextColor(C(col)); spr.setTextDatum(d); spr.drawString(s, x, y);
}
void csBannerSet(const char* s, int frames) { strncpy(csBanner, s, sizeof csBanner - 1); csBanner[sizeof csBanner - 1] = 0; csBannerT = frames; }
void csPart(float x, float y, uint32_t col, int n, float sp) {
  for (int k = 0; k < n; k++)
    for (int i = 0; i < CS_NP; i++) if (csP[i].life <= 0) {
      float a = csRf(0, 6.2832f), v = csRf(0.3f, sp);
      csP[i] = {x, y, cosf(a) * v, sinf(a) * v, (int16_t)(csRi(14) + 8), col};
      break;
    }
}

// ---------------- map ----------------
void csMakeMap() {
  // random fill, then smooth a few times = round caves
  for (int y = 0; y < CS_MH; y++) for (int x = 0; x < CS_MW; x++) {
    bool edge = x == 0 || y == 0 || x == CS_MW - 1 || y == CS_MH - 1;
    csCell(x, y) = edge ? CS_BEDROCK : (csRi(100) < 45 ? 4 : 0);
    csOre[y * CS_MW + x] = 0;
  }
  static uint8_t tmp[CS_MW * CS_MH];
  for (int it = 0; it < 4; it++) {
    for (int y = 1; y < CS_MH - 1; y++) for (int x = 1; x < CS_MW - 1; x++) {
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && csCell(x + dx, y + dy)) n++;
      tmp[y * CS_MW + x] = n >= 5 ? 4 : (n <= 2 ? 0 : csCell(x, y) ? 4 : 0);
    }
    for (int y = 1; y < CS_MH - 1; y++) for (int x = 1; x < CS_MW - 1; x++) csCell(x, y) = tmp[y * CS_MW + x];
  }
  // open space in the middle where you start
  int mx = CS_MW / 2, my = CS_MH / 2;
  for (int y = my - 5; y <= my + 5; y++) for (int x = mx - 5; x <= mx + 5; x++)
    if ((x - mx) * (x - mx) + (y - my) * (y - my) <= 26) csCell(x, y) = 0;
  // ore in rock that touches open floor
  for (int y = 1; y < CS_MH - 1; y++) for (int x = 1; x < CS_MW - 1; x++) {
    if (csCell(x, y) != 4) continue;
    bool open = !csCell(x + 1, y) || !csCell(x - 1, y) || !csCell(x, y + 1) || !csCell(x, y - 1);
    int r = csRi(100);
    if (open && r < 7) { csOre[y * CS_MW + x] = 1; csCell(x, y) = 7; }
    else if (open && r < 10) { csOre[y * CS_MW + x] = 2; csCell(x, y) = 7; }
  }
  for (auto& s : csSpeck) { s[0] = (int16_t)csRi(CS_WORLD_W); s[1] = (int16_t)csRi(CS_WORLD_H); }
}
void csDropGem(float x, float y, uint8_t kind, uint8_t val);
// hit a rock cell, returns true if something was there
bool csDig(int cx, int cy, int dmg) {
  if (cx <= 0 || cy <= 0 || cx >= CS_MW - 1 || cy >= CS_MH - 1) return false;
  uint8_t& c = csCell(cx, cy);
  if (!c || c == CS_BEDROCK) return false;
  float x = cx * CS_CELL + CS_CELL / 2, y = cy * CS_CELL + CS_CELL / 2;
  if (c > dmg) { c -= dmg; csPart(x, y, 0x8A6E52, 1, 1.2f); return true; }
  c = 0;
  uint8_t ore = csOre[cy * CS_MW + cx]; csOre[cy * CS_MW + cx] = 0;
  csPart(x, y, ore == 1 ? 0xF2C230 : ore == 2 ? 0xFF4D6A : 0x8A6E52, 6, 2.0f);
  if (ore == 1) csDropGem(x, y, 1, 5);
  if (ore == 2) csDropGem(x, y, 2, 2);
  return true;
}
// does a circle hit rock? (first rock cell found goes to hcx, hcy)
bool csHitsRock(float x, float y, float r, int& hcx, int& hcy) {
  int x0 = (int)floorf((x - r) / CS_CELL), x1 = (int)floorf((x + r) / CS_CELL);
  int y0 = (int)floorf((y - r) / CS_CELL), y1 = (int)floorf((y + r) / CS_CELL);
  for (int cy = y0; cy <= y1; cy++) for (int cx = x0; cx <= x1; cx++) {
    if (!csRockAt(cx, cy)) continue;
    float nx = constrain(x, (float)cx * CS_CELL, (float)cx * CS_CELL + CS_CELL);
    float ny = constrain(y, (float)cy * CS_CELL, (float)cy * CS_CELL + CS_CELL);
    if ((x - nx) * (x - nx) + (y - ny) * (y - ny) < r * r) { hcx = cx; hcy = cy; return true; }
  }
  return false;
}

// ---------------- spawning ----------------
int csSpawn(uint8_t type, float x, float y) {
  for (int i = 0; i < CS_NE; i++) if (!csE[i].on) {
    float hpMul = 1.0f + csSec() / 140.0f;
    int hp = type == E_GRUNT ? 3 : type == E_SWARM ? 2 : type == E_SPIT ? 6 : type == E_TANK ? 40 : 420;
    hp = (int)(hp * (type == E_BOSS ? 1.0f : hpMul));
    csE[i] = {x, y, 0, 0, (int16_t)hp, (int16_t)hp, (int16_t)(40 + csRi(60)), type, 0, 0, true};
    return i;
  }
  return -1;
}
// a point just outside the screen (bugs dig out of the walls)
void csEdgePoint(float& x, float& y) {
  float a = csRf(0, 6.2832f), d = max(W, H) * 0.62f + csRf(0, 30);
  x = constrain(csX + cosf(a) * d, (float)CS_CELL * 1.5f, (float)CS_WORLD_W - CS_CELL * 1.5f);
  y = constrain(csY + sinf(a) * d, (float)CS_CELL * 1.5f, (float)CS_WORLD_H - CS_CELL * 1.5f);
}
int csAlive() { int n = 0; for (int i = 0; i < CS_NE; i++) n += csE[i].on; return n; }
void csSpawnTick() {
  int t = csT();
  int cap = min(CS_NE - 4, 6 + t / 7);
  int every = max(5, 36 - t / 12);
  if (csPodOn) { cap = CS_NE - 2; every = 4; }        // everyone wants a piece of you at the end
  if (csFrame % every == 0 && csAlive() < cap) {
    float x, y; csEdgePoint(x, y);
    int r = csRi(100);
    uint8_t type = E_GRUNT;
    if (t >= 25 && r < 30) type = E_SWARM;
    else if (t >= 55 && r < 30 + min(22, t / 15)) type = E_SPIT;
    if (type == E_SWARM) { for (int k = 0; k < 4 + csRi(4); k++) csSpawn(E_SWARM, x + csRf(-14, 14), y + csRf(-14, 14)); }
    else csSpawn(type, x, y);
  }
  if (t >= 90 && csFrame % (CS_FPS * max(25, 60 - t / 15)) == 0) { float x, y; csEdgePoint(x, y); csSpawn(E_TANK, x, y); }
  if (t == csSwarmNext && csFrame % CS_FPS == 0) {   // big wave
    csBannerSet("SWARM INCOMING!", 70); ledFlash(0xFF6A00, 600);
    for (int k = 0; k < 26; k++) { float x, y; csEdgePoint(x, y); csSpawn(k % 3 ? E_SWARM : E_GRUNT, x, y); }
    csSwarmNext += 120;
  }
  if (t == CS_BOSS_S && csBossIdx < 0 && !csBossDone && csFrame % CS_FPS == 0) {
    float x, y; csEdgePoint(x, y);
    csBossIdx = csSpawn(E_BOSS, x, y);
    if (csBossIdx < 0) { csE[0].on = false; csBossIdx = csSpawn(E_BOSS, x, y); }
    csBannerSet("DREADNOUGHT!", 80); ledFlash(0xB040FF, 1000);
  }
  if (t == CS_POD_CALL_S && csFrame % CS_FPS == 0) csBannerSet("Drop pod in 30 s", 80);
  if (t == CS_POD_LAND_S && !csPodOn && csFrame % CS_FPS == 0) {
    // land somewhere far from you, and clear the rock around it
    for (int tries = 0; tries < 40; tries++) {
      csPodX = csRf(CS_CELL * 4, CS_WORLD_W - CS_CELL * 4); csPodY = csRf(CS_CELL * 4, CS_WORLD_H - CS_CELL * 4);
      if (hypotf(csPodX - csX, csPodY - csY) > 220) break;
    }
    int pcx = csPodX / CS_CELL, pcy = csPodY / CS_CELL;
    for (int y = pcy - 2; y <= pcy + 2; y++) for (int x = pcx - 2; x <= pcx + 2; x++)
      if (x > 0 && y > 0 && x < CS_MW - 1 && y < CS_MH - 1) { csCell(x, y) = 0; csOre[y * CS_MW + x] = 0; }
    csPodOn = true; csShake = 12;
    csBannerSet("Pod is down! Get in!", 80); ledFlash(0x40FF90, 800);
  }
}

// ---------------- pickups ----------------
void csDropGem(float x, float y, uint8_t kind, uint8_t val) {
  for (int i = 0; i < CS_NG; i++) if (!csG[i].on) {
    csG[i] = {x, y, csRf(-0.8f, 0.8f), csRf(-0.8f, 0.8f), kind, val, false, true};
    return;
  }
  // full: add it to the closest one of the same kind
  int best = -1; float bd = 1e9;
  for (int i = 0; i < CS_NG; i++) if (csG[i].kind == kind) { float d = fabsf(csG[i].x - x) + fabsf(csG[i].y - y); if (d < bd) { bd = d; best = i; } }
  if (best >= 0) csG[best].val = min(250, csG[best].val + val);
}
void csLevelUp();
void csGain(CsGem& g) {
  if (g.kind == 0) { csXp += g.val; }
  else if (g.kind == 1) { csGold += g.val; }
  else { csHp = min(csMaxHp, csHp + g.val); csPart(csX, csY, 0xFF6680, 5, 1.5f); }
  g.on = false;
}

// ---------------- upgrades ----------------
int csCooldown(int base) { return max(4, (int)(base * (1.0f - 0.08f * csUp[U_OVERCLOCK]))); }
void csPickChoices() {
  uint8_t pool[U_N]; int n = 0;
  int weaps = 0; for (int i = 0; i < CS_NWEAP; i++) weaps += csUp[i] > 0;
  for (int i = 0; i < U_N; i++) {
    if (csUp[i] >= 5) continue;
    if (i < CS_NWEAP && !csUp[i] && weaps >= 4) continue;     // 4 guns max
    pool[n++] = i;
  }
  csNChoice = 0;
  while (csNChoice < 3 && n > 0) { int k = csRi(n); csChoice[csNChoice++] = pool[k]; pool[k] = pool[--n]; }
  if (csNChoice == 0) { csChoice[0] = U_HEAL; csChoice[1] = U_GOLD; csNChoice = 2; }
  csSel = 0;
}
void csApply(uint8_t u) {
  if (u == U_HEAL) { csHp = csMaxHp; return; }
  if (u == U_GOLD) { csGold += 50; return; }
  csUp[u]++;
  if (u == U_HEART) { csMaxHp += 2; csHp = min(csMaxHp, csHp + 4); }
}
void csLevelUp() {
  csXp -= csXpNeed; csLvl++;
  csXpNeed = 5 + csLvl * 3 + csLvl * csLvl / 4;
  csPickChoices();
  csState = CS_LEVELUP; csMenuT = millis();
  ledFlash(0x40E0FF, 300);
}

// ---------------- weapons ----------------
int csNearest(float range) {
  int best = -1; float bd = range * range;
  for (int i = 0; i < CS_NE; i++) if (csE[i].on) {
    float dx = csE[i].x - csX, dy = csE[i].y - csY, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
void csFire(float ang, float speed, uint8_t kind, uint8_t dmg, int8_t pierce, int life) {
  for (int i = 0; i < CS_NB; i++) if (!csB[i].on) {
    csB[i] = {csX, csY, cosf(ang) * speed, sinf(ang) * speed, 0, 0, (int16_t)life, kind, dmg, pierce, true};
    return;
  }
}
float csEnemyR(uint8_t t) { return t == E_SWARM ? 4 : t == E_TANK ? 11 : t == E_BOSS ? 18 : 6; }
void csKill(int i);
void csHurt(int i, int dmg) {
  CsEnemy& e = csE[i];
  e.hp -= dmg; e.flash = 3;
  if (e.hp <= 0) csKill(i);
}
void csWeapons() {
  // rifle
  if (csUp[U_RIFLE] && --csWcd[U_RIFLE] <= 0) {
    int l = csUp[U_RIFLE], tgt = csNearest(170);
    if (tgt >= 0) {
      float a = atan2f(csE[tgt].y - csY, csE[tgt].x - csX);
      int shots = l >= 5 ? 3 : l >= 3 ? 2 : 1;
      for (int k = 0; k < shots; k++) csFire(a + (k - (shots - 1) / 2.0f) * 0.12f, 6.0f, 0, 3 + l / 2, l >= 4 ? 1 : 0, 34);
      csWcd[U_RIFLE] = csCooldown(15 - l);
    } else csWcd[U_RIFLE] = 3;
  }
  // shotgun
  if (csUp[U_SHOTGUN] && --csWcd[U_SHOTGUN] <= 0) {
    int l = csUp[U_SHOTGUN], tgt = csNearest(100);
    if (tgt >= 0) {
      float a = atan2f(csE[tgt].y - csY, csE[tgt].x - csX);
      int n = 4 + l;
      for (int k = 0; k < n; k++) csFire(a + csRf(-0.38f, 0.38f), csRf(4.5f, 6.0f), 1, 2 + l / 3, 0, 15);
      csWcd[U_SHOTGUN] = csCooldown(48 - l * 3);
    } else csWcd[U_SHOTGUN] = 4;
  }
  // grenade
  if (csUp[U_GRENADE] && --csWcd[U_GRENADE] <= 0) {
    int l = csUp[U_GRENADE], tgt = csNearest(150);
    float tx, ty;
    if (tgt >= 0) { tx = csE[tgt].x; ty = csE[tgt].y; } else { float a = csRf(0, 6.2832f); tx = csX + cosf(a) * 70; ty = csY + sinf(a) * 70; }
    for (int i = 0; i < CS_NB; i++) if (!csB[i].on) {
      csB[i] = {csX, csY, (tx - csX) / 18, (ty - csY) / 18, tx, ty, 18, 2, (uint8_t)(6 + l * 3), 0, true};
      break;
    }
    csWcd[U_GRENADE] = csCooldown(100 - l * 9);
  }
  // arc zap
  if (csUp[U_ZAP] && --csWcd[U_ZAP] <= 0) {
    int l = csUp[U_ZAP], cur = csNearest(120);
    if (cur >= 0) {
      float fx = csX, fy = csY;
      bool hit[CS_NE] = {false};
      for (int j = 0; j < 2 + l && cur >= 0; j++) {
        CsEnemy& e = csE[cur];
        for (auto& z : csZ) if (z.life <= 0) { z = {fx, fy, e.x, e.y, 6}; break; }
        fx = e.x; fy = e.y; hit[cur] = true;
        csHurt(cur, 3 + l);
        int nb = -1; float bd = 70 * 70;
        for (int i = 0; i < CS_NE; i++) if (csE[i].on && !hit[i]) {
          float d = (csE[i].x - fx) * (csE[i].x - fx) + (csE[i].y - fy) * (csE[i].y - fy);
          if (d < bd) { bd = d; nb = i; }
        }
        cur = nb;
      }
      csWcd[U_ZAP] = csCooldown(70 - l * 6);
    } else csWcd[U_ZAP] = 5;
  }
  // drills: spin around you, hurt bugs and dig rock
  if (csUp[U_DRILLS]) {
    int l = csUp[U_DRILLS], n = 1 + l;
    float r = 24 + l * 1.5f, a0 = csFrame * 0.13f;
    for (int k = 0; k < n; k++) {
      float a = a0 + k * 6.2832f / n, dx = csX + cosf(a) * r, dy = csY + sinf(a) * r;
      for (int i = 0; i < CS_NE; i++) {
        CsEnemy& e = csE[i];
        if (!e.on || e.hitT) continue;
        float rr = csEnemyR(e.type) + 4;
        if ((e.x - dx) * (e.x - dx) + (e.y - dy) * (e.y - dy) < rr * rr) {
          e.hitT = 9;
          float pa = atan2f(e.y - csY, e.x - csX); e.vx += cosf(pa) * 1.2f; e.vy += sinf(pa) * 1.2f;
          csHurt(i, 1 + l / 2);
        }
      }
      if (csFrame % 6 == k % 6) csDig((int)floorf(dx / CS_CELL), (int)floorf(dy / CS_CELL), 1);
    }
  }
}
void csExplode(float x, float y, int dmg, float R) {
  for (int i = 0; i < CS_NE; i++) if (csE[i].on && hypotf(csE[i].x - x, csE[i].y - y) < R + csEnemyR(csE[i].type)) csHurt(i, dmg);
  int r = (int)(R / CS_CELL) + 1, cx = x / CS_CELL, cy = y / CS_CELL;
  for (int yy = cy - r; yy <= cy + r; yy++) for (int xx = cx - r; xx <= cx + r; xx++)
    if (hypotf(xx * CS_CELL + 8 - x, yy * CS_CELL + 8 - y) < R) csDig(xx, yy, 20);
  csPart(x, y, 0xFFB040, 14, 3.5f); csPart(x, y, 0xFFF0A0, 6, 2.0f);
  csShake = max(csShake, 5);
}

// ---------------- enemies ----------------
void csEnemyShot(float x, float y, float ang, float sp, uint8_t kind) {
  for (int i = 0; i < CS_NS; i++) if (!csS[i].on) { csS[i] = {x, y, cosf(ang) * sp, sinf(ang) * sp, 220, kind, true}; return; }
}
void csKill(int i) {
  CsEnemy& e = csE[i];
  e.on = false; csKills++;
  uint32_t col = e.type == E_SPIT ? 0x8EE04A : e.type == E_BOSS ? 0xC070FF : 0xE8843A;
  csPart(e.x, e.y, col, e.type == E_BOSS ? 30 : e.type == E_TANK ? 12 : 5, e.type == E_BOSS ? 4.0f : 2.2f);
  uint8_t xp = e.type == E_SWARM ? 1 : e.type == E_GRUNT ? 1 : e.type == E_SPIT ? 2 : e.type == E_TANK ? 8 : 40;
  if (e.type == E_BOSS) {
    for (int k = 0; k < 10; k++) csDropGem(e.x + csRf(-20, 20), e.y + csRf(-20, 20), k < 6 ? 0 : 1, k < 6 ? 8 : 15);
    csDropGem(e.x, e.y, 2, 6);
    csBossIdx = -1; csBossDone = true; csShake = 14;
    csBannerSet("Dreadnought down!", 80); ledFlash(0xFFFFFF, 800);
  } else {
    csDropGem(e.x, e.y, 0, xp);
    if (csRi(100) < 4) csDropGem(e.x, e.y, 2, 2);
  }
}
void csHitPlayer(int dmg) {
  if (csIfr > 0 || csDashT > 0) return;
  if (csRi(100) < csUp[U_ARMOR] * 10) { csIfr = 10; csPart(csX, csY, 0xB0B8C0, 4, 1.5f); return; }   // armor blocked it
  dmg = max(1, dmg - csUp[U_ARMOR] / 3);
  csHp -= dmg; csIfr = 40; csShake = max(csShake, 4);
  csPart(csX, csY, 0xFF3030, 6, 2.0f);
  ledFlash(0xFF0000, 150);
}
void csBossAttack(CsEnemy& e) {
  int c = csFrame % 300;
  if (c == 0 || c == 30 || c == 60) {                           // rings
    float off = c * 0.11f;
    for (int k = 0; k < 18; k++) csEnemyShot(e.x, e.y, off + k * 6.2832f / 18, 1.5f, 1);
  } else if (c >= 110 && c < 190 && c % 4 == 0) {               // spiral
    float a = c * 0.23f;
    for (int k = 0; k < 3; k++) csEnemyShot(e.x, e.y, a + k * 2.094f, 1.9f, 1);
  } else if (c == 230 || c == 245 || c == 260) {                // aimed fan
    float a = atan2f(csY - e.y, csX - e.x);
    for (int k = -3; k <= 3; k++) csEnemyShot(e.x, e.y, a + k * 0.16f, 2.3f, 2);
  }
}
void csEnemies() {
  for (int i = 0; i < CS_NE; i++) {
    CsEnemy& e = csE[i];
    if (!e.on) continue;
    if (e.hitT) e.hitT--;
    if (e.flash) e.flash--;
    float dx = csX - e.x, dy = csY - e.y, d = sqrtf(dx * dx + dy * dy) + 0.001f;
    float sp = e.type == E_SWARM ? 1.55f : e.type == E_GRUNT ? 0.95f : e.type == E_SPIT ? 0.75f : e.type == E_TANK ? 0.5f : 0.42f;
    sp *= 1.0f + min(0.35f, csSec() / 1200.0f);
    float mx = dx / d, my = dy / d;
    if (e.type == E_SWARM) { float w = sinf(csFrame * 0.2f + i) * 0.6f; float t = mx; mx -= my * w; my += t * w; }
    if (e.type == E_SPIT && d < 95) { mx = -mx * 0.6f; my = -my * 0.6f; }   // spitters keep their distance
    if (csRockXY(e.x, e.y)) sp *= 0.55f;                                   // digging through rock is slow
    e.vx = e.vx * 0.82f + mx * sp * 0.18f;
    e.vy = e.vy * 0.82f + my * sp * 0.18f;
    e.x = constrain(e.x + e.vx, (float)CS_CELL, (float)CS_WORLD_W - CS_CELL);
    e.y = constrain(e.y + e.vy, (float)CS_CELL, (float)CS_WORLD_H - CS_CELL);
    float er = csEnemyR(e.type);
    if (d < er + 5) {
      bool hurt = csIfr <= 0 && csDashT <= 0;
      csHitPlayer(e.type == E_BOSS ? 3 : e.type == E_TANK ? 2 : 1);
      if (hurt && e.type != E_BOSS) { e.vx -= mx * 4; e.vy -= my * 4; }      // the bug bounces off you
    }
    if (--e.cd <= 0) {
      if (e.type == E_SPIT && d < 190) {
        float a = atan2f(dy, dx);
        for (int k = -1; k <= 1; k++) csEnemyShot(e.x, e.y, a + k * 0.22f, 1.7f, 0);
        e.cd = 80 + csRi(30);
      } else if (e.type == E_TANK && d < 160) {
        float a = atan2f(dy, dx);
        for (int k = -2; k <= 2; k++) csEnemyShot(e.x, e.y, a + k * 0.18f, 1.4f, 2);
        e.cd = 120 + csRi(40);
      } else e.cd = 30;
    }
    if (e.type == E_BOSS) csBossAttack(e);
  }
  // push bugs apart so they don't stack into one blob
  for (int i = 0; i < CS_NE; i++) {
    if (!csE[i].on) continue;
    for (int j = i + 1; j < CS_NE; j++) {
      if (!csE[j].on) continue;
      float dx = csE[j].x - csE[i].x, dy = csE[j].y - csE[i].y;
      float r = csEnemyR(csE[i].type) + csEnemyR(csE[j].type);
      float d2 = dx * dx + dy * dy;
      if (d2 < r * r && d2 > 0.01f) {
        float d = sqrtf(d2), p = (r - d) * 0.25f / d;
        float wi = csE[i].type == E_BOSS ? 0.1f : 1, wj = csE[j].type == E_BOSS ? 0.1f : 1;
        csE[i].x -= dx * p * wi; csE[i].y -= dy * p * wi; csE[j].x += dx * p * wj; csE[j].y += dy * p * wj;
      }
    }
  }
}

// ---------------- one game step (1/30 s) ----------------
void csMovePlayer() {
  float jx = csIn.jx, jy = csIn.jy, m = sqrtf(jx * jx + jy * jy);
  if (m > 1) { jx /= m; jy /= m; m = 1; }
  if (m > 0.1f) { csFaceX = jx / m; csFaceY = jy / m; }
  if (csIn.dash && csDashCd <= 0) { csDashT = 7; csDashCd = 75; csPart(csX, csY, 0xCFE8FF, 6, 1.5f); }
  float sp = 1.75f * (1.0f + 0.1f * csUp[U_BOOTS]);
  if (csDashT > 0) { csDashT--; jx = csFaceX; jy = csFaceY; sp *= 3.2f; }
  csVX = csVX * 0.6f + jx * sp * 0.4f;
  csVY = csVY * 0.6f + jy * sp * 0.4f;
  int hx, hy;
  float nx = csX + csVX;
  if (!csHitsRock(nx, csY, 5, hx, hy)) csX = nx;
  else if (fabsf(csVX) > 0.3f && ++csMineT % 7 == 0) csDig(hx, hy, 1);       // pickaxe
  float ny = csY + csVY;
  if (!csHitsRock(csX, ny, 5, hx, hy)) csY = ny;
  else if (fabsf(csVY) > 0.3f && ++csMineT % 7 == 0) csDig(hx, hy, 1);
  if (csDashCd > 0) csDashCd--;
  if (csIfr > 0) csIfr--;
}
void csBullets() {
  for (int i = 0; i < CS_NB; i++) {
    CsBullet& b = csB[i];
    if (!b.on) continue;
    b.x += b.vx; b.y += b.vy;
    if (--b.life <= 0) { b.on = false; if (b.kind == 2) csExplode(b.tx, b.ty, b.dmg, 24 + csUp[U_GRENADE] * 4); continue; }
    if (b.kind == 2) continue;                               // grenades fly over everything
    int cx = (int)floorf(b.x / CS_CELL), cy = (int)floorf(b.y / CS_CELL);
    if (csRockAt(cx, cy)) { csDig(cx, cy, 1); b.on = false; continue; }
    for (int j = 0; j < CS_NE; j++) {
      CsEnemy& e = csE[j];
      if (!e.on) continue;
      float r = csEnemyR(e.type) + 2;
      if ((e.x - b.x) * (e.x - b.x) + (e.y - b.y) * (e.y - b.y) < r * r) {
        if (b.kind == 1) { e.vx += b.vx * 0.25f; e.vy += b.vy * 0.25f; }   // shotgun pushes back
        csHurt(j, b.dmg);
        if (--b.pierce < 0) { b.on = false; break; }
      }
    }
  }
  for (int i = 0; i < CS_NS; i++) {
    CsShot& s = csS[i];
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (--s.life <= 0 || csRockXY(s.x, s.y)) { s.on = false; continue; }
    float r = s.kind == 2 ? 4.5f : 3.0f;
    if ((s.x - csX) * (s.x - csX) + (s.y - csY) * (s.y - csY) < (r + 2.5f) * (r + 2.5f)) {
      if (csDashT <= 0 && csIfr <= 0) { s.on = false; csHitPlayer(s.kind == 2 ? 2 : 1); }
    }
  }
}
void csPickups() {
  float mag = 26 + csUp[U_MAGNET] * 16;
  for (int i = 0; i < CS_NG; i++) {
    CsGem& g = csG[i];
    if (!g.on) continue;
    float dx = csX - g.x, dy = csY - g.y, d = sqrtf(dx * dx + dy * dy) + 0.001f;
    if (d < mag) g.home = true;
    if (g.home) { float sp = 2.0f + (mag - min(d, mag)) * 0.08f; g.vx = dx / d * sp * 1.6f; g.vy = dy / d * sp * 1.6f; }
    else { g.vx *= 0.9f; g.vy *= 0.9f; }
    g.x += g.vx; g.y += g.vy;
    if (d < 8) csGain(g);
  }
  if (csXp >= csXpNeed && csState == CS_PLAY) csLevelUp();
}
void csSaveBest() {
  int t = min(csT(), CS_RUN_S);
  if (t > csBestS) { csBestS = t; prefs.putInt("csBest", csBestS); }
  if (csWon) { csWins++; prefs.putInt("csWins", csWins); }
}
void csStep() {
  csFrame++;
  // slow healing: 1 HP every 20 s (Big Heart makes it faster)
  if (csFrame % (CS_FPS * max(8, 20 - csUp[U_HEART] * 2)) == 0 && csHp < csMaxHp) csHp++;
  csMovePlayer();
  csWeapons();
  csEnemies();
  csBullets();
  csPickups();
  csSpawnTick();
  for (int i = 0; i < CS_NP; i++) if (csP[i].life > 0) { csP[i].life--; csP[i].x += csP[i].vx; csP[i].y += csP[i].vy; csP[i].vx *= 0.9f; csP[i].vy *= 0.9f; }
  for (auto& z : csZ) if (z.life > 0) z.life--;
  if (csShake > 0) csShake--;
  if (csBannerT > 0) csBannerT--;
  if (csPodOn) {
    if (hypotf(csX - csPodX, csY - csPodY) < 16) { csWon = true; csState = CS_OVER; csSaveBest(); ledFlash(0x40FF90, 1500); return; }
    if (csT() >= CS_POD_LAND_S + CS_POD_STAY_S) { csState = CS_OVER; csSaveBest(); return; }   // the pod left without you
  }
  if (csHp <= 0) { csHp = 0; csState = CS_OVER; csSaveBest(); ledFlash(0xFF0000, 1200); }
}

// ---------------- drawing ----------------
int csCamX, csCamY;
inline int csSX(float x) { return (int)x - csCamX; }
inline int csSY(float y) { return (int)y - csCamY; }
void csDrawMap() {
  spr.fillScreen(C(0x18120E));
  for (auto& s : csSpeck) {
    int x = s[0] - csCamX, y = s[1] - csCamY;
    if (x >= 0 && y >= CS_HUD && x < W && y < H) spr.drawPixel(x, y, C(0x3A2C22));
  }
  int c0 = max(0, csCamX / CS_CELL), c1 = min(CS_MW - 1, (csCamX + W) / CS_CELL);
  int r0 = max(0, csCamY / CS_CELL), r1 = min(CS_MH - 1, (csCamY + H) / CS_CELL);
  for (int cy = r0; cy <= r1; cy++) for (int cx = c0; cx <= c1; cx++) {
    uint8_t v = csCell(cx, cy);
    if (!v) continue;
    int x = cx * CS_CELL - csCamX, y = cy * CS_CELL - csCamY;
    if (v == CS_BEDROCK) { spr.fillRect(x, y, CS_CELL, CS_CELL, C(0x2B2420)); continue; }
    uint8_t ore = csOre[cy * CS_MW + cx];
    uint32_t base = blend(0x4A3A2C, 0x7A6248, min(1.0f, v / 7.0f));
    spr.fillRect(x, y, CS_CELL, CS_CELL, C(base));
    if (!csRockAt(cx, cy - 1)) spr.drawFastHLine(x, y, CS_CELL, C(0x9C8264));           // lit top edge
    if (!csRockAt(cx, cy + 1)) spr.drawFastHLine(x, y + CS_CELL - 1, CS_CELL, C(0x2E241C));
    if (ore) {
      uint32_t oc = ore == 1 ? 0xF2C230 : 0xFF4D6A;
      spr.fillRect(x + 4, y + 5, 3, 3, C(oc)); spr.fillRect(x + 9, y + 9, 3, 2, C(oc)); spr.drawPixel(x + 11, y + 4, C(oc));
    }
  }
}
void csDrawBug(const CsEnemy& e) {
  int x = csSX(e.x), y = csSY(e.y);
  float r = csEnemyR(e.type);
  if (x < -30 || y < -30 || x > W + 30 || y > H + 30) return;
  uint32_t body = e.type == E_SWARM ? 0xF0A040 : e.type == E_GRUNT ? 0xD0602A : e.type == E_SPIT ? 0x6FBF3A
                : e.type == E_TANK ? 0xA0402A : 0x8A3FC0;
  if (csRockXY(e.x, e.y)) body = blend(body, 0x4A3A2C, 0.5f);                            // inside the wall
  if (e.flash) body = e.type == E_BOSS ? blend(body, 0xFFFFFF, 0.45f) : 0xFFFFFF;
  float a = atan2f(csY - e.y, csX - e.x), ca = cosf(a), sa = sinf(a);
  // legs
  float wig = sinf(csFrame * 0.5f + e.x) * 2;
  for (int k = -1; k <= 1; k++) {
    float lx = -sa * (r + 3), ly = ca * (r + 3), ox = ca * k * r * 0.6f, oy = sa * k * r * 0.6f;
    spr.drawLine(x + ox, y + oy, x + ox + lx + ca * wig, y + oy + ly + sa * wig, C(0x3A2010));
    spr.drawLine(x + ox, y + oy, x + ox - lx - ca * wig, y + oy - ly - sa * wig, C(0x3A2010));
  }
  spr.fillCircle(x - ca * r * 0.7f, y - sa * r * 0.7f, r * 0.8f, C(blend(body, 0x000000, 0.25f)));   // back
  spr.fillCircle(x, y, r, C(body));
  int ex = x + ca * r * 0.55f, ey = y + sa * r * 0.55f;
  spr.fillCircle(ex - sa * r * 0.35f, ey + ca * r * 0.35f, max(1.0f, r / 5), C(0xFFF2A0));
  spr.fillCircle(ex + sa * r * 0.35f, ey - ca * r * 0.35f, max(1.0f, r / 5), C(0xFFF2A0));
  if (e.type == E_TANK || e.type == E_BOSS) {
    spr.drawCircle(x, y, r, C(0x200810));
    spr.drawArc(x, y, r - 3, r - 5, (int)(a * 57.3f) + 120, (int)(a * 57.3f) + 240, C(blend(body, 0xFFFFFF, 0.35f)));
  }
}
void csDrawPlayer() {
  int x = csSX(csX), y = csSY(csY);
  if (csIfr > 0 && (csIfr / 3) % 2) return;                                             // blink after a hit
  // head lamp beam
  float fa = atan2f(csFaceY, csFaceX);
  int bx1 = x + cosf(fa - 0.35f) * 46, by1 = y + sinf(fa - 0.35f) * 46, bx2 = x + cosf(fa + 0.35f) * 46, by2 = y + sinf(fa + 0.35f) * 46;
  spr.fillTriangle(x, y, bx1, by1, bx2, by2, C(0x3A3020));
  if (csDashT > 0) { spr.fillCircle(x - csFaceX * 8, y - csFaceY * 8, 5, C(0x5A7090)); spr.fillCircle(x - csFaceX * 15, y - csFaceY * 15, 3, C(0x3A5070)); }
  spr.fillCircle(x, y, 7, C(0x2C4A7A));                                                  // armor
  spr.fillCircle(x, y, 5, C(0x4C78B8));
  spr.fillCircle(x + csFaceX * 2, y + csFaceY * 2, 3, C(0xF2B030));                      // helmet
  spr.fillCircle(x + csFaceX * 5, y + csFaceY * 5, 2, C(0xFFF6C0));                      // lamp
  spr.fillCircle(x - csFaceX * 3 + csFaceY * 2, y - csFaceY * 3 - csFaceX * 2, 2, C(0xC8783A));   // beard
  if (csDashCd <= 0) spr.drawCircle(x, y, 9, C(0x3F6F9F));                               // dash ready
  if (csUp[U_DRILLS]) {
    int l = csUp[U_DRILLS], n = 1 + l; float r = 24 + l * 1.5f, a0 = csFrame * 0.13f;
    for (int k = 0; k < n; k++) {
      float a = a0 + k * 6.2832f / n; int dx = x + cosf(a) * r, dy = y + sinf(a) * r;
      spr.fillTriangle(dx + cosf(a + 1.6f) * 6, dy + sinf(a + 1.6f) * 6, dx + cosf(a - 1.2f) * 3, dy + sinf(a - 1.2f) * 3, dx + cosf(a + 2.6f) * 3, dy + sinf(a + 2.6f) * 3, C(0x9AD1FF));
      spr.fillCircle(dx, dy, 2, C(0xD8F0FF));
    }
  }
}
void csDrawPod() {
  if (!csPodOn) return;
  int x = csSX(csPodX), y = csSY(csPodY);
  float pulse = 0.5f + 0.5f * sinf(csFrame * 0.25f);
  spr.drawCircle(x, y, 16 + pulse * 4, C(0x40FF90));
  spr.fillRoundRect(x - 9, y - 12, 18, 24, 6, C(0xC8D0D8));
  spr.fillRect(x - 5, y - 6, 10, 8, C(0x203040));
  spr.fillRect(x - 9, y + 8, 18, 4, C(0xFF8C30));
  if (x < 0 || y < CS_HUD || x >= W || y >= H) {                                         // arrow at the screen edge
    float a = atan2f(csPodY - csY, csPodX - csX);
    int cx = W / 2, cy = (H + CS_HUD) / 2;
    float k = min((W / 2 - 14) / max(0.01f, fabsf(cosf(a))), ((H - CS_HUD) / 2 - 14) / max(0.01f, fabsf(sinf(a))));
    int ax = cx + cosf(a) * k, ay = cy + sinf(a) * k;
    spr.fillTriangle(ax + cosf(a) * 9, ay + sinf(a) * 9, ax + cosf(a + 2.4f) * 7, ay + sinf(a + 2.4f) * 7, ax + cosf(a - 2.4f) * 7, ay + sinf(a - 2.4f) * 7, C(0x40FF90));
    char b[12]; snprintf(b, sizeof b, "%dm", (int)(hypotf(csPodX - csX, csPodY - csY) / 8));
    csText(FS, b, ax - cosf(a) * 16, ay - sinf(a) * 16, 0x40FF90, D_MC);
  }
}
void csDrawWorld() {
  int sx = csShake ? csRi(5) - 2 : 0, sy = csShake ? csRi(5) - 2 : 0;
  csCamX = constrain((int)csX - W / 2, 0, CS_WORLD_W - W) + sx;
  csCamY = constrain((int)csY - (H + CS_HUD) / 2, -CS_HUD, CS_WORLD_H - H) + sy;
  csDrawMap();
  for (int i = 0; i < CS_NG; i++) if (csG[i].on) {
    const CsGem& g = csG[i]; int x = csSX(g.x), y = csSY(g.y);
    if (x < -4 || y < -4 || x > W + 4 || y > H + 4) continue;
    if (g.kind == 0) { int s = g.val >= 8 ? 4 : g.val >= 2 ? 3 : 2; spr.fillTriangle(x, y - s - 1, x - s, y, x + s, y, C(0x4FC3FF)); spr.fillTriangle(x - s, y, x + s, y, x, y + s + 1, C(0x2A88D8)); }
    else if (g.kind == 1) { spr.fillCircle(x, y, 3, C(0xF2C230)); spr.drawPixel(x - 1, y - 1, C(0xFFF4B0)); }
    else { spr.fillCircle(x, y, 4, C(0xFF4D6A)); spr.fillRect(x - 1, y - 3, 2, 6, C(0xFFFFFF)); spr.fillRect(x - 3, y - 1, 6, 2, C(0xFFFFFF)); }
  }
  csDrawPod();
  for (int i = 0; i < CS_NE; i++) if (csE[i].on) csDrawBug(csE[i]);
  csDrawPlayer();
  for (int i = 0; i < CS_NB; i++) if (csB[i].on) {
    const CsBullet& b = csB[i]; int x = csSX(b.x), y = csSY(b.y);
    if (b.kind == 2) { float h = sinf((18 - b.life) / 18.0f * 3.1416f) * 10; spr.fillCircle(x, y - h, 3, C(0x7CE38B)); spr.drawPixel(x, y - h - 3, C(0xFFFFFF)); }
    else if (b.kind == 1) spr.fillRect(x - 1, y - 1, 2, 2, C(0xFFB060));
    else spr.drawLine(x, y, x - b.vx * 1.2f, y - b.vy * 1.2f, C(0xFFE070));
  }
  for (auto& z : csZ) if (z.life > 0) {
    int x1 = csSX(z.x1), y1 = csSY(z.y1), x2 = csSX(z.x2), y2 = csSY(z.y2);
    int mx = (x1 + x2) / 2 + csRi(9) - 4, my = (y1 + y2) / 2 + csRi(9) - 4;
    spr.drawLine(x1, y1, mx, my, C(0xD8C8FF)); spr.drawLine(mx, my, x2, y2, C(0xD8C8FF));
    spr.drawLine(x1 + 1, y1, mx + 1, my, C(0x9070FF)); spr.drawLine(mx + 1, my, x2 + 1, y2, C(0x9070FF));
  }
  for (int i = 0; i < CS_NS; i++) if (csS[i].on) {
    const CsShot& s = csS[i]; int x = csSX(s.x), y = csSY(s.y);
    if (x < -6 || y < -6 || x > W + 6 || y > H + 6) continue;
    if (s.kind == 0) { spr.fillCircle(x, y, 3, C(0x5FD030)); spr.fillCircle(x, y, 1, C(0xE8FFB0)); }
    else if (s.kind == 1) { spr.fillCircle(x, y, 3, C(0xFF4FC8)); spr.fillCircle(x, y, 1, C(0xFFE0F6)); }
    else { spr.fillCircle(x, y, 4, C(0xFF7A2A)); spr.fillCircle(x, y, 2, C(0xFFE0A0)); }
  }
  for (int i = 0; i < CS_NP; i++) if (csP[i].life > 0) spr.fillRect(csSX(csP[i].x), csSY(csP[i].y), 2, 2, C(csP[i].col));
}
void csDrawHud() {
  spr.fillRect(0, 0, W, CS_HUD, C(0x0A0806));
  // HP
  int hw = min(70, W / 4);
  spr.fillRoundRect(4, 5, hw, 10, 3, C(0x401010));
  spr.fillRoundRect(4, 5, max(0, hw * csHp / max(1, csMaxHp)), 10, 3, C(csHp * 3 <= csMaxHp ? 0xFF3030 : 0xE04848));
  char b[40]; snprintf(b, sizeof b, "%d", csHp);
  csText(&fonts::Font0, b, 4 + hw + 4, 10, 0xFFB0B0, D_ML);
  // time
  int t = min(csT(), CS_RUN_S); snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
  csText(FB, b, W / 2, CS_HUD / 2, csPodOn ? 0x40FF90 : 0xFFFFFF, D_MC);
  // gold + level
  snprintf(b, sizeof b, "Lv%d", csLvl);
  csText(&fonts::Font0, b, W - 30, 7, 0x9AD8FF, D_MR);
  snprintf(b, sizeof b, "%d", csGold);
  spr.fillCircle(W - 30 - 4 - (int)strlen(b) * 6 - 6, 16, 2, C(0xF2C230));
  csText(&fonts::Font0, b, W - 30, 16, 0xF2C230, D_MR);
  // pause button
  spr.fillRoundRect(W - 24, 3, 20, 16, 4, C(0x2A2A2A));
  spr.fillRect(W - 18, 7, 3, 8, C(0xFFFFFF)); spr.fillRect(W - 13, 7, 3, 8, C(0xFFFFFF));
  // xp bar
  spr.fillRect(0, CS_HUD, W, 3, C(0x102030));
  spr.fillRect(0, CS_HUD, W * min(csXp, csXpNeed) / max(1, csXpNeed), 3, C(0x4FC3FF));
  // boss bar
  if (csBossIdx >= 0 && csE[csBossIdx].on) {
    const CsEnemy& e = csE[csBossIdx];
    spr.fillRect(20, CS_HUD + 8, W - 40, 6, C(0x301040));
    spr.fillRect(20, CS_HUD + 8, (W - 40) * max(0, (int)e.hp) / max(1, (int)e.maxhp), 6, C(0xB050FF));
    csText(&fonts::Font0, "DREADNOUGHT", W / 2, CS_HUD + 20, 0xD8B0FF, D_MC);
  }
  if (csPodOn && csState != CS_OVER) {
    int left = max(0, CS_POD_LAND_S + CS_POD_STAY_S - csT());
    snprintf(b, sizeof b, "Get to the pod! %ds", left);
    csText(FS, b, W / 2, H - 12, 0x40FF90, D_MC);
  }
  if (csBannerT > 0) {
    int y = CS_HUD + 52;
    spr.setFont(FL); int tw = spr.textWidth(csBanner) + 20;
    spr.fillRoundRect(W / 2 - tw / 2, y - 16, tw, 32, 8, C(0x000000));
    csText(FL, csBanner, W / 2, y, (csFrame / 4) % 2 ? 0xFFB040 : 0xFFFFFF, D_MC);
  }
}
// box with up to 3 option cards (level up)
void csCardRect(int i, int& x, int& y, int& w, int& h) {
  if (W > H) { w = (W - 24) / 3; h = H - 84; x = 6 + i * (w + 6); y = 76; }
  else { w = W - 20; h = 56; x = 10; y = 86 + i * (h + 8); }
}
void csDrawLevelUp() {
  spr.fillRect(0, CS_HUD + 3, W, H - CS_HUD - 3, C(0x000000));
  csText(FL, "LEVEL UP!", W / 2, CS_HUD + 22, 0x4FC3FF, D_MC);
  csText(FS, csJoy ? "Push up/down, press to pick" : "Tap one to pick", W / 2, CS_HUD + 42, 0x9AA7B4, D_MC);
  for (int i = 0; i < csNChoice; i++) {
    int x, y, w, h; csCardRect(i, x, y, w, h);
    uint8_t u = csChoice[i];
    bool sel = i == csSel && csJoy;
    uint32_t col = u < U_N ? CS_UPG_COL[u] : 0xFFFFFF;
    spr.fillRoundRect(x, y, w, h, 8, C(sel ? blend(0x1A1A1A, col, 0.3f) : 0x1A1A1A));
    spr.drawRoundRect(x, y, w, h, 8, C(sel ? col : 0x444444));
    if (sel) spr.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, C(col));
    char name[24], lvl[16], info[48];
    if (u == U_HEAL) { strcpy(name, "Full heal"); strcpy(lvl, ""); strcpy(info, "Back to full HP"); }
    else if (u == U_GOLD) { strcpy(name, "+50 gold"); strcpy(lvl, ""); strcpy(info, "More score"); }
    else {
      strcpy(name, CS_UPG_NAME[u]);
      if (csUp[u]) snprintf(lvl, sizeof lvl, "Lv %d", csUp[u] + 1); else strcpy(lvl, "NEW");
      strcpy(info, CS_UPG_INFO[u]);
    }
    if (W > H) {
      // narrow card: icon, name, level, then the text in up to 3 lines
      spr.fillCircle(x + w / 2, y + 16, 8, C(col));
      csText(FB, name, x + w / 2, y + 38, col, D_MC);
      csText(FS, lvl, x + w / 2, y + 58, lvl[0] == 'N' ? 0xFFE070 : 0x9AA7B4, D_MC);
      spr.setFont(FS);
      char* words = info; int ly = y + 86;
      while (*words && ly < y + h - 6) {
        char line[48]; int n = 0, lastSp = -1;
        while (words[n]) {
          line[n] = words[n]; line[n + 1] = 0;
          if (spr.textWidth(line) > w - 8) break;
          if (words[n] == ' ') lastSp = n;
          n++;
        }
        if (words[n] && lastSp > 0) n = lastSp;
        line[n] = 0;
        csText(FS, line, x + w / 2, ly, 0xC8D0D8, D_MC);
        words += n; while (*words == ' ') words++;
        ly += 19;
      }
    } else {
      spr.fillCircle(x + 16, y + h / 2, 7, C(col));
      csText(FB, name, x + 30, y + 18, col, D_ML);
      spr.setFont(FB); int nw = spr.textWidth(name);
      csText(FS, lvl, x + 30 + nw + 8, y + 18, lvl[0] == 'N' ? 0xFFE070 : 0x9AA7B4, D_ML);
      csText(FS, info, x + 30, y + 40, 0xC8D0D8, D_ML);
    }
  }
}
void csPanel(const char* title, uint32_t tc, int lines, const char* const* txt, const uint32_t* cols) {
  // sits above the two buttons at the bottom
  int ls = H < 260 ? 19 : 21;                       // shorter lines on the wide screen
  int bw = W - 20, bh = 58 + lines * ls, bx = 10, by = max(CS_HUD + 4, (H - 56 - bh) / 2);
  spr.fillRoundRect(bx, by, bw, bh, 12, C(0x000000));
  spr.drawRoundRect(bx, by, bw, bh, 12, C(0xFFFFFF));
  csText(FL, title, W / 2, by + 22, tc, D_MC);
  for (int i = 0; i < lines; i++) csText(FS, txt[i], W / 2, by + 48 + i * ls, cols[i], D_MC);
}
void csButtonsRect(int i, int& x, int& y, int& w, int& h) { w = (W - 36) / 2; h = 34; x = 12 + i * (w + 12); y = H - 50; }
void csDrawButtons(const char* a, const char* b) {
  for (int i = 0; i < 2; i++) {
    int x, y, w, h; csButtonsRect(i, x, y, w, h);
    spr.fillRoundRect(x, y, w, h, 9, C(i ? 0xF2B030 : 0x333333));
    csText(FB, i ? b : a, x + w / 2, y + h / 2, i ? 0x000000 : 0xFFFFFF, D_MC);
  }
}
void caveDraw() {
  if (csState == CS_TITLE) {
    spr.fillScreen(C(0x0E0A08));
    csDrawWorld();
    char best[40];
    if (csWins) snprintf(best, sizeof best, "Escaped %dx, best %d:%02d", csWins, csBestS / 60, csBestS % 60);
    else if (csBestS) snprintf(best, sizeof best, "Best: lived %d:%02d", csBestS / 60, csBestS % 60);
    else strcpy(best, "9:30 = pod lands. Get in!");
    const char* L[5] = {csJoy ? "Stick = move" : "Drag = move",
                        csJoy ? "Press = dash past acid" : "Tap = dash past acid",
                        "Guns fire by themselves",
                        "Walk into rock = dig", best};
    const uint32_t cols[5] = {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xF2C230, 0x9AA7B4};
    csPanel("CAVE SWARM", 0xFFB040, 5, L, cols);
    csDrawButtons("Back", "Start");
  } else if (csState == CS_LEVELUP) {
    csDrawWorld(); csDrawHud(); csDrawLevelUp();
  } else {
    csDrawWorld(); csDrawHud();
    if (csState == CS_PAUSE) {
      const char* L[3] = {"Bugs wait for you.", csJoy ? "Press stick = go on" : "", ""};
      const uint32_t cols[3] = {0xFFFFFF, 0xFFE9A8, 0xFFFFFF};
      csPanel("PAUSED", 0xFFFFFF, 2, L, cols);
      csDrawButtons("Quit", "Go on");
    } else if (csState == CS_OVER) {
      char a[40], b[40], c[40];
      int t = min(csT(), CS_RUN_S);
      snprintf(a, sizeof a, "Time %d:%02d   Level %d", t / 60, t % 60, csLvl);
      snprintf(b, sizeof b, "Bugs %d   Gold %d", csKills, csGold);
      snprintf(c, sizeof c, "Best %d:%02d", csBestS / 60, csBestS % 60);
      const char* L[3] = {a, b, c};
      const uint32_t cols[3] = {0xFFFFFF, 0xF2C230, 0x9AA7B4};
      const char* title = csWon ? "YOU ESCAPED!" : csHp <= 0 ? "MINER DOWN" : "LEFT BEHIND";
      csPanel(title, csWon ? 0x40FF90 : 0xFF5050, 3, L, cols);
      csDrawButtons("Back", "Again");
    }
  }
}

// ---------------- start / input / loop ----------------
bool csAlloc() {
  if (csMap) return true;
#ifdef CAVE_HOST
  csMap = (uint8_t*)calloc(CS_MW * CS_MH, 1); csOre = (uint8_t*)calloc(CS_MW * CS_MH, 1);
  csE = (CsEnemy*)calloc(CS_NE, sizeof(CsEnemy)); csB = (CsBullet*)calloc(CS_NB, sizeof(CsBullet));
  csS = (CsShot*)calloc(CS_NS, sizeof(CsShot)); csG = (CsGem*)calloc(CS_NG, sizeof(CsGem)); csP = (CsPart*)calloc(CS_NP, sizeof(CsPart));
#else
  auto A = [](size_t n) { void* p = heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM); return p ? p : calloc(1, n); };
  csMap = (uint8_t*)A(CS_MW * CS_MH); csOre = (uint8_t*)A(CS_MW * CS_MH);
  csE = (CsEnemy*)A(CS_NE * sizeof(CsEnemy)); csB = (CsBullet*)A(CS_NB * sizeof(CsBullet));
  csS = (CsShot*)A(CS_NS * sizeof(CsShot)); csG = (CsGem*)A(CS_NG * sizeof(CsGem)); csP = (CsPart*)A(CS_NP * sizeof(CsPart));
#endif
  return csMap && csOre && csE && csB && csS && csG && csP;
}
void caveNewRun() {
  csMakeMap();
  memset(csE, 0, CS_NE * sizeof(CsEnemy)); memset(csB, 0, CS_NB * sizeof(CsBullet));
  memset(csS, 0, CS_NS * sizeof(CsShot)); memset(csG, 0, CS_NG * sizeof(CsGem)); memset(csP, 0, CS_NP * sizeof(CsPart));
  memset(csZ, 0, sizeof csZ); memset(csUp, 0, sizeof csUp); memset(csWcd, 0, sizeof csWcd);
  csUp[U_RIFLE] = 1;
  csX = CS_WORLD_W / 2; csY = CS_WORLD_H / 2; csVX = csVY = 0; csFaceX = 1; csFaceY = 0;
  csMaxHp = 12; csHp = 12; csIfr = csDashT = csDashCd = csMineT = 0;
  csLvl = 1; csXp = 0; csXpNeed = 6; csKills = 0; csGold = 0; csFrame = 0; csShake = 0;
  csBossIdx = -1; csBossDone = false; csWon = false; csPodOn = false; csBannerT = 0; csSwarmNext = 120;
}
// read the joystick and the touch screen into csIn
#ifndef CAVE_HOST
bool csTouchDown = false, csTouchMoved = false; int csTx0, csTy0, csTxN, csTyN; uint32_t csTt0;
int csJoyRepeatT = 0;
void caveReadInput() {
  memset(&csIn, 0, sizeof csIn);
  if (csJoy) {
    float jx = joyAxis(joyRawX(), joyCX), jy = joyAxis(joyRawY(), joyCY);
    if (joyMap & 1) { float t = jx; jx = jy; jy = t; }
    if (joyMap & 2) jx = -jx;
    if (joyMap & 4) jy = -jy;
    csIn.jx = jx; csIn.jy = jy;
    if (joyPressed()) csIn.dash = true;
    // menus: one step per push (held = repeat)
    if (fabsf(jy) > 0.6f) { if (csJoyRepeatT <= 0) { if (jy < 0) csIn.up = true; else csIn.down = true; csJoyRepeatT = 8; } else csJoyRepeatT--; }
    else if (fabsf(jx) > 0.6f && W > H) { if (csJoyRepeatT <= 0) { if (jx < 0) csIn.up = true; else csIn.down = true; csJoyRepeatT = 8; } else csJoyRepeatT--; }
    else csJoyRepeatT = 0;
  }
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (down && !csTouchDown) { csTouchDown = true; csTouchMoved = false; csTx0 = csTxN = tp.x; csTy0 = csTyN = tp.y; csTt0 = millis(); }
  else if (down) {
    csTxN = tp.x; csTyN = tp.y;
    if (abs(csTxN - csTx0) + abs(csTyN - csTy0) > 10) csTouchMoved = true;
    if (csTouchMoved && csState == CS_PLAY && csTy0 > CS_HUD) {            // virtual stick: drag from where you put the finger
      float dx = (csTxN - csTx0) / 28.0f, dy = (csTyN - csTy0) / 28.0f, m = sqrtf(dx * dx + dy * dy);
      if (m > 1) { dx /= m; dy /= m; }
      if (fabsf(csIn.jx) + fabsf(csIn.jy) < 0.1f) { csIn.jx = dx; csIn.jy = dy; }
    }
  } else if (csTouchDown) {
    csTouchDown = false;
    if (!csTouchMoved && millis() - csTt0 < 600) { csIn.tap = true; csIn.tx = csTx0; csIn.ty = csTy0; }
  }
}
#endif
bool csHit(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
void caveExit() {
  scr = S_GAMES; dirty = true;
}
// what a frame does with csIn (also used by the computer test)
void caveUpdate() {
  int bx, by, bw, bh;
  switch (csState) {
    case CS_TITLE:
      if (csIn.tap) {
        csButtonsRect(0, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { caveExit(); return; }
        csButtonsRect(1, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { caveNewRun(); csState = CS_PLAY; return; }
      }
      if (csIn.dash) { caveNewRun(); csState = CS_PLAY; }
      csFrame++;                                  // let the cave behind the title move a little
      break;
    case CS_PLAY:
      if (csIn.tap && csIn.ty < CS_HUD + 4 && csIn.tx > W - 40) { csState = CS_PAUSE; return; }
      if (csIn.tap && !csJoy && csIn.ty >= CS_HUD) csIn.dash = true;          // touch: tap = dash
      csStep();
      break;
    case CS_PAUSE:
      if (csIn.dash) { csState = CS_PLAY; return; }
      if (csIn.tap) {
        csButtonsRect(0, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { csSaveBest(); caveExit(); return; }
        csButtonsRect(1, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { csState = CS_PLAY; return; }
      }
      break;
    case CS_LEVELUP:
      if (millis() - csMenuT < 350) break;       // don't pick by accident while still dashing
      if (csIn.up) csSel = (csSel + csNChoice - 1) % csNChoice;
      if (csIn.down) csSel = (csSel + 1) % csNChoice;
      if (csIn.dash) { csApply(csChoice[csSel]); csState = CS_PLAY; return; }
      if (csIn.tap) for (int i = 0; i < csNChoice; i++) {
        int x, y, w, h; csCardRect(i, x, y, w, h);
        if (csHit(csIn.tx, csIn.ty, x, y, w, h)) { csApply(csChoice[i]); csState = CS_PLAY; return; }
      }
      break;
    case CS_OVER:
      if (csIn.tap) {
        csButtonsRect(0, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { caveExit(); return; }
        csButtonsRect(1, bx, by, bw, bh); if (csHit(csIn.tx, csIn.ty, bx, by, bw, bh)) { caveNewRun(); csState = CS_PLAY; return; }
      }
      if (csIn.dash) { caveNewRun(); csState = CS_PLAY; }
      break;
  }
}
#ifndef CAVE_HOST
void caveOpen() {
  if (!csAlloc()) { scr = S_GAMES; dirty = true; return; }
  scr = S_CAVE;
  csSeed = esp_random() | 1;
  csBestS = prefs.getInt("csBest", 0); csWins = prefs.getInt("csWins", 0);
  csJoy = joyDetect();
  joyMap = prefs.getUChar("joyMap", 0);
  caveNewRun();
  csState = CS_TITLE;
  csTouchDown = true; csTouchMoved = true;     // ignore the finger that opened the game
  csLastFrame = 0;
}
// called from loop() while the game is open
void caveLoop() {
  if (millis() - csLastFrame < 1000 / CS_FPS) return;
  csLastFrame = millis();
  lastTouchMs = millis();                      // keep the screen on while playing
  caveReadInput();
  caveUpdate();
  if (scr != S_CAVE) return;
  caveDraw();
  spr.pushSprite(0, 0);
}
String caveSub() {
  int b = prefs.getInt("csBest", 0), w = prefs.getInt("csWins", 0);
  if (w) return "Escaped " + String(w) + "x";
  if (b) return "Best " + String(b / 60) + ":" + (b % 60 < 10 ? "0" : "") + String(b % 60);
  return "Bug swarm shooter";
}
#endif
