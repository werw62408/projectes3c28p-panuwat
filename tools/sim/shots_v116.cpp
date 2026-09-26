#include "sim_common.h"
// v11.6 pictures: the Dragon game, weather in Sathorn, Sudoku with the New game window, the Games menu. Tall and wide, light and dark.
//   ./shots.sh shots_v116.cpp ../../SomudTick_v11.6/SomudTick run/shots116

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void stick(float x, float y) { g_simAnalog[JOY_X] = 2048 + (int)(x * 1900); g_simAnalog[JOY_Y] = 2048 + (int)(y * 1900); }

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s116"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3; g_simOffline = true;
  for (int dark = 0; dark < 2; dark++)
    for (int r : {0, 1}) {
      cur_prefix = std::string(dark ? "dark_" : "") + (r ? "wide_" : "tall_");
      themeDark = dark; applyTheme(); rot = r; applyRotation();
      // Dragon: start window, flying in a curve (small), bigger, dashing, paused
      gameOpen(); run(200); gameDraw(); savePng(spr, "01_dragon_ready", W, H);
      g_simDigital[JOY_SW] = LOW; run(80); g_simDigital[JOY_SW] = HIGH; run(100);
      for (int k = 0; k < 60; k++) { float a = k * 0.12f; stick(cosf(a), sinf(a) * 0.8f); run(34); }
      stick(0, 0); run(60); gameDraw(); savePng(spr, "02_dragon_fly", W, H);
      pr = 20; for (int k = 0; k < 50; k++) { float a = 2 + k * 0.1f; stick(cosf(a), sinf(a)); run(34); }
      stick(0.3f, -0.2f); gameDraw(); savePng(spr, "03_dragon_big", W, H);
      g_simDigital[JOY_SW] = LOW; run(60); g_simDigital[JOY_SW] = HIGH; run(60); gameDraw(); savePng(spr, "04_dragon_dash", W, H);
      pr = 33; for (int k = 0; k < 40; k++) { float a = 4 + k * 0.1f; stick(cosf(a), sinf(a)); run(34); }
      stick(0, 0); gameDraw(); savePng(spr, "05_dragon_biggest", W, H);
      gameTap(W - 80, 10); gameDraw(); savePng(spr, "06_dragon_pause", W, H);
      scr = S_GAMES; shot("07_games");
      // weather in Sathorn (the numbers are made up: the simulator is offline here)
      wx.ok = true; wx.at = millis(); wx.temp = 31.4f; wx.feels = 36; wx.hum = 70; wx.wind = 9; wx.rain = 0; wx.code = 2;
      for (int i = 0; i < 3; i++) { wx.dCode[i] = i ? 61 : 2; wx.dMax[i] = 33 + i; wx.dMin[i] = 26; wx.dRain[i] = 40 + 20 * i; }
      wx.aqOk = true; wx.pm25 = 28; wx.pm10 = 41; netTab = 0; scr = S_NET; netMsg = ""; netScroll = 0; newsOpen = -1; shot("08_weather");
      // Sudoku with the New game window open (the clock stops)
      sudokuOpen(); if (sdkNewMenu) sdkNew(0); sdkNewMenu = true; shot("09_sudoku_newgame"); sdkNewMenu = false; sudokuClose();
    }
  themeDark = 0; applyTheme(); rot = 0; applyRotation();
  return 0;
}
