/* ==========================================================================
   บอร์ดควบคุมโหลด  ESP32  —  เฟิร์มแวร์ v7
   โครงงาน: เครื่องไล่นกอัตโนมัติ

   หน้าที่: รับคำสั่งจาก Raspberry Pi ผ่าน USB Serial แล้วสั่งงานอุปกรณ์
            พร้อมรายงานสถานะกลับ และตัดโหลดเองเมื่อเกิดเหตุผิดปกติ

   เปลี่ยนจาก v6 (รุ่นนี้ — เน้นความปลอดภัยของปั๊มและมอเตอร์):
     - สั่ง PUMP ซ้ำตอนปั๊มเปิดอยู่จะถูกปฏิเสธ (เดิมยืดเวลาได้ไม่รู้จบ)
     - เพิ่มโควตาเวลาปั๊มต่อ 10 นาที กันปั๊มเดินแห้งตอนยังไม่มีเซนเซอร์น้ำ
     - รอบไล่เช็กก่อนว่าปั๊มเปิดได้จริง ถ้าไม่ได้จะตอบ ERR พร้อมเหตุผล
     - เซนเซอร์น้ำอ่านไม่ได้ติดกันหลายรอบ = ล็อกปั๊มไว้ก่อน (fail-safe)
     - ห้ามสั่งมอเตอร์ใหม่ระหว่างหมุน / MOT STOP ชะลอก่อนหยุด กันสเต็ปหลุด
     - เพิ่ม MOT ZERO ตั้งจุดกลาง และรายงาน POSOK ว่าตำแหน่งเชื่อถือได้ไหม
     - บอกสาเหตุการรีบูต (เช่น ไฟตก) ให้ Pi รู้ว่าตำแหน่งมอเตอร์อาจเพี้ยน
     - วัดแบตไม่ทำตอนมอเตอร์หมุน (เดิมหน่วงลูป 32 ms ทำให้มอเตอร์กระตุก)
     - บรรทัดคำสั่งยาวเกินจะถูกทิ้งทั้งบรรทัด ไม่เอาครึ่งบรรทัดไปทำงาน
     - ค่า VCAL จำไว้ในแฟลช รีบูตแล้วไม่หาย

   เปลี่ยนจาก v4:
     - รีเลย์กลับมาใช้ขาแบบ OUTPUT ธรรมดา และสั่งงานด้วย HIGH
       (จัมเปอร์บนโมดูลรีเลย์ต้องอยู่ฝั่ง H)
     - ย้ายขา ENA ของ TB6600 จาก GPIO4 ไป GPIO25
       GPIO4 เป็น strapping pin ทำให้แฟลชไม่ผ่าน (Wrong boot mode 0xb)

   เปลี่ยนจาก v3:
     - รีเลย์ใช้ขาแบบ OUTPUT_OPEN_DRAIN แก้อาการรีเลย์ติดค้างตลอด
       (ESP32 ส่ง HIGH ได้แค่ 3.3V ดันออปโตฝั่ง L ให้ดับไม่ไหว)
     - ย้ายขา PUL/DIR ไป 32/33
     - เปิด HAS_STATUS_LAMP และ HAS_PIR

   เปลี่ยนจาก v2:
     - ถอดเลเซอร์ออกทั้งหมด
     - เปิดใช้ PIR 1 ตัว
     - เพิ่มวัดระดับน้ำด้วย HC-SR04 + ล็อกปั๊มเมื่อน้ำหมด (กันปั๊มเดินแห้ง)
     - เพิ่มไฟส้มบอกความพร้อม ขับผ่านรีเลย์ช่อง 2 (ไฟ 12V)
     - เพิ่มวัดแรงดันแบตผ่าน ADC
     - AUTO_DISABLE_MS = 3000 ปล่อยแรงบิดเมื่อมอเตอร์นิ่ง (ประหยัดไฟ)

   Arduino IDE: Board = ESP32 Dev Module / ไม่ต้องลงไลบรารีเพิ่ม
   ========================================================================== */

#define FW_ID "BIRDCTRL v7"

#include <Preferences.h>
#include <esp_system.h>

/* ==========================================================================
   0. สวิตช์เปิดปิดอุปกรณ์   <<<<<  แก้ตรงนี้ตอนทยอยต่ออุปกรณ์
   --------------------------------------------------------------------------
   ต่ออุปกรณ์ตัวไหนเสร็จแล้ว ค่อยเปลี่ยนตัวนั้นเป็น true แล้วแฟลชใหม่
   ตัวที่ยังเป็น false จะถูกข้ามทั้งหมด ไม่อ่าน ไม่เขียน ไม่ตั้งค่าขา

   ทำไมต้องมี: ขาอินพุตที่ยังไม่ได้ต่ออะไรจะ "ลอย" และรับสัญญาณรบกวน
   อ่านค่าออกมามั่ว ๆ ได้ โดยเฉพาะ HC-SR04 ถ้าอ่านได้ระยะมั่วจนคิดว่า
   น้ำหมด มันจะล็อกปั๊มไว้ทันที แล้วหาสาเหตุยากมาก
   ========================================================================== */
const bool HAS_MOTOR        = true;    // TB6600 + สเต็ปเปอร์
const bool HAS_PUMP         = true;    // รีเลย์ + ปั๊มน้ำ
const bool HAS_WATER_SENSOR = false;   // HC-SR04 วัดระดับน้ำ
const bool HAS_STATUS_LAMP  = true;    // ไฟส้มผ่านรีเลย์ช่อง 2
const bool HAS_VBAT_SENSE   = false;   // ตัวแบ่งแรงดันวัดแบต
const bool HAS_PIR          = true;    // HC-SR501

/* ==========================================================================
   1. ผังขา
   ========================================================================== */
const int PIN_PUL   = 32;   // TB6600 PUL+
const int PIN_DIR   = 33;   // TB6600 DIR+
const int PIN_ENA   = 25;    // TB6600 ENA+
const int PIN_PUMP  = 18;   // รีเลย์ช่อง 1 -> ปั๊มน้ำ 12V
const int PIN_PIR   = 21;   // HC-SR501 OUT (3.3V ต่อตรงได้)
const int PIN_TRIG  = 22;   // HC-SR04 TRIG
const int PIN_ECHO  = 23;   // HC-SR04 ECHO  <-- ต้องผ่านตัวแบ่งแรงดัน 1k/2k
const int PIN_LAMP  = 19;   // รีเลย์ช่อง 2 -> ไฟส้ม 12V

/*  ไฟเขียวกับไฟแดงไม่มีในผังนี้ เพราะไม่ได้ต่อกับ ESP32 เลย
    ไฟเขียว : ต่อคร่อมไฟ 12V หลังสวิตช์เปิดเครื่อง ติดทันทีที่มีไฟ
    ไฟแดง   : ต่อผ่านหน้าสัมผัส NO ของปุ่มฉุกเฉิน ติดค้างเองจนกว่าจะบิดคลาย  */
const int PIN_VBAT  = 34;   // ADC วัดแรงดันแบต <-- ผ่านตัวแบ่ง 100k/27k

/*  โมดูลรีเลย์ 4 ช่อง "High/Low Level Trigger"  เลี้ยง 5V
    ---------------------------------------------------------------------
    เวอร์ชันนี้สั่งงานด้วย HIGH  ->  จัมเปอร์เหลืองบนโมดูลต้องอยู่ฝั่ง H

      สั่งเปิด  -> ขาส่ง 3.3V ออก -> ออปโตนำกระแส -> รีเลย์ติด
      สั่งปิด   -> ขาเป็น 0V      -> ไม่มีกระแส   -> รีเลย์ดับสนิท

    เหตุผลที่ไม่ใช้ฝั่ง L: ฝั่ง L ต้องดัน IN ขึ้น 5V ถึงจะดับ แต่ ESP32
    ส่งได้แค่ 3.3V เหลือคร่อมออปโต 1.7V ซึ่งยังพอให้กระแสไหล รีเลย์เลย
    ติดค้างตลอด                                                        */
const bool RELAY_ACTIVE_LOW = false;  // จัมเปอร์ฝั่ง H -> สั่งงานด้วย HIGH
const bool ENA_ACTIVE_LOW   = true;   // TB6600 ENA เป็น active-low

/* ==========================================================================
   2. ค่าตั้งของมอเตอร์
   ========================================================================== */
const int  MICROSTEP     = 8;                    // ต้องตรงกับ DIP switch บน TB6600
const int  STEPS_PER_REV = 200 * MICROSTEP;      // = 1600
const int  LIMIT_STEPS   = 400;                  // กำแพงแข็ง ±90 องศา

const unsigned int  PULSE_US      = 5;           // ความกว้างพัลส์
const unsigned long SPEED_START   = 1400;        // us ต่อสเต็ป ตอนเริ่ม (ช้า)
const unsigned long SPEED_MAX     = 320;         // us ต่อสเต็ป ตอนเร็วสุด
const int           RAMP_STEPS    = 160;         // กี่สเต็ปถึงจะเร่งเต็ม

const unsigned long ENA_SETTLE_MS   = 5;         // รอไดร์เวอร์พร้อมก่อนยิงพัลส์แรก
const unsigned long AUTO_DISABLE_MS = 3000;      // นิ่งกี่ ms แล้วปล่อยแรงบิด
                                                 // 0 = ไม่ปล่อย (กินไฟเพิ่ม ~1A)

/* ==========================================================================
   3. ค่าตั้งของปั๊มและรอบไล่
   ========================================================================== */
const unsigned long PUMP_MAX_MS  = 8000;   // เพดานเวลาเปิดปั๊มต่อครั้ง
const unsigned long PUMP_MIN_GAP = 3000;   // ต้องพักกี่ ms ก่อนเปิดซ้ำ

// โควตาปั๊ม: ใน 10 นาทีเปิดรวมกันได้ไม่เกินกี่ ms
// ตอนยังไม่มีเซนเซอร์น้ำ นี่คือด่านเดียวที่กันปั๊มเดินแห้งนาน ๆ
const unsigned long PUMP_DUTY_WINDOW_MS = 600000;
const unsigned long PUMP_DUTY_MAX_MS    = 60000;
const unsigned long PUMP_MIN_RUN_MS     = 300;   // เหลือโควตาน้อยกว่านี้ = ไม่เปิด

const unsigned long REPEL_PUMP_MS  = 4000; // รอบไล่: เปิดปั๊มกี่ ms
const int           REPEL_SWEEP    = 300;  // รอบไล่: กวาดข้างละกี่สเต็ป

/* ==========================================================================
   4. ค่าตั้งของถังน้ำ  (แก้ 2 บรรทัดนี้ให้ตรงกับถังจริง)
   ========================================================================== */
// วัดจากหน้าเซนเซอร์ (ปากท่อ PVC ที่ยกขึ้น) ดิ่งลงไปถึงก้นถัง
const float TANK_SENSOR_TO_BOTTOM_CM = 28.0;
// วัดจากหน้าเซนเซอร์ ลงไปถึงผิวน้ำ ตอนเติมเต็ม
const float TANK_SENSOR_TO_FULL_CM   = 11.0;

const int WATER_LOW_PCT   = 20;   // ต่ำกว่านี้ = เตือน (ไฟส้มกระพริบ)
const int WATER_EMPTY_PCT = 5;    // ต่ำกว่านี้ = ล็อกปั๊ม ห้ามทำงาน

const unsigned long WATER_READ_EVERY_MS = 2000;   // อ่านทุกกี่ ms
const unsigned long ECHO_TIMEOUT_US     = 15000;  // ~2.5 เมตร พอเหลือเฟือ
const int WATER_FAIL_LIMIT = 3;   // อ่านไม่ได้ติดกันกี่รอบ ถือว่าเซนเซอร์เสีย แล้วล็อกปั๊ม

/* ==========================================================================
   5. ค่าตั้งของแบตเตอรี่
   ========================================================================== */
// ตัวแบ่งแรงดัน: R1 จากขั้วบวกแบต, R2 ลงกราวด์, จุดกลางเข้า PIN_VBAT
const float VBAT_R1_K = 100.0;
const float VBAT_R2_K = 27.0;
const float VBAT_DIVIDER = (VBAT_R1_K + VBAT_R2_K) / VBAT_R2_K;   // = 4.70
float VBAT_TRIM = 1.000;   // ตัวคูณแก้ค่า ถ้าเทียบมัลติมิเตอร์แล้วเพี้ยน
                           // ใช้คำสั่ง  VCAL <แรงดันจริง>  ปรับอัตโนมัติได้

const float VBAT_LOW_V = 11.5;    // ต่ำกว่านี้ = เตือนแบตอ่อน

/* ==========================================================================
   6. ค่าตั้งของ PIR และการเชื่อมต่อ
   ========================================================================== */
#define PIR_ENABLED HAS_PIR            // ใช้สวิตช์จากบล็อก 0 ด้านบน
const unsigned long PIR_LOCKOUT_MS  = 5000;   // ทริกแล้วห้ามทริกซ้ำในกี่ ms
const unsigned long PIR_WARMUP_MS   = 60000;  // HC-SR501 ต้องอุ่นตัว 1 นาที

const unsigned long LINK_TIMEOUT_MS = 15000;  // เงียบจาก Pi เกินนี้ = ตัดโหลด

/* ==========================================================================
   7. ตัวแปรสถานะ
   ========================================================================== */
long  posSteps   = 0;          // ตำแหน่งปัจจุบัน (สเต็ป) 0 = จุดกลาง
long  targetStep = 0;
bool  moving     = false;
bool  dirPositive = true;
long  stepsDone  = 0;
long  stepsTotal = 0;
unsigned long nextStepUs = 0;
unsigned long lastMoveEndMs = 0;
bool  enaOn = false;
bool  posKnown = false;        // false = ยังไม่เคยตั้งจุดกลาง (MOT ZERO) หลังบูต
                               // posSteps = 0 แค่แปลว่า "ตรงที่แกนอยู่ตอนเปิดเครื่อง"

bool  pumpOn = false;
unsigned long pumpOnAtMs = 0;
unsigned long pumpOffAtMs = 0;
unsigned long pumpLastOffMs = 0;
unsigned long dutyWindowStartMs = 0;
unsigned long dutyUsedMs = 0;  // เวลาปั๊มที่ใช้ไปแล้วในหน้าต่าง 10 นาทีนี้

float waterCm     = -1.0;
int   waterPct    = -1;
bool  waterLow    = false;
bool  waterEmpty  = false;
bool  waterFault  = false;     // เซนเซอร์อ่านไม่ได้ติดกัน -> ถือว่าน้ำหมดไว้ก่อน
int   waterFailCount = 0;
unsigned long lastWaterMs = 0;

float vbat = 0.0;
bool  vbatLow = false;

bool  pirPrev = false;
unsigned long pirBlockUntil = 0;

unsigned long lastHostMs = 0;
bool  linkDown = true;
bool  camOnline = false;      // Pi เป็นคนบอกด้วยคำสั่ง CAM 1 / CAM 0
bool  lampOn = false;
unsigned long bootMs = 0;

enum RepelStep { RP_IDLE, RP_SWEEP_R, RP_SWEEP_L, RP_HOME };
RepelStep repelStep = RP_IDLE;

// ต้องประกาศก่อนฟังก์ชันแรก เพราะ Arduino IDE สร้าง prototype ไว้ด้านบนให้เอง
enum PumpResult { PUMP_OK, PUMP_NONE, PUMP_EMPTY, PUMP_GAP, PUMP_BUSY, PUMP_DUTY };
unsigned long repelEndMs = 0;

String rxBuf = "";
bool   rxOverflow = false;      // บรรทัดนี้ยาวเกิน ทิ้งทั้งบรรทัดเมื่อเจอขึ้นบรรทัดใหม่

Preferences prefs;
const char* resetReasonText = "UNKNOWN";

/* ==========================================================================
   8. ตัวช่วยระดับล่าง
   ========================================================================== */
void relayWrite(int pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void enaWrite(bool on) {
  enaOn = on;
  digitalWrite(PIN_ENA, ENA_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

bool pirRaw() {
  if (!PIR_ENABLED) return false;
  if (millis() - bootMs < PIR_WARMUP_MS) return false;   // ยังอุ่นตัวไม่เสร็จ
  return digitalRead(PIN_PIR) == HIGH;
}

/* ==========================================================================
   9. ปั๊มน้ำ  (มีด่านกันเดินแห้ง)
   ========================================================================== */
const char* pumpErrText(PumpResult r) {
  switch (r) {
    case PUMP_NONE:  return "ERR NO PUMP";
    case PUMP_EMPTY: return "ERR WATER EMPTY";
    case PUMP_GAP:   return "ERR PUMP GAP";
    case PUMP_BUSY:  return "ERR PUMP BUSY";
    case PUMP_DUTY:  return "ERR PUMP DUTY";
    default:         return "OK";
  }
}

void dutyRoll() {
  if (millis() - dutyWindowStartMs >= PUMP_DUTY_WINDOW_MS) {
    dutyWindowStartMs = millis();
    dutyUsedMs = 0;
  }
}

unsigned long dutyLeftMs() {
  dutyRoll();
  unsigned long used = dutyUsedMs + (pumpOn ? millis() - pumpOnAtMs : 0);
  return used >= PUMP_DUTY_MAX_MS ? 0 : PUMP_DUTY_MAX_MS - used;
}

// เช็กอย่างเดียวว่าเปิดปั๊มได้ไหม ไม่ได้เปิดจริง
PumpResult pumpCheck() {
  if (!HAS_PUMP) return PUMP_NONE;
  if (waterEmpty) return PUMP_EMPTY;                      // น้ำหมด ห้ามเปิดเด็ดขาด
  if (pumpOn) return PUMP_BUSY;                           // ห้ามยืดเวลาด้วยการสั่งซ้ำ
  if (millis() - pumpLastOffMs < PUMP_MIN_GAP) return PUMP_GAP;
  if (dutyLeftMs() < PUMP_MIN_RUN_MS) return PUMP_DUTY;
  return PUMP_OK;
}

PumpResult pumpStart(unsigned long durMs) {
  PumpResult r = pumpCheck();
  if (r != PUMP_OK) return r;
  if (durMs == 0 || durMs > PUMP_MAX_MS) durMs = PUMP_MAX_MS;
  unsigned long left = dutyLeftMs();
  if (durMs > left) durMs = left;                         // ตัดให้พอดีโควตาที่เหลือ
  pumpOn = true;
  pumpOnAtMs = millis();
  pumpOffAtMs = millis() + durMs;
  relayWrite(PIN_PUMP, true);
  return PUMP_OK;
}

void pumpStop() {
  if (pumpOn) {
    pumpLastOffMs = millis();
    dutyRoll();
    dutyUsedMs += millis() - pumpOnAtMs;
  }
  pumpOn = false;
  relayWrite(PIN_PUMP, false);
}

void servicePump() {
  if (pumpOn && (long)(millis() - pumpOffAtMs) >= 0) pumpStop();
  if (pumpOn && waterEmpty) {                             // น้ำหมดกลางคัน
    pumpStop();
    Serial.println("EVT PUMP CUT WATER");
  }
}

/* ==========================================================================
   10. มอเตอร์สเต็ปเปอร์  (ไม่บล็อกลูปหลัก มีแรมป์เร่ง-ชะลอ)
   ========================================================================== */
unsigned long stepIntervalUs() {
  long remain = stepsTotal - stepsDone;
  long ramp = min((long)RAMP_STEPS, stepsTotal / 2);
  long phase = min(stepsDone, remain);
  if (ramp <= 0) return SPEED_MAX;
  if (phase >= ramp) return SPEED_MAX;
  float k = (float)phase / (float)ramp;
  return (unsigned long)(SPEED_START - (SPEED_START - SPEED_MAX) * k);
}

void stopMove() {
  moving = false;
  stepsDone = stepsTotal = 0;
  lastMoveEndMs = millis();
}

// หยุดแบบชะลอ: ลดความเร็วตามแรมป์จนช้าสุดแล้วค่อยหยุด ตำแหน่งจึงไม่หลุด
// (เดิม MOT STOP หยุดทันทีจากความเร็วสูงสุด แกนไหลเลยได้แต่ตัวนับไม่รู้)
void softStop() {
  if (!moving) return;
  long remain = stepsTotal - stepsDone;
  long ramp = min((long)RAMP_STEPS, stepsTotal / 2);
  long phase = min(stepsDone, remain);
  long k = min(phase, ramp);                // ต้องใช้อีกกี่สเต็ปถึงจะช้าสุด
  if (k <= 1) { stopMove(); return; }
  stepsTotal = stepsDone + k;
  targetStep = posSteps + (dirPositive ? k : -k);
}

void startMoveTo(long target) {
  if (!HAS_MOTOR) return;
  if (target >  LIMIT_STEPS) target =  LIMIT_STEPS;       // กำแพงแข็ง
  if (target < -LIMIT_STEPS) target = -LIMIT_STEPS;
  long delta = target - posSteps;
  if (delta == 0) { stopMove(); return; }

  targetStep  = target;
  dirPositive = (delta > 0);
  stepsTotal  = labs(delta);
  stepsDone   = 0;

  digitalWrite(PIN_DIR, dirPositive ? HIGH : LOW);
  if (!enaOn) { enaWrite(true); delay(ENA_SETTLE_MS); }   // ปลุกไดร์เวอร์ก่อน
  delayMicroseconds(10);                                  // DIR setup time

  moving = true;
  nextStepUs = micros();
}

void serviceMotor() {
  if (moving) {
    unsigned long now = micros();
    if ((long)(now - nextStepUs) >= 0) {
      digitalWrite(PIN_PUL, HIGH);
      delayMicroseconds(PULSE_US);
      digitalWrite(PIN_PUL, LOW);

      posSteps += dirPositive ? 1 : -1;
      stepsDone++;
      if (stepsDone >= stepsTotal) { posSteps = targetStep; stopMove(); }
      else nextStepUs = now + stepIntervalUs();
    }
    return;
  }
  // นิ่งแล้ว ครบเวลาก็ปล่อยแรงบิดเพื่อประหยัดไฟ
  if (AUTO_DISABLE_MS > 0 && enaOn && repelStep == RP_IDLE &&
      millis() - lastMoveEndMs > AUTO_DISABLE_MS) {
    enaWrite(false);
  }
}

/* ==========================================================================
   11. รอบไล่นกอัตโนมัติ  (กวาดซ้าย-ขวาพร้อมพ่นน้ำ)
   ========================================================================== */
void repelAbort() {
  repelStep = RP_IDLE;
  stopMove();
  pumpStop();
}

// คืน PUMP_OK ถ้าเริ่มรอบไล่ได้ ไม่งั้นคืนเหตุผลที่ปั๊มเปิดไม่ได้
// เช็กก่อนเริ่ม จะได้ไม่กวาดหัวฉีดไปมาโดยไม่มีน้ำออก
PumpResult repelStart() {
  PumpResult r = pumpCheck();
  if (r != PUMP_OK) return r;
  startMoveTo(0);                       // กลับจุดกลางก่อนเสมอ กันตำแหน่งเพี้ยน
  repelStep = RP_HOME;
  repelEndMs = millis() + REPEL_PUMP_MS;
  return PUMP_OK;
}

void serviceRepel() {
  if (repelStep == RP_IDLE) return;

  bool timeUp = (long)(millis() - repelEndMs) >= 0;

  if (repelStep == RP_HOME && !moving) {
    PumpResult r = pumpStart(REPEL_PUMP_MS);
    if (r != PUMP_OK) {                 // เช่นน้ำหมดหรือโควตาหมดระหว่างกลับจุดกลาง
      repelAbort();
      Serial.printf("EVT REPEL FAIL %s\n", pumpErrText(r) + 4);
      return;
    }
    repelEndMs = millis() + REPEL_PUMP_MS;
    startMoveTo(REPEL_SWEEP);
    repelStep = RP_SWEEP_R;
    return;
  }

  // ปั๊มดับก่อนเวลา (น้ำหมด/โควตาหมด) ก็ไม่ต้องกวาดต่อให้เปลืองไฟ
  if (!pumpOn) timeUp = true;

  if (timeUp && !moving) {              // หมดเวลาพ่น -> เก็บงาน
    pumpStop();
    startMoveTo(0);
    repelStep = RP_IDLE;
    Serial.println("EVT REPEL DONE");
    return;
  }

  if (!moving) {                        // ยังไม่หมดเวลา -> กวาดกลับอีกฝั่ง
    if (repelStep == RP_SWEEP_R)      { startMoveTo(-REPEL_SWEEP); repelStep = RP_SWEEP_L; }
    else if (repelStep == RP_SWEEP_L) { startMoveTo( REPEL_SWEEP); repelStep = RP_SWEEP_R; }
  }
}

/* ==========================================================================
   12. วัดระดับน้ำ  HC-SR04
   ========================================================================== */
float readDistanceOnceCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1.0;
  return (float)us / 58.0f;              // ความเร็วเสียง ~343 m/s ที่ 20C
}

void serviceWater() {
  if (!HAS_WATER_SENSOR) return;     // ไม่ต่อ = ไม่อ่าน ค่าคงอยู่ที่ -1
                                     // และ waterEmpty คงเป็น false ปั๊มจึงใช้ได้ปกติ
  if (millis() - lastWaterMs < WATER_READ_EVERY_MS) return;
  if (moving) return;                    // ห้ามวัดตอนมอเตอร์หมุน pulseIn จะกวนจังหวะ
  lastWaterMs = millis();

  // วัด 5 ครั้งแล้วเอาค่ากลาง ตัดค่าหลุดจากคลื่นน้ำกระเพื่อม
  float s[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    float d = readDistanceOnceCm();
    if (d > 1.0 && d < 400.0) s[n++] = d;
    delay(12);                           // HC-SR04 ต้องพักระหว่างยิง
  }
  if (n < 3) {                           // อ่านไม่ได้
    // ครั้งเดียวอาจเป็นคลื่นน้ำ แต่ถ้าติดกันหลายรอบ แปลว่าสายหลุดหรือเซนเซอร์เสีย
    // ต้องล็อกปั๊มไว้ก่อน ไม่งั้นค่าเก่าค้างอยู่แล้วปั๊มเดินแห้งได้โดยไม่มีใครรู้
    if (++waterFailCount >= WATER_FAIL_LIMIT && !waterFault) {
      waterFault = true;
      waterEmpty = true;
      Serial.println("EVT WATER FAULT");
    }
    return;
  }
  waterFailCount = 0;
  if (waterFault) {
    waterFault = false;
    Serial.println("EVT WATER SENSOR OK");
  }

  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (s[j] < s[i]) { float t = s[i]; s[i] = s[j]; s[j] = t; }
  float med = s[n / 2];

  waterCm = med;

  float span = TANK_SENSOR_TO_BOTTOM_CM - TANK_SENSOR_TO_FULL_CM;
  int pct = (int)(((TANK_SENSOR_TO_BOTTOM_CM - med) / span) * 100.0f + 0.5f);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  bool wasLow = waterLow, wasEmpty = waterEmpty;
  waterPct   = pct;
  waterLow   = (pct <= WATER_LOW_PCT);
  waterEmpty = (pct <= WATER_EMPTY_PCT);

  if (waterEmpty && !wasEmpty)      Serial.printf("EVT WATER EMPTY %d\n", pct);
  else if (waterLow && !wasLow)     Serial.printf("EVT WATER LOW %d\n", pct);
  else if (!waterLow && wasLow)     Serial.printf("EVT WATER OK %d\n", pct);
}

/* ==========================================================================
   13. วัดแรงดันแบต
   ========================================================================== */
void serviceBattery() {
  if (!HAS_VBAT_SENSE) return;
  if (moving) return;                    // อ่าน ADC 16 ครั้งใช้ ~32 ms มอเตอร์จะกระตุก
  static unsigned long last = 0;
  if (millis() - last < 3000) return;
  last = millis();

  long sum = 0;
  for (int i = 0; i < 16; i++) { sum += analogReadMilliVolts(PIN_VBAT); delay(2); }
  float mv = sum / 16.0f;
  vbat = (mv / 1000.0f) * VBAT_DIVIDER * VBAT_TRIM;

  // ต่ำกว่า 2V = ยังไม่ได้ต่อตัวแบ่งแรงดัน ไม่ใช่แบตหมดจริง
  // ไม่งั้นไฟส้มจะกระพริบค้างตลอดตั้งแต่ยังไม่ประกอบเสร็จ
  if (vbat < 2.0f) { vbatLow = false; return; }

  bool was = vbatLow;
  vbatLow = (vbat < VBAT_LOW_V);
  if (vbatLow && !was) Serial.printf("EVT BATT LOW %.2f\n", vbat);
}

/* ==========================================================================
   14. ไฟส้มบอกความพร้อม (รีเลย์ช่อง 2)
   ========================================================================== */
// พร้อมใช้งาน = ครบทั้งสามระบบ
//   1) ESP32 บูตเสร็จ (ถึงบรรทัดนี้ได้ก็คือเสร็จแล้ว)
//   2) Pi คุยอยู่     (มีคำสั่งเข้ามาภายใน LINK_TIMEOUT_MS)
//   3) กล้องออนไลน์  (Pi แจ้งมาด้วย CAM 1)
bool systemReady() {
  return (!linkDown) && camOnline;
}

void serviceLamp() {
  if (!HAS_STATUS_LAMP) return;
  bool want = systemReady();
  if (want != lampOn) {
    lampOn = want;
    relayWrite(PIN_LAMP, lampOn);
    Serial.printf("EVT READY %d\n", lampOn ? 1 : 0);
  }
}

/* ==========================================================================
   15. PIR
   ========================================================================== */
void servicePir() {
  if (!PIR_ENABLED) return;
  bool v = pirRaw();
  unsigned long now = millis();
  if (v && !pirPrev && (long)(now - pirBlockUntil) > 0) {   // เทียบแบบนี้ไม่พังตอน millis() วนรอบ
    pirBlockUntil = now + PIR_LOCKOUT_MS;
    Serial.println("EVT PIR");
  }
  pirPrev = v;
}

/* ==========================================================================
   16. ตัวกันอันตราย: Pi เงียบนานเกินไป
   ========================================================================== */
void serviceWatchdog() {
  bool busy = pumpOn || moving || repelStep != RP_IDLE;
  if (millis() - lastHostMs > LINK_TIMEOUT_MS) {
    if (!linkDown) {
      linkDown = true;
      camOnline = false;       // Pi เงียบแล้ว ข้อมูลกล้องที่มีก็เชื่อไม่ได้
      Serial.println("EVT SAFE TIMEOUT");
    }
    if (busy) { repelAbort(); enaWrite(false); }
  }
}

/* ==========================================================================
   17. ตัวแปลคำสั่ง
   ========================================================================== */
void sendStat() {
  Serial.print("OK POS=");   Serial.print(posSteps);
  Serial.print(" DEG=");     Serial.print((float)posSteps * 360.0f / (float)STEPS_PER_REV, 1);
  Serial.print(" PUMP=");    Serial.print(pumpOn ? 1 : 0);
  Serial.print(" MOVING=");  Serial.print(moving ? 1 : 0);
  Serial.print(" REPEL=");   Serial.print(repelStep != RP_IDLE ? 1 : 0);
  Serial.print(" PIR=");     Serial.print(pirRaw() ? 1 : 0);
  Serial.print(" WPCT=");    Serial.print(waterPct);
  Serial.print(" WCM=");     Serial.print(waterCm, 1);
  Serial.print(" WLOW=");    Serial.print(waterLow ? 1 : 0);
  Serial.print(" WEMPTY=");  Serial.print(waterEmpty ? 1 : 0);
  Serial.print(" WFAULT=");  Serial.print(waterFault ? 1 : 0);
  Serial.print(" PDUTY=");   Serial.print(dutyLeftMs());
  Serial.print(" POSOK=");   Serial.print(posKnown ? 1 : 0);
  Serial.print(" RST=");     Serial.print(resetReasonText);
  Serial.print(" VBAT=");    Serial.print(vbat, 2);
  Serial.print(" CAM=");     Serial.print(camOnline ? 1 : 0);
  Serial.print(" READY=");   Serial.print(systemReady() ? 1 : 0);
  Serial.print(" LIM=");     Serial.print(LIMIT_STEPS);
  Serial.print(" SPR=");     Serial.println(STEPS_PER_REV);
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  lastHostMs = millis();
  if (linkDown) { linkDown = false; Serial.println("EVT LINK OK"); }

  String up = line;
  up.toUpperCase();

  if (up == "PING") { Serial.printf("OK PONG %s\n", FW_ID); return; }
  if (up == "STAT") { sendStat(); return; }
  if (up == "HELP") {
    Serial.println("OK CMDS PING STAT ABORT REPEL MOT L|R <n> MOT HOME MOT STOP "
                   "MOT ZERO PUMP <ms> WATER CAM 0|1 VCAL <v>");
    return;
  }

  if (up == "ABORT") {
    repelAbort();
    enaWrite(false);
    Serial.println("OK ABORT");
    return;
  }

  if (up.startsWith("CAM")) {          // Pi แจ้งสถานะกล้อง: CAM 1 / CAM 0
    String arg = up.substring(3);
    arg.trim();
    if (arg != "0" && arg != "1") { Serial.println("ERR ARG"); return; }
    camOnline = (arg == "1");
    Serial.printf("OK CAM %d\n", camOnline ? 1 : 0);
    return;
  }

  if (up == "WATER") {
    if (!HAS_WATER_SENSOR) { Serial.println("ERR NO WATER SENSOR"); return; }
    lastWaterMs = 0;                 // บังคับให้อ่านใหม่รอบหน้าทันที
    Serial.printf("OK WATER %d %% (%.1f cm)\n", waterPct, waterCm);
    return;
  }

  if (up.startsWith("VCAL")) {       // สอบเทียบแรงดัน: VCAL 12.63
    float real = up.substring(4).toFloat();
    if (real > 5.0 && real < 20.0 && vbat > 1.0) {
      VBAT_TRIM = VBAT_TRIM * (real / vbat);
      prefs.putFloat("vtrim", VBAT_TRIM);   // จำไว้ในแฟลช รีบูตแล้วไม่หาย
      Serial.printf("OK VCAL TRIM=%.4f\n", VBAT_TRIM);
    } else Serial.println("ERR VCAL RANGE");
    return;
  }

  if (up == "REPEL") {
    if (repelStep != RP_IDLE || moving) { Serial.println("ERR BUSY"); return; }
    PumpResult r = repelStart();
    if (r != PUMP_OK) { Serial.println(pumpErrText(r)); return; }
    Serial.println("OK REPEL");
    return;
  }

  if (up.startsWith("PUMP")) {
    if (repelStep != RP_IDLE) { Serial.println("ERR BUSY"); return; }
    long ms = up.substring(4).toInt();
    if (ms <= 0) { Serial.println("ERR ARG"); return; }
    PumpResult r = pumpStart((unsigned long)ms);
    if (r != PUMP_OK) { Serial.println(pumpErrText(r)); return; }
    Serial.printf("OK PUMP %lu\n", pumpOffAtMs - pumpOnAtMs);   // เวลาจริงหลังตัดตามเพดาน/โควตา
    return;
  }

  if (up.startsWith("MOT")) {
    if (repelStep != RP_IDLE) { Serial.println("ERR BUSY"); return; }
    String rest = up.substring(3);
    rest.trim();

    if (rest == "STOP") { softStop(); Serial.println("OK STOP"); return; }

    // คำสั่งหมุนใหม่ระหว่างที่แกนยังหมุนอยู่ = กลับทิศทันทีจากความเร็วสูง
    // สเต็ปหลุดแล้วตำแหน่งเพี้ยน จึงให้รอหมุนเสร็จ (หรือสั่ง MOT STOP ก่อน)
    if (moving) { Serial.println("ERR MOVING"); return; }

    if (rest == "ZERO") {              // ผู้ใช้จัดหัวฉีดตรงกลางด้วยมือแล้ว สั่งจำจุดนี้
      posSteps = 0;
      targetStep = 0;
      posKnown = true;
      Serial.println("OK ZERO");
      return;
    }
    if (rest == "HOME") {
      if (posSteps == 0) { Serial.println("OK HOME"); return; }
      startMoveTo(0);
      Serial.println("OK HOME");
      return;
    }
    if (rest.startsWith("L") || rest.startsWith("R")) {
      bool right = rest.startsWith("R");
      long n = rest.substring(1).toInt();
      if (n <= 0) { Serial.println("ERR ARG"); return; }
      startMoveTo(posSteps + (right ? n : -n));
      Serial.printf("OK MOT %ld\n", targetStep);
      return;
    }
    Serial.println("ERR ARG");
    return;
  }

  Serial.println("ERR UNKNOWN");
}

/* ==========================================================================
   18. setup / loop
   ========================================================================== */
void setup() {
  Serial.begin(115200);
  delay(300);

  if (HAS_MOTOR) {
    pinMode(PIN_PUL, OUTPUT);  digitalWrite(PIN_PUL, LOW);
    pinMode(PIN_DIR, OUTPUT);  digitalWrite(PIN_DIR, LOW);
    pinMode(PIN_ENA, OUTPUT);  enaWrite(false);
  }

  // ขาเป็น LOW ตั้งแต่ pinMode อยู่แล้ว ซึ่งคือสถานะปิดของเวอร์ชันนี้
  if (HAS_PUMP) {
    pinMode(PIN_PUMP, OUTPUT);
    relayWrite(PIN_PUMP, false);
  }

  // PULLDOWN สำคัญ: ขา PIR ที่ลอยอยู่จะอ่านค่าสลับมั่วจนพ่น EVT PIR รัว
  if (HAS_PIR) pinMode(PIN_PIR, INPUT_PULLDOWN);

  if (HAS_WATER_SENSOR) {
    pinMode(PIN_TRIG, OUTPUT); digitalWrite(PIN_TRIG, LOW);
    pinMode(PIN_ECHO, INPUT);
  }

  if (HAS_STATUS_LAMP) {
    pinMode(PIN_LAMP, OUTPUT);
    relayWrite(PIN_LAMP, false);
  }

  if (HAS_VBAT_SENSE) analogSetPinAttenuation(PIN_VBAT, ADC_11db);

  prefs.begin("birdctrl", false);
  VBAT_TRIM = prefs.getFloat("vtrim", 1.0f);

  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  resetReasonText = "POWERON";  break;
    case ESP_RST_BROWNOUT: resetReasonText = "BROWNOUT"; break;   // ไฟตก เช่นตอนปั๊มเริ่มเดิน
    case ESP_RST_PANIC:    resetReasonText = "PANIC";    break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      resetReasonText = "WDT";      break;
    case ESP_RST_SW:       resetReasonText = "SW";       break;
    case ESP_RST_EXT:      resetReasonText = "EXT";      break;
    default:               resetReasonText = "OTHER";    break;
  }

  bootMs = millis();
  lastHostMs = millis();
  dutyWindowStartMs = millis();
  rxBuf.reserve(64);

  // ข้อความตอนบูตขึ้นต้นด้วย EVT ไม่ใช่ OK
  // ไม่งั้น Pi อาจเข้าใจผิดว่าเป็นคำตอบของคำสั่งที่ส่งค้างไว้ตอนบอร์ดรีเซ็ต
  Serial.printf("EVT BOOT %s RST=%s\n", FW_ID, resetReasonText);
  Serial.printf("EVT HW MOTOR=%d PUMP=%d WATER=%d LAMP=%d VBAT=%d PIR=%d\n",
                HAS_MOTOR, HAS_PUMP, HAS_WATER_SENSOR,
                HAS_STATUS_LAMP, HAS_VBAT_SENSE, HAS_PIR);
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxOverflow) Serial.println("ERR TOOLONG");
      else if (rxBuf.length()) handleLine(rxBuf);
      rxBuf = "";
      rxOverflow = false;
    } else if (rxBuf.length() < 60) {
      rxBuf += c;
    } else {
      rxOverflow = true;             // ยาวเกิน ทิ้งทั้งบรรทัด ไม่เอาครึ่งคำสั่งไปทำงาน
    }
  }

  serviceMotor();
  serviceRepel();
  servicePump();
  serviceWater();
  serviceBattery();
  servicePir();
  serviceWatchdog();
  serviceLamp();
}
