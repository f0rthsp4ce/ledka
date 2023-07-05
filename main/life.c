#include "main.h"

#include <string.h>

void putpixel(uint8_t *data, uint32_t x, uint32_t y, bool on) {
  if (x >= 32 * PANELS_X || y >= 16 * PANELS_Y)
    return;
  size_t offset = y * 32 * PANELS_X + x;
  if (on)
    data[offset / 8] |= 1 << (offset % 8);
  else
    data[offset / 8] &= ~(1 << (offset % 8));
}

static bool getpixel(const uint8_t *data, uint32_t x, uint32_t y) {
  if (x >= 32 * PANELS_X || y >= 16 * PANELS_Y)
    return false;
  size_t offset = y * 32 * PANELS_X + x;
  return data[offset / 8] & (1 << (offset % 8));
}

static bool getpixel_wrap(const uint8_t *data, uint32_t x, uint32_t y) {
  x = x % (32 * PANELS_X);
  y = y % (16 * PANELS_Y);
  size_t offset = y * 32 * PANELS_X + x;
  return data[offset / 8] & (1 << (offset % 8));
}

void life_step(const uint8_t *old, uint8_t *new) {
  for (int y = 0; y < 16 * PANELS_Y; y++) {
    for (int x = 0; x < 32 * PANELS_X; x++) {
      int neighbours = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
          if ((dx | dy) && getpixel_wrap(old, x + dx + 32 * PANELS_X,
                                         y + dy + 16 * PANELS_Y))
            neighbours++;

      if (getpixel(old, x, y))
        putpixel(new, x, y, neighbours == 2 || neighbours == 3);
      else
        putpixel(new, x, y, neighbours == 3);
    }
  }
}

void life_randomize(uint8_t *data) {
  for (size_t y = 0; y < 16 * PANELS_Y; y++)
    for (size_t x = 0; x < 32 * PANELS_X; x++)
      putpixel(data, x, y, rand() % 2);
}

void life_randomize_col(uint8_t *data, int x) {
  for (size_t y = 0; y < 16 * PANELS_Y; y++)
    putpixel(data, x, y, rand() % 2);
}

static uint16_t life_live_cells(const uint8_t *data) {
  const uint32_t *vdata = (uint32_t *)data,
                 *vdata_end = vdata + 64 * PANELS_X * PANELS_Y / sizeof *vdata;
  uint16_t count = 0;
  for (; vdata < vdata_end; vdata++)
    count += __builtin_popcount(*vdata);
  return count;
}

static uint16_t hist[12];
static size_t hist_idx = 0;
static bool __attribute__((always_inline)) is_stalled(size_t period) {
  for (size_t i = period; i < sizeof hist / sizeof *hist; i++)
    if (hist[i] != hist[i - period])
      return false;
  return true;
}

bool life_is_stalled(const uint8_t *data) {
  hist[hist_idx++] = life_live_cells(data);
  if (hist_idx == sizeof hist / sizeof *hist)
    hist_idx = 0;
  return is_stalled(1) || is_stalled(2) || is_stalled(3);
}
