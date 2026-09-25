#pragma once
// Clock: the time, the day key, the guessed time used before the real time is known.
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

// While the real time is not known, the board runs on a guess (the last saved time).
// When the real time arrives, logs made on the guess are moved to the right time and day (see fixGuessedLogs).
uint32_t guessEpoch = 0, guessMs = 0;   // the guess, and millis() when it was made
volatile bool timeFixPending = false; volatile int32_t timeFixDelta = 0;
uint32_t guessNow() { return guessEpoch + (millis() - guessMs) / 1000; }
void onTimeSync(struct timeval*) {   // runs in the network task: only note it, loop() does the work
  if (timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)nowT() - guessNow()); timeFixPending = true; }
  timeApprox = false;
  dirty = true;
}
void setClock(uint32_t epoch, bool exact) {
  if (exact && timeApprox && guessEpoch) { timeFixDelta = (int32_t)((int64_t)epoch - guessNow()); timeFixPending = true; }
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  if (exact) timeApprox = false;
  else { guessEpoch = epoch; guessMs = millis(); }
  prefs.putUInt("epoch", epoch);
}
