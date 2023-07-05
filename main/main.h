#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

// bars.c
void bars_init(void);
void bars_set(const uint8_t *values, size_t count);
void bars_step(void);
void bars_draw(void);

// font.c
extern const unsigned char font_pzim3x5[96][3];

// http.c
void http_start(void);

// life.c
void putpixel(uint8_t *data, uint32_t x, uint32_t y, bool on);
void life_randomize(uint8_t *data);
void life_randomize_col(uint8_t *data, int x);
void life_step(const uint8_t *old, uint8_t *new);
bool life_is_stalled(const uint8_t *data);

// text.c
void draw_panels_hint(uint8_t *topo);
void draw_text(const char *text, uint16_t pos_x, uint16_t pos_y);

// wifi.c
void wifi_start(void);

// udp.c
void udp_start(void);
