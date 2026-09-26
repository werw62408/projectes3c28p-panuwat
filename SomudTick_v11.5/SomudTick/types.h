#pragma once
// Types used in function headers of SomudTick.ino.
// They live here (not in the .ino) because the Arduino IDE writes its own list of
// function headers near the top of the .ino, before the .ino's own types exist.
#include <Arduino.h>

struct Act {
  String id, name, unit;
  uint32_t color = 0x2F8F82;
  float step = 1, goal = 0;
  uint8_t goalType = 0;   // 0 ไม่มี, 1 อย่างน้อย, 2 ไม่เกิน
  uint16_t remind = 0;    // นาที (0 = ไม่เตือน)
};
struct Ev { uint32_t t; float v; char place; bool ok; String id; };
struct Sum { int count = 0; float sum = 0; uint32_t last = 0; };

enum Screen { S_HOME, S_STATS, S_APPS, S_SET, S_KEYPAD, S_HEAT, S_FILES, S_AC, S_GAME, S_SUDOKU, S_NET, S_WIFI, S_KBD, S_BT, S_GAMES,
              S_SAND, S_GARDEN, S_USB, S_MAZE, S_BLOCKS };

struct KpGeo { int x0, y0, kw, kh, px, py, panelW; };
