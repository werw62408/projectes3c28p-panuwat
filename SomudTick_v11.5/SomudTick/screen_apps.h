#pragma once
// Apps menu and Games menu.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Apps menu ----------------
// 4 big tiles. "Games" opens a second page with 4 games.
enum AppIcon { IC_FILES, IC_AC, IC_GAMES, IC_NET, IC_SWIM, IC_SUDOKU, IC_SAND, IC_GARDEN, IC_MAZE, IC_BLOCKS };
const int N_APPS = 4;
const char* APP_NAMES[N_APPS] = {"Files", "AC Remote", "Games", "Internet"};
const char* APP_SUBS[N_APPS] = {"Photos, clips", "Air con", "6 games", "Weather, news"};
const AppIcon APP_ICONS[N_APPS] = {IC_FILES, IC_AC, IC_GAMES, IC_NET};
void appTileRect(int i, int& x, int& y, int& w, int& h) {
  int cols = 2, rows = 2, gap = 6;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (CONT_H - 12 - gap * (rows - 1)) / rows;
  x = 6 + (i % cols) * (w + gap); y = HDR_H + 6 + (i / cols) * (h + gap);
}
void drawAppIcon(int ic, int cx, int cy, uint32_t c) {
  switch (ic) {
    case IC_FILES: spr.fillRoundRect(cx - 16, cy - 10, 14, 6, 2, C(c)); spr.fillRoundRect(cx - 16, cy - 6, 32, 20, 3, C(c)); break;
    case IC_AC: for (int k = 0; k < 3; k++) { float a = k * PI / 3;
              wideLine(cx - 13 * cosf(a), cy - 13 * sinf(a), cx + 13 * cosf(a), cy + 13 * sinf(a), 2.5f, C(c)); } break;
    case IC_GAMES:   // game pad
      spr.fillRoundRect(cx - 18, cy - 9, 36, 20, 9, C(c));
      spr.fillRect(cx - 12, cy - 1, 9, 3, C(CARD)); spr.fillRect(cx - 9, cy - 4, 3, 9, C(CARD));
      spr.fillCircle(cx + 8, cy - 2, 2, C(CARD)); spr.fillCircle(cx + 12, cy + 3, 2, C(CARD)); break;
    case IC_NET: spr.drawCircle(cx, cy, 13, C(c)); spr.drawCircle(cx, cy, 12, C(c)); spr.drawEllipse(cx, cy, 5, 13, C(c));
            spr.drawFastHLine(cx - 13, cy, 26, C(c)); spr.drawFastHLine(cx - 11, cy - 6, 22, C(c)); spr.drawFastHLine(cx - 11, cy + 6, 22, C(c)); break;
    case IC_SWIM: spr.fillCircle(cx + 8, cy, 7, C(c)); spr.fillCircle(cx - 4, cy + 3, 5, C(blend(CARD, c, 0.6f)));
            spr.fillCircle(cx - 13, cy + 5, 4, C(blend(CARD, c, 0.35f))); break;
    case IC_SUDOKU: {   // 3x3 board, small digits sit inside their boxes
      const int cs = 10, x0 = cx - 15, y0 = cy - 15;
      for (int k = 0; k <= 3; k++) { spr.drawFastHLine(x0, y0 + k * cs, 3 * cs + 1, C(c)); spr.drawFastVLine(x0 + k * cs, y0, 3 * cs + 1, C(c)); }
      spr.drawRect(x0 - 1, y0 - 1, 3 * cs + 3, 3 * cs + 3, C(c));
      spr.setFont(&fonts::Font0); spr.setTextSize(1); spr.setTextColor(C(c)); spr.setTextDatum(D_MC);
      const char* d[3] = {"5", "3", "8"}; const int px[3] = {0, 2, 1}, py[3] = {0, 1, 2};
      for (int k = 0; k < 3; k++) spr.drawString(d[k], x0 + px[k] * cs + cs / 2 + 1, y0 + py[k] * cs + cs / 2 + 1);
      break; }
    case IC_SAND:    // a small pile with grains falling on it
      spr.fillTriangle(cx - 16, cy + 12, cx + 16, cy + 12, cx, cy - 2, C(c));
      for (int k = 0; k < 4; k++) spr.fillRect(cx - 2 + (k & 1) * 4, cy - 16 + k * 4, 3, 3, C(c));
      break;
    case IC_MAZE: {  // a small maze with a ball
      spr.drawRect(cx - 15, cy - 13, 30, 26, C(c)); spr.drawRect(cx - 14, cy - 12, 28, 24, C(c));
      spr.fillRect(cx - 7, cy - 13, 3, 16, C(c)); spr.fillRect(cx + 4, cy - 3, 3, 16, C(c));
      spr.fillCircle(cx - 10, cy + 7, 3, C(c)); spr.fillCircle(cx + 10, cy - 7, 3, C(blend(CARD, c, 0.5f)));
      break; }
    case IC_BLOCKS:  // falling blocks
      for (int k = 0; k < 3; k++) spr.fillRect(cx - 15 + k * 10, cy + 4, 9, 9, C(c));
      spr.fillRect(cx - 5, cy - 6, 9, 9, C(c));
      spr.fillRect(cx + 5, cy - 16, 9, 9, C(blend(CARD, c, 0.5f))); spr.fillRect(cx + 5, cy - 6, 9, 9, C(blend(CARD, c, 0.5f)));
      break;
    case IC_GARDEN:  // a little tree on the ground
      spr.fillRect(cx - 2, cy - 2, 4, 14, C(c));
      spr.fillCircle(cx, cy - 8, 9, C(c)); spr.fillCircle(cx - 8, cy - 2, 6, C(c)); spr.fillCircle(cx + 8, cy - 2, 6, C(c));
      spr.fillRect(cx - 16, cy + 12, 32, 3, C(c));
      break;
  }
}
void drawAppTile(int x, int y, int w, int h, int icon, const String& name, const String& sub) {
  navAdd(x, y, w, h);
  spr.fillRoundRect(x, y, w, h, 12, C(CARD));
  spr.drawRoundRect(x, y, w, h, 12, C(LINE));
  bool tiny = h < 70;
  drawAppIcon(icon, x + w / 2, y + h / 2 - (tiny ? 10 : 16), INK);
  spr.setFont(FB); const lgfx::IFont* nf = (int)spr.textWidth(name) <= w - 8 ? FB : FS;   // narrow tile: smaller letters, not a cut name
  txt(nf, fitText(nf, name, w - 8), x + w / 2, y + h / 2 + (tiny ? 16 : 12), INK, D_MC);
  if (!tiny && sub.length()) txt(FS, fitText(FS, sub, w - 8), x + w / 2, y + h / 2 + 31, SOFT, D_MC);
}
void drawApps() {
  for (int i = 0; i < N_APPS; i++) { int x, y, w, h; appTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, APP_ICONS[i], APP_NAMES[i], APP_SUBS[i]); }
}
// Games page: 4 tiles (2 x 2) under a title bar
const int N_GAMES = 6;
void gamesTileRect(int i, int& x, int& y, int& w, int& h) {   // tall: 2 across, 3 down. wide: 3 across, 2 down
  int top = HDR_H + 38, gap = 6, cols = land() ? 3 : 2, rows = (N_GAMES + cols - 1) / cols;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (FTR_Y - top - 6 - gap * (rows - 1)) / rows;
  x = 6 + (i % cols) * (w + gap); y = top + (i / cols) * (h + gap);
}
String gameSub(int i);   // (in the game files) a short line: best score / saved game / how the garden is
void drawGames() {
  drawAppTitle("Games");
  const char* n[N_GAMES] = {"Pixel Swim", "Sudoku", "Sand", "Garden", "Tilt Maze", "Blocks"};
  const AppIcon ic[N_GAMES] = {IC_SWIM, IC_SUDOKU, IC_SAND, IC_GARDEN, IC_MAZE, IC_BLOCKS};
  for (int i = 0; i < N_GAMES; i++) { int x, y, w, h; gamesTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, ic[i], n[i], gameSub(i)); }
}
