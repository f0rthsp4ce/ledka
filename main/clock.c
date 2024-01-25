#include "main.h"

static time_t timer_start = 0;
static time_t timer_end = 0;

void ledka_clock_init(void) {}

void ledka_clock_draw(time_t now) {
  char buf[32], *p = buf;
  reset_text();
  const struct font_t *font = find_font_by_name("BMSPA");
  if (timer_start && timer_end && now >= timer_start && now <= timer_end) {
    int passed = now - timer_start;
    int remaining = timer_end - now;
    p += sprintf(p, "%d:%02d\n", passed / 3600 % 10, passed / 60 % 60);
    p += sprintf(p, "%d:%02d", remaining / 3600 % 10, remaining / 60 % 60);
    draw_text(font, buf, 0, 0, TEXT_SPACING_TABULAR);
  } else {
    p += sprintf(p, "%02d\n%02d", now / 3600 % 24, now / 60 % 60);
    draw_text(font, buf, (32 * PANELS_X - 2 * (font->width + 1)) / 2, 0,
              TEXT_SPACING_MONO);
  }
}

void ledka_clock_timer_set(time_t start, time_t end) {
  timer_start = start;
  timer_end = end;
}

/*
static int32_t timer_duration = 0;
void ledka_clock_timer_control_step(int32_t step) {}
void ledka_clock_timer_start(void) {}
void ledka_clock_timer_stop(void) {}
*/
