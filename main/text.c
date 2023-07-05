#include "main.h"

#include <string.h>

static void draw_glyph(uint8_t *data, size_t pos_x, size_t pos_y, char c) {
  if (c < ' ' || c > '~')
    return;
  const unsigned char *glyph = font_pzim3x5[c - ' '];
  for (int y = -1; y <= 6; y++)
    for (int x = -1; x <= 3; x++) {
      putpixel(data_text_mask, pos_x + x, pos_y + y, false);
      if (x >= 0 && x < 3 && y >= 0 && y < 6)
        putpixel(data_text, pos_x + x, pos_y + y, glyph[x] & (1 << y));
    }
}

void draw_text(const char *text, uint16_t pos_x, uint16_t pos_y) {
  memset(data_text, 0, sizeof data_text);
  memset(data_text_mask, 0xff, sizeof data_text);

  size_t px = pos_x + 1, py = pos_y + 1;
  for (; *text; text++) {
    if (*text == '\n') {
      px = 1;
      py += 6;
      continue;
    }
    draw_glyph(data_text, px, py, *text);
    px += 4;
    if (px >= 32 * PANELS_X - 4) {
      px = pos_x + 1;
      py += 6;
    }
  }
}
