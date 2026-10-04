#pragma once
// Joystick: VRX -> IO2, VRY -> IO3, +5V -> 3.3V (!), GND -> GND. The press:
//   - with the button board (PCF8574): the stick press comes in on P6 (K), and IO14 is the IR receiver KY-022 (v13)
//   - without it (the old wiring): SW -> IO14 as before
// Raw readings, the saved direction setup and range, a direction that turns with the screen, and the events the menus use.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

#define JOY_X  2
#define JOY_Y  3
#define JOY_SW 14

int joyCX = 2048, joyCY = 2048;   // where the stick rests
// v13: how far the stick really goes (Settings > Joystick direction, last step). A small stick like the Joy-Con one
// may not reach 0 / 4095: its edge then counts as a full push. 0 / 4095 = not set (the whole range).
int joyLoX = 0, joyHiX = 4095, joyLoY = 0, joyHiY = 4095;
uint8_t joyMap = 0;   // bit0 = swap X/Y, bit1 = flip left-right, bit2 = flip up-down (set by "Joystick direction")
uint8_t joyRot = 0;   // the screen direction when "Joystick direction" was done (v11.3: directions follow the screen)
bool joyOk = false;   // a stick was found at start (a free pin gives noise, so nothing is read without one)
bool joyNavOn = true; // Settings: the stick moves around the menus (the games always use it)
bool joySwIo14 = true;   // v13: the stick press is on IO14 (old wiring). false when the button board is there: IO14 = IR receiver
bool swPrev = true; uint32_t swT = 0;

int joyRawX() { return analogRead(JOY_X); }
int joyRawY() { return analogRead(JOY_Y); }
void joyCenter() {   // remember where the stick rests
  long sx = 0, sy = 0;
  for (int i = 0; i < 16; i++) { sx += joyRawX(); sy += joyRawY(); delay(2); }
  joyCX = sx / 16; joyCY = sy / 16;
}
void joyRangeLoad() {
  joyLoX = prefs.getUShort("jLoX", 0); joyHiX = prefs.getUShort("jHiX", 4095);
  joyLoY = prefs.getUShort("jLoY", 0); joyHiY = prefs.getUShort("jHiY", 4095);
  if (joyHiX - joyLoX < 600) { joyLoX = 0; joyHiX = 4095; }   // (a broken setting: the whole range)
  if (joyHiY - joyLoY < 600) { joyLoY = 0; joyHiY = 4095; }
}
float joyAxis(int raw, int c, int& lo, int& hi) {   // -1 .. +1 with a small dead zone
  if (raw > hi) hi = raw; if (raw < lo) lo = raw;      // went further than when it was set up: that is the edge now
  float v = raw >= c ? (raw - c) / float(max(300, hi - c)) : (raw - c) / float(max(300, c - lo));
  if (fabsf(v) < 0.12f) return 0;
  return constrain(v, -1.0f, 1.0f);
}
// The stick as screen directions: x right, y down, each -1..1.
// The stick is fixed to the case, the picture turns with Settings > Screen direction, so the saved setup is
// turned by the difference (each step is a quarter turn: (x, y) -> (y, -x), the same as the touch screen does).
void joyVec(float& jx, float& jy) {
  jx = joyAxis(joyRawX(), joyCX, joyLoX, joyHiX); jy = joyAxis(joyRawY(), joyCY, joyLoY, joyHiY);
  if (joyMap & 1) { float t = jx; jx = jy; jy = t; }
  if (joyMap & 2) jx = -jx;
  if (joyMap & 4) jy = -jy;
  for (int k = (rot - joyRot) & 3; k > 0; --k) { float t = jx; jx = jy; jy = -t; }
}
// ---------------- v12: button board (NA011: A B C D E F + the stick press K) through a PCF8574 ----------------
// The PCF8574 sits on the I2C wires with the touch chip and the clock module. Wiring: P0 A, P1 B, P2 C, P3 D, P4 E, P5 F, P6 K.
// A pressed button pulls its pin to GND (reads 0). Without the board nothing changes (padOk = false).
enum PadBtn : uint8_t { PB_A, PB_B, PB_C, PB_D, PB_E, PB_F, PB_K, PB_N };
uint8_t padAddr = 0;
uint8_t padBits = 0;          // buttons down now (bit = PadBtn), after the debounce
uint8_t padLastRaw = 0, padFails = 0; uint32_t padReadT = 0;
#ifdef SIM
bool i2cReadRaw(uint8_t a, uint8_t* d, uint8_t n) { return simI2cRead(a, d, n); }          // (the simulator's extra parts, v13)
bool i2cWriteRaw(uint8_t a, const uint8_t* d, uint8_t n) { return simI2cWrite(a, d, n); }
#else
bool i2cReadRaw(uint8_t addr, uint8_t* d, uint8_t n) { return lgfx::i2c::transactionRead(RTC_I2C_PORT, addr, d, n, 400000).has_value(); }
bool i2cWriteRaw(uint8_t addr, const uint8_t* d, uint8_t n) { return lgfx::i2c::transactionWrite(RTC_I2C_PORT, addr, d, n, 400000).has_value(); }
#endif
bool padBegin() {
  // PCF8574 0x20-0x27; PCF8574A 0x38-0x3F, but 0x38 is the touch chip and 0x3C / 0x3D are where small screens live:
  // those are never asked (v13: 0x3C was asked when the small screen did not answer at start = it was taken for the buttons)
  static const uint8_t A[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x39, 0x3A, 0x3B, 0x3E, 0x3F};
  padOk = false;
  for (uint8_t a : A) {
    uint8_t v, ff = 0xFF;
    if (i2cReadRaw(a, &v, 1) && i2cWriteRaw(a, &ff, 1)) { padAddr = a; padOk = true; break; }   // all pins high = inputs
  }
  padBits = padLastRaw = 0;
  return padOk;
}
void padPoll() {   // read at most every 8 ms; a button counts after 2 same readings (a bounce can't press twice)
  if (!padOk) return;
  uint32_t now = millis();
  if (now - padReadT < 8) return;
  padReadT = now;
  uint8_t v;
  if (!i2cReadRaw(padAddr, &v, 1)) { if (++padFails >= 20) padBits = padLastRaw = 0; return; }   // unplugged: nothing held down
  padFails = 0;
  uint8_t raw = (uint8_t)(~v) & 0x7F;
  if (raw == padLastRaw) padBits = raw;
  padLastRaw = raw;
}
bool padDown(uint8_t b) { padPoll(); return padBits & (1 << b); }

// the old wiring: IO14 low = pressed. Read 4 times over 1 ms: an IR receiver on IO14 blinks (it never stays low that
// long between its pulses), a pressed button stays low. The answer is kept 10 ms (the Game Boy asks very often).
bool joySwLow() {
  static uint32_t t = 0; static bool v = false;
  uint32_t now = millis();
  if (t && now - t < 10) return v;
  t = now ? now : 1; v = false;
  for (int i = 0; i < 4; i++) { if (digitalRead(JOY_SW) != LOW) return false; if (i < 3) delayMicroseconds(300); }
  v = true;
  return true;
}
bool joyDown() { return (joySwIo14 && joySwLow()) || padDown(PB_K); }
// v13: in the games A works like the stick press (the button board's "OK" button)
bool gameDown() { return joyDown() || padDown(PB_A); }
bool gameBtnOk() { return joyOk || padOk; }   // a press can come from somewhere
bool padAnyDown() { padPoll(); return padOk && (padBits & 0x7F); }   // any button of the board held (games: wakes the screen)
bool joyPressed() {   // true once per press (games)
  bool v = !gameDown();
  bool fired = false;
  if (v != swPrev && millis() - swT > 40) { swT = millis(); swPrev = v; if (!v) fired = true; }
  return fired;
}
// The stick's button in the games (v11.4): 1 = a short press (when let go), 2 = held 1 s (once) = leave the game.
// Every game uses the same: hold the press 1 s to get out, with the stick only (v13: or with A).
struct JoyBtn { bool down = false, longDone = false; uint32_t t = 0; };
void joyBtnReset(JoyBtn& b) { b.down = gameDown(); b.longDone = b.down; b.t = millis(); }   // a press still down from the menu does nothing
int joyBtnRead(JoyBtn& b) {
  bool d = gameDown(); uint32_t now = millis();
  if (d && !b.down) { b.down = true; b.t = now; b.longDone = false; return 0; }
  if (d && !b.longDone && now - b.t >= 1000) { b.longDone = true; return 2; }
  if (!d && b.down) { b.down = false; bool tap = !b.longDone && now - b.t >= 30; b.longDone = false; return tap ? 1 : 0; }
  return 0;
}
// Is a joystick plugged in? A free (unplugged) pin gives jumpy readings,
// a real stick at rest gives a steady middle value.
bool joyDetect() {
  if (joySwIo14) pinMode(JOY_SW, INPUT_PULLUP);   // (with the button board IO14 belongs to the IR receiver)
  analogReadResolution(12);
  joyMap = prefs.getUChar("joyMap", 0);
  joyRot = prefs.isKey("joyRot") ? prefs.getUChar("joyRot", 0) & 3 : rot;   // set up before v11.3: most likely on today's screen direction
  joyRangeLoad();
  int mnx = 4095, mxx = 0, mny = 4095, mxy = 0; long sx = 0, sy = 0;
  for (int i = 0; i < 24; i++) {
    int x = joyRawX(), y = joyRawY();
    mnx = min(mnx, x); mxx = max(mxx, x); mny = min(mny, y); mxy = max(mxy, y); sx += x; sy += y;
    delay(1);
  }
  int cx = sx / 24, cy = sy / 24;
  bool ok = mxx - mnx < 250 && mxy - mny < 250 && cx > 900 && cx < 3200 && cy > 900 && cy < 3200;
  if (ok) { joyCX = cx; joyCY = cy; joyOk = true; }
  swPrev = !joyDown();
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
