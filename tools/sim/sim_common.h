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

