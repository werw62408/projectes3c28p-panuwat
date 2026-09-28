#pragma once
// Wi-Fi: the board hotspot, joining home Wi-Fi, auto Home/Uni by Wi-Fi name.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- Wi-Fi ----------------
// Two separate switches:
//   apOn  = board makes its own Wi-Fi "SomudTick" (phone joins it to open the web page)
//   staOn = board joins your home Wi-Fi / phone hotspot (internet: time, weather, news)
void setupWifi() {
  static bool mdnsUp = false;
  bool ap = apOn && !apAsleep;   // apAsleep: hotspot resting while the screen is off (see core_power.h)
  if (!ap && !staOn) { WiFi.mode(WIFI_OFF); return; }
  WiFi.mode(ap && staOn ? WIFI_AP_STA : ap ? WIFI_AP : WIFI_STA);
  if (ap) WiFi.softAP(AP_SSID, apPass.c_str());
  if (staOn) {
    WiFi.setAutoReconnect(true);
    if (staSsid.length() && WiFi.status() != WL_CONNECTED) WiFi.begin(staSsid.c_str(), staPass.c_str());
  }
  if (!mdnsUp) { mdnsUp = MDNS.begin(MDNS_NAME); if (mdnsUp) MDNS.addService("http", "tcp", 80); }
}
bool staOk() { return staOn && WiFi.status() == WL_CONNECTED; }

void autoPlaceTask() {
  static uint32_t lastScan = 0;
  if (!autoPlace || !staOn || (!homeSsid.length() && !uniSsid.length())) return;
  int n = WiFi.scanComplete();
  if (n >= 0) {
    bool home = false, uni = false;
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (homeSsid.length() && s == homeSsid) home = true;
      if (uniSsid.length() && s == uniSsid) uni = true;
    }
    WiFi.scanDelete();
    char np = place;
    if (home && !uni) np = 'H'; else if (uni && !home) np = 'U';
    if (np != place) { place = np; prefs.putChar("place", place); dirty = true; }
  } else if (n == WIFI_SCAN_FAILED && millis() - lastScan > 60000UL) {
    lastScan = millis();
    WiFi.scanNetworks(true);
  }
}
