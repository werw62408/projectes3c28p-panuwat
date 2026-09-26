#pragma once
// ============================================================================
//  Deck (v11.7): a grid of shortcut keys, like a small Stream Deck.
//  The board becomes a keyboard for:
//    - USB: a computer (Windows / Mac / Linux), no program needed. Plug the USB cable in.
//    - Bluetooth: a phone (iPhone / Android) or a computer. Pair "SomudTick Deck" once in its Bluetooth settings.
//  Each key sends a key combination (like Ctrl+C), a media key (play, volume...) or types a short text.
//  24 keys on 2 pages. The keys are set on the phone web page (tab "Deck"), kept in /deck.json.
//  Joystick: move the ring, press = the key. Screen off: the first touch / press only wakes it.
//  v11.7.2: its own black + neon look (not the light/dark theme), no bottom bar: Exit (top left) or hold the stick left 1 s.
//  Included from SomudTick.ino.
// ============================================================================
#ifndef SIM
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#endif

#define DECK_NEON 1   // v11.7.2: own neon look, no bottom bar (tests read it)
enum DeckKind : uint8_t { DK_NONE, DK_COMBO, DK_MEDIA, DK_TEXT };
struct DeckKey { String label; uint8_t kind = DK_NONE; uint8_t mod = 0; uint8_t key = 0; uint16_t media = 0; String text; };
const int DECK_N = 24, DECK_PER_PAGE = 12;
DeckKey deckKeys[DECK_N];
int deckPage = 0;
enum DeckVia : uint8_t { DV_USB, DV_BT };
DeckVia deckVia = DV_USB;
bool deckUsbOn = false, deckBleOn = false;
volatile bool deckBleLinked = false;
String deckNote; uint32_t deckNoteMs = 0; bool deckNoteBad = false;
uint32_t deckSent = 0;          // keys sent (tests read it)
int deckFlash = -1; uint32_t deckFlashMs = 0;
// HID modifier bits
const uint8_t DM_CTRL = 0x01, DM_SHIFT = 0x02, DM_ALT = 0x04, DM_GUI = 0x08;   // GUI = Windows key / Command

// ---------------- the keys: defaults, file, JSON ----------------
void deckSet(int i, const char* label, uint8_t kind, uint8_t mod, uint8_t key, uint16_t media, const char* text = "") {
  deckKeys[i].label = label; deckKeys[i].kind = kind; deckKeys[i].mod = mod; deckKeys[i].key = key; deckKeys[i].media = media; deckKeys[i].text = text;
}
void deckDefaults() {
  for (auto& k : deckKeys) k = DeckKey();
  // page 1: phone / music (works on iPhone, Android and computers)
  deckSet(0, "Play Pause", DK_MEDIA, 0, 0, 0xCD); deckSet(1, "Next", DK_MEDIA, 0, 0, 0xB5); deckSet(2, "Back", DK_MEDIA, 0, 0, 0xB6);
  deckSet(3, "Vol +", DK_MEDIA, 0, 0, 0xE9); deckSet(4, "Vol -", DK_MEDIA, 0, 0, 0xEA); deckSet(5, "Mute", DK_MEDIA, 0, 0, 0xE2);
  deckSet(6, "Photo", DK_MEDIA, 0, 0, 0xE9);          // iPhone camera: volume up takes a picture
  deckSet(7, "Search", DK_COMBO, DM_GUI, 0x2C, 0);    // Cmd+Space: iPhone / Mac search
  deckSet(8, "Home", DK_COMBO, DM_GUI, 0x0B, 0);      // Cmd+H: iPhone home screen
  deckSet(9, "Slide >", DK_COMBO, 0, 0x4F, 0);        // right arrow: next slide
  deckSet(10, "< Slide", DK_COMBO, 0, 0x50, 0);       // left arrow
  deckSet(11, "Enter", DK_COMBO, 0, 0x28, 0);
  // page 2: Windows computer
  deckSet(12, "Copy", DK_COMBO, DM_CTRL, 0x06, 0); deckSet(13, "Paste", DK_COMBO, DM_CTRL, 0x19, 0); deckSet(14, "Undo", DK_COMBO, DM_CTRL, 0x1D, 0);
  deckSet(15, "Save", DK_COMBO, DM_CTRL, 0x16, 0);
  deckSet(16, "Screen shot", DK_COMBO, DM_GUI | DM_SHIFT, 0x16, 0);   // Win+Shift+S
  deckSet(17, "Lock PC", DK_COMBO, DM_GUI, 0x0F, 0);                 // Win+L
  deckSet(18, "Show desk", DK_COMBO, DM_GUI, 0x07, 0);                 // Win+D
  deckSet(19, "Alt+Tab", DK_COMBO, DM_ALT, 0x2B, 0);
  deckSet(20, "Zoom mic", DK_COMBO, DM_ALT, 0x04, 0);                // Zoom: Alt+A
  deckSet(21, "Teams mic", DK_COMBO, DM_CTRL | DM_SHIFT, 0x10, 0);   // Teams: Ctrl+Shift+M
  deckSet(22, "Task Mgr", DK_COMBO, DM_CTRL | DM_SHIFT, 0x29, 0);    // Ctrl+Shift+Esc
  deckSet(23, "Hello", DK_TEXT, 0, 0, 0, "Hello from SomudTick!");
}
void deckToJson(JsonArray a) {
  for (auto& k : deckKeys) {
    JsonObject o = a.add<JsonObject>();
    o["label"] = k.label; o["kind"] = k.kind; o["mod"] = k.mod; o["key"] = k.key; o["media"] = k.media; o["text"] = k.text;
  }
}
// from the web page (or the file). Anything wrong is cleaned: long texts are cut, unknown values become "nothing".
bool deckFromJson(JsonArrayConst a) {
  if (a.isNull() || a.size() > DECK_N) return false;
  for (int i = 0; i < DECK_N; i++) {
    DeckKey k;
    if (i < (int)a.size()) {
      JsonObjectConst o = a[i];
      k.label = String((const char*)(o["label"] | "")).substring(0, 16);
      int kind = o["kind"] | 0; k.kind = kind >= DK_NONE && kind <= DK_TEXT ? kind : DK_NONE;
      k.mod = (o["mod"] | 0) & 0x0F;
      int key = o["key"] | 0; k.key = key >= 0 && key <= 0x65 ? key : 0;
      int media = o["media"] | 0; k.media = media >= 0 && media <= 0x3FF ? media : 0;
      k.text = String((const char*)(o["text"] | "")).substring(0, 60);
      if (k.kind == DK_COMBO && !k.key) k.kind = DK_NONE;
      if (k.kind == DK_MEDIA && !k.media) k.kind = DK_NONE;
      if (k.kind == DK_TEXT && !k.text.length()) k.kind = DK_NONE;
    }
    deckKeys[i] = k;
  }
  return true;
}
void deckSave() {
  JsonDocument d; deckToJson(d.to<JsonArray>());
  File f = LittleFS.open("/deck.json", "w");
  if (f) { serializeJson(d, f); f.close(); }
}
void deckLoad() {
  deckDefaults();
  File f = LittleFS.open("/deck.json", "r");
  if (!f) return;
  JsonDocument d;
  if (deserializeJson(d, f) || !deckFromJson(d.as<JsonArrayConst>())) deckDefaults();   // a broken file: the defaults
  f.close();
  deckVia = prefs.getUChar("deckVia", DV_USB) == DV_BT ? DV_BT : DV_USB;
}

// ---------------- typing text: US keyboard layout ----------------
bool deckCharKey(char c, uint8_t& mod, uint8_t& key) {
  mod = 0;
  if (c >= 'a' && c <= 'z') { key = 0x04 + (c - 'a'); return true; }
  if (c >= 'A' && c <= 'Z') { key = 0x04 + (c - 'A'); mod = DM_SHIFT; return true; }
  if (c >= '1' && c <= '9') { key = 0x1E + (c - '1'); return true; }
  if (c == '0') { key = 0x27; return true; }
  const char* plain = " -=[]\\;'`,./";   const uint8_t pk[] = {0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
  const char* shifted = "!@#$%^&*()_+{}|:\"~<>?";
  const uint8_t sk[] = {0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
  for (int i = 0; plain[i]; i++) if (plain[i] == c) { key = pk[i]; return true; }
  for (int i = 0; shifted[i]; i++) if (shifted[i] == c) { key = sk[i]; mod = DM_SHIFT; return true; }
  if (c == '\n') { key = 0x28; return true; }
  return false;   // (Thai and other letters can't be typed as keys)
}

// ---------------- sending: USB and Bluetooth ----------------
#ifdef SIM
struct DeckSent { char via; uint8_t mod, key; uint16_t media; bool down; };
std::vector<DeckSent> g_simDeck;   // everything sent (tests)
bool g_simUsbHost = true;          // a computer is on the cable
void deckRawKey(uint8_t mod, uint8_t key, bool down) { g_simDeck.push_back({deckVia == DV_USB ? 'U' : 'B', mod, key, 0, down}); }
void deckRawMedia(uint16_t u, bool down) { g_simDeck.push_back({deckVia == DV_USB ? 'U' : 'B', 0, 0, u, down}); }
bool deckUsbStart() { deckUsbOn = true; return true; }
bool deckUsbReady() { return deckUsbOn && g_simUsbHost; }
bool deckBleStart() { if (!BLEDevice::getInitialized()) BLEDevice::init("SomudTick Deck"); deckBleOn = true; return true; }
void deckBleStop() { if (deckBleOn) BLEDevice::deinit(false); deckBleOn = false; deckBleLinked = false; }
#else
USBHIDKeyboard* deckKb = nullptr; USBHIDConsumerControl* deckCc = nullptr;
BLEServer* deckSrv = nullptr; BLEHIDDevice* deckHid = nullptr; BLECharacteristic* deckInKey = nullptr; BLECharacteristic* deckInMedia = nullptr;
const uint8_t DECK_REPORT_MAP[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,             // keyboard, report 1
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,   // 8 modifier bits
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,                         // reserved byte
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,   // 6 keys
  0xC0,
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02,             // media keys, report 2
  0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
  0xC0};
class DeckSrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { deckBleLinked = true; dirty = true; }
  void onDisconnect(BLEServer*) override { deckBleLinked = false; dirty = true; if (deckBleOn) BLEDevice::startAdvertising(); }
};
void deckRawKey(uint8_t mod, uint8_t key, bool down) {
  uint8_t r[8] = {down ? mod : (uint8_t)0, 0, down ? key : (uint8_t)0, 0, 0, 0, 0, 0};
  if (deckVia == DV_USB) { if (deckKb) { KeyReport k; memset(&k, 0, sizeof k); k.modifiers = r[0]; k.keys[0] = r[2]; deckKb->sendReport(&k); } }
  else if (deckInKey && deckBleLinked) { deckInKey->setValue(r, 8); deckInKey->notify(); }
}
void deckRawMedia(uint16_t u, bool down) {
  if (deckVia == DV_USB) { if (deckCc) { if (down) deckCc->press(u); else deckCc->release(); } }
  else if (deckInMedia && deckBleLinked) { uint8_t r[2] = {(uint8_t)(down ? u & 0xFF : 0), (uint8_t)(down ? u >> 8 : 0)}; deckInMedia->setValue(r, 2); deckInMedia->notify(); }
}
bool deckUsbStart() {
  if (deckUsbOn) return true;
  if (usbStarted) return false;   // the USB drive was used since the board started: a restart is needed first
  deckKb = new USBHIDKeyboard(); deckCc = new USBHIDConsumerControl();
  deckKb->begin(); deckCc->begin();
  USB.productName("SomudTick Deck");
  USB.begin(); usbStarted = true; deckUsbOn = true;
  return true;
}
bool deckUsbReady() { return deckUsbOn && (bool)USB; }
bool deckBleStart() {
  if (deckBleOn) return true;
  if (btBusy) return false;
  if (!BLEDevice::getInitialized()) BLEDevice::init("SomudTick Deck");
  BLESecurity* sec = new BLESecurity();
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setAuthenticationMode(true, false, true);   // bond (remember the phone), no PIN
  deckSrv = BLEDevice::createServer();
  deckSrv->setCallbacks(new DeckSrvCb());
  deckHid = new BLEHIDDevice(deckSrv);
  deckHid->manufacturer()->setValue("SomudTick");
  deckHid->pnp(0x02, 0xE502, 0xA111, 0x0210);
  deckHid->hidInfo(0x00, 0x01);
  deckHid->reportMap((uint8_t*)DECK_REPORT_MAP, sizeof DECK_REPORT_MAP);
  deckInKey = deckHid->inputReport(1); deckInMedia = deckHid->inputReport(2);
  deckHid->setBatteryLevel(max(0, batPct()));
  deckHid->startServices();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);   // keyboard
  adv->addServiceUUID(deckHid->hidService()->getUUID());
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  deckBleOn = true;
  return true;
}
void deckBleStop() {   // gives the Bluetooth memory back (the phone connects again by itself next time)
  if (!deckBleOn) return;
  BLEDevice::deinit(false);
  deckSrv = nullptr; deckHid = nullptr; deckInKey = deckInMedia = nullptr;   // (freed with Bluetooth)
  deckBleOn = false; deckBleLinked = false;
}
#endif
bool deckReady() { return deckVia == DV_USB ? deckUsbReady() : deckBleOn && deckBleLinked; }
void deckNoteSet(const String& s, bool bad) { deckNote = s; deckNoteBad = bad; deckNoteMs = millis(); dirty = true; }
void deckStartVia() {
  if (deckVia == DV_USB) {
    deckBleStop();
    if (!deckUsbStart()) deckNoteSet("Switch off/on first", true);   // the USB drive was used since the start
  } else if (!deckBleStart()) deckNoteSet("Bluetooth busy: again", true);
  dirty = true;
}
void deckSend(int i) {
  if (i < 0 || i >= DECK_N) return;
  DeckKey& k = deckKeys[i];
  if (k.kind == DK_NONE) return;
  if (!deckReady()) { deckNoteSet(deckVia == DV_USB ? "Not connected: USB" : "Not paired yet", true); return; }
  if (k.kind == DK_COMBO) { deckRawKey(k.mod, k.key, true); delay(12); deckRawKey(0, 0, false); }
  else if (k.kind == DK_MEDIA) { deckRawMedia(k.media, true); delay(12); deckRawMedia(0, false); }
  else {
    for (unsigned c = 0; c < k.text.length(); c++) {
      uint8_t m, key; if (!deckCharKey(k.text[c], m, key)) continue;
      deckRawKey(m, key, true); delay(6); deckRawKey(0, 0, false); delay(6);
    }
  }
  deckSent++; deckFlash = i; deckFlashMs = millis();
  ledFlash(0x40A0FF, 60);
  dirty = true;
}

// ---------------- the screen ----------------
void deckOpen() { scr = S_DECK; deckPage = 0; deckStartVia(); dirty = true; }
void deckLeave() { deckBleStop(); scr = S_APPS; dirty = true; }   // (USB stays on: it can't be taken back without a restart, and it is harmless)
int deckCols() { return land() ? 4 : 3; }
int deckTop() { return HDR_H + 34 + 30; }
const int DECK_GAP = 6;   // (the glow is 2 px on each side)
void deckKeyRect(int slot, int& x, int& y, int& w, int& h) {   // v11.7.2: no bottom bar here, the keys go down to the screen edge
  int cols = deckCols(), rows = DECK_PER_PAGE / cols, top = deckTop();
  w = (W - 14 - DECK_GAP * (cols - 1)) / cols; h = (H - 7 - top - DECK_GAP * (rows - 1)) / rows;
  x = 7 + (slot % cols) * (w + DECK_GAP); y = top + (slot / cols) * (h + DECK_GAP);
}
String deckStatus() {
  if (deckNote.length() && millis() - deckNoteMs < 4000) return deckNote;
  if (deckVia == DV_USB) return deckUsbReady() ? "USB: connected" : "USB: plug in cable";
  return deckBleLinked ? "Bluetooth: connected" : "BT: not paired";
}
// ---------------- v11.7.2: the Deck's own look (black + neon), the same on the light and the dark theme ----------------
const uint32_t DN_BG = 0x05070D, DN_GRID = 0x0C1422, DN_CYAN = 0x00E5FF, DN_PINK = 0xFF2BD6, DN_GREEN = 0x39FF88,
               DN_RED = 0xFF3B5C, DN_DIM = 0x3A4A66, DN_TEXT = 0xDDF6FF;
// a glowing box: 2 soft rings outside, a dark tinted inside, a 2 px bright edge. fillT = how much colour inside.
void neonBox(int x, int y, int w, int h, int r, uint32_t col, float fillT) {
  spr.drawRoundRect(x - 2, y - 2, w + 4, h + 4, r + 2, C(blend(DN_BG, col, 0.14f)));
  spr.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, C(blend(DN_BG, col, 0.35f)));
  spr.fillRoundRect(x, y, w, h, r, C(blend(DN_BG, col, fillT)));
  spr.drawRoundRect(x, y, w, h, r, C(col));
  spr.drawRoundRect(x + 1, y + 1, w - 2, h - 2, r - 1, C(blend(DN_BG, col, 0.7f)));
}
void neonBtn(int x, int y, int w, int h, const String& label, uint32_t col, uint8_t kind = NK_BTN) {
  navAdd(x, y, w, h, kind);
  neonBox(x, y, w, h, 8, col, 0.10f);
  spr.setFont(pickFont(FB, label));
  bool small = spr.textWidth(label) > w - 8;
  txt(small ? FS : FB, small ? fitText(FS, label, w - 4) : label, x + w / 2, y + h / 2, blend(col, 0xFFFFFF, 0.45f), D_MC);
}
// the top bar in neon colours (same layout as everywhere, so the Home/Uni chip still works with a long press)
void drawDeckHeader() {
  uint32_t b = HDRBG, f = HDRFG, s = HDRSOFT;
  HDRBG = 0x000000; HDRFG = DN_CYAN; HDRSOFT = 0x5A86A8;
  drawHeader();
  HDRBG = b; HDRFG = f; HDRSOFT = s;
  spr.drawFastHLine(0, HDR_H - 1, W, C(DN_CYAN));
  spr.drawFastHLine(0, HDR_H, W, C(blend(DN_BG, DN_CYAN, 0.35f)));
  spr.drawFastHLine(0, HDR_H + 1, W, C(blend(DN_BG, DN_CYAN, 0.12f)));
}
// the name on a key: big if it fits, else small, else small on two lines (split at a space), else cut
void deckLabel(const String& s, int x, int y, int w, int h, uint32_t col) {
  int room = w - 8;
  spr.setFont(pickFont(FB, s)); if ((int)spr.textWidth(s) <= room) { txt(FB, s, x + w / 2, y + h / 2 + 2, col, D_MC); return; }
  spr.setFont(pickFont(FS, s)); if ((int)spr.textWidth(s) <= room) { txt(FS, s, x + w / 2, y + h / 2 + 2, col, D_MC); return; }
  int sp = -1, best = 999;
  for (int i = 1; i + 1 < (int)s.length(); i++) if (s[i] == ' ' && abs(i - (int)s.length() / 2) < best) { best = abs(i - (int)s.length() / 2); sp = i; }
  if (sp > 0 && h >= 30) {
    String a = s.substring(0, sp), b = s.substring(sp + 1);
    txt(FS, fitText(FS, a, room), x + w / 2, y + h / 2 - 5, col, D_MC); txt(FS, fitText(FS, b, room), x + w / 2, y + h / 2 + 9, col, D_MC);
    return;
  }
  txt(FS, fitText(FS, s, room), x + w / 2, y + h / 2 + 2, col, D_MC);
}
uint32_t deckKindCol(uint8_t kind) { return kind == DK_MEDIA ? DN_CYAN : kind == DK_TEXT ? DN_GREEN : DN_PINK; }   // cyan media, pink shortcut, green text
void drawDeck() {
  spr.fillRect(0, 0, W, H, C(DN_BG));
  for (int gx = 0; gx < W; gx += 20) spr.drawFastVLine(gx, HDR_H, H - HDR_H, C(DN_GRID));   // a faint grid behind everything
  for (int gy = HDR_H + 10; gy < H; gy += 20) spr.drawFastHLine(0, gy, W, C(DN_GRID));
  // title row: Exit (this page has no bottom bar), the name, USB <-> Bluetooth
  int ty = HDR_H + 4;
  navAdd(6, ty, 64, 26, NK_BACK);
  neonBox(6, ty, 64, 26, 8, DN_CYAN, 0.10f);
  spr.fillTriangle(15, ty + 13, 23, ty + 7, 23, ty + 19, C(DN_CYAN));
  txt(FB, "Exit", 28, ty + 13, blend(DN_CYAN, 0xFFFFFF, 0.45f), D_ML);
  txt(FB, "DECK", (76 + W - 76) / 2, ty + 13, DN_CYAN, D_MC);
  neonBtn(W - 70, ty, 64, 26, deckVia == DV_USB ? "USB" : "BT", DN_CYAN);   // tap: USB <-> Bluetooth
  // status row: a glowing dot, the state, the page button
  int y = HDR_H + 36;
  bool bad = deckNote.length() && millis() - deckNoteMs < 4000 ? deckNoteBad : !deckReady();
  uint32_t dc = bad ? DN_RED : DN_GREEN;
  spr.fillCircle(15, y + 12, 7, C(blend(DN_BG, dc, 0.25f))); spr.fillCircle(15, y + 12, 4, C(dc));
  txt(FS, fitText(FS, deckStatus(), W - 28 - 78), 27, y + 12, bad ? blend(DN_RED, 0xFFFFFF, 0.3f) : DN_TEXT, D_ML);
  neonBtn(W - 70, y, 64, 24, String(deckPage + 1) + "/2  >", DN_DIM);   // next page
  for (int s = 0; s < DECK_PER_PAGE; s++) {
    int i = deckPage * DECK_PER_PAGE + s, x, yy, w, h; deckKeyRect(s, x, yy, w, h);
    DeckKey& k = deckKeys[i];
    bool lit = deckFlash == i && millis() - deckFlashMs < 250;
    if (k.kind == DK_NONE) { spr.drawRoundRect(x, yy, w, h, 10, C(blend(DN_BG, DN_DIM, 0.6f))); continue; }
    navAdd(x, yy, w, h);
    uint32_t col = deckKindCol(k.kind);
    neonBox(x, yy, w, h, 10, col, lit ? 0.75f : 0.10f);
    spr.fillRoundRect(x + 7, yy + 6, 12, 3, 1, C(lit ? DN_BG : col));   // small tag in the key's colour
    deckLabel(k.label, x, yy, w, h, lit ? DN_BG : blend(col, 0xFFFFFF, 0.55f));
  }
  if (deckFlash >= 0 && millis() - deckFlashMs < 300) dirty = true;   // (the key lights up for a moment)
}
void deckTap(int x, int y) {
  if (backHit(x, y)) { deckLeave(); return; }
  if (hitR(x, y, W - 70, HDR_H + 4, 64, 26)) { deckVia = deckVia == DV_USB ? DV_BT : DV_USB; prefs.putUChar("deckVia", deckVia); deckNote = ""; deckStartVia(); return; }
  if (hitR(x, y, W - 70, HDR_H + 36, 64, 24)) { deckPage = 1 - deckPage; dirty = true; return; }
  for (int s = 0; s < DECK_PER_PAGE; s++) { int kx, ky, kw, kh; deckKeyRect(s, kx, ky, kw, kh); if (hitR(x, y, kx, ky, kw, kh)) { deckSend(deckPage * DECK_PER_PAGE + s); return; } }
}
