// File systems, Wi-Fi, web server and other ESP32 libraries, faked for the PC build.
#pragma once
#include "Arduino.h"

// ---------------- files (backed by real folders) ----------------
#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
struct SimFileImpl {
  std::string host, path;   // host path, path as the board sees it
  FILE* fp = nullptr;
  bool dir = false;
  DIR* dp = nullptr;
  ~SimFileImpl() { if (fp) fclose(fp); if (dp) closedir(dp); }
};
class File : public Stream {
  std::shared_ptr<SimFileImpl> f_;
 public:
  File() {}
  explicit File(std::shared_ptr<SimFileImpl> f) : f_(f) {}
  explicit operator bool() const { return f_ && (f_->fp || f_->dir); }
  bool isDirectory() const { return f_ && f_->dir; }
  int available() override {
    if (!f_ || !f_->fp) return 0;
    long p = ftell(f_->fp); fseek(f_->fp, 0, SEEK_END); long e = ftell(f_->fp); fseek(f_->fp, p, SEEK_SET);
    return (int)std::max(0L, e - p);
  }
  int read() override { if (!f_ || !f_->fp) return -1; int c = fgetc(f_->fp); return c == EOF ? -1 : c; }
  int peek() override { if (!f_ || !f_->fp) return -1; int c = fgetc(f_->fp); if (c != EOF) ungetc(c, f_->fp); return c == EOF ? -1 : c; }
  int read(uint8_t* b, size_t n) { if (!f_ || !f_->fp) return -1; return (int)fread(b, 1, n, f_->fp); }
  size_t readBytes(char* b, size_t n) override { if (!f_ || !f_->fp) return 0; return fread(b, 1, n, f_->fp); }
  size_t write(uint8_t c) override { return (f_ && f_->fp && fputc(c, f_->fp) != EOF) ? 1 : 0; }
  size_t write(const uint8_t* b, size_t n) override { return (f_ && f_->fp) ? fwrite(b, 1, n, f_->fp) : 0; }
  bool seek(size_t pos) { return f_ && f_->fp && fseek(f_->fp, (long)pos, SEEK_SET) == 0; }
  size_t position() const { return (f_ && f_->fp) ? (size_t)ftell(f_->fp) : 0; }
  size_t size() const { struct stat st; return (f_ && !stat(f_->host.c_str(), &st)) ? st.st_size : 0; }
  const char* path() const { return f_ ? f_->path.c_str() : ""; }
  const char* name() const { if (!f_) return ""; auto p = f_->path.rfind('/'); return f_->path.c_str() + (p == std::string::npos ? 0 : p + 1); }
  void close() { f_.reset(); }
  File openNextFile();
};
class SimFS {
 public:
  std::string root;
  bool mounted = true;
  std::string host(const String& p) const { return root + (p.length() && p[0] == '/' ? p.std() : "/" + p.std()); }
  bool begin(bool = false) { mkdir(String("/")); return true; }
  bool exists(const String& p) const { struct stat st; return !stat(host(p).c_str(), &st); }
  File open(const String& p, const char* mode = "r", bool = false) const {
    auto f = std::make_shared<SimFileImpl>();
    f->host = host(p); f->path = p.std();
    struct stat st;
    if (!stat(f->host.c_str(), &st) && S_ISDIR(st.st_mode)) { f->dir = true; f->dp = opendir(f->host.c_str()); return File(f); }
    f->fp = fopen(f->host.c_str(), !strcmp(mode, "w") ? "wb" : !strcmp(mode, "a") ? "ab" : "rb");
    if (!f->fp) return File();
    return File(f);
  }
  bool mkdir(const String& p) const { return !::mkdir(host(p).c_str(), 0755) || errno == EEXIST; }
  bool remove(const String& p) const { return !::unlink(host(p).c_str()); }
  bool rmdir(const String& p) const { return !::rmdir(host(p).c_str()); }
  bool rename(const String& a, const String& b) const { return !::rename(host(a).c_str(), host(b).c_str()); }
};
inline File File::openNextFile() {
  if (!f_ || !f_->dp) return File();
  while (struct dirent* e = readdir(f_->dp)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    std::string p = f_->path == "/" ? "/" + std::string(e->d_name) : f_->path + "/" + e->d_name;
    SimFS fs; fs.root = f_->host.substr(0, f_->host.size() - (f_->path == "/" ? 1 : f_->path.size()));
    return fs.open(String(p));
  }
  return File();
}
namespace fs { typedef SimFS FS; }
extern std::string g_simLogsRoot;
class LittleFSFS : public SimFS {
 public:
  uint64_t total = 896 * 1024;
  bool begin(bool = false, const char* = "/littlefs", int = 10, const char* label = "spiffs") {
    if (!strcmp(label, "logs")) { if (g_simLogsRoot.empty()) return false; root = g_simLogsRoot; total = 12ULL * 1048576; }
    ::mkdir(root.c_str(), 0755); return true;
  }
  uint64_t totalBytes() const { return total; }
  uint64_t usedBytes() const {   // files rounded up to 4 KB blocks, like LittleFS
    uint64_t u = 8192; std::string cmd = "find '" + root + "' -type f -printf '%s\\n' 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r"); long v; while (p && fscanf(p, "%ld", &v) == 1) u += (v + 4095) / 4096 * 4096; if (p) pclose(p); return u;
  }
};
namespace fs { using ::LittleFSFS; }
extern LittleFSFS LittleFS;
#define CARD_NONE 0
#define CARD_SD 2
#define SDMMC_FREQ_DEFAULT 20000
class SDMMCFS : public SimFS {
 public:
  bool present = true;
  void setPins(int, int, int, int = -1, int = -1, int = -1) {}
  bool begin(const char* = "/sdcard", bool = false, bool = false, int = 0) { return present; }
  void end() {}
  int cardType() const { return present ? CARD_SD : CARD_NONE; }
  uint64_t totalBytes() const { return 7948ULL * 1048576; }
  uint64_t usedBytes() const { return 1204ULL * 1048576; }
  int sectorSize() { return 512; }
  int numSectors() { return 15523840; }
  bool readRAW(uint8_t* b, uint32_t) { memset(b, 0, 512); return true; }
  bool writeRAW(uint8_t*, uint32_t) { return true; }
};
extern SDMMCFS SD_MMC;

// ---------------- Preferences (kept in memory) ----------------
class Preferences {
  std::map<std::string, std::vector<uint8_t>> m_;
 public:
  bool clear() { m_.clear(); return true; }
 private:
  template <typename T> T get(const char* k, T d) { auto it = m_.find(k); if (it == m_.end() || it->second.size() != sizeof(T)) return d; T v; memcpy(&v, it->second.data(), sizeof v); return v; }
  template <typename T> size_t put(const char* k, T v) { m_[k].assign((uint8_t*)&v, (uint8_t*)&v + sizeof v); return sizeof v; }
 public:
  bool begin(const char*, bool = false) { return true; }
  bool isKey(const char* k) { return m_.count(k); }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return get(k, d); }
  int8_t getChar(const char* k, int8_t d = 0) { return get(k, d); }
  bool getBool(const char* k, bool d = false) { return get(k, d); }
  uint32_t getUInt(const char* k, uint32_t d = 0) { return get(k, d); }
  uint16_t getUShort(const char* k, uint16_t d = 0) { return get(k, d); }
  int32_t getInt(const char* k, int32_t d = 0) { return get(k, d); }
  size_t putUChar(const char* k, uint8_t v) { return put(k, v); }
  size_t putChar(const char* k, int8_t v) { return put(k, v); }
  size_t putBool(const char* k, bool v) { return put(k, v); }
  size_t putUInt(const char* k, uint32_t v) { return put(k, v); }
  size_t putUShort(const char* k, uint16_t v) { return put(k, v); }
  size_t putInt(const char* k, int32_t v) { return put(k, v); }
  String getString(const char* k, const String& d = String()) { auto it = m_.find(k); return it == m_.end() ? d : String(std::string(it->second.begin(), it->second.end())); }
  size_t putString(const char* k, const String& v) { m_[k].assign(v.c_str(), v.c_str() + v.length()); return v.length(); }
  size_t getBytes(const char* k, void* b, size_t n) { auto it = m_.find(k); if (it == m_.end()) return 0; size_t c = std::min(n, it->second.size()); memcpy(b, it->second.data(), c); return c; }
  size_t putBytes(const char* k, const void* b, size_t n) { m_[k].assign((const uint8_t*)b, (const uint8_t*)b + n); return n; }
};

// ---------------- Wi-Fi ----------------
class IPAddress {
  uint8_t a_[4];
 public:
  IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : a_{a, b, c, d} {}
  String toString() const { char b[20]; snprintf(b, sizeof b, "%u.%u.%u.%u", a_[0], a_[1], a_[2], a_[3]); return String(b); }
  bool operator==(const IPAddress& o) const { return memcmp(a_, o.a_, 4) == 0; }
  bool operator!=(const IPAddress& o) const { return !(*this == o); }
};
enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 };
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)
#define WIFI_AUTH_OPEN 0
struct SimNet { String ssid; int rssi; int enc; };
class WiFiClass {
 public:
  bool connected = true;
  String ssid = "Panuwat_Home_5G";
  std::vector<SimNet> nets;
  int scanState = WIFI_SCAN_FAILED;
  int curMode = 0; bool apUp = false; int stations = 0, beginCalls = 0;   // for tests
  bool mode(int m) { curMode = m; if (m != 2 && m != 3) apUp = false; return true; }   // 2 = AP, 3 = AP+STA
  bool softAP(const char*, const char*) { apUp = true; return true; }
  bool softAPdisconnect(bool) { apUp = false; return true; }
  int softAPgetStationNum() { return stations; }
  void setAutoReconnect(bool) {}
  int begin(const char*, const char*) { beginCalls++; return 0; }
  bool disconnect() { return true; }
  int status() { return connected ? WL_CONNECTED : WL_DISCONNECTED; }
  String SSID() { return ssid; }
  String SSID(int i) { return nets[i].ssid; }
  int RSSI(int i) { return nets[i].rssi; }
  int encryptionType(int i) { return nets[i].enc; }
  IPAddress localIP() { return IPAddress(192, 168, 1, 57); }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  int scanNetworks(bool = false) { scanState = nets.size(); return WIFI_SCAN_RUNNING; }
  int scanComplete() { return scanState; }
  void scanDelete() { scanState = WIFI_SCAN_FAILED; }
};
extern WiFiClass WiFi;
typedef Stream WiFiClient;
class MDNSClass { public: bool begin(const char*) { return true; } void addService(const char*, const char*, int) {} };
extern MDNSClass MDNS;
typedef void (*sntp_sync_time_cb_t)(struct timeval*);
inline void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t) {}

// ---------------- web server (not served in the simulator) ----------------
enum HTTPMethod { HTTP_GET, HTTP_POST };
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status; String filename; uint8_t buf[1]; size_t currentSize; };
class WebServer {
 public:
  std::map<std::string, std::string> args;
  String lastBody; int lastCode = 0;
  WebServer(int) {}
  std::map<std::string, std::function<void()>> routes;
  void on(const char* p, HTTPMethod m, std::function<void()> f) { routes[std::string(m == HTTP_GET ? "GET " : "POST ") + p] = f; }
  void on(const char*, HTTPMethod, std::function<void()>, std::function<void()>) {}
  void onNotFound(std::function<void()>) {}
  void begin() {}
  void handleClient() {}
  String arg(const char* k) { auto it = args.find(k); return it == args.end() ? String() : String(it->second); }
  bool hasArg(const char* k) { return args.count(k); }
  std::map<std::string, std::string> headers;
  void collectHeaders(const char**, size_t) {}
  String header(const char* k) { auto it = headers.find(k); return it == headers.end() ? String() : String(it->second); }
  void send(int c, const char*, const String& b = String()) { lastCode = c; lastBody = b; }
  void send(int c, const char* t, const char* b) { send(c, t, String(b)); }
  void send_P(int c, const char*, const char*) { lastCode = c; }
  void sendHeader(const char*, const String&) {}
  void setContentLength(size_t) {}
  void sendContent(const String& s) { lastBody += s; }
  HTTPUpload& upload() { static HTTPUpload u; return u; }
  struct SimClient { IPAddress local = IPAddress(192, 168, 1, 57); IPAddress localIP() { return local; } } simClient;   // tests: which Wi-Fi the phone came in on
  SimClient& client() { return simClient; }
};

// ---------------- HTTP client: really downloads with curl ----------------
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
class SimBufStream : public Stream {
 public:
  std::string d; size_t p = 0;
  int available() override { return d.size() - p; }
  int read() override { return p < d.size() ? (uint8_t)d[p++] : -1; }
  int peek() override { return p < d.size() ? (uint8_t)d[p] : -1; }
  size_t readBytes(char* b, size_t n) override { n = std::min(n, d.size() - p); memcpy(b, d.data() + p, n); p += n; return n; }
  size_t write(uint8_t) override { return 0; }
};
class NetworkClientSecure { public: void setInsecure() {} };
extern bool g_simOffline;
class HTTPClient {
  String url_; SimBufStream st_;
 public:
  void setFollowRedirects(int) {}
  void setTimeout(int) {}
  void useHTTP10(bool) {}
  void setUserAgent(const char*) {}
  bool begin(NetworkClientSecure&, const String& u) { url_ = u; return true; }
  int GET() {
    if (g_simOffline) return -1;
    std::string cmd = "curl -s -L --max-time 20 -A 'SomudTick/3 (ESP32)' '" + url_.std() + "'";
    FILE* p = popen(cmd.c_str(), "r"); if (!p) return -1;
    char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, p)) > 0) st_.d.append(b, n);
    int rc = pclose(p);
    return rc == 0 && !st_.d.empty() ? 200 : -1;
  }
  int getSize() { return st_.d.size(); }
  bool connected() { return st_.available() > 0; }
  WiFiClient* getStreamPtr() { return &st_; }
  void end() {}
};

// ---------------- I2S audio ----------------
enum { I2S_MODE_STD, I2S_DATA_BIT_WIDTH_16BIT = 16, I2S_SLOT_MODE_MONO = 1 };
extern std::vector<int16_t> g_simI2S;   // every sample written to the sound chip (tests)
class I2SClass {
 public:
  void setPins(int, int, int, int, int) {}
  bool begin(int, int, int, int) { return false; }
  size_t write(const uint8_t* d, size_t n) { const int16_t* s = (const int16_t*)d; g_simI2S.insert(g_simI2S.end(), s, s + n / 2); return n; }   // tests read the samples
};

// ---------------- IR remote ----------------
enum panasonic_ac_remote_model_t { kPanasonicUnknown = 0, kPanasonicLke, kPanasonicNke, kPanasonicDke, kPanasonicJke, kPanasonicCkp, kPanasonicRkr };
const uint8_t kPanasonicAcAuto = 0, kPanasonicAcDry = 2, kPanasonicAcCool = 3, kPanasonicAcHeat = 4, kPanasonicAcFan = 6;
const uint8_t kPanasonicAcMinTemp = 16, kPanasonicAcMaxTemp = 30;
const uint8_t kPanasonicAcFanMin = 0, kPanasonicAcFanLow = 1, kPanasonicAcFanMed = 2, kPanasonicAcFanHigh = 3, kPanasonicAcFanMax = 4, kPanasonicAcFanAuto = 7;
const uint8_t kPanasonicAcSwingVHighest = 1, kPanasonicAcSwingVHigh = 2, kPanasonicAcSwingVMiddle = 3, kPanasonicAcSwingVLow = 4, kPanasonicAcSwingVLowest = 5, kPanasonicAcSwingVAuto = 15;
extern int g_simIrMsgs;   // IR messages sent (tests)
typedef int gpio_num_t;
enum { GPIO_DRIVE_CAP_0, GPIO_DRIVE_CAP_1, GPIO_DRIVE_CAP_2, GPIO_DRIVE_CAP_3 };
extern int g_simDriveCap[64];
inline int gpio_set_drive_capability(gpio_num_t p, int c) { g_simDriveCap[p & 63] = c; return 0; }
// The Panasonic AC state is kept in a few bytes of the 27-byte message (only the simulator reads them back):
// [13] power, [14] mode, [15] temp, [16] fan, [17] swing, [18] quiet, [19] model
const uint16_t kPanasonicAcStateLength = 27;
class IRPanasonicAc {
  uint8_t st_[27] = {0};
 public:
  IRPanasonicAc(int) {}
  void begin() {} void stateReset() { memset(st_, 0, sizeof st_); st_[0] = 0x02; st_[1] = 0x20; }
  void setModel(panasonic_ac_remote_model_t m) { st_[19] = m; } void setMode(uint8_t v) { st_[14] = v; }
  void setTemp(uint8_t v) { st_[15] = v; } void setFan(uint8_t v) { st_[16] = v; } void setSwingVertical(uint8_t v) { st_[17] = v; }
  void setQuiet(bool v) { st_[18] = v; } void setPower(bool v) { st_[13] = v; }
  void send(uint16_t repeat = 0) { g_simIrMsgs += 1 + repeat; }
  void setRaw(const uint8_t s[]) { memcpy(st_, s, sizeof st_); }
  uint8_t* getRaw() { return st_; }
  bool getPower() { return st_[13]; } uint8_t getMode() { return st_[14]; } uint8_t getTemp() { return st_[15]; }
  uint8_t getFan() { return st_[16]; } uint8_t getSwingVertical() { return st_[17]; } bool getQuiet() { return st_[18]; }
  panasonic_ac_remote_model_t getModel() { return (panasonic_ac_remote_model_t)st_[19]; }
};
// ---------------- IR receiver KY-022 (v13) and the general sender ----------------
// Tests put "received" messages in g_simIrIn (simIrPush...), the firmware reads them with IRrecv::decode().
// Everything the board sends with IRsend lands in g_simIrOut.
enum decode_type_t { UNKNOWN = -1, UNUSED = 0, RC5 = 1, RC6 = 2, NEC = 3, SONY = 4, PANASONIC = 5, JVC = 6, SAMSUNG = 7, PANASONIC_AC = 49, kLastDecodeType = 130 };
const uint16_t kNoRepeat = 0;
const uint16_t kStateSizeMax = 53;
const uint64_t kRepeat = 0xFFFFFFFFFFFFFFFFULL;
struct decode_results {
  decode_type_t decode_type;
  union { struct { uint64_t value; uint32_t address; uint32_t command; }; uint8_t state[kStateSizeMax]; };
  uint16_t bits; volatile uint16_t* rawbuf; uint16_t rawlen; bool overflow; bool repeat;
  decode_results() { memset((void*)this, 0, sizeof *this); decode_type = UNKNOWN; }
};
struct SimIrMsg { decode_results r; std::vector<uint16_t> raw; };   // raw: the durations in us (mark, space, ...)
extern std::vector<SimIrMsg> g_simIrIn;
struct SimIrSent { int type; uint64_t value; uint16_t bits; std::vector<uint8_t> state; std::vector<uint16_t> raw; };
extern std::vector<SimIrSent> g_simIrOut;
extern bool g_simIrRxOn;   // the receiver was switched on by the firmware
class IRrecv {
  std::vector<uint16_t> buf_;
 public:
  IRrecv(uint16_t, uint16_t = 100, uint8_t = 15, bool = false, uint8_t = 3) {}
  void enableIRIn(bool = false) { g_simIrRxOn = true; }
  void disableIRIn() { g_simIrRxOn = false; }
  void pause() {} void resume() {}
  void setUnknownThreshold(uint16_t) {} void setTolerance(uint8_t = 25) {}
  bool decode(decode_results* r, void* = nullptr, uint8_t = 0, uint16_t = 0) {
    if (!g_simIrRxOn || g_simIrIn.empty()) return false;
    SimIrMsg m = g_simIrIn.front(); g_simIrIn.erase(g_simIrIn.begin());
    *r = m.r;
    buf_.assign(1, 0); for (auto d : m.raw) buf_.push_back(d / 2);   // the real buffer keeps 2 us ticks, after a first gap entry
    r->rawbuf = buf_.data(); r->rawlen = buf_.size();
    return true;
  }
};
inline String typeToString(decode_type_t t, bool = false) {
  switch (t) { case NEC: return "NEC"; case SONY: return "SONY"; case SAMSUNG: return "SAMSUNG"; case RC5: return "RC5"; case RC6: return "RC6";
    case PANASONIC: return "PANASONIC"; case JVC: return "JVC"; case PANASONIC_AC: return "PANASONIC_AC"; case UNKNOWN: return "UNKNOWN"; default: return "PROTO" + String((int)t); }
}
inline bool hasACState(decode_type_t t) { return t == PANASONIC_AC; }
inline uint16_t getCorrectedRawLength(const decode_results* r) { return r->rawlen > 0 ? r->rawlen - 1 : 0; }
inline uint16_t* resultToRawArray(const decode_results* r) {
  uint16_t n = getCorrectedRawLength(r); uint16_t* a = new uint16_t[n];
  for (uint16_t i = 0; i < n; i++) a[i] = r->rawbuf[i + 1] * 2;
  return a;
}
inline String resultToHexidecimal(const decode_results* r) {
  char b[8]; String s = "0x";
  if (hasACState(r->decode_type)) { for (int i = 0; i < r->bits / 8; i++) { snprintf(b, sizeof b, "%02X", r->state[i]); s += b; } return s; }
  char v[24]; snprintf(v, sizeof v, "0x%llX", (unsigned long long)r->value); return v;
}
class IRsend {
 public:
  IRsend(uint16_t, bool = false, bool = true) {}
  void begin() {}
  bool send(decode_type_t t, uint64_t d, uint16_t n, uint16_t = 0) { g_simIrOut.push_back({(int)t, d, n, {}, {}}); return true; }
  bool send(decode_type_t t, const uint8_t* s, uint16_t n) { g_simIrOut.push_back({(int)t, 0, (uint16_t)(n * 8), std::vector<uint8_t>(s, s + n), {}}); return true; }
  void sendRaw(const uint16_t* b, uint16_t len, uint16_t) { g_simIrOut.push_back({(int)UNKNOWN, 0, 0, {}, std::vector<uint16_t>(b, b + len)}); }
};

// ---------------- extra parts on the I2C wires (v13): small screen SH1106 / SSD1306 and the PCF8574 button board ----------------
struct SimOled {
  uint8_t ram[8][132];        // the screen's memory: 8 pages of 8 rows, 132 columns (the SH1106 shows columns 2..129)
  uint8_t page = 0, col = 0, contrast = 0x7F;
  bool dispOn = false, segRemap = false, comRev = false;
  long dataBytes = 0, cmdWrites = 0;   // what was sent (tests: only changed parts should be sent)
  long dataSinceOff = 0; bool onBeforePicture = false;   // switched on with nothing written since it was dark = noise on screen
};
extern SimOled g_simOled; extern bool g_simOledOn; extern bool g_simPadOn; extern uint8_t g_simPadAddr, g_simPadDown;
extern int g_simI2cFail;   // > 0: the next n transfers to the extra parts fail (a loose wire)
inline bool simI2cWrite(uint8_t addr, const uint8_t* d, uint8_t n) {
  if (g_simI2cFail > 0) { g_simI2cFail--; return false; }
  if (addr == 0x3C && g_simOledOn) {
    if (!n) return true;
    SimOled& o = g_simOled;
    if (d[0] == 0x40) { for (int i = 1; i < n; i++) { if (o.col < 132) o.ram[o.page][o.col] = d[i]; o.col++; o.dataBytes++; o.dataSinceOff++; } return true; }
    o.cmdWrites++;
    for (int i = 1; i < n; i++) {
      uint8_t c = d[i];
      if (c <= 0x0F) o.col = (o.col & 0xF0) | c;
      else if (c <= 0x1F) o.col = (o.col & 0x0F) | ((c & 0x0F) << 4);
      else if (c >= 0xB0 && c <= 0xB7) o.page = c & 7;
      else if (c == 0xAE) { o.dispOn = false; o.dataSinceOff = 0; }
      else if (c == 0xAF) { if (!o.dispOn && !o.dataSinceOff) o.onBeforePicture = true; o.dispOn = true; }
      else if (c == 0x81 && i + 1 < n) o.contrast = d[++i];
      else if (c == 0xA0 || c == 0xA1) o.segRemap = c == 0xA1;
      else if (c == 0xC0 || c == 0xC8) o.comRev = c == 0xC8;
      else if ((c == 0xA8 || c == 0xD3 || c == 0xD5 || c == 0xD9 || c == 0xDA || c == 0xDB || c == 0x8D || c == 0xAD || c == 0x20) && i + 1 < n) i++;   // one value follows
      else if ((c == 0x21 || c == 0x22) && i + 2 < n) i += 2;
    }
    return true;
  }
  if (addr == g_simPadAddr && g_simPadOn) return true;   // (writing 0xFF = all pins are inputs)
  return false;
}
inline bool simI2cRead(uint8_t addr, uint8_t* d, uint8_t n) {
  if (g_simI2cFail > 0) { g_simI2cFail--; return false; }
  if (addr == 0x3C && g_simOledOn) { for (int i = 0; i < n; i++) d[i] = 0x43; return true; }   // (a status byte)
  if (addr == g_simPadAddr && g_simPadOn) { for (int i = 0; i < n; i++) d[i] = (uint8_t)~g_simPadDown; return true; }   // pressed = pin low
  return false;
}

// ---------------- BLE ----------------
struct SimBle { std::string name, addr; int rssi; };
extern std::vector<SimBle> g_simBle;
class BLEAddress { std::string a_; public: BLEAddress(std::string a) : a_(a) {} std::string toString() const { return a_; } };
class BLEAdvertisedDevice {
  SimBle d_;
 public:
  BLEAdvertisedDevice(const SimBle& d) : d_(d) {}
  bool haveName() { return !d_.name.empty(); }
  std::string getName() { return d_.name; }
  BLEAddress getAddress() { return BLEAddress(d_.addr); }
  int getRSSI() { return d_.rssi; }
};
class BLEScanResults { public: int getCount() { return g_simBle.size(); } BLEAdvertisedDevice getDevice(int i) { return BLEAdvertisedDevice(g_simBle[i]); } };
class BLEScan {
  BLEScanResults r_;
 public:
  void setActiveScan(bool) {} void setInterval(int) {} void setWindow(int) {}
  BLEScanResults* start(int, bool) { return &r_; }
  bool start(uint32_t, void (*cb)(BLEScanResults), bool = false) { if (cb) cb(r_); return true; }
  BLEScanResults* getResults() { return &r_; }
  void clearResults() {}
};
class BLEDevice {
 public:
  static bool& inited() { static bool b = false; return b; }
  static int& deinits() { static int n = 0; return n; }
  static bool getInitialized() { return inited(); }
  static void init(const char*) { inited() = true; }
  static void deinit(bool) { if (!inited()) return; inited() = false; deinits()++; }   // (like the real one: nothing if not on)
  static BLEScan* getScan() { static BLEScan s; return &s; }
};
