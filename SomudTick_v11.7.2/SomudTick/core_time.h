#pragma once
// Clock: the time, the day key, the guessed time used before the real time is known,
// and the DS3231 clock module (keeps the time while the board is switched off).
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- เวลา ----------------
time_t nowT() { return time(nullptr); }
String dayKey(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[24]; snprintf(b, sizeof b, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return b;
}
String hhmm(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[16]; snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
const char* EN_DOW[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* EN_DOW2[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
const char* EN_MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// where the time came from (shown in Settings > About)
enum TimeSrc : uint8_t { TS_GUESS, TS_RTC, TS_NET, TS_PHONE, TS_KEPT };
uint8_t timeSrc = TS_GUESS;
const char* TIME_SRC_N[] = {"guess", "module", "Wi-Fi", "phone", "kept"};   // short: shown after the time in About

// ---------------- DS3231 clock module ----------------
// I2C connector: SDA IO16, SCL IO15, 3.3V, GND (same wires as the touch chip). Address 0x68.
// The module keeps UTC. It is read once at start and written when the time comes from the internet or the phone.
#define RTC_ADDR 0x68
#define RTC_I2C_PORT 1
bool rtcFound = false;      // a DS3231 answered at start
bool rtcTimeOk = false;     // it had a good time (not stopped / battery OK)
volatile bool rtcWritePending = false;   // set from the network task, written by loop() (one user of the I2C wires at a time)
uint32_t rtcWrittenAt = 0;  // epoch of the last write (About: "set 2h ago")

#ifdef SIM
extern bool g_simRtcOn; extern uint8_t g_simRtc[19];
bool rtcRead(uint8_t reg, uint8_t* d, size_t n) { if (!g_simRtcOn) return false; memcpy(d, g_simRtc + reg, n); return true; }
bool rtcWriteRegs(uint8_t reg, const uint8_t* d, size_t n) { if (!g_simRtcOn) return false; memcpy(g_simRtc + reg, d, n); return true; }
#else
bool rtcRead(uint8_t reg, uint8_t* d, size_t n) { return lgfx::i2c::readRegister(RTC_I2C_PORT, RTC_ADDR, reg, d, n, 400000).has_value(); }
bool rtcWriteRegs(uint8_t reg, const uint8_t* d, size_t n) {
  uint8_t b[9]; if (n > 8) return false;
  b[0] = reg; memcpy(b + 1, d, n);
  return lgfx::i2c::transactionWrite(RTC_I2C_PORT, RTC_ADDR, b, n + 1, 400000).has_value();
}
#endif
static uint8_t bcd2(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static uint8_t tobcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
// days since 1970-01-01 for a date (and back), so no time zone gets in the way
static int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2; int era = (y >= 0 ? y : y - 399) / 400; int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
static void civilFromDays(int32_t z, int& y, int& m, int& d) {
  z += 719468; int era = (z >= 0 ? z : z - 146096) / 146097; int doe = z - era * 146097;
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; y = yoe + era * 400;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100); int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1; m = mp + (mp < 10 ? 3 : -9); y += m <= 2;
}
// read the module: returns the time (UTC epoch) or 0 if there is no module / no good time
uint32_t rtcGet() {
  uint8_t r[7], st = 0;
  if (!rtcRead(0x00, r, 7) || !rtcRead(0x0F, &st, 1)) return 0;
  if (st & 0x80) return 0;   // "oscillator stopped": the battery ran out or it was never set
  int y = 2000 + bcd2(r[6]), mo = bcd2(r[5] & 0x1F), d = bcd2(r[4] & 0x3F);
  int h = bcd2(r[2] & 0x3F), mi = bcd2(r[1] & 0x7F), s = bcd2(r[0] & 0x7F);
  if (r[2] & 0x40) return 0;   // 12-hour mode: not ours
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) return 0;
  return (uint32_t)((int64_t)daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s);
}
bool rtcSet(uint32_t epoch) {
  int32_t days = epoch / 86400; uint32_t sec = epoch % 86400;
  int y, mo, d; civilFromDays(days, y, mo, d);
  if (y < 2000 || y > 2099) return false;
  uint8_t r[7] = {tobcd(sec % 60), tobcd(sec / 60 % 60), tobcd(sec / 3600), (uint8_t)((days + 4) % 7 + 1), tobcd(d), tobcd(mo), tobcd(y - 2000)};
  if (!rtcWriteRegs(0x00, r, 7)) return false;
  uint8_t st = 0; rtcRead(0x0F, &st, 1); st &= ~0x80; rtcWriteRegs(0x0F, &st, 1);   // clear "stopped"
  uint8_t ctl = 0x1C; rtcWriteRegs(0x0E, &ctl, 1);   // oscillator on (also on battery), 32 kHz / square wave off
  rtcFound = true; rtcTimeOk = true; rtcWrittenAt = epoch;
  return true;
}
// at start: is there a module, and does it know the time?
uint32_t rtcBegin() {
  uint8_t st;
  rtcFound = rtcRead(0x0F, &st, 1);
  uint32_t e = rtcFound ? rtcGet() : 0;
  rtcTimeOk = e >= 1700000000UL;
  return rtcTimeOk ? e : 0;
}

// ---------------- the guessed time ----------------
// While the real time is not known, the board runs on a guess (the last saved time).
// When the real time arrives, logs made on the guess are moved to the right time and day (see fixGuessedLogs).
uint32_t guessEpoch = 0, guessMs = 0;   // the guess, and millis() when it was made
volatile bool timeFixPending = false; volatile int32_t timeFixDelta = 0;
uint32_t guessNow() { return guessEpoch + (millis() - guessMs) / 1000; }
// after a restart without power loss (USB drive eject, "Update firmware" cancelled) the chip still has the right time:
// this marker in memory that survives a restart says the time was real before the restart
#ifdef SIM
uint32_t keptTimeMark = 0;
#else
RTC_NOINIT_ATTR uint32_t keptTimeMark;
#endif
const uint32_t KEPT_MARK = 0x5EC0C10C;
void onTimeSync(struct timeval*) {   // runs in the network task: only note it, loop() does the work
  if (timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)nowT() - guessNow()); timeFixPending = true; }
  timeApprox = false; timeSrc = TS_NET; keptTimeMark = KEPT_MARK;
  rtcWritePending = true;
  dirty = true;
}
void setClock(uint32_t epoch, bool exact, uint8_t src = TS_GUESS) {
  if (exact && timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)epoch - guessNow()); timeFixPending = true; }
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  if (exact) { timeApprox = false; timeSrc = src; keptTimeMark = KEPT_MARK; if (src == TS_NET || src == TS_PHONE) rtcWritePending = true; }
  else { guessEpoch = epoch; guessMs = millis(); timeSrc = TS_GUESS; }
  prefs.putUInt("epoch", epoch);
}
// the time at start: clock module, else the time kept over a restart, else a guess from the last saved time
void timeBegin() {
  uint32_t r = rtcBegin();
#ifdef SIM
  bool softRestart = false;
#else
  esp_reset_reason_t rr = esp_reset_reason();
  bool softRestart = rr == ESP_RST_SW || rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT;
#endif
  if (r) { setClock(r, true, TS_RTC); return; }
  if (softRestart && keptTimeMark == KEPT_MARK && nowT() >= 1700000000) { timeApprox = false; timeSrc = TS_KEPT; return; }
  keptTimeMark = 0;
  if (nowT() < 1700000000 || timeApprox) {   // no real time -> use the last saved time
    uint32_t e = prefs.getUInt("epoch", 1767200000UL);
    setClock(max((uint32_t)nowT(), e + 30), false);
    timeApprox = true;
  }
}
void rtcTask() {   // from loop(): write the module when a new real time came in
  if (!rtcWritePending || timeApprox) return;
  rtcWritePending = false;
  rtcSet((uint32_t)nowT());   // no module: the write just fails (a module plugged in later is set here and used from the next start)
}
