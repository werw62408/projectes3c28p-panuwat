#pragma once
// ============================================================================
//  Ant Colony (v11.8, takes the place of Habit Garden): an ant farm that grows from the goals you reach in Log.
//    - it starts with a queen and 3 small workers in a little room under the ground
//    - every goal reached on a day = food = the queen lays 1 egg.
//      egg (3 days) -> larva (3 days) -> pupa (2 days) -> a new worker: 8 days, like real small ants
//    - more workers dig more: winding tunnels, side tunnels and flat-floored rooms, like a real ant farm.
//      While the page is open you see them dig, carry the soil out to a pile next to the door and bring food down;
//      while it is closed they go on digging, and the new tunnels are there when you come back.
//    - the "colony brain" picks where the next room goes and when to dig a side tunnel;
//      each ant follows simple rules (walk the tunnels, dig at the face, carry soil out, bring food, look after the brood)
//    - streak = days in a row with at least half of the goals reached (like the Garden)
//  v11.9: the whole screen like Deck (no tabs, no clock bar: a thin bar with Exit), the farm is 240 x 296
//  (was 240 x 144), soil with patches, pebbles and roots, glass shine, bigger ants, soil carried out to the pile.
//  A v11.8 colony is moved into the bigger farm (nothing is lost).
//  Nothing is ever taken away: the colony only grows. The old Garden numbers stay in Preferences (not used).
//  Saved in LittleFS /ants.bin (a small header + 1 bit per pixel of the ground), written at most once a minute.
//  Included from SomudTick.ino.
// ============================================================================

#define HAS_ANTS 2
const int AF_W = 240, AF_H = 296, AF_SKY = 24;    // the farm: 240 x 296 pixels, rows 0..23 above the ground
const int AG_W = AF_W / 2, AG_H = AF_H / 2;       // the ants walk on a 2 px grid
const int AN_MAXROOM = 48, AN_HATCH = 8;
const int AN_BAR = 24;                            // the thin bar on top (Exit, name, day)
enum AnKind : uint8_t { AK_QUEEN, AK_BROOD, AK_FOOD, AK_ROOM };
struct AnRoom { int16_t x, y; uint8_t r, kind, prog, seed; };   // prog 0..8 (8 = dug out)
struct AnPlan { float hx, hy, ha; int16_t room, tx, ty; uint16_t steps; uint8_t on, pad; };   // the tunnel being dug
struct AnSave {
  uint32_t magic, ver, rng;
  char day[12];              // the last day counted (YYYY-MM-DD), "" = never
  uint16_t workers, streak, best, nRooms;
  uint16_t brood[AN_HATCH];  // eggs laid 0..7 days ago
  uint32_t dug, born, lastT, mound, foundT;   // pixels dug, workers born, digging counted until, soil at the door, start
  AnPlan plan;
  AnRoom room[AN_MAXROOM];
  int16_t door, pad2;        // where the entrance is (x)
};
const uint32_t AN_MAGIC = 0x414E5433;   // "ANT3" (v11.9)
const int AN_BITS = AF_W * AF_H / 8;
// the v11.8 file ("ANT2": 240 x 144, 14 rows of sky, 40 rooms, door at x 80), moved into the new farm when found
struct AnSaveV2 { uint32_t magic, ver, rng; char day[12]; uint16_t workers, streak, best, nRooms; uint16_t brood[AN_HATCH];
                  uint32_t dug, born, lastT, mound, foundT; AnPlan plan; AnRoom room[40]; };
const uint32_t AN_MAGIC_V2 = 0x414E5432; const int V2_W = 240, V2_H = 144, V2_SKY = 14, V2_DOOR = 80;
AnSave an;
uint8_t* anBits = nullptr;              // 1 = dug (open)
bool anLoaded = false, anDirtyMap = false, anLabels = false, anFounding = false;
String anSyncedFor;
uint32_t anSavedMs = 0;
int anToday = 0, anGoals = 0;           // goals reached today (the food / eggs of today)
int anScroll = 0, anScrollMax = 0;      // wide screen: the farm is taller than the screen (drag / stick up-down)
int16_t anRx0 = AF_W, anRy0 = AF_H, anRx1 = -1, anRy1 = -1;   // what changed since the picture was drawn

inline bool anPx(int x, int y) {        // open (dug, or air)?
  if (x < 0 || x >= AF_W || y < 0 || y >= AF_H) return false;
  if (y < AF_SKY) return true;
  int i = y * AF_W + x; return anBits[i >> 3] & (1 << (i & 7));
}
uint32_t anRand() { an.rng = an.rng * 1664525u + 1013904223u; return an.rng >> 8; }
float anRf() { return (anRand() & 0xFFFF) / 65535.0f; }
int anRnd(int n) { return n > 0 ? (int)(anRand() % (uint32_t)n) : 0; }
uint32_t anHash(int x, int y) { uint32_t h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }

// ---------------- goals of a day (the same rule as the Log page and the old Garden) ----------------
void anScore(const std::map<String, Sum>& sums, bool any, int& done, int& goals) {
  done = goals = 0;
  for (auto& a : acts) {
    if (a.goal <= 0 || !a.goalType) continue;
    goals++;
    auto it = sums.find(a.id);
    Sum s = it == sums.end() ? Sum() : it->second;
    float m = measure(a, s);
    if (a.goalType == 1 ? m >= a.goal : (any && m <= a.goal)) done++;
  }
}
void anTodayScore() {
  std::map<String, Sum> m;
  for (auto& e : todayEv) { Sum& s = m[e.id]; s.count++; s.sum += e.v; }
  anScore(m, !todayEv.empty(), anToday, anGoals);
}

// ---------------- digging ----------------
void anOpenPx(int x, int y) {
  if (x < 2 || x >= AF_W - 2 || y < AF_SKY || y >= AF_H - 2) return;   // the glass edges stay
  int i = y * AF_W + x; if (anBits[i >> 3] & (1 << (i & 7))) return;
  anBits[i >> 3] |= 1 << (i & 7);
  an.dug++; anDirtyMap = true;
  anRx0 = min(anRx0, (int16_t)x); anRy0 = min(anRy0, (int16_t)y); anRx1 = max(anRx1, (int16_t)x); anRy1 = max(anRy1, (int16_t)y);
}
void anDisk(float cx, float cy, float rx, float ry) {   // dig an oval
  for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++) for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
    float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
    if (dx * dx + dy * dy <= 1.0f) anOpenPx(x, y);
  }
}
// v11.9: a room is a long, flat gallery (like in a real ant farm): r = half its length, a little tilted, 5-7 px high,
// the floor flat, the roof a bit uneven. The shape comes from its seed.
float anTilt(const AnRoom& r) { return ((int)(anHash(r.seed, 31) % 21) - 10) * 0.016f; }   // -0.16 .. 0.16 (about +-9 degrees)
float anRoomH(const AnRoom& r) { return 2.0f + (anHash(r.seed, 47) % 3) * 0.4f; }         // half the height
// the floor of the gallery at x (y of its lowest open row)
float anFloorY(const AnRoom& r, float x) { return r.y + (x - r.x) * anTilt(r) + anRoomH(r) * 0.9f; }
void anDigRoom(const AnRoom& r, int prog) {   // prog 1..8: dug from the middle outward
  float half = r.r * prog / 8.0f, t = anTilt(r), hh = anRoomH(r);
  for (float dx = -half; dx <= half; dx += 1.0f) {
    float cx = r.x + dx, cy = r.y + dx * t;
    float end = 1 - fabsf(dx) / (r.r + 0.01f);                           // lower at the ends (rounded)
    float roof = hh * (0.55f + 0.45f * sqrtf(max(0.0f, end))) + ((int)(anHash((int)dx + 50, r.seed) % 3) - 1) * 0.4f;
    for (int y = (int)floorf(cy - roof); y <= (int)ceilf(cy + hh * 0.9f); y++) anOpenPx((int)roundf(cx), y);
  }
}
bool anRoomFree(int x, int y, int r) {
  if (x - r < 6 || x + r > AF_W - 6 || y < AF_SKY + 16 || y > AF_H - 10) return false;
  for (int i = 0; i < an.nRooms; i++) {
    AnRoom& o = an.room[i];
    if (abs(o.x - x) < o.r + r + 10 && abs(o.y - y) < 22) return false;   // galleries: long and flat, 22 px apart up / down
  }
  return true;
}
// the nearest open pixel under the ground (where a new tunnel starts)
bool anNearestOpen(int tx, int ty, int& ox, int& oy) {
  long bd = 0x7FFFFFFF; ox = oy = -1;
  for (int y = AF_SKY + 1; y < AF_H; y += 2) for (int x = 1; x < AF_W; x += 2) {
    if (!anPx(x, y)) continue;
    long d = (long)(x - tx) * (x - tx) + (long)(y - ty) * (y - ty) * 2;
    if (d < bd) { bd = d; ox = x; oy = y; }
  }
  return ox >= 0;
}
// the colony brain: a new room (deeper as the colony grows), or now and then a side tunnel
bool anNewPlan() {
  AnPlan& p = an.plan;
  if (an.nRooms >= 2 && anRnd(4) == 0) {   // side tunnel: from a random open place, sideways or down, 20-45 px
    for (int t = 0; t < 200; t++) {
      int x = 4 + anRnd(AF_W - 8), y = AF_SKY + 10 + anRnd(AF_H - AF_SKY - 14);
      if (!anPx(x, y)) continue;
      float a = (anRnd(2) ? 0.0f : PI) + (anRf() - 0.3f) * 1.1f, len = 20 + anRnd(26);
      int tx = x + cosf(a) * len, ty = y + fabsf(sinf(a)) * len;
      if (tx < 8 || tx > AF_W - 8 || ty > AF_H - 8) continue;
      p = AnPlan{(float)x, (float)y, atan2f(ty - y, tx - x), -1, (int16_t)tx, (int16_t)ty, 0, 1, 0};
      return true;
    }
  }
  if (an.nRooms >= AN_MAXROOM) return false;
  for (int t = 0; t < 160; t++) {
    int r = t < 90 ? 20 + anRnd(24) : 10 + anRnd(8);   // half the length of a gallery (no room for a long one: a shorter one)
    int deepest = min(AF_H - 10, AF_SKY + 44 + an.nRooms * 14);
    int x = 8 + r + anRnd(AF_W - 16 - 2 * r), y = AF_SKY + 16 + anRnd(max(1, deepest - AF_SKY - 16));
    if (!anRoomFree(x, y, r)) continue;
    int sx, sy; if (!anNearestOpen(x, y, sx, sy)) return false;
    int nb = 0, nf = 0; for (int i = 0; i < an.nRooms; i++) { nb += an.room[i].kind == AK_BROOD; nf += an.room[i].kind == AK_FOOD; }
    uint8_t kind = nb <= nf ? AK_BROOD : (anRnd(3) ? AK_FOOD : AK_ROOM);
    an.room[an.nRooms] = AnRoom{(int16_t)x, (int16_t)y, (uint8_t)r, kind, 0, (uint8_t)anRnd(256)};
    p = AnPlan{(float)sx, (float)sy, atan2f(y - sy, x - sx), (int16_t)an.nRooms, (int16_t)x, (int16_t)y, 0, 1, 0};
    an.nRooms++;
    return true;
  }
  return false;
}
// how much the colony wants to have dug: grows with the workers, at most a third of the ground
uint32_t anDigGoal() { return min((uint32_t)((AF_H - AF_SKY) * AF_W / 3), (uint32_t)(260 + an.workers * 120)); }
// one bite of digging: the tunnel head moves on (winding), or the room gets bigger
bool anDigStep() {
  if (!anFounding && an.dug >= anDigGoal()) return false;
  AnPlan& p = an.plan;
  if (!p.on && !anNewPlan()) return false;
  if (p.room >= 0) {
    AnRoom& r = an.room[p.room];
    float dx = r.x - p.hx, dy = (r.y - p.hy) * 3.0f;
    if ((fabsf(dx) < r.r * 0.8f && fabsf(r.y + (p.hx - r.x) * anTilt(r) - p.hy) < 3) || dx * dx + dy * dy < 25 || p.steps > 500) {   // reached the gallery line: it grows from there
      r.prog = min(8, r.prog + 1);
      anDigRoom(r, r.prog);
      if (r.prog >= 8) p.on = 0;
      return true;
    }
  } else if (p.steps > 40 || (fabsf(p.tx - p.hx) < 3 && fabsf(p.ty - p.hy) < 3)) {   // side tunnel: ends in a small bulb
    anDisk(p.hx, p.hy, 2.2f, 1.8f); p.on = 0; return true;
  }
  // the head turns a little toward the goal, and wanders a little (winding tunnels)
  float want = atan2f(p.ty - p.hy, p.tx - p.hx), d = want - p.ha;
  while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI;
  p.ha += d * 0.3f + (anRf() - 0.5f) * 0.45f;   // straighter than v11.8 (real shafts run fairly straight)
  float nx = p.hx + cosf(p.ha) * 1.7f, ny = p.hy + sinf(p.ha) * 1.7f;
  if (nx < 5 || nx > AF_W - 5 || ny < AF_SKY + 3 || ny > AF_H - 5) { p.ha = want; nx = p.hx + cosf(want) * 1.7f; ny = p.hy + sinf(want) * 1.7f; }
  p.hx = nx; p.hy = ny; p.steps++;
  float w = 1.45f + 0.3f * sinf(p.steps * 0.37f + p.room * 1.3f) + anRf() * 0.25f;   // about 3 px wide, not the same everywhere
  anDisk(p.hx, p.hy, w, w);
  return true;
}
// digging while nobody looked: about 10 pixels an hour per worker (and a little more)
void anCatchUp() {
  uint32_t n = (uint32_t)nowT();
  if (timeApprox) return;
  if (!an.lastT || an.lastT > n) { an.lastT = n; return; }
  uint32_t hrs10 = (n - an.lastT) / 360;   // tenths of an hour
  uint32_t px = hrs10 * (10 + an.workers * 10) / 10;
  if (!px) return;
  an.lastT = n;
  uint32_t goal = an.dug + px;
  for (int k = 0; k < 40000 && an.dug < goal; k++) if (!anDigStep()) break;
}
// a new colony: the queen dug a winding shaft and one room, 3 small workers
void anFound() {
  memset(&an, 0, sizeof an);
  an.magic = AN_MAGIC; an.ver = 3; an.rng = esp_random() | 1; an.workers = 3; an.born = 3; an.door = AF_W / 2;
  an.foundT = timeApprox ? 0 : (uint32_t)nowT();
  memset(anBits, 0, AN_BITS);
  an.room[0] = AnRoom{(int16_t)(an.door + 14), (int16_t)(AF_SKY + 30), 22, AK_QUEEN, 0, (uint8_t)anRnd(256)};
  an.nRooms = 1;
  an.plan = AnPlan{(float)an.door, (float)AF_SKY, PI / 2, 0, an.room[0].x, an.room[0].y, 0, 1, 0};
  anFounding = true;
  for (int k = 0; k < 400 && an.plan.on; k++) anDigStep();
  anFounding = false;
  an.dug = 0; an.mound = 30; anDirtyMap = true;
}
// a v11.8 colony into the v11.9 farm: the same tunnels, 10 rows lower (more sky), everything else kept
bool anFromV2(File& f) {
  AnSaveV2* o = (AnSaveV2*)heap_caps_malloc(sizeof(AnSaveV2), MALLOC_CAP_SPIRAM);
  uint8_t* ob = (uint8_t*)heap_caps_malloc(V2_W * V2_H / 8, MALLOC_CAP_SPIRAM);
  bool ok = o && ob && f.read((uint8_t*)o, sizeof(AnSaveV2)) == sizeof(AnSaveV2) && o->magic == AN_MAGIC_V2 && o->nRooms >= 1 && o->nRooms <= 40
            && f.read(ob, V2_W * V2_H / 8) == V2_W * V2_H / 8;
  if (ok) {
    const int dy = AF_SKY - V2_SKY;
    memset(&an, 0, sizeof an); memset(anBits, 0, AN_BITS);
    an.magic = AN_MAGIC; an.ver = 3; an.rng = o->rng; memcpy(an.day, o->day, sizeof an.day);
    an.workers = o->workers; an.streak = o->streak; an.best = o->best; an.nRooms = o->nRooms;
    memcpy(an.brood, o->brood, sizeof an.brood);
    an.dug = o->dug; an.born = o->born; an.lastT = o->lastT; an.mound = o->mound; an.foundT = o->foundT;
    an.plan = o->plan; an.plan.hy += dy; an.plan.ty += dy;
    for (int i = 0; i < an.nRooms; i++) { an.room[i] = o->room[i]; an.room[i].y += dy; }
    an.door = V2_DOOR;
    for (int y = V2_SKY; y < V2_H; y++) for (int x = 0; x < V2_W; x++) {
      int i = y * V2_W + x;
      if (ob[i >> 3] & (1 << (i & 7))) { int j = (y + dy) * AF_W + x; anBits[j >> 3] |= 1 << (j & 7); }
    }
    for (int y = AF_SKY; y < AF_SKY + dy + 1; y++) for (int x = an.door - 1; x <= an.door + 2; x++) { int j = y * AF_W + x; anBits[j >> 3] |= 1 << (j & 7); }   // the door shaft, a bit longer
  }
  if (o) free(o); if (ob) free(ob);
  return ok;
}

// ---------------- save / load ----------------
void anSave() {
  File f = LittleFS.open("/ants.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)&an, sizeof an); f.write(anBits, AN_BITS); f.close();
  anDirtyMap = false; anSavedMs = millis();
}
bool anLoad() {
  if (!anBits) { anBits = (uint8_t*)heap_caps_malloc(AN_BITS, MALLOC_CAP_SPIRAM); if (!anBits) return false; }
  if (anLoaded) return true;
  File f = LittleFS.open("/ants.bin", "r");
  bool ok = false, moved = false;
  if (f && f.size() == sizeof(AnSaveV2) + V2_W * V2_H / 8) { ok = moved = anFromV2(f); }
  else ok = f && f.size() == sizeof an + AN_BITS && f.read((uint8_t*)&an, sizeof an) == sizeof an && an.magic == AN_MAGIC
            && f.read(anBits, AN_BITS) == AN_BITS && an.nRooms >= 1 && an.nRooms <= AN_MAXROOM && an.plan.room < (int)an.nRooms
            && an.door > 4 && an.door < AF_W - 4;
  if (f) f.close();
  if (!ok) anFound();   // no file (or a broken one): a new colony
  if (!ok || moved) anSave();
  anLoaded = true;
  return true;
}
// count the days that ended since last time: food -> eggs, eggs get older, 8 days old -> workers
void antsSync() {
  if (!anLoad()) return;
  anTodayScore();
  if (timeApprox || anSyncedFor == curDay) return;   // clock not right yet: wait, so days are not counted wrong
  time_t n = nowT();
  String yesterday = dayKey(n - 86400);
  bool first = !an.day[0];
  int back = first ? 7 : 30;   // first time: the last 7 days of logs
  bool changed = false;
  if (!first && strcmp(dayKey(n - (time_t)(back + 1) * 86400).c_str(), an.day) > 0 && an.streak) { an.streak = 0; changed = true; }
  for (int d = back; d >= 1; --d) {
    String k = dayKey(n - (time_t)d * 86400);
    if (!first && strcmp(k.c_str(), an.day) <= 0) continue;   // already counted
    std::map<String, Sum> m; bool any = false;
    forEachEvent(k, [&](const Ev& e) { Sum& s = m[e.id]; s.count++; s.sum += e.v; any = true; });
    int done, goals; anScore(m, any, done, goals);
    an.workers += an.brood[AN_HATCH - 1]; an.born += an.brood[AN_HATCH - 1];   // pupae open: new workers
    for (int i = AN_HATCH - 1; i > 0; --i) an.brood[i] = an.brood[i - 1];
    an.brood[0] = done;                                                      // the eggs of that day
    if (goals && done * 2 >= goals) { an.streak++; an.best = max(an.best, an.streak); } else an.streak = 0;
    changed = true;
  }
  if (strcmp(an.day, yesterday.c_str())) { strncpy(an.day, yesterday.c_str(), sizeof an.day - 1); changed = true; }
  if (!an.foundT) an.foundT = (uint32_t)n;
  anCatchUp();
  if (changed || anDirtyMap) anSave();
  anSyncedFor = curDay;
}
uint32_t anBrood() { uint32_t b = anToday; for (int i = 0; i < AN_HATCH; i++) b += an.brood[i]; return b; }
int anNextHatch() { for (int i = AN_HATCH - 1; i >= 0; --i) if (an.brood[i]) return AN_HATCH - i; return anToday ? AN_HATCH + 1 : 0; }   // days
const char* anStage() {
  uint32_t w = an.workers;
  return w < 10 ? "Founding" : w < 40 ? "Young" : w < 120 ? "Growing" : w < 300 ? "Big" : "Super";   // (+ " colony" where it fits)
}

// ---------------- the ants you see (at most 40, each with a job) ----------------
// soil carriers go up to the door, out to the pile, drop the grain and come back in (v11.9)
enum AnJob : uint8_t { AJ_WANDER, AJ_NURSE, AJ_TO_FACE, AJ_DIG, AJ_CARRY, AJ_FOOD, AJ_OUT, AJ_IN, AJ_SURF };
struct AnAnt { float x, y, a; int16_t tx, ty; uint8_t job, home; uint16_t wait; };
std::vector<AnAnt> anAnts;
uint16_t *anDist = nullptr, *anFace = nullptr, *anFoodF = nullptr;   // walking steps to: the door, the digging face, the food room
int anFoodRoom = -1;
uint32_t anStepMs = 0, anDigMs = 0, anFoodMs = 0;
LGFX_Sprite anBg(&spr);       // the farm drawn once (soil, tunnels), only the changed part is drawn again
bool anBgOk = false, anBgNeed = true, anGroundNeed = true;
const int AN_WALK_Y = AF_SKY / 2 - 1;   // the grid row the ants walk on outside (on the ground)
inline bool anWalk(int gx, int gy) {   // can an ant stand on this grid cell? (underground: open; outside: the row on the ground)
  if (gx < 0 || gy < 0 || gx >= AG_W || gy >= AG_H) return false;
  if (gy * 2 < AF_SKY) return gy == AN_WALK_Y;
  return anPx(gx * 2 + 1, gy * 2 + 1) || anPx(gx * 2, gy * 2 + 1);
}
void anField(uint16_t* d, int sx, int sy) {   // steps to (sx, sy) through the tunnels (breadth-first)
  for (int i = 0; i < AG_W * AG_H; i++) d[i] = 0xFFFF;
  static int16_t* q = nullptr; if (!q) q = (int16_t*)heap_caps_malloc(AG_W * AG_H * 2, MALLOC_CAP_SPIRAM);   // (PSRAM: the small fast memory is kept for Wi-Fi / news)
  if (!q) return;
  size_t qn = 0;
  auto push = [&](int x, int y, uint16_t v) { if (!anWalk(x, y)) return; int i = y * AG_W + x; if (d[i] != 0xFFFF) return; d[i] = v; q[qn++] = i; };
  push(sx, sy, 0);
  for (int k = 1; k < 4 && !qn; k++) for (int dy = -k; dy <= k; dy++) for (int dx = -k; dx <= k; dx++) push(sx + dx, sy + dy, 0);
  for (size_t h = 0; h < qn; h++) {
    int i = q[h], x = i % AG_W, y = i / AG_W; uint16_t v = d[i] + 1;
    push(x - 1, y, v); push(x + 1, y, v); push(x, y - 1, v); push(x, y + 1, v);
  }
}
void anFields() {
  for (auto** f : {&anDist, &anFace, &anFoodF}) if (!*f) *f = (uint16_t*)heap_caps_malloc(AG_W * AG_H * 2, MALLOC_CAP_SPIRAM);
  if (!anDist || !anFace || !anFoodF) return;
  anField(anDist, an.door / 2, AN_WALK_Y);
  // the face: where the tunnel head (or the room being dug) is
  bool dig = an.dug < anDigGoal() && (an.plan.on || anNewPlan());
  if (dig) anField(anFace, (int)an.plan.hx / 2, (int)an.plan.hy / 2); else for (int i = 0; i < AG_W * AG_H; i++) anFace[i] = 0xFFFF;
  anFoodRoom = -1;
  for (int i = 0; i < an.nRooms; i++) if (an.room[i].kind == AK_FOOD && an.room[i].prog >= 8) { anFoodRoom = i; break; }
  if (anFoodRoom < 0) anFoodRoom = 0;   // no food room yet: to the queen
  anField(anFoodF, an.room[anFoodRoom].x / 2, (an.room[anFoodRoom].y + 2) / 2);
}
bool anCanDig() { return anFace && anFace[AN_WALK_Y * AG_W + an.door / 2] != 0xFFFF && an.plan.on && an.dug < anDigGoal(); }
int anNurseRoom() {   // the brood lives in a brood room (the queen's room at first)
  for (int i = 0; i < an.nRooms; i++) if (an.room[i].kind == AK_BROOD && an.room[i].prog >= 8) return i;
  return 0;
}
int anPileHalf() { return (min(14, 2 + (int)sqrtf((float)an.mound) / 3) * 3 + 6) / 2; }
int anPileX() { return an.door + 12 + anPileHalf(); }   // the middle of the soil pile (right of the door)
void anPlace(AnAnt& a, int gx, int gy) { a.x = gx * 2 + 1; a.y = gy * 2 + 1; a.tx = gx; a.ty = gy; }
void anSpawnAnts() {
  anAnts.clear();
  int n = min(40, (int)an.workers);
  std::vector<int> open; for (int gy = AF_SKY / 2; gy < AG_H; gy++) for (int gx = 0; gx < AG_W; gx++) if (anWalk(gx, gy)) open.push_back(gy * AG_W + gx);
  if (open.empty()) return;
  int nr = anNurseRoom();
  for (int k = 0; k < n; k++) {
    AnAnt a{}; a.a = anRf() * 2 * PI;
    a.job = k % 4 == 1 ? AJ_NURSE : (k < 3 ? AJ_TO_FACE : (k % 5 == 3 ? AJ_SURF : AJ_WANDER)); a.home = nr;   // v11.9: some walk about outside
    int i = open[anRnd(open.size())];
    if (a.job == AJ_NURSE) { AnRoom& r = an.room[nr]; int gx = r.x / 2 + anRnd(7) - 3, gy = (r.y + 1) / 2; if (anWalk(gx, gy)) i = gy * AG_W + gx; }
    if (a.job == AJ_SURF) i = AN_WALK_Y * AG_W + anRnd(AG_W);
    anPlace(a, i % AG_W, i / AG_W);
    anAnts.push_back(a);
  }
}
// choose the next grid cell for one ant
void anNext(AnAnt& a) {
  const int D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
  auto ok = [&](int k) { int nx = a.tx + D[k][0], ny = a.ty + D[k][1]; if (!anWalk(nx, ny)) return false;
    return k < 4 || (anWalk(a.tx + D[k][0], a.ty) && anWalk(a.tx, a.ty + D[k][1])); };   // no cutting corners
  if (a.job == AJ_SURF) {   // outside, looking for food: walk along the sand, turn now and then, stop to look
    if (anRnd(14) == 0) { a.wait = 6 + anRnd(30); return; }
    int dir = cosf(a.a) >= 0 ? 1 : -1; if (anRnd(10) == 0) dir = -dir;
    if (!anWalk(a.tx + dir, a.ty)) dir = -dir;
    if (anWalk(a.tx + dir, a.ty)) a.tx += dir;
    return;
  }
  if (a.job == AJ_OUT) {   // outside: walk on the ground to the pile, drop the grain there
    int px = anPileX() / 2 + (int)(anHash((int)(uintptr_t)&a, 3) % 5) - 2;
    if (a.tx != px && anWalk(a.tx + (px > a.tx ? 1 : -1), a.ty)) { a.tx += px > a.tx ? 1 : -1; return; }
    an.mound++; anGroundNeed = true; a.job = AJ_IN; a.wait = 4 + anRnd(8); return;
  }
  uint16_t* f = a.job == AJ_TO_FACE ? anFace : (a.job == AJ_CARRY || a.job == AJ_IN) ? anDist : a.job == AJ_FOOD ? anFoodF : nullptr;
  if (f) {
    int best = -1; uint16_t bv = f[a.ty * AG_W + a.tx]; int s = anRnd(8);
    if (a.job == AJ_IN && bv == 0) { a.job = anRnd(3) ? AJ_TO_FACE : AJ_WANDER; return; }   // back in: to work
    if (a.job == AJ_IN || a.job == AJ_CARRY) {   // on the way in / out: the door first (then down the steps to the face)
      for (int j = 0; j < 8; j++) { int k = (j + s) & 7; if (!ok(k)) continue; uint16_t v = f[(a.ty + D[k][1]) * AG_W + a.tx + D[k][0]]; if (v < bv) { bv = v; best = k; } }
    } else
    for (int j = 0; j < 8; j++) { int k = (j + s) & 7; if (!ok(k)) continue; uint16_t v = f[(a.ty + D[k][1]) * AG_W + a.tx + D[k][0]]; if (v < bv) { bv = v; best = k; } }
    if (best >= 0) { a.tx += D[best][0]; a.ty += D[best][1]; return; }
    // at the end of the way
    if (a.job == AJ_TO_FACE) { a.job = AJ_DIG; a.wait = 0; }
    else if (a.job == AJ_CARRY) { a.job = AJ_OUT; }                      // at the door: go out to the pile
    else if (a.job == AJ_FOOD) { a.job = AJ_WANDER; a.wait = 10 + anRnd(20); }
    return;
  }
  if (a.job == AJ_DIG) return;
  // wander: mostly straight on, sometimes a turn, sometimes a stop (they look around with the feelers)
  if (anRnd(18) == 0) { a.wait = 5 + anRnd(25); return; }
  int best = -1; float bs = -9;
  for (int k = 0; k < 8; k++) {
    if (!ok(k)) continue;
    int nx = a.tx + D[k][0], ny = a.ty + D[k][1];
    if (a.job == AJ_NURSE) { AnRoom& r = an.room[a.home]; if (abs(nx * 2 - r.x) > r.r + 2 || abs(ny * 2 - r.y) > r.r / 2 + 4) continue; }   // nurses stay with the brood
    if (a.job == AJ_WANDER && ny * 2 < AF_SKY) continue;   // the ground is for the soil carriers and food bringers
    float da = atan2f(D[k][1], D[k][0]) - a.a; while (da > PI) da -= 2 * PI; while (da < -PI) da += 2 * PI;
    float sc = cosf(da) * 1.2f + anRf();
    if (sc > bs) { bs = sc; best = k; }
  }
  if (best >= 0) { a.tx += D[best][0]; a.ty += D[best][1]; }
}
// move every ant a little (called ~16 times a second while the page is open)
void anStep() {
  bool canDig = anCanDig();
  int diggers = 0; for (auto& a : anAnts) diggers += a.job == AJ_TO_FACE || a.job == AJ_DIG;
  for (auto& a : anAnts) {
    if ((a.job == AJ_TO_FACE || a.job == AJ_DIG) && !canDig) a.job = AJ_WANDER;
    if (a.job == AJ_WANDER && canDig && diggers < 4 && anRnd(60) == 0) { a.job = AJ_TO_FACE; diggers++; }
    if (a.wait) { a.wait--; if (anRnd(4) == 0) a.a += (anRf() - 0.5f) * 0.8f; continue; }
    if (a.job == AJ_DIG) { a.a += (anRf() - 0.5f) * 0.5f; continue; }   // head down at the face, the jaws working
    float gx = a.tx * 2 + 1, gy = a.ty * 2 + 1, dx = gx - a.x, dy = gy - a.y, d = sqrtf(dx * dx + dy * dy);
    const float SPEED = 1.1f;
    if (d < SPEED) { a.x = gx; a.y = gy; anNext(a); continue; }
    float want = atan2f(dy, dx), da = want - a.a; while (da > PI) da -= 2 * PI; while (da < -PI) da += 2 * PI;
    a.a += da * 0.5f;   // turn smoothly
    a.x += dx / d * SPEED; a.y += dy / d * SPEED;
  }
  // food: while goals are reached today, now and then an ant brings a crumb down from outside
  if (anToday > 0 && millis() - anFoodMs > (uint32_t)(9000 / anToday) && anAnts.size() > 3) {
    anFoodMs = millis();
    for (auto& a : anAnts) if (a.job == AJ_WANDER) { a.job = AJ_FOOD; anPlace(a, an.door / 2 + (anRnd(2) ? 14 : -14), AN_WALK_Y); a.a = anRnd(2) ? PI : 0; a.wait = 0; break; }
  }
}
// the face is dug when the diggers have worked long enough (faster with more workers)
void anDigTick() {
  if (!anCanDig()) return;
  int at = 0; for (auto& a : anAnts) at += a.job == AJ_DIG;
  if (!at) return;
  int need = max(200, 1000 - min(800, (int)an.workers * 6)) / at;
  if (millis() - anDigMs < (uint32_t)need) return;
  anDigMs = millis();
  if (!anDigStep()) return;
  for (auto& a : anAnts) if (a.job == AJ_DIG) { a.job = AJ_CARRY; break; }   // one of them takes the soil out
  an.lastT = (uint32_t)nowT();
  anFields();
  if (millis() - anSavedMs > 60000) anSave();
}

// ---------------- drawing ----------------
struct AnBox { int x, y, w, h; };
AnBox anScene() { return land() ? AnBox{0, AN_BAR, AF_W, H - AN_BAR} : AnBox{0, AN_BAR, AF_W, min(AF_H, H - AN_BAR)}; }   // tall: all of it; wide: a window (scrolls)
bool anNight() { time_t n = nowT(); struct tm tm; localtime_r(&n, &tm); float h = timeApprox ? 12 : tm.tm_hour + tm.tm_min / 60.0f; return h < 6 || h >= 19; }
// smooth noise (0..1) for patches in the soil
float anNoise(float x, float y, int s) {
  int ix = (int)floorf(x), iy = (int)floorf(y); float fx = x - ix, fy = y - iy;
  auto v = [&](int a, int b) { return (anHash(a * 7 + s, b * 13 - s) & 1023) / 1023.0f; };
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  float a = v(ix, iy) + (v(ix + 1, iy) - v(ix, iy)) * fx, b = v(ix, iy + 1) + (v(ix + 1, iy + 1) - v(ix, iy + 1)) * fx;
  return a + (b - a) * fy;
}
// one pixel of the farm: sky, soil (patches, layers, grains, pebbles, roots, darker packed walls) or tunnel
uint32_t anColor(int x, int y, bool night) {
  uint32_t h = anHash(x, y), c;
  if (y < AF_SKY) {
    uint32_t t = night ? 0x1C2028 : 0xDCE0E2, b = night ? 0x2A2E38 : 0xC4C8CB;   // behind the glass: a light grey wall (dark at night)
    c = blend(t, b, (float)y / AF_SKY);
  } else if (!anPx(x, y)) {
    float n1 = anNoise(x / 30.0f, y / 24.0f, 1);
    c = blend(0xCC5E28, 0xB85022, n1 * 0.7f);                                    // fine orange sand, a very soft change
    c = blend(c, 0x8A3414, (float)(y - AF_SKY) / (AF_H - AF_SKY) * 0.22f);       // deeper = a little darker
    int g = h % 9; if (g == 0) c = blend(c, 0xFFD2A0, 0.45f); else if (g == 1) c = blend(c, 0xF0A070, 0.3f); else if (g == 2) c = blend(c, 0x6A2410, 0.25f);   // light and dark grains
    if (y == AF_SKY && (h & 3)) c = blend(c, 0xE8905A, 0.3f);                    // the top edge of the sand
    int near = 0;   // packed wall: soil right next to a tunnel is darker
    for (int k = 1; k <= 2 && !near; k++) if ((y - k >= AF_SKY && anPx(x, y - k)) || anPx(x, y + k) || anPx(x - k, y) || anPx(x + k, y)) near = k;
    if (near == 1) c = blend(c, 0x3A1608, 0.42f); else if (near == 2) c = blend(c, 0x3A1608, 0.18f);
  } else {
    bool floor = !anPx(x, y + 1), ceil = !anPx(x, y - 1);
    c = (h & 7) ? 0x3E1C0C : 0x331608;                                             // dark tunnel
    if (floor) c = (h & 3) ? 0x5C2E14 : 0x6E3A1A;                                  // loose sand on the floor
    else if (ceil) c = 0x2A1206;                                                   // shadow under the ceiling
  }
  // the glass: two faint shine bands across the farm
  int d = (x + y * 2) % 520; if ((d > 60 && d < 74) || (d > 84 && d < 88)) c = blend(c, 0xFFFFFF, night ? 0.04f : 0.07f);
  return c;
}
void anDrawRect(int x0, int y0, int x1, int y1) {
  bool night = anNight();
  x0 = max(0, x0); y0 = max(0, y0); x1 = min(AF_W - 1, x1); y1 = min(AF_H - 1, y1);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) anBg.drawPixel(x, y, C(anColor(x, y, night)));
}
void anDrawGround() {   // sky, grass, a little plant, the door and the pile of soil the ants carried out
  bool night = anNight();
  anDrawRect(0, 0, AF_W - 1, AF_SKY - 1);
  int hgt = min(14, 2 + (int)sqrtf((float)an.mound) / 3), half = anPileHalf(), cx = anPileX();
  for (int y = 0; y < hgt; y++) {
    int hw = (int)(half * sqrtf(1.0f - (float)y / hgt));
    for (int x = cx - hw; x <= cx + hw; x++) { uint32_t h = anHash(x, y + 999); anBg.drawPixel(x, AF_SKY - 1 - y, C((h % 5) ? (h % 3 ? 0xC96A36 : 0xB85A2C) : ((x - cx) < -hw / 3 && y > hgt / 3 ? 0xE8A070 : 0xE08A50))); }
  }
  for (int x = 0; x < AF_W; x++) {   // the top of the sand is not a straight line: loose grains on it
    if (abs(x - cx) <= half || abs(x - an.door) <= 2) continue;
    uint32_t h = anHash(x, 7);
    if (h % 3 == 0) anBg.drawPixel(x, AF_SKY - 1, C(0xC86A38));
    if (h % 11 == 0) anBg.drawPixel(x, AF_SKY - 2, C(0xD8804A));
  }
  for (int x = an.door - 2; x <= an.door + 2; x++) anBg.drawPixel(x, AF_SKY - 1, C(0x2A1208));   // the door
  anGroundNeed = false;
}
void anDrawBg() {
  if (!anBgOk) { anBg.setPsram(true); anBg.setColorDepth(16); anBgOk = anBg.createSprite(AF_W, AF_H); anBgNeed = true; }
  if (!anBgOk) return;
  if (anBgNeed) { anDrawRect(0, AF_SKY, AF_W - 1, AF_H - 1); anRx1 = -1; anGroundNeed = true; }
  else if (anRx1 >= 0) { anDrawRect(anRx0 - 3, anRy0 - 3, anRx1 + 3, anRy1 + 3); anRx1 = -1; }   // only what the ants just dug
  if (anGroundNeed) anDrawGround();
  anRx0 = AF_W; anRy0 = AF_H; anBgNeed = false;
}
// an ant seen from above: abdomen, thorax, head, 6 legs that move, 2 feelers. s = size (the queen is bigger)
void anDrawAnt(float x, float y, float a, float s, bool moving, int carry, int ox, int oy) {
  float c = cosf(a), sn = sinf(a);
  auto P = [&](float d, float side, int& px, int& py) { px = ox + (int)roundf(x + (c * d - sn * side) * s); py = oy + (int)roundf(y + (sn * d + c * side) * s); };
  uint16_t body = C(0x0C0604), hi = C(0x6A4636);
  int px, py, qx, qy;
  int step = moving ? (int)((millis() / 90 + (int)x) & 1) : 0;
  for (int k = -1; k <= 1; k++) for (int side = -1; side <= 1; side += 2) {   // legs: front, middle, back
    float sw = (((k + 2 + (side > 0)) & 1) == step) ? 0.9f : -0.9f;
    P(k * 0.9f, 0, px, py); P(k * 1.3f + (moving ? sw : 0), side * 2.2f, qx, qy);
    spr.drawLine(px, py, qx, qy, body);
  }
  P(2.8f, 0, px, py); P(4.4f, 1.6f, qx, qy); spr.drawLine(px, py, qx, qy, body); P(4.4f, -1.6f, qx, qy); spr.drawLine(px, py, qx, qy, body);   // feelers
  P(-2.6f, 0, px, py); spr.fillEllipse(px, py, max(1, (int)(1.7f * s)), max(1, (int)(1.35f * s)), body);   // abdomen
  P(-2.9f, -0.6f, qx, qy); spr.drawPixel(qx, qy, hi);                                                     // shine
  P(0, 0, px, py); spr.fillRect(px - (s > 1.2f ? 1 : 0), py - (s > 1.2f ? 1 : 0), s > 1.2f ? 3 : 2, s > 1.2f ? 3 : 2, body);   // thorax
  P(2.1f, 0, px, py); spr.fillRect(px, py, 2, 2, body);                                                   // head
  if (carry) { P(3.7f, 0, px, py); spr.fillRect(px, py - 1, 2, 2, C(carry == 1 ? 0xE08A50 : 0xF2C94C)); }   // soil / food in the jaws
}
// the floor of a gallery at x: the lowest open pixel there (-1 = not dug there)
int anFloorAt(const AnRoom& r, int x) {
  int y = (int)ceilf(anFloorY(r, x)) + 2;
  for (int k = 0; k < 10 && y > AF_SKY; k++, y--) if (anPx(x, y)) return y;
  return -1;
}
// things lying in a row on the floor of galleries of one kind (brood or food), spread over all of them, the same every time
template <typename F> void anAlongFloors(uint8_t kind, int n, int ox, int oy, F draw) {
  int rooms[AN_MAXROOM], nr = 0;
  for (int i = 0; i < an.nRooms; i++) if (an.room[i].prog >= 8 && (an.room[i].kind == kind || (kind == AK_BROOD && an.room[i].kind == AK_QUEEN))) rooms[nr++] = i;
  if (!nr || n <= 0) return;
  int per = (n + nr - 1) / nr, k = 0;
  for (int q = 0; q < nr && k < n; q++) {
    AnRoom& r = an.room[rooms[q]];
    int len = 2 * r.r - 6, start = r.x - r.r + 3 + (int)(anHash(r.seed, 5) % 4);
    for (int m = 0; m < per && k < n; m++, k++) {
      int x = start + (len > 0 ? (m * 3) % max(3, len) : 0);
      if (r.kind == AK_QUEEN && abs(x - r.x) < 5) x += 10;   // not under the queen
      int y = anFloorAt(r, x); if (y < 0) continue;
      draw(ox + x, oy + y - (m * 3 / max(3, len)), k);        // a second layer when the row is full
    }
  }
}
void anDrawBrood(int ox, int oy) {
  // in rows on the floor of the brood galleries, like in a real ant farm: eggs (small white), larvae (cream), pupae (tan cocoons)
  int nE = (int)anToday + an.brood[0] + an.brood[1] + an.brood[2], nL = an.brood[3] + an.brood[4] + an.brood[5], nP = an.brood[6] + an.brood[7];
  int n = min(160, nE + nL + nP) , shownE = min(nE, n), shownL = min(nL, n - shownE);
  anAlongFloors(AK_BROOD, n, ox, oy, [&](int X, int Y, int k) {
    if (k < shownE) { spr.drawPixel(X, Y, C(0xFFFFFF)); spr.drawPixel(X + 1, Y, C(0xEDE6D8)); }
    else if (k < shownE + shownL) { spr.fillRect(X, Y - 1, 2, 2, C(0xF6EEDC)); spr.drawPixel(X + 2, Y, C(0xE8DCC0)); }
    else { spr.fillRect(X, Y - 1, 3, 2, C(0xE2C9A0)); spr.drawPixel(X, Y - 1, C(0xB89868)); }
  });
}
void anDrawFood(int ox, int oy) {   // seeds and crumbs in the food galleries (more on a good day)
  anAlongFloors(AK_FOOD, min(90, anToday * 10 + 8), ox, oy, [&](int X, int Y, int k) {
    uint32_t col = k % 4 == 0 ? 0x9BD35A : (k % 4 == 1 ? 0xFFF1A8 : 0xF2C94C);
    spr.fillRect(X, Y, 2, 1, C(col));
  });
}
void drawAntsBar() {   // the thin bar on top: [< EXIT]  ANT COLONY ........ DAY 5
  spr.fillRect(0, 0, W, AN_BAR, C(themeHud ? PAPER : HDRBG));
  uint32_t fg = themeHud ? INK : HDRFG;
  if (themeHud) hudShape(4, 3, 58, AN_BAR - 6, 5, CARD, INK); else spr.fillRoundRect(4, 3, 58, AN_BAR - 6, 6, C(blend(HDRBG, HDRFG, 0.18f)));
  spr.fillTriangle(10, AN_BAR / 2, 16, AN_BAR / 2 - 5, 16, AN_BAR / 2 + 5, C(fg));
  txt(FS, hudUp("Exit"), 19, AN_BAR / 2, fg, D_ML);
  navAdd(4, 3, 58, AN_BAR - 6, NK_BACK);
  uint32_t days = an.foundT && !timeApprox && (uint32_t)nowT() >= an.foundT ? ((uint32_t)nowT() - an.foundT) / 86400 + 1 : 1;
  String d = hudUp("Day " + String(days));
  txt(FS, d, W - 6, AN_BAR / 2, themeHud ? SOFT : HDRSOFT, D_MR);
  spr.setFont(pickFont(FS, d)); int dw = spr.textWidth(d);
  txt(FB, fitText(FB, hudUp("Ant colony"), W - 72 - dw - 12), 70, AN_BAR / 2, themeHud ? HUDB : HDRFG, D_ML);
  if (themeHud) hudTicks(AN_BAR - 1, true, LINE);
}
bool antExitHit(int x, int y) { return y < AN_BAR && x < 66; }
void drawAnts() {
  antsSync();
  AnBox S = anScene();
  anScrollMax = max(0, AF_H - S.h); anScroll = constrain(anScroll, 0, anScrollMax);
  if (anBgNeed || !anBgOk || anRx1 >= 0 || anGroundNeed) anDrawBg();
  int oy = S.y - anScroll;   // where row 0 of the farm is on the screen
  spr.setClipRect(S.x, S.y, S.w, S.h);
  if (anBgOk) anBg.pushSprite(&spr, S.x, oy); else spr.fillRect(S.x, S.y, S.w, S.h, C(0xB9562A));
  anDrawBrood(S.x, oy); anDrawFood(S.x, oy);
  { AnRoom& q = an.room[0]; float wob = sinf(millis() / 900.0f) * 0.25f;   // the queen: big, slow, standing on the floor of her room
    int qx = q.x - 2, qy = anFloorAt(q, qx); if (qy < 0) qy = q.y; else qy -= 2;
    anDrawAnt(qx, qy, PI + wob, 1.7f, false, 0, S.x, oy); }
  for (auto& a : anAnts) anDrawAnt(a.x, a.y, a.a, 1.15f, !a.wait && a.job != AJ_DIG, a.job == AJ_CARRY || a.job == AJ_OUT ? 1 : a.job == AJ_FOOD ? 2 : 0, S.x, oy);
  // the numbers, small, in the sky (left of the door) so the farm keeps all the room
  uint32_t brood = anBrood();
  if (!land()) {
    uint32_t tc = anNight() ? 0xDDE6FF : 0x10304A;
    txt(&fonts::Font0, "ANTS " + String(an.workers) + "  BROOD " + String(brood), S.x + 4, oy + 3, tc);
    txt(&fonts::Font0, "STREAK " + String(an.streak) + "  FOOD " + String(anToday) + "/" + String(anGoals), S.x + 4, oy + 12, tc);
  }
  if (anLabels) {   // tap: the room names and a small card
    for (int i = 0; i < an.nRooms; i++) {
      AnRoom& r = an.room[i]; if (r.prog < 8 && r.kind != AK_QUEEN) continue;
      const char* t = r.kind == AK_QUEEN ? "QUEEN" : r.kind == AK_BROOD ? "BROOD" : r.kind == AK_FOOD ? "FOOD" : "ROOM";
      txt(&fonts::Font0, t, S.x + r.x, oy + r.y - r.r / 2 - 6, themeHud ? HUDB : 0xFFFFFF, D_MC);
    }
    int ch = 46, cy = S.y + S.h - ch - 4;
    String l1 = String(anStage()) + (an.workers >= 10 ? " colony" : "");
    int nh = anNextHatch(); String l2 = nh ? "Next ant in " + String(nh) + (nh == 1 ? " day" : " days") : String("Reach a goal: an egg");
    if (timeApprox && !an.day[0]) l2 = "Set the time to grow";
    if (themeHud) hudShape(S.x + 6, cy, S.w - 12, ch, 8, PAPER, INK); else spr.fillRoundRect(S.x + 6, cy, S.w - 12, ch, 10, C(0x000000));
    txt(FB, fitText(FB, hudUp(l1), S.w - 28), S.x + 14, cy + 13, themeHud ? HUDB : 0xFFFFFF, D_ML);
    txt(FS, fitText(FS, hudUp(l2), S.w - 28), S.x + 14, cy + 33, themeHud ? INK : 0xC8C8C8, D_ML);
  }
  if (themeHud) hudBrackets(S.x + 3, S.y + 3, S.w - 6, S.h - 6, INK);
  spr.clearClipRect();
  scrollBar(S.y, S.h, anScroll, anScrollMax);   // wide: drag / stick up-down to see the deep part
  navAdd(S.x + 8, S.y + 30, S.w - 16, min(S.h - 40, 120));   // tap the farm: names on the rooms
  drawAntsBar();
  // wide screen: the numbers on the right
  if (land()) {
    int px = S.w + 4, pw = W - S.w - 8, py = S.y + 2;
    String V[3] = {String(an.workers), String(brood), String(an.streak)};
    const char* K[3] = {"Ants", "Brood", "Streak"};
    for (int i = 0; i < 3; i++) {
      int by = py + i * 38; card(px, by, pw, 34, 8, CARD);
      txt(FS, fitText(FS, hudUp(K[i]), pw - 8), px + 6, by + 1, SOFT);
      txt(themeHud ? (const lgfx::IFont*)&hM : FB, V[i], px + 6, by + 18, themeHud ? HUDB : INK);
    }
    int ly = py + 3 * 38 + 4;
    txt(FS, fitText(FS, hudUp(anStage()), pw), px, ly, themeHud ? HUDB : INK); ly += 18;
    txt(FS, fitText(FS, hudUp("Food " + String(anToday) + "/" + String(anGoals)), pw), px, ly, SOFT); ly += 18;
    int nh = anNextHatch(); if (nh) txt(FS, fitText(FS, hudUp("Ant in " + String(nh) + "d"), pw), px, ly, SOFT);
  }
}

// ---------------- open / close / touch / joystick ----------------
JoyBtn anBtn;   // the stick's button: hold 1 s = back to Games, like every game
void antOpen() {
  antsSync();
  anCatchUp();   // the digging done since the page was closed (also on the same day)
  scr = S_ANTS; dirty = true; joyBtnReset(anBtn); anScroll = 0;
  anFields(); anSpawnAnts(); anBgNeed = true; anDigMs = anFoodMs = millis();
}
void antLeave() {
  if (anLoaded) anSave();
  anAnts.clear(); anAnts.shrink_to_fit();
  if (anBgOk) { anBg.deleteSprite(); anBgOk = false; }
  for (auto** f : {&anDist, &anFace, &anFoodF}) if (*f) { free(*f); *f = nullptr; }
  scr = S_GAMES; dirty = true;
}
void antsTask() {   // from loop(): the ants move while the page is open and the screen is on
  if (joyOk) {
    if (pw == P_OFF) joyBtnReset(anBtn);   // a press that woke the screen does not count
    else if (joyBtnRead(anBtn) == 2) { antLeave(); joyNavReset(); return; }
  }
  if (pw != P_ON || scr != S_ANTS) return;
  if (millis() - anStepMs < 60) return;
  anStepMs = millis();
  anStep(); anDigTick();
  dirty = true;
}
void antsTap(int x, int y) {
  if (antExitHit(x, y)) { antLeave(); return; }
  AnBox S = anScene();
  if (hitR(x, y, S.x, S.y, S.w, S.h)) { anLabels = !anLabels; dirty = true; }
}
String antsSub() {
  if (!anLoad()) return "";
  return String(an.workers) + " ants";
}
