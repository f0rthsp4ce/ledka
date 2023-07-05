#include "main.h"

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
