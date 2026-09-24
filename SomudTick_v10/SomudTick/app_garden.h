#pragma once
// ============================================================================
//  Habit Garden: a tree that grows from the goals you really reach in Log.
//    - every goal reached on a day = +1 growth. More growth = a bigger tree
//      (Seed -> Sprout -> ... -> Ancient tree). The tree never gets smaller.
//    - goals reached today = flowers on the tree today
//    - a day with ALL goals reached = one fruit. Tap a fruit to pick it.
//    - streak = days in a row with at least half of the goals reached
//    - the sky follows the real time (morning, day, evening, night)
//    - Rain button = a short, calm rain
//  "max" goals (like Snack max 2) count as reached if you stayed under the
//  limit on a day you used the board.
//  Saved in Preferences (keys gd...), so it stays after a restart.
//  Included from SomudTick.ino.
// ============================================================================

uint32_t gdPts = 0, gdSeed = 0, gdBasket = 0;
uint16_t gdStreak = 0, gdBest = 0;
uint8_t gdFruit = 0;
String gdDay;            // the last day already counted (YYYY-MM-DD)
String gdSyncedFor;      // curDay when we last counted
bool gdLoaded = false;
uint32_t gdRainUntil = 0, gdToastT = 0; String gdToast;
struct GdPt { int16_t x, y; };
std::vector<GdPt> gdFruitPos;   // where the fruit were drawn (for taps)
const int GD_NSTAGE = 12;
const uint16_t GD_STAGE_AT[GD_NSTAGE] = {0, 2, 5, 9, 14, 20, 28, 38, 50, 65, 85, 110};
const char* GD_STAGE_N[GD_NSTAGE] = {"Seed", "Sprout", "Seedling", "Little plant", "Young plant", "Small tree",
                                    "Young tree", "Tree", "Big tree", "Old tree", "Great tree", "Ancient tree"};

// goals reached on one day. sums: activity id -> (count, sum). any = something was logged that day
void gdScore(const std::map<String, Sum>& sums, bool any, int& done, int& goals) {
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
void gdToday(int& done, int& goals) {
  std::map<String, Sum> m;
  for (auto& e : todayEv) { Sum& s = m[e.id]; s.count++; s.sum += e.v; }
  gdScore(m, !todayEv.empty(), done, goals);
}
void gdLoad() {
  if (gdLoaded) return;
  gdPts = prefs.getUInt("gdPts", 0); gdStreak = prefs.getUInt("gdStrk", 0); gdBest = prefs.getUInt("gdBest", 0);
  gdFruit = prefs.getUChar("gdFruit", 0); gdBasket = prefs.getUInt("gdBask", 0);
  gdSeed = prefs.getUInt("gdSeed", 0);
  if (!gdSeed) { gdSeed = esp_random() | 1; prefs.putUInt("gdSeed", gdSeed); }
  gdDay = prefs.getString("gdDay", "");
  gdLoaded = true;
}
// count the days that ended since last time (reads one small log file per day)
void gardenSync() {
  gdLoad();
  if (timeApprox || gdSyncedFor == curDay) return;   // clock not right yet: wait, so days are not counted wrong
  time_t n = nowT();
  String yesterday = dayKey(n - 86400);
  int back = gdDay.length() ? 30 : 7;   // first time: grow from the last 7 days of logs
  bool changed = false;
  for (int d = back; d >= 1; --d) {
    String k = dayKey(n - (time_t)d * 86400);
    if (gdDay.length() && strcmp(k.c_str(), gdDay.c_str()) <= 0) continue;   // already counted
    std::map<String, Sum> m; bool any = false;
    forEachEvent(k, [&](const Ev& e) { Sum& s = m[e.id]; s.count++; s.sum += e.v; any = true; });
    int done, goals; gdScore(m, any, done, goals);
    gdPts += done;
    if (goals && done * 2 >= goals) { gdStreak++; gdBest = max(gdBest, gdStreak); } else gdStreak = 0;
    if (goals && done == goals && gdFruit < 12) gdFruit++;
    changed = true;
  }
  if (gdDay != yesterday) { gdDay = yesterday; changed = true; }
  if (changed) {
    prefs.putUInt("gdPts", gdPts); prefs.putUInt("gdStrk", gdStreak); prefs.putUInt("gdBest", gdBest);
    prefs.putUChar("gdFruit", gdFruit); prefs.putString("gdDay", gdDay);
  }
  gdSyncedFor = curDay;
}
int gdStage(uint32_t pts) { int s = 0; while (s + 1 < GD_NSTAGE && pts >= GD_STAGE_AT[s + 1]) s++; return s; }
uint32_t gdGrowth() { int d, g; gdToday(d, g); return gdPts + d; }

void gardenOpen() { gardenSync(); scr = S_GARDEN; dirty = true; }

// ---------------- scene ----------------
struct GdBox { int x, y, w, h; };
GdBox gdScene() { int top = HDR_H + 34; return land() ? GdBox{0, top, 196, FTR_Y - top} : GdBox{0, top, W, FTR_Y - 84 - top}; }
GdBox gdRainBtn() { GdBox s = gdScene(); return {s.x + s.w - 40, s.y + 6, 34, 30}; }
uint32_t gdRng;
float gdR() { gdRng = gdRng * 1664525u + 1013904223u; return (gdRng >> 8) / 16777216.0f; }   // 0..1, same every frame
struct GdSky { uint32_t top, bot; bool night; float light; };
GdSky gdSky() {
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  float h = timeApprox ? 12 : tm.tm_hour + tm.tm_min / 60.0f;
  if (h < 5 || h >= 19.5f) return {0x0B1530, 0x1D2B4F, true, 0.45f};
  if (h < 7) return {0x3B4F8A, 0xF4B183, false, 0.8f};
  if (h < 17) return {0x6FB8EC, 0xD4ECFA, false, 1.0f};
  return {0x4A3F7A, 0xF39C6B, false, 0.8f};
}
std::vector<GdPt> gdTips, gdLeaves;   // branch ends (flowers, fruit) and places for leaves
void gdBranch(float x, float y, float ang, float len, int depth, float w, float sway, uint32_t bark) {
  float a = ang + sway * (6 - depth) * 0.25f;
  float x2 = x + cosf(a) * len, y2 = y - sinf(a) * len;
  wideLine(x, y, x2, y2, max(0.6f, w / 2), C(bark));
  if (depth <= 0 || len < 4) { gdTips.push_back({(int16_t)x2, (int16_t)y2}); return; }
  if (depth <= 2) gdLeaves.push_back({(int16_t)x2, (int16_t)y2});   // leaves inside the crown too, so it looks full
  int kids = gdR() < 0.25f ? 3 : 2;
  for (int k = 0; k < kids; k++) {
    float spread = (kids == 3 ? (k - 1) * 0.5f : (k ? 0.42f : -0.42f)) + (gdR() - 0.5f) * 0.3f;
    float na = a + spread;
    na += (PI / 2 - na) * 0.2f;   // branches keep reaching up, so the crown stays round
    gdBranch(x2, y2, na, len * (0.68f + gdR() * 0.12f), depth - 1, w * 0.68f, sway, bark);
  }
}
void gdFlower(int x, int y, uint32_t col) {
  for (int k = 0; k < 5; k++) { float a = k * 2 * PI / 5; spr.fillCircle(x + cosf(a) * 2.6f, y + sinf(a) * 2.6f, 2, C(col)); }
  spr.fillCircle(x, y, 1, C(0xF2C94C));
}
void drawGardenScene(int todayDone, int todayGoals) {
  GdBox S = gdScene();
  GdSky sky = gdSky();
  spr.setClipRect(S.x, S.y, S.w, S.h);
  for (int y = 0; y < S.h; y++) spr.drawFastHLine(S.x, S.y + y, S.w, C(blend(sky.top, sky.bot, (float)y / S.h)));
  float t = millis() / 1000.0f;
  bool raining = millis() < gdRainUntil;
  // sun or moon and stars
  gdRng = gdSeed ^ 0x55;
  if (sky.night) {
    for (int k = 0; k < 28; k++) { int sx = S.x + gdR() * S.w, sy = S.y + gdR() * S.h * 0.6f; if (sinf(t * 1.7f + k) > -0.6f) spr.drawPixel(sx, sy, C(0xDDE6FF)); }
    spr.fillCircle(S.x + S.w - 58, S.y + 26, 10, C(0xF4F1DE)); spr.fillCircle(S.x + S.w - 53, S.y + 23, 9, C(sky.top));
  } else if (!raining) spr.fillCircle(S.x + 30, S.y + 26, 12, C(0xFFE08A));
  // clouds drift slowly
  for (int k = 0; k < 2; k++) {
    int cx = S.x + (int)(fmodf(t * (4 + k * 2) + k * 120, S.w + 80)) - 40, cy = S.y + 18 + k * 20;
    uint32_t cc = raining ? 0x8A96A3 : (sky.night ? 0x33415F : 0xFFFFFF);
    spr.fillEllipse(cx, cy, 18, 7, C(cc)); spr.fillEllipse(cx + 10, cy - 4, 11, 7, C(cc));
  }
  // ground
  int gy = S.y + S.h - 22;
  uint32_t grass = blend(0x2E4A2E, 0x6DAA4F, sky.light), soil = blend(0x2A2118, 0x7A5C3E, sky.light);
  spr.fillEllipse(S.x + S.w / 2, gy + 30, S.w * 0.75f, 34, C(grass));
  spr.fillRect(S.x, gy + 14, S.w, S.h, C(grass));
  gdRng = gdSeed ^ 0x99;
  for (int k = 0; k < 14; k++) { int x = S.x + gdR() * S.w, y = gy + 10 + gdR() * 12; spr.drawLine(x, y, x - 2, y - 4, C(blend(grass, 0x000000, 0.25f))); spr.drawLine(x, y, x + 2, y - 4, C(blend(grass, 0x000000, 0.25f))); }
  // the plant
  int stage = gdStage(gdPts + todayDone);
  float health = todayGoals ? 0.45f + 0.55f * todayDone / todayGoals : 0.8f;
  if (raining) health = 1;
  uint32_t leaf1 = blend(blend(0xA8B87A, 0x3FA34D, health), 0x000000, 1 - sky.light), leaf2 = blend(leaf1, 0xFFFFFF, 0.18f);
  uint32_t bark = blend(0x2A2118, 0x6B4A2E, sky.light);
  int px = S.x + S.w / 2, py = gy + 8;
  float sway = sinf(t * 1.1f) * 0.035f;
  gdTips.clear(); gdLeaves.clear(); gdFruitPos.clear();
  if (stage == 0) {
    spr.fillEllipse(px, py + 2, 16, 6, C(soil));
    spr.fillEllipse(px, py - 2, 4, 3, C(0xC9A66B));
  } else if (stage <= 2) {
    int h = stage == 1 ? 14 : 26;
    spr.fillEllipse(px, py + 2, 14, 5, C(soil));
    float tx = px + sinf(t * 1.1f) * 2, ty = py - h;
    wideLine(px, py, tx, ty, 1.2f, C(leaf1));
    for (int k = 0; k < stage * 2; k++) {
      float ly = py - h * (0.45f + 0.5f * k / (stage * 2)), lx = px + (tx - px) * (py - ly) / h;
      int dir = k & 1 ? 1 : -1;
      spr.fillEllipse(lx + dir * 6, ly, 6, 3, C(k & 2 ? leaf2 : leaf1));
    }
    gdTips.push_back({(int16_t)tx, (int16_t)ty});
  } else {
    float maxH = (py - S.y - 14) / 3.1f;
    float len = min(maxH, S.h * (0.09f + 0.018f * stage));
    int depth = min(8, 2 + stage / 2);
    gdRng = gdSeed;
    gdBranch(px, py, PI / 2, len, depth, 3 + stage * 0.7f, sway, bark);
    gdRng = gdSeed ^ 0x1234;
    int lr = 3 + stage / 3;
    for (auto& p : gdLeaves) spr.fillCircle(p.x + (gdR() - 0.5f) * 6, p.y + (gdR() - 0.5f) * 6, lr + 2, C(blend(leaf1, 0x000000, 0.15f)));
    for (auto& p : gdTips) {   // leaf clusters
      spr.fillCircle(p.x + (gdR() - 0.5f) * 4, p.y + (gdR() - 0.5f) * 4, lr + 1, C(leaf1));
      spr.fillCircle(p.x + (gdR() - 0.5f) * 5 - 1, p.y - 2, lr - 1, C(leaf2));
    }
  }
  // flowers = goals reached today, fruit = full days
  int nt = gdTips.size();
  if (nt) {
    int fl = min(todayDone * (stage >= 3 ? 2 : 1), nt);
    const uint32_t FC[3] = {0xF7A8C4, 0xFFFFFF, 0xF4B6E0};
    for (int k = 0; k < fl; k++) { auto& p = gdTips[(2 * k + 1) * nt / (2 * fl)]; gdFlower(p.x + 3, p.y - 3, FC[k % 3]); }   // spread over the crown
    if (stage >= 3)
      for (int k = 0, nf = min((int)gdFruit, nt); k < nf; k++) {
        auto& p = gdTips[((4 * k + 3) * nt / (4 * nf)) % nt];
        int fx = p.x - 2, fy = p.y + 5;
        spr.fillCircle(fx, fy, 4, C(0xF0843C)); spr.fillCircle(fx - 1, fy - 1, 1, C(0xFFD2A8));
        gdFruitPos.push_back({(int16_t)fx, (int16_t)fy});
      }
  }
  // all goals reached today: a butterfly visits
  if (todayGoals && todayDone == todayGoals) {
    float bx = px + sinf(t * 0.7f) * S.w * 0.3f, by = S.y + S.h * 0.45f + sinf(t * 1.9f) * 14, f = fabsf(sinf(t * 9)) * 5 + 1;
    spr.fillTriangle(bx, by, bx - f, by - 5, bx - f, by + 3, C(0xF2A65A));
    spr.fillTriangle(bx, by, bx + f, by - 5, bx + f, by + 3, C(0xF2A65A));
  }
  // rain
  if (raining) {
    gdRng = 7;
    for (int k = 0; k < 40; k++) {
      int x = S.x + gdR() * S.w, sp = 90 + gdR() * 60;
      int y = S.y + (int)(gdR() * S.h + t * sp) % S.h;
      spr.drawFastVLine(x, y, 5, C(0x9CC8F5));
    }
  } else if (gdRainUntil && millis() - gdRainUntil < 4000 && !sky.night) {   // a rainbow after the rain
    const uint32_t RB[5] = {0xF28B82, 0xF6C26B, 0xF7E58A, 0x9AD9A1, 0x8CC8F0};
    for (int k = 0; k < 5; k++) spr.drawArc(px, gy + 10, S.w * 0.46f - k * 3, S.w * 0.46f - k * 3 - 2, 180, 360, C(RB[k]));
  }
  // the Rain button
  GdBox rb = gdRainBtn();
  spr.fillRoundRect(rb.x, rb.y, rb.w, rb.h, 10, C(0xFFFFFF));
  spr.fillEllipse(rb.x + 15, rb.y + 12, 8, 5, C(0x8A96A3)); spr.fillEllipse(rb.x + 21, rb.y + 10, 6, 5, C(0x8A96A3));
  for (int k = 0; k < 3; k++) spr.drawFastVLine(rb.x + 11 + k * 6, rb.y + 19, 4, C(0x3B82F6));
  if (stage == 0 && !raining) txt(FS, fitText(FS, "Reach a goal to grow it", S.w - 16), S.x + S.w / 2, gy - 34, sky.night ? 0xDDE6FF : 0x2A3A4A, D_MC);
  if (gdToastT && millis() - gdToastT < 1500) {
    spr.setFont(FB); int tw = spr.textWidth(gdToast) + 20;
    spr.fillRoundRect(S.x + (S.w - tw) / 2, S.y + 8, tw, 24, 12, C(0x000000));
    txt(FB, gdToast, S.x + S.w / 2, S.y + 20, 0xFFFFFF, D_MC);
  }
  spr.clearClipRect();
}
void drawGarden() {
  gardenSync();
  int done, goals; gdToday(done, goals);
  uint32_t pts = gdPts + done;
  int st = gdStage(pts);
  drawGardenScene(done, goals);
  drawAppTitle("Garden", gdStreak ? "Streak " + String(gdStreak) : String(""));
  // info card
  int x, y, w;
  if (land()) { x = 200; y = HDR_H + 34; w = W - 206; } else { x = 6; y = FTR_Y - 80; w = W - 12; }
  int h = FTR_Y - 4 - y;
  spr.fillRoundRect(x, y, w, h, 10, C(CARD));
  int tx = x + 10, tw = w - 20, ly = y + 8;
  bool top = st + 1 >= GD_NSTAGE;
  uint32_t a = GD_STAGE_AT[st], b = top ? a : GD_STAGE_AT[st + 1];
  String grow = top ? String("Fully grown!") : String(pts) + " / " + String(b);   // growth now / needed for the next size
  String today = goals ? "Today " + String(done) + "/" + String(goals) + " goals" : String("Set goals on web");
  String fruit = gdFruit ? String("Tap a fruit!") : "Picked " + String(gdBasket);
  auto bar = [&](int by) {
    spr.fillRoundRect(tx, by, tw, 6, 3, C(LINE));
    spr.fillRoundRect(tx, by, top ? tw : max(6, (int)(tw * (pts - a) / (b - a))), 6, 3, C(0x3FA34D));
  };
  if (land()) {   // narrow card on the right: one thing per line
    txt(FB, fitText(FB, GD_STAGE_N[st], tw), tx, ly, INK); ly += 24;
    bar(ly); ly += 12;
    txt(FS, fitText(FS, grow, tw), tx, ly, SOFT); ly += 26;
    txt(FS, fitText(FS, goals ? "Today " + String(done) + "/" + String(goals) : String("No goals yet"), tw), tx, ly, INK); ly += 20;
    if (goals) { txt(FS, fitText(FS, String(done) + (done == 1 ? " flower" : " flowers"), tw), tx, ly, SOFT); ly += 20; }
    txt(FS, fitText(FS, fruit, tw), tx, ly, SOFT);
  } else {        // wide card under the tree: name | growth, bar, today | fruit
    spr.setFont(FS); int gw = spr.textWidth(grow);
    txt(FB, fitText(FB, GD_STAGE_N[st], tw - gw - 8), tx, ly, INK);
    txt(FS, grow, x + w - 10, ly + 2, SOFT, D_TR); ly += 26;
    bar(ly); ly += 14;
    spr.setFont(FS); int fw = spr.textWidth(fruit);
    if (goals && spr.textWidth(today) > tw - fw - 8) today = "Today " + String(done) + "/" + String(goals);   // short form, no cut word
    txt(FS, fitText(FS, today, tw - fw - 8), tx, ly, INK);
    txt(FS, fruit, x + w - 10, ly, SOFT, D_TR);
  }
}
void gardenTap(int x, int y) {
  if (backHit(x, y)) { scr = S_GAMES; dirty = true; return; }
  GdBox rb = gdRainBtn();
  if (hitR(x, y, rb.x - 4, rb.y - 4, rb.w + 8, rb.h + 8)) { gdRainUntil = millis() + 5000; dirty = true; return; }
  for (auto& f : gdFruitPos) {
    if ((x - f.x) * (x - f.x) + (y - f.y) * (y - f.y) > 14 * 14) continue;
    gdFruit--; gdBasket++;
    prefs.putUChar("gdFruit", gdFruit); prefs.putUInt("gdBask", gdBasket);
    gdToast = "+1 fruit"; gdToastT = millis();
    ledFlash(0xF0843C, 200);
    dirty = true;
    return;
  }
}
String gardenSub() {
  gdLoad();
  int d, g; gdToday(d, g);
  return GD_STAGE_N[gdStage(gdPts + d)];
}
