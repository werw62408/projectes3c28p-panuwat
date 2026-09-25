#pragma once
// Hardware helpers: RGB LED, battery voltage and %, and saving battery while the screen is off.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- LED ----------------
uint32_t ledColor = 0, ledUntil = 0;
bool anyOverdue = false;   // some activity is late (checked once a second in loop(), not on every LED update)
void ledFlash(uint32_t col, uint32_t ms) { ledColor = col; ledUntil = millis() + ms; }
void ledTask() {
  static uint32_t lastSet = 1;
  uint32_t want = 0;
  if ((int32_t)(ledUntil - millis()) > 0) want = ledColor;   // (this way also works when millis() wraps after 49 days)
  else if (anyOverdue && (millis() % 4000) < 150) want = 0xFFFFFF;
  if (want != lastSet) {
    rgbLedWrite(PIN_RGB, ((want >> 16) & 255) / 6, ((want >> 8) & 255) / 6, (want & 255) / 6);
    lastSet = want;
  }
}
void overdueCheck() { bool od = false; for (auto& a : acts) if (overdue(a)) { od = true; break; } anyOverdue = od; }

// ---------------- แบตเตอรี่ ----------------
float batV = 0;
void readBattery() {
  uint32_t mv = 0; for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  batV = mv / 8.0f * 2.0f / 1000.0f;   // วงจรแบ่งแรงดัน 1/2
}
// Li-ion voltage -> %: the curve is flat in the middle and drops fast at the end, so a straight line was wrong
// (it showed 30% when the battery was almost empty). While charging the voltage reads a bit high.
int batPct() {
  if (batV < 2.5f) return -1;   // no battery: USB power
  const float V[] = {3.30f, 3.50f, 3.60f, 3.65f, 3.70f, 3.75f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  const int P[] = {0, 3, 8, 15, 25, 35, 45, 62, 78, 90, 100};
  if (batV <= V[0]) return 0;
  for (int i = 1; i < 11; i++) if (batV <= V[i]) return P[i - 1] + (int)((batV - V[i - 1]) / (V[i] - V[i - 1]) * (P[i] - P[i - 1]));
  return 100;
}

// ---------------- battery saving (screen off) ----------------
// Screen off:  the screen chip sleeps, the processor runs slower (80 MHz instead of 240), touch is checked 25 times
//              a second instead of 200. Reminders, the web page and the clock keep working.
// After 10 minutes off with no phone on the hotspot, the hotspot sleeps too. Waking the screen turns it back on.
// Everything is back to full speed the moment the screen wakes.
bool lowPower = false;
bool apAsleep = false;               // the hotspot is off only to save battery (the setting stays ON)
const uint32_t AP_SLEEP_MS = 600000UL;
void setupWifi();
void powerLow(bool on) {
  if (on == lowPower) return;
  lowPower = on;
  if (on) {
    lcd.sleep();
#ifndef SIM
    setCpuFrequencyMhz(80);
#endif
  } else {
#ifndef SIM
    setCpuFrequencyMhz(240);
#endif
    lcd.wakeup();
    if (apAsleep) { apAsleep = false; setupWifi(); }
  }
}
void hotspotSleepTask(uint32_t offForMs) {   // from loop() every few seconds while the screen is off
  if (!lowPower || apAsleep || !apOn || offForMs < AP_SLEEP_MS) return;
  if (WiFi.softAPgetStationNum() > 0) return;   // a phone is on it: keep it
  apAsleep = true; setupWifi();
}
