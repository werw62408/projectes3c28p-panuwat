#pragma once
// Touch and buttons: taps, long press, scrolling, touch calibration, BOOT button.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Touch ----------------
void goScreen(Screen s) {
  if (s == S_SET) setPage = 0;   // Settings tab = main settings page
  // Stats: the numbers are kept and counted again only when the logs changed (see statsNeed)
  scr = s;
  dirty = true;
}
void refreshStatsIfVisible() {
  if (scr == S_STATS) dirty = true;   // statsNeed() sees the new logRev and counts again
}
void onTap(int x, int y) {
  if (scr == S_GAME) { gameTap(x, y); return; }
  if (remindBarShown && y >= remindBarY && y < remindBarY + 30) {
    if (!remindBarIsNotice) { remindAct = -1; kpAct = -1; goScreen(S_HOME); return; }   // reminder bar: go to Log
    int n = logNotice();   // notice on Log: time -> Settings > Wi-Fi, space -> About
    goScreen(S_SET); setOpenPage(n == 3 ? 2 : 3); if (n != 3) setAboutPage();
    return;
  }
  // the Home / Uni chip switches on a long press (see onLongPress). Wide Sudoku has no top bar: its board and Back start at y = 3
  if (y < HDR_H && !(scr == S_SUDOKU && land())) return;
  if (scr == S_SUDOKU) { sudokuTap(x, y); return; }
  if (scr == S_KBD) { kbdTap(x, y); return; }
  if (scr == S_KEYPAD) {
    if (y >= FTR_Y) {
      if (x < W / 2) { scr = S_HOME; }
      else { logValue(kpAct, kpVal.toFloat()); scr = S_HOME; }
      dirty = true; return;
    }
    KpGeo g = kpGeo();
    if (x >= g.x0 && y >= g.y0) {
      int c = (x - g.x0) / g.px, r = (y - g.y0) / g.py;
      if (c > 2 || r > 3) return;
      int k = r * 3 + c;
      if (k == 11) { if (kpVal.length()) kpVal.remove(kpVal.length() - 1); }
      else if (k == 9) { if (kpVal.indexOf('.') < 0 && kpVal.length() < 8) kpVal += "."; }
      else if (kpVal.length() < 8) kpVal += KP_KEYS[k];
      dirty = true;
    }
    return;
  }
  if (y >= FTR_Y && scr != S_USB) {   // bottom tabs (the USB drive page has none: it must not be left by a stray tap)
    int x0[N_TABS + 1]; footerTabs(x0);
    int t = N_TABS - 1; for (int i = 0; i < N_TABS; i++) if (x < x0[i + 1]) { t = i; break; }
    const Screen TAB_SCR[N_TABS] = {S_HOME, S_APPS, S_SET};
    goScreen(TAB_SCR[t]);
    return;
  }
  switch (scr) {
    case S_HOME: {
      if (y < HDR_H + 4 + SUM_H - scrollY && x >= W - 76) { goScreen(S_STATS); break; }   // Stats button
      int i = homeTileAt(x, y);
      if (i < 0) return;
      int tx, ty; homeTileRect(i, tx, ty);
      int by = ty + TILE_H - 30;
      if (y >= by) {
        if (x >= tx + 40) logDefault(i);         // [+]
        else undoEvent(i);                       // [-]
      }
      // tapping the rest of the tile does nothing (no accidental logs in a pocket); hold it to type a value
      break;
    }
    case S_STATS: statsTap(x, y); break;
    case S_APPS:
      for (int i = 0; i < N_APPS; i++) {
        int ax, ay, aw, ah; appTileRect(i, ax, ay, aw, ah);
        if (!hitR(x, y, ax, ay, aw, ah)) continue;
        if (i == 0) filesOpen();
        else if (i == 1) { scr = S_AC; dirty = true; }
        else if (i == 2) { scr = S_GAMES; dirty = true; }
        else netOpen();
      }
      break;
    case S_GAMES:
      if (backHit(x, y)) { scr = S_APPS; dirty = true; break; }
      for (int i = 0; i < N_GAMES; i++) {
        int ax, ay, aw, ah; gamesTileRect(i, ax, ay, aw, ah);
        if (!hitR(x, y, ax, ay, aw, ah)) continue;
        if (i == 0) gameOpen(); else if (i == 1) sudokuOpen(); else if (i == 2) sandOpen(); else if (i == 3) gardenOpen();
        else if (i == 4) mazeOpen(); else if (i == 5) blocksOpen(); else gbListOpen();
      }
      break;
    case S_FILES: filesTap(x, y); break;
    case S_AC: acTap(x, y); break;
    case S_NET: netTap(x, y); break;
    case S_WIFI: wifiTap(x, y); break;
    case S_BT: btTap(x, y); break;
    case S_GARDEN: gardenTap(x, y); break;
    case S_GBLIST: gbListTap(x, y); break;
    case S_USB: usbTap(x, y); break;
    case S_SET:
      if (askUpdate) {
        FmBox b = fmAskBox(); int bw = (b.w - 24) / 2, by = b.y + b.h - 42;
        askUpdate = false; dirty = true;
        if (hitR(x, y, b.x + 16 + bw, by, bw, 36)) enterUpdateMode();
        break;
      }
      if (setPage && backHit(x, y)) { setOpenPage(0); break; }
      if (setPage && y < HDR_H + 34) break;
      settingsUI(false, x, y); break;
    default: break;
  }
}
void onLongPress(int x, int y) {
  if (y < HDR_H && x >= hdrChipX0 - 4 && x <= hdrChipX1 + 4 && !(scr == S_SUDOKU && land())) {   // hold the chip: Home <-> Uni
    place = place == 'H' ? 'U' : 'H'; prefs.putChar("place", place); ledFlash(0xFFFFFF, 120); dirty = true; tHandled = true; return;
  }
  if (scr == S_FILES) { filesLongPress(x, y); return; }
  if (scr == S_HOME && y > HDR_H && y < FTR_Y) {
    int i = homeTileAt(x, y);
    int tx, ty; if (i >= 0) homeTileRect(i, tx, ty);
    if (i >= 0 && y < ty + TILE_H - 30) { kpAct = i; kpVal = ""; scr = S_KEYPAD; dirty = true; tHandled = true; }   // not on the buttons
  }
}
void wake() {
  lastTouchMs = millis();
  if (pw != P_ON) {
    powerLow(false);   // full speed, screen chip awake, hotspot back on
    pw = P_ON; lcd.setBrightness(BRIGHT[brightIdx]); dirty = true;
    if (scr == S_SUDOKU && !sdkStartMs) sdkStartMs = millis();   // Sudoku clock runs again
  }
}
int* scrollVar() {
  switch (scr) {
    case S_HOME: return &scrollY;
    case S_STATS: return statView == 0 ? &statScroll : statView == 2 ? &heatScroll : nullptr;
    case S_SET: return &setScroll;
    case S_FILES: return fmUi == FU_PICK ? &pickScroll : (fmUi == FU_LIST ? &filesScroll : nullptr);
    case S_AC: return &acScroll;
    case S_NET: return &netScroll;
    case S_WIFI: return &wifiScroll;
    case S_BT: return &btScroll;
    case S_GBLIST: return &gbListScroll;
    default: return nullptr;
  }
}
void touchTask() {
  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp) > 0;
  if (down && !tDown) {
    if (navShow) { navShow = false; dirty = true; }   // a finger: hide the joystick ring
    tDown = true; tMoved = false; tHandled = false;
    tWakeOnly = (pw == P_OFF);
    wake();
    tX0 = tp.x; tY0 = tp.y; tT0 = millis();
    int* sv = scrollVar(); tScroll0 = sv ? *sv : 0;
  } else if (down && tDown) {
    lastTouchMs = millis();
    if (tWakeOnly) return;
    int dy = tp.y - tY0;
    int* sv = scrollVar();
    if (!tMoved && abs(dy) > 10 && tY0 > HDR_H && tY0 < FTR_Y && sv) tMoved = true;
    if (tMoved && sv) {
      *sv = tScroll0 - dy;
      dirty = true;
    } else if (!tHandled && millis() - tT0 > 650) {
      onLongPress(tX0, tY0);
      tHandled = true;
    }
  } else if (!down && tDown) {
    tDown = false;
    if (!tWakeOnly && !tMoved && !tHandled) onTap(tX0, tY0);
  }
}

// ---------------- Touch calibration ----------------
void loadTouchCal() {
  uint16_t p[8];
  if (prefs.getBytes("tcal", p, sizeof p) == sizeof p) lcd.setTouchCalibrate(p);
}
void calText(const char* l1, const char* l2, const char* l3, const char* l4) {
  lcd.fillScreen(TFT_WHITE);
  lcd.setTextDatum(D_MC);
  lcd.setTextColor(TFT_BLACK);
  int cx = lcd.width() / 2;
  lcd.setFont(FL); lcd.drawString(l1, cx, 110);
  lcd.setFont(FS);
  lcd.drawString(l2, cx, 145); lcd.drawString(l3, cx, 167); lcd.drawString(l4, cx, 189);
}
void runTouchCal() {
  powerLow(false);   // BOOT held while the screen was off
  lcd.setBrightness(BRIGHT[brightIdx]);
  lcd.setRotation(0);   // always calibrate standing up (the result works for every direction)
  while (true) {
    calText("Touch setup", "Tap the middle of each", "black square in the corners.", "4 corners, one at a time.");
    { lgfx::touch_point_t tr; while (lcd.getTouchRaw(&tr)) delay(10); }   // wait for finger up
    uint16_t p[8];
    lcd.calibrateTouch(p, TFT_BLACK, TFT_WHITE, 12);
    lcd.setTouchCalibrate(p);
    calText("Test touch", "A dot should appear", "right under your finger.", "OK? Tap \"Use this\".");
    lcd.fillRoundRect(8, 270, 108, 42, 10, TFT_LIGHTGREY);
    lcd.fillRoundRect(124, 270, 108, 42, 10, TFT_BLACK);
    lcd.setFont(FB);
    lcd.setTextColor(TFT_BLACK); lcd.drawString("Again", 62, 291);
    lcd.setTextColor(TFT_WHITE); lcd.drawString("Use this", 178, 291);
    int choice = 0;
    uint32_t t0 = millis();
    while (!choice) {
      lgfx::touch_point_t tp;
      if (lcd.getTouch(&tp)) {
        t0 = millis();
        if (tp.y >= 270 && tp.x < 120) choice = 1;
        else if (tp.y >= 270 && tp.x >= 124) choice = 2;
        else if (tp.y < 262) lcd.fillCircle(tp.x, tp.y, 3, TFT_BLACK);
      }
      if (millis() - t0 > 60000) choice = 2;   // no touch for 1 minute = keep it
      delay(10);
    }
    { lgfx::touch_point_t tr; while (lcd.getTouch(&tr)) delay(10); }
    if (choice == 2) { prefs.putBytes("tcal", p, sizeof p); break; }
  }
  lcd.setRotation(rot);
  tDown = false; tHandled = true;
  lastTouchMs = millis(); pw = P_ON;
  dirty = true;
}

// BOOT button: short press = log the first activity / wake screen, hold 3 s = touch setup
void buttonTask() {
  static bool prev = true, longDone = false; static uint32_t tChg = 0, bT0 = 0;
  bool v = digitalRead(PIN_BOOT);
  if (v != prev && millis() - tChg > 40) {
    tChg = millis(); prev = v;
    if (!v) { bT0 = millis(); longDone = false; }
    else if (!longDone) {
      bool wasOff = pw == P_OFF;
      wake();
      if (!wasOff && !acts.empty() && scr == S_HOME) logDefault(0);   // only on the Log screen (it used to log from any screen, unseen)
    }
  }
  if (!prev && !longDone && millis() - bT0 > 3000) { longDone = true; runTouchCal(); }
}
