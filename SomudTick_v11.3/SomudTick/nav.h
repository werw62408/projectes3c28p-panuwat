#pragma once
// Joystick control of every screen (v11.3). One system for all screens, no second "joystick mode":
//  - while a screen is drawn, every button / tile / row it draws is written down (navAdd, called by btn() and friends)
//  - pushing the stick moves an orange ring to the nearest button in that direction (like a TV menu)
//  - pressing the stick = tapping that button (the same onTap() the finger uses), holding it = a long press
//  - holding the stick LEFT for 1 s = Back
// The ring shows only after the stick is used; touching the screen hides it again.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// (NavKind: NK_BTN normal, NK_BACK the Back button, NK_FOOTER a bottom tab - in SomudTick.ino)
struct NavT {
  int16_t x, y, w, h;      // the whole button
  int16_t vx, vy, vw, vh;  // the part you can see (lists are cut at the edge of their area)
  int16_t hx, hy;          // where a long press goes (default: the middle)
  int16_t clipY, clipH;    // the area it was drawn in (a scrolling list, or the whole screen)
  uint8_t kind;
};
std::vector<NavT> navT;
bool navShow = false;           // the ring is on screen
bool navLocked = false;         // a window is open and finished: nothing behind it counts
int16_t navFX = 0, navFY = 0, navFW = 0, navFH = 0;   // the button the ring is on (found again after every drawing)
int16_t navBackX = -1, navBackY = -1;                 // where "Back" taps when there is no Back button (windows)
const uint32_t NAV_RING = NAV_RING_C;                 // orange: seen on every theme and tile colour

bool navScreenOn() {   // screens that use the stick themselves (games) are left alone
  return joyOk && joyNavOn && scr != S_GAME && scr != S_SAND && scr != S_SUDOKU && scr != S_MAZE && scr != S_BLOCKS;
}
void navBegin() { navT.clear(); navLocked = false; navBackX = navBackY = -1; }
void navAdd(int x, int y, int w, int h, uint8_t kind, int hx, int hy) {
  if (navLocked || w <= 0 || h <= 0 || !navScreenOn()) return;
  int32_t cx, cy, cw, ch; spr.getClipRect(&cx, &cy, &cw, &ch);
  int vx0 = max(x, (int)cx), vy0 = max(y, (int)cy), vx1 = min(x + w, (int)(cx + cw)), vy1 = min(y + h, (int)(cy + ch));
  vx0 = max(vx0, 0); vy0 = max(vy0, 0); vx1 = min(vx1, W); vy1 = min(vy1, H);
  if (vx1 - vx0 < 4 || vy1 - vy0 < 4) return;   // not on screen
  NavT t{(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)vx0, (int16_t)vy0, (int16_t)(vx1 - vx0), (int16_t)(vy1 - vy0),
         (int16_t)(hx < 0 ? x + w / 2 : hx), (int16_t)(hy < 0 ? y + h / 2 : hy), (int16_t)cy, (int16_t)ch, kind};
  if (kind == NK_BACK) { for (auto& o : navT) if (o.kind == NK_BACK) o.kind = NK_BTN; }   // only one Back
  navT.push_back(t);
}
// a window (menu, "are you sure?") is drawn on top: forget what is behind it; call navModalEnd() when its buttons are drawn
void navModal(int backX, int backY) { navT.clear(); navLocked = false; navBackX = backX; navBackY = backY; }
void navModalEnd() { navLocked = true; }

int navFind() {   // the button the ring is on (same place, or the nearest one)
  int best = -1; long bd = 0x7FFFFFFFL;
  for (size_t i = 0; i < navT.size(); i++) {
    auto& t = navT[i];
    if (t.x == navFX && t.y == navFY && t.w == navFW && t.h == navFH) return i;
    long dx = (t.x + t.w / 2) - (navFX + navFW / 2), dy = (t.y + t.h / 2) - (navFY + navFH / 2);
    long d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
void navFocus(const NavT& t) { navFX = t.x; navFY = t.y; navFW = t.w; navFH = t.h; }
int navFirst() {   // where the ring starts on a screen: the first button that is not Back or a tab
  for (size_t i = 0; i < navT.size(); i++) if (navT[i].kind == NK_BTN) return i;
  return navT.empty() ? -1 : 0;
}
int navPage = -1;   // the screen (and page) the ring was on: a new screen starts at its first button
int navPageKey() { return (int)scr * 64 + setPage * 8 + (int)fmUi; }
void navCheckPage() {
  int k = navPageKey();
  if (k != navPage) { navPage = k; int f = navFirst(); if (f >= 0) navFocus(navT[f]); }
}
// the ring, drawn last (before the picture goes to the screen)
void navDraw() {
  if (!navShow || !navScreenOn() || navT.empty()) return;
  navCheckPage();
  int i = navFind(); if (i < 0) return;
  auto& t = navT[i]; navFocus(t);
  spr.clearClipRect();
  for (int k = 0; k < 3; k++) spr.drawRoundRect(t.vx - 2 - k + 1, t.vy - 2 - k + 1, t.vw + 2 + 2 * k, t.vh + 2 + 2 * k, 8 + k, C(NAV_RING));
}
// the nearest button in a direction (1 left, 2 right, 3 up, 4 down); sameArea = only in the area of `from`
int navPick(const NavT& from, int dir, bool sameArea) {
  int fcx = from.vx + from.vw / 2, fcy = from.vy + from.vh / 2;
  int best = -1; long bs = 0x7FFFFFFFL;
  for (size_t i = 0; i < navT.size(); i++) {
    auto& t = navT[i];
    if (t.x == from.x && t.y == from.y && t.w == from.w && t.h == from.h) continue;
    if (sameArea && (t.clipY != from.clipY || t.clipH != from.clipH)) continue;
    int tcx = t.vx + t.vw / 2, tcy = t.vy + t.vh / 2;
    long along, side; bool overlap;
    if (dir == 1 || dir == 2) {
      along = dir == 1 ? fcx - tcx : tcx - fcx;
      if (dir == 1 ? t.vx + t.vw > from.vx + 2 : t.vx < from.vx + from.vw - 2) continue;   // must be past the edge
      overlap = t.vy < from.vy + from.vh && t.vy + t.vh > from.vy;
      side = overlap ? 0 : labs(tcy - fcy);
    } else {
      along = dir == 3 ? fcy - tcy : tcy - fcy;
      if (dir == 3 ? t.vy + t.vh > from.vy + 2 : t.vy < from.vy + from.vh - 2) continue;
      overlap = t.vx < from.vx + from.vw && t.vx + t.vw > from.vx;
      side = overlap ? 0 : labs(tcx - fcx);
    }
    if (along <= 0) continue;
    long score = along + side * 3 + (overlap ? 0 : 40);
    if (score < bs) { bs = score; best = i; }
  }
  return best;
}
int* scrollVar();
bool navScroll(int dy) {   // scroll the list under the ring; true if it moved
  int* sv = scrollVar(); if (!sv) return false;
  int old = *sv; *sv = max(0, *sv + dy);
  dirty = true; render();   // drawing clamps the scroll to its limits and writes the buttons down again
  if (*sv == old) return false;
  navFY -= (*sv - old);   // the ring's button moved with the list
  return true;
}
void navMove(int dir) {
  int i = navFind(); if (i < 0) return;
  NavT cur = navT[i];
  bool inList = cur.clipH < H && scrollVar();   // a scrolling list (not the title bar / tabs)
  int j = navPick(cur, dir, false);
  // up / down in a list: scroll the list before jumping out of it (to the tabs or the title bar)
  if (inList && (dir == 3 || dir == 4) && (j < 0 || navT[j].clipY != cur.clipY || navT[j].clipH != cur.clipH)) {
    if (navScroll(dir == 4 ? 60 : -60)) {
      int k = navFind(); if (k < 0) return;
      cur = navT[k];
      int j2 = navPick(cur, dir, true);
      if (j2 >= 0) navFocus(navT[j2]); else navFocus(cur);
      dirty = true; return;
    }
  }
  if (j < 0) return;
  NavT t = navT[j];
  navFocus(t);
  if (t.clipH < H && (t.vh < t.h) && scrollVar()) {   // only half on screen: scroll it into view
    int hidden = t.y < t.vy ? -(t.vy - t.y) - 4 : (t.y + t.h) - (t.vy + t.vh) + 4;
    navScroll(hidden);
  }
  dirty = true;
}
void onTap(int x, int y); void onLongPress(int x, int y);
void navPress() {
  int i = navFind(); if (i < 0) return;
  auto t = navT[i];
  onTap(t.vx + t.vw / 2, t.vy + t.vh / 2);
  dirty = true;
}
void navHold() {
  int i = navFind(); if (i < 0) return;
  auto t = navT[i];
  tHandled = false;
  onLongPress(t.hx, t.hy);
  dirty = true;
}
void navBack() {
  for (auto& t : navT) if (t.kind == NK_BACK) { onTap(t.vx + t.vw / 2, t.vy + t.vh / 2); dirty = true; return; }
  if (navBackX >= 0) { onTap(navBackX, navBackY); dirty = true; }
}
void navTask() {   // from loop()
  if (!navScreenOn() || tDown) { if (joyOk) joyNavRead(); return; }   // (keep reading so nothing is saved up)
  JoyEv e = joyNavRead();
  if (e == JE_NONE) return;
  if (pw == P_OFF) {   // in a pocket the stick gets pushed: only a press wakes the screen, and does nothing else
    if (e == JE_PRESS || e == JE_HOLD) { wake(); joyNavReset(); }
    return;
  }
  wake();
  if (!navShow) {   // the first use only shows the ring (no surprise taps)
    navShow = true;
    navPage = -1; navCheckPage();   // start at the first button of this screen
    dirty = true;
    if (e != JE_BACK) return;
  }
  navCheckPage();
  if (navFind() < 0) return;
  switch (e) {
    case JE_LEFT: navMove(1); break;
    case JE_RIGHT: navMove(2); break;
    case JE_UP: navMove(3); break;
    case JE_DOWN: navMove(4); break;
    case JE_PRESS: navPress(); break;
    case JE_HOLD: navHold(); break;
    case JE_BACK: navBack(); break;
    default: break;
  }
}
// for the screens that wait in their own loop (photo viewer, "are you sure?", after a video)
JoyEv navWaitEvent() { return (joyOk && joyNavOn) ? joyNavRead() : JE_NONE; }
