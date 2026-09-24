#pragma once
// Sudoku puzzle maker and solver (plain C++, no screen code).
// Makes a random full grid, then removes numbers one by one
// while the puzzle still has exactly one answer.
#include <stdint.h>
#include <string.h>

#ifdef ARDUINO
static inline uint32_t sdkRand() { return esp_random(); }
#else
#include <stdlib.h>
static inline uint32_t sdkRand() { return (uint32_t)rand(); }
#endif

namespace sdk {

inline bool canPlace(const uint8_t* g, int i, int v) {
  int r = i / 9, c = i % 9;
  for (int k = 0; k < 9; k++) if (g[r * 9 + k] == v || g[k * 9 + c] == v) return false;
  int br = r / 3 * 3, bc = c / 3 * 3;
  for (int y = 0; y < 3; y++) for (int x = 0; x < 3; x++) if (g[(br + y) * 9 + bc + x] == v) return false;
  return true;
}
inline void shuffle(uint8_t* a, int n) {
  for (int i = n - 1; i > 0; i--) { int j = sdkRand() % (i + 1); uint8_t t = a[i]; a[i] = a[j]; a[j] = t; }
}
inline bool fill(uint8_t* g, int i) {
  while (i < 81 && g[i]) i++;
  if (i == 81) return true;
  uint8_t d[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  shuffle(d, 9);
  for (int k = 0; k < 9; k++) {
    if (canPlace(g, i, d[k])) { g[i] = d[k]; if (fill(g, i + 1)) return true; g[i] = 0; }
  }
  return false;
}
// count answers, stop at `limit` (picks the empty cell with the fewest choices first = fast)
inline int count(uint8_t* g, int limit) {
  int best = -1, bestN = 10;
  for (int i = 0; i < 81; i++) {
    if (g[i]) continue;
    int n = 0;
    for (int v = 1; v <= 9; v++) if (canPlace(g, i, v)) n++;
    if (n == 0) return 0;
    if (n < bestN) { bestN = n; best = i; if (n == 1) break; }
  }
  if (best < 0) return 1;
  int total = 0;
  for (int v = 1; v <= 9 && total < limit; v++) {
    if (!canPlace(g, best, v)) continue;
    g[best] = v;
    total += count(g, limit - total);
    g[best] = 0;
  }
  return total;
}
// keep = how many numbers stay on the board (more = easier). Returns numbers actually kept.
inline int make(uint8_t* sol, uint8_t* puz, int keep) {
  memset(sol, 0, 81);
  fill(sol, 0);
  memcpy(puz, sol, 81);
  uint8_t order[81];
  for (int i = 0; i < 81; i++) order[i] = i;
  shuffle(order, 81);
  int filled = 81;
  uint8_t tmp[81];
  for (int k = 0; k < 81 && filled > keep; k++) {
    int i = order[k];
    uint8_t v = puz[i];
    puz[i] = 0;
    memcpy(tmp, puz, 81);
    if (count(tmp, 2) != 1) puz[i] = v; else filled--;
  }
  return filled;
}

}  // namespace sdk
