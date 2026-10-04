#pragma once
// SomudTick PC simulator: shared setup. Builds the real firmware code (SomudTick.ino) with stub hardware.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 1
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 1
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include "sim_hw.h"

uint64_t g_simMs = 5000;
int64_t g_simEpoch = 0;
int g_simBatMv = 1960;             // 3.92 V battery
std::mt19937 g_simRng(7);
HardwareSerial Serial;
LittleFSFS LittleFS;
SDMMCFS SD_MMC;
WiFiClass WiFi;
MDNSClass MDNS;
bool g_simOffline = false;
std::string g_simLogsRoot;
std::vector<SimBle> g_simBle;
std::vector<int16_t> g_simI2S;
int g_simIrMsgs = 0; int g_simDriveCap[64] = {0};   // IR messages sent, pin drive strength (tests)
int g_simAnalog[64], g_simDigital[64];
static bool g_simPinsInit = [] { for (int i = 0; i < 64; i++) { g_simAnalog[i] = 2048; g_simDigital[i] = HIGH; } return true; }();
bool g_simTouch = false; int g_simTouchX = 0, g_simTouchY = 0;   // a finger on the screen
bool g_simTaskFail = false;       // the next background task fails to start
bool g_simBleOn = false; int g_simBleDeinits = 0;   // BLE stack state
bool g_simRtcOn = false;           // a DS3231 clock module on the I2C wires
std::vector<SimIrMsg> g_simIrIn;   // v13: IR messages "received" by the KY-022 (tests push them)
std::vector<SimIrSent> g_simIrOut; // v13: everything sent by the IR LED through IRsend
bool g_simIrRxOn = false;
SimOled g_simOled; bool g_simOledOn = false;   // v13: small screen at 0x3C
bool g_simPadOn = false; uint8_t g_simPadAddr = 0x20, g_simPadDown = 0;   // v13: PCF8574 button board (bit = button held)
int g_simI2cFail = 0;
void (*g_simDelayHook)() = nullptr;
uint8_t g_simRtc[19] = {0};        // its registers

#include "SomudTick.ino"

#include <sys/stat.h>
static std::string OUT;
static std::string cur_prefix;

static void savePng(LovyanGFX& g, const std::string& name, int w, int h) {
  size_t len = 0;
  void* png = g.createPng(&len, 0, 0, w, h);
  if (!png) { fprintf(stderr, "png failed: %s\n", name.c_str()); return; }
  std::string p = OUT + "/" + cur_prefix + name + ".png";
  FILE* f = fopen(p.c_str(), "wb"); fwrite(png, 1, len, f); fclose(f); free(png);
  printf("saved %s\n", p.c_str());
}
static void shot(const std::string& name) { dirty = true; render(); savePng(spr, name, W, H); }
// v13: what the small screen really shows (its memory, as the chip got it), 4x bigger. chip 0 = SH1106 (columns 2..129)
static void oledPng(const std::string& name, int chip = 0) {
  LGFX_Sprite big; big.setColorDepth(16); big.createSprite(128 * 4 + 8, 64 * 4 + 8); big.fillScreen(0x2104);
  int off = chip == 0 ? 2 : 0;
  for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) {
    bool on = g_simOled.ram[y >> 3][x + off] & (1 << (y & 7));
    uint16_t c = !g_simOled.dispOn ? 0x0000 : on ? (g_simOled.contrast < 0x10 ? 0x630C : 0xDFFF) : 0x0841;
    big.fillRect(4 + x * 4, 4 + y * 4, 3, 3, c);
  }
  savePng(big, name, big.width(), big.height());
}
static void shotLcd(const std::string& name) { savePng(lcd, name, lcd.width(), lcd.height()); }
static void tick(int n = 1) { for (int i = 0; i < n; i++) { g_simMs += 200; loop(); } }
static void tap(int x, int y) { onTap(x, y); }

static void writeFile(const std::string& p, const std::string& s) { FILE* f = fopen(p.c_str(), "wb"); fwrite(s.data(), 1, s.size(), f); fclose(f); }
static void seedLogs(time_t now) {
  // 14 days of history; typical day of a student
  struct Plan { const char* id; int n; int h0, h1; float v; };
  const Plan P[] = {{"water", 7, 8, 22, 1}, {"toilet", 6, 7, 23, 1}, {"meal", 3, 8, 20, 1}, {"snack", 2, 13, 22, 1},
                    {"smoke", 5, 9, 23, 1}, {"rest", 2, 12, 16, 1}, {"fuel", 0, 17, 18, 100}};
  for (int d = 14; d >= 0; --d) {
    time_t day = now - (time_t)d * 86400;
    String key = dayKey(day);
    struct tm tm; localtime_r(&day, &tm);
    std::string out;
    std::vector<std::pair<uint32_t, std::string>> ev;
    for (auto& p : P) {
      int n = p.n + (int)(g_simRng() % 3) - 1; if (!strcmp(p.id, "fuel")) n = (d % 4 == 0);
      for (int k = 0; k < n; k++) {
        struct tm t = tm; t.tm_hour = p.h0 + (int)(g_simRng() % (p.h1 - p.h0)); t.tm_min = g_simRng() % 60; t.tm_sec = 0;
        uint32_t ts = mktime(&t);
        if (d == 0 && ts > (uint32_t)now) continue;
        char b[96]; snprintf(b, sizeof b, "%u,%s,%s,%c,1\n", ts, p.id, fmtNum(!strcmp(p.id, "fuel") ? 350 : p.v).c_str(), d % 3 ? 'H' : 'U');
        ev.push_back({ts, b});
      }
    }
    std::sort(ev.begin(), ev.end());
    for (auto& e : ev) out += e.second;
    writeFile(LittleFS.host("/log/" + key + ".csv"), out);
  }
}
static void seedSd() {
  const char* dirs[] = {"/photos", "/videos", "/photos/Trip 2026", "/.trash"};
  for (auto d : dirs) SD_MMC.mkdir(String(d));
  std::string jpg(150000, 'x');
  const char* ph[] = {"/photos/beach_trip.jpg", "/photos/cat.jpg", "/photos/Grandma_birthday_party_at_the_river_restaurant.jpg",
                      "/photos/IMG_20260921_183055.jpg", "/photos/photo_mf3k2x.jpg", "/photos/Trip 2026/khonkaen_01.jpg",
                      "/videos/graduation.mjpeg", "/videos/graduation.pcm"};
  for (auto p : ph) writeFile(SD_MMC.host(p), jpg);
  writeFile(SD_MMC.host("/.trash/3_old_selfie.jpg"), jpg);
  writeFile(SD_MMC.host("/.trash/.index"), "3_old_selfie.jpg\t/photos\n");
}

