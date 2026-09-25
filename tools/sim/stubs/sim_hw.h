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
  template <typename T> T get(const char* k, T d) { auto it = m_.find(k); if (it == m_.end() || it->second.size() != sizeof(T)) return d; T v; memcpy(&v, it->second.data(), sizeof v); return v; }
  template <typename T> size_t put(const char* k, T v) { m_[k].assign((uint8_t*)&v, (uint8_t*)&v + sizeof v); return sizeof v; }
 public:
  bool begin(const char*, bool = false) { return true; }
  bool isKey(const char* k) { return m_.count(k); }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return get(k, d); }
  int8_t getChar(const char* k, int8_t d = 0) { return get(k, d); }
  bool getBool(const char* k, bool d = false) { return get(k, d); }
  uint32_t getUInt(const char* k, uint32_t d = 0) { return get(k, d); }
  int32_t getInt(const char* k, int32_t d = 0) { return get(k, d); }
  size_t putUChar(const char* k, uint8_t v) { return put(k, v); }
  size_t putChar(const char* k, int8_t v) { return put(k, v); }
  size_t putBool(const char* k, bool v) { return put(k, v); }
  size_t putUInt(const char* k, uint32_t v) { return put(k, v); }
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
class I2SClass {
 public:
  void setPins(int, int, int, int, int) {}
  bool begin(int, int, int, int) { return false; }
  size_t write(const uint8_t*, size_t n) { return n; }
};

// ---------------- IR remote ----------------
enum panasonic_ac_remote_model_t { kPanasonicUnknown = 0, kPanasonicLke, kPanasonicNke, kPanasonicDke, kPanasonicJke, kPanasonicCkp, kPanasonicRkr };
const uint8_t kPanasonicAcAuto = 0, kPanasonicAcDry = 2, kPanasonicAcCool = 3, kPanasonicAcHeat = 4, kPanasonicAcFan = 6;
const uint8_t kPanasonicAcMinTemp = 16, kPanasonicAcMaxTemp = 30;
const uint8_t kPanasonicAcFanMin = 0, kPanasonicAcFanLow = 1, kPanasonicAcFanMed = 2, kPanasonicAcFanHigh = 3, kPanasonicAcFanMax = 4, kPanasonicAcFanAuto = 7;
const uint8_t kPanasonicAcSwingVHighest = 1, kPanasonicAcSwingVHigh = 2, kPanasonicAcSwingVMiddle = 3, kPanasonicAcSwingVLow = 4, kPanasonicAcSwingVLowest = 5, kPanasonicAcSwingVAuto = 15;
class IRPanasonicAc {
 public:
  IRPanasonicAc(int) {}
  void begin() {} void stateReset() {} void setModel(panasonic_ac_remote_model_t) {} void setMode(uint8_t) {}
  void setTemp(uint8_t) {} void setFan(uint8_t) {} void setSwingVertical(uint8_t) {} void setQuiet(bool) {} void setPower(bool) {} void send() {}
};

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
  static bool getInitialized() { return true; }
  static void init(const char*) {}
  static BLEScan* getScan() { static BLEScan s; return &s; }
};
