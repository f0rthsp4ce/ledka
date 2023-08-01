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

void draw_panels_hint(uint8_t *topo) {
  static int p = 0;
  int sx = p / 4, sy = p % 4;
  if (++p == 12 * 3)
    p = 0;

  memset(data_text, 0, sizeof data_text);
  memset(data_text_mask, 0xff, sizeof data_text);
  for (size_t y = 0; y < PANELS_Y; y++)
    for (size_t x = 0; x < PANELS_X; x++) {
      draw_glyph(data_text, x * 32 + sx + 1, y * 16 + sy, 'X');
      draw_glyph(data_text, x * 32 + sx + 5, y * 16 + sy, '0' + x);

      draw_glyph(data_text, x * 32 + sx + 13, y * 16 + sy, 'Y');
      draw_glyph(data_text, x * 32 + sx + 17, y * 16 + sy, '0' + y);

      draw_glyph(data_text, x * 32 + sx + 1, y * 16 + sy + 6, 'T');
      draw_glyph(data_text, x * 32 + sx + 5, y * 16 + sy + 6,
                 '0' + (topo[x + y * PANELS_X] & 0x7f));
      if (topo[x + y * PANELS_X] & 0x80)
        draw_glyph(data_text, x * 32 + sx + 9, y * 16 + sy + 6, 'R');
    }
}

void draw_text(const char *text, uint16_t pos_x, uint16_t pos_y) {
  memset(data_text, 0, sizeof data_text);
  memset(data_text_mask, 0xff, sizeof data_text);

  size_t px = pos_x + 1, py = pos_y + 1;
  for (; *text; text++) {
    if (*text == '\n') {
      px = pos_x + 1;
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
