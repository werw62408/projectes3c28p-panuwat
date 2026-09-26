#!/bin/sh
# คอมไพล์เฟิร์มแวร์ให้รันบนคอม (ต้องมี g++)
#   fw_host        = ตั้งค่าเหมือนบอร์ดจริงทุกอย่าง
#   fw_host_test   = เปิด HAS_WATER_SENSOR และลดโควตาปั๊มเหลือ 10 วินาที
#                    ไว้ทดสอบด่านน้ำหมด/โควตาได้เร็ว
set -e
cd "$(dirname "$0")"
mkdir -p build/controller_board_esp32
g++ -std=c++17 -O1 -Wall -Wno-unused-variable -Istubs -include arduino_shim.h \
    -o build/fw_host host_main.cpp
sed -e 's/const bool HAS_WATER_SENSOR = false;/const bool HAS_WATER_SENSOR = true; /' \
    -e 's/PUMP_DUTY_MAX_MS    = 60000;/PUMP_DUTY_MAX_MS    = 10000;/' \
    ../controller_board_esp32/controller_board_esp32.ino \
    > build/controller_board_esp32/controller_board_esp32.ino
sed 's#"../controller_board_esp32/controller_board_esp32.ino"#"build/controller_board_esp32/controller_board_esp32.ino"#' \
    host_main.cpp > build/host_main_test.cpp
g++ -std=c++17 -O1 -Wall -Wno-unused-variable -I. -Istubs -include arduino_shim.h \
    -o build/fw_host_test build/host_main_test.cpp
echo "build ok"
