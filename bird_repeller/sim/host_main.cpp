// รันเฟิร์มแวร์ controller_board_esp32.ino บนคอม เพื่อทดสอบร่วมกับเซิร์ฟเวอร์ Pi
//
//   Serial  = stdin/stdout (ตัวจำลองจะต่อเข้ากับ pseudo-terminal ให้เอง)
//   log ขา  = stderr  เช่น "[   1234 ms] PUMP ON"
//   ค่าเซนเซอร์ = อ่านจากไฟล์ที่ชี้ด้วย env SIM_CTL ทุก 100 ms
//                 PIR=1          ขา PIR เป็น HIGH
//                 ECHO_CM=12.5   HC-SR04 วัดได้ 12.5 cm   (-1 = ไม่มีเสียงสะท้อน)
//   สาเหตุรีเซ็ต = env SIM_RESET (POWERON / BROWNOUT / ...)
#include "arduino_shim.h"

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <thread>
#include <unistd.h>

#include "../controller_board_esp32/controller_board_esp32.ino"

HostSerial Serial;
static auto t0 = std::chrono::steady_clock::now();
static int simPir = 0;
static float simEchoCm = 15.0f;
static float simVtrim = 1.0f;

unsigned long micros() {
  return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now() - t0).count();
}
unsigned long millis() { return micros() / 1000; }
void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void delayMicroseconds(unsigned int us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

static const char* pinName(int pin) {
  if (pin == PIN_PUMP) return "PUMP";
  if (pin == PIN_LAMP) return "LAMP";
  if (pin == PIN_ENA) return "ENA";
  return nullptr;
}

static int pinState[64];
void pinMode(int, int) {}
void digitalWrite(int pin, int v) {
  if (pin < 0 || pin >= 64) return;
  const char* n = pinName(pin);
  if (n && pinState[pin] != v)
    fprintf(stderr, "[%7lu ms] %s %s\n", millis(), n, v ? "HIGH" : "LOW");
  if (pin == PIN_PUL && v == HIGH) {
    static unsigned long steps = 0;
    if (++steps % 100 == 0) fprintf(stderr, "[%7lu ms] STEPS %lu pos=%ld\n", millis(), steps, posSteps);
  }
  pinState[pin] = v;
}
int digitalRead(int pin) { return pin == PIN_PIR ? simPir : 0; }
unsigned long pulseIn(int, int, unsigned long) {
  delayMicroseconds(300);
  return simEchoCm < 0 ? 0 : (unsigned long)(simEchoCm * 58.0f);
}
int analogReadMilliVolts(int) { return 2600; }

float Preferences::getFloat(const char*, float def) { return simVtrim ? simVtrim : def; }
void Preferences::putFloat(const char*, float v) { simVtrim = v; }

esp_reset_reason_t esp_reset_reason() {
  const char* r = getenv("SIM_RESET");
  if (r && !strcmp(r, "BROWNOUT")) return ESP_RST_BROWNOUT;
  if (r && !strcmp(r, "PANIC")) return ESP_RST_PANIC;
  return ESP_RST_POWERON;
}

static char rxb[256];
static int rxn = 0, rxi = 0;
int HostSerial::available() {
  if (rxi < rxn) return rxn - rxi;
  ssize_t n = ::read(0, rxb, sizeof rxb);
  if (n <= 0) return 0;
  rxn = (int)n;
  rxi = 0;
  return rxn;
}
int HostSerial::read() { return available() ? (unsigned char)rxb[rxi++] : -1; }
void HostSerial::write_raw(const char* p, size_t n) {
  while (n > 0) {
    ssize_t w = ::write(1, p, n);
    if (w <= 0) return;
    p += w;
    n -= (size_t)w;
  }
}

static void readSimCtl() {
  const char* path = getenv("SIM_CTL");
  if (!path) return;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("PIR=", 0) == 0) simPir = atoi(line.c_str() + 4);
    if (line.rfind("ECHO_CM=", 0) == 0) simEchoCm = (float)atof(line.c_str() + 8);
  }
}

int main() {
  fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
  setvbuf(stderr, nullptr, _IOLBF, 0);
  setup();
  unsigned long lastCtl = 0;
  for (;;) {
    if (millis() - lastCtl >= 100) { readSimCtl(); lastCtl = millis(); }
    loop();
    std::this_thread::sleep_for(std::chrono::microseconds(40));
  }
}
