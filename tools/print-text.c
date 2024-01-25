#include "../main/main.h"
#include "lib.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint8_t data_text[64 * PANELS_X * PANELS_Y],
    data_text_mask[64 * PANELS_X * PANELS_Y];

int main(int argc, char **argv) {
  time_t start_time = time(NULL);
  time_t target_time = start_time + 3;
  ledka_clock_init();
  ledka_clock_timer_set(start_time + 60, start_time + 60 * 10);

  for (;;) {
    static char buf[PANELS_X * 32 * PANELS_Y * 16 * 8 + 128];
    time_t fast_time = start_time + (time(NULL) - start_time) * 60;

    if (!strcmp(argv[1], "hint")) {
      draw_panels_hint((uint8_t[]){0, 1, 2, 3, 0x84, 5, 6, 7});
    } else if (!strcmp(argv[1], "clock1")) {
      draw_clock1(time(NULL), target_time, false);
    } else if (!strcmp(argv[1], "clock2")) {
      ledka_clock_draw(fast_time);
    } else if (!strcmp(argv[1], "text")) {
      const struct font_t *font = find_font_by_name("BMSPA");
      reset_text();
      draw_text(font, argv[2], 0, 0, TEXT_SPACING_VAR);
    } else {
      fprintf(stderr, "Unknown mode: %s\n", argv[1]);
      return 1;
    }
    render_life(buf, PANELS_X * 32, PANELS_Y * 16, data_text);
    printf("\x1b[H\x1b[2J\x1b[3J");
    printf("%s", buf);

    usleep(1000000 / 4);
  }
}
