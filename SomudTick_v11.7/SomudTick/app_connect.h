#pragma once
// ============================================================================
//  Wi-Fi page: find networks, type the password on the screen, connect.
//  Bluetooth page: find BLE devices nearby (this chip has BLE only, so it can
//  NOT play music to speakers or headphones).
//  Included from SomudTick.ino.
// ============================================================================
#include <BLEDevice.h>
#include <BLEScan.h>

// ---------------- on-screen keyboard ----------------
String kbdTitle, kbdText;
bool kbdShow = false;
int kbdLayer = 0;                 // 0 abc, 1 ABC, 2 123, 3 #+= (more symbols, v11.5)
Screen kbdBack = S_HOME;
void (*kbdDone)(const String&) = nullptr;
const char* KB_ROWS[4][3] = {
  {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
  {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
  {"1234567890", "-/:;()$&@\"", ".,?!'_"},
  {"[]{}#%^*+=", "_\\|~<>`&@", ".,?!'\""}};   // Wi-Fi passwords can have any of these
void kbdOpen(const String& title, const String& start, void (*done)(const String&)) {
  kbdTitle = title; kbdText = start; kbdDone = done; kbdBack = scr; kbdLayer = 0; kbdShow = false;
  scr = S_KBD; dirty = true;
}
struct KbGeo { int kw, kh, y0; };
KbGeo kbGeo() { int kh = land() ? 34 : 38; return {W / 10, kh, H - 4 * kh - 4}; }
// find the key under (x, y). Returns the character, or a code: 1 shift, 2 123, 3 del, 4 ok, 5 space
int kbKeyAt(int x, int y, int* kx = nullptr, int* kyy = nullptr, int* kww = nullptr) {
  KbGeo g = kbGeo();
  if (y < g.y0) return 0;
  int r = (y - g.y0) / g.kh;
  if (r < 3) {
    const char* row = KB_ROWS[kbdLayer][r];
    int n = strlen(row), off = (W - n * g.kw) / 2;
    int c = (x - off) / g.kw;
    if (x < off || c >= n) return 0;
    return row[c];
  }
  // bottom row: shift 1.5, 123 1.5, space 4, del 1.5, ok 1.5
  float u = x / (float)g.kw;
  if (u < 1.5f) return 1; if (u < 3) return 2; if (u < 7) return 5; if (u < 8.5f) return 3; return 4;
}
void drawKbd() {
  KbGeo g = kbGeo();
  txt(FB, fitText(FB, kbdTitle, W - 86), 8, HDR_H + 8, INK);
  int by = HDR_H + 32;
  spr.fillRoundRect(6, by, W - 12, 34, 8, C(CARD));
  spr.drawRoundRect(6, by, W - 12, 34, 8, C(INK));
  navAdd(6, by, W - 12, 34);   // tap = show / hide
  String shown = kbdShow ? kbdText : String("");
  if (!kbdShow) for (size_t i = 0; i < kbdText.length(); i++) shown += '*';
  txt(FB, fitText(FB, shown + "_", W - 90), 14, by + 17, INK, D_ML);
  txt(FS, kbdShow ? "hide" : "show", W - 14, by + 17, SOFT, D_MR);
  for (int r = 0; r < 3; r++) {
    const char* row = KB_ROWS[kbdLayer][r];
    int n = strlen(row), off = (W - n * g.kw) / 2, y = g.y0 + r * g.kh;
    for (int c = 0; c < n; c++) {
      int x = off + c * g.kw;
      navAdd(x + 1, y + 2, g.kw - 2, g.kh - 4);
      spr.fillRoundRect(x + 1, y + 2, g.kw - 2, g.kh - 4, 5, C(CARD));
      spr.drawRoundRect(x + 1, y + 2, g.kw - 2, g.kh - 4, 5, C(LINE));
      char s[2] = {row[c], 0};
      txt(FB, s, x + g.kw / 2, y + g.kh / 2, INK, D_MC);
    }
  }
  int y = g.y0 + 3 * g.kh;
  struct { float a, b; const char* t; bool dark; } bot[] = {
    {0, 1.5f, kbdLayer == 0 ? "Aa" : "abc", kbdLayer == 1}, {1.5f, 3, kbdLayer == 2 ? "#+=" : "123", kbdLayer >= 2},
    {3, 7, "space", false}, {7, 8.5f, "Del", false}, {8.5f, 10, "OK", true}};
  for (auto& k : bot) {
    int x0 = k.a * g.kw, x1 = k.b * g.kw;
    navAdd(x0 + 1, y + 2, x1 - x0 - 2, g.kh - 4);
    spr.fillRoundRect(x0 + 1, y + 2, x1 - x0 - 2, g.kh - 4, 5, C(k.dark ? INK : KEYBG));
    txt(FS, fitText(FS, k.t, x1 - x0 - 4), (x0 + x1) / 2, y + g.kh / 2, k.dark ? ONINK : INK, D_MC);
  }
  // cancel
  spr.fillRoundRect(W - 70, HDR_H + 4, 64, 22, 8, C(KEYBG));
  navAdd(W - 70, HDR_H + 4, 64, 22, NK_BACK);
  txt(FS, "Cancel", W - 38, HDR_H + 15, INK, D_MC);
}
void kbdTap(int x, int y) {
  if (y >= HDR_H && y < HDR_H + 28 && x > W - 74) { scr = kbdBack; dirty = true; return; }
  if (y >= HDR_H + 32 && y < HDR_H + 66) { kbdShow = !kbdShow; dirty = true; return; }
  int k = kbKeyAt(x, y);
  if (!k) return;
  if (k == 1) kbdLayer = kbdLayer == 0 ? 1 : 0;           // Aa: abc <-> ABC; from the symbols: back to abc
  else if (k == 2) kbdLayer = kbdLayer == 2 ? 3 : 2;      // 123 -> #+= -> 123
  else if (k == 3) { if (kbdText.length()) kbdText.remove(kbdText.length() - 1); }
  else if (k == 5) { if (kbdText.length() < 63) kbdText += ' '; }
  else if (k == 4) { scr = kbdBack; if (kbdDone) kbdDone(kbdText); }
  else if (kbdText.length() < 63) kbdText += (char)k;
  dirty = true;
}

// ---------------- Wi-Fi networks ----------------
struct WNet { String ssid; int rssi; bool lock; };
std::vector<WNet> wnets;
bool wScanning = false;
uint32_t wConnT = 0;          // when we started connecting
String wPickSsid;
int wifiScroll = 0, wifiMax = 0;

void wifiScanStart() {
  if (!staOn) { staOn = true; prefs.putBool("sta", true); setupWifi(); }
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  wScanning = true; dirty = true;
}
void wifiScanPoll() {
  if (!wScanning) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wScanning = false;
  wnets.clear();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (auto& w : wnets) if (w.ssid == s) { dup = true; if (WiFi.RSSI(i) > w.rssi) w.rssi = WiFi.RSSI(i); }
    if (!dup) wnets.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  WiFi.scanDelete();
  std::sort(wnets.begin(), wnets.end(), [](const WNet& a, const WNet& b) { return a.rssi > b.rssi; });
  dirty = true;
}
void wifiConnect(const String& ssid, const String& pass) {
  staSsid = ssid; staPass = pass;
  prefs.putString("ssid", staSsid); prefs.putString("pass", staPass);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(staSsid.c_str(), staPass.c_str());
  wConnT = millis();
  dirty = true;
}
void wifiPassDone(const String& pw) { wifiConnect(wPickSsid, pw); }
void wifiPageOpen() { scr = S_WIFI; wifiScroll = 0; wifiScanStart(); }
void drawBars(int x, int y, int rssi, uint32_t col) {
  int lv = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
  for (int i = 0; i < 4; i++) spr.fillRect(x + i * 5, y + 12 - i * 4, 3, 4 + i * 4, C(i < lv ? col : LINE));
}
int wifiTop() { return HDR_H + 36 + 62; }
void drawWifi() {
  wifiScanPoll();
  drawAppTitle("Wi-Fi", "");
  btn(W - 70, HDR_H + 4, 64, 26, wScanning ? "..." : "Scan", false);
  // status card
  int y = HDR_H + 36;
  spr.fillRoundRect(6, y, W - 12, 56, 10, C(CARD));
  String st, st2;
  if (WiFi.status() == WL_CONNECTED) { st = "Connected: " + WiFi.SSID(); st2 = "Wi-Fi OK   IP " + WiFi.localIP().toString();   /* joined Wi-Fi (the internet itself is not tested here) */ }
  else if (wConnT && millis() - wConnT < 15000) { st = "Connecting to " + staSsid + "..."; st2 = "Please wait"; }
  else if (wConnT) { st = "Could not connect"; st2 = "Wrong password? Tap the network again."; }
  else if (staSsid.length()) { st = "Not connected"; st2 = "Saved: " + staSsid; }
  else { st = "Not connected"; st2 = "Tap a network below to join it"; }
  txt(FB, fitText(FB, st, W - 30), 14, y + 8, INK);
  txt(FS, fitText(FS, st2, W - 30), 14, y + 30, SOFT);
  int top = wifiTop();
  wifiScroll = constrain(wifiScroll, 0, wifiMax);
  spr.setClipRect(0, top, W, FTR_Y - top);
  int yy = top - wifiScroll;
  if (wScanning && wnets.empty()) txt(FS, "Looking for Wi-Fi...", 14, yy + 6, SOFT);
  for (auto& w : wnets) {
    bool cur = WiFi.status() == WL_CONNECTED && WiFi.SSID() == w.ssid;
    navAdd(6, yy, W - 12, 34);
    spr.fillRoundRect(6, yy, W - 12, 34, 8, C(cur ? blend(CARD, INK, 0.12f) : CARD));
    drawBars(14, yy + 9, w.rssi, INK);
    txt(cur ? FB : FS, fitText(FS, w.ssid, W - 90), 42, yy + 17, INK, D_ML);
    if (w.lock) { spr.drawRoundRect(W - 30, yy + 10, 10, 9, 3, C(SOFT)); spr.fillRect(W - 32, yy + 16, 14, 10, C(SOFT)); }
    yy += 38;
  }
  wifiMax = max(0, (int)wnets.size() * 38 - (FTR_Y - top));
  scrollBar(top, FTR_Y - top, wifiScroll, wifiMax);
  spr.clearClipRect();
}
void wifiTap(int x, int y) {
  if (backHit(x, y)) { scr = S_SET; dirty = true; return; }
  if (y >= HDR_H && y < HDR_H + 32 && x > W - 74) { wifiScanStart(); return; }
  int top = wifiTop();
  if (y < top) return;
  int i = (y - top + wifiScroll) / 38;
  if (i < 0 || i >= (int)wnets.size()) return;
  wPickSsid = wnets[i].ssid;
  if (!wnets[i].lock) wifiConnect(wPickSsid, "");
  else kbdOpen("Password: " + wPickSsid, wPickSsid == staSsid ? staPass : String(""), wifiPassDone);
}

// ---------------- Bluetooth (BLE scan) ----------------
struct BDev { String name, addr; int rssi; };
std::vector<BDev> bdevs;
int btScroll = 0, btMax = 0;
bool btScanned = false;
volatile bool btBusy = false, btDone = false;   // the scan runs in the background (5 s); the screen keeps working
uint32_t btStartMs = 0;
void btScanDone(BLEScanResults) { btDone = true; }
void btScan() {
  if (btBusy) return;
  if (!BLEDevice::getInitialized()) BLEDevice::init("SomudTick");
  BLEScan* sc = BLEDevice::getScan();
  sc->setActiveScan(true);
  sc->setInterval(100); sc->setWindow(99);
  sc->clearResults();
  btBusy = true; btDone = false; btStartMs = millis(); dirty = true;
  if (!sc->start(5, btScanDone, false)) { btBusy = false; }
}
void btCollect() {   // from loop() (v11.7; was only from drawBt(): leaving the page kept the Bluetooth memory): the scan finished -> make the list
  if (!btDone) return;
  btDone = false; btBusy = false;
  BLEScanResults* r = BLEDevice::getScan()->getResults();
  BLEScan* sc = BLEDevice::getScan();
  bdevs.clear();
  for (int i = 0; r && i < r->getCount(); i++) {
    BLEAdvertisedDevice d = r->getDevice(i);
    bdevs.push_back({d.haveName() ? String(d.getName().c_str()) : String(""), String(d.getAddress().toString().c_str()), d.getRSSI()});
  }
  sc->clearResults();
  // v11.5: give the Bluetooth memory back (about 90 KB). It stayed taken until a restart, and then the news could
  // not be loaded any more (HTTPS needs that memory). The next Scan starts Bluetooth again.
  BLEDevice::deinit(false);
  std::sort(bdevs.begin(), bdevs.end(), [](const BDev& a, const BDev& b) {
    if (a.name.length() != 0 && b.name.length() == 0) return true;
    if (a.name.length() == 0 && b.name.length() != 0) return false;
    return a.rssi > b.rssi; });
  btScanned = true; lastTouchMs = millis(); dirty = true;
}
void btPageOpen() { scr = S_BT; btScroll = 0; dirty = true; }
void drawBt() {
  btCollect();
  drawAppTitle("Bluetooth", "");
  btn(W - 70, HDR_H + 4, 64, 26, btBusy ? "..." : "Scan", false);
  int y = HDR_H + 36;
  spr.fillRoundRect(6, y, W - 12, 44, 10, C(CARD));
  String st = btBusy ? "Scanning... " + String(max(0, 5 - (int)((millis() - btStartMs) / 1000))) + " s" : btScanned ? String(bdevs.size()) + " devices nearby" : String("Tap Scan to find devices");
  txt(FS, fitText(FS, st, W - 28), 14, y + 6, INK);
  txt(FS, fitText(FS, "BLE only - no speakers", W - 28), 14, y + 24, SOFT);
  int top = y + 50;
  btScroll = constrain(btScroll, 0, btMax);
  spr.setClipRect(0, top, W, FTR_Y - top);
  int yy = top - btScroll;
  for (auto& d : bdevs) {
    spr.fillRoundRect(6, yy, W - 12, 40, 8, C(CARD));
    drawBars(14, yy + 12, d.rssi, INK);
    txt(FB, fitText(FB, d.name.length() ? d.name : String("(no name)"), W - 42 - 80), 42, yy + 5, d.name.length() ? INK : SOFT);
    txt(FS, fitText(FS, d.addr, W - 54), 42, yy + 22, SOFT);
    txt(FS, String(d.rssi) + " dBm", W - 12, yy + 6, SOFT, D_TR);
    yy += 44;
  }
  btMax = max(0, (int)bdevs.size() * 44 - (FTR_Y - top));
  scrollBar(top, FTR_Y - top, btScroll, btMax);
  spr.clearClipRect();
}
void btTap(int x, int y) {
  if (backHit(x, y)) { scr = S_SET; dirty = true; return; }
  if (y >= HDR_H && y < HDR_H + 32 && x > W - 74) btScan();
}
