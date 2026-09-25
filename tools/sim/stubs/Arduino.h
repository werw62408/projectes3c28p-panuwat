// Minimal Arduino/ESP32 stand-ins so SomudTick can be built and drawn on a PC.
// Hardware calls do nothing; files live in real folders; time is a virtual clock.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <ctime>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <random>
using std::min; using std::max;

#define PROGMEM
#define PI 3.14159265358979f
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

// ---------------- virtual clock ----------------
extern uint64_t g_simMs;       // millis()
extern int64_t g_simEpoch;     // wall clock at g_simMs == 0
inline uint32_t millis() { return (uint32_t)g_simMs; }
inline void delay(uint32_t ms) { g_simMs += ms; }
inline void yield() {}
inline time_t sim_time(time_t* t) { time_t v = (time_t)(g_simEpoch + (int64_t)(g_simMs / 1000)); if (t) *t = v; return v; }
inline int sim_settimeofday(const struct timeval* tv, const void*) { g_simEpoch = tv->tv_sec - (int64_t)(g_simMs / 1000); return 0; }
#define time(x) sim_time(x)
#define settimeofday sim_settimeofday
inline void configTzTime(const char* tz, const char*, const char* = nullptr, const char* = nullptr) { setenv("TZ", tz, 1); tzset(); }

// ---------------- String ----------------
class String {
  std::string s_;
 public:
  String() {}
  String(const char* c) : s_(c ? c : "") {}
  String(const std::string& s) : s_(s) {}
  String(const String&) = default;
  String& operator=(const String&) = default;
  String& operator=(const char* c) { s_ = c ? c : ""; return *this; }
  explicit String(char c) : s_(1, c) {}
  explicit String(unsigned char v) : s_(std::to_string(v)) {}
  explicit String(int v) : s_(std::to_string(v)) {}
  explicit String(unsigned v) : s_(std::to_string(v)) {}
  explicit String(long v) : s_(std::to_string(v)) {}
  explicit String(unsigned long v) : s_(std::to_string(v)) {}
  explicit String(long long v) : s_(std::to_string(v)) {}
  explicit String(unsigned long long v) : s_(std::to_string(v)) {}
  explicit String(float v, unsigned d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, (double)v); s_ = b; }
  explicit String(double v, unsigned d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); s_ = b; }
  const char* c_str() const { return s_.c_str(); }
  unsigned length() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  void reserve(size_t n) { s_.reserve(n); }
  char operator[](size_t i) const { return i < s_.size() ? s_[i] : 0; }
  char& operator[](size_t i) { return s_[i]; }
  char charAt(size_t i) const { return (*this)[i]; }
  std::string::iterator begin() { return s_.begin(); }
  std::string::iterator end() { return s_.end(); }
  std::string::const_iterator begin() const { return s_.begin(); }
  std::string::const_iterator end() const { return s_.end(); }
  String substring(int a) const { if (a < 0) a = 0; if ((size_t)a >= s_.size()) return String(); return String(s_.substr(a)); }
  String substring(int a, int b) const { if (a > b) std::swap(a, b); if (a < 0) a = 0; if ((size_t)a >= s_.size()) return String(); if ((size_t)b > s_.size()) b = s_.size(); return String(s_.substr(a, b - a)); }
  int indexOf(char c, int from = 0) const { if (from < 0) from = 0; auto p = s_.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String& t, int from = 0) const { if (from < 0) from = 0; auto p = s_.find(t.s_, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const char* t, int from = 0) const { return indexOf(String(t), from); }
  int lastIndexOf(char c) const { auto p = s_.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c, int from) const { if (from < 0) return -1; auto p = s_.rfind(c, from); return p == std::string::npos ? -1 : (int)p; }
  long toInt() const { return atol(s_.c_str()); }
  float toFloat() const { return (float)atof(s_.c_str()); }
  void trim() { size_t a = 0, b = s_.size(); while (a < b && isspace((unsigned char)s_[a])) a++; while (b > a && isspace((unsigned char)s_[b - 1])) b--; s_ = s_.substr(a, b - a); }
  void toLowerCase() { for (auto& c : s_) c = tolower((unsigned char)c); }
  void toUpperCase() { for (auto& c : s_) c = toupper((unsigned char)c); }
  void replace(const String& a, const String& b) { if (a.s_.empty()) return; size_t p = 0; while ((p = s_.find(a.s_, p)) != std::string::npos) { s_.replace(p, a.s_.size(), b.s_); p += b.s_.size(); } }
  void remove(unsigned i) { if (i < s_.size()) s_.erase(i); }
  void remove(unsigned i, unsigned n) { if (i < s_.size()) s_.erase(i, n); }
  bool startsWith(const String& p) const { return s_.compare(0, p.s_.size(), p.s_) == 0; }
  bool endsWith(const String& p) const { return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0; }
  bool equals(const String& o) const { return s_ == o.s_; }
  bool concat(const char* c) { s_ += c ? c : ""; return true; }
  bool concat(const char* c, size_t n) { s_.append(c, n); return true; }
  bool concat(const String& o) { s_ += o.s_; return true; }
  bool concat(char c) { s_ += c; return true; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* c) { s_ += c ? c : ""; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value, int>::type = 0>
  String& operator+=(T v) { s_ += String(v).s_; return *this; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* c) const { return s_ == (c ? c : ""); }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  bool operator!=(const char* c) const { return !(*this == c); }
  bool operator<(const String& o) const { return s_ < o.s_; }
  bool operator>(const String& o) const { return s_ > o.s_; }
  const std::string& std() const { return s_; }
  // for ArduinoJson
  size_t write(uint8_t c) { s_ += (char)c; return 1; }
};
inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, char c) { String r(a); r += c; return r; }
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value, int>::type = 0>
inline String operator+(const String& a, T v) { String r(a); r += String(v); return r; }

// ---------------- Print / Stream ----------------
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t print(const String& s) { return write((const uint8_t*)s.c_str(), s.length()); }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t println(const String& s = String()) { return print(s) + print("\n"); }
  size_t printf(const char* f, ...) { char b[512]; va_list a; va_start(a, f); int n = vsnprintf(b, sizeof b, f, a); va_end(a); return write((const uint8_t*)b, n); }
};
class Printable { public: virtual ~Printable() {} virtual size_t printTo(Print&) const = 0; };
class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual size_t readBytes(char* b, size_t n) { size_t k = 0; while (k < n) { int c = read(); if (c < 0) break; b[k++] = c; } return k; }
  size_t readBytes(uint8_t* b, size_t n) { return readBytes((char*)b, n); }
  String readStringUntil(char t) { std::string o; int c; while ((c = read()) >= 0 && c != t) o += (char)c; return String(o); }
  void setTimeout(unsigned long) {}
};
class HardwareSerial : public Print {
 public:
  bool quiet = true;
  void begin(unsigned long) {}
  size_t write(uint8_t c) override { if (!quiet) fputc(c, stderr); return 1; }
};
extern HardwareSerial Serial;

// ---------------- pins / misc ----------------
extern int g_simBatMv;
inline void pinMode(int, int) {}
extern int g_simAnalog[64], g_simDigital[64];   // pin levels tests can set (joystick: IO2, IO3, IO14)
inline int digitalRead(int p) { return (p >= 0 && p < 64) ? g_simDigital[p] : HIGH; }
inline void digitalWrite(int, int) {}
inline int analogRead(int p) { return (p >= 0 && p < 64) ? g_simAnalog[p] : 2048; }
inline uint32_t analogReadMilliVolts(int) { return g_simBatMv; }
inline void analogReadResolution(int) {}
inline void rgbLedWrite(int, uint8_t, uint8_t, uint8_t) {}
extern std::mt19937 g_simRng;
inline long random(long n) { return n > 0 ? (long)(g_simRng() % n) : 0; }
inline long random(long a, long b) { return b > a ? a + (long)(g_simRng() % (b - a)) : a; }
inline uint32_t esp_random() { return g_simRng(); }
inline void randomSeed(unsigned long v) { g_simRng.seed(v); }
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void* heap_caps_malloc(size_t n, int) { return malloc(n); }
typedef void* TaskHandle_t;
inline int xTaskCreatePinnedToCore(void (*fn)(void*), const char*, int, void* arg, int, TaskHandle_t* h, int) { if (h) *h = nullptr; fn(arg); return 1; }   // simulator: runs the job at once
inline void vTaskDelete(void*) {}
