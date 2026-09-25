#!/bin/bash
# build.sh <test or shots file.cpp> <sketch folder> <output program>
# e.g.  ./build.sh test_v11.cpp ../../SomudTick_v11.2/SomudTick t11
set -e
cd "$(dirname "$0")"
[ -d deps/LovyanGFX ] || ./setup.sh
mkdir -p bin
g++ -std=gnu++17 -O1 -w -DSIM -DLGFX_LINUX_FB -DLGFX_USE_V1 -Istubs -I"$2" -Ideps/LovyanGFX/src -Ideps/ArduinoJson/src "$1" obj/*.o -o "bin/$3" -lpthread
