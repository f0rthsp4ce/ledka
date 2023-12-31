#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

// For uart
#include <driver/uart.h>
#include <esp_vfs_dev.h>
#include <soc/uart_periph.h>

#include "main.h"

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

static int iteration = 0;

static void ledmx_write_byte(uint8_t byte) {
  for (size_t i = 0; i < 8; i++) {
    gpio_set_level(LEDMX_PIN_DATA, byte & 1);
    gpio_set_level(LEDMX_PIN_CLK, 0);
    gpio_set_level(LEDMX_PIN_CLK, 1);
    byte >>= 1;
  }
}

static void ledmx_write_byte_rev(uint8_t byte) {
  for (size_t i = 0; i < 8; i++) {
    gpio_set_level(LEDMX_PIN_DATA, (byte & 0x80) >> 7);
    gpio_set_level(LEDMX_PIN_CLK, 0);
    gpio_set_level(LEDMX_PIN_CLK, 1);
    byte <<= 1;
  }
}

static void ledmx_init(void) {
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

bool ledmx_mktopo(uint8_t *idxes, char *error) {
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

static bool show_clock = false;

// TODO: experiment with field_config, find the best
// permutation, and drop this code by making it non-configurable.  Or it is
// already the best permutation?
char field_config[128] = "01,23";

static void ledmx_refresh(void *arg) {
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
        ledmx_write_byte(~byte);
      else
        ledmx_write_byte_rev(~byte);
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

esp_err_t example_configure_stdin_stdout(void) {
  static bool configured = false;
  if (configured) {
    return ESP_OK;
  }
  // Initialize VFS & UART so we can use std::cout/cin
  setvbuf(stdin, NULL, _IONBF, 0);
  /* Install UART driver for interrupt-driven reads and writes */
  ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
                                      256, 0, 0, NULL, 0));

  uart_config_t uart_config = {
      // .baud_rate = 9600,
      .baud_rate = 115200,
      // .baud_rate = 460800,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_MODE_UART,
  };

  uart_param_config(UART_NUM_0, &uart_config);

  /* Tell VFS to use UART driver */
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
  esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CR);
  /* Move the caret to the beginning of the next line on '\n' */
  esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CRLF);
  configured = true;
  return ESP_OK;
}

esp_timer_handle_t timer;

static void clock_task(void *arg) {
  time_t t = time(NULL);
  const time_t delta_pre = 2 * 86400, delta_post = 86400, delta_urgent = 60;
  const time_t time1 = 1700000000, time2 = 0x66666666;
  if ((t >= time1 - delta_urgent && t <= time1 + delta_urgent) ||
      (t >= time2 - delta_urgent && t <= time2 + delta_urgent))
    text_timeout = 0;
  if (text_timeout)
    return;
  if (t >= time1 - delta_pre && t <= time1 + delta_post)
    show_clock = true, draw_clock(t, time1, false);
  if (t >= time2 - delta_pre && t <= time2 + delta_post)
    show_clock = true, draw_clock(t, time2, true);
}

void app_main() {
  example_configure_stdin_stdout();

  esp_timer_early_init();

  esp_timer_create_args_t args = {.callback = &ledmx_refresh,
                                  .arg = NULL,
                                  .dispatch_method = ESP_TIMER_TASK,
                                  .name = "ledmx_refresh"};
  ESP_ERROR_CHECK(esp_timer_create(&args, &timer));

  // TODO: do I need separate esp_timer_create_args_t for clock_task?
  esp_timer_handle_t clock_timer;
  esp_timer_create_args_t clock_args = {.callback = &clock_task,
                                        .arg = NULL,
                                        .dispatch_method = ESP_TIMER_TASK,
                                        .name = "clock_task"};
  ESP_ERROR_CHECK(esp_timer_create(&clock_args, &clock_timer));

  const uint8_t rev = 0x80;
  ledmx_mktopo(
      (uint8_t[]){
          // clang-format off
          rev|5, rev|4, rev|3, rev|0,
          rev|7, rev|6, rev|2, rev|1,
          // clang-format on
      },
      NULL);

  memset(data1, 0, sizeof data1);
  memset(data2, 0, sizeof data2);
  srand(esp_timer_get_time());
  life_randomize(data1);

  ledmx_init();
  bars_init();

#ifdef WIFI_ENABLED
  wifi_start();
  http_start();
  udp_start();
#endif

  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 10000));

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
  ESP_ERROR_CHECK(esp_timer_start_periodic(clock_timer, 1000000 / 4)); // 4 Hz

  int randcol = -1;
  while (1) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    if (config.gol_enabled) {
      life_step(data1_active ? data1 : data2, data1_active ? data2 : data1);
      if (life_is_stalled(data1_active ? data2 : data1) && randcol < 0)
        randcol = 0;
      if (randcol >= 0) {
        life_randomize_col(data1_active ? data2 : data1, randcol);
        if (randcol++ == PANELS_X * 32)
          randcol = -1;
      }
      data1_active = !data1_active;
    }

    if (text_timeout)
      text_timeout--;

    if (config.bars_enabled) {
      bars_step();
      bars_draw();
    }
    xSemaphoreGive(data_mutex);

    vTaskDelay(1);
    ++iteration;
  }
}
