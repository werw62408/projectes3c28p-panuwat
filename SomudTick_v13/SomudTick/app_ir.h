#pragma once
// ============================================================================
//  v13: the IR receiver KY-022 on IO14 (S -> IO14, middle -> 3.3V, "-" -> GND).
//  Only with the button board (PCF8574): then the stick press comes in on K and IO14 is free.
//  Without the button board IO14 is still the stick press of the old wiring, and the receiver is not used.
//  - Remotes app (Apps > Remotes): learn the keys of any remote (TV, fan, light ...), name them, and send them again
//    from the IR LED (IO21). 24 keys. Hold a key: Rename / Use as / Learn again / Delete.
//  - "Use as": a learned key works as a SomudTick button (Up, Down, Left, Right, OK, Back, Next tab, Log page, +1)
//  - the AC page follows the real Panasonic remote: temperature, mode, fan, swing, on / off and the remote type
//  - Signals: the last 8 signals that came in (kind, bits, code), to check what a remote sends
//  Included from SomudTick.ino.
// ============================================================================
#include <IRrecv.h>
#include <IRutils.h>
#define PIN_IR_RX 14
// 1024 places: an AC message is long (two halves); 50 ms: the gap between the halves is still the same message
IRrecv irRx(PIN_IR_RX, 1024, 50, true);
IRsend irTx(PIN_IR);
decode_results irRes;
bool irOn = false;                // the receiver is used (the button board is there, so IO14 is free)
uint32_t irLastMs = 0, irCount = 0;   // the last signal (About) and how many came in
bool irCtl = true;                // learned keys with a job work as SomudTick buttons (Remotes page)
bool irSigView = false;           // the Remotes page shows the Signals view (the last signals that came in)

// ---------------- the last signals (Signals view) ----------------
struct IrSig { uint32_t ms; int16_t type; uint16_t bits, rawLen; uint64_t value; bool repeat; int8_t key; String text; };
const int IR_LOG_N = 8;
IrSig irLog[IR_LOG_N]; int irLogN = 0;   // newest first

// ---------------- learned keys ----------------
const int IR_KEYS = 24;
enum IrAct : uint8_t { IA_NONE, IA_UP, IA_DOWN, IA_LEFT, IA_RIGHT, IA_OK, IA_BACK, IA_TAB, IA_LOG, IA_PLUS, IA_N };
const char* IA_NAMES[IA_N] = {"Nothing", "Up", "Down", "Left", "Right", "OK", "Back", "Next tab", "Log page", "+1 (remind)"};
const char* IA_SHORT[IA_N] = {"", "UP", "DOWN", "LEFT", "RIGHT", "OK", "BACK", "TAB", "LOG", "+1"};
struct IrKey { bool used = false; String name; int16_t type = UNKNOWN; uint16_t bits = 0; uint64_t value = 0;
               std::vector<uint8_t> state; std::vector<uint16_t> raw; uint8_t act = IA_NONE; };
IrKey irKeys[IR_KEYS];
String hex64(uint64_t v) { char b[20]; snprintf(b, sizeof b, "%llX", (unsigned long long)v); return b; }
uint64_t unhex64(const char* s) { return s ? strtoull(s, nullptr, 16) : 0; }
String hexBytes(const std::vector<uint8_t>& v) { String s; char b[4]; for (uint8_t x : v) { snprintf(b, sizeof b, "%02X", x); s += b; } return s; }
std::vector<uint8_t> unhexBytes(const char* s) { std::vector<uint8_t> v; if (!s) return v; for (size_t i = 0; s[i] && s[i + 1] && v.size() < kStateSizeMax; i += 2) { char b[3] = {s[i], s[i + 1], 0}; v.push_back(strtoul(b, nullptr, 16)); } return v; }
void irKeysSave() {   // /irkeys.json, written safely (a power cut keeps the old file)
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < IR_KEYS; i++) {
    IrKey& k = irKeys[i]; if (!k.used) continue;
    JsonObject o = a.add<JsonObject>();
    o["s"] = i; o["n"] = k.name; o["t"] = k.type; o["b"] = k.bits; o["a"] = k.act;
    if (k.state.size()) o["st"] = hexBytes(k.state); else o["v"] = hex64(k.value);
    if (k.raw.size()) { JsonArray r = o["r"].to<JsonArray>(); for (uint16_t x : k.raw) r.add(x); }
  }
  String s; serializeJson(d, s);
  if (!writeFileSafe(LittleFS, "/irkeys.json", s)) Serial.println("irkeys.json not saved (storage full?)");
}
void irKeysLoad() {
  for (auto& k : irKeys) k = IrKey();
  fileRepair(LittleFS, "/irkeys.json");
  File f = LittleFS.open("/irkeys.json", "r");
  if (!f) return;
  JsonDocument d;
  if (!deserializeJson(d, f)) for (JsonObjectConst o : d.as<JsonArrayConst>()) {
    int i = o["s"] | -1; if (i < 0 || i >= IR_KEYS) continue;
    IrKey k; k.used = true;
    k.name = String((const char*)(o["n"] | "Key")).substring(0, 16);
    k.type = o["t"] | (int)UNKNOWN; k.bits = o["b"] | 0; k.act = (o["a"] | 0) % IA_N;
    if (o["st"].is<const char*>()) k.state = unhexBytes(o["st"]); else k.value = unhex64(o["v"] | "0");
    for (uint16_t x : o["r"].as<JsonArrayConst>()) if (k.raw.size() < 600) k.raw.push_back(x);
    irKeys[i] = k;
  }
  f.close();
}
// is this signal a learned key? (same kind, same bits, same code; unknown kinds compare their "hash" code)
int irFind(const decode_results& r) {
  for (int i = 0; i < IR_KEYS; i++) {
    IrKey& k = irKeys[i];
    if (!k.used || k.type != r.decode_type || k.bits != r.bits) continue;
    if (hasACState(r.decode_type)) { if (k.state.size() == r.bits / 8U && !memcmp(k.state.data(), r.state, k.state.size())) return i; }
    else if (k.value == r.value) return i;
  }
  return -1;
}

// ---------------- sending (the IR LED, IO21) ----------------
// The receiver is switched off while the LED sends (the board would hear itself), and what comes in just after is skipped.
void irTxBegin() { if (irOn) irRx.disableIRIn(); }
void irTxEnd() { if (irOn) { irRx.enableIRIn(true); } irQuietUntil = millis() + 400; }
int irFlash = -1; uint32_t irFlashMs = 0;
String irNote; uint32_t irNoteMs = 0; bool irNoteBad = false;
void irSay(const String& s, bool bad) { irNote = s; irNoteMs = millis(); irNoteBad = bad; dirty = true; }
void irSendKey(int i) {
  if (i < 0 || i >= IR_KEYS || !irKeys[i].used) return;
  IrKey& k = irKeys[i];
  irTxBegin();
  if (k.raw.size() && (k.type == UNKNOWN || !k.bits)) irTx.sendRaw(k.raw.data(), k.raw.size(), 38);
  else if (hasACState((decode_type_t)k.type)) irTx.send((decode_type_t)k.type, k.state.data(), k.state.size());
  else irTx.send((decode_type_t)k.type, k.value, k.bits);
  irTxEnd();
  irFlash = i; irFlashMs = millis();
  ledFlash(0x2060FF, 120); sfx(SFX_KEY); dirty = true;
}

// ---------------- learning ----------------
int irLearnSlot = -1; uint32_t irLearnT = 0; const uint32_t IR_LEARN_MS = 15000;
int irNameSlot = -1;
bool irLearning() { return irLearnSlot >= 0; }
String irLearnLeft() { uint32_t e = millis() - irLearnT; return String(e >= IR_LEARN_MS ? 0 : (IR_LEARN_MS - e + 999) / 1000) + "S"; }
void irLearnStart(int slot) {
  if (!irOn) { irSay("No IR receiver: needs the button board", true); return; }
  irLearnSlot = slot; irLearnT = millis(); dirty = true;
}
void irNameDone(const String& t) {
  if (irNameSlot < 0 || irNameSlot >= IR_KEYS || !irKeys[irNameSlot].used) return;
  String n = t; n.trim();
  if (n.length()) { irKeys[irNameSlot].name = n.substring(0, 16); irKeysSave(); }
  irNameSlot = -1;
}
// a new key came in while learning: keep it at once (with a plain name), then ask for a name
void irLearnGot(const decode_results& r) {
  int slot = irLearnSlot; irLearnSlot = -1;
  int same = irFind(r);
  if (same >= 0 && same != slot) { irSay("Already learned: " + irKeys[same].name, true); irFlash = same; irFlashMs = millis(); return; }
  IrKey k; k.used = true; k.type = r.decode_type; k.bits = r.bits;
  k.name = irKeys[slot].used ? irKeys[slot].name : "Key " + String(slot + 1);
  k.act = irKeys[slot].used ? irKeys[slot].act : (uint8_t)IA_NONE;
  if (hasACState(r.decode_type)) k.state.assign(r.state, r.state + min((int)kStateSizeMax, r.bits / 8));
  else k.value = r.value;
  if (r.decode_type == UNKNOWN || !r.bits) {   // a kind the library does not know: keep the pulses themselves
    uint16_t n = min((uint16_t)600, getCorrectedRawLength(&r));
    uint16_t* raw = resultToRawArray(&r);
    if (raw) { k.raw.assign(raw, raw + n); delete[] raw; }
  }
  irKeys[slot] = k;
  irKeysSave();
  sfx(SFX_GOAL); ledFlash(0x40FF60, 300);
  irNameSlot = slot;
  kbdOpen("Name this key", k.name, irNameDone);
}

// ---------------- the AC page follows the real remote ----------------
String acStateText(const uint8_t* st) {   // "COOL 25C FAN 2 ON" (Signals view)
  IRPanasonicAc r(PIN_IR); r.setRaw(st);
  String m = "?"; for (int i = 0; i < 4; i++) if (AC_MODE_V[i] == r.getMode()) m = AC_MODE_N[i];
  if (r.getMode() == kPanasonicAcHeat) m = "Heat";
  String f = "?"; for (int i = 0; i < 6; i++) if (AC_FAN_V[i] == r.getFan()) f = AC_FAN_N[i];
  return "AC: " + m + " " + String(r.getTemp()) + "C fan " + f + (r.getPower() ? " ON" : " OFF");
}
void acFromRemote(const uint8_t* st) {
  IRPanasonicAc r(PIN_IR); r.setRaw(st);
  AcState n = acS;
  n.power = r.getPower();
  n.temp = constrain((int)r.getTemp(), (int)kPanasonicAcMinTemp, (int)kPanasonicAcMaxTemp);
  for (int i = 0; i < 4; i++) if (AC_MODE_V[i] == r.getMode()) n.mode = i;     // (heat: not on this page, the mode stays)
  for (int i = 0; i < 6; i++) if (AC_FAN_V[i] == r.getFan()) n.fan = i;
  for (int i = 0; i < 6; i++) if (AC_SWING_V[i] == r.getSwingVertical()) n.swing = i;
  n.quiet = r.getQuiet();
  for (int i = 0; i < AC_NMODELS; i++) if (AC_MODEL_V[i] == r.getModel()) n.model = i;   // the remote's type: "Type" is right at once
  acS = n; acSave();
  acSyncMs = millis(); acSyncs++;
  if (scr == S_AC) dirty = true;
}

// ---------------- a learned key as a SomudTick button ----------------
int irKeyLast = -1; uint32_t irKeyLastMs = 0, irRepNext = 0;
bool irDir(uint8_t a) { return a >= IA_UP && a <= IA_RIGHT; }
JoyEv irActEv(uint8_t a) {
  switch (a) { case IA_UP: return JE_UP; case IA_DOWN: return JE_DOWN; case IA_LEFT: return JE_LEFT; case IA_RIGHT: return JE_RIGHT;
               case IA_OK: return JE_PRESS; case IA_BACK: return JE_BACK; default: return JE_NONE; }
}
void irDo(uint8_t a) {
  lastTouchMs = millis();
  if (pw == P_OFF) { wake(); joyNavReset(); return; }   // like every button: the first one only wakes the screen
  switch (a) {
    case IA_UP: case IA_DOWN: case IA_LEFT: case IA_RIGHT: case IA_OK: navHandle(irActEv(a)); break;
    case IA_BACK: navBackAny(); break;
    case IA_TAB: wake(); padTab(1); break;
    case IA_LOG: wake(); if (padTabsOk()) { goScreen(S_HOME); sfx(SFX_TAP); } else sfx(SFX_ERR); break;
    case IA_PLUS: wake(); padQuickLog(); break;
    default: break;
  }
}
// one signal came in. act = do what a learned key is for (menus); else the event is only given back (windows)
JoyEv irHandle(const decode_results& r, bool act) {
  uint32_t ms = millis();
  int32_t q = (int32_t)(ms - irQuietUntil);
  if (q < 0 && q > -2000) return JE_NONE;   // the board's own LED, heard back (the window is never longer than 0.4 s)
  irLastMs = ms; irCount++;
  int key = irFind(r);
  bool rep = r.repeat || (r.decode_type == NEC && r.value == kRepeat);
  if (key < 0 && rep && irKeyLast >= 0 && ms - irKeyLastMs < 250) key = irKeyLast;   // "still held" codes (NEC)
  // write it down for the Signals view (a held key: the first line is only updated)
  bool same = irLogN && irLog[0].type == r.decode_type && irLog[0].value == r.value && irLog[0].bits == r.bits && ms - irLog[0].ms < 400;
  if (!same && !rep) {
    for (int i = min(irLogN, IR_LOG_N - 1); i > 0; i--) irLog[i] = irLog[i - 1];
    IrSig& s = irLog[0];
    s.type = r.decode_type; s.bits = r.bits; s.value = r.value; s.repeat = rep; s.key = key; s.rawLen = getCorrectedRawLength(&r);
    s.text = r.decode_type == PANASONIC_AC && r.bits == kPanasonicAcStateLength * 8 ? acStateText(r.state) : resultToHexidecimal(&r);
    irLogN = min(irLogN + 1, IR_LOG_N);
  }
  if (irLogN) irLog[0].ms = ms;
  if (scr == S_REMOTE) dirty = true;
  if (irLearning()) { if (!rep && (r.decode_type != UNKNOWN || getCorrectedRawLength(&r) >= 16)) irLearnGot(r); return JE_NONE; }
  if (r.decode_type == PANASONIC_AC && r.bits == kPanasonicAcStateLength * 8) { if (acFollow) acFromRemote(r.state); return JE_NONE; }
  if (key < 0) return JE_NONE;
  if (scr == S_REMOTE) { irFlash = key; irFlashMs = ms; }
  uint8_t a = irKeys[key].act;
  if (!irCtl || a == IA_NONE) { irKeyLast = key; irKeyLastMs = ms; return JE_NONE; }
  // a held key sends its code again and again: the first one counts, then only Up / Down / Left / Right repeat
  bool cont = key == irKeyLast && ms - irKeyLastMs < 250;
  irKeyLast = key; irKeyLastMs = ms;
  bool fire = false;
  if (!cont) { fire = true; irRepNext = ms + 400; }
  else if (irDir(a) && (int32_t)(ms - irRepNext) >= 0) { fire = true; irRepNext = ms + 150; }
  if (!fire) return JE_NONE;
  if (act) { irDo(a); return JE_NONE; }
  return irActEv(a);
}
void irTask() {   // from loop()
  if (!irOn) return;
  if (irLearning() && millis() - irLearnT > IR_LEARN_MS) { irLearnSlot = -1; irSay("Nothing came in", true); }
  if (irLearning() && scr != S_REMOTE && scr != S_KBD) irLearnSlot = -1;   // left the page (a tab button): stop learning
  static uint32_t sigT = 0;
  if (scr == S_REMOTE && irSigView && pw == P_ON && millis() - sigT > 1000) { sigT = millis(); dirty = true; }   // "5s ago" goes on
  if (!irRx.decode(&irRes)) return;
  irHandle(irRes, true);
  irRx.resume();
}
JoyEv irWaitEvent() {   // the windows that wait in their own loop (photo viewer ...): only the button events
  if (!irOn || !irRx.decode(&irRes)) return JE_NONE;
  JoyEv e = irHandle(irRes, false);
  irRx.resume();
  return e;
}
void irBegin() {
  irKeysLoad();
  irCtl = prefs.getBool("irCtl", true);
  irTx.begin();
  irOn = !joySwIo14;   // with the button board IO14 is the receiver's
  if (!irOn) return;
  irRx.setUnknownThreshold(12);   // shorter blips are light noise, not a remote
  irRx.enableIRIn(true);          // (with the pin's pull-up: a missing receiver reads as "no signal")
}
String irAboutText() {
  if (!irOn) return joySwIo14 ? String("off (IO14 = stick)") : String("off");
  if (!irLastMs) return "no signal yet";
  uint32_t s = (millis() - irLastMs) / 1000;
  return "OK, " + (s < 60 ? String(s) + "s ago" : s < 3600 ? String(s / 60) + "m ago" : String(s / 3600) + "h ago");
}

// ---------------- the Remotes page ----------------
int irPage = 0;
uint8_t irMenu = 0; int irMenuKey = -1;   // 1 key menu, 2 "Use as" list, 3 "Delete?"
int irCols() { return land() ? 4 : 3; }
int irGridTop() { return HDR_H + 68; }
int irRows() { return constrain((FTR_Y - 6 - irGridTop() + 6) / 46, 2, 4); }
int irPerPage() { return irCols() * irRows(); }
int irPages() { return (IR_KEYS + irPerPage() - 1) / irPerPage(); }
void irKeyRect(int slot, int& x, int& y, int& w, int& h) {
  int cols = irCols(), rows = irRows(), top = irGridTop(), gap = 6;
  w = (W - 12 - gap * (cols - 1)) / cols; h = (FTR_Y - 6 - top - gap * (rows - 1)) / rows;
  x = 6 + (slot % cols) * (w + gap); y = top + (slot / cols) * (h + gap);
}
void remoteOpen() { scr = S_REMOTE; irMenu = 0; irSigView = false; irPage = constrain(irPage, 0, irPages() - 1); dirty = true; }
void remoteLeave() { irLearnSlot = -1; irMenu = 0; scr = S_APPS; dirty = true; }
// The windows stay between the top bar and the bottom tabs (wide screen with both bars: only 180 px).
int irAreaTop() { return HDR_H + 4; }
int irAreaH() { return FTR_Y - 4 - irAreaTop(); }
// the key menu: one column, or 2 columns when the page is short
bool irMenu2Col() { return 36 + 5 * 38 > irAreaH(); }
FmBox irMenuBox() {
  bool two = irMenu2Col();
  int w = two ? min(W - 16, 300) : min(W - 30, 220), h = 36 + (two ? 3 : 5) * 38;
  return {(W - w) / 2, irAreaTop() + max(0, (irAreaH() - h) / 2), w, h};
}
void irMenuBtn(const FmBox& b, int k, int& x, int& y, int& w) {   // k: 0 Rename, 1 Use as, 2 Learn again, 3 Delete, 4 Cancel
  if (!irMenu2Col() || k == 4) { x = b.x + 8; y = b.y + 36 + (irMenu2Col() ? 2 : k) * 38; w = b.w - 16; return; }
  w = (b.w - 22) / 2; x = b.x + 8 + (k % 2) * (w + 6); y = b.y + 36 + (k / 2) * 38;
}
// the "Use as" list: 2 columns, or 3 when the page is short
struct IrUseGrid { FmBox b; int cols, bw, step; };
IrUseGrid irUseGrid() {
  int cols = 2, rows = (IA_N + 1) / 2, step = 36;
  if (40 + rows * step > irAreaH()) { cols = 3; rows = (IA_N + 2) / 3; step = min(36, (irAreaH() - 40) / rows); }
  int w = min(W - 16, 312), h = 40 + rows * step;
  return {{(W - w) / 2, irAreaTop() + max(0, (irAreaH() - h) / 2), w, h}, cols, (w - 16 - (cols - 1) * 6) / cols, step};
}
void irUseBtn(const IrUseGrid& g, int a, int& x, int& y, int& w) {   // a last button alone on its row gets the whole row
  x = g.b.x + 8 + (a % g.cols) * (g.bw + 6); y = g.b.y + 36 + (a / g.cols) * g.step;
  w = (a == IA_N - 1 && a % g.cols == 0) ? g.b.w - 16 : g.bw;
}
const char* IR_MENU[5] = {"Rename", "Use as", "Learn again", "Delete", "Cancel"};
void drawRemoteKey(int i, int x, int y, int w, int h) {
  IrKey& k = irKeys[i];
  if (!k.used) {   // empty: tap = learn here
    if (themeHud) hudShape(x, y, w, h, 8, -1, LINE); else spr.drawRoundRect(x, y, w, h, 10, C(LINE));
    navAdd(x, y, w, h);
    txt(FL, "+", x + w / 2, y + h / 2 - (h > 40 ? 7 : 0), SOFT, D_MC);
    if (h > 40) txt(FS, hudUp("learn"), x + w / 2, y + h / 2 + 13, SOFT, D_MC);
    return;
  }
  bool lit = irFlash == i && millis() - irFlashMs < 300;
  navAdd(x, y, w, h);
  if (themeHud) hudShape(x, y, w, h, 8, lit ? INK : CARD, INK);
  else { spr.fillRoundRect(x, y, w, h, 10, C(lit ? INK : CARD)); spr.drawRoundRect(x, y, w, h, 10, C(LINE)); }
  uint32_t fg = lit ? ONINK : INK;
  if (k.act) { String t = IA_SHORT[k.act]; spr.setFont(&fonts::Font0); int tw = spr.textWidth(t) + 6;
    spr.fillRoundRect(x + w - tw - 4, y + 4, tw, 11, 3, C(lit ? ONINK : SOFT)); txt(&fonts::Font0, t, x + w - tw / 2 - 4, y + 10, lit ? INK : CARD, D_MC); }
  deckLabel(hudUp(k.name), x, y + (k.act ? 3 : 0), w, h, fg);   // big, small, or on two lines (like the Deck keys)
}
// the kind of remote signal, as people write it
String irKindName(int t) {
  switch (t) { case UNKNOWN: return "Unknown"; case NEC: return "NEC"; case SONY: return "Sony"; case SAMSUNG: return "Samsung";
               case PANASONIC: return "Panasonic"; case PANASONIC_AC: return "Panasonic AC"; case RC5: return "RC5"; case RC6: return "RC6"; case JVC: return "JVC"; default: break; }
  String s = typeToString((decode_type_t)t); s.replace("_", " "); return s;
}
void drawRemote() {
  bool sig = irSigView;
  drawAppTitle("Remotes", "", true);
  btn(W - 70, HDR_H + 4, 64, 26, sig ? "Keys" : "Signals", sig);
  if (sig) {   // ---- the last signals ----
    int y = HDR_H + 36;
    txt(FS, fitText(FS, irOn ? (irLogN ? "Newest first" : "Point a remote here, press a key") : "No IR receiver (needs the buttons)", W - 16), 8, y + 4, SOFT);
    y += 26;
    for (int i = 0; i < irLogN && y + 38 <= FTR_Y - 2; i++, y += 42) {   // only whole rows (the bottom tabs cover the rest)
      IrSig& s = irLog[i];
      card(6, y, W - 12, 38, 8, CARD);
      String kind = irKindName(s.type);
      String size = s.type == UNKNOWN ? String(s.rawLen) + " pulses" : String(s.bits) + " bit";
      uint32_t ago = (millis() - s.ms) / 1000;
      String when = ago < 60 ? String(ago) + "s ago" : String(ago / 60) + "m ago";
      txt(FS, when, W - 12, y + 3, SOFT, D_TR);
      spr.setFont(FS); int ww = spr.textWidth(when) + 10;
      txt(FB, fitText(FB, kind, W - 24 - ww), 12, y + 3, INK);
      bool known = s.key >= 0 && s.key < IR_KEYS && irKeys[s.key].used;
      String l2 = known ? "= " + irKeys[s.key].name + "   " + s.text : s.type == PANASONIC_AC ? s.text : size + "   " + s.text;
      txt(FS, fitText(FS, l2, W - 24), 12, y + 21, s.key >= 0 ? INK : SOFT);
    }
    return;
  }
  // ---- the keys ----
  int y = HDR_H + 36;
  btn(6, y, W - 82, 26, irCtl ? "Control: ON" : "Control: OFF", irCtl);   // the keys with a job ("Use as") work as SomudTick buttons
  btn(W - 70, y, 64, 26, String(irPage + 1) + "/" + irPages() + "  >", false);
  int per = irPerPage();
  for (int s = 0; s < per; s++) {
    int i = irPage * per + s; if (i >= IR_KEYS) break;
    int x, yy, w, h; irKeyRect(s, x, yy, w, h);
    drawRemoteKey(i, x, yy, w, h);
  }
  if (irFlash >= 0 && millis() - irFlashMs < 350) dirty = true;
  // a short note at the bottom (learned / sent / nothing came in)
  if (irNote.length() && millis() - irNoteMs < 3000) {
    spr.setFont(FS); int tw = min(W - 16, (int)spr.textWidth(irNote) + 24);
    spr.fillRoundRect((W - tw) / 2, FTR_Y - 34, tw, 26, 13, C(irNoteBad ? 0xD03030 : INK));
    txt(FS, fitText(FS, irNote, tw - 12), W / 2, FTR_Y - 21, irNoteBad ? 0xFFFFFF : ONINK, D_MC);
    dirty = true;
  }
  // windows on top
  if (irLearning()) {
    FmBox b = fmAskBox(); drawWindow(b);
    navModal(b.x + b.w / 2, b.y + b.h - 25);   // Back = Cancel
    txt(FB, fitText(FB, hudUp("Learning ") + irLearnLeft().substring(0, irLearnLeft().length() - 1) + " s", b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    txt(FS, fitText(FS, "Point the remote here", b.w - 16), b.x + b.w / 2, b.y + 44, INK, D_MC);
    txt(FS, fitText(FS, "and press one key.", b.w - 16), b.x + b.w / 2, b.y + 64, SOFT, D_MC);
    btn(b.x + 8, b.y + b.h - 42, b.w - 16, 34, "Cancel", false);
    navModalEnd();
    dirty = true;   // (the seconds go down)
  } else if (irMenu == 1 && irMenuKey >= 0) {
    FmBox b = irMenuBox(); drawWindow(b);
    navModal(2, H / 2);
    txt(FB, fitText(FB, irKeys[irMenuKey].name, b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    for (int k = 0; k < 5; k++) {
      String t = k == 1 ? String("Use as: ") + IA_NAMES[irKeys[irMenuKey].act] : String(IR_MENU[k]);
      int x, y, w; irMenuBtn(b, k, x, y, w);
      btn(x, y, w, 32, t, k == 3, 0xD03030);
    }
    navModalEnd();
  } else if (irMenu == 2 && irMenuKey >= 0) {
    IrUseGrid g = irUseGrid(); drawWindow(g.b);
    navModal(2, H / 2);
    txt(FB, fitText(FB, "Use " + irKeys[irMenuKey].name + " as", g.b.w - 16), g.b.x + g.b.w / 2, g.b.y + 18, INK, D_MC);
    for (int a = 0; a < IA_N; a++) { int x, y, w; irUseBtn(g, a, x, y, w); btn(x, y, w, g.step - 6, IA_NAMES[a], irKeys[irMenuKey].act == a); }
    navModalEnd();
  } else if (irMenu == 3 && irMenuKey >= 0) {
    FmBox b = fmAskBox(); drawWindow(b);
    navModal(2, H / 2);
    txt(FB, fitText(FB, "Delete this key?", b.w - 16), b.x + b.w / 2, b.y + 18, INK, D_MC);
    txt(FS, fitText(FS, irKeys[irMenuKey].name, b.w - 16), b.x + b.w / 2, b.y + 44, INK, D_MC);
    txt(FS, fitText(FS, "Learn it again any time.", b.w - 16), b.x + b.w / 2, b.y + 64, SOFT, D_MC);
    int bw = (b.w - 24) / 2;
    btn(b.x + 8, b.y + b.h - 42, bw, 34, "Cancel", false);
    btn(b.x + 16 + bw, b.y + b.h - 42, bw, 34, "Delete", true, 0xD03030);
    navModalEnd();
  }
}
void remoteMenuPick(int k) {
  int i = irMenuKey; irMenu = 0;
  if (i < 0 || i >= IR_KEYS || !irKeys[i].used) return;
  if (k == 0) { irNameSlot = i; kbdOpen("Name this key", irKeys[i].name, irNameDone); }
  else if (k == 1) irMenu = 2;
  else if (k == 2) irLearnStart(i);
  else if (k == 3) irMenu = 3;
  dirty = true;
}
void remoteTap(int x, int y) {
  dirty = true;
  if (irLearning()) {   // the learning window: only Cancel
    FmBox b = fmAskBox();
    if (hitR(x, y, b.x + 8, b.y + b.h - 42, b.w - 16, 36) || !hitR(x, y, b.x, b.y, b.w, b.h)) irLearnSlot = -1;
    return;
  }
  if (irMenu == 1) {
    FmBox b = irMenuBox();
    for (int k = 0; k < 5; k++) { int bx, by, bw; irMenuBtn(b, k, bx, by, bw); if (hitR(x, y, bx, by, bw, 32)) { remoteMenuPick(k); return; } }
    if (!hitR(x, y, b.x, b.y, b.w, b.h)) irMenu = 0;
    return;
  }
  if (irMenu == 2) {
    IrUseGrid g = irUseGrid();
    for (int a = 0; a < IA_N; a++) { int bx, by, bw; irUseBtn(g, a, bx, by, bw);
      if (hitR(x, y, bx, by, bw, g.step - 6)) { irKeys[irMenuKey].act = a; irKeysSave(); irMenu = 0; irSay(String("Works as: ") + IA_NAMES[a], false); return; } }
    if (!hitR(x, y, g.b.x, g.b.y, g.b.w, g.b.h)) irMenu = 0;
    return;
  }
  if (irMenu == 3) {
    FmBox b = fmAskBox(); int bw = (b.w - 24) / 2, by = b.y + b.h - 42;
    if (hitR(x, y, b.x + 16 + bw, by, bw, 36)) { irKeys[irMenuKey] = IrKey(); irKeysSave(); irSay("Deleted", false); }
    irMenu = 0;
    return;
  }
  if (backHit(x, y)) { remoteLeave(); return; }
  if (hitR(x, y, W - 70, HDR_H + 4, 64, 26)) { irSigView = !irSigView; return; }
  if (irSigView) return;
  if (hitR(x, y, 6, HDR_H + 36, W - 82, 26)) { irCtl = !irCtl; prefs.putBool("irCtl", irCtl); return; }
  if (hitR(x, y, W - 70, HDR_H + 36, 64, 26)) { irPage = (irPage + 1) % irPages(); return; }
  int per = irPerPage();
  for (int s = 0; s < per; s++) {
    int i = irPage * per + s; if (i >= IR_KEYS) break;
    int kx, ky, kw, kh; irKeyRect(s, kx, ky, kw, kh);
    if (!hitR(x, y, kx, ky, kw, kh)) continue;
    if (irKeys[i].used) irSendKey(i); else irLearnStart(i);
    return;
  }
}
void remoteLongPress(int x, int y) {   // hold a learned key: its menu
  if (irSigView || irMenu || irLearning()) return;
  int per = irPerPage();
  for (int s = 0; s < per; s++) {
    int i = irPage * per + s; if (i >= IR_KEYS) break;
    int kx, ky, kw, kh; irKeyRect(s, kx, ky, kw, kh);
    if (hitR(x, y, kx, ky, kw, kh) && irKeys[i].used) { irMenuKey = i; irMenu = 1; tHandled = true; dirty = true; return; }
  }
}
