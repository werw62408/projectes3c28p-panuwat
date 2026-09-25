#pragma once
// Joystick control of every screen (v11.3, scrolling redone in v11.4). One system for all screens, no second "joystick mode":
//  - while a screen is drawn, every button / tile / row it draws is written down (navAdd, called by btn() and friends)
//  - pushing the stick moves an orange ring to the nearest button in that direction (like a TV menu)
//  - pressing the stick = tapping that button (the same onTap() the finger uses), holding it = a long press
//  - holding the stick LEFT for 1 s = Back
// Up / down on a screen that scrolls: what is not seen yet is scrolled in first, the ring leaves the scrolling
// area (to the tabs at the top or the bottom) only at its ends. Where the area has no button to go to
// (weather, a story, oil prices, About) the ring goes around the whole area and up / down scroll it.
// The ring shows only after the stick is used; touching the screen hides it again (it comes back where it was).
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
bool navOnArea = false;         // the ring is around the scrolling area (not on a button)
int16_t navBackX = -1, navBackY = -1;                 // where "Back" taps when there is no Back button (windows)
const uint32_t NAV_RING = NAV_RING_C;                 // orange: seen on every theme and tile colour
// the scrolling area of this screen (set by scrollBar() while drawing; navArH = 0: none)
int16_t navArY = 0, navArH = 0; int navArPos = 0, navArMax = 0;

bool navScreenOn() {   // screens that use the stick themselves (games) are left alone
  return joyOk && joyNavOn && scr != S_GAME && scr != S_SAND && scr != S_SUDOKU && scr != S_MAZE && scr != S_BLOCKS;
}
void navBegin() { navT.clear(); navLocked = false; navBackX = navBackY = -1; navArH = 0; }
void navArea(int top, int h, int pos, int maxPos) {
  if (navLocked) return;
  navArY = top; navArH = h; navArPos = pos; navArMax = max(0, maxPos);
}
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
void navModal(int backX, int backY) { navT.clear(); navLocked = false; navBackX = backX; navBackY = backY; navArH = 0; navOnArea = false; }
void navModalEnd() { navLocked = true; }

// ---------------- the scrolling area ----------------
bool navHasArea() { return navArH > 20 && scrollVar(); }
bool navInArea(const NavT& t) {   // a button inside the scrolling area (it moves when the area scrolls)
  return navHasArea() && t.clipH < H && abs(t.clipY - navArY) <= 6;
}
bool navFull(const NavT& t) { return t.vh >= t.h - 1; }   // the whole button is on screen
int navStep() { return max(40, navArH * 2 / 5); }         // one push scrolls this much

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
void navFocus(const NavT& t) { navFX = t.x; navFY = t.y; navFW = t.w; navFH = t.h; navOnArea = false; }
void navFocusArea() { navOnArea = true; }
int navFirst() {   // where the ring starts on a screen: the first button that is not Back or a tab
  for (size_t i = 0; i < navT.size(); i++) if (navT[i].kind == NK_BTN) return i;
  return navT.empty() ? -1 : 0;
}
int navPage = -1;   // the screen (and page) the ring was on: a new screen starts at its first button
int navPageKey() { return (int)scr * 64 + setPage * 8 + (int)fmUi; }
void navCheckPage() {
  int k = navPageKey();
  if (k != navPage) {
    navPage = k; navOnArea = false;
    int f = navFirst();
    if (f >= 0) navFocus(navT[f]); else if (navHasArea()) navFocusArea();
  }
  if (navOnArea && !navHasArea()) {   // the area went away (a window opened, the page changed its look)
    navOnArea = false; int f = navFirst(); if (f >= 0) navFocus(navT[f]);
  }
}
// is there anything for the ring at all (a button, or an area to scroll)?
bool navAny() { return !navT.empty() || (navOnArea && navHasArea()); }
// the ring, drawn last (before the picture goes to the screen)
void navDraw() {
  if (!navShow || !navScreenOn()) return;
  navCheckPage();
  spr.clearClipRect();
  if (navOnArea) {
    if (!navHasArea()) return;
    for (int k = 0; k < 2; k++) spr.drawRoundRect(1 + k, navArY + k, W - 2 - 2 * k, navArH - 2 * k, 8, C(NAV_RING));
    return;
  }
  if (navT.empty()) return;
  int i = navFind(); if (i < 0) return;
  auto& t = navT[i]; navFocus(t);
  for (int k = 0; k < 3; k++) spr.drawRoundRect(t.vx - 2 - k + 1, t.vy - 2 - k + 1, t.vw + 2 + 2 * k, t.vh + 2 + 2 * k, 8 + k, C(NAV_RING));
}
// the nearest button in a direction (1 left, 2 right, 3 up, 4 down)
// only: 0 any button, 1 only buttons in the scrolling area, 2 only buttons outside it
int navPick(const NavT& from, int dir, int only) {
  int fcx = from.vx + from.vw / 2, fcy = from.vy + from.vh / 2;
  int best = -1; long bs = 0x7FFFFFFFL;
  for (size_t i = 0; i < navT.size(); i++) {
    auto& t = navT[i];
    if (t.x == from.x && t.y == from.y && t.w == from.w && t.h == from.h) continue;
    if (only == 1 && !navInArea(t)) continue;
    if (only == 2 && navInArea(t)) continue;
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
NavT navAreaBox() {   // the scrolling area as a "button" (to find what is above / below it)
  NavT a{}; a.x = a.vx = 0; a.w = a.vw = W; a.y = a.vy = navArY; a.h = a.vh = navArH; a.clipY = 0; a.clipH = H; return a;
}
bool navScroll(int dy) {   // scroll the area; true if it moved. The ring's button moves with it.
  int* sv = scrollVar(); if (!sv) return false;
  int old = *sv; *sv = max(0, *sv + dy);
  dirty = true; render();   // drawing clamps the scroll to its limits and writes the buttons down again
  if (*sv == old) return false;
  navFY -= (*sv - old);
  return true;
}
// make the whole button seen (a tall one: at least its top)
void navReveal(const NavT& t) {
  if (!navInArea(t) || navFull(t)) return;
  int top = navArY, bot = navArY + navArH;
  int d = 0;
  if (t.y < top) d = t.y - top - 4;                          // cut at the top: scroll up
  else if (t.y + t.h > bot) d = min(t.y + t.h - bot + 4, t.y - top - 4);   // cut at the bottom: scroll down (keep its top in view)
  if (d) navScroll(d);
}
// the area buttons that are fully seen, on one side of the middle of the area (for coming back from "area" mode)
int navAreaPick(int dir) {
  int mid = navArY + navArH / 2, best = -1;
  for (size_t i = 0; i < navT.size(); i++) {
    auto& t = navT[i];
    if (!navInArea(t) || !navFull(t)) continue;
    int c = t.y + t.h / 2;
    if (dir == 4 ? c <= mid : c >= mid) continue;
    if (best < 0) { best = i; continue; }
    auto& b = navT[best];
    if (dir == 4 ? (t.y < b.y || (t.y == b.y && t.x < b.x)) : (t.y > b.y || (t.y == b.y && t.x < b.x))) best = i;
  }
  return best;
}
// coming into the area from above (dir 4): its top button fully seen; from below (dir 3): its bottom one
int navEdgePick(int dir, const NavT& from) {
  int best = -1, fcx = from.vx + from.vw / 2;
  for (size_t i = 0; i < navT.size(); i++) {
    auto& t = navT[i];
    if (!navInArea(t) || !navFull(t)) continue;
    if (best < 0) { best = i; continue; }
    auto& b = navT[best];
    int ey = dir == 4 ? t.y : -(t.y + t.h), eb = dir == 4 ? b.y : -(b.y + b.h);
    if (ey < eb - 4 || (abs(ey - eb) <= 4 && abs(t.x + t.w / 2 - fcx) < abs(b.x + b.w / 2 - fcx))) best = i;
  }
  return best;
}
bool navAreaCan(int dir) { return dir == 4 ? navArPos < navArMax : navArPos > 0; }
void navMoveArea(int dir) {   // the ring is around the area
  if (dir != 3 && dir != 4) return;   // left / right: nothing here (hold left = Back still works)
  int k = navAreaPick(dir);
  if (k < 0 && navAreaCan(dir)) { navScroll(dir == 4 ? navStep() : -navStep()); k = navAreaPick(dir); if (k < 0) { dirty = true; return; } }
  if (k >= 0) { navFocus(navT[k]); dirty = true; return; }
  int j = navPick(navAreaBox(), dir, 2);   // at the end: out to the tabs / title bar
  if (j >= 0) { navFocus(navT[j]); dirty = true; }
}
void navMove(int dir) {
  if (navOnArea) { navMoveArea(dir); return; }
  int i = navFind(); if (i < 0) { if (navHasArea()) { navFocusArea(); dirty = true; } return; }
  NavT cur = navT[i];
  bool vert = dir == 3 || dir == 4;
  if (vert && navHasArea()) {
    bool curIn = navInArea(cur);
    int j = navPick(cur, dir, 0);
    bool jIn = j >= 0 && navInArea(navT[j]);
    // is the area in the way (the ring is in it, or it lies between the ring and where it would go)?
    bool above = cur.vy + cur.vh <= navArY + 2, below = cur.vy >= navArY + navArH - 2;
    bool crosses = curIn || (dir == 4 && above) || (dir == 3 && below);
    if (crosses && !jIn && navArMax > 0) {
      if (curIn) {
        // inside the area and nothing more to go to on screen: scroll the next part in
        if (navAreaCan(dir)) {
          int* sv = scrollVar(); int before = *sv;
          navScroll(dir == 4 ? navStep() : -navStep());
          int moved = *sv - before;
          cur.y -= moved; cur.vy -= moved;   // where the ring's button is now (maybe off screen): the next one is past it
          int j2 = navPick(cur, dir, 1);
          if (j2 >= 0) { navFocus(navT[j2]); navReveal(navT[j2]); dirty = true; return; }
          if (cur.y >= navArY && cur.y + cur.h <= navArY + navArH) { navFocus(cur); dirty = true; return; }   // still fully seen: stay on it
          navFocusArea(); dirty = true; return;   // it scrolled away: the ring goes around the area
        }
        // at the end of the area: out of it (below: the tabs, above: the title bar)
      } else {
        // coming into the area from the tabs / the title bar: the first button fully seen there, else the area itself
        int k = navEdgePick(dir, cur);
        if (k >= 0) { navFocus(navT[k]); dirty = true; return; }
        navFocusArea(); dirty = true; return;
      }
    }
    if (j < 0) return;
    navFocus(navT[j]);
    navReveal(navT[j]);
    dirty = true;
    return;
  }
  int j = navPick(cur, dir, 0);
  if (j < 0) return;
  navFocus(navT[j]);
  navReveal(navT[j]);
  dirty = true;
}
void onTap(int x, int y); void onLongPress(int x, int y);
void navPress() {
  if (navOnArea) return;   // the area itself is only for reading
  int i = navFind(); if (i < 0) return;
  auto t = navT[i];
  onTap(t.vx + t.vw / 2, t.vy + t.vh / 2);
  dirty = true;
}
void navHold() {
  if (navOnArea) return;
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
  if (!navShow) {   // the first use only shows the ring (no surprise taps), where it was last time on this screen
    navShow = true;
    navCheckPage();
    dirty = true;
    if (e != JE_BACK) return;
  }
  navCheckPage();
  if (!navAny()) { if (e == JE_BACK) navBack(); return; }
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
