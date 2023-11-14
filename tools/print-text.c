#include "../main/main.h"
#include "lib.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint8_t data_text[64 * PANELS_X * PANELS_Y],
    data_text_mask[64 * PANELS_X * PANELS_Y];

int main(int argc, char **argv) {
  time_t target_time = time(NULL) + 3;
  for (;;) {
    static char buf[PANELS_X * 32 * PANELS_Y * 16 * 8 + 128];
    if (argc == 1)
      draw_panels_hint((uint8_t[]){0, 1, 2, 3, 0x84, 5, 6, 7});
    else if (!*argv[1])
      draw_clock(time(NULL), target_time, false);
    else {
      const struct font_t *font = find_font_by_name("BMplain");
      // const struct font_t *font = find_font_by_name("pzim3x5");
      reset_text();
      draw_text(font, argv[1], 0, 0, TEXT_SPACING_TABULAR);
    }
    render_life(buf, PANELS_X * 32, PANELS_Y * 16, data_text);
    printf("\x1b[H\x1b[2J\x1b[3J");
    printf("%s", buf);

    usleep(1000000 / 4);
  }
}
