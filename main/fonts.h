#pragma once

struct font_t {
  const char *name;
  unsigned char width, height, flags;
  const unsigned char *data;
};

extern const struct font_t *fonts;
