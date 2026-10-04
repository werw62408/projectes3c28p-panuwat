#include "sim_common.h"
// v13.1 pictures: the v13 set again (the small screen in every state, Remotes, AC, Settings, About, Apps, notices),
// plus what v13.1 changed: LOG FULL on the small screen with the big screen off, the AC "Set from the remote" bar,
// Settings scrolled to its end above the reminder bar (the ring on About), a window with no reminder bar over it.
//   ./shots.sh shots_v131.cpp ../../SomudTick_v13.1/SomudTick run/shots131

static void run(int ms) { uint64_t end = g_simMs + ms; while (g_simMs < end) { g_simMs += 20; loop(); } }
static void oled(const std::string& n) { oledT = 0; oledTask(); oledPng(n); }
static void irPush(decode_type_t t, uint64_t v, uint16_t bits, bool rep = false) {
  SimIrMsg m; m.r.decode_type = t; m.r.value = v; m.r.bits = bits; m.r.repeat = rep;
  for (int k = 0; k < 68; k++) m.raw.push_back(k % 2 ? 560 : (k == 0 ? 9000 : 1690));
  g_simIrIn.push_back(m);
}
static void irPushAc(bool pw, uint8_t mode, uint8_t temp, uint8_t fan) {
  SimIrMsg m; m.r.decode_type = PANASONIC_AC; m.r.bits = 27 * 8;
  m.r.state[0] = 0x02; m.r.state[13] = pw; m.r.state[14] = mode; m.r.state[15] = temp; m.r.state[16] = fan; m.r.state[17] = kPanasonicAcSwingVAuto; m.r.state[19] = kPanasonicJke;
  for (int k = 0; k < 440; k++) m.raw.push_back(k % 2 ? 430 : 1300);
  g_simIrIn.push_back(m);
}

int main(int argc, char** argv) {
  OUT = argv[1]; mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_s131"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  g_simEpoch = 1790255700 - 5; uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", 1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  g_simOledOn = true; g_simPadOn = true;   // the small screen and the button board are on the wires
  setup();
  cur_prefix = ""; oledPng("oled_00_start");
  onTimeSync(nullptr); timeFixPending = false; loadDay();
  offIdx = 3; g_simOffline = true; animOn = false;
  run(400);
  // ---------------- the small screen ----------------
  scr = S_HOME; oled("oled_01_clock_log");
  goScreen(S_APPS); run(100); oled("oled_02_clock_apps");
  deckOpen(); run(100); oled("oled_03_clock_deck"); deckLeave();
  goScreen(S_SET); setAboutPage(); run(100); oled("oled_04_clock_about"); setOpenPage(0); goScreen(S_HOME);
  oledPage = 1; oled("oled_05_goals"); oledPage = 0;
  remindAct = 0; remindId = acts[0].id; g_simEpoch += (g_simEpoch % 2 ? 0 : 1); oled("oled_06_remind_a"); g_simEpoch += 1; oled("oled_07_remind_b");
  popAct = 0; popMs = millis(); popText = "+1"; oled("oled_08_plus1"); popAct = -1; remindAct = -1;
  burstNoticeMs = millis(); burstName = "Button E loose?"; oled("oled_09_loose"); burstNoticeMs = 0;
  logFull = true; oled("oled_10_logfull"); logFull = false;
  irLearnSlot = 0; irLearnT = millis(); oled("oled_11_learn"); irLearnSlot = -1;
  pw = P_OFF; remindAct = 1; remindId = acts[1].id; oled("oled_12_bigoff_remind"); remindAct = -1; oled("oled_13_bigoff"); wake();
  timeApprox = true; oled("oled_14_time_not_set"); timeApprox = false;
  { String n0 = acts[0].name; acts[0].name = "ดื่มน้ำ";   // a Thai name: a taller row, the clock smaller
    remindAct = 0; remindId = acts[0].id; if (time(nullptr) & 1) g_simEpoch += 1; oled("oled_15_thai_remind"); remindAct = -1;
    oledPage = 1; oled("oled_16_thai_goals"); oledPage = 0;
    popAct = 0; popMs = millis(); popText = "+1"; oled("oled_17_thai_plus1"); popAct = -1;
    acts[0].name = n0; }
  logFull = true; pw = P_OFF; oled("oled_18_logfull_bigoff"); logFull = false; wake();   // v13.1: one quiet line, no white screen
  // ---------------- the big screen ----------------
  for (int th : {0, 2}) for (int r : {0, 1}) for (int bare : {1, 0}) {
    if (!bare && th == 2) continue;
    cur_prefix = std::string(bare ? "bare_" : "bars_") + (th == 2 ? "hud_" : "light_") + (r ? "wide_" : "tall_");
    barsMode = bare ? 0 : 1; themeSet(th); rot = r; applyRotation(); layoutUpdate();
    goScreen(S_APPS); shot("01_apps");
    remindAct = 0; remindId = acts[0].id; shot("02_apps_remind"); remindAct = -1;
    for (auto& k : irKeys) k = IrKey();
    remoteOpen(); shot("03_remotes_empty");
    const char* NM[] = {"TV power", "Vol +", "Vol -", "Fan speed", "Light", "Ch +", "Ch -", "Mute"};
    for (int i = 0; i < 8; i++) { irKeys[i].used = true; irKeys[i].name = NM[i]; irKeys[i].type = NEC; irKeys[i].bits = 32; irKeys[i].value = 0x20DF0000 + i; }
    irKeys[1].act = IA_UP; irKeys[2].act = IA_DOWN; irKeys[7].act = IA_OK;
    shot("04_remotes_keys");
    irMenuKey = 1; irMenu = 1; shot("05_remotes_menu"); irMenu = 2; shot("06_remotes_useas"); irMenu = 3; shot("07_remotes_delete"); irMenu = 0;
    irLearnSlot = 9; irLearnT = millis(); shot("08_remotes_learning"); irLearnSlot = -1;
    irLogN = 0; irQuietUntil = 0;
    irPush(NEC, 0x20DF0001, 32); run(60); irPushAc(true, kPanasonicAcCool, 25, kPanasonicAcFanMed); run(60); irPush(SONY, 0xA90, 12); run(60);
    { SimIrMsg m; m.r.decode_type = UNKNOWN; m.r.value = 0x8A1B2C3D; m.r.bits = 32; for (int k = 0; k < 40; k++) m.raw.push_back(700); g_simIrIn.push_back(m); run(60); }
    navShow = false; irSigView = true; shot("09_remotes_signals"); irSigView = false;
    scr = S_AC; acScroll = 0; acSyncMs = millis(); shot("10_ac_synced"); acScroll = 9999; shot("11_ac_bottom"); acScroll = 0;
    goScreen(S_SET); setOpenPage(1); shot("12_set_screen"); setAboutPage(); shot("13_about"); setScroll = 9999; shot("14_about_end"); setOpenPage(0);
    scr = S_HOME; logFull = true; shot("15_log_full"); logFull = false;
    burstNoticeMs = millis(); burstName = "Button E loose?"; scr = S_APPS; shot("16_button_loose"); burstNoticeMs = 0;
    scr = S_GAMES; shot("17_games");
    // v13.1: Settings at its end with the reminder bar: the last row (About) comes up above the bar, the ring on it
    goScreen(S_SET); setOpenPage(0); remindAct = 0; remindId = acts[0].id; navShow = true; navPage = -1; setScroll = 9999; dirty = true; render(); setScroll = 9999;
    { dirty = true; render(); int best = -1; for (size_t i = 0; i < navT.size(); i++) if (navT[i].w == W - 16 && navT[i].h == 38 && (best < 0 || navT[i].y > navT[best].y)) best = i; if (best >= 0) navFocus(navT[best]); }
    shot("18_settings_end_remind");
    remoteOpen(); irMenuKey = 1; irMenu = 2; shot("19_window_no_bar"); irMenu = 0; remindAct = -1; navShow = false; goScreen(S_HOME);
  }
  return 0;
}
