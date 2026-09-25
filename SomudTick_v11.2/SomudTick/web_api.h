#pragma once
// Phone web page: every /api/... answer, photo upload, SD tools, PIN.
// Part of SomudTick (included from SomudTick.ino, in the order listed there).

// ---------------- เว็บ ----------------
void sendJson(JsonDocument& d) { String s; serializeJson(d, s); server.send(200, "application/json; charset=utf-8", s); }
void apiState() {
  JsonDocument d;
  d["now"] = (uint32_t)nowT(); d["approx"] = timeApprox; d["place"] = String(place);
  d["auto"] = autoPlace; d["bat"] = batV; d["sta"] = staOk();
  d["ip"] = staOk() ? WiFi.localIP().toString() : String("");
  JsonArray arr = d["acts"].to<JsonArray>();
  for (auto& a : acts) {
    JsonObject o = arr.add<JsonObject>();
    Sum s = sumFor(a.id);
    o["id"] = a.id; o["name"] = a.name; o["unit"] = a.unit; o["color"] = colorHex(a.color);
    o["step"] = a.step; o["goal"] = a.goal; o["type"] = a.goalType; o["remind"] = a.remind;
    o["count"] = s.count; o["sum"] = s.sum;
    o["last"] = lastSeen.count(a.id) ? lastSeen[a.id] : 0;
    o["state"] = goalState(a, measure(a, s)); o["overdue"] = overdue(a);
  }
  sendJson(d);
}
int actIndex(const String& id) { for (size_t i = 0; i < acts.size(); i++) if (acts[i].id == id) return i; return -1; }
void apiLog() {
  int i = actIndex(server.arg("id"));
  if (i < 0) { server.send(404, "text/plain", "no act"); return; }
  if (server.hasArg("v")) { float v = server.arg("v").toFloat(); if (v <= 0) { server.send(400, "text/plain", "bad v"); return; } logValue(i, v); }
  else logDefault(i);
  refreshStatsIfVisible();
  apiState();
}
void apiUndo() {
  int i = actIndex(server.arg("id"));
  undoEvent(i);
  refreshStatsIfVisible();
  apiState();
}
void apiActsGet() { JsonDocument d; actsToJson(d.to<JsonArray>()); sendJson(d); }
void apiActsPost() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain")) || !d.is<JsonArray>() || !actsFromJson(d.as<JsonArrayConst>())) {
    server.send(400, "text/plain", "invalid"); return;
  }
  saveActs(); scrollY = 0; statSel = 0;
  refreshStatsIfVisible();
  dirty = true;
  apiActsGet();
}
void apiStats() {
  int days = constrain(server.arg("days").toInt(), 1, 90);
  computeStats(days, true);
  JsonDocument d;
  JsonArray k = d["days"].to<JsonArray>(); for (auto& s : stKeys) k.add(s);
  JsonArray arr = d["acts"].to<JsonArray>();
  for (size_t i = 0; i < acts.size(); i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = acts[i].id;
    JsonArray dl = o["daily"].to<JsonArray>(); for (float v : st[i].daily) dl.add(v);
    JsonArray dc = o["dcount"].to<JsonArray>(); for (int v : st[i].dcnt) dc.add(v);
    JsonArray hr = o["hours"].to<JsonArray>(); for (int h = 0; h < 24; h++) hr.add(st[i].hours[h]);
    o["total"] = st[i].total; o["totalCount"] = st[i].totalCnt;
    o["gapMin"] = st[i].gapN ? st[i].gapSum / st[i].gapN / 60.0 : 0;
  }
  refreshStatsIfVisible();   // คืนค่าสถิติของหน้าจอบอร์ด
  sendJson(d);
}
String csvRow(const Ev& e) {   // one line of the CSV export (web page and USB drive)
    int i = actIndex(e.id);
    time_t tt = e.t; struct tm tm; localtime_r(&tt, &tm);
    char dt[24]; strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S", &tm);
    String name = i >= 0 ? acts[i].name : (knownNames.count(e.id) ? knownNames[e.id] + " (ลบแล้ว)" : String("(ลบแล้ว)"));
    name.replace("\"", "\"\"");
    String row = String(dt) + "," + e.t + "," + e.id + ",\"" + name + "\"," + fmtNum(e.v) + "," +
                 (i >= 0 ? acts[i].unit : String("")) + "," + (e.place == 'U' ? "มอ" : "บ้าน") + "," + (e.ok ? 1 : 0) + "\n";
    return row;
}
void exportFile(const String& key, File& f) {
  (void)key;
  while (f.available()) {
    String l = f.readStringUntil('\n'); Ev e;
    if (parseLine(l, e)) server.sendContent(csvRow(e));
  }
}
void apiExport() {
  int days = server.arg("days").toInt();   // 0 = ทั้งหมด
  server.sendHeader("Content-Disposition", "attachment; filename=\"somudtick_" + curDay + ".csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent("\xEF\xBB\xBF" "datetime,epoch,activity_id,activity_name,value,unit,place,time_ok\n");
  std::vector<String> keys;
  if (days > 0) {
    time_t n = nowT();
    for (int d = days - 1; d >= 0; --d) keys.push_back(dayKey(n - (time_t)d * 86400));
  } else {
    File root = logFs().open("/log");
    File f = root.openNextFile();
    while (f) { String nm = f.name(); nm.replace(".csv", ""); if (!nm.endsWith(".new")) keys.push_back(nm); f.close(); f = root.openNextFile(); }
    root.close();
    std::sort(keys.begin(), keys.end());
  }
  for (auto& k : keys) {
    if (!logFs().exists(logPath(k))) continue;
    File f = logFs().open(logPath(k), "r");
    if (f) { exportFile(k, f); f.close(); }
  }
  server.sendContent("");
}
void apiTime() {
  uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
  if (t > 1700000000UL) {
    bool wasApprox = timeApprox;
    setClock(t, true);
    if (wasApprox || dayKey(nowT()) != curDay) loadDay();
    dirty = true;
  }
  apiState();
}
void apiPlace() {
  if (server.hasArg("p")) place = server.arg("p") == "U" ? 'U' : 'H';
  if (server.hasArg("auto")) autoPlace = server.arg("auto") == "1";
  prefs.putChar("place", place); prefs.putBool("auto", autoPlace);
  dirty = true;
  apiState();
}
void apiWifiGet() {
  JsonDocument d;
  d["appass"] = apPass;
  d["ssid"] = staSsid; d["home"] = homeSsid; d["uni"] = uniSsid; d["auto"] = autoPlace; d["sta"] = staOk();
  d["ip"] = staOk() ? WiFi.localIP().toString() : String("");
  sendJson(d);
}
void apiWifiPost() {
  staSsid = server.arg("ssid"); if (server.hasArg("pass") && server.arg("pass") != "********") staPass = server.arg("pass");
  homeSsid = server.arg("home"); uniSsid = server.arg("uni");
  if (server.hasArg("auto")) autoPlace = server.arg("auto") == "1";
  prefs.putString("ssid", staSsid); prefs.putString("pass", staPass);
  prefs.putString("home", homeSsid); prefs.putString("uni", uniSsid); prefs.putBool("auto", autoPlace);
  bool apChanged = false;
  if (server.hasArg("appass")) {
    String np = server.arg("appass"); np.trim();
    if (np.length() >= 8 && np.length() <= 63 && np != apPass) { apPass = np; prefs.putString("appass", apPass); apChanged = true; }
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  WiFi.disconnect();
  if (staSsid.length()) WiFi.begin(staSsid.c_str(), staPass.c_str());
  if (apChanged) { WiFi.softAPdisconnect(false); WiFi.softAP(AP_SSID, apPass.c_str()); }
  dirty = true;
}
// ---- photos & videos sent from the phone (already made small by the phone) ----
File upFile; String upErr;
String safeFileName(const String& n) {
  String o;
  for (char c : n) if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') o += c;
  if (o.length() < 3 || o[0] == '.') o = "file_" + String(millis()) + o;
  return o.substring(0, 60);
}
void apiUploadData() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upErr = "";
    if (!pinOk()) { upErr = "PIN"; return; }   // no PIN: nothing is written
    String dir = server.arg("dir");   // any folder on the card (not the Trash)
    if (!dir.startsWith("/") || dir.indexOf("..") >= 0 || dir.startsWith(TRASH_DIR)) dir = "/photos";
    if (!sdMount()) { upErr = "No SD card in the board"; return; }
    if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
    upFile = SD_MMC.open(joinPath(dir, fsFreeName(dir, safeFileName(u.filename))), FILE_WRITE);   // same name: "cat (2).jpg", never write over
    if (!upFile) upErr = "Can't write to the SD card";
    lastTouchMs = millis();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upFile && upFile.write(u.buf, u.currentSize) != u.currentSize) upErr = "SD card full?";
  } else if (u.status == UPLOAD_FILE_END) {
    if (upFile) upFile.close();
    if (scr == S_FILES) { filesLoad(); dirty = true; }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (upFile) { String pth = String(upFile.path()); upFile.close(); SD_MMC.remove(pth); }
    upErr = "Upload stopped";
  }
}
void apiUploadDone() {
  if (upErr.length()) server.send(500, "text/plain", upErr); else server.send(200, "text/plain", "ok");
}
// list one folder: /api/sd?dir=/photos   (dir=/.trash shows the Trash)
bool webPathOk(const String& p) { return p.startsWith("/") && p.indexOf("..") < 0 && p.indexOf("//") < 0; }
void apiSdList() {
  JsonDocument d;
  d["ok"] = sdMount();
  if (sdOk) {
    String dir = server.hasArg("dir") ? server.arg("dir") : String("/");
    if (!webPathOk(dir) || !fsIsDir(dir)) dir = dir == TRASH_DIR ? String(TRASH_DIR) : String("/");
    bool tr = dir == TRASH_DIR;
    d["total"] = (double)SD_MMC.totalBytes(); d["used"] = (double)SD_MMC.usedBytes();
    d["dir"] = dir;
    int tn; uint32_t tb; fsTrashInfo(tn, tb); d["trash"] = tn; d["trashBytes"] = tb;
    std::vector<std::pair<String, String>> idx; if (tr) idx = trashIndex();
    JsonArray a = d["items"].to<JsonArray>();
    File dd = SD_MMC.open(dir);
    if (dd) {
      File f = dd.openNextFile();
      while (f && a.size() < 300) {
        String n = baseOf(f.path());
        if (n.length() && n[0] != '.' && n != "System Volume Information") {
          String e = lowerExt(n);
          char t = f.isDirectory() ? 'd' : isImg(e) ? 'i' : isVid(e) ? 'v' : 0;
          if (t && !(tr && t == 'd')) {
            JsonObject o = a.add<JsonObject>();
            o["n"] = n; o["t"] = String(t); o["s"] = (uint32_t)f.size();
            if (tr) { o["show"] = trashShowName(n); String from = "/"; for (auto& x : idx) if (x.first == n) from = x.second; o["from"] = from; }
          }
        }
        f.close(); f = dd.openNextFile();
      }
      dd.close();
    }
  }
  sendJson(d);
}
void apiSdDirs() {   // every folder (for "Move to")
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  if (sdMount()) { std::vector<String> v; fsAllDirs("/", v, 0); for (auto& x : v) a.add(x); }
  sendJson(d);
}
// one action: op = trash | restore | purge | empty | mkdir | rename | move
void apiSdOp() {
  String op = server.arg("op"), path = server.arg("path"), name = server.arg("name"), dest = server.arg("dest");
  if ((path.length() && !webPathOk(path)) || (dest.length() && !webPathOk(dest))) { server.send(400, "application/json", "{\"err\":\"fail\"}"); return; }
  // files in the Trash only come out with "restore" (it also updates the Trash list); nothing is moved into it by hand
  if ((op == "trash" || op == "rename" || op == "move") && (path.startsWith(TRASH_DIR) || dest.startsWith(TRASH_DIR))) { server.send(400, "application/json", "{\"err\":\"missing\"}"); return; }
  String e;
  if (op == "trash") e = fsTrash(path);
  else if (op == "restore") e = fsRestore(name);
  else if (op == "purge") e = fsPurge(name);
  else if (op == "empty") fsEmptyTrash();
  else if (op == "mkdir") e = fsMkdir(path, name);
  else if (op == "rename") e = fsRename(path, name);
  else if (op == "move") e = fsMove(path, dest);
  else e = "fail";
  if (scr == S_FILES) { fmUi = FU_LIST; filesLoad(); dirty = true; }
  if (e.length()) server.send(400, "application/json", "{\"err\":\"" + e + "\"}");
  else server.send(200, "application/json", "{\"ok\":true}");
}
void apiSdDel() {   // old web page: "delete" = move to Trash
  String pth = server.arg("path");
  if (!webPathOk(pth)) { server.send(400, "text/plain", "bad path"); return; }
  fsTrash(pth);
  if (scr == S_FILES) { filesLoad(); dirty = true; }
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- web PIN ----
// Every request except the page itself needs the PIN: the phone keeps it in a cookie "stpin".
// 5 wrong tries = locked for 1 minute (4 numbers are only 10000 choices).
void newWebPin() { char b[8]; snprintf(b, sizeof b, "%04u", (unsigned)(esp_random() % 10000)); webPin = b; prefs.putString("pin", webPin); dirty = true; }
int pinFails = 0; uint32_t pinLockUntil = 0;
bool pinOk() {   // no answer sent (the upload uses this)
  String c = server.header("Cookie");
  int i = c.indexOf("stpin=");
  String got = i >= 0 ? c.substring(i + 6, i + 10) : server.arg("pin");
  return webPin.length() == 4 && got == webPin;
}
bool webAuth() {
  if (pinLockUntil && millis() < pinLockUntil) { server.send(429, "text/plain", "locked"); return false; }
  if (pinOk()) { pinFails = 0; return true; }
  bool tried = server.header("Cookie").indexOf("stpin=") >= 0 || server.hasArg("pin");
  if (tried && ++pinFails >= 5) { pinFails = 0; pinLockUntil = millis() + 60000; }
  server.send(401, "text/plain", "pin");
  return false;
}
std::function<void()> guard(void (*fn)()) { return [fn] { if (webAuth()) fn(); }; }

void setupWeb() {
  const char* hk[] = {"Cookie"};
  server.collectHeaders(hk, 1);
  server.on("/upload", HTTP_POST, guard(apiUploadDone), apiUploadData);
  server.on("/api/sd", HTTP_GET, guard(apiSdList));
  server.on("/api/sd/del", HTTP_POST, guard(apiSdDel));
  server.on("/api/sd/dirs", HTTP_GET, guard(apiSdDirs));
  server.on("/api/sd/op", HTTP_POST, guard(apiSdOp));
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/pin", HTTP_GET, guard([] { server.send(200, "application/json", "{\"ok\":true}"); }));
  server.on("/api/state", HTTP_GET, guard(apiState));
  server.on("/api/log", HTTP_POST, guard(apiLog));
  server.on("/api/undo", HTTP_POST, guard(apiUndo));
  server.on("/api/acts", HTTP_GET, guard(apiActsGet));
  server.on("/api/acts", HTTP_POST, guard(apiActsPost));
  server.on("/api/stats", HTTP_GET, guard(apiStats));
  server.on("/api/time", HTTP_POST, guard(apiTime));
  server.on("/api/place", HTTP_POST, guard(apiPlace));
  server.on("/api/wifi", HTTP_GET, guard(apiWifiGet));
  server.on("/api/wifi", HTTP_POST, guard(apiWifiPost));
  server.on("/export.csv", HTTP_GET, guard(apiExport));
  server.onNotFound([] { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
}
