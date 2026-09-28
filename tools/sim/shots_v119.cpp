#include "sim_common.h"
// v11.9 pictures: Pixel Swim (finer water), Game Boy with the built-in game, the full-screen Ant farm (new, grown, wide),
// Settings (Sounds / Motion), the +1 and goal animations, Deck Bluetooth text. HUD and Light, tall and wide.
//   ./shots.sh shots_v119.cpp ../../SomudTick_v11.9/SomudTick run/shots119

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void stick(float x, float y) { g_simAnalog[JOY_X] = 2048 + (int)(x * 1900); g_simAnalog[JOY_Y] = 2048 + (int)(y * 1900); }

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s119"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3; g_simOffline = true; animOn = false;   // (pictures: no page wipe)
  for (int th : {2, 0}) for (int r : {0, 1}) {
    cur_prefix = std::string(th == 2 ? "hud_" : "light_") + (r ? "wide_" : "tall_");
    themeSet(th); rot = r; applyRotation();
    // Pixel Swim: the start window, then swimming in a circle
    gameOpen(); run(100); gameDraw(); savePng(spr, "01_swim_ready", W, H);
    gs = G_PLAY; for (int k = 0; k < 50; k++) { float a = k * 0.13f; stick(cosf(a), sinf(a) * 0.8f); run(34); }
    food[0].x = px_ + pr + 1; food[0].y = py_; run(80); stick(0.6f, 0.2f); run(120); stick(0, 0); gameDraw(); savePng(spr, "02_swim_play", W, H);
    gs = G_READY; scr = S_GAMES; shot("03_games");
    gbListOpen(); shot("04_gb_list");
    // Log: +1 floating up, then a goal reached with sparks
    scr = S_HOME; scrollY = 0; popAct = 1; popMs = millis() - 250; popText = "+1"; goalAct = -1; shot("05_log_plus");
    goalAct = 0; goalMs = millis() - 400; popAct = -1; shot("06_log_goal"); goalAct = -1;
    goScreen(S_SET); shot("07_settings");
    deckVia = DV_BT; deckOpen(); shot("08_deck_bt"); deckLeave(); deckVia = DV_USB;
    // the ant farm: as it is, then 40 and 187 workers after days away
    antOpen(); run(3000); shot("09_ants");
    if (r == 0) {
      an.workers = 40; an.lastT -= 4 * 86400; anCatchUp(); anFields(); anSpawnAnts(); anBgNeed = true; run(5000); shot("10_ants_40");
      an.workers = 187; an.lastT -= 25 * 86400; anCatchUp(); anFields(); anSpawnAnts(); anBgNeed = true; run(5000); shot("11_ants_187");
      anLabels = true; shot("12_ants_labels"); anLabels = false;
    } else { anScroll = 200; shot("10_ants_wide_deep"); anScroll = 0; }
    antLeave();
  }
  themeSet(0); rot = 0; applyRotation();
  return 0;
}
