#include "../main/main.h"
#include "lib.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint8_t data_text[64 * PANELS_X * PANELS_Y],
    data_text_mask[64 * PANELS_X * PANELS_Y];

int main(int argc, char **argv) {
  if (argc == 1)
    draw_panels_hint((uint8_t[]){0, 1, 2, 3, 0x84, 5, 6, 7});
  else
    draw_text(argv[1], 64, 0);
  static char buf[PANELS_X * 32 * PANELS_Y * 16 + 128];
  render_life(buf, PANELS_X * 32, PANELS_Y * 16, data_text);
  printf("%s", buf);
}
