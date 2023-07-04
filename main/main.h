#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

// Common
#define PANELS_X 4
#define PANELS_Y 2

#define OUTPUT_LED
// #define OUTPUT_UART

#define WIFI_ENABLED

// globals.c
extern SemaphoreHandle_t data_mutex;
extern bool data1_active;
extern uint8_t data1[64 * PANELS_X * PANELS_Y];
extern uint8_t data2[64 * PANELS_X * PANELS_Y];

extern bool data3_active;
extern uint8_t data3[64 * PANELS_X * PANELS_Y];
extern uint8_t data4[64 * PANELS_X * PANELS_Y];

extern SemaphoreHandle_t config_mutex;
struct config_t {
  uint16_t order[16 * PANELS_X * PANELS_Y];
  bool gol_enabled;
  bool bars_enabled;
} config;

// bars.c
void bars_init(void);
void bars_set(const uint8_t *values, size_t count);
void bars_step(void);
void bars_draw(void);

// http.c
void http_start(void);

// life.c
void life_randomize(uint8_t *data);
void life_randomize_col(uint8_t *data, int x);
void life_step(const uint8_t *old, uint8_t *new);
bool life_is_stalled(const uint8_t *data);

// wifi.c
void wifi_start(void);

// udp.c
void udp_start(void);
