#include "sim_common.h"

int main(int argc,char**argv){
  OUT=argv[1]; mkdir(OUT.c_str(),0755);
  std::string base="run/state_s10"; system(("rm -rf "+base+" && mkdir -p "+base+"/lfs "+base+"/sd "+base+"/logs").c_str());
  LittleFS.root=base+"/lfs"; SD_MMC.root=base+"/sd"; g_simLogsRoot=base+"/logs"; setenv("TZ","ICT-7",1); tzset();
  g_simEpoch=1790255700-5; uint16_t cal[8]={0}; prefs.putBytes("tcal",cal,sizeof cal); prefs.putUInt("epoch",1790255700);
  prefs.putString("ssid","Panuwat_Home_5G");
  WiFi.nets = {{"Panuwat_Home_5G", -48, 3}, {"KKU-WiFi", -61, 0}, {"TrueMove H 4G_2.4GHz_A91C", -70, 3}};
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending=false; loadDay();
  { Act c; c.id="coffee"; c.name="Coffee"; c.color=0x6E3B12; c.unit="ml"; c.step=250; c.goal=500; c.goalType=2; acts.push_back(c); logEvent(acts.size()-1,250); logEvent(acts.size()-1,300); }
  for (int r : {0,1}) {
    cur_prefix = r ? "wide_" : "tall_"; rot=r; applyRotation();
    scr=S_HOME; scrollY=0; shot("01_log"); scrollY=9999; shot("02_log_bottom"); scrollY=0;
    goScreen(S_STATS); statView=0; shot("03_stats_all"); statView=1; statSel=0; shot("04_stats_days"); statView=2; shot("05_stats_hours");
    goScreen(S_APPS); shot("06_apps"); scr=S_GAMES; shot("07_games");
    goScreen(S_SET); shot("08_settings"); setOpenPage(2); shot("09_set_wifi"); setScroll=9999; shot("09b_set_wifi_bottom"); setScroll=0; setAboutPage(); shot("10_about");
    filesOpen(); shot("11_files"); curDir="/photos"; filesLoad(); shot("12_photos"); fmIdx=2; fmUi=FU_CONFIRM; fmAsk=1; shot("13_trash_confirm"); fmUi=FU_LIST; curDir="/"; filesLoad();
    scr=S_AC; acS.power=true; shot("14_ac");
    sdkHave=false; prefs.putBytes("sdk","",0); sudokuOpen(); sdkNew(0); for(int i=0;i<81;i++) if(!sg.puz[i]){sdkSel=i;break;} shot("15_sudoku"); sdkPad=true; shot("16_sudoku_pad"); sdkPad=false; sudokuClose();
    sandOpen(); saHint=true; sandDraw(); savePng(spr,"17_sand_start",W,H);
    saHint=false; saTool=ST_ROCK; saLastGX=-1; saPour(W*0.12,H*0.62); saPour(W*0.55,H*0.72); saTool=ST_SAND; saColor=8;
    for(int f=0;f<200;f++){ saLastGX=-1; saPour(W*0.3+(f%30)*0.5, SA_BAR+14); if(f>100){saColor=1; saLastGX=-1; saPour(W*0.75,SA_BAR+14); saColor=8;} saStep(0);} 
    saTool=ST_WATER; for(int f=0;f<120;f++){ saLastGX=-1; saPour(W*0.5,SA_BAR+12); saStep(0);} for(int f=0;f<150;f++) saStep(0);
    saTool=ST_SAND; sandDraw(); savePng(spr,"18_sand_play",W,H);
    themeDark=1; applyTheme(); saMakePalette(); sandDraw(); savePng(spr,"19_sand_dark",W,H); themeDark=0; applyTheme(); sandClose();
    netTab=0; netOpen(); netPoll(); shot("20_weather"); netTab=1; newsCat=0; netLoad(false); netPoll(); shot("21_news_oil");
    scr=S_USB; usbNote="Logs: SomudTick/logs.csv"; usbBytes=12*1048576; usbLastIoMs=millis(); shot("22_usb"); usbAsk=true; shot("23_usb_leave"); usbAsk=false;
    scr=S_APPS; remindAct=0; shot("24_remind_bar"); remindAct=-1;
    scr=S_KEYPAD; kpAct=0; kpVal="3"; shot("25_keypad");
  }

  cur_prefix="m_"; rot=0; applyRotation(); themeDark=0; applyTheme();
  scr=S_HOME; scrollY=0; timeApprox=false;
  goScreen(S_STATS); statView=2; shot("stats_hours");
  scr=S_GAMES; shot("games");
  gdLoaded=false; gdSyncedFor=""; prefs.putString("gdDay",""); gardenOpen(); shot("garden");
  scr=S_WIFI; wnets.clear(); wScanning=true; WiFi.scanState=WiFi.nets.size(); shot("wifi");
  kbdOpen("Password: KKU-WiFi","abc",nullptr); shot("keyboard"); scr=S_SET;
  g_simBle={{"Mi Smart Band 8","c8:47:8c:1a:22:90",-52},{"","5e:21:9a:0b:77:10",-67}}; scr=S_BT; btScan(); shot("bluetooth");
  netTab=1; newsCat=1; scr=S_NET; netLoad(false); netPoll(); shot("news_ai"); newsOpen=0; shot("news_story"); newsOpen=-1; newsCat=2; netLoad(false); netPoll(); shot("news_robots"); newsCat=0;
  scr=S_AC; acScroll=9999; shot("ac_bottom"); acScroll=0;
  filesOpen(); curDir=TRASH_DIR; filesLoad(); shot("trash"); curDir="/"; filesLoad();
  sdkHave=false; prefs.putBytes("sdk","",0); sudokuOpen(); shot("sudoku_new"); sudokuClose();
  goScreen(S_SET); setOpenPage(1); shot("set_screen");
  setAboutPage(); askUpdate=true; shot("ask_update"); askUpdate=false;
  themeDark=1; applyTheme(); scr=S_HOME; shot("home_dark"); goScreen(S_STATS); statView=1; shot("stats_dark"); scr=S_GARDEN; shot("garden_dark"); themeDark=0; applyTheme();
  timeApprox=true; scr=S_HOME; shot("home_time_notice"); gdLoaded=false; gdSyncedFor=""; prefs.putString("gdDay",""); prefs.putUInt("gdPts",0); scr=S_GARDEN; shot("garden_time_unknown"); timeApprox=false;
  logPctCache=93; scr=S_HOME; shot("home_space_notice"); logPctCache=0;
  { int r=0; for(int k=0;k<(int)acts.size();k++){} scr=S_HOME; lastSeen["water"]=nowT()-7200; }

  cur_prefix="x_"; rot=0; applyRotation();
  timeApprox=true; scr=S_HOME; shot("time_not_set"); timeApprox=false;
  gs=G_READY; gameOpen(); gameDraw(); savePng(spr,"game_ready",W,H); scr=S_GAMES;
  lcd.setRotation(0); lcdConfirmDraw("Move to Trash?", fitText(FS,"beach_trip.jpg",200)); shotLcd("viewer_confirm");
  enterUpdateMode(); shotLcd("update_mode");
  return 0;
}
