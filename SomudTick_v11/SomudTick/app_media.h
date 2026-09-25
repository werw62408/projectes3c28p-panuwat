#pragma once
// ============================================================================
//  Files app: browse the microSD card, view photos, play videos with sound.
//  Videos must be converted on a computer first with "convert_for_SomudTick.bat"
//  (makes NAME.mjpeg = pictures + NAME.pcm = sound).
//  Included from SomudTick.ino (uses its globals and drawing helpers).
// ============================================================================
#include <SD_MMC.h>
#include <ESP_I2S.h>

// SD card pins (ES3C28P, 4-bit SDMMC)
#define SD_CLK 38
#define SD_CMD 40
#define SD_D0  39
#define SD_D1  41
#define SD_D2  48
#define SD_D3  47
// Audio (ES8311 codec + amplifier)
#define AUD_MCLK 4
#define AUD_BCLK 5
#define AUD_DIN  6   // mic data (not used)
#define AUD_WS   7
#define AUD_DOUT 8
#define AUD_EN   1   // amplifier on = LOW
uint8_t esAddr = 0x18;   // ES8311 I2C address (0x18, some boards 0x19)
#define TOUCH_I2C_PORT 1   // codec shares the I2C bus with the touch chip
#define VIDEO_FPS 15       // must match the converter
#define AUDIO_RATE 16000   // must match the converter

bool sdOk = false;
bool usbDriveOn = false;   // USB drive mode: the computer owns the card, the board must not touch it
enum FType : uint8_t { FT_DIR, FT_IMG, FT_VID, FT_TRASH, FT_USB };
struct FEntry { String name; uint32_t size; FType type; String show; };   // show = name to draw (Trash hides the number in front)
std::vector<FEntry> fList;
String curDir = "/";
String filesMsg;
int viewIdx = -1;

// ---------------- SD ----------------
bool sdMount() {
  if (usbDriveOn) return false;
  if (sdOk && SD_MMC.cardType() != CARD_NONE) return true;
  SD_MMC.end();
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  sdOk = SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT);
  if (!sdOk) {   // try 1-bit mode
    SD_MMC.end();
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
    sdOk = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  }
  return sdOk;
}
String baseNoExt(const String& n);   // below
String lowerExt(const String& n) { int d = n.lastIndexOf('.'); if (d < 0) return ""; String e = n.substring(d + 1); e.toLowerCase(); return e; }
bool isImg(const String& e) { return e == "jpg" || e == "jpeg" || e == "png" || e == "bmp"; }
bool isVid(const String& e) { return e == "mjpeg" || e == "mjpg"; }
String joinPath(const String& dir, const String& name) { return dir == "/" ? "/" + name : dir + "/" + name; }
String dirOf(const String& p) { int i = p.lastIndexOf('/'); return i <= 0 ? String("/") : p.substring(0, i); }
String baseOf(const String& p) { return p.substring(p.lastIndexOf('/') + 1); }

// ============================================================================
//  File tools (used by the board screen AND the phone web page)
//  - "Delete" moves a file to the Trash (/.trash). It can be put back later.
//  - A video is 2 files: NAME.mjpeg (pictures) + NAME.pcm (sound). They always move together.
//  Every function returns "" when OK, or a short error code (see fsErrText).
// ============================================================================
#define TRASH_DIR "/.trash"
#define TRASH_INDEX "/.trash/.index"   // lines: <name in trash> TAB <folder it came from>
const char* fsErrText(const String& e) {   // board text (the web page has its own Thai text)
  if (e == "nocard") return "No SD card";
  if (e == "badname") return "Use A-Z 0-9 space - _ . ( )";
  if (e == "exists") return "That name is already used";
  if (e == "notempty") return "Folder is not empty";
  if (e == "same") return "It is already there";
  if (e == "inside") return "Can't move a folder into itself";
  if (e == "missing") return "File not found";
  return "Could not do it (card error)";
}
// names: letters, numbers, space, - _ . ( ), 1..40 characters, not starting with a dot
String fsCheckName(String n) {
  n.trim();
  if (!n.length() || n.length() > 40 || n[0] == '.') return "badname";
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    if (!(isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')')) return "badname";
  }
  return "";
}
bool fsIsVideoName(const String& n) { return isVid(lowerExt(n)); }
String fsPcmOf(const String& p) { return p.substring(0, p.lastIndexOf('.')) + ".pcm"; }
bool fsIsDir(const String& p) { File f = SD_MMC.open(p); bool d = f && f.isDirectory(); if (f) f.close(); return d; }
bool fsDirEmpty(const String& p) {
  File d = SD_MMC.open(p); if (!d) return true;
  File f = d.openNextFile(); bool empty = !f; if (f) f.close(); d.close();
  return empty;
}
// rename one file and its sound file (if it is a video)
String fsMovePair(const String& from, const String& to) {
  if (SD_MMC.exists(to)) return "exists";
  bool pcm = fsIsVideoName(from) && SD_MMC.exists(fsPcmOf(from));
  if (pcm && SD_MMC.exists(fsPcmOf(to))) return "exists";
  if (!SD_MMC.rename(from, to)) return "fail";
  if (pcm) SD_MMC.rename(fsPcmOf(from), fsPcmOf(to));
  return "";
}
// a free name in a folder: "cat.jpg" -> "cat (2).jpg" if needed
String fsFreeName(const String& dir, const String& name) {
  if (!SD_MMC.exists(joinPath(dir, name))) return name;
  int d = name.lastIndexOf('.'); String b = d > 0 ? name.substring(0, d) : name, e = d > 0 ? name.substring(d) : String("");
  for (int k = 2; k < 100; k++) { String n = b + " (" + k + ")" + e; if (!SD_MMC.exists(joinPath(dir, n))) return n; }
  return b + "_" + String(millis()) + e;
}
// ---- trash index ----
std::vector<std::pair<String, String>> trashIndex() {
  std::vector<std::pair<String, String>> v;
  File f = SD_MMC.open(TRASH_INDEX, "r");
  if (!f) return v;
  while (f.available()) {
    String l = f.readStringUntil('\n'); int t = l.indexOf('\t');
    if (t > 0) v.push_back({l.substring(0, t), l.substring(t + 1)});
  }
  f.close();
  return v;
}
void trashIndexWrite(const std::vector<std::pair<String, String>>& v) {
  File f = SD_MMC.open(TRASH_INDEX, "w");
  if (!f) return;
  for (auto& x : v) f.print(x.first + "\t" + x.second + "\n");
  f.close();
}
String trashShowName(const String& tn) { int u = tn.indexOf('_'); return u > 0 ? tn.substring(u + 1) : tn; }   // "12_cat.jpg" -> "cat.jpg"
String trashFrom(const String& tn) { for (auto& x : trashIndex()) if (x.first == tn) return x.second; return "/"; }
// ---- the operations ----
String fsTrash(const String& path) {           // "delete": move to the Trash
  if (!sdMount()) return "nocard";
  if (!SD_MMC.exists(path) || path.startsWith(TRASH_DIR)) return "missing";
  if (fsIsDir(path)) {                          // folders: only empty ones, removed right away
    if (!fsDirEmpty(path)) return "notempty";
    return SD_MMC.rmdir(path) ? "" : "fail";
  }
  if (!SD_MMC.exists(TRASH_DIR)) SD_MMC.mkdir(TRASH_DIR);
  uint32_t id = prefs.getUInt("trashN", 0) + 1; prefs.putUInt("trashN", id);
  String tn = String(id) + "_" + baseOf(path);
  String e = fsMovePair(path, joinPath(TRASH_DIR, tn));
  if (e.length()) return e;
  auto v = trashIndex(); v.push_back({tn, dirOf(path)}); trashIndexWrite(v);
  return "";
}
String fsRestore(const String& tn) {           // take a file out of the Trash, back to its folder
  if (!sdMount()) return "nocard";
  String from = trashFrom(tn);
  if (!SD_MMC.exists(from)) SD_MMC.mkdir(from);
  String e = fsMovePair(joinPath(TRASH_DIR, tn), joinPath(from, fsFreeName(from, trashShowName(tn))));
  if (e.length()) return e;
  auto v = trashIndex(); v.erase(std::remove_if(v.begin(), v.end(), [&](const std::pair<String, String>& x) { return x.first == tn; }), v.end()); trashIndexWrite(v);
  return "";
}
String fsPurge(const String& tn) {             // delete forever (one file in the Trash)
  if (!sdMount()) return "nocard";
  String p = joinPath(TRASH_DIR, tn);
  if (fsIsVideoName(tn)) SD_MMC.remove(fsPcmOf(p));
  if (!SD_MMC.remove(p)) return "missing";
  auto v = trashIndex(); v.erase(std::remove_if(v.begin(), v.end(), [&](const std::pair<String, String>& x) { return x.first == tn; }), v.end()); trashIndexWrite(v);
  return "";
}
int fsEmptyTrash(void (*progress)(int) = nullptr) {   // delete everything in the Trash, returns how many files
  if (!sdMount()) return 0;
  std::vector<String> names;
  File d = SD_MMC.open(TRASH_DIR);
  if (d) { File f = d.openNextFile(); while (f) { if (!f.isDirectory()) names.push_back(baseOf(f.path())); f.close(); f = d.openNextFile(); } d.close(); }
  int n = 0;
  for (auto& nm : names) {
    if (SD_MMC.remove(joinPath(TRASH_DIR, nm)) && nm != ".index" && !nm.endsWith(".pcm")) n++;
    if (progress && n % 5 == 0) progress(n);
  }
  return n;
}
void fsTrashInfo(int& count, uint32_t& bytes) {   // files in the Trash (videos count once) and their size
  count = 0; bytes = 0;
  File d = SD_MMC.open(TRASH_DIR); if (!d) return;
  File f = d.openNextFile();
  while (f) {
    String n = baseOf(f.path());
    if (!f.isDirectory() && n != ".index") { bytes += f.size(); if (!n.endsWith(".pcm")) count++; }
    f.close(); f = d.openNextFile();
  }
  d.close();
}
String fsRename(const String& path, String newName) {   // newName without the file type; the type (.jpg ...) stays
  if (!sdMount()) return "nocard";
  newName.trim();
  String e = fsCheckName(newName); if (e.length()) return e;
  bool dir = fsIsDir(path);
  String ext = dir ? String("") : path.substring(path.lastIndexOf('.'));
  if (!dir && lowerExt(newName) == lowerExt(path)) ext = "";   // they typed the type too
  String to = joinPath(dirOf(path), newName + ext);
  if (to == path) return "";
  return dir ? (SD_MMC.exists(to) ? String("exists") : (SD_MMC.rename(path, to) ? String("") : String("fail"))) : fsMovePair(path, to);
}
String fsMove(const String& path, const String& destDir) {
  if (!sdMount()) return "nocard";
  if (dirOf(path) == destDir) return "same";
  if (destDir == path || destDir.startsWith(path + "/")) return "inside";
  String to = joinPath(destDir, baseOf(path));
  if (fsIsDir(path)) return SD_MMC.exists(to) ? String("exists") : (SD_MMC.rename(path, to) ? String("") : String("fail"));
  return fsMovePair(path, to);
}
String fsMkdir(const String& dir, String name) {
  if (!sdMount()) return "nocard";
  name.trim();
  String e = fsCheckName(name); if (e.length()) return e;
  String p = joinPath(dir, name);
  if (SD_MMC.exists(p)) return "exists";
  return SD_MMC.mkdir(p) ? "" : "fail";
}
// all folders on the card (for "Move to"), not the Trash
void fsAllDirs(const String& dir, std::vector<String>& out, int depth) {
  out.push_back(dir);
  if (depth >= 5 || out.size() >= 150) return;
  std::vector<String> kids;
  File d = SD_MMC.open(dir); if (!d) return;
  File f = d.openNextFile();
  while (f) {
    String n = baseOf(f.path());
    if (f.isDirectory() && n.length() && n[0] != '.' && n != "System Volume Information") kids.push_back(joinPath(dir, n));
    f.close(); f = d.openNextFile();
  }
  d.close();
  std::sort(kids.begin(), kids.end());
  for (auto& k : kids) fsAllDirs(k, out, depth + 1);
}

bool inTrash() { return curDir == TRASH_DIR; }
int trashN = 0; uint32_t trashBytes = 0;
void filesLoad() {
  fList.clear(); filesMsg = ""; filesScroll = 0;
  if (!sdMount()) { filesMsg = "No SD card found. Insert a microSD card (FAT32) and tap here to try again."; return; }
  File d = SD_MMC.open(curDir);
  if (!d || !d.isDirectory()) { curDir = "/"; d = SD_MMC.open("/"); }
  File f = d.openNextFile();
  bool tr = inTrash();
  while (f && fList.size() < 300) {
    String n = baseOf(f.path());
    if (n.length() && n[0] != '.' && n != "System Volume Information") {
      if (f.isDirectory()) { if (!tr) fList.push_back({n, 0, FT_DIR, n}); }
      else {
        String e = lowerExt(n);
        String show = tr ? trashShowName(n) : n;
        if (isImg(e)) fList.push_back({n, (uint32_t)f.size(), FT_IMG, show});
        else if (isVid(e)) fList.push_back({n, (uint32_t)f.size(), FT_VID, show});
      }
    }
    f.close();
    f = d.openNextFile();
  }
  d.close();
  std::sort(fList.begin(), fList.end(), [](const FEntry& a, const FEntry& b) {
    if ((a.type == FT_DIR) != (b.type == FT_DIR)) return a.type == FT_DIR;
    String x = a.show, y = b.show; x.toLowerCase(); y.toLowerCase(); return x < y;
  });
  fsTrashInfo(trashN, trashBytes);
  if (curDir == "/" && trashN) fList.push_back({"Trash", trashBytes, FT_TRASH, "Trash"});   // Trash row at the end of the first page
  if (curDir == "/") fList.push_back({"USB drive", 0, FT_USB, "USB drive"});   // last row: use the card from a computer
}
String sizeStr(uint32_t b) {
  if (b >= 1048576) return String(b / 1048576.0f, 1) + " MB";
  return String((b + 1023) / 1024) + " KB";
}

// ---------------- Files screen ----------------
// normal list, or a small window on top: menu (long press), "are you sure?", or the "Move to" folder chooser
enum FmUi { FU_LIST, FU_MENU, FU_CONFIRM, FU_PICK };
FmUi fmUi = FU_LIST;
int fmIdx = -1;            // the file the menu is for
int fmAsk = 0;             // confirm: 1 trash, 2 delete forever, 3 empty trash
String fmToast; uint32_t fmToastT = 0;
String pickDir = "/"; std::vector<String> pickList; int pickScroll = 0;
void fmSay(const String& s) { fmToast = s; fmToastT = millis(); dirty = true; }
void fmResult(const String& err, const String& okText) { fmSay(err.length() ? String(fsErrText(err)) : okText); }
String fmPath(int i) { return joinPath(curDir, fList[i].name); }

const int FROW = 38;
int filesTop() { return HDR_H + 38; }
int filesMax() { return max(0, (int)fList.size() * FROW - (FTR_Y - filesTop())); }
void drawFileIcon(FType t, int x, int cy, uint32_t c) {
  if (t == FT_DIR) { spr.fillRoundRect(x, cy - 8, 10, 4, 1, C(c)); spr.fillRoundRect(x, cy - 5, 22, 14, 2, C(c)); }
  else if (t == FT_IMG) { spr.drawRoundRect(x, cy - 8, 22, 17, 2, C(c)); spr.fillTriangle(x + 3, cy + 6, x + 10, cy - 2, x + 16, cy + 6, C(c)); spr.fillCircle(x + 16, cy - 3, 2, C(c)); }
  else if (t == FT_TRASH) {   // bin
    spr.fillRect(x + 3, cy - 8, 16, 3, C(c)); spr.fillRect(x + 8, cy - 10, 6, 2, C(c));
    spr.fillRoundRect(x + 5, cy - 4, 12, 14, 2, C(c));
    for (int k = 0; k < 2; k++) spr.drawFastVLine(x + 9 + k * 4, cy - 1, 9, C(CARD));
  }
  else if (t == FT_USB) {   // a small plug
    spr.fillRoundRect(x + 4, cy - 4, 14, 12, 2, C(c)); spr.fillRect(x + 7, cy - 9, 3, 5, C(c)); spr.fillRect(x + 12, cy - 9, 3, 5, C(c));
    spr.fillRect(x + 9, cy + 8, 4, 3, C(c));
  }
  else { spr.fillRoundRect(x, cy - 8, 22, 17, 3, C(c)); spr.fillTriangle(x + 8, cy - 4, x + 8, cy + 5, x + 15, cy, C(CARD)); }
}
// window geometry
struct FmBox { int x, y, w, h; };
std::vector<const char*> fmMenuItems() {
  if (inTrash()) return {"Put back", "Delete forever", "Cancel"};
  return {"Rename", "Move", "Delete", "Cancel"};
}
FmBox fmMenuBox() { int w = min(W - 40, 200), n = fmMenuItems().size(), h = 36 + n * 40; return {(W - w) / 2, max(HDR_H + 4, (H - h) / 2), w, h}; }
FmBox fmAskBox() { int w = min(W - 24, 230), h = 132; return {(W - w) / 2, (H - h) / 2, w, h}; }
int pickTop() { return HDR_H + 60; }
int pickBottom() { return FTR_Y - 44; }
void pickLoad() {
  pickList.clear(); pickScroll = 0;
  String moving = fmIdx >= 0 && fmIdx < (int)fList.size() ? fmPath(fmIdx) : String("");
  File d = SD_MMC.open(pickDir); if (!d) return;
  File f = d.openNextFile();
  while (f) {
    String n = baseOf(f.path());
    String p = joinPath(pickDir, n);
    if (f.isDirectory() && n.length() && n[0] != '.' && n != "System Volume Information" && p != moving) pickList.push_back(n);
    f.close(); f = d.openNextFile();
  }
  d.close();
  std::sort(pickList.begin(), pickList.end());
}
void drawWindow(const FmBox& b) {
  spr.fillRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 12, C(INK));
  spr.fillRoundRect(b.x, b.y, b.w, b.h, 10, C(PAPER));
}
void drawFiles() {
  int top = filesTop();
  if (fmUi == FU_PICK) {   // ---- "Move to" folder chooser ----
    drawAppTitle(pickDir == "/" ? String("Card") : baseOf(pickDir));
    txt(FS, fitText(FS, "Move " + fList[fmIdx].show + " to...", W - 20), 10, HDR_H + 38, SOFT);
    int pt = pickTop(), pb = pickBottom();
    int mx = max(0, (int)pickList.size() * FROW - (pb - pt));
    pickScroll = constrain(pickScroll, 0, mx);
    spr.setClipRect(0, pt, W, pb - pt);
    for (size_t i = 0; i < pickList.size(); i++) {
      int y = pt + i * FROW - pickScroll;
      spr.fillRoundRect(6, y, W - 12, FROW - 4, 8, C(CARD));
      drawFileIcon(FT_DIR, 14, y + (FROW - 4) / 2, INK);
      txt(FB, fitText(FB, pickList[i], W - 80), 44, y + (FROW - 4) / 2, INK, D_ML);
      txt(FS, ">", W - 14, y + (FROW - 4) / 2, SOFT, D_MR);
    }
    if (pickList.empty()) txt(FS, "No folders inside", W / 2, pt + 20, SOFT, D_TC);
    scrollBar(pt, pb - pt, pickScroll, mx);
    spr.clearClipRect();
    int bw = (W - 18) / 2;
    btn(6, FTR_Y - 38, bw, 32, "Cancel", false);
    btn(12 + bw, FTR_Y - 38, bw, 32, "Move here", true);
    return;
  }
  String title = inTrash() ? String("Trash") : curDir == "/" ? String("Files") : baseOf(curDir);
  drawAppTitle(title);
  if (sdOk && !filesMsg.length()) {
    if (inTrash()) { if (!fList.empty()) btn(W - 72, HDR_H + 4, 64, 26, "Empty", false); }
    else {   // "new folder" button: + and a folder picture
      btn(W - 72, HDR_H + 4, 64, 26, "", false);
      txt(FB, "+", W - 56, HDR_H + 17, INK, D_MC);
      drawFileIcon(FT_DIR, W - 46, HDR_H + 17, INK);
    }
  }
  if (filesMsg.length()) {
    spr.fillRoundRect(8, top + 10, W - 16, 90, 10, C(CARD));
    String m = filesMsg; int y = top + 20;   // simple word wrap
    while (m.length() && y < top + 96) {
      int cut = m.length();
      spr.setFont(FS);
      while (cut > 0 && spr.textWidth(m.substring(0, cut)) > W - 36) { int sp = m.lastIndexOf(' ', cut - 1); cut = sp > 0 ? sp : cut - 1; }
      txt(FS, m.substring(0, cut), 18, y, INK); m = m.substring(cut); m.trim(); y += 20;
    }
    return;
  }
  filesScroll = constrain(filesScroll, 0, filesMax());
  spr.setClipRect(0, top, W, FTR_Y - top);
  for (size_t i = 0; i < fList.size(); i++) {
    int y = top + i * FROW - filesScroll;
    if (y > FTR_Y || y + FROW < top) continue;
    FEntry& e = fList[i];
    bool dirLike = e.type == FT_DIR || e.type == FT_TRASH || e.type == FT_USB;
    spr.fillRoundRect(6, y, W - 12, FROW - 4, 8, C(CARD));
    drawFileIcon(e.type, 14, y + (FROW - 4) / 2, dirLike ? INK : SOFT);
    String right = e.type == FT_DIR ? String(">") : e.type == FT_TRASH ? String(trashN) + " >" : e.type == FT_USB ? String("PC >") : sizeStr(e.size);
    txt(FS, right, W - 14, y + (FROW - 4) / 2, SOFT, D_MR);
    spr.setFont(FS); int rw = spr.textWidth(right) + 12;
    String nm = dirLike ? e.show : baseNoExt(e.show);   // the icon already says photo / clip
    txt(dirLike ? FB : FS, fitText(dirLike ? FB : FS, nm, W - 44 - 14 - rw), 44, y + (FROW - 4) / 2, INK, D_ML);
  }
  if (fList.empty()) {
    txt(FS, inTrash() ? "Trash is empty" : "Nothing here yet", W / 2, top + 24, SOFT, D_TC);
    if (!inTrash()) txt(FS, fitText(FS, "Send photos from the phone web page", W - 20), W / 2, top + 46, SOFT, D_TC);
  }
  scrollBar(top, FTR_Y - top, filesScroll, filesMax());
  spr.clearClipRect();
  // hint line at the bottom of the list
  if (!fList.empty() && fmUi == FU_LIST && filesMax() == 0) {
    int y = top + fList.size() * FROW + 4;
    if (y < FTR_Y - 20) txt(FS, fitText(FS, inTrash() ? "Tap a file to put it back" : "Hold a file for more", W - 20), W / 2, y, SOFT, D_TC);
  }
  if (fmUi == FU_MENU && fmIdx >= 0 && fmIdx < (int)fList.size()) {
    FmBox b = fmMenuBox(); drawWindow(b);
    txt(FB, fitText(FB, fList[fmIdx].show, b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    auto it = fmMenuItems();
    for (size_t k = 0; k < it.size(); k++) {
      bool red = !strcmp(it[k], "Delete") || !strcmp(it[k], "Delete forever");
      btn(b.x + 8, b.y + 36 + k * 40, b.w - 16, 34, it[k], red, 0xD03030);
    }
  }
  if (fmUi == FU_CONFIRM) {
    FmBox b = fmAskBox(); drawWindow(b);
    String t = fmAsk == 1 ? "Move to Trash?" : fmAsk == 2 ? "Delete forever?" : "Empty the Trash?";
    String l1 = fmAsk == 3 ? String(trashN) + " files, " + sizeStr(trashBytes) : (fmIdx >= 0 && fmIdx < (int)fList.size() ? fList[fmIdx].show : String(""));
    String l2 = fmAsk == 1 ? "You can put it back later." : "This can't be undone.";
    if (fmAsk == 1 && fmIdx >= 0 && fList[fmIdx].type == FT_DIR) { t = "Delete this folder?"; l2 = "Only empty folders."; }
    txt(FB, fitText(FB, t, b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    txt(FS, fitText(FS, l1, b.w - 16), b.x + b.w / 2, b.y + 44, INK, D_MC);
    txt(FS, fitText(FS, l2, b.w - 16), b.x + b.w / 2, b.y + 66, SOFT, D_MC);
    int bw = (b.w - 24) / 2;
    btn(b.x + 8, b.y + b.h - 42, bw, 34, "Cancel", false);
    btn(b.x + 16 + bw, b.y + b.h - 42, bw, 34, fmAsk == 3 ? "Empty" : fmAsk == 1 ? "Move" : "Delete", true, 0xD03030);   // "Move to Trash?" -> Move
  }
  if (fmToastT && millis() - fmToastT < 2500) {
    spr.setFont(FS); int tw = min(W - 16, (int)spr.textWidth(fmToast) + 24);
    spr.fillRoundRect((W - tw) / 2, FTR_Y - 34, tw, 26, 13, C(INK));
    txt(FS, fitText(FS, fmToast, tw - 12), W / 2, FTR_Y - 21, ONINK, D_MC);
  }
}
void filesTick() {   // called from loop(): hide the message after a moment
  if (fmToastT && millis() - fmToastT > 2600) { fmToastT = 0; dirty = true; }
}

// ---------------- Audio (ES8311) ----------------
I2SClass i2s;
bool i2sOn = false, codecOk = false;
volatile bool audRun = false;
volatile TaskHandle_t audTask = nullptr;
File audFile;

void esW(uint8_t r, uint8_t v) { lgfx::i2c::writeRegister8(TOUCH_I2C_PORT, esAddr, r, v, 0, 400000); }
void audioSetVolume() {
  if (!codecOk) return;
  const uint8_t VOL[5] = {0x98, 0xA8, 0xB4, 0xBF, 0xC8};   // 0xBF = 0 dB
  if (volIdx < 0) esW(0x31, 0x60);                       // mute
  else { esW(0x32, VOL[constrain(volIdx, 0, 4)]); esW(0x31, 0x00); }
}
bool audioInit() {
  if (i2sOn) return codecOk;
  pinMode(AUD_EN, OUTPUT); digitalWrite(AUD_EN, HIGH);   // amp off while setting up
  i2s.setPins(AUD_BCLK, AUD_WS, AUD_DOUT, AUD_DIN, AUD_MCLK);
  i2sOn = i2s.begin(I2S_MODE_STD, AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  if (!i2sOn) return false;
  codecOk = false;
  for (uint8_t a : {0x18, 0x19}) {
    auto id = lgfx::i2c::readRegister8(TOUCH_I2C_PORT, a, 0xFD, 400000);
    if (id.has_value()) { esAddr = a; codecOk = true; break; }
  }
  if (!codecOk) { Serial.println("ES8311 not found"); return false; }
  // ES8311 setup: slave mode, MCLK = 256 x rate, 16-bit I2S, DAC on
  esW(0x44, 0x08); esW(0x44, 0x08);
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24); esW(0x04, 0x10); esW(0x05, 0x00);
  esW(0x0B, 0x00); esW(0x0C, 0x00); esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80);                  // power on, slave
  esW(0x01, 0x3F);                  // clocks on, MCLK from pin
  esW(0x06, 0x03); esW(0x07, 0x00); esW(0x08, 0xFF);
  esW(0x09, 0x0C); esW(0x0A, 0x0C); // 16-bit I2S in/out
  esW(0x0D, 0x01); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x13, 0x10);
  esW(0x14, 0x1A); esW(0x1B, 0x0A); esW(0x1C, 0x6A); esW(0x37, 0x08);
  audioSetVolume();
  return true;
}
void audioTaskFn(void*) {
  const size_t N = 2048;
  uint8_t* b = (uint8_t*)malloc(N);
  while (b && audRun && audFile.available()) {
    int n = audFile.read(b, N);
    if (n <= 0) break;
    i2s.write(b, n);
  }
  if (b) { memset(b, 0, N); i2s.write(b, N); free(b); }   // short silence = no click
  audFile.close();
  audRun = false;
  audTask = nullptr;
  vTaskDelete(NULL);
}
bool audioStart(const String& pcmPath) {
  if (volIdx < 0 || !SD_MMC.exists(pcmPath) || !audioInit()) return false;
  audFile = SD_MMC.open(pcmPath, "r");
  if (!audFile) return false;
  digitalWrite(AUD_EN, LOW);   // amp on
  audRun = true;
  TaskHandle_t h;
  xTaskCreatePinnedToCore(audioTaskFn, "audio", 4096, nullptr, 5, &h, 0);
  audTask = h;
  return true;
}
void audioStop() {
  audRun = false;
  uint32_t t = millis();
  while (audTask && millis() - t < 500) delay(5);
  if (i2sOn) digitalWrite(AUD_EN, HIGH);   // amp off (saves battery)
}

// ---------------- Viewer ----------------
bool waitTap(uint32_t ms) {   // true if the screen was tapped
  uint32_t t = millis();
  lgfx::touch_point_t tp;
  do { if (lcd.getTouch(&tp)) { while (lcd.getTouch(&tp)) delay(5); return true; } delay(5); } while (millis() - t < ms);
  return false;
}
void viewerHint(const String& s) {
  lcd.setFont(FS); lcd.setTextDatum(lgfx::textdatum::bottom_center);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString(" " + s + " ", lcd.width() / 2, lcd.height() - 2);
}
bool showImage(const String& path) {
  lcd.fillScreen(TFT_BLACK);
  String e = lowerExt(path);
  bool ok;
  // zoom 0 = shrink to fit the screen, keep the shape
#ifndef SIM
  if (e == "png") ok = lcd.drawPngFile((fs::FS&)SD_MMC, path.c_str(), 0, 0, lcd.width(), lcd.height(), 0, 0, 0.0f, 0.0f, lgfx::datum_t::middle_center);
  else if (e == "bmp") ok = lcd.drawBmpFile((fs::FS&)SD_MMC, path.c_str(), 0, 0, lcd.width(), lcd.height(), 0, 0, 0.0f, 0.0f, lgfx::datum_t::middle_center);
  else ok = lcd.drawJpgFile((fs::FS&)SD_MMC, path.c_str(), 0, 0, lcd.width(), lcd.height(), 0, 0, 0.0f, 0.0f, lgfx::datum_t::middle_center);
#else
  ok = true;
#endif
  if (!ok) {
    lcd.setFont(FB); lcd.setTextDatum(lgfx::textdatum::middle_center); lcd.setTextColor(TFT_WHITE);
    lcd.drawString("Can't open this photo", lcd.width() / 2, lcd.height() / 2 - 12);
    lcd.setFont(FS);
    lcd.drawString("Run it through the converter first", lcd.width() / 2, lcd.height() / 2 + 12);
  }
  return ok;
}
// find image number `from + dir` in the list (skips folders and videos)
int nextImage(int from, int dir) {
  for (int i = from + dir; i >= 0 && i < (int)fList.size(); i += dir) if (fList[i].type == FT_IMG) return i;
  return -1;
}
// Blocking "are you sure?" box drawn straight on the screen (used inside the photo viewer / after a video)
struct LcdBox { int x, y, w, h, bw, by; };
LcdBox lcdConfirmDraw(const String& title, const String& line) {
  int w = min((int)lcd.width() - 24, 230), h = 120, x = (lcd.width() - w) / 2, y = (lcd.height() - h) / 2;
  lcd.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 12, TFT_WHITE);
  lcd.fillRoundRect(x, y, w, h, 10, TFT_BLACK);
  lcd.setTextDatum(D_MC); lcd.setTextColor(TFT_WHITE);
  lcd.setFont(FB); lcd.drawString(title, x + w / 2, y + 20);
  lcd.setFont(pickFont(FS, line)); lcd.setTextColor(0xBDF7); lcd.drawString(line, x + w / 2, y + 46);
  int bw = (w - 24) / 2, by = y + h - 44;
  lcd.fillRoundRect(x + 8, by, bw, 34, 8, 0x4208);
  lcd.fillRoundRect(x + 16 + bw, by, bw, 34, 8, 0xD186);
  lcd.setFont(FB); lcd.setTextColor(TFT_WHITE);
  lcd.drawString("Cancel", x + 8 + bw / 2, by + 17);
  lcd.drawString("Move", x + 16 + bw + bw / 2, by + 17);   // the question is "Move to Trash?"
  return {x, y, w, h, bw, by};
}
bool lcdConfirm(const String& title, const String& line) {
  LcdBox b = lcdConfirmDraw(title, line);
  int x = b.x, y = b.y, w = b.w, h = b.h, bw = b.bw, by = b.by;
  lgfx::touch_point_t tp;
  while (true) {
    uint32_t t0 = millis();
    while (!lcd.getTouch(&tp)) { server.handleClient(); delay(10); if (millis() - t0 > 30000) return false; }   // no answer = Cancel
    int tx = tp.x, ty = tp.y; while (lcd.getTouch(&tp)) delay(5);
    if (ty >= by - 4 && ty < by + 40) { if (tx >= x + 8 && tx < x + 8 + bw) return false; if (tx >= x + 16 + bw && tx < x + w) return true; }
    else if (tx < x || tx > x + w || ty < y || ty > y + h) return false;
  }
}
// the bar at the bottom of the photo viewer: [<] [Back] [Delete] [>]
const int VBAR = 32;
// bar buttons: arrows narrow, Back and Delete wide (so the words fit)
int vbarX(int k, int w) { const int a = 48; return k == 0 ? 0 : k == 1 ? a : k == 2 ? w / 2 : k == 3 ? w - a : w; }
int vbarAt(int x, int w) { for (int k = 0; k < 4; k++) if (x < vbarX(k + 1, w)) return k; return 3; }
void viewerBar() {
  int w = lcd.width(), y = lcd.height() - VBAR;
  lcd.fillRect(0, y, w, VBAR, TFT_BLACK);
  const char* L[4] = {"<", "Back", "Delete", ">"};
  lcd.setFont(FS); lcd.setTextDatum(D_MC);
  for (int k = 0; k < 4; k++) {
    int x0 = vbarX(k, w), bw = vbarX(k + 1, w) - x0;
    lcd.drawRoundRect(x0 + 3, y + 3, bw - 6, VBAR - 6, 6, 0x632C);
    lcd.setTextColor(k == 2 ? 0xFA69 : TFT_WHITE);
    lcd.drawString(L[k], x0 + bw / 2, y + VBAR / 2);
  }
}
// Photo viewer: tap left half = previous, right half = next, bottom bar = buttons
void runImageViewer(int idx) {
  while (idx >= 0 && idx < (int)fList.size()) {
    showImage(fmPath(idx));
    viewerBar();
    lgfx::touch_point_t tp;
    uint32_t t0 = millis(), offMs = OFF_MS[offIdx];
    bool gone = false;
    while (!lcd.getTouch(&tp)) {   // left alone: go back to the list, so the screen can dim and turn off as usual
      server.handleClient(); delay(10);
      if (offMs && millis() - t0 > offMs) { gone = true; break; }
    }
    if (gone) break;
    int x = tp.x, y = tp.y; while (lcd.getTouch(&tp)) delay(5);
    int w = lcd.width();
    int act;   // 0 prev, 1 back, 2 delete, 3 next
    if (y >= lcd.height() - VBAR) act = vbarAt(x, w); else act = x < w / 2 ? 0 : 3;
    if (act == 0) { int n = nextImage(idx, -1); if (n >= 0) idx = n; }
    else if (act == 3) { int n = nextImage(idx, 1); if (n >= 0) idx = n; }
    else if (act == 1) break;
    else if (lcdConfirm("Move to Trash?", fitText(FS, fList[idx].show, min((int)lcd.width() - 40, 210)))) {
      String err = fsTrash(fmPath(idx));
      if (err.length()) { fmSay(fsErrText(err)); break; }
      String cur = curDir; filesLoad(); curDir = cur;
      fmSay("Moved to Trash");
      // show the next photo (or the one before if it was the last)
      int n = (idx < (int)fList.size() && fList[idx].type == FT_IMG) ? idx : nextImage(idx - 1, 1);
      if (n < 0) n = nextImage(min(idx, (int)fList.size()), -1);
      idx = n;
    }
  }
}

// read picture size from a JPEG header (SOF0/SOF2)
bool jpegSize(const uint8_t* b, size_t n, int& w, int& h) {
  for (size_t i = 2; i + 9 < n; i++) {
    if (b[i] == 0xFF && (b[i + 1] == 0xC0 || b[i + 1] == 0xC2)) { h = (b[i + 5] << 8) | b[i + 6]; w = (b[i + 7] << 8) | b[i + 8]; return true; }
  }
  return false;
}
// Video player: tap = stop
void runVideo(const String& path) {
  File f = SD_MMC.open(path, "r");
  if (!f) return;
  const size_t BUF = 256 * 1024;
  uint8_t* buf = (uint8_t*)heap_caps_malloc(BUF, MALLOC_CAP_SPIRAM);
  if (!buf) { f.close(); return; }
  size_t len = f.read(buf, BUF);
  int vw = 0, vh = 0;
  jpegSize(buf, len, vw, vh);
  lcd.setRotation(vw > vh ? (rot == 3 ? 3 : 1) : (rot == 2 ? 2 : 0));   // turn the screen to match the video
  lcd.fillScreen(TFT_BLACK);
  String pcm = path.substring(0, path.lastIndexOf('.')) + ".pcm";
  bool snd = audioStart(pcm);
  uint32_t t0 = millis(), frame = 0;
  size_t off = 0;   // where unread data starts in buf
  bool stop = false;
  lgfx::touch_point_t tp;
  while (!stop) {
    // keep the buffer at least half full (move leftover data to the front first)
    if (len - off < BUF / 2 && f.available()) {
      if (off) { memmove(buf, buf + off, len - off); len -= off; off = 0; }
      len += f.read(buf + len, BUF - len);
    }
    // find start (FF D8) and end (FF D9) of the next picture
    size_t s = off; while (s + 1 < len && !(buf[s] == 0xFF && buf[s + 1] == 0xD8)) s++;
    if (s + 1 >= len) { if (!f.available()) break; off = len; continue; }
    size_t e = s + 2; while (e + 1 < len && !(buf[e] == 0xFF && buf[e + 1] == 0xD9)) e++;
    if (e + 1 >= len) {
      if (!f.available()) break;
      if (s == 0 && len == BUF) { off = len; continue; }   // picture bigger than buffer: skip
      memmove(buf, buf + s, len - s); len -= s; off = 0;
      len += f.read(buf + len, BUF - len);
      continue;
    }
    uint32_t due = t0 + frame * 1000UL / VIDEO_FPS;
    bool late = millis() > due + 1000UL / VIDEO_FPS;   // running behind -> skip this picture
    if (!late) {
      while (millis() < due) delay(1);
#ifndef SIM
      lcd.drawJpg(buf + s, e + 2 - s, 0, 0, lcd.width(), lcd.height(), 0, 0, 0.0f, 0.0f, lgfx::datum_t::middle_center);
#endif
    }
    frame++;
    off = e + 2;
    if (lcd.getTouch(&tp)) { stop = true; while (lcd.getTouch(&tp)) delay(5); }
  }
  if (snd) audioStop();
  f.close();
  free(buf);
  lcd.setRotation(rot);
}
// after a video: [Play again] [Delete] [Back]. Returns 0 back, 1 play again, 2 delete
int videoEndDraw(const String& name) {   // returns y of the first button
  lcd.fillScreen(TFT_BLACK);
  int w = lcd.width(), h = lcd.height(), bw = min(w - 40, 220), x = (w - bw) / 2, y0 = h / 2 - 40;
  lcd.setTextDatum(D_MC); lcd.setTextColor(TFT_WHITE);
  lcd.setFont(pickFont(FB, name)); lcd.drawString(fitText(FB, name, w - 20), w / 2, y0 - 36);
  const char* L[3] = {"Play again", "Delete", "Back"};
  const uint16_t BG[3] = {0x4208, 0xD186, 0x4208};
  lcd.setFont(FB);
  for (int k = 0; k < 3; k++) { lcd.fillRoundRect(x, y0 + k * 46, bw, 38, 8, BG[k]); lcd.drawString(L[k], w / 2, y0 + k * 46 + 19); }
  return y0;
}
int videoEndMenu(const String& name) {
  int y0 = videoEndDraw(name);
  lgfx::touch_point_t tp; uint32_t t0 = millis();
  while (millis() - t0 < 15000) {
    server.handleClient();
    if (lcd.getTouch(&tp)) {
      int ty = tp.y; while (lcd.getTouch(&tp)) delay(5);
      for (int k = 0; k < 3; k++) if (ty >= y0 + k * 46 && ty < y0 + k * 46 + 40) return k == 0 ? 1 : k == 1 ? 2 : 0;
    }
    delay(10);
  }
  return 0;
}

void usbDriveStart();   // in app_usb.h
void filesOpen() { scr = S_FILES; fmUi = FU_LIST; filesLoad(); dirty = true; }
// keyboard results
void fmDoRename(const String& n) { if (fmIdx < 0 || fmIdx >= (int)fList.size()) return; String e = fsRename(fmPath(fmIdx), n); filesLoad(); fmResult(e, "Renamed"); }
void fmDoMkdir(const String& n) { String e = fsMkdir(curDir, n); filesLoad(); fmResult(e, "Folder made"); }
String baseNoExt(const String& n) { int d = n.lastIndexOf('.'); return d > 0 ? n.substring(0, d) : n; }
void fmMenuPick(const char* item) {
  fmUi = FU_LIST;
  String it = item;
  if (it == "Rename") kbdOpen("New name", fList[fmIdx].type == FT_DIR ? fList[fmIdx].name : baseNoExt(fList[fmIdx].name), fmDoRename);
  else if (it == "Move") { pickDir = "/"; fmUi = FU_PICK; pickLoad(); }
  else if (it == "Delete") { fmAsk = 1; fmUi = FU_CONFIRM; }
  else if (it == "Put back") { String e = fsRestore(fList[fmIdx].name); filesLoad(); fmResult(e, "Put back"); if (fList.empty()) { curDir = "/"; filesLoad(); } }
  else if (it == "Delete forever") { fmAsk = 2; fmUi = FU_CONFIRM; }
}
void emptyProgress(int n) {
  spr.fillRoundRect(W / 2 - 80, H / 2 - 18, 160, 36, 10, C(INK));
  txt(FB, "Deleting... " + String(n), W / 2, H / 2, ONINK, D_MC);
  spr.pushSprite(0, 0);
}
void filesLongPress(int x, int y) {
  if (fmUi != FU_LIST || filesMsg.length()) return;
  int top = filesTop();
  if (y < top || y >= FTR_Y) return;
  int i = (y - top + filesScroll) / FROW;
  if (i < 0 || i >= (int)fList.size() || fList[i].type == FT_TRASH || fList[i].type == FT_USB) return;
  fmIdx = i; fmUi = FU_MENU; dirty = true; tHandled = true;
}
void filesTap(int x, int y) {
  dirty = true;
  if (fmUi == FU_MENU) {
    FmBox b = fmMenuBox(); auto it = fmMenuItems();
    for (size_t k = 0; k < it.size(); k++) if (hitR(x, y, b.x + 8, b.y + 36 + k * 40, b.w - 16, 36)) { fmMenuPick(it[k]); return; }
    if (!hitR(x, y, b.x, b.y, b.w, b.h)) fmUi = FU_LIST;
    return;
  }
  if (fmUi == FU_CONFIRM) {
    FmBox b = fmAskBox(); int bw = (b.w - 24) / 2, by = b.y + b.h - 42;
    if (hitR(x, y, b.x + 16 + bw, by, bw, 36)) {
      fmUi = FU_LIST;
      if (fmAsk == 1) { bool dir = fList[fmIdx].type == FT_DIR; String e = fsTrash(fmPath(fmIdx)); filesLoad(); fmResult(e, dir ? "Folder deleted" : "Moved to Trash"); }
      else if (fmAsk == 2) { String e = fsPurge(fList[fmIdx].name); filesLoad(); fmResult(e, "Deleted forever"); }
      else { int n = fsEmptyTrash(emptyProgress); curDir = "/"; filesLoad(); fmSay(String(n) + " files deleted"); }
      if (inTrash() && fList.empty()) { curDir = "/"; filesLoad(); }
    } else if (hitR(x, y, b.x + 8, by, bw, 36) || !hitR(x, y, b.x, b.y, b.w, b.h)) fmUi = FU_LIST;
    return;
  }
  if (fmUi == FU_PICK) {
    if (backHit(x, y)) {   // Back = one folder up, or cancel at the top
      if (pickDir == "/") fmUi = FU_LIST; else { pickDir = dirOf(pickDir); pickLoad(); }
      return;
    }
    int bw = (W - 18) / 2;
    if (y >= FTR_Y - 40 && y < FTR_Y) {
      if (x < 6 + bw + 3) fmUi = FU_LIST;
      else { String e = fsMove(fmPath(fmIdx), pickDir); fmUi = FU_LIST; filesLoad(); fmResult(e, "Moved"); }
      return;
    }
    int pt = pickTop();
    if (y >= pt && y < pickBottom()) {
      int i = (y - pt + pickScroll) / FROW;
      if (i >= 0 && i < (int)pickList.size()) { pickDir = joinPath(pickDir, pickList[i]); pickLoad(); }
    }
    return;
  }
  if (backHit(x, y)) {
    if (inTrash()) { curDir = "/"; filesLoad(); }
    else if (curDir != "/") { curDir = dirOf(curDir); filesLoad(); }
    else scr = S_APPS;
    return;
  }
  if (filesMsg.length()) { filesLoad(); return; }   // retry
  if (y < HDR_H + 34 && x >= W - 74) {   // + Folder / Empty
    if (inTrash()) { if (!fList.empty()) { fmAsk = 3; fmUi = FU_CONFIRM; } }
    else kbdOpen("Folder name", "", fmDoMkdir);
    return;
  }
  int top = filesTop();
  if (y < top) return;
  int i = (y - top + filesScroll) / FROW;
  if (i < 0 || i >= (int)fList.size()) return;
  FEntry e = fList[i];
  if (e.type == FT_TRASH) { curDir = TRASH_DIR; filesLoad(); return; }
  if (e.type == FT_USB) { usbDriveStart(); return; }
  if (inTrash()) { fmIdx = i; fmUi = FU_MENU; return; }   // in the Trash a tap opens: Put back / Delete forever
  if (e.type == FT_DIR) { curDir = joinPath(curDir, e.name); filesLoad(); return; }
  lastTouchMs = millis();
  if (e.type == FT_IMG) runImageViewer(i);
  else {
    String p = joinPath(curDir, e.name);
    while (true) {
      runVideo(p);
      int a = videoEndMenu(e.show);
      if (a == 1) continue;
      if (a == 2 && lcdConfirm("Move to Trash?", fitText(FS, e.show, min((int)lcd.width() - 40, 210)))) {
        String err = fsTrash(p); filesLoad(); fmResult(err, "Moved to Trash");
      }
      break;
    }
  }
  lastTouchMs = millis();
}
