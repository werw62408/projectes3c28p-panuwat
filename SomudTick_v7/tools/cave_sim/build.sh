#!/bin/sh
# needs LovyanGFX installed in ~/Arduino/libraries (Arduino Library Manager)
set -e
cd "$(dirname "$0")"
L=${LGFX:-$HOME/Arduino/libraries/LovyanGFX/src}
mkdir -p build && cd build
if [ ! -f lgfx.o ]; then
  g++ -std=c++17 -O2 -DLGFX_LINUX_FB -I"$L" -c "$L/lgfx/v1/lgfx_v1.cpp" -o lgfx.o
  find "$L/lgfx" -name "*.c" | while read -r c; do gcc -O2 -I"$L" -c "$c" -o "$(basename "$c").o"; done
fi
g++ -std=c++17 -O2 -Wall -Wno-unused-variable -DLGFX_LINUX_FB -I"$L" ../host.cpp lgfx.o *.c.o -o cave_host -lpthread
echo build ok
