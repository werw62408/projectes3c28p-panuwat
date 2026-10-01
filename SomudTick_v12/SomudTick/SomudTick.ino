/*
  SomudTick — daily activity tracker gadget for the ES3C28P board
  (ESP32-S3 + 2.8" IPS 240x320 screen + FT6336G touch)

  Screen text is in simple English. (The web page on the phone stays in Thai.)
  Tabs:  Log (Stats opens from Log) | Apps | Settings
  Apps:  Files (SD photos/videos) | AC Remote (Panasonic IR) | Games | Internet
  Games: Dragon | Sudoku | Sand & Water | Ant Colony | Tilt Maze | Blocks | Game Boy

  Extra parts (optional):
    Joystick  VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (not 5V!), GND -> GND
    KY-005    S -> IO21, middle -> 3.3V, "-" -> GND
    DS3231    SDA -> IO16, SCL -> IO15, VCC -> 3.3V (not 5V!), GND -> GND   (clock module, v11.2)
    (3.3V and GND can be taken from the I2C connector)
    v12, on the same I2C wires (SDA IO16, SCL IO15, 3.3V, GND):
    OLED 0.91" SSD1306 128x32, address 0x3C: the clock moves up there (ext_io.h)
    PCF8574 (0x20-0x27, not the "A" chip at 0x38 = the touch chip) + NA011 button board (switch on 3.3V!):
      P0 A, P1 B, P2 C, P3 D, P4 E, P5 F, P6 K (stick press); stick X/Y still IO2/IO3; IO14 then stays free

  Arduino IDE: board "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM",
  Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)" (partitions.csv in this folder is used),
  USB Mode "USB-OTG (TinyUSB)", USB CDC On Boot "Disabled"
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
#include <set>
#include <functional>
#include <algorithm>

#include "types.h"
#include "lgfx_es3c28p.h"
#include "fonts_th.h"
#include "fonts_hud.h"   // v11.8 HUD theme: Chakra Petch + Orbitron (English and numbers only)
#include "webpage.h"

// ---------------- ค่าที่ปรับได้ ----------------
#define FW_VERSION     "v12"      // shown in Settings and About
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
static lgfx::PointerWrapper pwHS(FONT_HUD_S, sizeof(FONT_HUD_S)), pwHB(FONT_HUD_B, sizeof(FONT_HUD_B)), pwHL(FONT_HUD_L, sizeof(FONT_HUD_L)), pwHXL(FONT_HUD_XL, sizeof(FONT_HUD_XL)), pwHM(FONT_HUD_M, sizeof(FONT_HUD_M));
static lgfx::VLWfont hS, hB, hL, hXL, hM;   // v11.8 HUD fonts

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
// v11.8 HUD theme (Settings > Screen > Theme > HUD): black, one colour, cut corners, sci-fi letters.
// It is a third theme; Light and Dark stay as they were. The HUD colour is chosen where the text colour is.
uint8_t themeHud = 0, hudCol = 0;
struct HudCol { const char* name; uint32_t main, bright, dim, warn; };
const HudCol HUDCOLS[] = {
  {"Arctic", 0xE8F0F6, 0xFFFFFF, 0x6F7C87, 0xFF6A2B}, {"Amber", 0xFFB020, 0xFFD678, 0x96641C, 0xFF6A2B},
  {"Cyan", 0x2FE3FF, 0xB8F6FF, 0x1E7A8A, 0xFF4F7A}, {"Green", 0x5CFF8A, 0xC8FFD6, 0x2A8A48, 0xFFB020}};
const int N_HUDCOLS = 4;
#define THEME_HUD 2   // (tests / pictures: this version has the HUD theme)
bool darkUi() { return themeDark || themeHud; }   // screens that only know light / dark (weather icons, sand) use the dark look on HUD

int HDR_H = 30;   // v12: 0 when the small screen above shows the clock (layoutUpdate in ui_draw.h)
int W = 240, H = 320, FTR_Y = 290, CONT_H = 260;   // เปลี่ยนตามการหมุนจอ (FTR_Y = H when the buttons replace the tabs)
bool land() { return W > H; }
// v12 extra parts on the I2C wires (optional, found at start): SSD1306 0.91" 128x32 small screen, PCF8574 + NA011 buttons
bool oledOk = false, padOk = false;

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
int logNotice();
String oldIdFor(const String& name, const std::vector<Act>& used);
void drawAskUpdate();
void enterUpdateMode();
void newWebPin();
bool pinOk();
bool pinLocked();
void wifiPageOpen();
void btPageOpen();
void joySetup();
void powerTask();
void remindBeep();
enum Sfx : uint8_t { SFX_TAP, SFX_PLUS, SFX_BACK, SFX_ERR, SFX_EAT, SFX_LEVEL, SFX_REMIND, SFX_GOAL, SFX_KEY, SFX_N };
bool sfxOn = true, animOn = true;   // v11.9: Settings > Sounds / Motion
uint32_t sfxCount[SFX_N] = {0};    // how many of each sound were asked for (the simulator tests read it)
void sfx(uint8_t id);   // v11.9: a short UI sound in the background (app_media.h)
void kbdOpen(const String& title, const String& start, void (*done)(const String&));
// joystick control (nav.h): every button drawn is written down so the stick can move between them
enum NavKind : uint8_t { NK_BTN, NK_BACK, NK_FOOTER, NK_OVER };   // NK_OVER: drawn over other buttons (the reminder bar), found by its middle
#define NAV_RING_C 0xFF8C00   // the joystick ring colour (orange)
void navAdd(int x, int y, int w, int h, uint8_t kind = 0, int hx = -1, int hy = -1);
void navModal(int backX, int backY);
void navModalEnd();
void navBegin(); void navDraw(); extern bool navShow;
void navArea(int top, int h, int pos, int maxPos);   // a scrolling area was drawn (called by scrollBar)
enum JoyEv : uint8_t; JoyEv navWaitEvent();

// ---------------- the parts (order matters: each file uses the ones above it) ----------------
#include "core_time.h"
#include "core_log.h"
#include "core_power.h"
#include "core_wifi.h"
#include "joystick.h"
#include "ui_draw.h"
#include "screen_log.h"
#include "screen_stats.h"
#include "screen_settings.h"
#include "screen_apps.h"
// apps
#include "app_media.h"
#include "app_ac.h"
#include "app_game.h"
#include "app_sudoku.h"
#include "app_sand.h"
#include "app_ants.h"     // v11.8: Ant Colony (was Habit Garden)
#include "app_net.h"
#include "app_connect.h"
#include "app_usb.h"
#include "app_maze.h"
#include "app_blocks.h"
#include "app_gb.h"
#include "app_deck.h"
#include "ui_main.h"
#include "input.h"
#include "web_api.h"
#include "nav.h"
#include "ext_io.h"      // v12: small OLED screen (clock) + button board
#include "backup.h"

// ---------------- setup / loop ----------------
// Screen power: dim a little before the screen turns off, then off (screen chip asleep, slower processor).
// The idle time is read from the clock now: lastTouchMs can be a few ms newer than the time loop() read
// at its start (a reminder calls wake() later in the same pass), and "old - newer" used to wrap to 49 days
// = the screen went dark at once, right when the reminder turned it on (fixed in v11.5).
uint32_t screenOffAt = 0;   // millis() when the screen last turned off (the hotspot rests 10 min after that)
uint32_t idleMs() { uint32_t now = millis(); return (int32_t)(now - lastTouchMs) > 0 ? now - lastTouchMs : 0; }
void powerTask() {
  uint32_t idle = idleMs();
  uint32_t extra = (scr == S_NET || scr == S_SUDOKU) ? 60000UL : 0;   // reading news / thinking: stay on longer
  uint32_t offMs = OFF_MS[offIdx];
  if (offMs) offMs += extra;
  uint32_t dimMs = offMs - min((uint32_t)20000, offMs / 3);   // dim a bit before off
  if (offMs && pw == P_ON && idle > dimMs) { pw = P_DIM; lcd.setBrightness(max(10, BRIGHT[brightIdx] / 6)); }
  if (offMs && pw == P_DIM && idle > offMs) {
    pw = P_OFF; lcd.setBrightness(0); screenOffAt = millis();
    // pocket: the joystick ring is hidden and goes back to the first button of the page,
    // so two presses in a pocket (wake + tap) can't hit the button the ring was left on (e.g. +1)
    navShow = false; navPage = -1;
    if (scr == S_SUDOKU) { sdkSave(); sdkStartMs = 0; }   // pause the Sudoku clock while the screen is off
    if (scr != S_USB) powerLow(true);   // save battery: slower processor, screen chip asleep (not while a PC uses the card)
  }
}
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

// v11.8 HUD theme: the start screen checks the parts one by one ("SYS BOOT"). It only shows what setup() already does,
// so the board does not start slower. Light / Dark keep the plain "Starting..." screen.
void bootHud(int step) {   // step 0..6: how many lines are done
  if (!themeHud) return;
  spr.fillScreen(C(PAPER));
  hudShape(4, 4, W - 8, H - 8, 10, -1, LINE);
  bool wide = land();
  int cx = wide ? 70 : W / 2, top = wide ? 18 : 16;
  txt(&hM, "SOMUDTICK", wide ? 18 : W / 2, top, HUDB, wide ? D_TL : D_TC);
  String sub = String("HUD OS // FW ") + FW_VERSION; sub.toUpperCase();
  txt(&hS, sub, wide ? 18 : W / 2, top + 22, SOFT, wide ? D_TL : D_TC);
  int pct = step * 100 / 6, gy = wide ? 132 : 110;
  hudGauge(cx, gy, 36, pct / 100.0f, "", INK);
  txt(&hM, String(pct), cx, gy - 2, HUDB, D_MC);
  txt(&fonts::Font0, "LOADING", cx, gy + 14, SOFT, D_MC);
  const char* K[6] = {"RTC CLOCK", "LOG STORAGE", "SETTINGS", "JOYSTICK", "ACTIVITIES", "WI-FI HOTSPOT"};
  String V[6] = {rtcFound ? "OK" : "NONE", String(logPctCache) + "%", "OK", joyOk ? "OK" : "NONE", String(acts.size()), apOn ? "ON" : "OFF"};
  int lx = wide ? 160 : 14, lw = wide ? W - 174 : W - 28, ly = wide ? 62 : 170;
  for (int i = 0; i < 6; i++) {
    uint32_t c = i < step ? INK : blend(PAPER, INK, 0.25f);
    txt(&hS, String("> ") + K[i], lx, ly + i * 17, c);
    if (i < step) txt(&hS, V[i], lx + lw, ly + i * 17, V[i] == "NONE" || V[i] == "OFF" ? SOFT : HUDB, D_TR);
  }
  int by = H - 30;
  hudSegs(12, by, W - 24, 7, 24, step * 4, INK, blend(PAPER, INK, 0.15f));
  txt(&fonts::Font0, "SYS.BOOT", 12, by + 12, SOFT);
  txt(&fonts::Font0, "ES3C28P", W - 12, by + 12, SOFT, D_TR);
  spr.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);
  crashBoot();   // v11.9: remember where a crash happened (shown in About)
  pinMode(PIN_BOOT, INPUT_PULLUP);
  analogReadResolution(12);
  rgbLedWrite(PIN_RGB, 0, 0, 0);

  lcd.init();
  prefs.begin("somud", false);
  brightIdx = prefs.getUChar("bright", 3); if (brightIdx > 4) brightIdx = 3;
  themeDark = prefs.getUChar("dark", 0) ? 1 : 0;
  themeHud = prefs.getUChar("hud", 0) ? 1 : 0;
  hudCol = prefs.getUChar("hudCol", 0); if (hudCol >= N_HUDCOLS) hudCol = 0;
  textColIdx = prefs.getUChar("tcol", 0); if (textColIdx >= N_TEXTCOLS) textColIdx = 0;
  rot = prefs.getUChar("rot", 0) & 3;
  lcd.setBrightness(BRIGHT[brightIdx]);
  fS.loadFont(&pwS);
  fB.loadFont(&pwB);
  hS.loadFont(&pwHS); hB.loadFont(&pwHB); hL.loadFont(&pwHL); hXL.loadFont(&pwHXL); hM.loadFont(&pwHM);
  applyTheme();
  applyRotation();
  spr.fillScreen(C(PAPER));
  txt(FL, "SomudTick", W / 2, H / 2 - 20, INK, D_MC);
  txt(FS, "Starting...", W / 2, H / 2 + 10, SOFT, D_MC);
  spr.pushSprite(0, 0);
  bootHud(0);

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  logsMove();
  dayFilesRepair();   // a power cut while a day file was being saved: finish it
  totalsLoad();
  namesLoad();
  logPctCache = logUsedPct();

  setenv("TZ", "ICT-7", 1); tzset();
  timeBegin();   // clock module -> time kept over a restart -> last saved time (a guess)
  sntp_set_time_sync_notification_cb(onTimeSync);
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com", "th.pool.ntp.org");
  extBegin();   // v12: small screen + button board on the I2C wires (the touch chip set the wires up in lcd.init())
  bootHud(2);   // clock, log space

  staSsid = prefs.getString("ssid", ""); staPass = prefs.getString("pass", "");
  homeSsid = prefs.getString("home", ""); uniSsid = prefs.getString("uni", "");
  autoPlace = prefs.getBool("auto", false);
  { bool old = prefs.getBool("wifi", true); apOn = prefs.getBool("ap", old); staOn = prefs.getBool("sta", old); }   // v5 had one switch
  place = prefs.getChar("place", 'H');
  volIdx = constrain(prefs.getChar("vol", 2), -1, 4);
  sfxOn = prefs.getBool("sfx", true); animOn = prefs.getBool("anim", true);   // v11.9
  offIdx = min(3, (int)prefs.getUChar("offT", 1));
  apPass = prefs.getString("appass", AP_PASS); if (apPass.length() < 8) apPass = AP_PASS;
  acLoad();
  deckLoad();
  backupLoad();
  bootHud(3);   // settings
  joyNavOn = prefs.getBool("joyNav", true);
  joyDetect();   // a stick plugged in? (without one nothing is read: a free pin only gives noise)
  joyNavReset();
  loadActs();
  loadDay();
  bootHud(5);   // joystick, activities
  if (prefs.isKey("tcal")) loadTouchCal(); else runTouchCal();   // first start: set up touch
  readBattery();
  setupWifi();
  webPin = prefs.getString("pin", ""); if (webPin.length() != 4) newWebPin();   // after Wi-Fi: real random numbers
  setupWeb();
  bootHud(6);
  lastTouchMs = millis();
  dirty = true;
  Serial.println("SomudTick ready");
}

void loop() {
  crashNote();
  layoutUpdate();   // v12: top / bottom bars for this screen (hidden when the small screen and the buttons do their job)
  server.handleClient();
  // Pixel Swim, Tilt Maze and Blocks draw themselves (~30 times a second), without render().
  // v11.5: the rest of loop() runs for them too (reminders, midnight, the real time, the screen timer):
  // a game left alone now pauses and stays open with the screen off, so it must not stop the reminders.
  bool selfDraw = scr == S_GAME || scr == S_MAZE || scr == S_BLOCKS || scr == S_GB;
  if (scr == S_GAME) { touchTask(); gameLoop(); }
  else if (selfDraw) { if (scr == S_MAZE) mazeLoop(); else if (scr == S_BLOCKS) blocksLoop(); else gbLoop(); }
  else if (scr == S_SAND) sandLoop();   // Sand & Water reads the finger itself (you drag to pour)
  else { touchTask(); buttonTask(); navTask(); padTask(); }
  if (selfDraw && scr != S_GAME && scr != S_MAZE && scr != S_BLOCKS && scr != S_GB) { selfDraw = false; dirty = true; }   // just left a game: draw the menu
  ledTask();
  oledTask();   // v12: the clock on the small screen (sent only when it changed)
  if (scr == S_SUDOKU) sudokuJoyTask();
  if (scr == S_ANTS) antsTask();
  if (scr == S_FILES) filesTick();
  if (scr == S_USB) usbTick();
  if (scr == S_DECK) deckTick();   // v12: Bluetooth retry, "ready" state
  netPoll();   // a background download finished?
  btCollect(); // a Bluetooth scan finished (also when its page is closed): list it, give the memory back
  if (deckBleOn && scr != S_DECK) deckBleStop();   // left Deck (also by the bottom tabs): Bluetooth off again
  rtcTask();   // new real time came in -> write it to the clock module
  { static uint32_t anim = 0;   // "Loading..." / "Scanning..." dots
    if (((scr == S_NET && netBusy) || (scr == S_BT && btBusy)) && pw == P_ON && millis() - anim > 350) { anim = millis(); dirty = true; } }
  if (scr != S_WIFI && scr != S_KBD) autoPlaceTask();   // the Wi-Fi page uses the scanner itself

  static uint32_t tick = 0, slow = 0, lastMin = 99, homeT = 0;
  uint32_t ms = millis();
  if (timeFixPending) {   // the real time arrived: put logs made on the guessed time where they belong
    timeFixPending = false;
    fixGuessedLogs(timeFixDelta);
    prefs.putUInt("epoch", (uint32_t)nowT());
    loadDay(); anSyncedFor = ""; refreshStatsIfVisible(); dirty = true;
  }
  if (ms - tick > 1000) {
    tick = ms;
    if (dayKey(nowT()) != curDay) { loadDay(); refreshStatsIfVisible(); dirty = true; }
    // the clock shows minutes: the Log page is drawn again when the minute changes (and every 15 s for "5m ago"),
    // not every second (drawing the whole screen takes ~30 ms of the processor each time)
    uint32_t minute = (uint32_t)nowT() / 60;
    bool newMin = minute != lastMin; lastMin = minute;
    if (scr == S_HOME && (newMin || ms - homeT > 15000)) { homeT = ms; dirty = true; }
    if (scr == S_SUDOKU || scr == S_WIFI || (scr == S_AC && ms - acSentMs < 2500)) dirty = true;   // a running clock / status / "Sent!"
    if ((scr == S_SET && (newMin || ms - homeT > 5000)) || (scr == S_AC && newMin)) { homeT = ms; dirty = true; }   // Settings: Wi-Fi / battery state
    if (flashIdx >= 0 && (int32_t)(ms - flashUntil) > 0) { flashIdx = -1; dirty = true; }
    overdueCheck();   // for the LED blink
    // เตือน: ปลุกจอครั้งเดียวต่อรอบ
    for (auto& a : acts) {
      if (overdue(a) && !remindedFor.count(a.id)) {   // show a bar at the bottom, don't leave the screen you are on
        remindedFor[a.id] = 1;
        remindAct = &a - &acts[0];
        wake(); dirty = true;
        ledFlash(0xFFFFFF, 1500);
        remindBeep();   // v11.5: the LED faces into the case, so also a short sound
      }
    }
  }
  static uint32_t saveT = 0;
  if (ms - slow > 30000) {
    slow = ms; readBattery(); logPctCache = logUsedPct();
    if (!joyOk && joyNavOn && pw == P_ON && !tDown) { joyDetect(); joyNavReset(); }   // a stick plugged in later
  }
  if (ms - saveT > 600000UL) { saveT = ms; prefs.putUInt("epoch", (uint32_t)nowT()); }   // every 10 min (saves flash wear)

  // จัดการพลังงานจอ
  powerTask();
  if (ms - slow < 50) {   // right after the battery check (every 30 s): hotspot rest, weekly backup
    uint32_t idle = idleMs();
    if (pw == P_OFF) hotspotSleepTask(millis() - screenOffAt);   // v11.7: from when it really went dark (News / Sudoku get 1 min more)
    backupTask(idle);
  }

  if (scr == S_HOME && pw == P_ON && ((popAct >= 0 && ms - popMs < POP_MS + 60) || (goalAct >= 0 && ms - goalMs < GOAL_MS + 60))) {
    static uint32_t animT = 0; if (ms - animT > 40) { animT = ms; dirty = true; } }   // v11.9: +1 / goal animation frames
  if (dirty && pw != P_OFF && !selfDraw) render();
  delay(pw == P_OFF ? 40 : selfDraw ? 1 : 5);   // screen off: check touch 25 times a second instead of 200 (games: smooth pictures)
}
