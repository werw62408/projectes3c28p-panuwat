// ชุดจำลอง Arduino/ESP32 แบบย่อ สำหรับรันเฟิร์มแวร์บนคอม (Linux) เพื่อทดสอบ
// ไม่ได้ใช้บนบอร์ดจริง — มีแค่ฟังก์ชันที่ controller_board_esp32.ino เรียกใช้
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLDOWN 2
#define ADC_11db 3

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int v);
int digitalRead(int pin);
unsigned long pulseIn(int pin, int level, unsigned long timeoutUs);
int analogReadMilliVolts(int pin);
inline void analogSetPinAttenuation(int, int) {}

template <class A, class B>
inline typename std::common_type<A, B>::type min(A a, B b) { return a < b ? a : b; }

class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c) {}
  String(const std::string& x) : s(x) {}
  unsigned int length() const { return (unsigned int)s.size(); }
  void reserve(unsigned int n) { s.reserve(n); }
  void trim() {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    s = s.substr(a, b - a);
  }
  void toUpperCase() { for (auto& c : s) c = (char)toupper((unsigned char)c); }
  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
  int indexOf(char c) const { size_t i = s.find(c); return i == std::string::npos ? -1 : (int)i; }
  String substring(unsigned int from) const { return from >= s.size() ? String() : String(s.substr(from)); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  const char* c_str() const { return s.c_str(); }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const char* o) const { return s == o; }
  bool operator!=(const char* o) const { return s != o; }
  bool operator==(const String& o) const { return s == o.s; }
};

class HostSerial {
 public:
  void begin(long) {}
  int available();
  int read();
  void write_raw(const char* p, size_t n);
  void print(const char* p) { write_raw(p, strlen(p)); }
  void print(const String& p) { print(p.c_str()); }
  void print(long v) { printf("%ld", v); }
  void print(int v) { printf("%d", v); }
  void print(unsigned long v) { printf("%lu", v); }
  void print(double v, int digits = 2) { printf("%.*f", digits, v); }
  void println(const char* p = "") { print(p); print("\r\n"); }
  void println(long v) { print(v); println(); }
  void println(int v) { print(v); println(); }
  void printf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) write_raw(buf, std::min((size_t)n, sizeof buf - 1));
  }
};
extern HostSerial Serial;

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  float getFloat(const char*, float def);
  void putFloat(const char*, float v);
};

typedef enum {
  ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC,
  ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP,
  ESP_RST_BROWNOUT, ESP_RST_SDIO
} esp_reset_reason_t;
esp_reset_reason_t esp_reset_reason();
