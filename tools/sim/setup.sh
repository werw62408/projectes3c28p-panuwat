#!/bin/bash
# Get the two libraries the simulator needs and build their C parts once.
# Output goes to tools/sim/deps (not in git).
set -e
cd "$(dirname "$0")"
mkdir -p deps obj
[ -d deps/LovyanGFX ] || git clone -q --depth 1 --branch 1.2.30 https://github.com/lovyan03/LovyanGFX.git deps/LovyanGFX
[ -d deps/ArduinoJson ] || git clone -q --depth 1 --branch v7.4.2 https://github.com/bblanchon/ArduinoJson.git deps/ArduinoJson
# LovyanGFX only offers its String helpers on Arduino; the simulator has its own String
sed -i 's/^  #if defined (ARDUINO)$/  #if defined (ARDUINO) || defined (SIM)/' deps/LovyanGFX/src/lgfx/v1/LGFXBase.hpp
for f in $(find deps/LovyanGFX/src -name '*.c'); do
  o=obj/$(echo "$f" | tr / _).o
  [ -f "$o" ] || gcc -O1 -w -DLGFX_LINUX_FB -Ideps/LovyanGFX/src -c "$f" -o "$o"
done
[ -f obj/lgfx_v1.o ] || g++ -std=gnu++17 -O1 -w -DSIM -DLGFX_LINUX_FB -DLGFX_USE_V1 -Istubs -include Arduino.h -Ideps/LovyanGFX/src -c deps/LovyanGFX/src/lgfx/v1/lgfx_v1.cpp -o obj/lgfx_v1.o
echo "simulator ready"
