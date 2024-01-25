#include "main.h"

#if LEDKA_VERSION == 1
// Reversed bit 7 means the panel is rotated 180 degrees
static const uint8_t rev = 0x80;
#endif

// The default topology, i.e. the order in which the panels are connected
// to each other.
const uint8_t default_topo[PANELS_X * PANELS_Y] = {
// clang-format off
  #if LEDKA_VERSION == 1
    rev|5, rev|4, rev|3, rev|0,
    rev|7, rev|6, rev|2, rev|1,
  #elif LEDKA_VERSION == 2
    0,
  #endif
    // clang-format on
};

SemaphoreHandle_t data_mutex;
bool data1_active = true;
uint8_t data1[64 * PANELS_X * PANELS_Y];
uint8_t data2[64 * PANELS_X * PANELS_Y];

uint8_t data_bars[64 * PANELS_X * PANELS_Y];
uint8_t data_text[64 * PANELS_X * PANELS_Y];
uint8_t data_text_mask[64 * PANELS_X * PANELS_Y];

uint16_t text_timeout = 0;

uint16_t order[16 * PANELS_X * PANELS_Y];

struct config_t config = {
    .order = {0},
    .gol_enabled = true,
    .bars_enabled = true,
};

struct stats_t stats = {0};
