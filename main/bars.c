#include <string.h>

#include "main.h"

#define BAR_COUNT 32u
#define BAR_HEIGHT 32u
#define BAR_POS_X 0u
#define BAR_POS_Y 0u

static struct {
  uint8_t value, position;
  uint8_t max_value, max_position;
} bars[BAR_COUNT];
static int age = 0;

// We want to avoid out-of-bounds access on small screens
#define RETURN_IF_NOT_SUPPORTED(V)                                             \
  do {                                                                         \
    if (PANELS_X < 4 || PANELS_Y < 2)                                          \
      return V;                                                                \
  } while (0)

void bars_init(void) {
  RETURN_IF_NOT_SUPPORTED();
  bars_set(NULL, 0);
}

void bars_set(const uint8_t *values, size_t count) {
  RETURN_IF_NOT_SUPPORTED();
  age = 0;
  // printf("bars_set:");
  for (size_t i = 0; i < BAR_COUNT; i++) {
    uint8_t value = i < count ? values[i] : 0;
    // printf(" %d", i < count ? values[i] : -1);
    bars[i].value = value;
    bars[i].position = BAR_HEIGHT - value * BAR_HEIGHT / 255;
    if (value > bars[i].max_value) {
      bars[i].max_value = value;
      bars[i].max_position = bars[i].position;
    }
  }
  // printf("\n");
}

void bars_step(void) {
  RETURN_IF_NOT_SUPPORTED();
  age++;
  // TODO: under mutex
  if (age == 32)
    bars_set(NULL, 0);
  for (size_t i = 0; i < BAR_COUNT; i++) {
    int max_value = bars[i].max_value - 1;
    if (max_value < 0)
      max_value = 0;
    else if (max_value < bars[i].value)
      max_value = bars[i].value;
    bars[i].max_value = max_value;
    bars[i].max_position = BAR_HEIGHT - max_value * BAR_HEIGHT / 255;
  }
}

void bars_draw(void) {
  RETURN_IF_NOT_SUPPORTED();
  uint8_t *data_a = data1_active ? data1 : data2;
  uint8_t *data_b = data_bars;

  for (int y = 0; y < BAR_HEIGHT; y++) {
    uint8_t *row_a = data_a + (BAR_POS_X / 8) + (y + BAR_POS_Y) * 4 * PANELS_X;
    uint8_t *row_b = data_b + (BAR_POS_X / 8) + (y + BAR_POS_Y) * 4 * PANELS_X;
    for (int i = 0; i < BAR_COUNT / 2; i++) {
      uint8_t val = 0;
      if (y >= bars[i * 2].position || y == bars[i * 2].max_position)
        val |= 0x07;
      if (y >= bars[i * 2 + 1].position || y == bars[i * 2 + 1].max_position)
        val |= 0x70;
      *row_a++ |= val;
      *row_b++ = val;
    }
  }
}
