#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static char *render_life(char *p, size_t width, size_t height,
                         const uint8_t *data) {
  for (size_t y = 0; y < height / 2; y++) {
    const uint8_t *row1 = data + (y * 2) * width / 8;
    const uint8_t *row2 = data + (y * 2 + 1) * width / 8;

    for (size_t x = 0; x < width / 8; x++) {
      if (x % 4 == 0)
        p += sprintf(p, (x / 4 + y / 8) % 2 ? "\x1b[100;97m" : "\x1b[40;37m");

      uint8_t c1 = row1[x], c2 = row2[x];
      for (int i = 0; i < 8; i++) {

        switch (((c1 >> i) & 1) | (((c2 >> i) & 1) << 1)) {
        case 0:
          *p++ = ' ';
          break;
        case 1:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x80;
          break;
        case 2:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x84;
          break;
        case 3:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x88;
          break;
        }
      }
    }
    p += sprintf(p, "\x1b[0m\n");
  }
  *p = 0;
  return p;
}
