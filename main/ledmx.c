#include "main.h"

#include <driver/gpio.h>

#if CONFIG_IDF_TARGET_ESP32
#define LEDMX_PIN_A GPIO_NUM_13
#define LEDMX_PIN_B GPIO_NUM_12
#define LEDMX_PIN_CLK GPIO_NUM_14
#define LEDMX_PIN_SCLK GPIO_NUM_27
#define LEDMX_PIN_DATA GPIO_NUM_26
#define LEDMX_PIN_OE GPIO_NUM_25
#endif

#if CONFIG_IDF_TARGET_ESP32C3
#define LEDMX_PIN_A GPIO_NUM_2
#define LEDMX_PIN_B GPIO_NUM_3
#define LEDMX_PIN_CLK GPIO_NUM_10
#define LEDMX_PIN_SCLK GPIO_NUM_6
#define LEDMX_PIN_DATA GPIO_NUM_7
#define LEDMX_PIN_OE GPIO_NUM_11
#endif

extern int iteration;   // main.c
extern bool show_clock; // main.c

// Send a single byte to ledmx in a big endian order, LSB first.
static void ledmx_write_byte_le(uint8_t byte) {
  for (size_t i = 0; i < 8; i++) {
    gpio_set_level(LEDMX_PIN_DATA, byte & 1);
    gpio_set_level(LEDMX_PIN_CLK, 0);
    gpio_set_level(LEDMX_PIN_CLK, 1);
    byte >>= 1;
  }
}

// Send a single byte to ledmx in a big endian order, MSB first.
static void ledmx_write_byte_be(uint8_t byte) {
  for (size_t i = 0; i < 8; i++) {
    gpio_set_level(LEDMX_PIN_DATA, (byte & 0x80) >> 7);
    gpio_set_level(LEDMX_PIN_CLK, 0);
    gpio_set_level(LEDMX_PIN_CLK, 1);
    byte <<= 1;
  }
}

// Initialize ledmx GPIOs and mutex. Should be called before any other functions
// from this file.
void ledmx_init(void) {
  data_mutex = xSemaphoreCreateMutex();
  static int gpios[] = {
      LEDMX_PIN_A,    LEDMX_PIN_B,    LEDMX_PIN_CLK,
      LEDMX_PIN_SCLK, LEDMX_PIN_DATA, LEDMX_PIN_OE,
  };
  for (size_t i = 0; i < sizeof gpios / sizeof *gpios; i++) {
    gpio_pad_select_gpio(gpios[i]);
    gpio_set_direction(gpios[i], GPIO_MODE_OUTPUT);
  }
}

// Configure the topology of the LED matrix.
// @param idxes A list of panel indexes, in the order in which they are
// connected
//        a panel index.
//        with 0x80 to reverse the order of fields in the panel.
// @param error a pointer to a buffer to store an error message in case of
//        failure.
// @return true on success, false on error.
bool ledmx_mktopo(const uint8_t *idxes, char *error) {
  for (uint16_t i = 0; i < sizeof config.order / sizeof config.order[0]; i++)
    config.order[i] = 0xffff;

  for (uint8_t i = 0; i < PANELS_X * PANELS_Y; i++) {
    uint8_t idx = idxes[i] & 0x7f;
    bool reverse = !!(idxes[i] & 0x80);
    if (idx >= PANELS_X * PANELS_Y) {
      if (error)
        sprintf(error, "idxes[%d]=%d out of range", i, idxes[i]);
      return false;
    }
    uint16_t base = 16 * (PANELS_X * PANELS_Y - 1 - idx);
    if (config.order[base] != 0xffff) {
      if (error)
        sprintf(error, "idxes[%d]=%d already set", i, idxes[i]);
      return false;
    }

    for (uint8_t j = 0; j < 16; j++) {
      uint8_t x = j / 4;
      uint8_t y = j % 4;
      if (reverse)
        y = 3 - y;
      else
        x = 3 - x;

      x += (i % PANELS_X) * 4;
      y += (i / PANELS_X) * 4;

      y *= 4; // 4 fields per panel
      config.order[base + j] = x + y * PANELS_X * 4;
      if (reverse)
        config.order[base + j] |= 0x8000;
    }
  }
  if (error)
    *error = 0;
  return true;
}

// TODO: experiment with field_config, find the best
// permutation, and drop this code by making it non-configurable.  Or it is
// already the best permutation?
char field_config[128] = "01,23";

void ledmx_refresh(void *arg) {
  uint64_t start = esp_timer_get_time();

  uint8_t *data_life = data1_active ? data1 : data2;
#ifdef OUTPUT_LED

  static size_t fc = 0;
  if (field_config[fc] == ',')
    fc++;
  if (field_config[fc] == 0)
    fc = 0;

  for (; field_config[fc] && field_config[fc] != ','; fc++) {
    int field = field_config[fc] - '0';

    gpio_set_level(LEDMX_PIN_OE, 1);
    for (int i = 0; i < 16 * PANELS_X * PANELS_Y; i++) {
      bool reverse = config.order[i] & 0x8000;
      size_t idx = (config.order[i] & 0x7fff) +
                   (reverse ? 3 - field : field) * PANELS_X * 4;
      uint8_t byte = data_life[idx] | data_bars[idx];
      if (text_timeout || show_clock)
        byte = (byte & data_text_mask[idx]) | data_text[idx];
      if (reverse)
        ledmx_write_byte_le(~byte);
      else
        ledmx_write_byte_be(~byte);
    }
    gpio_set_level(LEDMX_PIN_A, (field & 1) == 0);
    gpio_set_level(LEDMX_PIN_B, (field & 2) == 0);
    gpio_set_level(LEDMX_PIN_SCLK, 1);
    gpio_set_level(LEDMX_PIN_SCLK, 0);

    // Send zeroes to reduce brightness
    gpio_set_level(LEDMX_PIN_DATA, 1);
    for (int i = 0; i < 8 * 16 * PANELS_X * PANELS_Y; i++) {
      gpio_set_level(LEDMX_PIN_CLK, 0);
      gpio_set_level(LEDMX_PIN_CLK, 1);
    }
    gpio_set_level(LEDMX_PIN_SCLK, 1);
    gpio_set_level(LEDMX_PIN_SCLK, 0);
    gpio_set_level(LEDMX_PIN_OE, 0);
  }
#endif

#ifdef OUTPUT_UART
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  uint8_t data_copy[sizeof data1];
  memcpy(data_copy, data, sizeof data_copy);
  xSemaphoreGive(data_mutex);

  uart_write_bytes(UART_NUM_0, "<INBAND{", sizeof "<INBAND{" - 1);
  uart_write_bytes(UART_NUM_0,
                   (uint8_t[]){
                       (PANELS_X * 32) & 0xff,
                       ((PANELS_X * 32) >> 8) & 0xff,
                       (PANELS_Y * 16) & 0xff,
                       ((PANELS_Y * 16) >> 8) & 0xff,
                   },
                   4);
  int sent = 0;
  while (sent < sizeof data1) {
    int rc =
        uart_write_bytes(UART_NUM_0, data_copy + sent, sizeof data1 - sent);
    if (rc < 0)
      break;
    sent += rc;
  }
  uart_write_bytes(UART_NUM_0, "}INBAND>", sizeof "}INBAND>" - 1);
#endif

  uint64_t end = esp_timer_get_time();

  static int previous_iteration = 0;
  // printf("Refresh took %llu us, got %d iterations\n",
  //        (unsigned long long)end - start, iteration - previous_iteration);
  previous_iteration = iteration;
}
