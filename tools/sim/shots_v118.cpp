#include "sim_common.h"
// v11.8 pictures: the main pages (Log, Apps, Settings, Deck, Games) and the Ant Colony game, tall and wide, on each theme.
//   ./shots.sh shots_v118.cpp ../../SomudTick_v11.8/SomudTick run/shots118
// (also works on older versions: the parts they don't have are skipped)

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }

#ifndef THEME_HUD
#define THEME_HUD 2
static void setThemeN(int t) { themeDark = t == 1; applyTheme(); }
static const int N_THEMES = 2;
#else
static void setThemeN(int t) { themeSet(t); }
static const int N_THEMES = 3;
#endif

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s118"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3; g_simOffline = true;
  const char* only = getenv("ONLY_THEME");   // e.g. ONLY_THEME=2: only the HUD pictures
  for (int th = 0; th < N_THEMES; th++) {
    if (only && atoi(only) != th) continue;
    for (int r : {0, 1}) {
      cur_prefix = std::string(th == 0 ? "light_" : th == 1 ? "dark_" : "hud_") + (r ? "wide_" : "tall_");
      setThemeN(th); rot = r; applyRotation();
#ifdef HAS_ANTS
      if (th == 2) { bootHud(4); savePng(spr, "00_boot", W, H); }
#endif
      remindAct = -1; scr = S_HOME; scrollY = 0; shot("01_log");
      scr = S_APPS; shot("02_apps");
      remindAct = 0; shot("03_apps_remind"); remindAct = -1;
      goScreen(S_SET); shot("04_settings");
      scr = S_GAMES; shot("05_games");
      deckOpen(); shot("06_deck"); deckLeave();
#ifdef HAS_ANTS
      antOpen(); run(3000); shot("07_ants");
      if (th == 2 || !only) {   // the same colony later: 40 and 187 workers, dug while nobody looked (as if 3 and 20 days passed)
        an.workers = 40; an.lastT -= 3 * 86400; anCatchUp(); anFields(); anSpawnAnts(); anBgNeed = true; run(4000); shot("07b_ants_40");
        an.workers = 187; an.lastT -= 20 * 86400; anCatchUp(); anFields(); anSpawnAnts(); anBgNeed = true; run(4000); anLabels = true; shot("07c_ants_187"); anLabels = false;
      }
      antLeave();
#else
      gardenOpen(); shot("07_garden"); scr = S_GAMES;
#endif
      scr = S_STATS; shot("08_stats");
      kpAct = 0; kpVal = "12"; scr = S_KEYPAD; shot("09_keypad"); scr = S_HOME;
    }
  }
  setThemeN(0); rot = 0; applyRotation();
  return 0;
}
