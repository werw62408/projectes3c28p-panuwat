#include "sim_common.h"
// v11.5 pictures: the new and changed screens, tall and wide, light and dark.
//   ./shots.sh shots_v115.cpp ../../SomudTick_v11.5/SomudTick run/shots115

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s115"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3;
  { File f = logFs().open(logPath(curDay), "a"); f.print(String((unsigned long)nowT() - 60) + ",water,1,H,0\n"); f.close(); logRev++; }   // one log with an unsure time
  { Act a; a.id = "walk"; a.name = "Walk"; a.unit = "kilometres"; acts.push_back(a); }
  for (int dark = 0; dark < 2; dark++)
    for (int r : {0, 1}) {
      cur_prefix = std::string(dark ? "dark_" : "") + (r ? "wide_" : "tall_");
      themeDark = dark; applyTheme(); rot = r; applyRotation();
      goScreen(S_SET); setScroll = 0; shot("01_settings");
      setAboutPage(); setScroll = 0; shot("02_about_top");
      setScroll = 9999; shot("03_about_bottom"); setScroll = 0;
      askUpdate = true; shot("04_ask_update"); askUpdate = false;
      kbdOpen("Password: KKU-WiFi", "my_pass", nullptr); kbdShow = true; kbdLayer = 2; shot("05_keyboard_123"); kbdLayer = 3; shot("06_keyboard_more"); kbdLayer = 0; scr = S_SET;
      kpAct = acts.size() - 1; kpVal = "12.5"; scr = S_KEYPAD; shot("07_keypad_long_unit"); kpAct = 0; kpVal = "3"; shot("08_keypad_times"); scr = S_HOME;
      mazeOpen(); mzSetState(MZ_PLAY); mzStartMs = millis() - 12000; mazeDraw(); savePng(spr, "09_maze_play", W, H);
      mzSetState(MZ_PAUSE); mazeDraw(); savePng(spr, "10_maze_pause", W, H);
      mzSetState(MZ_READY); mazeDraw(); savePng(spr, "11_maze_ready", W, H); mazeLeave();
      blocksOpen(); blState = BL_PLAY; blocksDraw(); savePng(spr, "12_blocks_play", W, H);
      blState = BL_READY; blocksDraw(); savePng(spr, "13_blocks_ready", W, H);
      blState = BL_PAUSE; blocksDraw(); savePng(spr, "14_blocks_pause", W, H); blocksLeave();
      sudokuOpen(); if (sdkNewMenu) sdkNew(0); sdkPad = false; for (int i = 0; i < 81; i++) if (!sg.puz[i]) { sdkSel = i; break; } sdkPad = true; shot("15_sudoku_pad"); sdkPad = false; sudokuClose();
      { std::vector<Oil> v; const char* N[] = {"ดีเซล B20", "ไฮดีเซล S", "ไฮ พรีเมียม ดีเซล พลัส", "ไฮ พรีเมียม 98 พลัส", "แก๊สโซฮอล์ E85 S EVO", "แก๊สโซฮอล์ E20 S EVO", "แก๊สโซฮอล์ 91 S EVO", "แก๊สโซฮอล์ 95 S EVO"};
        const float P[] = {36.44, 41.44, 50.05, 49.29, 30.88, 34.94, 39.57, 39.94};
        for (int i = 0; i < 8; i++) { Oil o; o.name = N[i]; o.today = P[i]; o.tomorrow = P[i]; o.dif = i == 7 ? 0.4f : i == 1 ? -0.3f : 0; v.push_back(o); }
        oilSort(v); oil = v; newsC[0].clear(); newsAtC[0] = millis();
        netTab = 1; newsCat = 0; scr = S_NET; newsOpen = -1; netMsg = ""; netScroll = 0; shot("16_oil"); netScroll = 9999; shot("17_oil_bottom"); netScroll = 0; }
      scr = S_USB; usbNote = "Logs: SomudTick/logs.csv"; usbBytes = 0; shot("18_usb"); scr = S_HOME;
      { lcd.setRotation(rot); enterUpdateMode(); shotLcd("19_update_mode"); lcd.setRotation(rot); }
    }
  themeDark = 0; applyTheme(); rot = 0; applyRotation();
  return 0;
}
