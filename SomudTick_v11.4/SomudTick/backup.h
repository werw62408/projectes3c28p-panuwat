#pragma once
// Weekly backup: all logs are copied to the SD card as one CSV file, so a broken board / flash does not lose them.
//   /SomudTick/backup/logs_2026-09-25.csv   (same columns as the web page export; the newest 8 are kept)
// It runs by itself when a week has passed, while the screen is off (so it never slows you down),
// or at once from Settings > About > "Back up now".
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

#define BACKUP_DIR "/SomudTick/backup"
const int BACKUP_KEEP = 8;
const uint32_t BACKUP_EVERY = 7UL * 86400;
String csvRow(const Ev& e);   // web_api.h

// every day file, oldest first, into one CSV on the card. Returns the number of logs, or -1 if the card can't be written.
int writeLogsCsv(const String& path) {
  File out = SD_MMC.open(path, FILE_WRITE);
  if (!out) return -1;
  out.print("\xEF\xBB\xBF" "datetime,epoch,activity_id,activity_name,value,unit,place,time_ok\n");
  std::vector<String> keys;
  File root = logFs().open("/log");
  if (root) { for (File f = root.openNextFile(); f; f = root.openNextFile()) { String nm = f.name(); if (nm.endsWith(".csv")) keys.push_back(nm); f.close(); } root.close(); }
  std::sort(keys.begin(), keys.end());
  int rows = 0; bool ok = true;
  for (auto& k : keys) {
    File f = logFs().open("/log/" + k, "r"); if (!f) continue;
    String chunk;
    while (f.available()) { String l = f.readStringUntil('\n'); Ev e; if (parseLine(l, e)) { chunk += csvRow(e); rows++; } }
    f.close();
    if (chunk.length() && out.print(chunk) != chunk.length()) { ok = false; break; }   // card full
  }
  out.close();
  if (!ok) { SD_MMC.remove(path); return -1; }
  return rows;
}

uint32_t bkAt = 0;          // epoch of the last backup (0 = never)
String bkErr;               // last problem, shown in About
uint32_t bkTryMs = 0;       // when it last tried by itself (tries at most once an hour)
void backupLoad() { bkAt = prefs.getUInt("bkAt", 0); }
bool backupNow() {
  bkErr = "";
  if (timeApprox) { bkErr = "set the time first"; return false; }   // the file name is the date
  if (!sdMount()) { bkErr = "no SD card"; return false; }
  sdMkdirs(BACKUP_DIR);
  String path = String(BACKUP_DIR) + "/logs_" + dayKey(nowT()) + ".csv";
  int rows = writeLogsCsv(path);
  if (rows < 0) { bkErr = "card full?"; return false; }
  // keep the newest few
  std::vector<String> old;
  File d = SD_MMC.open(BACKUP_DIR);
  if (d) { for (File f = d.openNextFile(); f; f = d.openNextFile()) { String n = baseOf(f.path()); if (n.startsWith("logs_")) old.push_back(n); f.close(); } d.close(); }
  std::sort(old.begin(), old.end());
  for (int i = 0; i + BACKUP_KEEP < (int)old.size(); i++) SD_MMC.remove(String(BACKUP_DIR) + "/" + old[i]);
  bkAt = (uint32_t)nowT(); prefs.putUInt("bkAt", bkAt);
  return true;
}
// from loop(): a week has passed and nobody is using the board (screen off, or 2 minutes without a touch) -> back up
void backupTask(uint32_t idleMs) {
  if ((pw != P_OFF && idleMs < 120000UL) || timeApprox || usbDriveOn || scr == S_GAME) return;
  if ((uint32_t)nowT() - bkAt < BACKUP_EVERY && bkAt <= (uint32_t)nowT()) return;
  if (bkTryMs && millis() - bkTryMs < 3600000UL) return;
  bkTryMs = millis();
  backupNow();
}
String backupWhen() {   // About: "24 Sep" / "never"
  if (!bkAt) return "never";
  time_t t = bkAt; struct tm tm; localtime_r(&t, &tm);
  return String(tm.tm_mday) + " " + EN_MON[tm.tm_mon];
}
