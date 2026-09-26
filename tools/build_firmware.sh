#!/bin/bash
# build_firmware.sh <sketch folder>
# Compiles SomudTick with arduino-cli and writes <sketch folder>/SomudTick_merged.bin (flash at 0x0)
# and <sketch folder>/SomudTick_app.bin (the program only, flash at 0x10000; v11.5+)
# (bootloader + partition table + boot_app0 + app, NOT padded to 16 MB, so the log area is never touched).
# Needs: arduino-cli with esp32 core 3.3.12, LovyanGFX 1.2.30, ArduinoJson 7, IRremoteESP8266 2.9.0.
#   ARD=/path/to/arduino-cli  ARD_DATA=/path/to/arduino data dir  (defaults below)
set -e
SK="$(cd "$1" && pwd)"
ARD="${ARD:-arduino-cli}"
ARD_DATA="${ARD_DATA:-$HOME/.arduino15}"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default"
WORK="$(mktemp -d)"; mkdir "$WORK/SomudTick"; cp "$SK"/*.ino "$SK"/*.h "$SK"/partitions.csv "$WORK/SomudTick/"
"$ARD" compile -b "$FQBN" --output-dir "$WORK/out" "$WORK/SomudTick" 2>&1 | grep -E "error|Sketch uses|Global variables" 
CORE=$(ls -d "$ARD_DATA"/packages/esp32/hardware/esp32/* | tail -1)
ESPT=$(ls "$ARD_DATA"/packages/esp32/tools/esptool_py/*/esptool | tail -1)
"$ESPT" --chip esp32s3 merge-bin -o "$SK/SomudTick_merged.bin" --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 "$WORK/out/SomudTick.ino.bootloader.bin" 0x8000 "$WORK/out/SomudTick.ino.partitions.bin" \
  0xe000 "$CORE/tools/partitions/boot_app0.bin" 0x10000 "$WORK/out/SomudTick.ino.bin" | tail -1
cp "$WORK/out/SomudTick.ino.bin" "$SK/SomudTick_app.bin"
ls -l "$SK/SomudTick_merged.bin" "$SK/SomudTick_app.bin"
