#include "sim_common.h"
// v12 review pictures: every main page with and without the extra parts (small screen + button board = no top / bottom
// bars), Light / HUD, tall / wide, plus what the small screen shows.
//   ./shots.sh shots_v12review.cpp ../../SomudTick_v12/SomudTick run/shots12r

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void oledShot(const std::string& name) {   // the small screen, 4x bigger, white on black
  oledOk = true; oledT = 0; oledTask();
  LGFX_Sprite big; big.setColorDepth(16); big.createSprite(ospr.width() * 4, ospr.height() * 4);
  for (int y = 0; y < ospr.height(); y++) for (int x = 0; x < ospr.width(); x++)
    big.fillRect(x * 4, y * 4, 4, 4, ((ospr.readPixel(x, y) >> 2) & 7) >= 4 ? 0xFFFF : 0x0000);
  savePng(big, name, big.width(), big.height());
}

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s12r"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3; g_simOffline = true; animOn = false;
  ospr.setColorDepth(8); ospr.createSprite(128, 32);
  for (int extra : {0, 1}) for (int th : {0, 2}) for (int r : {0, 1}) {
    if (extra == 0 && th == 2) continue;
    cur_prefix = std::string(extra ? "bare_" : "bars_") + (th == 2 ? "hud_" : "light_") + (r ? "wide_" : "tall_");
    oledOk = padOk = extra; themeSet(th); rot = r; applyRotation(); layoutUpdate();
    scr = S_HOME; scrollY = 0; shot("01_log");
    scrollY = 9999; shot("02_log_end"); scrollY = 0;
    remindAct = 0; scr = S_APPS; shot("03_apps_remind"); remindAct = -1;
    goScreen(S_STATS); statView = 0; shot("04_stats_all");
    statView = 1; statSel = 0; shot("05_stats_days");
    statView = 2; shot("06_stats_hours"); statView = 0;
    scr = S_GAMES; shot("07_games");
    goScreen(S_SET); shot("08_settings");
    setOpenPage(1); shot("09_set_screen");
    setOpenPage(2); shot("10_set_wifi");
    setAboutPage(); shot("11_set_about");
    setScroll = 9999; shot("12_set_about_end"); setOpenPage(0);
    scr = S_AC; acScroll = 0; shot("13_ac");
    filesOpen(); shot("14_files");
    scr = S_HOME; kpAct = 0; kpVal = "3"; scr = S_KEYPAD; shot("15_keypad");
    scr = S_HOME; logFull = true; shot("16_log_full"); logFull = false;
    burstNoticeMs = millis(); burstName = "Button E loose?"; scr = S_APPS; shot("17_button_loose"); burstNoticeMs = 0;
    gbListOpen(); shot("18_gb_list");
    netOpen(); shot("19_net");
    if (r == 0 && extra) {
      for (int p : {0, 1}) { oledPage = p; oledShot(std::string("oled_page") + char('0' + p)); }
      oledPage = 0; remindAct = 0; g_simEpoch -= (g_simEpoch % 6) - 0; oledShot("oled_remind"); remindAct = -1;
      popAct = 0; popMs = millis(); popText = "+1"; oledShot("oled_pop"); popAct = -1;
      burstNoticeMs = millis(); burstName = "Button E loose?"; oledShot("oled_loose"); burstNoticeMs = 0;
      logFull = true; oledShot("oled_full"); logFull = false;
    }
    scr = S_HOME;
  }
  return 0;
}
