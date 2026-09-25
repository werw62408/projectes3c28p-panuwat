#pragma once
// ============================================================================
//  USB drive: plug the board into a computer, and the computer sees the SD card
//  as a drive (like a card reader). Copy, delete, rename anything on it.
//    - Start: Apps > Files > "USB drive" (last row of the first page)
//    - Before it starts, all logs are copied to the card as SomudTick/logs.csv
//    - While it is on, the board does not touch the card (two writers = broken files)
//    - Done: Eject the drive on the computer. The board restarts by itself.
//  Needs the Arduino setting "USB Mode: USB-OTG (TinyUSB)" (see README).
//  Included from SomudTick.ino.
// ============================================================================
#ifndef SIM
#include "USB.h"
#include "USBMSC.h"
#include "soc/rtc_cntl_reg.h"
USBMSC usbMsc;
#endif

volatile bool usbEjected = false;
volatile uint32_t usbBytes = 0, usbLastIoMs = 0;
uint32_t usbEjectMs = 0;
bool usbAsk = false;             // "did you eject it?" box
String usbNote;                  // e.g. "Logs copied to SomudTick/logs.csv"
static uint8_t usbSec[512];

// ---- the computer reads / writes the card, one 512-byte sector at a time ----
static int32_t usbRead(uint32_t lba, uint32_t offset, void* buf, uint32_t n) {
  uint8_t* out = (uint8_t*)buf; uint32_t done = 0;
  while (done < n) {
    if (!SD_MMC.readRAW(usbSec, lba)) return -1;
    uint32_t take = min((uint32_t)512 - offset, n - done);
    memcpy(out + done, usbSec + offset, take);
    done += take; offset = 0; lba++;
  }
  usbBytes += n; usbLastIoMs = millis();
  return n;
}
static int32_t usbWrite(uint32_t lba, uint32_t offset, uint8_t* buf, uint32_t n) {
  uint32_t done = 0;
  while (done < n) {
    if (offset == 0 && n - done >= 512) { if (!SD_MMC.writeRAW(buf + done, lba)) return -1; done += 512; }
    else {   // part of a sector: read it, change the part, write it back
      if (!SD_MMC.readRAW(usbSec, lba)) return -1;
      uint32_t take = min((uint32_t)512 - offset, n - done);
      memcpy(usbSec + offset, buf + done, take);
      if (!SD_MMC.writeRAW(usbSec, lba)) return -1;
      done += take; offset = 0;
    }
    lba++;
  }
  usbBytes += n; usbLastIoMs = millis();
  return n;
}
static bool usbStartStop(uint8_t power, bool start, bool loadEject) {
  if (loadEject && !start) usbEjected = true;   // "Eject" on the computer
  return true;
}

// all logs -> SomudTick/logs.csv on the card (same columns as the web page export)
int writeLogsCsv(const String& path);   // backup.h
int usbCopyLogs() {
  if (!SD_MMC.exists("/SomudTick")) SD_MMC.mkdir("/SomudTick");
  return writeLogsCsv("/SomudTick/logs.csv");
}

void usbDriveStart() {
  if (!sdMount()) { fmSay("No SD card"); return; }
  spr.fillRoundRect(W / 2 - 90, H / 2 - 20, 180, 40, 10, C(INK));
  txt(FB, "Getting ready...", W / 2, H / 2, ONINK, D_MC);
  spr.pushSprite(0, 0);
  int rows = usbCopyLogs();
  usbNote = rows >= 0 ? "Logs: SomudTick/logs.csv" : String("Could not copy the logs");
  usbDriveOn = true;   // from now on the board leaves the card alone (sdMount() says no)
  usbEjected = false; usbBytes = 0; usbAsk = false; usbEjectMs = 0;
#ifndef SIM
  usbMsc.vendorID("SomudTk");
  usbMsc.productID("SD card");
  usbMsc.productRevision("1.0");
  usbMsc.onRead(usbRead);
  usbMsc.onWrite(usbWrite);
  usbMsc.onStartStop(usbStartStop);
  usbMsc.mediaPresent(true);
  usbMsc.begin(SD_MMC.numSectors(), SD_MMC.sectorSize());
  USB.begin();
#endif
  scr = S_USB; dirty = true;
}
void usbRestart() {
#ifndef SIM
  delay(300);
  esp_restart();
#endif
}

// ---- screen ----
void drawUsbIcon(int cx, int cy, uint32_t c) {   // the USB "trident"
  wideLine(cx, cy + 22, cx, cy - 18, 1.5f, C(c));
  spr.fillTriangle(cx - 6, cy - 16, cx + 6, cy - 16, cx, cy - 26, C(c));
  wideLine(cx, cy + 8, cx - 12, cy - 2, 1.5f, C(c)); wideLine(cx - 12, cy - 2, cx - 12, cy - 8, 1.5f, C(c));
  spr.fillCircle(cx - 12, cy - 10, 3, C(c));
  wideLine(cx, cy + 2, cx + 12, cy - 6, 1.5f, C(c)); wideLine(cx + 12, cy - 6, cx + 12, cy - 12, 1.5f, C(c));
  spr.fillRect(cx + 9, cy - 17, 7, 6, C(c));
  spr.fillCircle(cx, cy + 24, 5, C(c));
}
void drawUsb() {
  int top = HDR_H + 6, x = 8, w = W - 16;
  int cardH = FTR_Y - 44 - top;
  spr.fillRoundRect(x, top, w, cardH, 12, C(CARD));
  bool wide = land();
  int icx = wide ? x + 44 : W / 2, icy = wide ? top + cardH / 2 - 10 : top + 32;
  drawUsbIcon(icx, icy, INK);
  int tx = wide ? x + 92 : x + 12, tw = wide ? w - 100 : w - 24, ty = wide ? top + 12 : top + 64;
  bool busy = usbLastIoMs && millis() - usbLastIoMs < 1200;
  String title = usbEjected ? "Ejected. Restarting..." : "USB drive is ON";
  txt(FB, fitText(FB, title, tw), tx, ty, INK); ty += 26;
  spr.setClipRect(x, top, w, cardH);
  for (auto& l : wrapText(FS, "The computer sees the SD card now. When done, Eject it there.", tw, 4)) { txt(FS, l, tx, ty, INK); ty += 19; }
  String act = String(busy ? "Working: " : "Waiting: ") + String(usbBytes / 1048576.0f, 1) + " MB";
  if (wide) txt(FS, fitText(FS, String(usbBytes / 1048576.0f, 1) + " MB", 80), icx, icy + 44, busy ? INK : SOFT, D_TC);   // under the icon
  else { ty += 6; txt(FS, fitText(FS, act, tw), tx, ty, busy ? INK : SOFT); ty += 19; }
  for (auto& l : wrapText(FS, usbNote, tw, 2)) { txt(FS, l, tx, ty, SOFT); ty += 19; }
  spr.clearClipRect();
  if (!usbEjected) btn(x, FTR_Y - 38, w, 34, "Leave USB drive", false);
  if (usbAsk) {
    FmBox b = fmAskBox(); drawWindow(b);
    txt(FB, fitText(FB, "Ejected it first?", b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    txt(FS, fitText(FS, "Leaving without Eject", b.w - 16), b.x + b.w / 2, b.y + 44, INK, D_MC);
    txt(FS, fitText(FS, "can break new files.", b.w - 16), b.x + b.w / 2, b.y + 64, SOFT, D_MC);
    int bw = (b.w - 24) / 2;
    btn(b.x + 8, b.y + b.h - 42, bw, 34, "Not yet", false);
    btn(b.x + 16 + bw, b.y + b.h - 42, bw, 34, "Leave", true, 0xD03030);
  }
}
void usbTap(int x, int y) {
  if (usbEjected) return;
  if (usbAsk) {
    FmBox b = fmAskBox(); int bw = (b.w - 24) / 2, by = b.y + b.h - 42;
    if (hitR(x, y, b.x + 16 + bw, by, bw, 36)) { usbEjected = true; usbEjectMs = millis(); dirty = true; }
    else usbAsk = false;
    dirty = true; return;
  }
  if (y >= FTR_Y - 40) { usbAsk = true; dirty = true; }
}
void usbTick() {   // from loop(): after Eject, restart so the board uses the card again
  static uint32_t t = 0;
  if (usbEjected && !usbEjectMs) { usbEjectMs = millis(); dirty = true; }
  if (usbEjectMs && millis() - usbEjectMs > 1500) usbRestart();
  if (millis() - t > 500) { t = millis(); if (pw == P_ON) dirty = true; }   // "Working..." / MB moved
}

// ---- Settings > About > Update firmware: restart into the chip's flash mode (no BOOT / RESET buttons needed) ----
void enterUpdateMode() {
  lcd.fillScreen(TFT_WHITE);
  lcd.setTextDatum(D_MC); lcd.setTextColor(TFT_BLACK);
  int cx = lcd.width() / 2, cy = lcd.height() / 2;
  lcd.setFont(FL); lcd.drawString("Update mode", cx, cy - 50);
  lcd.setFont(FS);
  lcd.drawString("Open the flasher web page", cx, cy - 14);
  lcd.drawString("and press Connect.", cx, cy + 8);
  lcd.drawString("Cancel: press RESET.", cx, cy + 40);
#ifndef SIM
  delay(800);
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
#endif
}
