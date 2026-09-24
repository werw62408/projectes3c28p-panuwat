#pragma once
// ============================================================================
//  AC Remote app: controls a Panasonic air conditioner with an IR LED (KY-005).
//  KY-005 wiring: S -> IO21, middle pin -> 3.3V, "-" -> GND.
//  Point the IR LED at the air con (about 1-3 m).
//  Included from SomudTick.ino.
// ============================================================================
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Panasonic.h>

#define PIN_IR 21
IRPanasonicAc acIr(PIN_IR);

struct AcState { bool power; uint8_t temp, mode, fan, swing, model; bool quiet; };
AcState acS = {false, 26, 1, 0, 0, 0, false};
const char* AC_MODE_N[] = {"Auto", "Cool", "Dry", "Fan"};
const uint8_t AC_MODE_V[] = {kPanasonicAcAuto, kPanasonicAcCool, kPanasonicAcDry, kPanasonicAcFan};
const char* AC_FAN_N[] = {"Auto", "1", "2", "3", "4", "5"};
const uint8_t AC_FAN_V[] = {kPanasonicAcFanAuto, kPanasonicAcFanMin, kPanasonicAcFanLow, kPanasonicAcFanMed, kPanasonicAcFanHigh, kPanasonicAcFanMax};
const char* AC_SWING_N[] = {"Auto", "1", "2", "3", "4", "5"};
const uint8_t AC_SWING_V[] = {kPanasonicAcSwingVAuto, kPanasonicAcSwingVHighest, kPanasonicAcSwingVHigh, kPanasonicAcSwingVMiddle, kPanasonicAcSwingVLow, kPanasonicAcSwingVLowest};
const char* AC_MODEL_N[] = {"DKE", "JKE", "NKE", "LKE", "CKP", "RKR"};
const panasonic_ac_remote_model_t AC_MODEL_V[] = {kPanasonicDke, kPanasonicJke, kPanasonicNke, kPanasonicLke, kPanasonicCkp, kPanasonicRkr};
const int AC_NMODELS = 6;
uint32_t acSentMs = 0;

void acLoad() {
  acS.power = prefs.getBool("acP", false);
  acS.temp = constrain(prefs.getUChar("acT", 26), kPanasonicAcMinTemp, kPanasonicAcMaxTemp);
  acS.mode = prefs.getUChar("acM", 1) % 4;
  acS.fan = prefs.getUChar("acF", 0) % 6;
  acS.swing = prefs.getUChar("acS", 0) % 6;
  acS.model = prefs.getUChar("acMo", 0) % AC_NMODELS;
  acS.quiet = prefs.getBool("acQ", false);
  acIr.begin();
}
void acSave() {
  prefs.putBool("acP", acS.power); prefs.putUChar("acT", acS.temp); prefs.putUChar("acM", acS.mode);
  prefs.putUChar("acF", acS.fan); prefs.putUChar("acS", acS.swing); prefs.putUChar("acMo", acS.model); prefs.putBool("acQ", acS.quiet);
}
// Send the whole setting (like the real remote does every time you press a button)
void acSend() {
  acIr.stateReset();
  acIr.setModel(AC_MODEL_V[acS.model]);
  acIr.setMode(AC_MODE_V[acS.mode]);
  acIr.setTemp(acS.temp);
  acIr.setFan(AC_FAN_V[acS.fan]);
  acIr.setSwingVertical(AC_SWING_V[acS.swing]);
  acIr.setQuiet(acS.quiet);
  acIr.setPower(acS.power);
  acIr.send();
  acSave();
  acSentMs = millis();
  ledFlash(0x2060FF, 150);
  dirty = true;
}

// One function draws and handles taps (same idea as Settings)
void acUI(bool draw, int tx, int ty) {
  const int X = 8, CW = W - 16;
  int y = HDR_H + 40 - acScroll;
  bool hit = false, send = false;
  auto label = [&](const String& s) { if (draw) txt(FS, s, X, y, SOFT); y += 20; };
  auto seg = [&](int n, int sel, const char* const* names, uint8_t* target) {
    // "Auto" button is wider than the number buttons so the word fits
    float wide = (n == 6 && !strcmp(names[0], "Auto")) ? 1.7f : 1.0f;
    float u = (CW - (n - 1) * 5) / (n - 1 + wide);
    float x = X;
    for (int i = 0; i < n; i++) {
      int w = (int)(i == 0 ? u * wide : u);
      if (draw) btn((int)x, y, w, 32, names[i], i == sel);
      else if (!hit && hitR(tx, ty, (int)x, y, w, 32)) { *target = i; hit = true; send = true; }
      x += w + 5;
    }
    y += 40;
  };
  // big temperature with - / +
  if (draw) {
    spr.fillRoundRect(X, y, CW, 64, 12, C(CARD));
    btn(X + 6, y + 10, 52, 44, "-", false);
    btn(X + CW - 58, y + 10, 52, 44, "+", false);
    txt(FXL, String(acS.temp), W / 2 - 6, y + 33, acS.power ? INK : SOFT, D_MC);
    txt(FB, "C", W / 2 + 32, y + 20, acS.power ? INK : SOFT, D_MC);
  } else if (!hit && hitR(tx, ty, X, y, 64, 64) && acS.temp > kPanasonicAcMinTemp) { acS.temp--; hit = send = true; }
  else if (!hit && hitR(tx, ty, X + CW - 64, y, 64, 64) && acS.temp < kPanasonicAcMaxTemp) { acS.temp++; hit = send = true; }
  y += 72;
  // power
  if (draw) btn(X, y, CW, 38, acS.power ? "ON  (tap to turn off)" : "OFF  (tap to turn on)", acS.power);
  else if (!hit && hitR(tx, ty, X, y, CW, 38)) { acS.power = !acS.power; hit = send = true; }
  y += 46;
  label("Mode");       seg(4, acS.mode, AC_MODE_N, &acS.mode);
  label("Fan speed");  seg(6, acS.fan, AC_FAN_N, &acS.fan);
  label("Swing (up-down)"); seg(6, acS.swing, AC_SWING_N, &acS.swing);
  // quiet + remote type
  int hw = (CW - 6) / 2;
  if (draw) {
    btn(X, y, hw, 32, acS.quiet ? "Quiet: ON" : "Quiet: OFF", acS.quiet);
    btn(X + hw + 6, y, hw, 32, String("Type: ") + AC_MODEL_N[acS.model], false);
  } else if (!hit && hitR(tx, ty, X, y, hw, 32)) { acS.quiet = !acS.quiet; hit = send = true; }
  else if (!hit && hitR(tx, ty, X + hw + 6, y, hw, 32)) { acS.model = (acS.model + 1) % AC_NMODELS; hit = true; acSave(); }
  y += 40;
  if (draw) {
    txt(FS, fitText(FS, "Point the board at the AC.", CW), X, y, SOFT); y += 18;
    txt(FS, fitText(FS, "No beep? Change Type.", CW), X, y, SOFT); y += 18;
  } else y += 36;
  y += 8;
  acMax = max(0, y + acScroll - FTR_Y);
  if (send) acSend();
  if (hit) dirty = true;
}
void drawAC() {
  drawAppTitle("AC Remote", millis() - acSentMs < 1200 ? String("Sent!") : String(""));
  acScroll = constrain(acScroll, 0, acMax);
  spr.setClipRect(0, HDR_H + 36, W, FTR_Y - HDR_H - 36);
  acUI(true, -1, -1);
  scrollBar(HDR_H + 36, FTR_Y - HDR_H - 36, acScroll, acMax);
  spr.clearClipRect();
}
void acTap(int x, int y) {
  if (backHit(x, y)) { scr = S_APPS; dirty = true; return; }
  if (y < HDR_H + 36) return;
  acUI(false, x, y);
}
