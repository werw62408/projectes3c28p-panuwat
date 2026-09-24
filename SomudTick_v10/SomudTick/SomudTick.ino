/*
  SomudTick — daily activity tracker gadget for the ES3C28P board
  (ESP32-S3 + 2.8" IPS 240x320 screen + FT6336G touch)

  Screen text is in simple English. (The web page on the phone stays in Thai.)
  Tabs:  Log | Stats | Apps | Settings
  Apps:  Files (SD photos/videos) | AC Remote (Panasonic IR) | Games | Internet
  Games: Pixel Swim | Sudoku | Sand & Water | Habit Garden

  Extra parts (optional):
    Joystick  VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (not 5V!), GND -> GND
    KY-005    S -> IO21, middle -> 3.3V, "-" -> GND
    (3.3V and GND can be taken from the I2C connector)

  Arduino IDE: board "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM",
  Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)", USB CDC On Boot "Enabled"
  Libraries: LovyanGFX, ArduinoJson (v7), IRremoteESP8266 (2.9+)
*/
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

#include "types.h"
#include "lgfx_es3c28p.h"
#include "fonts_th.h"
#include "webpage.h"

// ---------------- ค่าที่ปรับได้ ----------------
#define AP_SSID        "SomudTick"
#define AP_PASS        "12345678"      // default Wi-Fi password (change it on the web page)
#define MDNS_NAME      "somudtick"     // เปิด http://somudtick.local
#define PIN_BAT_ADC    9
#define PIN_RGB        42
#define PIN_BOOT       0
// screen off timer (Settings): 30s, 1m, 5m, never. The screen dims a little before it turns off.
const uint32_t OFF_MS[] = {30000UL, 60000UL, 300000UL, 0};
const char* OFF_NAMES[] = {"30s", "1m", "5m", "Never"};
#define REMIND_FROM_H  8               // เตือนเฉพาะช่วง 08:00–22:00
#define REMIND_TO_H    22
#define MAX_ACTS       12

// ---------------- จอ ----------------
LGFX_ES3C28P lcd;
LGFX_Sprite spr(&lcd);
static lgfx::PointerWrapper pwS(FONT_TH14, sizeof(FONT_TH14));
static lgfx::PointerWrapper pwB(FONT_TH18B, sizeof(FONT_TH18B));
static lgfx::VLWfont fS, fB;

#define D_TL lgfx::textdatum::top_left
#define D_TR lgfx::textdatum::top_right
#define D_TC lgfx::textdatum::top_center
#define D_ML lgfx::textdatum::middle_left
#define D_MR lgfx::textdatum::middle_right
#define D_MC lgfx::textdatum::middle_center

// ธีม (สีมีเฉพาะกิจกรรม) — ค่าจริงถูกตั้งใน applyTheme()
uint32_t PAPER, CARD, INK, SOFT, LINE, KEYBG, MUTE, ONINK, HDRBG, HDRFG, HDRSOFT;
const uint32_t QRWHITE = 0xFFFFFF;
struct TextCol { const char* name; uint32_t light, dark; };
const TextCol TEXTCOLS[] = {
  {"Black", 0x111111, 0xF2F2F2}, {"Blue", 0x1F3A93, 0x8FB3FF}, {"Green", 0x145A32, 0x7FD1A0},
  {"Red", 0x922B21, 0xF1948A}, {"Purple", 0x5B2C6F, 0xC39BD3}, {"Brown", 0x6E3B12, 0xE0B07A}};
const int N_TEXTCOLS = 6;
uint8_t themeDark = 0, textColIdx = 0, rot = 0;

const int HDR_H = 30;
int W = 240, H = 320, FTR_Y = 290, CONT_H = 260;   // เปลี่ยนตามการหมุนจอ
bool land() { return W > H; }

// ---------------- ข้อมูล ----------------
// Act (activity), Ev (one log line), Sum (today's total) are in types.h

std::vector<Act> acts;
std::vector<Ev> todayEv;
std::map<String, uint32_t> lastSeen;   // ครั้งล่าสุด (รวมเมื่อวาน)
String curDay;

Preferences prefs;
WebServer server(80);
String staSsid, staPass, homeSsid, uniSsid;
bool autoPlace = false, apOn = true, staOn = true, timeApprox = true;   // apOn = board hotspot, staOn = join other Wi-Fi
char place = 'H';
uint8_t brightIdx = 3;
const uint8_t BRIGHT[] = {30, 70, 130, 200, 255};

// ---------------- UI state ----------------
// enum Screen is in types.h
Screen scr = S_HOME;
bool dirty = true;
int scrollY = 0, heatScroll = 0, setScroll = 0, setMax = 0, filesScroll = 0, acScroll = 0, acMax = 0;
int8_t volIdx = 2;   // sound volume 0..4, -1 = off
uint8_t offIdx = 1;  // screen off timer: index into OFF_MS (default 1 minute)
String apPass = AP_PASS;
String webPin;   // 4 numbers the phone must know to use the web page (Settings > Wi-Fi)
int statSel = 0, heatDays = 7;
int kpAct = -1; String kpVal;
int flashIdx = -1; uint32_t flashUntil = 0;
bool qrWifi = true;
enum Power { P_ON, P_DIM, P_OFF };
Power pw = P_ON;
uint32_t lastTouchMs = 0;
std::map<String, uint32_t> remindedFor;  // กันเตือนซ้ำ

// touch
bool tDown = false, tMoved = false, tHandled = false, tWakeOnly = false;
int tX0, tY0, tScroll0; uint32_t tT0;

// forward declarations (default values live here only)
void txt(const lgfx::IFont* f, const String& s, int x, int y, uint32_t col, lgfx::textdatum_t d = D_TL);
void btn(int x, int y, int w, int h, const String& label, bool on, uint32_t onCol = INK);
void drawAppTitle(const String& title, const String& right = "", bool hasBtn = true);
void settingsUI(bool draw, int tx = -1, int ty = -1);
void runTouchCal();
void audioSetVolume();
void wake();
void ledFlash(uint32_t col, uint32_t ms);
void render();
void goScreen(Screen s);
void enterUpdateMode();
void newWebPin();
bool pinOk();
void wifiPageOpen();
void btPageOpen();
void joySetup();
void kbdOpen(const String& title, const String& start, void (*done)(const String&));

// ---------------- สี ----------------
static inline uint16_t C(uint32_t c) { return lgfx::color565((c >> 16) & 255, (c >> 8) & 255, c & 255); }
static uint32_t blend(uint32_t a, uint32_t b, float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  auto ch = [&](int s) { return (uint32_t)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) & 255; };
  return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

// ---------------- เวลา ----------------
time_t nowT() { return time(nullptr); }
String dayKey(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[24]; snprintf(b, sizeof b, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return b;
}
String hhmm(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[16]; snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
const char* EN_DOW[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* EN_DOW2[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
const char* EN_MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// While the real time is not known, the board runs on a guess (the last saved time).
// When the real time arrives, logs made on the guess are moved to the right time and day (see fixGuessedLogs).
uint32_t guessEpoch = 0, guessMs = 0;   // the guess, and millis() when it was made
volatile bool timeFixPending = false; volatile int32_t timeFixDelta = 0;
uint32_t guessNow() { return guessEpoch + (millis() - guessMs) / 1000; }
void onTimeSync(struct timeval*) {   // runs in the network task: only note it, loop() does the work
  if (timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)nowT() - guessNow()); timeFixPending = true; }
  timeApprox = false;
  dirty = true;
}
void setClock(uint32_t epoch, bool exact) {
  if (exact && timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)epoch - guessNow()); timeFixPending = true; }
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  if (exact) timeApprox = false;
  else { guessEpoch = epoch; guessMs = millis(); }
  prefs.putUInt("epoch", epoch);
}

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
void saveActs() {
  JsonDocument d; actsToJson(d.to<JsonArray>());
  File f = LittleFS.open("/acts.json", "w");
  if (f) { serializeJson(d, f); f.close(); }
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

// ---------------- LED ----------------
uint32_t ledColor = 0, ledUntil = 0;
void ledFlash(uint32_t col, uint32_t ms) { ledColor = col; ledUntil = millis() + ms; }
void ledTask() {
  static uint32_t lastSet = 1;
  uint32_t want = 0;
  if (millis() < ledUntil) want = ledColor;
  else {
    bool od = false;
    for (auto& a : acts) if (overdue(a)) od = true;
    if (od && (millis() % 4000) < 150) want = 0xFFFFFF;
  }
  if (want != lastSet) {
    rgbLedWrite(PIN_RGB, ((want >> 16) & 255) / 6, ((want >> 8) & 255) / 6, (want & 255) / 6);
    lastSet = want;
  }
}

// ---------------- บันทึก ----------------
std::vector<Ev> guessedEv;   // logs made this start while the time was a guess (moved when the real time arrives)
bool logFull = false;         // the last write failed: storage full
int remindAct = -1;           // the activity shown in the reminder bar (-1 = none)
void logEvent(int i, float v) {
  if (i < 0 || i >= (int)acts.size()) return;
  if (dayKey(nowT()) != curDay) loadDay();
  Ev e{(uint32_t)nowT(), v, place, !timeApprox, acts[i].id};
  todayEv.push_back(e);
  File f = logFs().open(logPath(curDay), "a");
  String line = evLine(e);
  logFull = !f || f.print(line) != line.length();
  if (f) f.close();
  if (timeApprox) guessedEv.push_back(e);
  totalsAdd(e.id, 1, v);
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
  int n = constrain((int)roundf(v), 1, 50);
  for (int k = 0; k < n; k++) logEvent(i, 1);
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
  return false;
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
    // 2) write them again at the right time (and so the right day)
    for (auto& e : guessedEv) {
      Ev r = e; r.t = (uint32_t)((int64_t)e.t + delta); r.ok = true;
      File f = logFs().open(logPath(dayKey(r.t)), "a");
      if (f) { f.print(evLine(r)); f.close(); }
    }
    Serial.printf("moved %d logs by %ld s\n", (int)guessedEv.size(), (long)delta);
  }
  guessedEv.clear();
}

// ---------------- แบตเตอรี่ ----------------
float batV = 0;
void readBattery() {
  uint32_t mv = 0; for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  batV = mv / 8.0f * 2.0f / 1000.0f;   // วงจรแบ่งแรงดัน 1/2
}
int batPct() { if (batV < 2.5f) return -1; int p = (int)((batV - 3.3f) / (4.15f - 3.3f) * 100); return constrain(p, 0, 100); }

// ---------------- Wi-Fi ----------------
// Two separate switches:
//   apOn  = board makes its own Wi-Fi "SomudTick" (phone joins it to open the web page)
//   staOn = board joins your home Wi-Fi / phone hotspot (internet: time, weather, news)
void setupWifi() {
  static bool mdnsUp = false;
  if (!apOn && !staOn) { WiFi.mode(WIFI_OFF); return; }
  WiFi.mode(apOn && staOn ? WIFI_AP_STA : apOn ? WIFI_AP : WIFI_STA);
  if (apOn) WiFi.softAP(AP_SSID, apPass.c_str());
  if (staOn) {
    WiFi.setAutoReconnect(true);
    if (staSsid.length() && WiFi.status() != WL_CONNECTED) WiFi.begin(staSsid.c_str(), staPass.c_str());
  }
  if (!mdnsUp) { mdnsUp = MDNS.begin(MDNS_NAME); if (mdnsUp) MDNS.addService("http", "tcp", 80); }
}
bool staOk() { return staOn && WiFi.status() == WL_CONNECTED; }

void autoPlaceTask() {
  static uint32_t lastScan = 0;
  if (!autoPlace || !staOn || (!homeSsid.length() && !uniSsid.length())) return;
  int n = WiFi.scanComplete();
  if (n >= 0) {
    bool home = false, uni = false;
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (homeSsid.length() && s == homeSsid) home = true;
      if (uniSsid.length() && s == uniSsid) uni = true;
    }
    WiFi.scanDelete();
    char np = place;
    if (home && !uni) np = 'H'; else if (uni && !home) np = 'U';
    if (np != place) { place = np; prefs.putChar("place", place); dirty = true; }
  } else if (n == WIFI_SCAN_FAILED && millis() - lastScan > 60000UL) {
    lastScan = millis();
    WiFi.scanNetworks(true);
  }
}

// ---------------- Theme / rotation ----------------
void applyTheme() {
  if (!themeDark) {
    PAPER = 0xF2F2F2; CARD = 0xFFFFFF; SOFT = 0x6E6E6E; LINE = 0xD0D0D0; KEYBG = 0xE8E8E8; MUTE = 0xBDBDBD;
    HDRBG = 0x111111; HDRFG = 0xFFFFFF; HDRSOFT = 0xBDBDBD; ONINK = 0xFFFFFF;
  } else {
    PAPER = 0x000000; CARD = 0x161616; SOFT = 0x9A9A9A; LINE = 0x333333; KEYBG = 0x2A2A2A; MUTE = 0x5A5A5A;
    HDRBG = 0x262626; HDRFG = 0xFFFFFF; HDRSOFT = 0x9A9A9A; ONINK = 0x000000;
  }
  INK = themeDark ? TEXTCOLS[textColIdx].dark : TEXTCOLS[textColIdx].light;
  dirty = true;
}
void applyRotation() {
  lcd.setRotation(rot);
  W = lcd.width(); H = lcd.height();
  FTR_Y = H - 30; CONT_H = FTR_Y - HDR_H;
  spr.deleteSprite();
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("sprite alloc failed (is OPI PSRAM enabled?)");
  scrollY = heatScroll = setScroll = filesScroll = acScroll = 0;
  dirty = true;
}

// ---------------- Drawing helpers ----------------
// English text uses sharp built-in fonts. Text with Thai letters (e.g. a Thai activity name) falls back to the Thai font.
const lgfx::IFont* FS = &fonts::FreeSans9pt7b;
const lgfx::IFont* FB = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* FL = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* FXL = &fonts::FreeSansBold24pt7b;
bool hasThai(const String& s) { for (size_t i = 0; i < s.length(); i++) if ((uint8_t)s[i] >= 0x80) return true; return false; }
bool forceTh = false;   // true = draw with the Thai font even if this line has no Thai (keeps wrapped lines the same size)
const lgfx::IFont* pickFont(const lgfx::IFont* f, const String& s) {
  if (!hasThai(s) && (!forceTh || (f != FS && f != FB))) return f;
  return f == FS ? (const lgfx::IFont*)&fS : (const lgfx::IFont*)&fB;
}
void txt(const lgfx::IFont* f, const String& s, int x, int y, uint32_t col, lgfx::textdatum_t d) {
  spr.setFont(pickFont(f, s)); spr.setTextColor(C(col)); spr.setTextDatum(d); spr.drawString(s, x, y);
}
String fitText(const lgfx::IFont* f, String s, int w) {
  spr.setFont(pickFont(f, s));
  if (spr.textWidth(s) <= w) return s;
  while (s.length()) {
    int i = s.length() - 1;
    while (i > 0 && (s[i] & 0xC0) == 0x80) i--;
    s.remove(i);
    if (spr.textWidth(s + "..") <= w) return s + "..";
  }
  return s;
}
void btn(int x, int y, int w, int h, const String& label, bool on, uint32_t onCol) {
  spr.fillRoundRect(x, y, w, h, 8, C(on ? onCol : CARD));
  spr.drawRoundRect(x, y, w, h, 8, C(on ? onCol : LINE));
  spr.setFont(pickFont(FB, label));
  bool small = spr.textWidth(label) > w - 8;   // narrow button -> smaller font
  txt(small ? FS : FB, small ? fitText(FS, label, w - 4) : label, x + w / 2, y + h / 2, on ? ONINK : INK, D_MC);
}
void scrollBar(int top, int viewH, int pos, int maxPos) {
  if (maxPos <= 0) return;
  int bh = viewH * viewH / (viewH + maxPos);
  int by = top + (viewH - bh) * pos / maxPos;
  spr.fillRoundRect(W - 4, by, 3, bh, 1, C(MUTE));
}
// LovyanGFX drawWideLine() removes the clip box when it finishes (library bug),
// so later drawing could spill over the title. This keeps the clip box.
void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) {
  int32_t cx, cy, cw, ch; spr.getClipRect(&cx, &cy, &cw, &ch);
  spr.drawWideLine(x0, y0, x1, y1, r, c);
  spr.setClipRect(cx, cy, cw, ch);
}
bool hitR(int tx, int ty, int x, int y, int w, int h) { return tx >= x && tx < x + w && ty >= y && ty < y + h; }

int hdrChipX0 = 0, hdrChipX1 = 0;   // where the Home/Uni chip is (for taps)
void drawHeader() {
  spr.fillRect(0, 0, W, HDR_H, C(HDRBG));
  time_t n = nowT(); struct tm tm; localtime_r(&n, &tm);
  // left: clock
  String ts = (timeApprox ? "~" : "") + hhmm(n);
  txt(FB, ts, 5, HDR_H / 2, timeApprox ? HDRSOFT : HDRFG, D_ML);
  spr.setFont(FB); int left = 5 + spr.textWidth(ts) + 7;
  // right: battery, Wi-Fi dot, place chip (measured, so nothing overlaps)
  int p = batPct();
  String bs = p < 0 ? String("USB") : String(p) + "%";
  txt(FS, bs, W - 4, HDR_H / 2, (p >= 0 && p < 15) ? HDRSOFT : HDRFG, D_MR);
  spr.setFont(FS); int x = W - 4 - spr.textWidth(bs) - 9;
  if (staOk()) spr.fillCircle(x, 15, 3, C(HDRFG)); else if (staOn) spr.drawCircle(x, 15, 3, C(HDRSOFT));
  x -= 8;
  String chip = String(place == 'H' ? "Home" : "Uni") + (autoPlace ? " A" : "");
  spr.setFont(FS); int cw = spr.textWidth(chip) + 14;
  hdrChipX0 = x - cw; hdrChipX1 = x;
  spr.fillRoundRect(hdrChipX0, 5, cw, 20, 10, C(HDRFG));
  txt(FS, chip, hdrChipX0 + cw / 2, 15, HDRBG, D_MC);
  // middle: date (only when the clock is right)
  // the longest text that fits: "Thu 24 Sep" -> "24 Sep" -> "24/9" (tall screen has little room)
  int room = hdrChipX0 - 6 - left;
  String opts[3];
  if (timeApprox) { opts[0] = "time not set"; opts[1] = "set time"; opts[2] = "time?"; }
  else { opts[0] = String(EN_DOW[tm.tm_wday]) + " " + tm.tm_mday + " " + EN_MON[tm.tm_mon]; opts[1] = String(tm.tm_mday) + " " + EN_MON[tm.tm_mon]; opts[2] = String(tm.tm_mday) + "/" + (tm.tm_mon + 1); }
  spr.setFont(FS);
  for (auto& o : opts) if ((int)spr.textWidth(o) <= room) { txt(FS, o, left, HDR_H / 2, timeApprox ? HDRFG : HDRSOFT, D_ML); break; }
}
// v10: 3 tabs. Stats opens from a button on the Log page, so it belongs to the Log tab.
const int N_TABS = 3;
int tabOf(Screen s) {
  switch (s) { case S_HOME: case S_KEYPAD: case S_STATS: return 0; case S_SET: case S_WIFI: case S_BT: case S_KBD: return 2; default: return 1; }
}
const char* FTR_TABS[N_TABS] = {"Log", "Apps", "Settings"};
void footerTabs(int* x0) {   // x0[0..N_TABS] = tab edges
  x0[0] = 0;
  for (int i = 1; i <= N_TABS; i++) x0[i] = W * i / N_TABS;
}
void drawFooter() {
  int x0[N_TABS + 1]; footerTabs(x0);
  int cur = tabOf(scr);
  spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(CARD));
  spr.drawFastHLine(0, FTR_Y, W, C(LINE));
  for (int i = 0; i < N_TABS; i++) {
    bool on = cur == i;
    int w = x0[i + 1] - x0[i];
    if (on) spr.fillRoundRect(x0[i] + 3, FTR_Y + 4, w - 6, 22, 8, C(INK));
    txt(FS, FTR_TABS[i], x0[i] + w / 2, FTR_Y + 15, on ? ONINK : SOFT, D_MC);
  }
}
// small title bar with a Back button, used by app screens
void drawAppTitle(const String& title, const String& right, bool hasBtn) {
  spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG));
  spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK));
  txt(FS, "Back", 29, HDR_H + 17, INK, D_ML);
  int rw = 0;
  if (right.length()) { spr.setFont(pickFont(FS, right)); rw = spr.textWidth(right) + 10; txt(FS, right, W - 8, HDR_H + 17, SOFT, D_MR); }
  txt(FB, fitText(FB, title, W - 8 - 78 - (rw ? rw : hasBtn ? 66 : 0)), 78, HDR_H + 17, INK, D_ML);   // hasBtn: leave room for a button on the right
}
bool backHit(int x, int y) { return y >= HDR_H && y < HDR_H + 34 && x < 74; }

// ---------------- Log (home) ----------------
// Tiles: 2 across on a tall screen, 3 across on a wide screen.
//  ● Name              ✓
//  12 /8
//  2h ago
//  [ - ][   + 1       ]   <- the + button fills up with colour as you get near your goal
const int SUM_H = 30;   // "today" line on top
int homeCols() { return land() ? 3 : 2; }
int tileW() { return (W - 12 - 6 * (homeCols() - 1)) / homeCols(); }
const int TILE_H = 100, TILE_GAP = 6;
void homeTileRect(int i, int& x, int& y) {
  x = 6 + (i % homeCols()) * (tileW() + TILE_GAP);
  y = HDR_H + 4 + SUM_H + (i / homeCols()) * (TILE_H + TILE_GAP) - scrollY;
}
int homeMaxScroll() {
  int rows = ((int)acts.size() + homeCols() - 1) / homeCols();
  int m = SUM_H + rows * (TILE_H + TILE_GAP) + 4 - CONT_H;
  return m > 0 ? m : 0;
}
int homeTileAt(int x, int y) {
  for (size_t i = 0; i < acts.size(); i++) { int tx, ty; homeTileRect(i, tx, ty); if (hitR(x, y, tx, ty, tileW(), TILE_H)) return i; }
  return -1;
}
void drawCheck(int x, int y, uint32_t c) { wideLine(x, y + 4, x + 3, y + 7, 1.2f, C(c)); wideLine(x + 3, y + 7, x + 9, y, 1.2f, C(c)); }
void drawHome() {
  scrollY = constrain(scrollY, 0, homeMaxScroll());
  spr.setClipRect(0, HDR_H, W, CONT_H);
  // today line: how many goals are done, one dot per activity that has a goal
  {
    int y = HDR_H + 4 - scrollY, goals = 0, done = 0;
    for (auto& a : acts) if (a.goal > 0 && a.goalType) { goals++; int g = goalState(a, measure(a, sumFor(a.id))); if (g == 2 || g == 3 || (a.goalType == 2 && g == 1)) done++; }
    String t = goals ? String("Goals ") + done + " / " + goals : String("Today");
    txt(FB, t, 8, y + SUM_H / 2 - 2, INK, D_ML);
    spr.setFont(FB); int dx = 8 + spr.textWidth(t) + 10;
    // Stats button (Stats used to be a tab)
    spr.fillRoundRect(W - 70, y + 1, 64, 24, 8, C(INK));
    for (int k = 0; k < 3; k++) spr.fillRect(W - 64 + k * 4, y + 17 - k * 4, 3, 4 + k * 4, C(ONINK));   // tiny bar chart
    txt(FS, "Stats", W - 50, y + 13, ONINK, D_ML);
    for (auto& a : acts) {
      if (!(a.goal > 0 && a.goalType) || dx > W - 84) continue;
      int g = goalState(a, measure(a, sumFor(a.id)));
      bool ok = g == 2 || g == 3 || (a.goalType == 2 && g == 1);
      if (g == 4) { spr.fillCircle(dx, y + SUM_H / 2 - 2, 5, C(INK)); spr.fillRect(dx - 3, y + SUM_H / 2 - 3, 7, 2, C(PAPER)); }   // over the limit
      else if (ok) spr.fillCircle(dx, y + SUM_H / 2 - 2, 5, C(a.color));
      else spr.drawCircle(dx, y + SUM_H / 2 - 2, 5, C(a.color));
      dx += 14;
    }
  }
  const int tw = tileW();
  for (size_t i = 0; i < acts.size(); i++) {
    int x, y; homeTileRect(i, x, y);
    if (y > FTR_Y || y + TILE_H < HDR_H) continue;
    Act& a = acts[i];
    Sum s = sumFor(a.id);
    float m = measure(a, s);
    int gs = goalState(a, m);
    bool od = overdue(a), over = gs == 4;
    uint32_t bg = over ? INK : (gs == 2 ? blend(CARD, a.color, 0.18f) : CARD);
    if ((int)i == flashIdx && millis() < flashUntil) bg = blend(CARD, a.color, 0.45f);
    uint32_t fg = over ? ONINK : INK, fg2 = over ? blend(INK, ONINK, 0.7f) : SOFT;
    spr.fillRoundRect(x, y, tw, TILE_H, 12, C(bg));
    uint32_t bd = od ? INK : ((gs == 2 || gs == 3) ? a.color : LINE);
    spr.drawRoundRect(x, y, tw, TILE_H, 12, C(bd));
    if (od || gs == 3) spr.drawRoundRect(x + 1, y + 1, tw - 2, TILE_H - 2, 11, C(bd));
    // name with a colour dot
    spr.fillCircle(x + 12, y + 14, 4, C(a.color));
    bool mark = gs == 2;
    txt(FB, fitText(FB, a.name, tw - 26 - (mark ? 14 : 0)), x + 21, y + 14, fg, D_ML);
    if (mark) drawCheck(x + tw - 18, y + 10, a.color);
    // number + goal / unit
    String big = isUnitAct(a) ? fmtNum(s.sum) : String(s.count);
    txt(FL, big, x + 9, y + 27, over ? ONINK : a.color);
    spr.setFont(FL); int bw = spr.textWidth(big);
    String suf;
    if (a.goal > 0 && a.goalType) suf = (a.goalType == 1 ? "/" : "max ") + fmtNum(a.goal);
    if (isUnitAct(a)) suf += (suf.length() ? " " : "") + a.unit;
    if (suf.length()) {   // "max 500 ml" -> "max 500" -> "/500": never cut the number off
      spr.setFont(FS);
      if ((int)spr.textWidth(suf) > tw - bw - 20 && a.goal > 0 && a.goalType) suf = (a.goalType == 1 ? "/" : "max ") + fmtNum(a.goal);
      if ((int)spr.textWidth(suf) > tw - bw - 20 && a.goal > 0 && a.goalType) suf = "/" + fmtNum(a.goal);
      txt(FS, fitText(FS, suf, tw - bw - 20), x + 13 + bw, y + 34, fg2);
    }
    // goal bar (thin line): fills up to the goal; for a "max" goal it turns dark when you go over
    if (a.goal > 0 && a.goalType) {
      float r = min(1.0f, m / a.goal);
      int bx = x + 9, bw2 = tw - 18, byy = y + 47;
      spr.fillRoundRect(bx, byy, bw2, 4, 2, C(over ? blend(INK, ONINK, 0.35f) : blend(CARD, a.color, 0.25f)));
      if (r > 0) spr.fillRoundRect(bx, byy, max(4, (int)(bw2 * r)), 4, 2, C(over ? ONINK : a.color));
    }
    // time since last
    uint32_t last = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
    {   // "1h 3m ago" -> "1h 3m" -> "1h": no cut words on a narrow tile
      String ag = (od ? "! " : "") + agoStr(last);
      spr.setFont(FS);
      if ((int)spr.textWidth(ag) > tw - 16 && ag.endsWith(" ago")) ag = ag.substring(0, ag.length() - 4);
      if ((int)spr.textWidth(ag) > tw - 16 && ag.indexOf('h') > 0) ag = ag.substring(0, ag.indexOf('h') + 1);
      txt(FS, fitText(FS, ag, tw - 16), x + 9, y + 56, od ? fg : fg2);
    }
    // buttons: [-] undo, [+] add
    int by = y + TILE_H - 28, bh = 24;
    bool can = s.count > 0;
    uint32_t mb = over ? blend(INK, ONINK, 0.2f) : KEYBG;
    spr.fillRoundRect(x + 5, by, 32, bh, 8, C(can ? mb : bg));
    spr.drawRoundRect(x + 5, by, 32, bh, 8, C(over ? blend(INK, ONINK, 0.35f) : LINE));
    spr.fillRect(x + 15, by + bh / 2 - 1, 12, 3, C(can ? fg : (over ? blend(INK, ONINK, 0.35f) : LINE)));
    int px = x + 41, pw_ = tw - 46;
    spr.fillRoundRect(px, by, pw_, bh, 8, C(a.color));
    String plus = "+" + fmtNum(isUnitAct(a) ? a.step : 1);
    txt(FB, fitText(FB, plus, pw_ - 6), px + pw_ / 2, by + bh / 2, 0xFFFFFF, D_MC);
  }
  if (acts.empty()) txt(FS, "No activities. Add them on the web page.", W / 2, HDR_H + 60, SOFT, D_MC);
  scrollBar(HDR_H, CONT_H, scrollY, homeMaxScroll());
  spr.clearClipRect();
}

// ---------------- Stats ----------------
void drawTri(int x, int y, bool left) {
  if (left) spr.fillTriangle(x + 10, y - 9, x + 10, y + 9, x, y, C(INK));
  else spr.fillTriangle(x, y - 9, x, y + 9, x + 10, y, C(INK));
}
String gapStr(double sec) {   // "1h 44m" (short, so the "Last" time fits next to it)
  long m = (long)(sec / 60);
  return (m >= 60 ? String(m / 60) + "h " : String("")) + String(m % 60) + "m";
}
// Stats has 3 views, picked with the buttons on top:
//   All     = every activity, one line each (tap a line to see Days)
//   Days    = one activity, last 7 days as bars
//   Hours   = what time of day you do things (was the "Hours" app)
int statView = 0, statScroll = 0, statMax = 0;
bool statTotalsOk = false;
const char* STAT_VIEWS[3] = {"All", "Days", "Hours"};
int statTop() { return HDR_H + 36; }
void statsNeed(int days) {   // make sure the numbers for `days` days are ready
  if (st.size() != acts.size()) statTotalsOk = false;
  if (st.size() == acts.size() && stDays == days && statTotalsOk) return;
  computeStats(days, !statTotalsOk);
  statTotalsOk = true;
}
// top row: [< Back] [All] [Days] [Hours]. Back goes to Log.
int statTabW() { return (W - 74 - 4 - 6) / 3; }   // 3 buttons after Back (gap 3)
void drawStatTabs() {
  spr.fillRoundRect(6, HDR_H + 4, 64, 26, 8, C(KEYBG));
  spr.fillTriangle(16, HDR_H + 17, 24, HDR_H + 11, 24, HDR_H + 23, C(INK));
  txt(FS, "Back", 29, HDR_H + 17, INK, D_ML);
  int w = statTabW();
  for (int i = 0; i < 3; i++) btn(74 + i * (w + 3), HDR_H + 4, w, 26, STAT_VIEWS[i], statView == i);
}
void drawStatSummary() {
  statsNeed(7);
  int top = statTop();
  const int RH = 46;
  statMax = max(0, (int)acts.size() * RH + 22 - (FTR_Y - top));
  statScroll = constrain(statScroll, 0, statMax);
  spr.setClipRect(0, top, W, FTR_Y - top);
  int y = top - statScroll;
  txt(FS, "Last 7 days", 10, y + 2, SOFT);
  txt(FS, "today", W - 12, y + 2, SOFT, D_TR);
  y += 22;
  const int bars = 7, bw = 6, bgap = 2, chartW = bars * (bw + bgap);
  for (size_t i = 0; i < acts.size(); i++, y += RH) {
    if (y > FTR_Y || y + RH < top) continue;
    Act& a = acts[i]; ActStat& s = st[i];
    auto val = [&](int di) { return isUnitAct(a) ? s.daily[di] : (float)s.dcnt[di]; };
    spr.fillRoundRect(6, y, W - 12, RH - 4, 10, C(CARD));
    spr.fillRoundRect(6, y, 5, RH - 4, 2, C(a.color));
    float wk = 0, mx = a.goal > 0 ? a.goal : 1;
    for (int d = 0; d < stDays; d++) { wk += val(d); mx = max(mx, val(d)); }
    // right: today big, left of it: 7 small bars
    String td = fmtNum(val(stDays - 1));
    txt(FL, td, W - 14, y + (RH - 4) / 2, a.color, D_MR);
    spr.setFont(FL); int tdw = max(24, (int)spr.textWidth(td));
    int cx = W - 14 - tdw - 10 - chartW, ch = RH - 16;
    for (int d = 0; d < stDays; d++) {
      int h = max(1, (int)(ch * val(d) / mx));
      spr.fillRect(cx + d * (bw + bgap), y + 6 + ch - h, bw, h, C(d == stDays - 1 ? a.color : blend(CARD, a.color, 0.5f)));
    }
    int nameW = cx - 18 - 6;
    txt(FB, fitText(FB, a.name, nameW), 16, y + 5, INK);
    String sub = "avg " + fmtNum(wk / 7.0f) + (isUnitAct(a) ? " " + a.unit : String(""));
    txt(FS, fitText(FS, sub, nameW), 16, y + 23, SOFT);
  }
  scrollBar(top, FTR_Y - top, statScroll, statMax);
  spr.clearClipRect();
}
void drawStatDays() {
  statsNeed(7);
  statSel = constrain(statSel, 0, (int)acts.size() - 1);
  Act& a = acts[statSel];
  ActStat& s = st[statSel];
  const int top = statTop();
  const int selY = top + 12, boxY = top + 28, boxH = land() ? 42 : 52;
  const int infoY = FTR_Y - 20, chartTop = boxY + boxH + 6, chartBot = infoY - 6;
  drawTri(10, selY, true); drawTri(W - 20, selY, false);
  txt(FB, fitText(FB, a.name, W - 70), W / 2, selY, a.color, D_MC);
  auto val = [&](int di) { return isUnitAct(a) ? s.daily[di] : (float)s.dcnt[di]; };
  float wk = 0;
  for (int i = 0; i < stDays; i++) wk += val(i);
  float tot = isUnitAct(a) ? s.total : s.totalCnt;
  // short words so they fit a narrow box: Week = last 7 days, Avg = average per day, Total = all time
  String boxes[4][2] = {{fmtNum(val(stDays - 1)), "Today"}, {fmtNum(wk), "Week"},
                        {fmtNum(wk / 7.0f), "Avg"}, {fmtNum(tot), "Total"}};
  // tall screen: 4 boxes in a row, chart under them. wide screen: 2x2 boxes on the left, chart on the right
  int bw = land() ? 60 : (W - 12) / 4, bh2 = boxH, chL = 6, chT = chartTop;
  if (land()) { bh2 = (chartBot - boxY - 4) / 2; chL = 6 + 2 * bw + 2; chT = boxY; }
  for (int i = 0; i < 4; i++) {
    int x = 6 + (land() ? (i % 2) : i) * bw, y = boxY + (land() ? (i / 2) * (bh2 + 4) : 0);
    spr.fillRoundRect(x, y, bw - 4, bh2, 8, C(CARD));
    int my = y + bh2 / 2;
    txt(FB, fitText(FB, boxes[i][0], bw - 8), x + (bw - 4) / 2, my - 18, a.color, D_TC);
    txt(FS, fitText(FS, boxes[i][1], bw - 5), x + (bw - 4) / 2, my + 3, SOFT, D_TC);
  }
  spr.fillRoundRect(chL, chT, W - 6 - chL, chartBot - chT, 10, C(CARD));
  int cx = chL + 8, cy = chT + 16, cw = W - 6 - chL - 16, ch = chartBot - 20 - cy;
  float mx = a.goal > 0 ? a.goal : 1;
  for (int i = 0; i < stDays; i++) mx = max(mx, val(i));
  int colW = cw / stDays;
  for (int i = 0; i < stDays; i++) {
    float v = val(i);
    int bh = (int)(ch * v / mx);
    int bx = cx + i * colW + 5, bwid = colW - 10;
    uint32_t col = a.color;
    if (a.goalType == 2 && a.goal > 0 && v > a.goal) col = INK;   // over limit = text-colour bar
    spr.fillRoundRect(bx, cy + ch - bh, bwid, max(bh, 1), 3, C(i == stDays - 1 ? col : blend(CARD, col, 0.6f)));
    if (v > 0) txt(FS, fmtNum(v), bx + bwid / 2, cy + ch - bh - 1, SOFT, lgfx::textdatum::bottom_center);
    struct tm tm = {}; int yy, mm, dd; sscanf(stKeys[i].c_str(), "%d-%d-%d", &yy, &mm, &dd);
    tm.tm_year = yy - 1900; tm.tm_mon = mm - 1; tm.tm_mday = dd; tm.tm_hour = 12; mktime(&tm);
    const char* D1[] = {"S", "M", "T", "W", "T", "F", "S"};   // narrow chart: one letter
    txt(FS, colW < 27 ? D1[tm.tm_wday] : EN_DOW2[tm.tm_wday], bx + bwid / 2, cy + ch + 3, SOFT, D_TC);
  }
  if (a.goal > 0 && a.goalType) {
    int gy = cy + ch - (int)(ch * a.goal / mx);
    for (int x = cx; x < cx + cw; x += 8) spr.drawFastHLine(x, gy, 4, C(INK));
  }
  // bottom line: left "Avg gap 1 h 44 min", right "Last 19:12" (or "Last Tue"). Each gets its own half: no overlap
  uint32_t last = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
  String ls = "";
  if (last) { time_t lt = last; struct tm tl; localtime_r(&lt, &tl); ls = "Last " + (dayKey(lt) == curDay ? hhmm(last) : String(EN_DOW[tl.tm_wday])); }
  spr.setFont(FS); int lw = ls.length() ? spr.textWidth(ls) + 10 : 0;
  String gp = s.gapN ? gapStr(s.gapSum / s.gapN) : String("-");
  String gl = "Avg gap " + gp;
  if ((int)spr.textWidth(gl) > W - 20 - lw) gl = "Gap " + gp;   // tall screen: short word, keep the number
  txt(FS, fitText(FS, gl, W - 20 - lw), 10, infoY, SOFT);
  if (ls.length()) txt(FS, ls, W - 10, infoY, SOFT, D_TR);
}
void drawStatHours() {
  statsNeed(heatDays);
  int top = statTop();
  // 7 / 30 days switch on the right of the hour numbers
  spr.fillRoundRect(W - 78, top, 72, 22, 11, C(INK));
  txt(FS, String(heatDays) + " days", W - 42, top + 11, ONINK, D_MC);
  const int gx = 64, cw = (W - gx - 6) / 24, rh = land() ? 20 : 24, gtop = top + 42;
  const int hrs[] = {0, 6, 12, 18};
  for (int h : hrs) txt(FS, String(h), gx + h * cw, top + 24, SOFT, D_TL);
  int maxScroll = max(0, (int)acts.size() * rh - (FTR_Y - gtop));
  heatScroll = constrain(heatScroll, 0, maxScroll);
  spr.drawFastVLine(gx + 12 * cw - 1, gtop - 2, FTR_Y - gtop + 2, C(LINE));
  spr.setClipRect(0, gtop, W, FTR_Y - gtop);
  for (size_t i = 0; i < acts.size(); i++) {
    int y = gtop + i * rh - heatScroll;
    if (y > FTR_Y || y + rh < gtop) continue;
    uint16_t mxh = 1; for (int h = 0; h < 24; h++) mxh = max(mxh, st[i].hours[h]);
    txt(FS, fitText(FS, acts[i].name, gx - 8), 6, y + rh / 2 - 2, INK, D_ML);
    for (int h = 0; h < 24; h++) {
      uint16_t c = st[i].hours[h];
      uint32_t col = c ? blend(CARD, acts[i].color, 0.25f + 0.75f * c / mxh) : LINE;
      spr.fillRect(gx + h * cw, y + 2, cw - 1, rh - 8, C(col));
    }
  }
  scrollBar(gtop, FTR_Y - gtop, heatScroll, maxScroll);
  spr.clearClipRect();
}
void drawStats() {
  drawStatTabs();
  if (acts.empty()) { txt(FS, "No activities", W / 2, 120, SOFT, D_MC); return; }
  if (statView == 0) drawStatSummary();
  else if (statView == 1) drawStatDays();
  else drawStatHours();
}
void statsTap(int x, int y) {
  if (y < HDR_H + 32) {   // Back, then the view buttons
    if (x < 74) { goScreen(S_HOME); return; }
    int w = statTabW(), v = (x - 74) / (w + 3);
    if (v >= 0 && v < 3 && v != statView) { statView = v; statScroll = heatScroll = 0; }
    dirty = true; return;
  }
  if (acts.empty()) return;
  int top = statTop();
  if (statView == 0) {
    int i = (y - top - 22 + statScroll) / 46;
    if (y >= top + 22 && i >= 0 && i < (int)acts.size()) { statSel = i; statView = 1; dirty = true; }
  } else if (statView == 1) {
    if (y < top + 26) {
      if (x < 60) statSel = (statSel + acts.size() - 1) % acts.size();
      else if (x > W - 60) statSel = (statSel + 1) % acts.size();
      dirty = true;
    }
  } else if (y < top + 24 && x > W - 84) { heatDays = heatDays == 7 ? 30 : 7; dirty = true; }
}

// ---------------- Settings ----------------
// One function both draws and handles taps, so buttons and touch areas always match.


// Settings: a main page (the things you change most + a list of groups) and one page per group.
int setPage = 0;   // 0 main, 1 Screen, 2 Wi-Fi & phone, 3 About
const char* SET_PAGE_N[4] = {"Settings", "Screen", "Wi-Fi", "About"};
void setOpenPage(int p) { setPage = p; setScroll = 0; dirty = true; }
void setScreenPage() { setOpenPage(1); }
void setWifiPage() { setOpenPage(2); }
bool sdMount();
void setAboutPage() { sdMount(); setOpenPage(3); }
extern bool sdOk;
void settingsUI(bool draw, int tx, int ty) {
  const int X = 8, CW = W - 16;
  int y = HDR_H + (setPage ? 40 : 6) - setScroll;
  bool hit = false;
  auto label = [&](const String& s) { if (draw) txt(FS, fitText(FS, s, CW), X, y, SOFT); y += 20; };
  auto seg = [&](int n, int sel, const char* const* names, std::function<void(int)> act) {
    int gap = n >= 4 ? 4 : 6, w = (CW - (n - 1) * gap) / n;
    for (int i = 0; i < n; i++) {
      int x = X + i * (w + gap);
      if (draw) btn(x, y, w, 32, names[i], i == sel);
      else if (!hit && hitR(tx, ty, x, y, w, 32)) { act(i); hit = true; }
    }
    y += 40;
  };
  auto bar5 = [&](int level, std::function<void(int)> set) {   // [-] 5 blocks [+]
    if (draw) {
      btn(X, y, 40, 32, "-", false);
      btn(X + CW - 40, y, 40, 32, "+", false);
      int bx = X + 48, bw = CW - 96, sw = (bw - 4 * 4) / 5;
      for (int i = 0; i < 5; i++) spr.fillRoundRect(bx + i * (sw + 4), y + 8, sw, 16, 4, C(i <= level ? INK : LINE));
    } else if (!hit && hitR(tx, ty, X, y, CW, 32)) {
      int v = level;
      if (tx < X + 48) v--;
      else if (tx > X + CW - 48) v++;
      else v = (tx - X - 48) * 5 / (CW - 96);
      set(constrain(v, -1, 4)); hit = true;
    }
    y += 40;
  };
  // a row that opens a page: [icon] Title .......... value  >
  auto navRow = [&](const String& t, const String& val, void (*fn)()) {
    if (draw) {
      spr.fillRoundRect(X, y, CW, 38, 10, C(CARD));
      spr.drawRoundRect(X, y, CW, 38, 10, C(LINE));
      txt(FB, fitText(FB, t, CW - 30), X + 12, y + 19, INK, D_ML);
      spr.setFont(FB); int tw = spr.textWidth(fitText(FB, t, CW - 30));
      if (val.length()) txt(FS, fitText(FS, val, CW - tw - 44), X + CW - 22, y + 19, SOFT, D_MR);
      spr.fillTriangle(X + CW - 14, y + 13, X + CW - 14, y + 25, X + CW - 8, y + 19, C(SOFT));
    } else if (!hit && hitR(tx, ty, X, y, CW, 38)) { hit = true; fn(); }
    y += 44;
  };
  if (setPage == 0) {
    label("Where am I now?");
    { const char* n[] = {"Home", "Uni"};
      seg(2, place == 'H' ? 0 : 1, n, [](int i) { place = i ? 'U' : 'H'; prefs.putChar("place", place); }); }
    bool canAuto = homeSsid.length() || uniSsid.length();
    if (draw) txt(FS, fitText(FS, canAuto ? (String("Auto switch by Wi-Fi: ") + (autoPlace ? "ON" : "OFF"))
                                           : String("Auto Home/Uni: set on web"), CW), X, y - 4, canAuto ? INK : SOFT);
    else if (!hit && canAuto && hitR(tx, ty, 0, y - 8, W, 24)) { autoPlace = !autoPlace; prefs.putBool("auto", autoPlace); hit = true; }
    y += 24;
    label("Brightness");
    bar5(brightIdx, [](int v) { brightIdx = max(0, v); prefs.putUChar("bright", brightIdx); lcd.setBrightness(BRIGHT[brightIdx]); });
    label(String("Sound volume") + (volIdx < 0 ? " (off)" : ""));
    bar5(volIdx, [](int v) { volIdx = v; prefs.putChar("vol", volIdx); audioSetVolume(); });
    label("More");
    const char* dirN[4] = {"Tall", "Wide", "Tall (flip)", "Wide (flip)"};
    navRow("Screen", String(dirN[rot]) + ", " + (themeDark ? "Dark" : "Light"), setScreenPage);
    navRow("Wi-Fi", String(apOn ? "Hotspot" : "") + (apOn && staOn ? ", " : "") + (staOn ? (staOk() ? "Net OK" : "no net") : "") + (!apOn && !staOn ? "OFF" : ""), setWifiPage);
    navRow("Bluetooth", "", btPageOpen);
    navRow("Joystick direction", "", joySetup);
    if (draw) { btn(X, y, CW, 38, "Fix touch (calibrate)", false); }
    else if (!hit && hitR(tx, ty, X, y, CW, 38)) { hit = true; runTouchCal(); return; }
    y += 44;
    navRow("About", timeApprox ? "time not set!" : String(batPct() < 0 ? "USB" : String(batPct()) + "%"), setAboutPage);
  } else if (setPage == 1) {
    label("Screen off after (no touch)");
    seg(4, offIdx, OFF_NAMES, [](int i) { offIdx = i; prefs.putUChar("offT", offIdx); });
    label("Screen direction");
    { const char* n1[] = {"Tall", "Wide"}, *n2[] = {"Tall (flip)", "Wide (flip)"};   // flip = upside down
      seg(2, rot < 2 ? rot : -1, n1, [](int i) { rot = i; prefs.putUChar("rot", rot); applyRotation(); });
      seg(2, rot >= 2 ? rot - 2 : -1, n2, [](int i) { rot = i + 2; prefs.putUChar("rot", rot); applyRotation(); }); }
    label("Theme");
    { const char* n[] = {"Light", "Dark"};
      seg(2, themeDark, n, [](int i) { themeDark = i; prefs.putUChar("dark", themeDark); applyTheme(); }); }
    label("Text color");
    {
      int sw = CW / N_TEXTCOLS;
      for (int i = 0; i < N_TEXTCOLS; i++) {
        int cx = X + i * sw + sw / 2;
        uint32_t col = themeDark ? TEXTCOLS[i].dark : TEXTCOLS[i].light;
        if (draw) {
          if (i == textColIdx) { spr.drawCircle(cx, y + 16, 17, C(INK)); spr.drawCircle(cx, y + 16, 16, C(INK)); }
          spr.fillCircle(cx, y + 16, 13, C(col));
        } else if (!hit && hitR(tx, ty, X + i * sw, y, sw, 36)) {
          textColIdx = i; prefs.putUChar("tcol", textColIdx); applyTheme(); hit = true;
        }
      }
      y += 42;
    }
  } else if (setPage == 2) {
    label("Phone link (own Wi-Fi)");
    if (draw) btn(X, y, CW, 32, apOn ? "Hotspot " AP_SSID ": ON" : "Hotspot " AP_SSID ": OFF", apOn);
    else if (!hit && hitR(tx, ty, X, y, CW, 32)) { apOn = !apOn; prefs.putBool("ap", apOn); setupWifi(); hit = true; }
    y += 40;
    if (apOn || staOk()) {
      if (draw) {
        bool wq = qrWifi && apOn;
        String qr = wq ? String("WIFI:T:WPA;S:" AP_SSID ";P:") + apPass + ";;"
                       : String("http://") + (staOk() ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "/";
        spr.fillRect(X, y, 92, 92, C(QRWHITE));
        spr.qrcode(qr.c_str(), X + 2, y + 2, 88, 1);
        int tx0 = X + 100, tw = W - tx0 - 4;
        String L[5];
        if (wq) { L[0] = "1) Join Wi-Fi"; L[1] = AP_SSID; L[2] = "pass " + apPass; L[4] = "tap QR: next"; }
        else if (apOn) { L[0] = "2) Open web"; L[1] = WiFi.softAPIP().toString(); if (staOk()) L[2] = WiFi.localIP().toString(); L[4] = "tap QR: back"; }
        else { L[0] = "Open web"; L[1] = WiFi.localIP().toString(); L[2] = "phone must be"; L[3] = "on same Wi-Fi"; }
        for (int k = 0; k < 5; k++) if (L[k].length()) txt(k ? FS : FB, fitText(k ? FS : FB, L[k], tw), tx0, y + k * 19, k ? SOFT : INK);
      } else if (!hit && hitR(tx, ty, X, y, 96, 92)) { qrWifi = !qrWifi; hit = true; }
      y += 100;
    } else {
      if (draw) txt(FS, fitText(FS, "Off: the phone web page can't open", CW), X, y - 4, SOFT);
      y += 20;
    }
    label("Web page PIN");
    if (draw) {
      spr.fillRoundRect(X, y, CW - 76, 32, 8, C(CARD));
      txt(FL, webPin, X + 12, y + 16, INK, D_ML);
      btn(X + CW - 70, y, 70, 32, "New", false);
    } else if (!hit && hitR(tx, ty, X + CW - 70, y, 70, 32)) { newWebPin(); hit = true; }
    y += 40;
    label("Internet: join a Wi-Fi");
    if (draw) btn(X, y, CW, 32, staOn ? "Internet Wi-Fi: ON" : "Internet Wi-Fi: OFF", staOn);
    else if (!hit && hitR(tx, ty, X, y, CW, 32)) { staOn = !staOn; prefs.putBool("sta", staOn); setupWifi(); hit = true; }
    y += 40;
    if (staOn) {
      if (draw) txt(FS, fitText(FS, staOk() ? "Connected: " + WiFi.SSID() : (staSsid.length() ? "Not connected yet" : "No Wi-Fi chosen yet"), CW), X, y - 4, staOk() ? INK : SOFT);
      y += 20;
    }
    navRow("Choose Wi-Fi", staSsid, wifiPageOpen);
  } else {
    auto line = [&](const String& k, const String& v, bool warn) {
      if (draw) {
        spr.fillRoundRect(X, y, CW, 34, 8, C(CARD));
        txt(FS, k, X + 10, y + 17, SOFT, D_ML);
        spr.setFont(FS); int kw = spr.textWidth(k);
        txt(FB, fitText(FB, v, CW - kw - 26), X + CW - 10, y + 17, warn ? INK : INK, D_MR);
      }
      y += 38;
    };
    line("Time", timeApprox ? String("not set") : hhmm(nowT()) + (timeApprox ? "" : " (synced)"), timeApprox);
    if (draw && timeApprox) { txt(FS, fitText(FS, "Open the web page or join Wi-Fi", CW), X, y - 2, SOFT); }
    if (timeApprox) y += 20;
    line("Battery", batPct() < 0 ? String("USB power") : String(batPct()) + "%  (" + String(batV, 2) + " V)", false);
    uint64_t tb = sdOk ? SD_MMC.totalBytes() : 0, ub = sdOk ? SD_MMC.usedBytes() : 0;
    line("SD card", sdOk ? String((uint32_t)(ub / 1048576)) + " / " + String((uint32_t)(tb / 1048576)) + " MB" : String("not checked"), false);
    line("Web page", apOn ? WiFi.softAPIP().toString() : (staOk() ? WiFi.localIP().toString() : String("Wi-Fi off")), false);
    line("Log space", String(logPctCache) + "% used" + (logFsOk ? "" : " (old)"), logPctCache >= 90);
    line("Version", "SomudTick v10", false);
    if (draw) btn(X, y, CW, 34, "Update firmware", false);
    else if (!hit && hitR(tx, ty, X, y, CW, 34)) { hit = true; enterUpdateMode(); }
    y += 40;
    if (draw) txt(FS, fitText(FS, "Restarts ready for the flasher page", CW), X, y - 2, SOFT);
    y += 20;
  }
  setMax = max(0, y + setScroll - FTR_Y);
  if (hit) dirty = true;
}
void drawSettings() {
  setScroll = constrain(setScroll, 0, setMax);
  int top = setPage ? HDR_H + 34 : HDR_H;
  spr.setClipRect(0, top, W, FTR_Y - top);
  settingsUI(true);
  scrollBar(top, FTR_Y - top, setScroll, setMax);
  spr.clearClipRect();
  if (setPage) drawAppTitle(SET_PAGE_N[setPage]);
}

// ---------------- Number pad (tall: keys below / wide: keys on the right) ----------------
const char* KP_KEYS[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "Del"};
KpGeo kpGeo() {
  if (land()) return {140, 36, 56, 38, 60, 43, 124};
  return {6, 108, 72, 40, 78, 45, W - 16};
}
void drawKeypad() {
  if (kpAct < 0 || kpAct >= (int)acts.size()) { scr = S_HOME; return; }
  Act& a = acts[kpAct];
  KpGeo g = kpGeo();
  txt(FB, fitText(FB, a.name, g.panelW), 8, 38, a.color);
  spr.fillRoundRect(8, 60, g.panelW, 40, 10, C(CARD));
  spr.drawRoundRect(8, 60, g.panelW, 40, 10, C(a.color));
  String unit = isUnitAct(a) ? a.unit : String("times");
  if (land()) {
    txt(FL, kpVal.length() ? kpVal : String("0"), 8 + g.panelW - 10, 80, INK, D_MR);
    txt(FS, unit, 12, 108, SOFT);
    txt(FS, fitText(FS, "Type a number", g.panelW), 12, 130, SOFT);
  } else {
    txt(FL, kpVal.length() ? kpVal : String("0"), 176, 80, INK, D_MR);
    txt(FS, unit, 184, 80, SOFT, D_ML);
  }
  for (int k = 0; k < 12; k++) {
    int x = g.x0 + (k % 3) * g.px, y = g.y0 + (k / 3) * g.py;
    spr.fillRoundRect(x, y, g.kw, g.kh, 8, C(CARD));
    spr.drawRoundRect(x, y, g.kw, g.kh, 8, C(LINE));
    txt(k == 11 ? FB : FL, KP_KEYS[k], x + g.kw / 2, y + g.kh / 2, INK, D_MC);
  }
}
void drawKeypadFooter() {
  int bw = (W - 18) / 2;
  spr.fillRect(0, FTR_Y, W, H - FTR_Y, C(PAPER));
  spr.fillRoundRect(6, FTR_Y + 2, bw, 26, 8, C(KEYBG));
  txt(FB, "Cancel", 6 + bw / 2, FTR_Y + 15, INK, D_MC);
  spr.fillRoundRect(12 + bw, FTR_Y + 2, bw, 26, 8, C(INK));
  txt(FB, "Save", 12 + bw + bw / 2, FTR_Y + 15, ONINK, D_MC);
}

// ---------------- Apps menu ----------------
// 4 big tiles. "Games" opens a second page with 4 games.
enum AppIcon { IC_FILES, IC_AC, IC_GAMES, IC_NET, IC_SWIM, IC_SUDOKU, IC_SAND, IC_GARDEN };
const int N_APPS = 4;
const char* APP_NAMES[N_APPS] = {"Files", "AC Remote", "Games", "Internet"};
const char* APP_SUBS[N_APPS] = {"Photos, clips", "Air con", "4 games", "Weather, news"};
const AppIcon APP_ICONS[N_APPS] = {IC_FILES, IC_AC, IC_GAMES, IC_NET};
void appTileRect(int i, int& x, int& y, int& w, int& h) {
  int cols = 2, rows = 2, gap = 6;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (CONT_H - 12 - gap * (rows - 1)) / rows;
  x = 6 + (i % cols) * (w + gap); y = HDR_H + 6 + (i / cols) * (h + gap);
}
void drawAppIcon(int ic, int cx, int cy, uint32_t c) {
  switch (ic) {
    case IC_FILES: spr.fillRoundRect(cx - 16, cy - 10, 14, 6, 2, C(c)); spr.fillRoundRect(cx - 16, cy - 6, 32, 20, 3, C(c)); break;
    case IC_AC: for (int k = 0; k < 3; k++) { float a = k * PI / 3;
              wideLine(cx - 13 * cosf(a), cy - 13 * sinf(a), cx + 13 * cosf(a), cy + 13 * sinf(a), 2.5f, C(c)); } break;
    case IC_GAMES:   // game pad
      spr.fillRoundRect(cx - 18, cy - 9, 36, 20, 9, C(c));
      spr.fillRect(cx - 12, cy - 1, 9, 3, C(CARD)); spr.fillRect(cx - 9, cy - 4, 3, 9, C(CARD));
      spr.fillCircle(cx + 8, cy - 2, 2, C(CARD)); spr.fillCircle(cx + 12, cy + 3, 2, C(CARD)); break;
    case IC_NET: spr.drawCircle(cx, cy, 13, C(c)); spr.drawCircle(cx, cy, 12, C(c)); spr.drawEllipse(cx, cy, 5, 13, C(c));
            spr.drawFastHLine(cx - 13, cy, 26, C(c)); spr.drawFastHLine(cx - 11, cy - 6, 22, C(c)); spr.drawFastHLine(cx - 11, cy + 6, 22, C(c)); break;
    case IC_SWIM: spr.fillCircle(cx + 8, cy, 7, C(c)); spr.fillCircle(cx - 4, cy + 3, 5, C(blend(CARD, c, 0.6f)));
            spr.fillCircle(cx - 13, cy + 5, 4, C(blend(CARD, c, 0.35f))); break;
    case IC_SUDOKU: {   // 3x3 board, small digits sit inside their boxes
      const int cs = 10, x0 = cx - 15, y0 = cy - 15;
      for (int k = 0; k <= 3; k++) { spr.drawFastHLine(x0, y0 + k * cs, 3 * cs + 1, C(c)); spr.drawFastVLine(x0 + k * cs, y0, 3 * cs + 1, C(c)); }
      spr.drawRect(x0 - 1, y0 - 1, 3 * cs + 3, 3 * cs + 3, C(c));
      spr.setFont(&fonts::Font0); spr.setTextSize(1); spr.setTextColor(C(c)); spr.setTextDatum(D_MC);
      const char* d[3] = {"5", "3", "8"}; const int px[3] = {0, 2, 1}, py[3] = {0, 1, 2};
      for (int k = 0; k < 3; k++) spr.drawString(d[k], x0 + px[k] * cs + cs / 2 + 1, y0 + py[k] * cs + cs / 2 + 1);
      break; }
    case IC_SAND:    // a small pile with grains falling on it
      spr.fillTriangle(cx - 16, cy + 12, cx + 16, cy + 12, cx, cy - 2, C(c));
      for (int k = 0; k < 4; k++) spr.fillRect(cx - 2 + (k & 1) * 4, cy - 16 + k * 4, 3, 3, C(c));
      break;
    case IC_GARDEN:  // a little tree on the ground
      spr.fillRect(cx - 2, cy - 2, 4, 14, C(c));
      spr.fillCircle(cx, cy - 8, 9, C(c)); spr.fillCircle(cx - 8, cy - 2, 6, C(c)); spr.fillCircle(cx + 8, cy - 2, 6, C(c));
      spr.fillRect(cx - 16, cy + 12, 32, 3, C(c));
      break;
  }
}
void drawAppTile(int x, int y, int w, int h, int icon, const String& name, const String& sub) {
  spr.fillRoundRect(x, y, w, h, 12, C(CARD));
  spr.drawRoundRect(x, y, w, h, 12, C(LINE));
  bool tiny = h < 70;
  drawAppIcon(icon, x + w / 2, y + h / 2 - (tiny ? 10 : 16), INK);
  txt(FB, fitText(FB, name, w - 8), x + w / 2, y + h / 2 + (tiny ? 16 : 12), INK, D_MC);
  if (!tiny && sub.length()) txt(FS, fitText(FS, sub, w - 8), x + w / 2, y + h / 2 + 31, SOFT, D_MC);
}
void drawApps() {
  for (int i = 0; i < N_APPS; i++) { int x, y, w, h; appTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, APP_ICONS[i], APP_NAMES[i], APP_SUBS[i]); }
}
// Games page: 4 tiles (2 x 2) under a title bar
const int N_GAMES = 4;
void gamesTileRect(int i, int& x, int& y, int& w, int& h) {
  int top = HDR_H + 38, gap = 6;
  w = (W - 12 - gap) / 2; h = (FTR_Y - top - 6 - gap) / 2;
  x = 6 + (i % 2) * (w + gap); y = top + (i / 2) * (h + gap);
}
String gameSub(int i);   // (in the game files) a short line: best score / saved game / how the garden is
void drawGames() {
  drawAppTitle("Games");
  const char* n[N_GAMES] = {"Pixel Swim", "Sudoku", "Sand", "Garden"};
  const AppIcon ic[N_GAMES] = {IC_SWIM, IC_SUDOKU, IC_SAND, IC_GARDEN};
  for (int i = 0; i < N_GAMES; i++) { int x, y, w, h; gamesTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, ic[i], n[i], gameSub(i)); }
}

// ---------------- Apps ----------------
#include "app_media.h"
#include "app_ac.h"
#include "app_game.h"
#include "app_sudoku.h"
#include "app_sand.h"
#include "app_garden.h"
#include "app_net.h"
#include "app_connect.h"
#include "app_usb.h"

// reminder: a dark bar just above the tabs, "Time for Water  >". Tap it = go to Log.
bool remindBarShown = false; int remindBarY = 0;
void drawRemindBar() {
  remindBarShown = false;
  if (remindAct < 0 || remindAct >= (int)acts.size() || scr == S_KEYPAD || scr == S_KBD || scr == S_SUDOKU || scr == S_USB) return;
  if (scr == S_HOME) return;   // on Log you see the tile already
  remindBarY = FTR_Y - 34;
  spr.fillRoundRect(6, remindBarY, W - 12, 30, 10, C(INK));
  spr.fillCircle(20, remindBarY + 15, 5, C(acts[remindAct].color));
  txt(FB, fitText(FB, "Time for " + acts[remindAct].name, W - 60), 32, remindBarY + 15, ONINK, D_ML);
  txt(FB, ">", W - 18, remindBarY + 15, ONINK, D_MC);
  remindBarShown = true;
}
void render() {
  if (scr == S_SAND) { sandDraw(); dirty = false; return; }   // Sand & Water draws the whole screen itself
  spr.fillScreen(C(PAPER));
  switch (scr) {
    case S_HOME: drawHome(); break;
    case S_STATS: drawStats(); break;
    case S_APPS: drawApps(); break;
    case S_GAMES: drawGames(); break;
    case S_SET: drawSettings(); break;
    case S_KEYPAD: drawKeypad(); break;
    case S_FILES: drawFiles(); break;
    case S_AC: drawAC(); break;
    case S_SUDOKU: drawSudoku(); break;
    case S_NET: drawNet(); break;
    case S_WIFI: drawWifi(); break;
    case S_KBD: drawKbd(); break;
    case S_BT: drawBt(); break;
    case S_GARDEN: drawGarden(); break;
    case S_USB: drawUsb(); break;
    default: break;
  }
  if (!(scr == S_SUDOKU && land())) drawHeader();   // wide Sudoku uses the full height
  if (scr == S_KEYPAD) drawKeypadFooter(); else if (scr != S_SUDOKU && scr != S_KBD && scr != S_USB) drawFooter();   // Sudoku uses the whole screen; USB drive: stay on this page
  drawRemindBar();
  spr.pushSprite(0, 0);
  dirty = false;
}

// ---------------- Touch ----------------
void goScreen(Screen s) {
  if (s == S_SET) setPage = 0;   // Settings tab = main settings page
  if (s == S_STATS && scr != S_STATS) { statTotalsOk = false; st.clear(); }   // fresh numbers when you open Stats
  scr = s;
  dirty = true;
}
void refreshStatsIfVisible() {
  if (scr == S_STATS) { statTotalsOk = false; st.clear(); dirty = true; }
}
void onTap(int x, int y) {
  if (scr == S_GAME) { gameTap(x, y); return; }
  if (remindAct >= 0 && remindBarShown && y >= remindBarY && y < remindBarY + 30) {   // reminder bar: go to Log
    remindAct = -1; kpAct = -1; goScreen(S_HOME); return;
  }
  if (y < HDR_H) return;   // the Home / Uni chip switches on a long press (see onLongPress)
  if (scr == S_SUDOKU) { sudokuTap(x, y); return; }
  if (scr == S_KBD) { kbdTap(x, y); return; }
  if (scr == S_KEYPAD) {
    if (y >= FTR_Y) {
      if (x < W / 2) { scr = S_HOME; }
      else { logValue(kpAct, kpVal.toFloat()); scr = S_HOME; }
      dirty = true; return;
    }
    KpGeo g = kpGeo();
    if (x >= g.x0 && y >= g.y0) {
      int c = (x - g.x0) / g.px, r = (y - g.y0) / g.py;
      if (c > 2 || r > 3) return;
      int k = r * 3 + c;
      if (k == 11) { if (kpVal.length()) kpVal.remove(kpVal.length() - 1); }
      else if (k == 9) { if (kpVal.indexOf('.') < 0 && kpVal.length() < 8) kpVal += "."; }
      else if (kpVal.length() < 8) kpVal += KP_KEYS[k];
      dirty = true;
    }
    return;
  }
  if (y >= FTR_Y) {   // bottom tabs
    int x0[N_TABS + 1]; footerTabs(x0);
    int t = N_TABS - 1; for (int i = 0; i < N_TABS; i++) if (x < x0[i + 1]) { t = i; break; }
    const Screen TAB_SCR[N_TABS] = {S_HOME, S_APPS, S_SET};
    goScreen(TAB_SCR[t]);
    return;
  }
  switch (scr) {
    case S_HOME: {
      if (y < HDR_H + 4 + SUM_H - scrollY && x >= W - 76) { goScreen(S_STATS); break; }   // Stats button
      int i = homeTileAt(x, y);
      if (i < 0) return;
      int tx, ty; homeTileRect(i, tx, ty);
      int by = ty + TILE_H - 30;
      if (y >= by) {
        if (x >= tx + 40) logDefault(i);         // [+]
        else undoEvent(i);                       // [-]
      }
      // tapping the rest of the tile does nothing (no accidental logs in a pocket); hold it to type a value
      break;
    }
    case S_STATS: statsTap(x, y); break;
    case S_APPS:
      for (int i = 0; i < N_APPS; i++) {
        int ax, ay, aw, ah; appTileRect(i, ax, ay, aw, ah);
        if (!hitR(x, y, ax, ay, aw, ah)) continue;
        if (i == 0) filesOpen();
        else if (i == 1) { scr = S_AC; dirty = true; }
        else if (i == 2) { scr = S_GAMES; dirty = true; }
        else netOpen();
      }
      break;
    case S_GAMES:
      if (backHit(x, y)) { scr = S_APPS; dirty = true; break; }
      for (int i = 0; i < N_GAMES; i++) {
        int ax, ay, aw, ah; gamesTileRect(i, ax, ay, aw, ah);
        if (!hitR(x, y, ax, ay, aw, ah)) continue;
        if (i == 0) gameOpen(); else if (i == 1) sudokuOpen(); else if (i == 2) sandOpen(); else gardenOpen();
      }
      break;
    case S_FILES: filesTap(x, y); break;
    case S_AC: acTap(x, y); break;
    case S_NET: netTap(x, y); break;
    case S_WIFI: wifiTap(x, y); break;
    case S_BT: btTap(x, y); break;
    case S_GARDEN: gardenTap(x, y); break;
    case S_USB: usbTap(x, y); break;
    case S_SET:
      if (setPage && backHit(x, y)) { setOpenPage(0); break; }
      if (setPage && y < HDR_H + 34) break;
      settingsUI(false, x, y); break;
    default: break;
  }
}
void onLongPress(int x, int y) {
  if (y < HDR_H && x >= hdrChipX0 - 4 && x <= hdrChipX1 + 4 && !(scr == S_SUDOKU && land())) {   // hold the chip: Home <-> Uni
    place = place == 'H' ? 'U' : 'H'; prefs.putChar("place", place); ledFlash(0xFFFFFF, 120); dirty = true; tHandled = true; return;
  }
  if (scr == S_FILES) { filesLongPress(x, y); return; }
  if (scr == S_HOME && y > HDR_H && y < FTR_Y) {
    int i = homeTileAt(x, y);
    int tx, ty; if (i >= 0) homeTileRect(i, tx, ty);
    if (i >= 0 && y < ty + TILE_H - 30) { kpAct = i; kpVal = ""; scr = S_KEYPAD; dirty = true; tHandled = true; }   // not on the buttons
  }
}
void wake() {
  lastTouchMs = millis();
  if (pw != P_ON) {
    pw = P_ON; lcd.setBrightness(BRIGHT[brightIdx]); dirty = true;
    if (scr == S_SUDOKU && !sdkStartMs) sdkStartMs = millis();   // Sudoku clock runs again
  }
}
int* scrollVar() {
  switch (scr) {
    case S_HOME: return &scrollY;
    case S_STATS: return statView == 0 ? &statScroll : statView == 2 ? &heatScroll : nullptr;
    case S_SET: return &setScroll;
    case S_FILES: return fmUi == FU_PICK ? &pickScroll : (fmUi == FU_LIST ? &filesScroll : nullptr);
    case S_AC: return &acScroll;
    case S_NET: return &netScroll;
    case S_WIFI: return &wifiScroll;
    case S_BT: return &btScroll;
    default: return nullptr;
  }
}
void touchTask() {
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (down && !tDown) {
    tDown = true; tMoved = false; tHandled = false;
    tWakeOnly = (pw == P_OFF);
    wake();
    tX0 = tp.x; tY0 = tp.y; tT0 = millis();
    int* sv = scrollVar(); tScroll0 = sv ? *sv : 0;
  } else if (down && tDown) {
    lastTouchMs = millis();
    if (tWakeOnly) return;
    int dy = tp.y - tY0;
    int* sv = scrollVar();
    if (!tMoved && abs(dy) > 10 && tY0 > HDR_H && tY0 < FTR_Y && sv) tMoved = true;
    if (tMoved && sv) {
      *sv = tScroll0 - dy;
      dirty = true;
    } else if (!tHandled && millis() - tT0 > 650) {
      onLongPress(tX0, tY0);
      tHandled = true;
    }
  } else if (!down && tDown) {
    tDown = false;
    if (!tWakeOnly && !tMoved && !tHandled) onTap(tX0, tY0);
  }
}

// ---------------- Touch calibration ----------------
void loadTouchCal() {
  uint16_t p[8];
  if (prefs.getBytes("tcal", p, sizeof p) == sizeof p) lcd.setTouchCalibrate(p);
}
void calText(const char* l1, const char* l2, const char* l3, const char* l4) {
  lcd.fillScreen(TFT_WHITE);
  lcd.setTextDatum(D_MC);
  lcd.setTextColor(TFT_BLACK);
  int cx = lcd.width() / 2;
  lcd.setFont(FL); lcd.drawString(l1, cx, 110);
  lcd.setFont(FS);
  lcd.drawString(l2, cx, 145); lcd.drawString(l3, cx, 167); lcd.drawString(l4, cx, 189);
}
void runTouchCal() {
  lcd.setBrightness(BRIGHT[brightIdx]);
  lcd.setRotation(0);   // always calibrate standing up (the result works for every direction)
  while (true) {
    calText("Touch setup", "Tap the middle of each", "black square in the corners.", "4 corners, one at a time.");
    { lgfx::touch_point_t tr; while (lcd.getTouchRaw(&tr)) delay(10); }   // wait for finger up
    uint16_t p[8];
    lcd.calibrateTouch(p, TFT_BLACK, TFT_WHITE, 12);
    lcd.setTouchCalibrate(p);
    calText("Test touch", "A dot should appear", "right under your finger.", "OK? Tap \"Use this\".");
    lcd.fillRoundRect(8, 270, 108, 42, 10, TFT_LIGHTGREY);
    lcd.fillRoundRect(124, 270, 108, 42, 10, TFT_BLACK);
    lcd.setFont(FB);
    lcd.setTextColor(TFT_BLACK); lcd.drawString("Again", 62, 291);
    lcd.setTextColor(TFT_WHITE); lcd.drawString("Use this", 178, 291);
    int choice = 0;
    uint32_t t0 = millis();
    while (!choice) {
      lgfx::touch_point_t tp;
      if (lcd.getTouch(&tp)) {
        t0 = millis();
        if (tp.y >= 270 && tp.x < 120) choice = 1;
        else if (tp.y >= 270 && tp.x >= 124) choice = 2;
        else if (tp.y < 262) lcd.fillCircle(tp.x, tp.y, 3, TFT_BLACK);
      }
      if (millis() - t0 > 60000) choice = 2;   // no touch for 1 minute = keep it
      delay(10);
    }
    { lgfx::touch_point_t tr; while (lcd.getTouch(&tr)) delay(10); }
    if (choice == 2) { prefs.putBytes("tcal", p, sizeof p); break; }
  }
  lcd.setRotation(rot);
  tDown = false; tHandled = true;
  lastTouchMs = millis(); pw = P_ON;
  dirty = true;
}

// BOOT button: short press = log the first activity / wake screen, hold 3 s = touch setup
void buttonTask() {
  static bool prev = true, longDone = false; static uint32_t tChg = 0, bT0 = 0;
  bool v = digitalRead(PIN_BOOT);
  if (v != prev && millis() - tChg > 40) {
    tChg = millis(); prev = v;
    if (!v) { bT0 = millis(); longDone = false; }
    else if (!longDone) {
      bool wasOff = pw == P_OFF;
      wake();
      if (!wasOff && !acts.empty() && scr == S_HOME) logDefault(0);   // only on the Log screen (it used to log from any screen, unseen)
    }
  }
  if (!prev && !longDone && millis() - bT0 > 3000) { longDone = true; runTouchCal(); }
}

// ---------------- เว็บ ----------------
void sendJson(JsonDocument& d) { String s; serializeJson(d, s); server.send(200, "application/json; charset=utf-8", s); }
void apiState() {
  JsonDocument d;
  d["now"] = (uint32_t)nowT(); d["approx"] = timeApprox; d["place"] = String(place);
  d["auto"] = autoPlace; d["bat"] = batV; d["sta"] = staOk();
  d["ip"] = staOk() ? WiFi.localIP().toString() : String("");
  JsonArray arr = d["acts"].to<JsonArray>();
  for (auto& a : acts) {
    JsonObject o = arr.add<JsonObject>();
    Sum s = sumFor(a.id);
    o["id"] = a.id; o["name"] = a.name; o["unit"] = a.unit; o["color"] = colorHex(a.color);
    o["step"] = a.step; o["goal"] = a.goal; o["type"] = a.goalType; o["remind"] = a.remind;
    o["count"] = s.count; o["sum"] = s.sum;
    o["last"] = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
    o["state"] = goalState(a, measure(a, s)); o["overdue"] = overdue(a);
  }
  sendJson(d);
}
int actIndex(const String& id) { for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == id) return i; return -1; }
void apiLog() {
  int i = actIndex(server.arg("id"));
  if (i < 0) { server.send(404, "text/plain", "no act"); return; }
  if (server.hasArg("v")) { float v = server.arg("v").toFloat(); if (v <= 0) { server.send(400, "text/plain", "bad v"); return; } logValue(i, v); }
  else logDefault(i);
  refreshStatsIfVisible();
  apiState();
}
void apiUndo() {
  int i = actIndex(server.arg("id"));
  undoEvent(i);
  refreshStatsIfVisible();
  apiState();
}
void apiActsGet() { JsonDocument d; actsToJson(d.to<JsonArray>()); sendJson(d); }
void apiActsPost() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain")) || !d.is<JsonArray>() || !actsFromJson(d.as<JsonArrayConst>())) {
    server.send(400, "text/plain", "invalid"); return;
  }
  saveActs(); scrollY = 0; statSel = 0;
  refreshStatsIfVisible();
  dirty = true;
  apiActsGet();
}
void apiStats() {
  int days = constrain(server.arg("days").toInt(), 1, 90);
  computeStats(days, true);
  JsonDocument d;
  JsonArray k = d["days"].to<JsonArray>(); for (auto& s : stKeys) k.add(s);
  JsonArray arr = d["acts"].to<JsonArray>();
  for (size_t i = 0; i < acts.size(); i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = acts[i].id;
    JsonArray dl = o["daily"].to<JsonArray>(); for (float v : st[i].daily) dl.add(v);
    JsonArray dc = o["dcount"].to<JsonArray>(); for (int v : st[i].dcnt) dc.add(v);
    JsonArray hr = o["hours"].to<JsonArray>(); for (int h = 0; h < 24; h++) hr.add(st[i].hours[h]);
    o["total"] = st[i].total; o["totalCount"] = st[i].totalCnt;
    o["gapMin"] = st[i].gapN ? st[i].gapSum / st[i].gapN / 60.0 : 0;
  }
  refreshStatsIfVisible();   // คืนค่าสถิติของหน้าจอบอร์ด
  sendJson(d);
}
String csvRow(const Ev& e) {   // one line of the CSV export (web page and USB drive)
    int i = actIndex(e.id);
    time_t tt = e.t; struct tm tm; localtime_r(&tt, &tm);
    char dt[24]; strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S", &tm);
    String name = i >= 0 ? acts[i].name : String("(ลบแล้ว)");
    name.replace("\"", "\"\"");
    String row = String(dt) + "," + e.t + "," + e.id + ",\"" + name + "\"," + fmtNum(e.v) + "," +
                 (i >= 0 ? acts[i].unit : String("")) + "," + (e.place == 'U' ? "มอ" : "บ้าน") + "," + (e.ok ? 1 : 0) + "\n";
    return row;
}
void exportFile(const String& key, File& f) {
  (void)key;
  while (f.available()) {
    String l = f.readStringUntil('\n'); Ev e;
    if (parseLine(l, e)) server.sendContent(csvRow(e));
  }
}
void apiExport() {
  int days = server.arg("days").toInt();   // 0 = ทั้งหมด
  server.sendHeader("Content-Disposition", "attachment; filename=\"somudtick_" + curDay + ".csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent("\xEF\xBB\xBF" "datetime,epoch,activity_id,activity_name,value,unit,place,time_ok\n");
  std::vector<String> keys;
  if (days > 0) {
    time_t n = nowT();
    for (int d = days - 1; d >= 0; --d) keys.push_back(dayKey(n - (time_t)d * 86400));
  } else {
    File root = logFs().open("/log");
    File f = root.openNextFile();
    while (f) { String nm = f.name(); nm.replace(".csv", ""); if (!nm.endsWith(".new")) keys.push_back(nm); f.close(); f = root.openNextFile(); }
    root.close();
    std::sort(keys.begin(), keys.end());
  }
  for (auto& k : keys) {
    if (!logFs().exists(logPath(k))) continue;
    File f = logFs().open(logPath(k), "r");
    if (f) { exportFile(k, f); f.close(); }
  }
  server.sendContent("");
}
void apiTime() {
  uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
  if (t > 1700000000UL) {
    bool wasApprox = timeApprox;
    setClock(t, true);
    if (wasApprox || dayKey(nowT()) != curDay) loadDay();
    dirty = true;
  }
  apiState();
}
void apiPlace() {
  if (server.hasArg("p")) place = server.arg("p") == "U" ? 'U' : 'H';
  if (server.hasArg("auto")) autoPlace = server.arg("auto") == "1";
  prefs.putChar("place", place); prefs.putBool("auto", autoPlace);
  dirty = true;
  apiState();
}
void apiWifiGet() {
  JsonDocument d;
  d["appass"] = apPass;
  d["ssid"] = staSsid; d["home"] = homeSsid; d["uni"] = uniSsid; d["auto"] = autoPlace; d["sta"] = staOk();
  d["ip"] = staOk() ? WiFi.localIP().toString() : String("");
  sendJson(d);
}
void apiWifiPost() {
  staSsid = server.arg("ssid"); if (server.hasArg("pass") && server.arg("pass") != "********") staPass = server.arg("pass");
  homeSsid = server.arg("home"); uniSsid = server.arg("uni");
  if (server.hasArg("auto")) autoPlace = server.arg("auto") == "1";
  prefs.putString("ssid", staSsid); prefs.putString("pass", staPass);
  prefs.putString("home", homeSsid); prefs.putString("uni", uniSsid); prefs.putBool("auto", autoPlace);
  bool apChanged = false;
  if (server.hasArg("appass")) {
    String np = server.arg("appass"); np.trim();
    if (np.length() >= 8 && np.length() <= 63 && np != apPass) { apPass = np; prefs.putString("appass", apPass); apChanged = true; }
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  WiFi.disconnect();
  if (staSsid.length()) WiFi.begin(staSsid.c_str(), staPass.c_str());
  if (apChanged) { WiFi.softAPdisconnect(false); WiFi.softAP(AP_SSID, apPass.c_str()); }
  dirty = true;
}
// ---- photos & videos sent from the phone (already made small by the phone) ----
File upFile; String upErr;
String safeFileName(const String& n) {
  String o;
  for (char c : n) if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') o += c;
  if (o.length() < 3 || o[0] == '.') o = "file_" + String(millis()) + o;
  return o.substring(0, 60);
}
void apiUploadData() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upErr = "";
    if (!pinOk()) { upErr = "PIN"; return; }   // no PIN: nothing is written
    String dir = server.arg("dir");   // any folder on the card (not the Trash)
    if (!dir.startsWith("/") || dir.indexOf("..") >= 0 || dir.startsWith(TRASH_DIR)) dir = "/photos";
    if (!sdMount()) { upErr = "No SD card in the board"; return; }
    if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
    upFile = SD_MMC.open(joinPath(dir, fsFreeName(dir, safeFileName(u.filename))), FILE_WRITE);   // same name: "cat (2).jpg", never write over
    if (!upFile) upErr = "Can't write to the SD card";
    lastTouchMs = millis();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upFile && upFile.write(u.buf, u.currentSize) != u.currentSize) upErr = "SD card full?";
  } else if (u.status == UPLOAD_FILE_END) {
    if (upFile) upFile.close();
    if (scr == S_FILES) { filesLoad(); dirty = true; }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (upFile) { String pth = String(upFile.path()); upFile.close(); SD_MMC.remove(pth); }
    upErr = "Upload stopped";
  }
}
void apiUploadDone() {
  if (upErr.length()) server.send(500, "text/plain", upErr); else server.send(200, "text/plain", "ok");
}
// list one folder: /api/sd?dir=/photos   (dir=/.trash shows the Trash)
bool webPathOk(const String& p) { return p.startsWith("/") && p.indexOf("..") < 0 && p.indexOf("//") < 0; }
void apiSdList() {
  JsonDocument d;
  d["ok"] = sdMount();
  if (sdOk) {
    String dir = server.hasArg("dir") ? server.arg("dir") : String("/");
    if (!webPathOk(dir) || !fsIsDir(dir)) dir = dir == TRASH_DIR ? String(TRASH_DIR) : String("/");
    bool tr = dir == TRASH_DIR;
    d["total"] = (double)SD_MMC.totalBytes(); d["used"] = (double)SD_MMC.usedBytes();
    d["dir"] = dir;
    int tn; uint32_t tb; fsTrashInfo(tn, tb); d["trash"] = tn; d["trashBytes"] = tb;
    std::vector<std::pair<String, String>> idx; if (tr) idx = trashIndex();
    JsonArray a = d["items"].to<JsonArray>();
    File dd = SD_MMC.open(dir);
    if (dd) {
      File f = dd.openNextFile();
      while (f && a.size() < 300) {
        String n = baseOf(f.path());
        if (n.length() && n[0] != '.' && n != "System Volume Information") {
          String e = lowerExt(n);
          char t = f.isDirectory() ? 'd' : isImg(e) ? 'i' : isVid(e) ? 'v' : 0;
          if (t && !(tr && t == 'd')) {
            JsonObject o = a.add<JsonObject>();
            o["n"] = n; o["t"] = String(t); o["s"] = (uint32_t)f.size();
            if (tr) { o["show"] = trashShowName(n); String from = "/"; for (auto& x : idx) if (x.first == n) from = x.second; o["from"] = from; }
          }
        }
        f.close(); f = dd.openNextFile();
      }
      dd.close();
    }
  }
  sendJson(d);
}
void apiSdDirs() {   // every folder (for "Move to")
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  if (sdMount()) { std::vector<String> v; fsAllDirs("/", v, 0); for (auto& x : v) a.add(x); }
  sendJson(d);
}
// one action: op = trash | restore | purge | empty | mkdir | rename | move
void apiSdOp() {
  String op = server.arg("op"), path = server.arg("path"), name = server.arg("name"), dest = server.arg("dest");
  if ((path.length() && !webPathOk(path)) || (dest.length() && !webPathOk(dest))) { server.send(400, "application/json", "{\"err\":\"fail\"}"); return; }
  String e;
  if (op == "trash") e = fsTrash(path);
  else if (op == "restore") e = fsRestore(name);
  else if (op == "purge") e = fsPurge(name);
  else if (op == "empty") fsEmptyTrash();
  else if (op == "mkdir") e = fsMkdir(path, name);
  else if (op == "rename") e = fsRename(path, name);
  else if (op == "move") e = fsMove(path, dest);
  else e = "fail";
  if (scr == S_FILES) { fmUi = FU_LIST; filesLoad(); dirty = true; }
  if (e.length()) server.send(400, "application/json", "{\"err\":\"" + e + "\"}");
  else server.send(200, "application/json", "{\"ok\":true}");
}
void apiSdDel() {   // old web page: "delete" = move to Trash
  String pth = server.arg("path");
  if (!webPathOk(pth)) { server.send(400, "text/plain", "bad path"); return; }
  fsTrash(pth);
  if (scr == S_FILES) { filesLoad(); dirty = true; }
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- web PIN ----
// Every request except the page itself needs the PIN: the phone keeps it in a cookie "stpin".
// 5 wrong tries = locked for 1 minute (4 numbers are only 10000 choices).
void newWebPin() { char b[8]; snprintf(b, sizeof b, "%04u", (unsigned)(esp_random() % 10000)); webPin = b; prefs.putString("pin", webPin); dirty = true; }
int pinFails = 0; uint32_t pinLockUntil = 0;
bool pinOk() {   // no answer sent (the upload uses this)
  String c = server.header("Cookie");
  int i = c.indexOf("stpin=");
  String got = i >= 0 ? c.substring(i + 6, i + 10) : server.arg("pin");
  return webPin.length() == 4 && got == webPin;
}
bool webAuth() {
  if (pinLockUntil && millis() < pinLockUntil) { server.send(429, "text/plain", "locked"); return false; }
  if (pinOk()) { pinFails = 0; return true; }
  bool tried = server.header("Cookie").indexOf("stpin=") >= 0 || server.hasArg("pin");
  if (tried && ++pinFails >= 5) { pinFails = 0; pinLockUntil = millis() + 60000; }
  server.send(401, "text/plain", "pin");
  return false;
}
std::function<void()> guard(void (*fn)()) { return [fn] { if (webAuth()) fn(); }; }

void setupWeb() {
  const char* hk[] = {"Cookie"};
  server.collectHeaders(hk, 1);
  server.on("/upload", HTTP_POST, guard(apiUploadDone), apiUploadData);
  server.on("/api/sd", HTTP_GET, guard(apiSdList));
  server.on("/api/sd/del", HTTP_POST, guard(apiSdDel));
  server.on("/api/sd/dirs", HTTP_GET, guard(apiSdDirs));
  server.on("/api/sd/op", HTTP_POST, guard(apiSdOp));
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/pin", HTTP_GET, guard([] { server.send(200, "application/json", "{\"ok\":true}"); }));
  server.on("/api/state", HTTP_GET, guard(apiState));
  server.on("/api/log", HTTP_POST, guard(apiLog));
  server.on("/api/undo", HTTP_POST, guard(apiUndo));
  server.on("/api/acts", HTTP_GET, guard(apiActsGet));
  server.on("/api/acts", HTTP_POST, guard(apiActsPost));
  server.on("/api/stats", HTTP_GET, guard(apiStats));
  server.on("/api/time", HTTP_POST, guard(apiTime));
  server.on("/api/place", HTTP_POST, guard(apiPlace));
  server.on("/api/wifi", HTTP_GET, guard(apiWifiGet));
  server.on("/api/wifi", HTTP_POST, guard(apiWifiPost));
  server.on("/export.csv", HTTP_GET, guard(apiExport));
  server.onNotFound([] { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
}

// ---------------- setup / loop ----------------
// First start of v10: move the day files from the small area to the new 12 MB "logs" area.
// A file is removed from the old area only after its copy is written in full.
void logsMove() {
  logFsOk = LOGFS.begin(true, "/logfs", 10, "logs");
  if (!logFsOk) { Serial.println("no 'logs' area (old partition table?): logs stay in the small area"); if (!LittleFS.exists("/log")) LittleFS.mkdir("/log"); return; }
  if (!LOGFS.exists("/log")) LOGFS.mkdir("/log");
  std::vector<String> names;
  File root = LittleFS.open("/log");
  if (root) { for (File f = root.openNextFile(); f; f = root.openNextFile()) { names.push_back(f.name()); f.close(); } root.close(); }
  if (names.empty()) return;
  spr.fillScreen(C(PAPER));
  txt(FL, "SomudTick", W / 2, H / 2 - 20, INK, D_MC);
  txt(FS, "Moving your logs...", W / 2, H / 2 + 10, SOFT, D_MC);
  spr.pushSprite(0, 0);
  int moved = 0;
  for (auto& n : names) {
    String from = "/log/" + n, to = "/log/" + n;
    File a = LittleFS.open(from, "r"); if (!a) continue;
    String data; while (a.available()) data += a.readStringUntil('\n') + "\n";
    size_t size = a.size(); a.close();
    File b = LOGFS.open(to, "a");   // "a": if the same day already exists there, keep both
    bool ok = b && b.print(data) == data.length();
    if (b) b.close();
    if (ok && data.length() >= size) { LittleFS.remove(from); moved++; }
  }
  LOGFS.remove("/totals.csv");   // count the totals again from all files
  Serial.printf("moved %d day files to the logs area\n", moved);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  analogReadResolution(12);
  rgbLedWrite(PIN_RGB, 0, 0, 0);

  lcd.init();
  prefs.begin("somud", false);
  brightIdx = prefs.getUChar("bright", 3); if (brightIdx > 4) brightIdx = 3;
  themeDark = prefs.getUChar("dark", 0) ? 1 : 0;
  textColIdx = prefs.getUChar("tcol", 0); if (textColIdx >= N_TEXTCOLS) textColIdx = 0;
  rot = prefs.getUChar("rot", 0) & 3;
  lcd.setBrightness(BRIGHT[brightIdx]);
  fS.loadFont(&pwS);
  fB.loadFont(&pwB);
  applyTheme();
  applyRotation();
  spr.fillScreen(C(PAPER));
  txt(FL, "SomudTick", W / 2, H / 2 - 20, INK, D_MC);
  txt(FS, "Starting...", W / 2, H / 2 + 10, SOFT, D_MC);
  spr.pushSprite(0, 0);

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  logsMove();
  totalsLoad();
  logPctCache = logUsedPct();

  setenv("TZ", "ICT-7", 1); tzset();
  if (nowT() < 1700000000) {   // ไม่มีเวลาจริง → ใช้เวลาล่าสุดที่จำไว้
    uint32_t e = prefs.getUInt("epoch", 1767200000UL);
    setClock(e + 30, false);
    timeApprox = true;
  }
  sntp_set_time_sync_notification_cb(onTimeSync);
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com", "th.pool.ntp.org");

  staSsid = prefs.getString("ssid", ""); staPass = prefs.getString("pass", "");
  homeSsid = prefs.getString("home", ""); uniSsid = prefs.getString("uni", "");
  autoPlace = prefs.getBool("auto", false);
  { bool old = prefs.getBool("wifi", true); apOn = prefs.getBool("ap", old); staOn = prefs.getBool("sta", old); }   // v5 had one switch
  place = prefs.getChar("place", 'H');
  volIdx = constrain(prefs.getChar("vol", 2), -1, 4);
  offIdx = min(3, (int)prefs.getUChar("offT", 1));
  apPass = prefs.getString("appass", AP_PASS); if (apPass.length() < 8) apPass = AP_PASS;
  webPin = prefs.getString("pin", ""); if (webPin.length() != 4) newWebPin();
  acLoad();

  loadActs();
  loadDay();
  if (prefs.isKey("tcal")) loadTouchCal(); else runTouchCal();   // first start: set up touch
  readBattery();
  setupWifi();
  setupWeb();
  lastTouchMs = millis();
  dirty = true;
  Serial.println("SomudTick ready");
}

void loop() {
  server.handleClient();
  if (scr == S_GAME) {   // the game draws itself, ~30 times a second
    touchTask(); ledTask(); gameLoop();
    if (scr != S_GAME) render();
    delay(1);
    return;
  }
  if (scr == S_SAND) sandLoop();   // Sand & Water reads the finger itself (you drag to pour)
  else { touchTask(); buttonTask(); }
  ledTask();
  if (scr == S_SUDOKU) sudokuJoyTask();
  if (scr == S_FILES) filesTick();
  if (scr == S_USB) usbTick();
  if (scr != S_WIFI && scr != S_KBD) autoPlaceTask();   // the Wi-Fi page uses the scanner itself

  static uint32_t tick = 0, slow = 0;
  uint32_t ms = millis();
  static uint32_t gardenT = 0;
  if (scr == S_GARDEN && pw == P_ON && ms - gardenT > 80) { gardenT = ms; dirty = true; }   // the tree sways, clouds move
  if (timeFixPending) {   // the real time arrived: put logs made on the guessed time where they belong
    timeFixPending = false;
    fixGuessedLogs(timeFixDelta);
    prefs.putUInt("epoch", (uint32_t)nowT());
    loadDay(); gdSyncedFor = ""; refreshStatsIfVisible(); dirty = true;
  }
  if (ms - tick > 1000) {
    tick = ms;
    if (dayKey(nowT()) != curDay) { loadDay(); refreshStatsIfVisible(); dirty = true; }
    if (scr == S_HOME || scr == S_SET || scr == S_AC || scr == S_SUDOKU || scr == S_WIFI) dirty = true;   // update clock / "min ago"
    if (flashIdx >= 0 && ms > flashUntil) { flashIdx = -1; dirty = true; }
    // เตือน: ปลุกจอครั้งเดียวต่อรอบ
    for (auto& a : acts) {
      if (overdue(a) && !remindedFor.count(a.id)) {   // show a bar at the bottom, don't leave the screen you are on
        remindedFor[a.id] = 1;
        remindAct = &a - &acts[0];
        wake(); dirty = true;
        ledFlash(0xFFFFFF, 1500);
      }
    }
  }
  static uint32_t saveT = 0;
  if (ms - slow > 30000) { slow = ms; readBattery(); logPctCache = logUsedPct(); }
  if (ms - saveT > 600000UL) { saveT = ms; prefs.putUInt("epoch", (uint32_t)nowT()); }   // every 10 min (saves flash wear)

  // จัดการพลังงานจอ
  uint32_t idle = ms - lastTouchMs;
  uint32_t extra = (scr == S_NET || scr == S_SUDOKU) ? 60000UL : 0;   // reading news / thinking: stay on longer
  uint32_t offMs = OFF_MS[offIdx];
  if (offMs) offMs += extra;
  uint32_t dimMs = offMs - min((uint32_t)20000, offMs / 3);   // dim a bit before off
  if (offMs && pw == P_ON && idle > dimMs) { pw = P_DIM; lcd.setBrightness(max(10, BRIGHT[brightIdx] / 6)); }
  if (offMs && pw == P_DIM && idle > offMs) {
    pw = P_OFF; lcd.setBrightness(0);
    if (scr == S_SUDOKU) { sdkSave(); sdkStartMs = 0; }   // pause the Sudoku clock while the screen is off
  }

  if (dirty && pw != P_OFF) render();
  delay(5);
}
