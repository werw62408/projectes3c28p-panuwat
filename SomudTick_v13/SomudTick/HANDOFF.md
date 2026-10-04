# SomudTick: สรุปงานล่าสุด (สำหรับทำต่อ)

อัปเดต 3 ต.ค. 2026 · เฟิร์มแวร์ล่าสุด **v13** · เอกสารนี้สรุปว่ามีอะไรอยู่ตรงไหน ทดสอบยังไง และอะไรยังค้าง

## 1. แผนที่โฟลเดอร์

| ที่ | คืออะไร |
|---|---|
| `SomudTick.ino` + `*.h` | เฟิร์มแวร์ v13 (ESP32-S3 ES3C28P) |
| `SomudTick_app.bin` | ไฟล์พร้อมแฟลช (Wokwi ESP Tool / esptool-js ที่ `0x10000`) |
| `SomudTick_merged.bin` | แฟลชทั้งเครื่องที่ `0x0` (ตัดท้ายแล้ว ไม่ทับข้อมูล log) |
| `README.md` | คู่มือผู้ใช้ภาษาไทย ทุกเวอร์ชัน (หัวข้อบนสุด = v13) |
| `preview_v13_*.png` | ภาพหน้าจอจากตัวจำลอง (จอเล็ก / จอใหญ่แนวตั้ง / แนวนอน) |
| `tools/sim_py/` | ตัวจำลองทดสอบ: `faultsim.py` (ไฟดับระหว่างบันทึก), `padsim.py` (ปุ่มหลวม/เด้ง) |
| `../../tools/sim/` (ที่รากโปรเจกต์) | ตัวจำลองเฟิร์มแวร์ C++ (คอมไพล์โค้ดจริงกับ stub) + ชุดทดสอบทุกเวอร์ชัน ดูข้อ 2 "ทดสอบ" |
| `case/` | เคส + กล่อง + สายไฟ (สคริปต์ Python สร้างไฟล์เอง) ดูข้อ 4 |
| `prototypes/` | เกม/ตัวจำลองบนเว็บ (ต้นฉบับ HTML) ดูข้อ 3 |
| `.claude/launch.json` | เซิร์ฟเวอร์ทดสอบในเครื่อง: `lastline` 8765, `antcolony` 8766, `caseview` 8768 |

## 2. เฟิร์มแวร์ v13

### คอมไพล์และส่งมอบ
ผู้ใช้ไม่คอมไพล์เอง: ทุกครั้งที่แก้โค้ดต้องคอมไพล์ให้ แล้ววาง bin ในโฟลเดอร์โปรเจกต์
- บน Linux (cloud session): `tools/build_firmware.sh SomudTick_vX/SomudTick` ทำทั้งสอง bin ให้ (ตัด merged bin ให้แล้ว)
- ตัวเช็คก่อนส่งทั้งชุด: `.claude/skills/somudtick-release/scripts/release_check.sh <เวอร์ชัน> <โฟลเดอร์ zip>`
- บน Windows:
```
%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe compile
  --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi"
  --build-path <โฟลเดอร์ชั่วคราว>\build <โฟลเดอร์สเก็ตช์>
```
- `SomudTick.ino.bin` → `SomudTick_app.bin`
- `SomudTick.ino.merged.bin` → `SomudTick_merged.bin` **ต้องตัดให้จบที่ท้ายแอป** (core 3.3 เติม 0xFF จนเต็ม 16 MB ซึ่งจะลบพาร์ทิชัน log ที่ `0x400000`)
- ห้ามกด Erase Flash

### ฮาร์ดแวร์และ I2C
บัสเดียว (I2C port 1, 400 kHz): ทัช `0x38` · DS3231 `0x68` · OLED `0x3C` · PCF8574 `0x20` (ถ้าเป็น PCF8574A ห้ามตั้ง `0x38`/`0x3C`; `padBegin` ไม่ลอง 0x3C/0x3D)
ขา: I2C = IO16 (SDA) / IO15 (SCL) · จอย X/Y = IO2 / IO3 · IR LED = IO21
**IO14:** เจอบอร์ดปุ่มตอนบูต = ตัวรับ IR KY-022 (`joySwIo14 = false`, กดจอยมาทาง K = PCF P6) / ไม่เจอ = ปุ่มกดจอยแบบสายเดิม (`joySwLow()` อ่าน 4 ครั้งใน ~0.9 ms กันพัลส์ IR)
จอเล็ก: **SH1106 1.3" 128×64** (ภาพเริ่มคอลัมน์ 2) หรือ SSD1306 0.96" 128×64 (Settings → Screen, pref `oledChip`)

### ไฟล์หลักและฟังก์ชัน
| ไฟล์ | หน้าที่ | ฟังก์ชันสำคัญ |
|---|---|---|
| `core_log.h` | บันทึกแบบไฟดับไม่พัง | `writeFileSafe` (.tmp → ตรวจ → .ok → แทนที่), `writeBytesSafe` (ไฟล์ไบนารี v13: รังมด, เซฟ GB), `fileRepair`, `writeDayFile`, `dayFilesRepair`, `eachEvent`, `fileEndsClean`, `dayEndsClean`, `logEvent` (เขียนยอดรวมก่อน log, ล้มแล้วย้อน), `undoEvent`, `saveActs` |
| `joystick.h` | จอย + บอร์ดปุ่ม PCF8574 | `padBegin`, `padPoll`, `padDown`, `i2cReadRaw`, `i2cWriteRaw`, ปุ่ม `PB_A..PB_K`, `joyDown` / `gameDown` / `gameBtnOk` (A = กดจอยในเกม), `joyAxis` + `joyRangeLoad` (ระยะโยก Joy-Con, pref `jLoX..jHiY`) |
| `ext_io.h` | จอเล็ก + งานปุ่ม | `oledInit`, `oledSendRange` / `oledPush` (ส่งเฉพาะไบต์ที่เปลี่ยน, `oledKnown`), `oledClock` / `oledGoals` / `oledPop` / `oledOff` / `oledNotice`, `oLine` (บรรทัดที่มีชื่อไทย = แถวสูง 19–21 จุด), `oledTask`, `extBegin` (ภาพแรกก่อน 0xAF), `padTask` (กันปุ่มเด้ง/หลวม), `padResync`, `padWaitEvent` |
| `app_ir.h` | ตัวรับ KY-022 + แอป Remotes (v13) | `irBegin`, `irTask`, `irHandle` (ข้ามเสียงตัวเอง 400 ms, กดค้าง/ซ้ำ, Sony 3 รอบ), `irLearnStart` / `irLearnGot`, `irSendKey`, `irKeysSave` / `irKeysLoad` (`/irkeys.json`), `acFromRemote`, `drawRemote` / `remoteTap` / `remoteLongPress`, `irMenuBox` / `irUseGrid` (หน้าต่างพอดีระหว่างแถบ) |
| `ui_draw.h` | เลย์เอาต์จอใหญ่ | `HDR_H`, `layoutUpdate`, `barsFreePage` (ซ่อนแถบบน/ล่างเมื่อมี OLED) |
| `nav.h` / `input.h` | จอยคุมทั้งเครื่อง / ทัช | `navHandle`, `navBackAny` (D ย้อนกลับได้แม้ปิดจอย), `navWaitEvent` (หน้าต่างที่รอเอง: จอย + ปุ่ม + IR + จอเล็ก) |
| `web_api.h` | หน้าเว็บในเครื่อง | `actsMoved` (แถบเตือนหากิจกรรมใหม่ด้วย `remindId` หลังแก้รายการในเว็บ) |
| `app_deck.h` | Deck ปุ่มลัด USB/Bluetooth | `deckTick`, `deckLongPress`, `deckForgetAll` |
| `core_wifi.h` | ปิดวิทยุชั่วคราวตอนใช้ BT | `radioQuiet` |
| `web_api.h` / `webpage.h` | หน้าเว็บในเครื่อง | ตอบ 507 เมื่อที่เก็บเต็ม |
| `app_ants.h` | เกมรังมดบนบอร์ด (ต้นแบบของตัวจำลองเว็บ) | |

### กติกาที่ต้องรักษาไว้
- บรรทัด log ปิดท้ายด้วย `!\n` บรรทัดสุดท้ายที่ไม่ปิดให้ข้าม
- ยอดรวมมีตัวกำกับ `#ok,<วัน>,<จำนวน>` ถ้าไม่ครบ/ไม่ตรง สร้างใหม่ตอนบูต
- ปุ่ม: กด 10 ครั้ง/5 วิ หรือ 40 ครั้ง/60 วิ = ปิดเสียงปุ่มนั้น 30 วิ แล้วเพิ่มขึ้นถึง 32 นาที
- BLE Deck: พารามิเตอร์แบบ Apple (`updateConnParams 12, 24, 0, 400`), ถือว่าพร้อมหลังจับคู่เสร็จ (สำรอง 5 วิ)

### ทดสอบ
- **ตัวจำลอง C++ (`tools/sim` ที่รากโปรเจกต์, Linux):** คอมไพล์โค้ดเฟิร์มแวร์จริงกับ stub (จอ LovyanGFX จริง, LittleFS/SD เป็นโฟลเดอร์, I2C จำลองจอ SH1106 + PCF8574 + DS3231, IR รับ/ส่ง)
  - ชุดของ v13: `./build.sh test_v13.cpp ../../SomudTick_v13/SomudTick test_v13 && ./bin/test_v13` (43 ข้อ)
  - ทั้งหมด: `./run_tests.sh ../../SomudTick_v13/SomudTick` ต้องขึ้น `ALL TESTS PASSED`
  - ภาพหน้าจอ: `./shots.sh shots_v13.cpp ../../SomudTick_v13/SomudTick run/shots13` แล้วรวมด้วย `sheet.py` (ภาพจอเล็กมาจาก `oledPng()` = หน่วยความจำจอจริงที่ชิปได้รับ)
  - สคริปต์ใส่ปุ่ม/IR ระหว่างหน้าต่างที่รอเอง: `g_simDelayHook`; ส่ง IR เข้า: `g_simIrIn`; ดูที่ยิงออก: `g_simIrOut`
- `python tools/sim_py/faultsim.py` → จำลองไฟดับทุกจุดของการเขียน (ผ่าน 100,468 รอบ)
  - ตอนทำเคยลองแก้กติกาทีละข้อให้ผิด (5 แบบ) แล้วตัวทดสอบจับได้ทุกแบบ ไฟล์ทดลองชุดนั้นไม่ได้เก็บไว้ในโปรเจกต์
- `python tools/sim_py/padsim.py` → ปุ่มเด้ง/หลวม (กรณีแย่สุด 94 ครั้ง/ชม.)
- หน้าเว็บ: `node tools/web/test_video.js <webpage.h>`, `test_deck.js` (ผ่าน) · `test_audio.js` ต้องใช้ Chromium ที่ถอด AAC ได้ (Chromium ใน cloud container ถอดไม่ได้ จึงรันไม่ผ่านที่นั่น)

## 3. เกมและตัวจำลองบนเว็บ (`prototypes/`)

ต้นฉบับอยู่ในโปรเจกต์แล้ว (เดิมอยู่ในโฟลเดอร์ชั่วคราว) ทุกตัวเป็น HTML ไฟล์เดียว จอเกม 320×240 หรือ 240×320 เท่าบอร์ด
**แก้แล้วจะอัปขึ้นลิงก์เดิม: publish ด้วย `url` ของ artifact นั้น**

### 3.1 LAST LINE (เกมป้องกันฐาน) `prototypes/lastline/index.html`
ลิงก์: https://claude.ai/artifact/HVUTVLV6RKXqjiuKqPYzL4 · เซฟ `lastline.v2`

มุมมองด้านข้าง ฐานอยู่ซ้าย ศัตรูมาจากขวา 10 ด่าน ด่านละ 5 เวฟ (บอสด่าน 5, 10) แล้วต่อแบบไม่จบ
- **ลูปหลัก:** `newBattle` → `startWave` → `step()` (60 ครั้ง/วิ กำหนดผลได้ซ้ำ) → `waveDone` (เลือกการ์ด) → `win` / `lose`
- **ทักษะ:** กดค้างเล็งเอง = ยิงหัว ×2.5 (สไนเปอร์ ×3) · ปล่อยมือ = ล็อคเป้าเอง ยิงช้ากว่า (`TUNE.auto` 0.7) โดนแค่ลำตัว
- **รีโหลด:** `startReload` / `pressReload` → กดในช่องทอง = perfect (เต็มทันที +30% ทั้งแม็ก), หลังช่องทอง = good, นอกนั้น = jam (+0.8 วิ)
- **สกิล (`ABIL`, `useAbility`):** Q ระเบิดมือ · W แผงกั้น (`B.barr`) · E พลุช็อต (นกร่วง)
- **อาวุธ:** `MAINS` 4 (rifle, shotgun, smg, sniper) · `SUPS` 8 (mg, wire, mortar, mines, tesla, repair, flame, drone) เลเวลสูงสุด 10
- **ศัตรู (`EN`) 12 แบบ:** walker, runner, rat, crow (บิน), armored (เกราะ 6), shield (กันลำตัว 85%), spitter, leaper, bloater, screamer (เร่ง 45%), saboteur (ปิดอาวุธเสริม 8 วิ), brute (บอส)
- **ความลึก:** `PERKS` 17 ใบ · ดาว 3 ดวง/ด่าน เป็น bitmask (1 ผ่าน, 2 กำแพง ≥60%, 4 ภารกิจ `STAGE_CHAL`) · `TAL` 9 ทักษะซื้อด้วยดาว · คอมโบ `streakMul` สูงสุด ×2
- **ค่าความยาก:** `TUNE = { hpS: 1.2, hpW: 0.15, nS: 3.2, nW: 3.5, n0: 10, dmgS: 0.1, rew: 0.6, sup: 0.5, auto: 0.7 }`

ตัวช่วยทดสอบ (พิมพ์ในคอนโซล):
| คำสั่ง | ทำอะไร |
|---|---|
| `LL.campaign({ short: true, skill: { head: 0.3, perfect: 0.35, good: 0.3, ab: true } })` | บอตเล่นด่าน 1-10 (อัปเกรดเอง) คืนกำแพงที่เหลือ + ดาว |
| `LL.sim(ด่าน, skill)` | จำลอง 1 ด่านด้วยโปรไฟล์ปัจจุบัน |
| `LL.run(วินาที)` | เดินเกมจริงไปข้างหน้าแล้ววาด (ใช้ตอนหน้าต่างซ่อนอยู่) |
| `LL.give({ cleared: 5, sel: 5 })` | แก้โปรไฟล์เพื่อข้ามไปทดสอบ |
| `LL.tune` | ปรับค่าความยากสด ๆ |

ผลบอตล่าสุด: ไม่เล็งเอง = ติดด่าน 3 · ยิงหัว 30% = เล่นซ้ำช่วงท้าย · ยิงหัว 55% = ผ่านครบ กำแพงเหลือ 4-50% ช่วงท้าย · Easy ไม่เล็ง = ถึงด่าน 7
**ยังไม่ได้ทำ:** ลองเล่นจริงด้วยมือบนมือถือ · พอร์ตลงบอร์ด (ผู้ใช้สั่งว่าออกแบบ/ลองบนเว็บก่อน ยังไม่เขียนลงเฟิร์มแวร์)

### 3.2 รังมดคันไฟ `prototypes/antcolony/index.html`
ลิงก์: https://claude.ai/artifact/VACFb7PG73w8K5jtbRmRea · เซฟ `somud.fireants.v1` (`GEN = 4` ถ้ากติกาโลกเปลี่ยนให้เพิ่มเลขนี้)

จำลองเกม Ants ของบอร์ด: ฟาร์ม 240×592 (ลึก 2 จอ) มด red imported fire ant
- **โลก:** `genWorld` (ชั้นดิน `LAYERS`, หิน, รากไม้, ของฝัง `ITEMS`) จาก seed เดียวได้โลกเดิม
- **ขุด:** `newPlan` (เลือกห้อง/อุโมงค์ ต้องมีทางตรงไม่ชนหิน `lineClear`) → `digStep` (อ้อมสิ่งกีดขวาง) → `digRoom` (ห้องโตออกจากปลายอุโมงค์) · `dropPlan` = ยกเลิกห้องที่ไปไม่ถึง
- **ห้อง:** `roomStyle` 2 = โพรงกลม (ราชินี/ไข่/อาหาร), 0/1 = แกลเลอรี/โถง · กองของ `heapSpots` / `heapInto`
- **วรรณะ:** `C_MINOR / C_MEDIA / C_MAJOR / C_MALE / C_GYNE / C_QUEEN` กำหนดงานผ่าน threshold (`a.th`)
- **สมองรัง:** `colonyBrain` (ความต้องการ nurse/forage/dig/store ต่อ threshold) · `recruit` (ชวนพวกเป็นฝูง) · ย้ายไข่ตามอุณหภูมิ (`broodRooms`, `broodTarget`, งาน `AJ_MOVE`/`AJ_MOVE2`) · `poke` (กรูออกป้องกันเนิน) · `flight` (มดมีปีกบิน)

ตัวช่วยทดสอบ:
| คำสั่ง | ทำอะไร |
|---|---|
| `antFarmCheck()` | ตรวจ: `touching` (อุโมงค์แตะหิน/ราก ต้อง 0), `unreachable` (ห้องตัดขาด ต้องว่าง), วรรณะ, ไข่, อาหาร |
| `antFarmSkip(วัน)` / `antFarmRun(วินาที)` | ข้ามวัน / เดินจำลอง |
| `antFarmPoke()` · `antFarmFlight()` · `antFarmSky("day"\|"night")` | แหย่เนิน · บินผสมพันธุ์ · กลางวัน/คืน |
| `antFarmMap(x0, y0, x1, y1)` | แผนที่ตัวอักษร (ว่าง = ขุดแล้ว, o หิน, r ราก) |

ผลทดสอบ: 25 รัง × 60 วัน = ไม่มีอุโมงค์แตะหิน/ราก ไม่มีห้องตัดขาด · **สถานะ: พักไว้** (ผู้ใช้บอกพักเกมมด)
บทเรียน: เซฟเก่าในเบราว์เซอร์ผู้ใช้ทำให้เห็นบั๊กที่ผมไม่เห็น → เปลี่ยนกติกาโลกเมื่อไหร่ต้องเปลี่ยนคีย์เซฟ/เลข `GEN`

### 3.3 ตัวดูเคส 3 มิติ `prototypes/caseview/`
ลิงก์ (v13, กล่องแบบ 2): https://claude.ai/artifact/QHp9WYyrmArYY3hwHa9M4r (ลิงก์เดิม BTNaYSXUALyYV5CDXBHCuf เปิดจากบัญชีนี้ไม่ได้)
`python prototypes/caseview/build.py` → รวม `template.html` + STL ใน `case/out/` + `case/out_box2/box_asm.json` + `box_wires.json` เป็น `index.html` (เปลี่ยนแบบกล่องที่ตัวแปร `BOX`)
แท็บ: กล่องสำเร็จรูป (เลขกำกับ, พื้นที่ว่าง, สายไฟ, มองจากด้านหลัง) · เคสพิมพ์ 3D · ชิ้นส่วนแต่ละชิ้น

### 3.4 PICK & PLACE (เกมเรียงอะไหล่) `prototypes/pickplace/index.html`
ลิงก์: https://claude.ai/artifact/2FEqLZMpoi9AL5V1gNPeLe · เซฟ `pickplace.v1` · หน้าจอแบบรอบ 3 (`layout_mock.png`)
- เครื่องรอซ่อม 3 ตัวบน 2 ตัวล่าง (`SEATS`) รอบสายพานดำ · กลางลูป 5 การ์ดหลอด (`drawCards`: ชิ้นหน้าชัด ชิ้นถัดไปจาง ×จำนวน) + UNDO / MAGNET / NEXT (`drawTools`)
- เครื่องผลิตกลางล่าง (`drawMachine`, `ENTRY`) วางชิ้นลงสายพาน ไฟ READY / WAIT (`TUNE.cool` 0.25 วิ) / AUTO
- **ป้อนเอง:** ไม่ปล่อยชิ้นเกิน `TUNE.auto` = 4 วิ เครื่องปล่อยจากหลอดสุ่ม (`G.autoT`, `G.rnd`)
- **กะงาน (SHIFT):** เวลาจำกัด `shift0 + shiftP × จำนวนชิ้น` = 25 + 2 วิ/ชิ้น หมดเวลา = `SHOP CLOSED` (กันชนะด้วยการนั่งเฉย ๆ)
- บอต: `PP.all("smart" | "slow" | "random" | "idle")` → smart / slow (ปล่อยทุก 1.2 วิ) ผ่าน 30/30, random ติดสายพานเต็มเกือบทุกด่าน, idle หมดเวลาทุกด่าน · slow เวลาเหลือน้อยลงช่วงท้าย (ด่าน 30: 181 จาก 219 วิ)

### 3.5 HOOKLINE `prototypes/hookline/index.html`
เกมหุ่นยนต์ตะขอยึดพื้นที่ มุมบน (ต้นแบบเว็บ) บอตผ่าน 10/10 ราว 17 นาที · รายละเอียดใน commit `a47c4d3`

## 4. เคส กล่อง และสายไฟ (`case/`)

| สคริปต์ | ได้อะไร |
|---|---|
| `make_case.py` | เคสพิมพ์ 3D 94.2 × 137.2 × 27.5: `out/*.stl`, `preview.png`, `template_1to1.pdf`, `inside_layout.png`, `na_test.stl` (แผ่นลองปุ่ม รูจอย 26/32) |
| `make_box.py` | **กล่องสำเร็จรูป 177 × 94 × 35** (ทางที่ผู้ใช้เลือกตอนนี้): `out_box/drill_template.pdf` (หน้ากล่อง, ผนัง, ฝา), `drill_list.txt`, `inside_layout.png`, `box_asm.json` |
| `make_box_harness.py` | สายไฟในกล่อง: `out_box/wiring_box.png`, `cut_list.md` (34 เส้น รวม ~4.3 ม.), `box_wires.json` |
| `make_wiring.py` | ผังต่อขา `../wiring_v12.png` |
| `make_harness.py` | ผังสายของ**เคสพิมพ์รุ่นเก่า** (ตำแหน่งจอเล็ก/IR/สวิตช์ล้าสมัยแล้ว) |

ทุกสคริปต์มีตัวเช็คของชนกันในตัว (`check_inside` / `check`) ค่าที่ต้องวัดจริงมีคำว่า `MEASURE`

**แบบกล่อง (ล่าสุด):** ของทุกชิ้นยึดกับตัวกล่อง (พื้นกล่อง = หน้าเครื่อง) ฝาหลังมีแค่รูลำโพง · จอเล็ก 1.3" มุมขวาบน ขอบขวาตรงจอใหญ่ · IR ส่ง/รับ มุมซ้ายบนออกผนังบน · สวิตช์ + USB-C ผนังซ้าย · ลำโพงขวาล่าง (หลังแปะหน้ากล่อง) · แบต/DS3231/PCF8574 หลังบอร์ดปุ่ม · บอร์ดปุ่มยกด้วยเสา 10 มม. (ปุ่มสีโผล่ 1.5, จอยโผล่ 17.5 ไม่ต้องพิมพ์หมวกปุ่ม)

**ขนาดที่วัดจริงแล้ว:** NA011 87×53 PCB 1.5, หัวจอย Ø20 สูง 30, หมวกปุ่ม Ø11.5 สูง 14, E/F สูง 5, รูน็อต 3, ขาเหลืองสูง 8 · จอใหญ่: หน้ากระจก→PCB 4.5, เสา 5, หนารวม 10-12, USB-C ขอบไกล 9 · DS3231 38×21.5×9 · แบต 48×30×10 · ลำโพง Ø16 (หู 30) หนา 10 · สวิตช์ 12×8×10 · IR 18×15

## 5. งานค้างและสิ่งที่รอผู้ใช้

**รอของ/รอวัด (กล่องแบบ 2, ทำแม่แบบเจาะแล้ว: `case/DRILL_BOX2.md` + `case/out_box2/drill_template.pdf`)**
1. กล่องจริง: ความหนาผนัง, ก้นกล่อง, ฝา, เสาน็อตมุม, **ความกว้างด้านในที่ก้นกล่อง** (ต้อง ≥ 86) → แก้ค่า `MEASURE` ใน `make_box2.py` แล้วรันใหม่
2. ตำแหน่งจอย/ปุ่มบน NA011 ยังอ่านจากรูป → ผู้ใช้วางบอร์ดทาบแม่แบบกระดาษก่อนเจาะ (DRILL_BOX2 ข้อ 2) ถ้าเยื้องให้ส่งระยะมา
3. จอย: ใช้ตัวที่ติดมากับ NA011 (หัว Ø20, รูเจาะ 28) · Joy-Con เก็บไว้ทีหลัง (เฟิร์มแวร์รองรับระยะโยกแล้ว: ขั้น `Roll it around`)

**ต้องลองบนบอร์ดจริง (v13)**: ดู README "ของใหม่ใน v13" หัวข้อ "ต้องลองบนบอร์ดจริง" (จอเล็ก SH1106, KY-022, Remotes, แอร์ตามรีโมท, A ในเกม, สายเดิมไม่มีบอร์ดปุ่ม)

**เกม**
- LAST LINE: รอผลลองเล่นจริง แล้วจูนความยากรายด่าน
- PICK & PLACE: รอผู้ใช้ลองเล่น แล้วจูน `TUNE` (ดูข้อ 3.4)
- HOOKLINE: `prototypes/hookline/` (ต้นแบบเว็บ บอตผ่าน 10/10)

**พักไว้ (อย่าทำจนกว่าจะสั่ง)**: OTA, สั่งด้วยเสียง, เซ็นเซอร์ช่อง UART

## 6. ข้อตกลงการทำงานกับผู้ใช้
- ตอบภาษาไทย · ผู้ใช้คนเดียว · DS3231 ติดตลอด · เป้าหมายคือของเล่นคลายเครียดประจำวัน
- เฟิร์มแวร์: ส่งเป็น bin พร้อมลงเสมอ
- เกมใหม่: ทำเป็นเว็บให้ลองก่อน ยังไม่เขียนลงบอร์ดจนกว่าจะสั่ง
- **ทดสอบก่อนส่ง:** รันตัวเช็ค + ดูภาพจริง และระวังเซฟเก่าของผู้ใช้
