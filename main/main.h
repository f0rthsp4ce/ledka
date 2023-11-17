#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "fonts.h"

// Common
#define PANELS_X 4
#define PANELS_Y 2

#define OUTPUT_LED
// #define OUTPUT_UART

#define WIFI_ENABLED

// globals.c
#ifdef ESP_PLATFORM
extern SemaphoreHandle_t data_mutex;
#endif
extern bool data1_active;
extern uint8_t data1[64 * PANELS_X * PANELS_Y];
extern uint8_t data2[64 * PANELS_X * PANELS_Y];

extern uint8_t data_bars[64 * PANELS_X * PANELS_Y];
extern uint8_t data_text[64 * PANELS_X * PANELS_Y];
extern uint8_t data_text_mask[64 * PANELS_X * PANELS_Y];

extern uint16_t text_timeout;

#ifdef ESP_PLATFORM
extern SemaphoreHandle_t config_mutex;
#endif
extern struct config_t {
  uint16_t order[16 * PANELS_X * PANELS_Y];
  bool gol_enabled;
  bool bars_enabled;
} config;

extern struct stats_t {
  uint32_t render_time[512];
  uint32_t data_update_time[512];
  int64_t last_data_time;
  size_t render_time_pos;
  size_t data_update_time_pos;
} stats;

// bars.c
void bars_init(void);
void bars_set(const uint8_t *values, size_t count);
void bars_step(void);
void bars_draw(void);

// http.c
void http_start(void);

// ledmx.c
bool ledmx_mktopo(uint8_t *idxes, char *error);
void ledmx_init(void);
void ledmx_refresh(void *arg);

// life.c
void putpixel(uint8_t *data, uint32_t x, uint32_t y, bool on);
void life_randomize(uint8_t *data);
void life_randomize_col(uint8_t *data, int x);
void life_step(const uint8_t *old, uint8_t *new);
bool life_is_stalled(const uint8_t *data);

// text.c
void draw_panels_hint(uint8_t *topo);
void draw_clock(time_t t, time_t target, bool hex);
const struct font_t *find_font_by_name(const char *name);
void reset_text(void);
enum text_flags_t {
  TEXT_SPACING_MASK = 0b11,

  TEXT_SPACING_VAR = 0b00,
  TEXT_SPACING_MONO = 0b01,
  TEXT_SPACING_TABULAR = 0b10,
};
void draw_text(const struct font_t *font, const char *text, int16_t pos_x,
               int16_t pos_y, enum text_flags_t flags);

// wifi.c
void wifi_start(void);

// udp.c
void udp_start(void);
