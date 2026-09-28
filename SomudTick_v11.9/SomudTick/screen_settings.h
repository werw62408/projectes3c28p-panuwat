#pragma once
// Settings screen: main page, Screen, Wi-Fi, About.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Settings ----------------
// One function both draws and handles taps, so buttons and touch areas always match.


// Settings: a main page (the things you change most + a list of groups) and one page per group.
int setPage = 0;   // 0 main, 1 Screen, 2 Wi-Fi & phone, 3 About
bool askUpdate = false;   // About: "Restart for update?" box
const char* SET_PAGE_N[4] = {"Settings", "Screen", "Wi-Fi", "About"};
void setOpenPage(int p) { setPage = p; setScroll = 0; dirty = true; }
void setScreenPage() { setOpenPage(1); }
void setWifiPage() { setOpenPage(2); }
bool sdMount();
void sdCountFiles(); String sdCardText(); String sdFilesText();   // app_media.h
void setAboutPage() { sdCountFiles(); setOpenPage(3); }
extern bool sdOk;
// About: why the board started last time (a crash or "low power" shows in red)
bool lastStartBad = false;
// v11.9: the screen that was open when the board crashed, kept over the restart (RTC memory), so a crash can be found
#ifdef SIM
uint32_t crashMark = 0, crashScrNow = 0, crashUpMin = 0;
#else
RTC_NOINIT_ATTR uint32_t crashMark, crashScrNow, crashUpMin;
#endif
uint32_t crashScrWas = 0xFF, crashUpWas = 0;
void crashNote() { crashMark = 0xC0DE5CA1; crashScrNow = (uint32_t)scr; crashUpMin = millis() / 60000; }   // from loop()
void crashBoot() { if (crashMark == 0xC0DE5CA1) { crashScrWas = crashScrNow; crashUpWas = crashUpMin; } crashNote(); }   // from setup()
const char* scrName(uint32_t s) {
  static const char* N[] = {"Log", "Stats", "Apps", "Settings", "Keypad", "Heat", "Files", "AC", "Pixel Swim", "Sudoku", "Internet", "Wi-Fi", "Keyboard",
                            "Bluetooth", "Games", "Sand", "Ants", "USB", "Maze", "Blocks", "GB list", "Game Boy", "Deck"};
  return s < sizeof(N) / sizeof(N[0]) ? N[s] : "?";
}
String crashWhere(const char* what) { return crashScrWas == 0xFF ? String(what) : String(what) + " in " + scrName(crashScrWas) + ", " + crashUpWas + "m"; }
String lastStartWhy() {
#ifdef SIM
  return "power on";
#else
  lastStartBad = false;
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "restart";
    case ESP_RST_EXT: return "RESET button";
    case ESP_RST_PANIC: lastStartBad = true; return crashWhere("crash");
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: lastStartBad = true; return crashWhere("froze");
    case ESP_RST_BROWNOUT: lastStartBad = true; return "battery too low";
    default: return "other";
  }
#endif
}
uint32_t memLowKB() {   // the least free memory since the start (red in About when it gets low)
#ifdef SIM
  return 150;
#else
  return ESP.getMinFreeHeap() / 1024;
#endif
}
String memText() {
#ifdef SIM
  return "180 KB";
#else
  return String(ESP.getFreeHeap() / 1024) + " KB";
#endif
}
String clockModuleText() {
  if (!rtcFound) return "not found";
  if (!rtcTimeOk) return "time lost: join Wi-Fi";
  return "OK";
}
extern uint32_t bkAt; extern String bkErr; String backupWhen(); bool backupNow();   // backup.h
void settingsUI(bool draw, int tx, int ty) {
  const int X = 8, CW = W - 16;
  int y = HDR_H + (setPage ? 40 : 6) - setScroll;
  bool hit = false;
  auto label = [&](const String& s) {   // HUD: "// BRIGHTNESS" with a faint line after it
    if (draw && themeHud) {
      String t = "// " + hudUp(s); txt(FS, fitText(FS, t, CW), X, y, SOFT);
      spr.setFont(pickFont(FS, t)); int e = X + min(CW, (int)spr.textWidth(t)) + 6;
      if (e < X + CW - 4) for (int k = e; k < X + CW; k += 4) spr.drawPixel(k, y + 8, C(LINE));
    } else if (draw) txt(FS, fitText(FS, s, CW), X, y, SOFT);
    y += 20;
  };
  auto seg = [&](int n, int sel, const char* const* names, std::function<void(int)> act) {
    int gap = n >= 4 ? 4 : 6, w = (CW - (n - 1) * gap) / n;
    for (int i = 0; i < n; i++) {
      int x = X + i * (w + gap);
      if (draw) btn(x, y, w, 32, names[i], i == sel);
      else if (!hit && hitR(tx, ty, x, y, w, 32)) { act(i); hit = true; }
    }
    y += 40;
  };
  auto bar5 = [&](int level, std::function<void(int)> set) {   // [-] 5 blocks [+]
    if (draw) {
      btn(X, y, 40, 32, "-", false);
      btn(X + CW - 40, y, 40, 32, "+", false);
      int bx = X + 48, bw = CW - 96, sw = (bw - 4 * 4) / 5;
      if (themeHud) hudSegs(bx + 2, y + 7, bw - 4, 18, 5, level + 1, INK, blend(PAPER, INK, 0.16f));   // slanted segments
      else for (int i = 0; i < 5; i++) spr.fillRoundRect(bx + i * (sw + 4), y + 8, sw, 16, 4, C(i <= level ? INK : LINE));
    } else if (!hit && hitR(tx, ty, X, y, CW, 32)) {
      int v = level;
      if (tx < X + 48) v--;
      else if (tx > X + CW - 48) v++;
      else v = (tx - X - 48) * 5 / (CW - 96);
      set(constrain(v, -1, 4)); hit = true;
    }
    y += 40;
  };
  // a row that opens a page: [icon] Title .......... value  >
  auto navRow = [&](const String& t, const String& val, void (*fn)()) {
    if (draw) {
      navAdd(X, y, CW, 38);
      String tt = hudUp(t), vv = hudUp(val);
      if (themeHud) {   // cut corners and a ">>" mark
        hudShape(X, y, CW, 38, 8, CARD, LINE);
        for (int k = 0; k < 2; k++) { int ax = X + CW - 18 + k * 6; spr.drawLine(ax, y + 13, ax + 5, y + 19, C(INK)); spr.drawLine(ax + 5, y + 19, ax, y + 25, C(INK)); }
      } else {
        spr.fillRoundRect(X, y, CW, 38, 10, C(CARD));
        spr.drawRoundRect(X, y, CW, 38, 10, C(LINE));
        spr.fillTriangle(X + CW - 14, y + 13, X + CW - 14, y + 25, X + CW - 8, y + 19, C(SOFT));
      }
      txt(FB, fitText(FB, tt, CW - 30), X + 12, y + 19, themeHud ? HUDB : INK, D_ML);
      spr.setFont(pickFont(FB, tt)); int tw = spr.textWidth(fitText(FB, tt, CW - 30));
      if (vv.length()) txt(FS, fitText(FS, vv, CW - tw - 44), X + CW - (themeHud ? 26 : 22), y + 19, SOFT, D_MR);
    } else if (!hit && hitR(tx, ty, X, y, CW, 38)) { hit = true; fn(); }
    y += 44;
  };
  if (setPage == 0) {
    label("Where am I now?");
    { const char* n[] = {"Home", "Uni"};
      seg(2, place == 'H' ? 0 : 1, n, [](int i) { place = i ? 'U' : 'H'; prefs.putChar("place", place); }); }
    bool canAuto = homeSsid.length() || uniSsid.length();
    if (draw && canAuto) navAdd(X, y - 8, CW, 24);
    if (draw && themeHud && canAuto) {
      txt(FS, "AUTO SWITCH BY WI-FI", X, y - 4, INK);
      txt(FB, autoPlace ? "[ ON ]" : "[ OFF ]", X + CW, y - 4, autoPlace ? HUDB : SOFT, D_TR);
    } else if (draw) txt(FS, fitText(FS, hudUp(canAuto ? (String("Auto switch by Wi-Fi: ") + (autoPlace ? "ON" : "OFF"))
                                           : String("Auto Home/Uni: set on web")), CW), X, y - 4, canAuto ? INK : SOFT);
    else if (!hit && canAuto && hitR(tx, ty, 0, y - 8, W, 24)) { autoPlace = !autoPlace; prefs.putBool("auto", autoPlace); hit = true; }
    y += 24;
    label("Brightness");
    bar5(brightIdx, [](int v) { brightIdx = max(0, v); prefs.putUChar("bright", brightIdx); lcd.setBrightness(BRIGHT[brightIdx]); });
    label(String("Sound volume") + (volIdx < 0 ? " (off)" : ""));
    bar5(volIdx, [](int v) { volIdx = v; prefs.putChar("vol", volIdx); audioSetVolume(); sfx(SFX_TAP); });
    { const char* n[] = {"Sounds", "Motion"};   // v11.9: button sounds, page wipes (lit = on)
      int bw = (CW - 6) / 2;
      if (draw) { btn(X, y, bw, 32, sfxOn ? "Sounds ON" : "Sounds OFF", sfxOn); btn(X + bw + 6, y, bw, 32, animOn ? "Motion ON" : "Motion OFF", animOn); }
      else if (!hit && hitR(tx, ty, X, y, bw, 32)) { sfxOn = !sfxOn; prefs.putBool("sfx", sfxOn); sfx(SFX_TAP); hit = true; }
      else if (!hit && hitR(tx, ty, X + bw + 6, y, bw, 32)) { animOn = !animOn; prefs.putBool("anim", animOn); hit = true; }
      (void)n; y += 40; }
    label("More");
    const char* dirN[4] = {"Tall", "Wide", "Tall (flip)", "Wide (flip)"};
    navRow("Screen", String(dirN[rot]) + ", " + (themeHud ? "HUD" : themeDark ? "Dark" : "Light"), setScreenPage);
    navRow("Wi-Fi", String(apOn ? "Hotspot" : "") + (apOn && staOn ? ", " : "") + (staOn ? (staOk() ? "Net OK" : "no net") : "") + (!apOn && !staOn ? "OFF" : ""), setWifiPage);
    navRow("Bluetooth", "", btPageOpen);
    navRow("Joystick direction", "", joySetup);
    // v11.3: the stick can move around every menu (it always works in the games)
    if (draw) btn(X, y, CW, 34, joyNavOn ? (joyOk ? "Joystick in menus: ON" : "Joystick in menus: no stick") : "Joystick in menus: OFF", joyNavOn && joyOk);
    else if (!hit && hitR(tx, ty, X, y, CW, 34)) {
      hit = true; joyNavOn = !joyNavOn; prefs.putBool("joyNav", joyNavOn);
      if (joyNavOn && !joyOk) joyDetect();   // plugged in after the start
      if (!joyNavOn) navShow = false;
    }
    y += 40;
    if (draw) { btn(X, y, CW, 38, "Fix touch (calibrate)", false); }
    else if (!hit && hitR(tx, ty, X, y, CW, 38)) { hit = true; runTouchCal(); return; }
    y += 44;
    navRow("About", timeApprox ? "time not set!" : String(FW_VERSION) + ", " + (batPct() < 0 ? String("USB") : String(batPct()) + "%"), setAboutPage);   // the version is seen at once (v11.5)
  } else if (setPage == 1) {
    label("Screen off after (no touch)");
    seg(4, offIdx, OFF_NAMES, [](int i) { offIdx = i; prefs.putUChar("offT", offIdx); });
    label("Screen direction");
    { const char* n1[] = {"Tall", "Wide"}, *n2[] = {"Tall (flip)", "Wide (flip)"};   // flip = upside down
      seg(2, rot < 2 ? rot : -1, n1, [](int i) { rot = i; prefs.putUChar("rot", rot); applyRotation(); });
      seg(2, rot >= 2 ? rot - 2 : -1, n2, [](int i) { rot = i + 2; prefs.putUChar("rot", rot); applyRotation(); }); }
    label("Theme");
    { const char* n[] = {"Light", "Dark", "HUD"};   // v11.8: HUD = black sci-fi look
      seg(3, themeNow(), n, [](int i) { themeSet(i); }); }
    if (themeHud) {   // HUD colour: Arctic white, Amber, Cyan, Green
      label(String("HUD color: ") + HUDCOLS[hudCol].name);
      int sw = CW / N_HUDCOLS;
      for (int i = 0; i < N_HUDCOLS; i++) {
        int cx = X + i * sw + sw / 2;
        if (draw) {
          navAdd(cx - 20, y - 1, 40, 34);
          hudShape(cx - 17, y + 2, 34, 28, 6, i == hudCol ? HUDCOLS[i].main : CARD, HUDCOLS[i].main);
          if (i != hudCol) hudShape(cx - 11, y + 8, 22, 16, 4, HUDCOLS[i].main, -1);
        } else if (!hit && hitR(tx, ty, X + i * sw, y, sw, 36)) {
          hudCol = i; prefs.putUChar("hudCol", hudCol); applyTheme(); hit = true;
        }
      }
      y += 42;
    } else {
    label("Text color");
      int sw = CW / N_TEXTCOLS;
      for (int i = 0; i < N_TEXTCOLS; i++) {
        int cx = X + i * sw + sw / 2;
        uint32_t col = themeDark ? TEXTCOLS[i].dark : TEXTCOLS[i].light;
        if (draw) {
          navAdd(cx - 17, y - 1, 34, 34);
          if (i == textColIdx) { spr.drawCircle(cx, y + 16, 17, C(INK)); spr.drawCircle(cx, y + 16, 16, C(INK)); }
          spr.fillCircle(cx, y + 16, 13, C(col));
        } else if (!hit && hitR(tx, ty, X + i * sw, y, sw, 36)) {
          textColIdx = i; prefs.putUChar("tcol", textColIdx); applyTheme(); hit = true;
        }
      }
      y += 42;
    }
  } else if (setPage == 2) {
    label("Phone link (own Wi-Fi)");
    if (draw) btn(X, y, CW, 32, apOn ? "Hotspot " AP_SSID ": ON" : "Hotspot " AP_SSID ": OFF", apOn);
    else if (!hit && hitR(tx, ty, X, y, CW, 32)) { apOn = !apOn; prefs.putBool("ap", apOn); setupWifi(); hit = true; }
    y += 40;
    if (apOn || staOk()) {
      if (draw) {
        bool wq = qrWifi && apOn;
        String qr = wq ? String("WIFI:T:WPA;S:" AP_SSID ";P:") + apPass + ";;"
                       : String("http://") + (staOk() ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "/";
        spr.fillRect(X, y, 92, 92, C(QRWHITE));
        if (apOn) navAdd(X, y, 92, 92);   // tap the QR: next step
        spr.qrcode(qr.c_str(), X + 2, y + 2, 88, 1);
        int tx0 = X + 100, tw = W - tx0 - 4;
        String L[5];
        if (wq) { L[0] = "1) Join Wi-Fi"; L[1] = AP_SSID; L[2] = "pass " + apPass; L[4] = "tap QR: next"; }
        else if (apOn) { L[0] = "2) Open web"; L[1] = WiFi.softAPIP().toString(); if (staOk()) L[2] = WiFi.localIP().toString(); L[4] = "tap QR: back"; }
        else { L[0] = "Open web"; L[1] = WiFi.localIP().toString(); L[2] = "phone must be"; L[3] = "on same Wi-Fi"; }
        for (int k = 0; k < 5; k++) if (L[k].length()) txt(k ? FS : FB, fitText(k ? FS : FB, L[k], tw), tx0, y + k * 19, k ? SOFT : INK);
      } else if (!hit && hitR(tx, ty, X, y, 96, 92)) { qrWifi = !qrWifi; hit = true; }
      y += 100;
    } else {
      if (draw) txt(FS, fitText(FS, "Off: the phone web page can't open", CW), X, y - 4, SOFT);
      y += 20;
    }
    label("PIN (home Wi-Fi only)");
    if (draw) {
      spr.fillRoundRect(X, y, CW - 76, 32, 8, C(CARD));
      txt(FL, webPin, X + 12, y + 16, INK, D_ML);
      btn(X + CW - 70, y, 70, 32, "New", false);
    } else if (!hit && hitR(tx, ty, X + CW - 70, y, 70, 32)) { newWebPin(); hit = true; }
    y += 40;
    label("Internet: join a Wi-Fi");
    if (draw) btn(X, y, CW, 32, staOn ? "Internet Wi-Fi: ON" : "Internet Wi-Fi: OFF", staOn);
    else if (!hit && hitR(tx, ty, X, y, CW, 32)) { staOn = !staOn; prefs.putBool("sta", staOn); setupWifi(); hit = true; }
    y += 40;
    if (staOn) {
      if (draw) txt(FS, fitText(FS, staOk() ? "Connected: " + WiFi.SSID() : (staSsid.length() ? "Not connected yet" : "No Wi-Fi chosen yet"), CW), X, y - 4, staOk() ? INK : SOFT);
      y += 20;
    }
    navRow("Choose Wi-Fi", staSsid, wifiPageOpen);
  } else {
    auto line = [&](const String& k, const String& v, bool warn) {
      if (draw) {
        spr.fillRoundRect(X, y, CW, 34, 8, C(CARD));
        txt(FS, k, X + 10, y + 17, SOFT, D_ML);
        spr.setFont(FS); int kw = spr.textWidth(k);
        txt(FB, fitText(FB, v, CW - kw - 26), X + CW - 10, y + 17, warn ? 0xD03030 : INK, D_MR);   // red = look at this
      }
      y += 38;
    };
    line("Time", timeApprox ? String("not set") : hhmm(nowT()) + " (" + TIME_SRC_N[timeSrc] + ")", timeApprox);
    if (draw && timeApprox) { txt(FS, fitText(FS, "Open the web page or join Wi-Fi", CW), X, y - 2, SOFT); }
    if (timeApprox) y += 20;
    line("Clock module", clockModuleText(), rtcFound && !rtcTimeOk);
    line("Battery", batPct() < 0 ? String("USB power") : String(batPct()) + "%  (" + String(batV, 2) + " V)", false);
    line("SD card", sdCardText(), false);          // used / size, with decimals (v11.5)
    if (sdOk) line("Files", sdFilesText(), false);  // how many photos and clips
    line("Web page", apOn ? WiFi.softAPIP().toString() : (staOk() ? WiFi.localIP().toString() : String("Wi-Fi off")), false);
    line("Backup to SD", bkErr.length() ? bkErr : backupWhen(), bkErr.length() > 0);
    if (draw) btn(X, y, CW, 34, "Back up now", false);
    else if (!hit && hitR(tx, ty, X, y, CW, 34)) { hit = true; backupNow(); }
    y += 40;
    line("Log space", logFull ? String("FULL!") : String(logPctCache) + "% used" + (logFsOk ? "" : " (old)"), logFull || logPctCache >= 90);
    { int u = unsureLogCount(); if (u > 0) line("Time unsure", String(u) + (u == 1 ? " log" : " logs"), true); }   // made on a guessed time, never put right
    { String why = lastStartWhy(); line("Last start", why, lastStartBad); }
    line("Free memory", memText() + ", low " + String(memLowKB()) + " KB", memLowKB() < 30);   // v11.9: the lowest since the start too
    line("Version", FW_VERSION, false);   // FW_VERSION is set at the top of SomudTick.ino
    if (draw) btn(X, y, CW, 34, "Update firmware", false);
    else if (!hit && hitR(tx, ty, X, y, CW, 34)) { hit = true; askUpdate = true; }   // asks first (a stray tap used to restart it)
    y += 40;
    if (draw) txt(FS, fitText(FS, "For the web flasher", CW), X, y - 2, SOFT);
    y += 20;
  }
  setMax = max(0, y + setScroll - FTR_Y);
  if (hit) dirty = true;
}
void drawSettings() {
  setScroll = constrain(setScroll, 0, setMax);
  int top = setPage ? HDR_H + 34 : HDR_H;
  spr.setClipRect(0, top, W, FTR_Y - top);
  settingsUI(true);
  scrollBar(top, FTR_Y - top, setScroll, setMax);
  spr.clearClipRect();
  if (setPage) drawAppTitle(SET_PAGE_N[setPage]);
  if (askUpdate) drawAskUpdate();
}
