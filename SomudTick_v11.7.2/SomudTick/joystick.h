#pragma once
// Joystick (KY-023 style): VRX -> IO2, VRY -> IO3, SW -> IO14, +5V -> 3.3V (!), GND -> GND
// Raw readings, the saved direction setup, a direction that turns with the screen, and the events the menus use.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

#define JOY_X  2
#define JOY_Y  3
#define JOY_SW 14

int joyCX = 2048, joyCY = 2048;   // where the stick rests
uint8_t joyMap = 0;   // bit0 = swap X/Y, bit1 = flip left-right, bit2 = flip up-down (set by "Joystick direction")
uint8_t joyRot = 0;   // the screen direction when "Joystick direction" was done (v11.3: directions follow the screen)
bool joyOk = false;   // a stick was found at start (a free pin gives noise, so nothing is read without one)
bool joyNavOn = true; // Settings: the stick moves around the menus (the games always use it)
bool swPrev = true; uint32_t swT = 0;

int joyRawX() { return analogRead(JOY_X); }
int joyRawY() { return analogRead(JOY_Y); }
void joyCenter() {   // remember where the stick rests
  long sx = 0, sy = 0;
  for (int i = 0; i < 16; i++) { sx += joyRawX(); sy += joyRawY(); delay(2); }
  joyCX = sx / 16; joyCY = sy / 16;
}
float joyAxis(int raw, int c) {   // -1 .. +1 with a small dead zone
  float v = raw >= c ? (raw - c) / float(max(1, 4095 - c)) : (raw - c) / float(max(1, c));
  if (fabsf(v) < 0.12f) return 0;
  return constrain(v, -1.0f, 1.0f);
}
// The stick as screen directions: x right, y down, each -1..1.
// The stick is fixed to the case, the picture turns with Settings > Screen direction, so the saved setup is
// turned by the difference (each step is a quarter turn: (x, y) -> (y, -x), the same as the touch screen does).
void joyVec(float& jx, float& jy) {
  jx = joyAxis(joyRawX(), joyCX); jy = joyAxis(joyRawY(), joyCY);
  if (joyMap & 1) { float t = jx; jx = jy; jy = t; }
  if (joyMap & 2) jx = -jx;
  if (joyMap & 4) jy = -jy;
  for (int k = (rot - joyRot) & 3; k > 0; --k) { float t = jx; jx = jy; jy = -t; }
}
bool joyPressed() {   // true once per press (games)
  bool v = digitalRead(JOY_SW);
  bool fired = false;
  if (v != swPrev && millis() - swT > 40) { swT = millis(); swPrev = v; if (!v) fired = true; }
  return fired;
}
bool joyDown() { return digitalRead(JOY_SW) == LOW; }
// The stick's button in the games (v11.4): 1 = a short press (when let go), 2 = held 1 s (once) = leave the game.
// Every game uses the same: hold the press 1 s to get out, with the stick only.
struct JoyBtn { bool down = false, longDone = false; uint32_t t = 0; };
void joyBtnReset(JoyBtn& b) { b.down = joyDown(); b.longDone = b.down; b.t = millis(); }   // a press still down from the menu does nothing
int joyBtnRead(JoyBtn& b) {
  bool d = joyDown(); uint32_t now = millis();
  if (d && !b.down) { b.down = true; b.t = now; b.longDone = false; return 0; }
  if (d && !b.longDone && now - b.t >= 1000) { b.longDone = true; return 2; }
  if (!d && b.down) { b.down = false; bool tap = !b.longDone && now - b.t >= 30; b.longDone = false; return tap ? 1 : 0; }
  return 0;
}
// Is a joystick plugged in? A free (unplugged) pin gives jumpy readings,
// a real stick at rest gives a steady middle value.
bool joyDetect() {
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  joyMap = prefs.getUChar("joyMap", 0);
  joyRot = prefs.isKey("joyRot") ? prefs.getUChar("joyRot", 0) & 3 : rot;   // set up before v11.3: most likely on today's screen direction
  int mnx = 4095, mxx = 0, mny = 4095, mxy = 0; long sx = 0, sy = 0;
  for (int i = 0; i < 24; i++) {
    int x = joyRawX(), y = joyRawY();
    mnx = min(mnx, x); mxx = max(mxx, x); mny = min(mny, y); mxy = max(mxy, y); sx += x; sy += y;
    delay(1);
  }
  int cx = sx / 24, cy = sy / 24;
  bool ok = mxx - mnx < 250 && mxy - mny < 250 && cx > 900 && cx < 3200 && cy > 900 && cy < 3200;
  if (ok) { joyCX = cx; joyCY = cy; joyOk = true; }
  swPrev = digitalRead(JOY_SW);
  return ok;
}

// ---------------- events for the menus ----------------
// Read every 20 ms. A direction counts after 3 same readings in a row (a noisy wire can't move anything).
//   push          -> one step (JE_LEFT/RIGHT/UP/DOWN)
//   hold up/down/right -> keeps stepping (after 0.38 s, then every 0.14 s)
//   hold left 1 s -> JE_BACK (left does not repeat, so holding it only goes back)
//   press + let go before 0.65 s -> JE_PRESS,  hold the press 0.65 s -> JE_HOLD
enum JoyEv : uint8_t { JE_NONE, JE_LEFT, JE_RIGHT, JE_UP, JE_DOWN, JE_PRESS, JE_HOLD, JE_BACK };
struct JoyNavState { uint32_t readT = 0, dirT = 0, nextT = 0, swDownT = 0; int seenDir = 0, seenN = 0, curDir = 0; bool backDone = false, swDown = false, holdDone = false; } joyNs;
void joyNavReset() { joyNs = JoyNavState(); joyNs.swDown = joyDown(); joyNs.holdDone = joyNs.swDown; }   // a press that was already down does nothing
JoyEv joyNavRead() {
  if (!joyOk) return JE_NONE;
  uint32_t now = millis();
  if (now - joyNs.readT < 20) return JE_NONE;
  joyNs.readT = now;
  // button
  bool down = joyDown();
  if (down && !joyNs.swDown) { joyNs.swDown = true; joyNs.swDownT = now; joyNs.holdDone = false; }
  else if (down && joyNs.swDown && !joyNs.holdDone && now - joyNs.swDownT >= 650) { joyNs.holdDone = true; return JE_HOLD; }
  else if (!down && joyNs.swDown) { joyNs.swDown = false; bool tap = !joyNs.holdDone && now - joyNs.swDownT >= 30; joyNs.holdDone = false; if (tap) return JE_PRESS; }
  // direction
  float jx, jy; joyVec(jx, jy);
  int dir = 0;   // 1 left, 2 right, 3 up, 4 down
  if (fabsf(jx) > 0.55f || fabsf(jy) > 0.55f) dir = fabsf(jx) > fabsf(jy) ? (jx < 0 ? 1 : 2) : (jy < 0 ? 3 : 4);
  else if (fabsf(jx) < 0.35f && fabsf(jy) < 0.35f) dir = 0;
  else dir = joyNs.curDir;   // between the two limits: keep what it was (no flicker at the edge)
  if (dir == joyNs.seenDir) joyNs.seenN++; else { joyNs.seenDir = dir; joyNs.seenN = 1; }
  if (joyNs.seenN < 3) return JE_NONE;
  if (dir != joyNs.curDir) {
    joyNs.curDir = dir; joyNs.dirT = now; joyNs.nextT = now + 380; joyNs.backDone = false;
    return dir ? (JoyEv)dir : JE_NONE;   // first step right away
  }
  if (!dir) return JE_NONE;
  if (dir == 1) {   // left: no repeat; held 1 s = Back
    if (!joyNs.backDone && now - joyNs.dirT >= 1000) { joyNs.backDone = true; return JE_BACK; }
    return JE_NONE;
  }
  if ((int32_t)(now - joyNs.nextT) >= 0) { joyNs.nextT = now + 140; return (JoyEv)dir; }
  return JE_NONE;
}
