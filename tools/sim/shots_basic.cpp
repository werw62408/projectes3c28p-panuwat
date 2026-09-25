#include "sim_common.h"
int main(int argc, char** argv) {
  OUT = argc > 1 ? argv[1] : "out";
  std::string base = argc > 2 ? argv[2] : "state";
  mkdir(OUT.c_str(), 0755);
  system(("rm -rf '" + base + "' && mkdir -p '" + base + "/lfs' '" + base + "/sd'").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd";
  setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5;       // Thu 24 Sep 2026 20:15 (Thai time)
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal);
  prefs.putUInt("epoch", 1790255700);
  prefs.putString("ssid", "Panuwat_Home_5G");
  WiFi.nets = {{"Panuwat_Home_5G", -48, 3}, {"KKU-WiFi", -61, 0}, {"TrueMove H 4G_2.4GHz_A91C", -70, 3}, {"AIS 5G Fibre", -79, 3}, {"iPhone ของปาณุวัฒน์", -58, 3}};
  g_simBle = {{"Mi Smart Band 8", "c8:47:8c:1a:22:90", -52}, {"", "5e:21:9a:0b:77:10", -67}, {"JBL Tune 520BT", "b8:f6:53:0c:9e:41", -71}, {"", "71:0a:3f:ee:02:c9", -88}};
  LittleFS.begin(); LittleFS.mkdir("/log");
  seedLogs(1790255700);
  seedSd();
  setup();
  onTimeSync(nullptr);   // pretend NTP worked
  loadDay();

  for (int r : {0, 1}) {
    cur_prefix = r ? "wide_" : "tall_";
    rot = r; applyRotation();
    scr = S_HOME; scrollY = 0; shot("01_log");
    scrollY = 9999; shot("02_log_bottom"); scrollY = 0;
    { int tx, ty; homeTileRect(0, tx, ty); onLongPress(tx + 20, ty + 20); kpVal = "3"; shot("03_keypad"); scr = S_HOME; }
    goScreen(S_STATS); statView = 0; shot("04_stats_all");
    statView = 1; statSel = 0; shot("05_stats_days");
    statSel = 6; shot("05b_stats_days_fuel");
    statView = 2; shot("06_stats_hours");
    goScreen(S_APPS); shot("07_apps");
    scr = S_GAMES; shot("08_games");
    goScreen(S_SET); shot("09_settings");
    setScroll = 9999; shot("10_settings_bottom"); setScroll = 0;
    setOpenPage(1); shot("11_set_screen");
    setOpenPage(2); shot("12_set_wifi");
    qrWifi = false; shot("12b_set_wifi_qr2"); qrWifi = true;
    setOpenPage(3); shot("13_set_about");
    filesOpen(); shot("14_files");
    curDir = "/photos"; filesLoad(); shot("15_files_photos");
    fmIdx = 2; fmUi = FU_MENU; shot("16_files_menu");
    fmUi = FU_CONFIRM; fmAsk = 1; shot("17_files_confirm");
    fmUi = FU_PICK; pickDir = "/"; pickLoad(); shot("18_files_move");
    fmUi = FU_LIST; curDir = TRASH_DIR; filesLoad(); shot("19_files_trash");
    curDir = "/"; filesLoad();
    scr = S_AC; acS.power = true; shot("20_ac");
    acScroll = 9999; shot("21_ac_bottom"); acScroll = 0;
    sdkHave = false; prefs.putBytes("sdk", "", 0); sudokuOpen(); shot("22_sudoku_new");
    sdkNew(0); sdkSel = 30; if (sg.puz[30]) { for (int i = 0; i < 81; i++) if (!sg.puz[i]) { sdkSel = i; break; } } sdkPad = true; shot("23_sudoku_pad");
    sdkPad = false; sg.cur[sdkSel] = (sg.sol[sdkSel] % 9) + 1; sg.mistakes = 1; shot("24_sudoku_board");
    sudokuClose();
    scr = S_BT; btScan(); shot("25_bluetooth");
    scr = S_WIFI; wnets.clear(); wScanning = true; WiFi.scanState = WiFi.nets.size(); shot("26_wifi");
    kbdOpen("Password: TrueMove H 4G_2.4GHz_A91C", "abc123", nullptr); shot("27_keyboard");
    kbdShow = true; kbdLayer = 2; shot("27b_keyboard_123"); scr = S_SET;
    kbdOpen("New name", "Grandma_birthday_party_at_the_river_restaurant", nullptr); kbdShow = true; shot("27c_keyboard_rename"); scr = S_FILES;
    gameOpen(); gameDraw(); savePng(spr, "28_game_ready", W, H); gs = G_PLAY; for (int i = 0; i < 20; i++) gameStep(); levelMsgT = millis(); gameDraw(); savePng(spr, "29_game_play", W, H); scr = S_GAMES;
    // Internet (real downloads through curl)
    netTab = 0; netOpen(); shot("30_weather");
    netScroll = 9999; shot("31_weather_bottom"); netScroll = 0;
    netTab = 1; newsOpen = -1; netLoad(false); shot("32_news");
    netScroll = 330; shot("33_news_scrolled"); netScroll = 0;
    if (!news.empty()) { newsOpen = 0; shot("34_news_story"); newsOpen = -1; }
  }
  // ----- special cases (tall) -----
  cur_prefix = "x_";
  rot = 0; applyRotation();
  // no internet
  WiFi.connected = false; netTab = 0; wx.ok = false; netOpen(); shot("net_offline");
  // time unknown after a restart without Wi-Fi
  timeApprox = true; scr = S_HOME; shot("home_time_not_set");
  goScreen(S_SET); setOpenPage(3); shot("about_time_not_set");
  timeApprox = false; WiFi.connected = true;
  // SD card missing
  SD_MMC.present = false; sdOk = false; filesOpen(); shot("files_no_sd"); SD_MMC.present = true;
  // dark theme
  themeDark = 1; applyTheme(); scr = S_HOME; shot("home_dark"); goScreen(S_STATS); statView = 1; statSel = 0; shot("stats_dark");
  themeDark = 0; applyTheme();
  // Thai + long activity names (made on the web page)
  {
    Act a; a.id = "read"; a.name = "อ่านหนังสือก่อนนอน"; a.color = 0xC0612B; a.goal = 1; a.goalType = 1; acts.push_back(a);
    Act b; b.id = "med"; b.name = "Vitamin C and fish oil"; b.color = 0x2B7BC0; b.unit = "tablets"; b.step = 2; b.goal = 4; b.goalType = 1; acts.push_back(b);
    Act c; c.id = "coffee"; c.name = "Coffee"; c.color = 0x6E3B12; c.unit = "ml"; c.step = 250; c.goal = 500; c.goalType = 2; acts.push_back(c);
    logEvent(acts.size() - 1, 250); logEvent(acts.size() - 1, 300); logEvent(acts.size() - 2, 2);
    scr = S_HOME; scrollY = 9999; shot("home_thai_long_names"); scrollY = 0;
    goScreen(S_STATS); statView = 0; statScroll = 9999; shot("stats_all_thai"); statView = 2; heatScroll = 9999; shot("hours_thai");
  }
  // blocking screens drawn straight on the LCD
  lcd.setRotation(0);
  showImage("/photos/cat.jpg"); viewerBar(); shotLcd("viewer_bar");
  lcdConfirmDraw("Move to Trash?", fitText(FS, "Grandma_birthday_party_at_the_river_restaurant.jpg", 200)); shotLcd("viewer_confirm");
  videoEndDraw("graduation.mjpeg"); shotLcd("video_end");
  calText("Touch setup", "Tap the middle of each", "black square in the corners.", "4 corners, one at a time."); shotLcd("touch_setup");
  joyMsg("Done!", "The stick direction is saved.", "Try it in Apps > Game."); shotLcd("joystick_done");
  // bug check: typed value on a "count" activity
  {
    scr = S_HOME; int before = sumFor("water").count;
    kpAct = 0; kpVal = "3"; scr = S_KEYPAD; onTap(W - 20, FTR_Y + 10);
    printf("CHECK keypad: typed 3 on Water -> count went %d -> %d\n", before, sumFor("water").count);
  }
  return 0;
}
