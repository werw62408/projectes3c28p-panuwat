#pragma once
// Activities and logs: day files, acts.json, names, all-time totals, +1 / undo, stats numbers, moving guessed logs.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- ตัวช่วย ----------------
String fmtNum(float v) {
  if (fabsf(v - roundf(v)) < 0.05f) return String((long)roundf(v));
  return String(v, 1);
}
String agoStr(uint32_t last) {
  if (!last) return "No log yet";
  long d = (long)nowT() - (long)last; if (d < 0) d = 0;
  if (d < 60) return "just now";
  long m = d / 60;
  if (m < 60) return String(m) + "m ago";
  long h = m / 60; m %= 60;
  if (h < 24) return String(h) + "h " + (m ? String(m) + "m " : String("")) + "ago";
  return String(h / 24) + "d ago";
}
String colorHex(uint32_t c) { char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)(c & 0xFFFFFF)); return b; }
uint32_t parseColor(const char* s) { if (!s) return 0x2F8F82; if (*s == '#') s++; return strtoul(s, nullptr, 16) & 0xFFFFFF; }
bool isUnitAct(const Act& a) { return a.unit.length() > 0; }

Sum sumFor(const String& id) {
  Sum s;
  for (auto& e : todayEv) if (e.id == id) { s.count++; s.sum += e.v; s.last = e.t; }
  return s;
}
float measure(const Act& a, const Sum& s) { return isUnitAct(a) ? s.sum : s.count; }
// 0 ไม่มีเป้า, 1 ยังไม่ถึง, 2 ถึงเป้าแล้ว(ดี), 3 ถึงลิมิต, 4 เกินลิมิต
int goalState(const Act& a, float m) {
  if (a.goal <= 0 || a.goalType == 0) return 0;
  if (a.goalType == 1) return m >= a.goal ? 2 : 1;
  if (m > a.goal) return 4;
  if (m >= a.goal) return 3;
  return 1;
}
bool overdue(const Act& a) {
  if (!a.remind || timeApprox) return false;
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  if (tm.tm_hour < REMIND_FROM_H || tm.tm_hour >= REMIND_TO_H) return false;
  uint32_t last = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
  struct tm s = tm; s.tm_hour = REMIND_FROM_H; s.tm_min = 0; s.tm_sec = 0;
  uint32_t base = (uint32_t)mktime(&s);
  if (last < base) last = base;   // เริ่มนับจาก 8 โมงเช้า
  if (last > (uint32_t)n) return false;   // last log looks newer than now (the clock was moved back): not late
  return (uint32_t)n - last > (uint32_t)a.remind * 60;
}

// ---------------- ไฟล์ ----------------
// v10: logs live in their own 12 MB flash area called "logs" (see partitions.csv).
// Older versions kept them in the small 896 KB area (full after about 7 months).
// The first start of v10 moves them over (logsMove). Settings and acts.json stay in the small area.
fs::LittleFSFS LOGFS;
bool logFsOk = false;
fs::FS& logFs() { return logFsOk ? (fs::FS&)LOGFS : (fs::FS&)LittleFS; }
uint64_t logTotalBytes() { return logFsOk ? LOGFS.totalBytes() : LittleFS.totalBytes(); }
uint64_t logUsedBytes() { return logFsOk ? LOGFS.usedBytes() : LittleFS.usedBytes(); }
int logUsedPct() { uint64_t t = logTotalBytes(); return t ? (int)(logUsedBytes() * 100 / t) : 0; }
int logPctCache = 0;   // updated now and then (reading it is not free)
String logPath(const String& key) { return "/log/" + key + ".csv"; }
// "Time unsure" in About (v11.7): the count is kept per day file. Only the days written since the last count are
// read again (v11.6 read every day file after every new log: seconds with years of logs).
std::map<String, int> unsureDay; std::set<String> unsureStale; bool unsureAllRead = false;
void unsureTouch(const String& key) { unsureStale.insert(key); }
uint32_t logRev = 1;   // +1 on every change to the logs (Stats counts again only when this changed)
String evLine(const Ev& e) {
  char b[96];
  snprintf(b, sizeof b, "%lu,%s,%s,%c,%d\n", (unsigned long)e.t, e.id.c_str(), fmtNum(e.v).c_str(), e.place, e.ok ? 1 : 0);
  return b;
}
bool parseLine(const String& l, Ev& e) {
  int p1 = l.indexOf(','), p2 = l.indexOf(',', p1 + 1), p3 = l.indexOf(',', p2 + 1), p4 = l.indexOf(',', p3 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || (int)l.length() <= p3 + 1) return false;
  e.t = (uint32_t)l.substring(0, p1).toInt();
  e.id = l.substring(p1 + 1, p2);
  e.v = l.substring(p2 + 1, p3).toFloat();
  e.place = l[p3 + 1];
  e.ok = (p4 < 0) ? true : (l[p4 + 1] == '1');
  return e.t > 0 && e.id.length();
}
void forEachEvent(const String& key, const std::function<void(const Ev&)>& fn) {
  String p = logPath(key);
  if (!logFs().exists(p)) return;
  File f = logFs().open(p, "r");
  if (!f) return;
  while (f.available()) {
    String l = f.readStringUntil('\n');
    Ev e;
    if (parseLine(l, e)) fn(e);
  }
  f.close();
}
// write a whole day file safely: to a new file first, then swap (a power cut can't leave half a file)
void writeDayFile(const String& key, const std::vector<String>& lines) {
  String p = logPath(key), tmp = p + ".new";
  File f = logFs().open(tmp, "w");
  if (!f) return;
  for (auto& l : lines) f.print(l);
  f.close();
  logFs().remove(p);
  logFs().rename(tmp, p);
  logRev++;
  unsureTouch(key);
}
// after a power cut in the middle of writeDayFile(): finish the swap (or drop the half-written copy)
void dayFilesRepair() {
  std::vector<String> news;
  File root = logFs().open("/log");
  if (root) { for (File f = root.openNextFile(); f; f = root.openNextFile()) { String n = f.name(); if (n.endsWith(".csv.new")) news.push_back(n); f.close(); } root.close(); }
  for (auto& n : news) {
    String tmp = "/log/" + n, p = tmp.substring(0, tmp.length() - 4);
    if (logFs().exists(p)) logFs().remove(tmp);   // the old file is still there: the swap never started
    else logFs().rename(tmp, p);                  // the old one was removed: the new copy is complete
  }
}
// sort one day file by time (after logs were moved there from a guessed time)
void sortDayFile(const String& key) {
  std::vector<Ev> ev; forEachEvent(key, [&](const Ev& e) { ev.push_back(e); });
  std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
  std::vector<String> lines; for (auto& e : ev) lines.push_back(evLine(e));
  writeDayFile(key, lines);
}
void rewriteToday() {
  std::vector<String> lines;
  for (auto& e : todayEv) lines.push_back(evLine(e));
  writeDayFile(curDay, lines);
}
void recomputeLast() {
  lastSeen.clear();
  forEachEvent(dayKey(nowT() - 86400), [](const Ev& e) { lastSeen[e.id] = e.t; });
  for (auto& e : todayEv) lastSeen[e.id] = e.t;
}
void loadDay() {
  curDay = dayKey(nowT());
  todayEv.clear();
  forEachEvent(curDay, [](const Ev& e) { todayEv.push_back(e); });
  recomputeLast();
  remindedFor.clear();
}

void defaultActs() {
  acts.clear();
  auto add = [](const char* id, const char* name, uint32_t col, const char* unit, float step, float goal, uint8_t type, uint16_t remind) {
    Act a; a.id = id; a.name = name; a.color = col; a.unit = unit; a.step = step; a.goal = goal; a.goalType = type; a.remind = remind;
    acts.push_back(a);
  };
  add("water", "Water", 0x2F8F82, "", 1, 8, 1, 90);   // count glasses/times (+1 each tap)
  add("toilet", "Toilet", 0x3E6B99, "", 1, 0, 0, 0);
  add("fuel", "Fuel", 0x6B7F3E, "baht", 100, 0, 0, 0);
  add("smoke", "Smoke", 0x5C6670, "", 1, 5, 2, 0);
  add("rest", "Rest", 0x7A6C9E, "", 1, 0, 0, 0);
}
void actsToJson(JsonArray arr) {
  for (auto& a : acts) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = a.id; o["name"] = a.name; o["unit"] = a.unit; o["color"] = colorHex(a.color);
    o["step"] = a.step; o["goal"] = a.goal; o["type"] = a.goalType; o["remind"] = a.remind;
  }
}
String cleanId(String s) {
  String o;
  for (char c : s) if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') o += c;
  return o.substring(0, 16);
}
bool actsFromJson(JsonArrayConst arr) {
  std::vector<Act> out;
  for (JsonObjectConst o : arr) {
    if (out.size() >= MAX_ACTS) break;
    Act a;
    a.name = String((const char*)(o["name"] | ""));
    a.name.trim();
    if (!a.name.length()) continue;
    a.name = a.name.substring(0, 60);
    a.id = cleanId(String((const char*)(o["id"] | "")));
    bool dup = false; for (auto& b : out) if (b.id == a.id) dup = true;
    if (!a.id.length()) a.id = oldIdFor(a.name, out);   // new on the web: same name as an old one = same history
    if (!a.id.length() || dup) a.id = "a" + String(esp_random() % 1000000);
    a.unit = String((const char*)(o["unit"] | "")).substring(0, 12);
    a.unit.trim();
    a.color = parseColor(o["color"] | "#2F8F82");
    a.step = o["step"] | 1.0f; if (a.step <= 0) a.step = 1;
    a.goal = o["goal"] | 0.0f; if (a.goal < 0) a.goal = 0;
    a.goalType = o["type"] | 0; if (a.goalType > 2) a.goalType = 0;
    a.remind = o["remind"] | 0;
    out.push_back(a);
  }
  if (out.empty()) return false;
  acts = out;
  return true;
}
// Names of every activity ever used (id TAB name), in the logs area.
// A deleted activity that is added again on the web with the same name gets its old id = its old history.
std::map<String, String> knownNames;
void namesSave() {
  File f = logFs().open("/names.txt", "w");
  if (!f) return;
  for (auto& n : knownNames) f.print(n.first + "\t" + n.second + "\n");
  f.close();
}
void namesLoad() {
  knownNames.clear();
  File f = logFs().open("/names.txt", "r");
  if (f) { while (f.available()) { String l = f.readStringUntil('\n'); int t = l.indexOf('\t'); if (t > 0) knownNames[l.substring(0, t)] = l.substring(t + 1); } f.close(); }
  if (!knownNames.count("meal")) knownNames["meal"] = "Meal";     // removed in v10
  if (!knownNames.count("snack")) knownNames["snack"] = "Snack";
}
void namesRemember() {
  bool ch = false;
  for (auto& a : acts) if (knownNames[a.id] != a.name) { knownNames[a.id] = a.name; ch = true; }
  if (ch) namesSave();
}
String oldIdFor(const String& name, const std::vector<Act>& used) {   // id that had this name before (not in use now)
  String n = name; n.toLowerCase();
  for (auto& k : knownNames) {
    String m = k.second; m.toLowerCase();
    if (m != n) continue;
    bool taken = false; for (auto& u : used) if (u.id == k.first) taken = true;
    if (!taken) return k.first;
  }
  return "";
}
void saveActs() {
  JsonDocument d; actsToJson(d.to<JsonArray>());
  File f = LittleFS.open("/acts.json", "w");
  if (f) { serializeJson(d, f); f.close(); }
  namesRemember();
}
void loadActs() {
  bool ok = false;
  if (LittleFS.exists("/acts.json")) {
    File f = LittleFS.open("/acts.json", "r");
    JsonDocument d;
    if (f && !deserializeJson(d, f)) ok = actsFromJson(d.as<JsonArrayConst>());
    if (f) f.close();
  }
  if (!ok) { defaultActs(); saveActs(); return; }
  // older versions used Thai default names -> switch them to English
  const char* MAP[][3] = {{"water", "น้ำดื่ม", "Water"}, {"toilet", "เข้าห้องน้ำ", "Toilet"}, {"meal", "กินข้าว", "Meal"},
                          {"snack", "กินขนม", "Snack"}, {"fuel", "เติมน้ำมัน", "Fuel"}, {"smoke", "สูบบุหรี่", "Smoke"}, {"rest", "พักผ่อน", "Rest"}};
  bool changed = false;
  for (auto& a : acts) {
    for (auto& m : MAP) if (a.id == m[0] && a.name == m[1]) { a.name = m[2]; changed = true; }
    if (a.unit == "บาท") { a.unit = "baht"; changed = true; }
    // v6: Water counts times (+1 per tap) instead of ml
    if (a.id == "water" && a.unit == "ml") {
      a.unit = ""; a.goal = a.goal > 0 ? max(1.0f, roundf(a.goal / max(1.0f, a.step))) : 0; a.step = 1; changed = true;
    }
  }
  // v10: Meal and Snack are not used any more (asked by the owner). Removed once; old logs stay in the files.
  if (!prefs.getBool("v10rm", false)) {
    size_t n = acts.size();
    acts.erase(std::remove_if(acts.begin(), acts.end(), [](const Act& a) { return a.id == "meal" || a.id == "snack"; }), acts.end());
    if (acts.size() != n) changed = true;
    prefs.putBool("v10rm", true);
  }
  if (changed) saveActs();
}

// ---------------- all-time totals ----------------
// Kept in one small file and updated on every +1 / -, so Stats never has to read years of files.
std::map<String, Sum> totals;
bool totalsOk = false;
void totalsSave() {
  File f = logFs().open("/totals.csv", "w");
  if (!f) return;
  for (auto& t : totals) f.print(t.first + "," + String(t.second.count) + "," + fmtNum(t.second.sum) + "\n");
  f.close();
}
void totalsRebuild() {   // read every log file once (first start of v10, or if the file is lost)
  totals.clear();
  File root = logFs().open("/log");
  if (root) {
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
      while (f.available()) { String l = f.readStringUntil('\n'); Ev e; if (parseLine(l, e)) { Sum& s = totals[e.id]; s.count++; s.sum += e.v; } }
      f.close();
    }
    root.close();
  }
  totalsSave();
}
void totalsLoad() {
  totals.clear();
  File f = logFs().open("/totals.csv", "r");
  if (!f) { totalsRebuild(); totalsOk = true; return; }
  while (f.available()) {
    String l = f.readStringUntil('\n');
    int a = l.indexOf(','), b = l.indexOf(',', a + 1);
    if (a > 0 && b > a) { Sum& s = totals[l.substring(0, a)]; s.count = l.substring(a + 1, b).toInt(); s.sum = l.substring(b + 1).toFloat(); }
  }
  f.close();
  totalsOk = true;
}
void totalsAdd(const String& id, int dc, float dv) {
  if (!totalsOk) totalsLoad();
  Sum& s = totals[id]; s.count += dc; s.sum += dv;
  if (s.count < 0) s.count = 0;
  totalsSave();
}

// ---------------- บันทึก ----------------
std::vector<Ev> guessedEv;   // logs made this start while the time was a guess (moved when the real time arrives)
bool logFull = false;         // the last write failed: storage full
int remindAct = -1;           // the activity shown in the reminder bar (-1 = none)
void logEvent(int i, float v, int times = 1) {   // times: the same log n times (typed "3" on a count activity)
  if (i < 0 || i >= (int)acts.size() || times < 1) return;
  if (dayKey(nowT()) != curDay) loadDay();
  Ev e{(uint32_t)nowT(), v, place, !timeApprox, acts[i].id};
  String line = evLine(e), lines;
  for (int k = 0; k < times; k++) { todayEv.push_back(e); lines += line; if (timeApprox) guessedEv.push_back(e); }
  File f = logFs().open(logPath(curDay), "a");   // one write for all of them
  logFull = !f || f.print(lines) != lines.length();
  if (f) f.close();
  logRev++;
  if (timeApprox) unsureTouch(curDay);   // (a log with a real time can't change the count)
  totalsAdd(e.id, times, v * times);
  lastSeen[e.id] = e.t;
  remindedFor.erase(e.id);
  if (remindAct == i) remindAct = -1;
  flashIdx = i; flashUntil = millis() + 300;
  ledFlash(acts[i].color, 200);
  dirty = true;
}
void logDefault(int i) { if (i >= 0 && i < (int)acts.size()) logEvent(i, isUnitAct(acts[i]) ? acts[i].step : 1); }
// a typed number: amount for "unit" activities (like 350 baht), number of times for the others (3 = 3 times)
void logValue(int i, float v) {
  if (i < 0 || i >= (int)acts.size() || v <= 0) return;
  if (isUnitAct(acts[i])) { logEvent(i, v); return; }
  logEvent(i, 1, constrain((int)roundf(v), 1, 50));
}
bool undoEvent(int i) {
  if (i < 0 || i >= (int)acts.size()) return false;
  for (int k = (int)todayEv.size() - 1; k >= 0; --k) {
    if (todayEv[k].id == acts[i].id) {
      Ev gone = todayEv[k];
      todayEv.erase(todayEv.begin() + k);
      totalsAdd(gone.id, -1, -gone.v);
      for (size_t g = 0; g < guessedEv.size(); g++)
        if (guessedEv[g].t == gone.t && guessedEv[g].id == gone.id) { guessedEv.erase(guessedEv.begin() + g); break; }
      rewriteToday();
      recomputeLast();
      dirty = true;
      return true;
    }
  }
  // nothing today: undo the last one from yesterday, if it was less than 6 hours ago (e.g. 23:50, noticed at 00:10)
  uint32_t lt = lastSeen.count(acts[i].id) ? lastSeen[acts[i].id] : 0;
  if (!lt || lt > (uint32_t)nowT() || (uint32_t)nowT() - lt > 6 * 3600UL) return false;
  String yk = dayKey(nowT() - 86400);
  std::vector<Ev> ev; forEachEvent(yk, [&](const Ev& e) { ev.push_back(e); });
  for (int k = (int)ev.size() - 1; k >= 0; --k) {
    if (ev[k].id != acts[i].id) continue;
    totalsAdd(ev[k].id, -1, -ev[k].v);
    for (size_t g = 0; g < guessedEv.size(); g++)   // made on a guessed time? then it must not come back when the time is fixed
      if (guessedEv[g].t == ev[k].t && guessedEv[g].id == ev[k].id) { guessedEv.erase(guessedEv.begin() + g); break; }
    ev.erase(ev.begin() + k);
    std::vector<String> lines; for (auto& e : ev) lines.push_back(evLine(e));
    writeDayFile(yk, lines);
    recomputeLast();
    dirty = true;
    return true;
  }
  return false;
}
bool canUndo(const Act& a, const Sum& s) {   // the [-] button works: something today, or last night's log (< 6 h)
  if (s.count > 0) return true;
  uint32_t lt = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
  return lt && lt <= (uint32_t)nowT() && (uint32_t)nowT() - lt <= 6 * 3600UL;
}

// logs whose time was a guess and was never put right (time_ok = 0 in the day files), shown in About (v11.5).
int unsureInFile(const String& key) {
  int n = 0;
  File f = logFs().open(logPath(key), "r");
  if (!f) return 0;
  while (f.available()) { String l = f.readStringUntil('\n'); Ev e; if (parseLine(l, e) && !e.ok) n++; }
  f.close();
  return n;
}
int unsureLogCount() {
  if (!unsureAllRead) {   // once: every day file
    unsureDay.clear(); unsureStale.clear();
    File root = logFs().open("/log");
    std::vector<String> keys;
    if (root) {
      for (File f = root.openNextFile(); f; f = root.openNextFile()) { String nm = f.name(); if (nm.endsWith(".csv")) keys.push_back(nm.substring(0, nm.length() - 4)); f.close(); }
      root.close();
    }
    for (auto& k : keys) { int c = unsureInFile(k); if (c) unsureDay[k] = c; }
    unsureAllRead = true;
  }
  for (auto& k : unsureStale) { int c = unsureInFile(k); if (c) unsureDay[k] = c; else unsureDay.erase(k); }   // only the days written since
  unsureStale.clear();
  int n = 0; for (auto& d : unsureDay) n += d.second;
  return n;
}

// ---------------- สถิติ ----------------
struct ActStat {
  std::vector<float> daily; std::vector<int> dcnt;
  uint16_t hours[24];
  float total = 0; int totalCnt = 0;
  double gapSum = 0; int gapN = 0;
};
std::vector<ActStat> st;
std::vector<String> stKeys;
int stDays = 0;

void computeStats(int days, bool allTime) {
  stDays = days;
  std::vector<float> keepTot; std::vector<int> keepCnt;   // reuse old totals (reading every file is slow)
  if (!allTime && st.size() == acts.size()) for (auto& x : st) { keepTot.push_back(x.total); keepCnt.push_back(x.totalCnt); }
  st.assign(acts.size(), ActStat());
  for (size_t i = 0; i < keepTot.size(); i++) { st[i].total = keepTot[i]; st[i].totalCnt = keepCnt[i]; }
  std::map<String, int> idx;
  for (size_t i = 0; i < acts.size(); i++) {
    idx[acts[i].id] = i;
    st[i].daily.assign(days, 0); st[i].dcnt.assign(days, 0);
    memset(st[i].hours, 0, sizeof st[i].hours);
  }
  stKeys.clear();
  time_t n = nowT();
  for (int d = days - 1; d >= 0; --d) {
    String k = dayKey(n - (time_t)d * 86400);
    stKeys.push_back(k);
    int di = days - 1 - d;
    std::vector<uint32_t> lastT(acts.size(), 0);
    forEachEvent(k, [&](const Ev& e) {
      auto it = idx.find(e.id); if (it == idx.end()) return;
      int i = it->second; ActStat& s = st[i];
      s.daily[di] += e.v; s.dcnt[di]++;
      time_t tt = e.t; struct tm tm; localtime_r(&tt, &tm);
      s.hours[tm.tm_hour]++;
      if (lastT[i] && e.t > lastT[i]) { s.gapSum += e.t - lastT[i]; s.gapN++; }
      lastT[i] = e.t;
    });
  }
  if (allTime) {
    if (!totalsOk) totalsLoad();
    for (size_t i = 0; i < acts.size(); i++) {
      auto it = totals.find(acts[i].id);
      if (it != totals.end()) { st[i].total = it->second.sum; st[i].totalCnt = it->second.count; }
    }
  }
}

// ---------------- the real time arrived: move logs made on the guessed time ----------------
void fixGuessedLogs(int32_t delta) {
  if (guessedEv.empty()) return;
  if (abs(delta) >= 30) {
    // 1) take them out of the day files they were written to
    std::map<String, std::vector<String>> byDay;
    for (auto& e : guessedEv) byDay[dayKey(e.t)].push_back(evLine(e));
    for (auto& d : byDay) {
      std::vector<String> keep;
      forEachEvent(d.first, [&](const Ev& e) {
        String l = evLine(e);
        auto it = std::find(d.second.begin(), d.second.end(), l);
        if (it != d.second.end()) d.second.erase(it); else keep.push_back(l);
      });
      writeDayFile(d.first, keep);
    }
    // 2) write them again at the right time (and so the right day), then put those days in time order
    std::vector<String> touched;
    for (auto& e : guessedEv) {
      Ev r = e; r.t = (uint32_t)((int64_t)e.t + delta); r.ok = true;
      String k = dayKey(r.t);
      File f = logFs().open(logPath(k), "a");
      if (f) { f.print(evLine(r)); f.close(); }
      if (std::find(touched.begin(), touched.end(), k) == touched.end()) touched.push_back(k);
    }
    for (auto& k : touched) sortDayFile(k);
    logRev++;
    Serial.printf("moved %d logs by %ld s\n", (int)guessedEv.size(), (long)delta);
  }
  guessedEv.clear();
}
