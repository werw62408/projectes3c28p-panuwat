#pragma once
// Apps menu and Games menu.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Apps menu ----------------
// big tiles. "Games" opens a second page with the games.
enum AppIcon { IC_FILES, IC_AC, IC_GAMES, IC_NET, IC_SWIM, IC_SUDOKU, IC_SAND, IC_ANTS, IC_MAZE, IC_BLOCKS, IC_GB, IC_DECK, IC_REMOTE };
const int N_APPS = 6;   // v13: + Remotes (learned IR keys)
const char* APP_NAMES[N_APPS] = {"Files", "AC Remote", "Games", "Internet", "Deck", "Remotes"};
const char* APP_SUBS[N_APPS] = {"Photos, clips", "Air con", "7 games", "Weather, news", "Hot keys", "TV, fan, light"};   // (v11.8: "Shortcut keys" was cut)
const AppIcon APP_ICONS[N_APPS] = {IC_FILES, IC_AC, IC_GAMES, IC_NET, IC_DECK, IC_REMOTE};
// v13: the "Time for ..." bar sits at the bottom of the page: the tiles make room for it (it covered half of the last row)
int remindRoom() { return (remindAct >= 0 && remindAct < (int)acts.size()) ? 36 : 0; }
void appTileRect(int i, int& x, int& y, int& w, int& h) {   // v13: 6 apps, tall 2 x 3, wide 3 x 2
  int cols = land() ? 3 : 2, rows = (N_APPS + cols - 1) / cols, gap = 6;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (CONT_H - 12 - remindRoom() - gap * (rows - 1)) / rows;
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
    case IC_SWIM: {   // Pixel Swim (v11.9): a ball in dotted water, the dots pushed aside
      for (int yy = -12; yy <= 12; yy += 5) for (int xx = -16; xx <= 16; xx += 5) {
        float d = sqrtf(xx * xx + yy * yy); if (d < 9) continue;
        spr.fillRect(cx + xx + (d < 14 ? xx / 5 : 0), cy + yy + (d < 14 ? yy / 5 : 0), 2, 2, C(d < 14 ? c : blend(CARD, c, 0.5f)));
      }
      spr.fillCircle(cx, cy, 6, C(c)); spr.fillCircle(cx + 2, cy - 1, 2, C(CARD));
      break; }
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
    case IC_GB: {   // a Game Boy: body, green screen, cross and two buttons (v11.7)
      spr.drawRoundRect(cx - 11, cy - 16, 22, 32, 3, C(c)); spr.drawRoundRect(cx - 10, cy - 15, 20, 30, 3, C(c));
      spr.fillRect(cx - 7, cy - 12, 14, 11, C(c));
      spr.fillRect(cx - 7, cy + 5, 6, 2, C(c)); spr.fillRect(cx - 5, cy + 3, 2, 6, C(c));
      spr.fillCircle(cx + 4, cy + 7, 2, C(c)); spr.fillCircle(cx + 7, cy + 4, 2, C(c));
      break; }
    case IC_DECK:   // 3 x 2 keys
      for (int k = 0; k < 6; k++) spr.fillRoundRect(cx - 16 + (k % 3) * 11, cy - 12 + (k / 3) * 13, 9, 11, 2, C(c));
      break;
    case IC_REMOTE: {   // v13: a remote with keys, and waves from its top
      spr.fillRoundRect(cx - 7, cy - 8, 14, 26, 4, C(c));
      spr.fillCircle(cx, cy - 3, 2, C(CARD));
      for (int k = 0; k < 4; k++) spr.fillRect(cx - 4 + (k % 2) * 6, cy + 3 + (k / 2) * 6, 3, 3, C(CARD));
      for (int r = 0; r < 2; r++) spr.drawArc(cx, cy - 10, 6 + r * 5, 5 + r * 5, 225, 315, C(c));
      break; }
    case IC_BLOCKS:  // falling blocks
      for (int k = 0; k < 3; k++) spr.fillRect(cx - 15 + k * 10, cy + 4, 9, 9, C(c));
      spr.fillRect(cx - 5, cy - 6, 9, 9, C(c));
      spr.fillRect(cx + 5, cy - 16, 9, 9, C(blend(CARD, c, 0.5f))); spr.fillRect(cx + 5, cy - 6, 9, 9, C(blend(CARD, c, 0.5f)));
      break;
    case IC_ANTS: {  // v11.8: an ant (head, body, legs, feelers)
      spr.fillCircle(cx - 9, cy + 2, 6, C(c)); spr.fillCircle(cx + 1, cy + 1, 4, C(c)); spr.fillCircle(cx + 9, cy - 1, 4, C(c));
      for (int k = -1; k <= 1; k++) { spr.drawLine(cx + 1 + k * 3, cy + 3, cx - 3 + k * 5, cy + 11, C(c)); spr.drawLine(cx + 1 + k * 3, cy - 1, cx - 3 + k * 5, cy - 9, C(c)); }
      spr.drawLine(cx + 11, cy - 4, cx + 15, cy - 11, C(c)); spr.drawLine(cx + 12, cy - 3, cx + 18, cy - 7, C(c));
      break; }
  }
}
// a narrow tile (wide screen): a short name instead of a cut one ("Tilt Ma.." -> "Maze")
String shortName(const String& n) {
  const char* L[][2] = {{"AC Remote", "AC"}, {"Tilt Maze", "Maze"}, {"Game Boy", "GB"}, {"Internet", "Net"}, {"Pixel Swim", "Swim"}};
  for (auto& p : L) if (n == p[0]) return p[1];
  return n;
}
void drawAppTile(int x, int y, int w, int h, int icon, const String& name0, const String& sub0, int num = 0) {
  navAdd(x, y, w, h);
  String name = name0;
  { spr.setFont(pickFont(FS, name)); if ((int)spr.textWidth(hudUp(name)) > w - 6) name = shortName(name); }
  String sub = sub0;   // too long: only the part before the comma ("Weather, news" -> "Weather"), then the first word, else none
  { spr.setFont(pickFont(FS, sub));   // (v13: "New g.." / "Pour &.." / "Game .." were cut on the narrow wide-screen tiles)
    if ((int)spr.textWidth(hudUp(sub)) > w - 6 && sub.indexOf(',') > 0) sub = sub.substring(0, sub.indexOf(','));
    if ((int)spr.textWidth(hudUp(sub)) > w - 6 && sub.indexOf(' ') > 0) sub = sub.substring(0, sub.indexOf(' '));
    if ((int)spr.textWidth(hudUp(sub)) > w - 6) sub = ""; }
  if (themeHud) {   // v11.8: cut corners, a number in the corner, capitals
    hudShape(x, y, w, h, 9, CARD, LINE);
    bool tiny = h < 70;
    if (num && !tiny) { char b[4]; snprintf(b, sizeof b, "%02d", num); txt(&hS, b, x + 8, y + 5, SOFT); }
    for (int k = 0; k < 3; k++) spr.drawFastHLine(x + w - 22 + k * 6, y + 4, 3, C(LINE));
    drawAppIcon(icon, x + w / 2, y + h / 2 - (tiny ? 10 : 14), INK);
    String nm = hudUp(name);
    spr.setFont(pickFont(FB, nm)); const lgfx::IFont* nf = (int)spr.textWidth(nm) <= w - 8 ? FB : FS;
    txt(nf, fitText(nf, nm, w - 8), x + w / 2, y + h / 2 + (tiny ? 16 : 14), HUDB, D_MC);
    if (!tiny && sub.length()) txt(FS, fitText(FS, hudUp(sub), w - 6), x + w / 2, y + h / 2 + 31, SOFT, D_MC);
    return;
  }
  spr.fillRoundRect(x, y, w, h, 12, C(CARD));
  spr.drawRoundRect(x, y, w, h, 12, C(LINE));
  bool tiny = h < 70;
  drawAppIcon(icon, x + w / 2, y + h / 2 - (tiny ? 10 : 16), INK);
  spr.setFont(FB); const lgfx::IFont* nf = (int)spr.textWidth(name) <= w - 8 ? FB : FS;   // narrow tile: smaller letters, not a cut name
  txt(nf, fitText(nf, name, w - 8), x + w / 2, y + h / 2 + (tiny ? 16 : 12), INK, D_MC);
  if (!tiny && sub.length()) txt(FS, fitText(FS, sub, w - 6), x + w / 2, y + h / 2 + 31, SOFT, D_MC);
}
void drawApps() {
  for (int i = 0; i < N_APPS; i++) { int x, y, w, h; appTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, APP_ICONS[i], APP_NAMES[i], APP_SUBS[i], i + 1); }
}
// Games page: 4 tiles (2 x 2) under a title bar
const int N_GAMES = 7;
void gamesTileRect(int i, int& x, int& y, int& w, int& h) {   // tall: 2 across, 4 down. wide: 4 across, 2 down (v11.7: 7 games)
  int top = HDR_H + 38, gap = 6, cols = land() ? 4 : 2, rows = (N_GAMES + cols - 1) / cols;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (FTR_Y - top - 6 - remindRoom() - gap * (rows - 1)) / rows;
  x = 6 + (i % cols) * (w + gap); y = top + (i / cols) * (h + gap);
}
String gameSub(int i);   // (in the game files) a short line: best score / saved game / how the garden is
void drawGames() {
  drawAppTitle("Games");
  const char* n[N_GAMES] = {"Pixel Swim", "Sudoku", "Sand", "Ants", "Tilt Maze", "Blocks", "Game Boy"};   // Pixel Swim again (v11.9; Dragon in v11.6-11.8), Ants was Garden (v11.8)
  const AppIcon ic[N_GAMES] = {IC_SWIM, IC_SUDOKU, IC_SAND, IC_ANTS, IC_MAZE, IC_BLOCKS, IC_GB};
  for (int i = 0; i < N_GAMES; i++) { int x, y, w, h; gamesTileRect(i, x, y, w, h); drawAppTile(x, y, w, h, ic[i], n[i], gameSub(i), i + 1); }
}
