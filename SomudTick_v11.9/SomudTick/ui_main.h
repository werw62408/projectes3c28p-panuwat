#pragma once
// Screen drawing: reminder / notice bars and render() for every screen.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).


void drawAskUpdate() {   // Settings > About > Update firmware: ask first
  FmBox b = fmAskBox(); drawWindow(b);
  navModal(b.x + 8 + (b.w - 24) / 4, b.y + b.h - 25);   // Back = Cancel
  txt(FB, fitText(FB, "Update firmware?", b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
  txt(FS, fitText(FS, "The screen goes dark.", b.w - 16), b.x + b.w / 2, b.y + 44, INK, D_MC);
  txt(FS, fitText(FS, "To cancel: switch off/on", b.w - 16), b.x + b.w / 2, b.y + 64, SOFT, D_MC);   // RESET is inside the case
  int bw = (b.w - 24) / 2;
  btn(b.x + 8, b.y + b.h - 42, bw, 34, "Cancel", false);
  btn(b.x + 16 + bw, b.y + b.h - 42, bw, 34, "Restart", true);
  navModalEnd();
}

// reminder: a dark bar just above the tabs, "Time for Water  >". Tap it = go to Log.
bool remindBarShown = false; int remindBarY = 0;
bool remindBarIsNotice = false;   // the bar on screen is a Log notice (not a reminder): a tap opens where to fix it
// v11.7.2: the reminder bar has its own [+1] button (logs at once, you stay on the page). After it: "Water +1" for a moment.
int remindPlusX = 0;              // left edge of the [+1] button in the bar
int remindDoneAct = -1; uint32_t remindDoneMs = 0; bool remindBarIsDone = false;
String remindDoneId;              // v11.8: kept by id (the list can change on the web page while "saved" shows)
const uint32_t REMIND_DONE_MS = 1500, REMIND_DONE_DECK_MS = 600;   // Deck: the bar covers the bottom keys, so only a short moment
uint32_t remindDoneFor() { return scr == S_DECK ? REMIND_DONE_DECK_MS : REMIND_DONE_MS; }
int actIndexOf(const String& id) { for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == id) return i; return -1; }
String remindPlusLabel(int i) { return "+" + fmtNum(isUnitAct(acts[i]) ? acts[i].step : 1); }   // the same as the tile's [+]
// Log page only: important notices in the same place (tap = go where you can fix it)
int logNotice() {   // 0 none, 1 cannot save (full), 2 almost full, 3 time not set
  if (logFull) return 1;
  if (logPctCache >= 90) return 2;
  if (timeApprox) return 3;
  return 0;
}
void drawLogNotice() {
  int n = logNotice();
  if (!n || scr != S_HOME) return;   // (a pending reminder has no bar on Log, so the notice is shown)
  const char* T[] = {"", "Log space FULL: logs not saved", "Log space almost full", "Time not set: tap here"};
  remindBarY = FTR_Y - 34;
  spr.fillRoundRect(6, remindBarY, W - 12, 30, 10, C(n <= 2 ? 0xD03030 : INK));
  navAdd(6, remindBarY, W - 12, 30);
  txt(FB, fitText(FB, T[n], W - 50), 14, remindBarY + 15, n <= 2 ? 0xFFFFFF : ONINK, D_ML);
  txt(FB, ">", W - 18, remindBarY + 15, n <= 2 ? 0xFFFFFF : ONINK, D_MC);
  remindBarShown = true; remindBarIsNotice = true;
}
void drawRemindBar() {
  remindBarShown = false; remindBarIsNotice = false;
  if (burstNoticeMs && millis() - burstNoticeMs < 4000 && scr != S_DECK) {   // v11.9: logs refused (too many in a minute)
    int y = FTR_Y - 34;
    if (themeHud) hudShape(6, y, W - 12, 30, 8, HUDW, HUDW); else spr.fillRoundRect(6, y, W - 12, 30, 10, C(0xD03030));
    txt(FB, fitText(FB, hudUp("Too fast: " + burstName + " not saved"), W - 24), 14, y + 15, themeHud ? 0x000000 : 0xFFFFFF, D_ML);
    dirty = true; return;
  }
  drawLogNotice();
  remindBarIsDone = false;
  if (remindDoneId.length()) remindDoneAct = actIndexOf(remindDoneId);
  if (remindDoneAct >= 0 && remindDoneAct < (int)acts.size() && millis() - remindDoneMs < remindDoneFor() && scr != S_HOME && scr != S_KEYPAD && scr != S_KBD && scr != S_SUDOKU && scr != S_USB) {
    remindBarY = (scr == S_DECK || scr == S_ANTS) ? H - 38 : FTR_Y - 34;   // "Water +1": the log was saved (a tap here does nothing)
    uint32_t bg = scr == S_DECK ? DN_CYAN : INK, fg = scr == S_DECK ? DN_BG : ONINK;
    if (themeHud) { bg = INK; fg = ONINK; hudShape(6, remindBarY, W - 12, 30, 8, bg, bg); } else
    spr.fillRoundRect(6, remindBarY, W - 12, 30, 10, C(bg));
    spr.fillCircle(20, remindBarY + 15, 5, C(acts[remindDoneAct].color));
    txt(FB, fitText(FB, hudUp(acts[remindDoneAct].name + " " + remindPlusLabel(remindDoneAct) + "  saved"), W - 50), 32, remindBarY + 15, fg, D_ML);
    remindBarShown = true; remindBarIsDone = true; dirty = true;   // (redrawn until it goes away)
    return;
  }
  if (remindAct < 0 || remindAct >= (int)acts.size() || scr == S_KEYPAD || scr == S_KBD || scr == S_SUDOKU || scr == S_USB) return;
  if (scr == S_HOME) return;   // on Log you see the tile already
  remindBarY = (scr == S_DECK || scr == S_ANTS) ? H - 38 : FTR_Y - 34;   // Deck and Ants have no bottom bar
  uint32_t bg = scr == S_DECK ? DN_CYAN : INK, fg = scr == S_DECK ? DN_BG : ONINK;   // Deck: in its neon colours
  if (themeHud) { bg = INK; fg = ONINK; hudShape(6, remindBarY, W - 12, 30, 8, bg, bg); } else
  spr.fillRoundRect(6, remindBarY, W - 12, 30, 10, C(bg));
  // [+1] on the right: logs now. The rest of the bar: go to Log.
  String pl = remindPlusLabel(remindAct);
  spr.setFont(FB); int pw2 = max(44, min(70, (int)spr.textWidth(pl) + 16));
  remindPlusX = W - 9 - pw2;
  navAdd(6, remindBarY, remindPlusX - 8, 30, NK_OVER);
  navAdd(remindPlusX, remindBarY + 3, pw2, 24, NK_OVER);
  if (themeHud) {   // v11.8: "> TIME FOR WATER   [+1]"
    hudShape(remindPlusX, remindBarY + 3, pw2, 24, 6, fg, fg);
    txt(FB, fitText(FB, pl, pw2 - 6), remindPlusX + pw2 / 2, remindBarY + 15, bg, D_MC);
    spr.fillTriangle(15, remindBarY + 8, 15, remindBarY + 22, 24, remindBarY + 15, C(fg));
    txt(FB, fitText(FB, hudUp("Time for " + acts[remindAct].name), remindPlusX - 38), 30, remindBarY + 15, fg, D_ML);
    remindBarShown = true; return;
  }
  spr.fillRoundRect(remindPlusX, remindBarY + 3, pw2, 24, 8, C(fg));
  txt(FB, fitText(FB, pl, pw2 - 6), remindPlusX + pw2 / 2, remindBarY + 15, bg, D_MC);
  spr.fillCircle(20, remindBarY + 15, 5, C(acts[remindAct].color));
  txt(FB, fitText(FB, "Time for " + acts[remindAct].name, remindPlusX - 38), 32, remindBarY + 15, fg, D_ML);
  remindBarShown = true;
}
// v11.9: moving to another page wipes the new page in (about 0.1 s): from the right when you go to a tab on the
// right, from the left when you go back, from the top when an app opens. Scrolling and updates are shown at once.
// (animOn: SomudTick.ino)
uint32_t wipeCount = 0;      // (the simulator tests read it)
int pageKeyNow() { return (int)scr * 8 + (scr == S_SET ? setPage : 0); }
void pushFrame() {
  static int lastKey = -1; static int lastTab = -1;
  int key = pageKeyNow(), tab = tabOf(scr);
  bool change = lastKey >= 0 && key != lastKey && pw == P_ON;
  int dir = tab > lastTab ? 1 : tab < lastTab ? -1 : (scr == S_APPS || scr == S_SET || scr == S_HOME || scr == S_GAMES ? -2 : 2);   // +-1 sideways, +-2 up / down
  lastKey = key; lastTab = tab;
  if (!change || !animOn) { spr.pushSprite(0, 0); return; }
  wipeCount++;
  const int N = 6;
  uint16_t lineC = C(themeHud ? HUDB : INK);
  for (int k = 1; k <= N; k++) {
    int32_t x0 = 0, y0 = 0, w = W, h = H;
    if (dir == 1) { x0 = W - W * k / N; w = W * k / N; } else if (dir == -1) { w = W * k / N; }
    else if (dir == 2) { h = H * k / N; } else { y0 = H - H * k / N; h = H * k / N; }
    lcd.setClipRect(x0, y0, w, h);
    spr.pushSprite(0, 0);
    lcd.clearClipRect();
    if (k < N) {   // the bright leading edge
      if (dir == 1) lcd.drawFastVLine(x0, 0, H, lineC); else if (dir == -1) lcd.drawFastVLine(w - 1, 0, H, lineC);
      else if (dir == 2) lcd.drawFastHLine(0, h - 1, W, lineC); else lcd.drawFastHLine(0, y0, W, lineC);
      delay(8);
    }
  }
}
void render() {
  if (scr == S_SAND) { sandDraw(); dirty = false; return; }   // Sand & Water draws the whole screen itself
  navBegin();   // the buttons drawn below are written down again for the joystick
  spr.fillScreen(C(PAPER));
  switch (scr) {
    case S_HOME: drawHome(); break;
    case S_STATS: drawStats(); break;
    case S_APPS: drawApps(); break;
    case S_GAMES: drawGames(); break;
    case S_SET: drawSettings(); break;
    case S_KEYPAD: drawKeypad(); break;
    case S_FILES: drawFiles(); break;
    case S_AC: drawAC(); break;
    case S_SUDOKU: drawSudoku(); break;
    case S_NET: drawNet(); break;
    case S_WIFI: drawWifi(); break;
    case S_KBD: drawKbd(); break;
    case S_BT: drawBt(); break;
    case S_ANTS: drawAnts(); break;
    case S_USB: drawUsb(); break;
    case S_GBLIST: drawGbList(); break;
    case S_DECK: drawDeck(); break;
    default: break;
  }
  if (scr == S_DECK) drawDeckHeader();   // Deck: its own neon top bar
  else if (!(scr == S_SUDOKU && land()) && scr != S_ANTS) drawHeader();   // wide Sudoku uses the full height; Ants: its own thin bar (v11.9)
  if (scr == S_KEYPAD) drawKeypadFooter(); else if (scr != S_SUDOKU && scr != S_KBD && scr != S_USB && scr != S_DECK && scr != S_ANTS) drawFooter();   // Sudoku uses the whole screen; USB drive: stay on this page; Deck: Exit is top left
  drawRemindBar();
  navDraw();   // the joystick ring (only after the stick was used)
  pushFrame();
  dirty = false;
}
