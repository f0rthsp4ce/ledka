#include "main.h"

#include <stdio.h>
#include <string.h>

static ssize_t char_width(const struct font_t *font, char c) {
  if (c < '!' || c > '~')
    return font->width / 2 + 1;
  size_t p = (font->width * font->height) * (c - '!');
  int width = 0;
  for (size_t y = 0; y < font->height; y++)
    for (size_t x = 0; x < font->width; x++) {
      if ((font->data[p / 8] & (1 << (p % 8))) && x > width)
        width = x;
      p++;
    }
  return width + 1;
}

static void draw_glyph(const struct font_t *font, uint8_t *data, ssize_t pos_x,
                       ssize_t pos_y, size_t char_width, char c) {
  if (c < ' ' || c > '~')
    return;
  for (int y = -1; y <= font->height; y++)
    for (int x = -1; x <= (int)char_width; x++)
      putpixel(data_text_mask, pos_x + x, pos_y + y, false);
  if (c == ' ')
    return;
  size_t p = (font->width * font->height) * (c - '!');
  for (ssize_t y = 0; y < font->height; y++)
    for (ssize_t x = 0; x < font->width; x++) {
      if (font->data[p / 8] & (1 << (p % 8)))
        putpixel(data_text, pos_x + x, pos_y + y, true);
      p++;
    }
}

void draw_panels_hint(uint8_t *topo) {
  const struct font_t *font = find_font_by_name("pzim3x5");

  static int p = 0;
  int sx = p / 4, sy = p % 4;
  if (++p == 12 * 3)
    p = 0;

  memset(data_text, 0, sizeof data_text);
  memset(data_text_mask, 0xff, sizeof data_text);
  for (size_t y = 0; y < PANELS_Y; y++)
    for (size_t x = 0; x < PANELS_X; x++) {
      char buf[32];
      snprintf(buf, sizeof buf, "X%d Y%d\nT%d%s", x, y,
               topo[x + y * PANELS_X] & 0x7f,
               topo[x + y * PANELS_X] & 0x80 ? "R" : "");
      draw_text(font, buf, x * 32 + sx, y * 16 + sy - 2, TEXT_SPACING_MONO);
    }
}

static void draw_bin(uint32_t value, int px, int py) {
  for (int i = 0; i < 64; i++) {
    uint32_t xx = 31 - i;
    for (size_t y = 0; y < 3; y++)
      for (size_t x = 0; x < 3; x++)
        putpixel(data_text, px + x + xx * 4, py + y, value & 1);
    for (size_t y = 0; y < 4; y++)
      for (size_t x = 0; x < 4; x++)
        putpixel(data_text_mask, px + x + xx * 4, py + y, false);
    value >>= 1;
  }
}

void draw_clock(time_t t, time_t target, bool hex) {
  const struct font_t *font = find_font_by_name("BMSPA");
  static bool blink = false;
  reset_text();
  char buf[64];
  if (hex)
    sprintf(buf, "%#llx", (unsigned long long)t);
  else
    sprintf(buf, "%lld", (long long)t);
  draw_bin(t, 0, 24);
  draw_bin(target, 0, 28);

  if (t >= target && t <= target + 5)
    if ((blink = !blink))
      return;
  draw_text(font, buf, (32 * PANELS_X - strlen(buf) * (font->width + 1)) / 2, 8,
            TEXT_SPACING_MONO);
}

const struct font_t *find_font_by_name(const char *name) {
  for (const struct font_t *font = fonts; font->name; font++)
    if (!strcmp(font->name, name))
      return font;
  return NULL;
}

void reset_text(void) {
  memset(data_text, 0, sizeof data_text);
  memset(data_text_mask, 0xff, sizeof data_text);
}

void draw_text(const struct font_t *font, const char *text, int16_t pos_x,
               int16_t pos_y, enum text_flags_t flags) {
  int16_t px = pos_x, py = pos_y;
  for (; *text; text++) {
    if (*text == '\n') {
      px = pos_x;
      py += font->height - 1; // TODO: configurable line spacing
      continue;
    }
    int16_t cw = (flags & TEXT_SPACING_MASK) == TEXT_SPACING_VAR ||
                         ((flags & TEXT_SPACING_MASK) == TEXT_SPACING_TABULAR &&
                          (*text < '0' || *text > '9'))
                     ? char_width(font, *text)
                     : font->width;
    if (px > pos_x && px + cw > 32 * PANELS_X) {
      px = pos_x;
      py += font->height - 1;
    }
    draw_glyph(font, data_text, px, py, cw, *text);
    px += cw + 1;
  }
}
