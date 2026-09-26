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
  drawLogNotice();
  if (remindAct < 0 || remindAct >= (int)acts.size() || scr == S_KEYPAD || scr == S_KBD || scr == S_SUDOKU || scr == S_USB) return;
  if (scr == S_HOME) return;   // on Log you see the tile already
  remindBarY = FTR_Y - 34;
  spr.fillRoundRect(6, remindBarY, W - 12, 30, 10, C(INK));
  navAdd(6, remindBarY, W - 12, 30);
  spr.fillCircle(20, remindBarY + 15, 5, C(acts[remindAct].color));
  txt(FB, fitText(FB, "Time for " + acts[remindAct].name, W - 60), 32, remindBarY + 15, ONINK, D_ML);
  txt(FB, ">", W - 18, remindBarY + 15, ONINK, D_MC);
  remindBarShown = true;
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
    case S_GARDEN: drawGarden(); break;
    case S_USB: drawUsb(); break;
    case S_GBLIST: drawGbList(); break;
    default: break;
  }
  if (!(scr == S_SUDOKU && land())) drawHeader();   // wide Sudoku uses the full height
  if (scr == S_KEYPAD) drawKeypadFooter(); else if (scr != S_SUDOKU && scr != S_KBD && scr != S_USB) drawFooter();   // Sudoku uses the whole screen; USB drive: stay on this page
  drawRemindBar();
  navDraw();   // the joystick ring (only after the stick was used)
  spr.pushSprite(0, 0);
  dirty = false;
}
