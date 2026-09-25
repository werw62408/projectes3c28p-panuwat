#include "sim_common.h"
// v11.2 tests: DS3231 clock module, weekly backup, battery saving, and every bug fixed in v11.2

static int lines(const String& key) { int n = 0; forEachEvent(key, [&](const Ev&) { n++; }); return n; }
static int countIn(const std::string& dir) { int n = 0; DIR* d = opendir(dir.c_str()); if (!d) return 0; while (auto e = readdir(d)) if (e->d_name[0] != '.') n++; closedir(d); return n; }
static void setNow(uint32_t e) { g_simEpoch = (int64_t)e - (int64_t)(g_simMs / 1000); }
static void freshBoot(bool rtcOn) {   // like switching the board on again
  g_simRtcOn = rtcOn; timeApprox = true; timeSrc = TS_GUESS; guessEpoch = 0; guessedEv.clear(); rtcFound = rtcTimeOk = false;
  setup();
}

int main() {
  OUT = "run/pics"; mkdir("run", 0755); mkdir(OUT.c_str(), 0755);
  std::string base = "run/state_t112"; system(("rm -rf " + base + " && mkdir -p " + base + "/lfs " + base + "/sd " + base + "/logs").c_str());
  LittleFS.root = base + "/lfs"; SD_MMC.root = base + "/sd"; g_simLogsRoot = base + "/logs"; setenv("TZ", "ICT-7", 1); tzset();
  const uint32_t T0 = 1790255700;   // Thu 24 Sep 2026, 16:55
  uint16_t cal[8] = {0}; prefs.putBytes("tcal", cal, sizeof cal); prefs.putUInt("epoch", T0 - 3600);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(T0); seedSd();
  auto R = [](bool ok) { return ok ? "PASS" : "FAIL"; };

  // ---------- DS3231 ----------
  // C1 dates <-> module registers, both ways
  { bool ok = true; uint32_t tests[] = {1772323199u, 1835395200u, 1790255700u, 4102444799u, 1704067200u};
    g_simRtcOn = true;
    for (uint32_t e : tests) { rtcSet(e); uint32_t b = rtcGet(); if (b != e) { ok = false; printf("   %u -> %u\n", e, b); } }
    printf("C1 clock module keeps the date right (leap days, 2099): %s\n", R(ok)); }
  // C2 switch on with a module that knows the time: real time at once, no "~"
  { rtcSet(T0); setNow(1000); freshBoot(true);
    printf("C2 start with module: time %s, source %s, not a guess=%d %s\n", hhmm(nowT()).c_str(), TIME_SRC_N[timeSrc], !timeApprox, R(!timeApprox && timeSrc == TS_RTC && (uint32_t)nowT() - T0 < 3)); }
  // C3 module whose battery ran out ("stopped" flag): not trusted, board uses its guess
  { g_simRtc[0x0F] |= 0x80; setNow(1000); freshBoot(true);
    printf("C3 module lost its time: guess=%d, About says \"%s\" %s\n", timeApprox, clockModuleText().c_str(), R(timeApprox && rtcFound && !rtcTimeOk)); }
  // C4 then the internet time comes: the module is set again and the flag cleared
  { setNow(T0 + 60); onTimeSync(nullptr); timeFixPending = false; loop();
    uint32_t m = rtcGet();
    printf("C4 internet time written to the module: module %u, board %u, flag %d %s\n", m, (uint32_t)nowT(), g_simRtc[0x0F] >> 7, R(m && (uint32_t)nowT() - m < 3 && !(g_simRtc[0x0F] & 0x80))); }
  // C5 no module at all: works like v11 (a guess until Wi-Fi)
  { setNow(1000); freshBoot(false);
    printf("C5 no module: found=%d, guess=%d, About \"%s\" %s\n", rtcFound, timeApprox, clockModuleText().c_str(), R(!rtcFound && timeApprox)); }
  // C6 time from the phone web page also sets the module
  { g_simRtcOn = true; memset(g_simRtc, 0, sizeof g_simRtc); g_simRtc[0x0F] = 0x80;
    server.headers["Cookie"] = std::string("stpin=") + webPin.c_str(); server.args.clear(); server.args["t"] = String(T0 + 120).c_str();
    server.routes["POST /api/time"](); loop();
    printf("C6 phone time -> module: %u, source %s %s\n", rtcGet(), TIME_SRC_N[timeSrc], R(rtcGet() == T0 + 120 || rtcGet() == T0 + 121)); }
  setNow(T0); onTimeSync(nullptr); timeFixPending = false; loadDay();

  // ---------- bugs ----------
  // B1 a log that looks newer than now (clock moved back) is not "late"
  { Act a = acts[0]; a.remind = 30; setNow(T0); lastSeen[a.id] = T0 + 7200; bool od = overdue(a); loadDay();
    printf("B1 reminder with a log 'in the future': overdue=%d %s\n", od, R(!od)); }
  // B2 power cut while a day file was saved
  { File f = logFs().open("/log/2026-01-02.csv.new", "w"); f.print("1767312000,water,1,H,1\n"); f.close();
    File g = logFs().open("/log/2026-01-03.csv.new", "w"); g.print("half"); g.close();
    File h = logFs().open("/log/2026-01-03.csv", "w"); h.print("1767398400,water,1,H,1\n"); h.close();
    dayFilesRepair();
    bool ok = logFs().exists("/log/2026-01-02.csv") && !logFs().exists("/log/2026-01-02.csv.new") && !logFs().exists("/log/2026-01-03.csv.new") && lines("2026-01-03") == 1;
    printf("B2 half-saved day files repaired at start: %s\n", R(ok)); }
  // B3 undo of last night's log made on a guessed time must not come back when the time is fixed
  { uint32_t y = T0 + 3 * 3600 + 40 * 60;   // Thu 23:55
    setNow(y); Ev e{y, 1, 'H', false, "smoke"}; File f = logFs().open(logPath(dayKey(y)), "a"); f.print(evLine(e)); f.close();
    guessedEv.clear(); guessedEv.push_back(e);
    setNow(y + 20 * 60); loadDay(); timeApprox = true; guessEpoch = y + 1200; guessMs = millis();
    bool ok = undoEvent(3);
    printf("B3 undo last night's guessed log: undone=%d, left in guessed list=%d %s\n", ok, (int)guessedEv.size(), R(ok && guessedEv.empty()));
    timeApprox = false; guessedEv.clear(); setNow(T0); loadDay(); }
  // B4 typed "5" on Water = 5 logs in one write
  { int before = lines(curDay); if (!totalsOk) totalsLoad(); int tb = totals["water"].count;
    logValue(0, 5);
    printf("B4 typed 5 times: file %d -> %d, total %d -> %d %s\n", before, lines(curDay), tb, totals["water"].count, R(lines(curDay) == before + 5 && totals["water"].count == tb + 5)); }
  // B5 logs moved from a guessed time end up in time order
  { uint32_t d0 = 1789923600; String k = dayKey(d0);   // Mon 21 Sep 2026 00:00
    std::vector<String> ls; for (int h : {8, 12, 20}) ls.push_back(evLine(Ev{d0 + h * 3600u, 1, 'H', true, "water"})); writeDayFile(k, ls);
    guessedEv.clear(); uint32_t gt = 1789000000; Ev g{gt, 1, 'H', false, "toilet"}; guessedEv.push_back(g);
    File f = logFs().open(logPath(dayKey(gt)), "a"); f.print(evLine(g)); f.close();
    fixGuessedLogs((int32_t)(d0 + 10 * 3600 - gt));   // it really was 10:00 on the 21st
    std::vector<uint32_t> ts; forEachEvent(k, [&](const Ev& e) { ts.push_back(e.t); });
    bool sorted = std::is_sorted(ts.begin(), ts.end());
    printf("B5 moved log put in order: %d logs, in time order=%d %s\n", (int)ts.size(), sorted, R(ts.size() == 4 && sorted)); }
  // B6 Log page: a notice still shows while a reminder is waiting
  { timeApprox = true; remindAct = 0; scr = S_HOME; render();
    printf("B6 notice with a reminder waiting: bar=%d notice=%d %s\n", remindBarShown, remindBarIsNotice, R(remindBarShown && remindBarIsNotice));
    savePng(spr, "t112_notice_and_reminder", W, H); timeApprox = false; remindAct = -1; }
  // B7 web Wi-Fi save: no joining when "Internet Wi-Fi" is OFF, fields not sent are kept
  { staOn = false; homeSsid = "HomeNet"; int b0 = WiFi.beginCalls;
    server.args.clear(); server.args["ssid"] = "CafeWiFi"; server.args["pass"] = "12345678"; server.routes["POST /api/wifi"]();
    printf("B7 save Wi-Fi with Internet OFF: joined=%d, home kept=\"%s\" %s\n", WiFi.beginCalls - b0, homeSsid.c_str(), R(WiFi.beginCalls == b0 && homeSsid == "HomeNet"));
    staOn = true; }
  // B9 a download asked for while another runs starts right after it
  { g_simOffline = true; netDone = false; netHasNext = false; netBusy = true; netStart(1, 2);
    bool queued = netHasNext; netBusy = false; netPoll();
    printf("B9 queued download: queued=%d, started after=%d %s\n", queued, netDone || !netHasNext, R(queued && !netHasNext)); netPoll(); g_simOffline = false; }
  // B10 a photo from the phone while the file menu is open: the menu keeps its file
  { curDir = "/photos"; filesOpen(); String first = fList[0].name; fmIdx = 0; fmUi = FU_MENU;
    File f = SD_MMC.open("/photos/aaa_new.jpg", FILE_WRITE); f.print("x"); f.close();
    filesChanged(); bool kept = fList[0].name == first && filesStale;
    fmUi = FU_LIST; filesTick(); bool reloaded = false; for (auto& x : fList) if (x.name == "aaa_new.jpg") reloaded = true;
    printf("B10 upload during menu: menu file kept=%d, list updated after=%d %s\n", kept, reloaded, R(kept && reloaded)); }
  // B11 put back into a folder that was deleted
  { String e1 = fsTrash("/photos/Trip 2026/khonkaen_01.jpg"); SD_MMC.rmdir("/photos/Trip 2026");
    String tn; for (auto& x : trashIndex()) if (trashShowName(x.first) == "khonkaen_01.jpg") tn = x.first;
    String e2 = fsRestore(tn);
    printf("B11 put back into a deleted folder: \"%s\" \"%s\" exists=%d %s\n", e1.c_str(), e2.c_str(), SD_MMC.exists("/photos/Trip 2026/khonkaen_01.jpg"), R(e2 == "" && SD_MMC.exists("/photos/Trip 2026/khonkaen_01.jpg"))); }
  // B12 a folder with more than 300 files says so
  { SD_MMC.mkdir("/many"); for (int i = 0; i < 305; i++) { char b[32]; snprintf(b, sizeof b, "/many/p%03d.jpg", i); File f = SD_MMC.open(b, FILE_WRITE); f.print("x"); f.close(); }
    curDir = "/many"; filesLoad(); scr = S_FILES; filesScroll = 100000; render();
    printf("B12 305 files: listed %d, note=%d %s\n", (int)fList.size(), filesMore, R(fList.size() == 300 && filesMore)); savePng(spr, "t112_files_300", W, H); curDir = "/"; }

  // ---------- speed ----------
  // S1 Stats are counted again only when the logs changed
  { scr = S_STATS; statView = 0; statsNeed(7); st[0].daily[6] = -99; statsNeed(7); bool kept = st[0].daily[6] == -99;
    logDefault(0); statsNeed(7); bool fresh = st[0].daily[6] != -99;
    printf("S1 Stats kept when nothing changed=%d, counted again after a log=%d %s\n", kept, fresh, R(kept && fresh)); }
  // S2 Log page is not drawn again every second
  { scr = S_HOME; pw = P_ON; flashIdx = -1; lastTouchMs = millis(); setNow((T0 / 60) * 60 + 5); loop(); g_simMs += 1100; loop(); loop();   // a first tick in this minute (draws once)
    lcd.drawPixel(0, 0, 0x1234); g_simMs += 1100; loop(); bool skipped = lcd.readPixel(0, 0) == 0x1234;
    g_simMs += 60000; lastTouchMs = millis(); loop(); g_simMs += 1100; loop(); bool drawn = lcd.readPixel(0, 0) != 0x1234;
    printf("S2 Log page: 1 s later not redrawn=%d, next minute redrawn=%d %s\n", skipped, drawn, R(skipped && drawn)); }

  // ---------- backup ----------
  { setNow(T0); SD_MMC.mkdir("/SomudTick"); SD_MMC.mkdir("/SomudTick/backup");
    for (int i = 1; i <= 9; i++) { char b[64]; snprintf(b, sizeof b, "/SomudTick/backup/logs_2026-0%d-01.csv", i); File f = SD_MMC.open(b, FILE_WRITE); f.print("old"); f.close(); }
    bool ok = backupNow(); String p = "/SomudTick/backup/logs_" + dayKey(nowT()) + ".csv";
    File f = SD_MMC.open(p, "r"); int rows = 0; while (f && f.available()) { f.readStringUntil('\n'); rows++; } if (f) f.close();
    int kept = countIn(SD_MMC.root + "/SomudTick/backup");
    printf("K1 backup now: ok=%d, %d lines, files kept %d, About \"%s\" %s\n", ok, rows, kept, backupWhen().c_str(), R(ok && rows > 100 && kept == 8)); }
  { bkAt = (uint32_t)nowT() - 8 * 86400; bkTryMs = 0; pw = P_OFF; backupTask(0); bool ran = (uint32_t)nowT() - bkAt < 5;
    bkAt = (uint32_t)nowT() - 2 * 86400; uint32_t keep = bkAt; bkTryMs = 0; backupTask(0); bool waited = bkAt == keep;
    printf("K2 weekly backup: after 8 days ran=%d, after 2 days waited=%d %s\n", ran, waited, R(ran && waited)); pw = P_ON; }

  // ---------- battery saving ----------
  { scr = S_HOME; pw = P_ON; apOn = true; apAsleep = false; setupWifi(); WiFi.stations = 0; offIdx = 1;
    lastTouchMs = millis() - 61000; loop(); loop();
    bool off = pw == P_OFF && lowPower && lcd.asleep;
    WiFi.stations = 1; hotspotSleepTask(AP_SLEEP_MS + 1000); bool keptForPhone = !apAsleep && WiFi.apUp;
    WiFi.stations = 0; hotspotSleepTask(AP_SLEEP_MS + 1000); bool apRest = apAsleep && !WiFi.apUp;
    wake(); bool back = pw == P_ON && !lowPower && !lcd.asleep && !apAsleep && WiFi.apUp;
    printf("P1 screen off: low power=%d | phone on hotspot keeps it=%d | hotspot rests=%d | wake: all back=%d %s\n", off, keptForPhone, apRest, back, R(off && keptForPhone && apRest && back)); }

  // pictures of the new About page
  { scr = S_SET; setPage = 3; setScroll = 0; rtcFound = rtcTimeOk = true; timeSrc = TS_RTC; render(); savePng(spr, "t112_about_1", W, H);
    setScroll = 1000; render(); savePng(spr, "t112_about_2", W, H); }
  return 0;
}
