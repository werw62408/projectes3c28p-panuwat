#pragma once
// ============================================================================
//  Ant Colony (v11.8, takes the place of Habit Garden): an ant farm that grows from the goals you reach in Log.
//    - it starts with a queen and 3 small workers in a little room under the ground
//    - every goal reached on a day = food = the queen lays 1 egg.
//      egg (3 days) -> larva (3 days) -> pupa (2 days) -> a new worker: 8 days, like real small ants
//    - more workers dig more: winding tunnels, side tunnels and flat-floored rooms, like a real ant farm.
//      While the page is open you see them dig, carry the soil up to a pile at the door and bring food down;
//      while it is closed they go on digging, and the new tunnels are there when you come back.
//    - the "colony brain" picks where the next room goes and when to dig a side tunnel;
//      each ant follows simple rules (walk the tunnels, dig at the face, carry soil up, bring food, look after the brood)
//    - streak = days in a row with at least half of the goals reached (like the Garden)
//  Nothing is ever taken away: the colony only grows. The old Garden numbers stay in Preferences (not used).
//  Saved in LittleFS /ants.bin (a small header + 1 bit per pixel of the ground), written at most once a minute.
//  Included from SomudTick.ino.
// ============================================================================

#define HAS_ANTS 1
const int AF_W = 240, AF_H = 144, AF_SKY = 14;    // the farm: 240 x 144 pixels, rows 0..13 above the ground
const int AG_W = AF_W / 2, AG_H = AF_H / 2;       // the ants walk on a 2 px grid
const int AN_MAXROOM = 40, AN_HATCH = 8;
const int AN_DOOR = 80;                           // the entrance (x) on the ground
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
};
const uint32_t AN_MAGIC = 0x414E5432;   // "ANT2"
const int AN_BITS = AF_W * AF_H / 8;
AnSave an;
uint8_t* anBits = nullptr;              // 1 = dug (open)
bool anLoaded = false, anDirtyMap = false, anLabels = false, anFounding = false;
String anSyncedFor;
uint32_t anSavedMs = 0;
int anToday = 0, anGoals = 0;           // goals reached today (the food / eggs of today)
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
// a room is 4 overlapping flat ovals (wider than tall, flat floor): the shape comes from its seed
void anLobe(const AnRoom& r, int i, float& x, float& y, float& rx, float& ry) {
  uint32_t h = anHash(r.seed * 7 + i, r.x);
  x = r.x + ((h & 255) / 255.0f - 0.5f) * r.r * (i ? 1.1f : 0.2f);
  y = r.y + (((h >> 8) & 255) / 255.0f - 0.5f) * r.r * 0.25f;
  rx = r.r * (0.55f + ((h >> 16) & 255) / 255.0f * 0.35f) * (i ? 0.85f : 1.0f); ry = rx * 0.5f;
  y -= ry * 0.3f;   // all lobes sit on about the same floor
}
bool anRoomFree(int x, int y, int r) {
  if (x - r < 8 || x + r > AF_W - 8 || y - r / 2 < AF_SKY + 14 || y + r / 2 > AF_H - 8) return false;
  for (int i = 0; i < an.nRooms; i++) {
    AnRoom& o = an.room[i];
    if (abs(o.x - x) < o.r + r + 10 && abs(o.y - y) < (o.r + r) / 2 + 12) return false;
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
    int r = t < 90 ? 11 + anRnd(8) : 7 + anRnd(5);   // no room for a big one: a smaller one
    int deepest = min(AF_H - 10, AF_SKY + 40 + an.nRooms * 10);
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
    float dx = r.x - p.hx, dy = (r.y - p.hy) * 1.8f;
    if (dx * dx + dy * dy < (r.r * 0.6f) * (r.r * 0.6f) || p.steps > 500) {   // reached: the room grows from the middle
      r.prog = min(8, r.prog + 1);
      for (int i = 0; i < 4; i++) { float x, y, rx, ry; anLobe(r, i, x, y, rx, ry); anDisk(x, y, rx * r.prog / 8, max(1.5f, ry * r.prog / 8)); }
      if (r.prog >= 8) p.on = 0;
      return true;
    }
  } else if (p.steps > 40 || (fabsf(p.tx - p.hx) < 3 && fabsf(p.ty - p.hy) < 3)) {   // side tunnel: ends in a small bulb
    anDisk(p.hx, p.hy, 3.2f, 2.4f); p.on = 0; return true;
  }
  // the head turns a little toward the goal, and wanders a little (winding tunnels)
  float want = atan2f(p.ty - p.hy, p.tx - p.hx), d = want - p.ha;
  while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI;
  p.ha += d * 0.22f + (anRf() - 0.5f) * 0.9f;
  float nx = p.hx + cosf(p.ha) * 1.7f, ny = p.hy + sinf(p.ha) * 1.7f;
  if (nx < 5 || nx > AF_W - 5 || ny < AF_SKY + 3 || ny > AF_H - 5) { p.ha = want; nx = p.hx + cosf(want) * 1.7f; ny = p.hy + sinf(want) * 1.7f; }
  p.hx = nx; p.hy = ny; p.steps++;
  float w = 2.1f + 0.6f * sinf(p.steps * 0.37f + p.room * 1.3f) + anRf() * 0.4f;   // not the same width everywhere
  anDisk(p.hx, p.hy, w, w * 0.9f);
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
  an.magic = AN_MAGIC; an.ver = 2; an.rng = esp_random() | 1; an.workers = 3; an.born = 3;
  an.foundT = timeApprox ? 0 : (uint32_t)nowT();
  memset(anBits, 0, AN_BITS);
  an.room[0] = AnRoom{(int16_t)(AN_DOOR + 16), (int16_t)(AF_SKY + 32), 13, AK_QUEEN, 0, (uint8_t)anRnd(256)};
  an.nRooms = 1;
  an.plan = AnPlan{(float)AN_DOOR, (float)AF_SKY, PI / 2, 0, an.room[0].x, an.room[0].y, 0, 1, 0};
  anFounding = true;
  for (int k = 0; k < 400 && an.plan.on; k++) anDigStep();
  anFounding = false;
  an.dug = 0; an.mound = 30; anDirtyMap = true;
}

// ---------------- save / load ----------------
void anSave() {
  File f = LittleFS.open("/ants.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)&an, sizeof an); f.write(anBits, AN_BITS); f.close();
  anDirtyMap = false; anSavedMs = millis();
}
bool anLoad() {
  if (!anBits) { anBits = (uint8_t*)heap_caps_malloc(AN_BITS, MALLOC_CAP_8BIT); if (!anBits) return false; }
  if (anLoaded) return true;
  File f = LittleFS.open("/ants.bin", "r");
  bool ok = f && f.size() == sizeof an + AN_BITS && f.read((uint8_t*)&an, sizeof an) == sizeof an && an.magic == AN_MAGIC
            && f.read(anBits, AN_BITS) == AN_BITS && an.nRooms >= 1 && an.nRooms <= AN_MAXROOM && an.plan.room < (int)an.nRooms;
  if (f) f.close();
  if (!ok) { anFound(); anSave(); }   // no file (or a broken one): a new colony
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
enum AnJob : uint8_t { AJ_WANDER, AJ_NURSE, AJ_TO_FACE, AJ_DIG, AJ_CARRY, AJ_FOOD };
struct AnAnt { float x, y, a; int16_t tx, ty; uint8_t job, home; uint16_t wait; };
std::vector<AnAnt> anAnts;
uint16_t *anDist = nullptr, *anFace = nullptr, *anFoodF = nullptr;   // walking steps to: the door, the digging face, the food room
int anFoodRoom = -1;
uint32_t anStepMs = 0, anDigMs = 0, anFoodMs = 0;
LGFX_Sprite anBg(&spr);       // the farm drawn once (soil, tunnels), only the changed part is drawn again
bool anBgOk = false, anBgNeed = true;
inline bool anWalk(int gx, int gy) {   // can an ant stand on this grid cell? (underground: open; on the ground: the row on top)
  if (gx < 0 || gy < 0 || gx >= AG_W || gy >= AG_H) return false;
  if (gy * 2 < AF_SKY) return gy == AF_SKY / 2 - 1;
  return anPx(gx * 2 + 1, gy * 2 + 1) || anPx(gx * 2, gy * 2 + 1);
}
void anField(uint16_t* d, int sx, int sy) {   // steps to (sx, sy) through the tunnels (breadth-first)
  for (int i = 0; i < AG_W * AG_H; i++) d[i] = 0xFFFF;
  static std::vector<int16_t> q; q.clear(); q.reserve(AG_W * AG_H);
  auto push = [&](int x, int y, uint16_t v) { if (!anWalk(x, y)) return; int i = y * AG_W + x; if (d[i] != 0xFFFF) return; d[i] = v; q.push_back(i); };
  push(sx, sy, 0);
  for (int k = 1; k < 4 && q.empty(); k++) for (int dy = -k; dy <= k; dy++) for (int dx = -k; dx <= k; dx++) push(sx + dx, sy + dy, 0);
  for (size_t h = 0; h < q.size(); h++) {
    int i = q[h], x = i % AG_W, y = i / AG_W; uint16_t v = d[i] + 1;
    push(x - 1, y, v); push(x + 1, y, v); push(x, y - 1, v); push(x, y + 1, v);
  }
}
void anFields() {
  for (auto** f : {&anDist, &anFace, &anFoodF}) if (!*f) *f = (uint16_t*)heap_caps_malloc(AG_W * AG_H * 2, MALLOC_CAP_8BIT);
  if (!anDist || !anFace || !anFoodF) return;
  anField(anDist, AN_DOOR / 2, AF_SKY / 2 - 1);
  // the face: where the tunnel head (or the room being dug) is
  bool dig = an.dug < anDigGoal() && (an.plan.on || anNewPlan());
  if (dig) anField(anFace, (int)an.plan.hx / 2, (int)an.plan.hy / 2); else for (int i = 0; i < AG_W * AG_H; i++) anFace[i] = 0xFFFF;
  anFoodRoom = -1;
  for (int i = 0; i < an.nRooms; i++) if (an.room[i].kind == AK_FOOD && an.room[i].prog >= 8) { anFoodRoom = i; break; }
  if (anFoodRoom < 0) anFoodRoom = 0;   // no food room yet: to the queen
  anField(anFoodF, an.room[anFoodRoom].x / 2, (an.room[anFoodRoom].y + 2) / 2);
}
bool anCanDig() { return anFace && anFace[(AF_SKY / 2 - 1) * AG_W + AN_DOOR / 2] != 0xFFFF && an.plan.on && an.dug < anDigGoal(); }
int anNurseRoom() {   // the brood lives in a brood room (the queen's room at first)
  for (int i = 0; i < an.nRooms; i++) if (an.room[i].kind == AK_BROOD && an.room[i].prog >= 8) return i;
  return 0;
}
void anPlace(AnAnt& a, int gx, int gy) { a.x = gx * 2 + 1; a.y = gy * 2 + 1; a.tx = gx; a.ty = gy; }
void anSpawnAnts() {
  anAnts.clear();
  int n = min(40, (int)an.workers);
  std::vector<int> open; for (int gy = AF_SKY / 2; gy < AG_H; gy++) for (int gx = 0; gx < AG_W; gx++) if (anWalk(gx, gy)) open.push_back(gy * AG_W + gx);
  if (open.empty()) return;
  int nr = anNurseRoom();
  for (int k = 0; k < n; k++) {
    AnAnt a{}; a.a = anRf() * 2 * PI;
    a.job = k % 4 == 1 ? AJ_NURSE : (k < 3 ? AJ_TO_FACE : AJ_WANDER); a.home = nr;
    int i = open[anRnd(open.size())];
    if (a.job == AJ_NURSE) { AnRoom& r = an.room[nr]; int gx = r.x / 2 + anRnd(7) - 3, gy = (r.y + 1) / 2; if (anWalk(gx, gy)) i = gy * AG_W + gx; }
    anPlace(a, i % AG_W, i / AG_W);
    anAnts.push_back(a);
  }
}
// choose the next grid cell for one ant
void anNext(AnAnt& a) {
  const int D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
  auto ok = [&](int k) { int nx = a.tx + D[k][0], ny = a.ty + D[k][1]; if (!anWalk(nx, ny)) return false;
    return k < 4 || (anWalk(a.tx + D[k][0], a.ty) && anWalk(a.tx, a.ty + D[k][1])); };   // no cutting corners
  uint16_t* f = a.job == AJ_TO_FACE ? anFace : a.job == AJ_CARRY ? anDist : a.job == AJ_FOOD ? anFoodF : nullptr;
  if (f) {
    int best = -1; uint16_t bv = f[a.ty * AG_W + a.tx]; int s = anRnd(8);
    for (int j = 0; j < 8; j++) { int k = (j + s) & 7; if (!ok(k)) continue; uint16_t v = f[(a.ty + D[k][1]) * AG_W + a.tx + D[k][0]]; if (v < bv) { bv = v; best = k; } }
    if (best >= 0) { a.tx += D[best][0]; a.ty += D[best][1]; return; }
    // at the end of the way
    if (a.job == AJ_TO_FACE) { a.job = AJ_DIG; a.wait = 0; }
    else if (a.job == AJ_CARRY) { an.mound++; anBgNeed = true; a.job = anRnd(3) ? AJ_TO_FACE : AJ_WANDER; }   // soil dropped on the pile
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
    for (auto& a : anAnts) if (a.job == AJ_WANDER) { a.job = AJ_FOOD; anPlace(a, AN_DOOR / 2 + (anRnd(2) ? 8 : -8), AF_SKY / 2 - 1); a.a = anRnd(2) ? PI : 0; a.wait = 0; break; }
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
  for (auto& a : anAnts) if (a.job == AJ_DIG) { a.job = AJ_CARRY; break; }   // one of them takes the soil up
  an.lastT = (uint32_t)nowT();
  anFields();
  if (millis() - anSavedMs > 60000) anSave();
}

// ---------------- drawing ----------------
struct AnBox { int x, y, w, h; };
AnBox anScene() { return {0, HDR_H + 34, AF_W, AF_H}; }   // 240 x 144, the same on a tall and a wide screen
bool anNight() { time_t n = nowT(); struct tm tm; localtime_r(&n, &tm); float h = timeApprox ? 12 : tm.tm_hour + tm.tm_min / 60.0f; return h < 6 || h >= 19; }
// one pixel of the farm: sky, soil (layers, grains, darker packed walls next to tunnels) or tunnel (dark, loose sand on the floor)
uint32_t anColor(int x, int y, bool night) {
  uint32_t h = anHash(x, y);
  if (y < AF_SKY) {
    uint32_t t = night ? 0x0A1024 : 0x86C8F0, b = night ? 0x1C2748 : 0xD6ECF8;
    uint32_t c = blend(t, b, (float)y / AF_SKY);
    if (night && (h % 97) == 0) c = 0xC8D0F0;   // stars
    return c;
  }
  if (!anPx(x, y)) {
    float band = sinf(y * 0.21f + sinf(x * 0.035f) * 1.6f) * 0.5f + 0.5f;   // soft layers, a little wavy
    uint32_t c = blend(0xC45E2C, 0xAE4C22, band * 0.6f);
    if (y < AF_SKY + 6) c = blend(0x7A3A1C, c, (y - AF_SKY) / 6.0f);        // darker top soil
    c = blend(c, 0x5E2410, (float)(y - AF_SKY) / (AF_H - AF_SKY) * 0.28f);  // deeper = darker
    int g = h % 13; if (g == 0) c = blend(c, 0xFFD2A0, 0.35f); else if (g == 1) c = blend(c, 0x3A1408, 0.35f); else if (g < 4) c = blend(c, 0xFFFFFF, 0.08f);   // grains
    int near = 0;   // packed wall: soil right next to a tunnel is darker
    for (int k = 1; k <= 2 && !near; k++) if ((y - k >= AF_SKY && anPx(x, y - k)) || anPx(x, y + k) || anPx(x - k, y) || anPx(x + k, y)) near = k;
    if (near == 1) c = blend(c, 0x3A1608, 0.42f); else if (near == 2) c = blend(c, 0x3A1608, 0.18f);
    return c;
  }
  bool floor = !anPx(x, y + 1), floor2 = !anPx(x, y + 2);
  uint32_t c = (h & 7) ? 0x4C2412 : 0x42200F;
  if (floor) c = (h & 3) ? 0x74401E : 0x8E4E26; else if (floor2) c = 0x5A2C14;   // loose sand on the floor
  return c;
}
void anDrawRect(int x0, int y0, int x1, int y1) {
  bool night = anNight();
  x0 = max(0, x0); y0 = max(0, y0); x1 = min(AF_W - 1, x1); y1 = min(AF_H - 1, y1);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) anBg.drawPixel(x, y, C(anColor(x, y, night)));
}
void anDrawGround() {   // sky, grass, the door and the pile of soil the ants carried out
  bool night = anNight();
  anDrawRect(0, 0, AF_W - 1, AF_SKY - 1);
  int hgt = min(12, 2 + (int)sqrtf((float)an.mound) / 3), wid = hgt * 3 + 6, cx = AN_DOOR + 10 + wid / 2;
  for (int y = 0; y < hgt; y++) {
    int half = (int)(wid / 2.0f * sqrtf(1.0f - (float)y / hgt));
    for (int x = cx - half; x <= cx + half; x++) { uint32_t h = anHash(x, y + 999); anBg.drawPixel(x, AF_SKY - 1 - y, C((h % 5) ? (h % 3 ? 0xC96A36 : 0xB85A2C) : 0xE08A50)); }
  }
  uint32_t grass = night ? 0x2E4A2E : 0x5E9A3F;
  for (int x = 0; x < AF_W; x++) if (abs(x - cx) > wid / 2 && abs(x - AN_DOOR) > 3) { anBg.drawPixel(x, AF_SKY - 1, C(grass)); if (anHash(x, 7) % 4 == 0) anBg.drawPixel(x, AF_SKY - 2 - (int)(anHash(x, 9) % 3), C(grass)); }
  for (int x = AN_DOOR - 3; x <= AN_DOOR + 3; x++) anBg.drawPixel(x, AF_SKY - 1, C(0x2A1208));   // the door
}
void anDrawBg() {
  if (!anBgOk) { anBg.setPsram(true); anBg.setColorDepth(16); anBgOk = anBg.createSprite(AF_W, AF_H); anBgNeed = true; }
  if (!anBgOk) return;
  if (anBgNeed) { anDrawRect(0, AF_SKY, AF_W - 1, AF_H - 1); anRx1 = -1; }
  else if (anRx1 >= 0) { anDrawRect(anRx0 - 3, anRy0 - 3, anRx1 + 3, anRy1 + 3); anRx1 = -1; }   // only what the ants just dug
  anDrawGround();
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
    P(k * 0.9f, 0, px, py); P(k * 1.3f + (moving ? sw : 0), side * 2.1f, qx, qy);
    spr.drawLine(px, py, qx, qy, body);
  }
  P(2.8f, 0, px, py); P(4.4f, 1.6f, qx, qy); spr.drawLine(px, py, qx, qy, body); P(4.4f, -1.6f, qx, qy); spr.drawLine(px, py, qx, qy, body);   // feelers
  P(-2.6f, 0, px, py); spr.fillEllipse(px, py, max(1, (int)(1.6f * s)), max(1, (int)(1.3f * s)), body);   // abdomen
  P(-2.9f, -0.5f, qx, qy); spr.drawPixel(qx, qy, hi);                                                     // shine
  P(0, 0, px, py); spr.fillRect(px - (s > 1.2f ? 1 : 0), py - (s > 1.2f ? 1 : 0), s > 1.2f ? 3 : 2, s > 1.2f ? 3 : 2, body);   // thorax
  P(2.1f, 0, px, py); spr.fillRect(px, py, 2, 2, body);                                                   // head
  if (carry) { P(3.6f, 0, px, py); spr.fillRect(px, py - 1, 2, 2, C(carry == 1 ? 0xE08A50 : 0xF2C94C)); }   // soil / food in the jaws
}
// a place on the floor of a room, the same every time (k = which one)
bool anFloorSpot(const AnRoom& r, uint32_t h, int& x, int& y) {
  x = r.x - r.r + 3 + (int)(h % (uint32_t)max(1, 2 * r.r - 6)); y = r.y + r.r / 2 + 2;
  while (y > r.y - r.r && !anPx(x, y)) y--;
  return anPx(x, y) && y >= AF_SKY;
}
void anDrawBrood(AnBox S) {
  // on the floor of the brood room: eggs (small white), larvae (cream), pupae (tan cocoons), by their age in days
  AnRoom& r = an.room[anNurseRoom()];
  int n[3] = {anToday + an.brood[0] + an.brood[1] + an.brood[2], an.brood[3] + an.brood[4] + an.brood[5], an.brood[6] + an.brood[7]};
  for (int kind = 2; kind >= 0; --kind) for (int k = 0; k < min(n[kind], 30); k++) {
    uint32_t h = anHash(k * 3 + kind, r.seed);
    int x, y; if (!anFloorSpot(r, h, x, y)) continue;
    if (r.kind == AK_QUEEN && abs(x - (r.x - 2)) < 6) continue;   // not under the queen
    y -= (int)((h >> 8) % 2);
    int X = S.x + x, Y = S.y + y;
    if (kind == 0) spr.drawPixel(X, Y, C(0xFFFFFF));
    else if (kind == 1) { spr.fillRect(X, Y - 1, 2, 2, C(0xF4EBD8)); spr.drawPixel(X + 2, Y, C(0xF4EBD8)); }
    else { spr.fillRect(X, Y - 2, 2, 3, C(0xE2C9A0)); spr.drawPixel(X, Y - 2, C(0xB89868)); }
  }
}
void anDrawFood(AnBox S) {
  if (anFoodRoom < 0 || anFoodRoom >= an.nRooms) return;
  AnRoom& r = an.room[anFoodRoom];
  if (r.kind != AK_FOOD) return;
  for (int k = 0; k < min(24, anToday * 4 + 3); k++) {
    int x, y; if (!anFloorSpot(r, anHash(k, r.seed + 77), x, y)) continue;
    spr.fillRect(S.x + x, S.y + y - 1, 2, 2, C(k % 3 ? 0xF2C94C : 0x9BD35A));
  }
}
void drawAnts() {
  antsSync();
  AnBox S = anScene();
  if (anBgNeed || !anBgOk || anRx1 >= 0) anDrawBg();
  if (anBgOk) anBg.pushSprite(&spr, S.x, S.y); else spr.fillRect(S.x, S.y, S.w, S.h, C(0xB9562A));
  spr.setClipRect(S.x, S.y, S.w, S.h);
  anDrawBrood(S); anDrawFood(S);
  { AnRoom& q = an.room[0]; float wob = sinf(millis() / 900.0f) * 0.25f;   // the queen: big, slow
    int qx, qy; if (anFloorSpot(q, 0, qx, qy)) {} qx = q.x - 2; qy = q.y;
    while (qy < AF_H - 3 && anPx(qx, qy + 3)) qy++;   // standing on the floor of her room
    anDrawAnt(qx, qy, PI + wob, 1.7f, false, 0, S.x, S.y); }
  for (auto& a : anAnts) anDrawAnt(a.x, a.y, a.a, 1.0f, !a.wait && a.job != AJ_DIG, a.job == AJ_CARRY ? 1 : a.job == AJ_FOOD ? 2 : 0, S.x, S.y);
  if (anLabels) for (int i = 0; i < an.nRooms; i++) {
    AnRoom& r = an.room[i]; if (r.prog < 8 && r.kind != AK_QUEEN) continue;
    const char* t = r.kind == AK_QUEEN ? "QUEEN" : r.kind == AK_BROOD ? "BROOD" : r.kind == AK_FOOD ? "FOOD" : "ROOM";
    txt(&fonts::Font0, t, S.x + r.x, S.y + r.y - r.r / 2 - 6, themeHud ? HUDB : 0xFFFFFF, D_MC);
  }
  if (themeHud) { hudBrackets(S.x + 2, S.y + 2, S.w - 4, S.h - 4, INK); spr.drawFastHLine(S.x, S.y + S.h - 1, S.w, C(LINE)); }
  spr.clearClipRect();
  navAdd(S.x + 8, S.y + 20, S.w - 16, S.h - 30);   // tap the nest: names on the rooms
  uint32_t days = an.foundT && !timeApprox && (uint32_t)nowT() >= an.foundT ? ((uint32_t)nowT() - an.foundT) / 86400 + 1 : 1;
  drawAppTitle("Ant colony", "Day " + String(days), false);
  // numbers: ANTS | BROOD | STREAK (under the farm on a tall screen, on the right on a wide one)
  uint32_t brood = anBrood();
  String V[3] = {String(an.workers), String(brood), String(an.streak)};
  const char* K[3] = {"Ants", "Brood", "Streak"};   // (capitals on HUD)
  bool wide = land();
  int px = wide ? S.w + 4 : 6, py = wide ? S.y : S.y + S.h + 4, pw = wide ? W - S.w - 8 : W - 12;
  int bottom = FTR_Y - 4;
  for (int i = 0; i < 3; i++) {
    int bx, by, bw, bh;
    if (wide) { bh = 34; bx = px; by = py + i * (bh + 4); bw = pw; }
    else { bw = (pw - 8) / 3; bh = 32; bx = px + i * (bw + 4); by = py; }
    card(bx, by, bw, bh, 8, CARD);
    txt(FS, fitText(FS, hudUp(K[i]), bw - 8), bx + 6, by + 1, SOFT);
    txt(themeHud ? (const lgfx::IFont*)&hM : FB, V[i], bx + 6, by + bh - 16, themeHud ? HUDB : INK);
  }
  String l1 = anStage(); if (an.workers >= 10 && !wide) { spr.setFont(FB); if ((int)spr.textWidth(hudUp(l1 + " colony")) <= pw / 2 + 20) l1 += " colony"; }
  String l2 = anGoals ? "Food " + String(anToday) + "/" + String(anGoals) : String("No goals");
  int nh = anNextHatch(); String l3 = nh ? "Next ant in " + String(nh) + (nh == 1 ? " day" : " days") : String("Reach a goal: an egg");
  if (timeApprox && !an.day[0]) l3 = "Set the time to grow";
  if (wide) {
    int ly = py + 3 * 38 + 2;
    txt(FS, fitText(FS, hudUp(l1), pw), px, ly, themeHud ? HUDB : INK); ly += 17;
    if (ly + 14 <= bottom) txt(FS, fitText(FS, hudUp(l2), pw), px, ly, SOFT);
  } else {
    int ly = py + 36;
    txt(FB, fitText(FB, hudUp(l1), pw / 2 + 20), px + 2, ly, themeHud ? HUDB : INK);
    txt(FS, fitText(FS, hudUp(l2), pw / 2 - 26), px + pw, ly + 2, SOFT, D_TR);
    ly += 19;
    if (ly + 14 <= bottom) txt(FS, fitText(FS, hudUp(l3), pw), px + 2, ly, SOFT);
  }
}

// ---------------- open / close / touch / joystick ----------------
JoyBtn anBtn;   // the stick's button: hold 1 s = back to Games, like every game
void antOpen() {
  antsSync();
  anCatchUp();   // the digging done since the page was closed (also on the same day)
  scr = S_ANTS; dirty = true; joyBtnReset(anBtn);
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
  if (backHit(x, y)) { antLeave(); return; }
  AnBox S = anScene();
  if (hitR(x, y, S.x, S.y, S.w, S.h)) { anLabels = !anLabels; dirty = true; }
}
String antsSub() {
  if (!anLoad()) return "";
  return String(an.workers) + " ants";
}
