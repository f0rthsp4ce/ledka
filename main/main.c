#include <driver/gpio.h>
#include <esp_log.h>
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

static bool ledmx_mktopo(uint8_t *idxes, char *error) {
  for (uint16_t i = 0; i < sizeof config.order / sizeof config.order[0]; i++)
    config.order[i] = 0xffff;

  for (uint8_t i = 0; i < PANELS_X * PANELS_Y; i++) {
    if (idxes[i] >= PANELS_X * PANELS_Y) {
      if (error)
        sprintf(error, "idxes[%d]=%d out of range", i, idxes[i]);
      return false;
    }
    uint16_t base = 16 * (PANELS_X * PANELS_Y - 1 - idxes[i]);
    if (config.order[base] != 0xffff) {
      if (error)
        sprintf(error, "idxes[%d]=%d already set", i, idxes[i]);
      return false;
    }

    for (uint8_t j = 0; j < 16; j++) {
      uint8_t x = 3 - (j / 4) + (i % PANELS_X) * 4;
      uint8_t y = j % 4 + (i / PANELS_X) * 4;
      y *= 4; // 4 fields per panel
      config.order[base + j] = x + y * PANELS_X * 4;
    }
  }
  if (error)
    *error = 0;
  return true;
}

static bool getpixel(uint8_t *data, uint32_t x, uint32_t y);

// TODO: experiment with ledmx_config and field_config, find the best
// permutation, and drop this code by making it non-configurable.  Or it is
// already the best permutation?
char ledmx_config[32] = "01256347";
char field_config[32] = "01,23";

static void ledmx_refresh(void *arg) {
  uint64_t start = esp_timer_get_time();

  uint8_t *data_a = data1_active ? data1 : data2;
  uint8_t *data_b = data3_active ? data3 : data4;
  static int draw_a_counter = 0;
  if (++draw_a_counter == 1)
    draw_a_counter = 0;
#ifdef OUTPUT_LED

  static size_t fc = 0;
  if (field_config[fc] == ',')
    fc++;
  if (field_config[fc] == 0)
    fc = 0;

  // for (int field = 0; field < 4; field++) {
  for (; field_config[fc] && field_config[fc] != ','; fc++) {
    int field = field_config[fc] - '0';

    for (char *conf = ledmx_config; *conf; conf++) {
      switch (*conf) {
      case '0':
        gpio_set_level(LEDMX_PIN_DATA, 0);
        break;
      case '1':
        gpio_set_level(LEDMX_PIN_OE, 1);
        break;

      case '2':
        for (int i = 0; i < 16 * PANELS_X * PANELS_Y; i++) {
          uint8_t byte = data_b[config.order[i] + field * PANELS_X * 4];
          if (draw_a_counter == 0)
            byte |= data_a[config.order[i] + field * PANELS_X * 4];
          ledmx_write_byte_rev(~byte);
        }
        break;

      case '3':
        gpio_set_level(LEDMX_PIN_SCLK, 1);
        break;
      case '4':
        gpio_set_level(LEDMX_PIN_SCLK, 0);
        break;
      case '5':
        gpio_set_level(LEDMX_PIN_A, (field & 1) == 0);
        break;
      case '6':
        gpio_set_level(LEDMX_PIN_B, (field & 2) == 0);
        break;
      case '7':
        gpio_set_level(LEDMX_PIN_OE, 0);
        break;
      case 'a':
        for (volatile int i = 0; i < 10; i++)
          ;
        break;
      case 'b':
        for (volatile int i = 0; i < 100; i++)
          ;
        break;
      case 'c':
        for (volatile int i = 0; i < 1000; i++)
          ;
        break;
      case 'd':
        for (volatile int i = 0; i < 10000; i++)
          ;
        break;
      case 'e':
        for (volatile int i = 0; i < 100000; i++)
          ;
        break;
      }
    }
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
  printf("Refresh took %llu us, got %d iterations\n",
         (unsigned long long)end - start, iteration - previous_iteration);
  previous_iteration = iteration;
  // 3391 3410 for 3*2 panels
  // 4480 5288 for 4*2 panels
}

static void putpixel(uint8_t *data, uint32_t x, uint32_t y, bool on) {
  if (x >= 32 * PANELS_X || y >= 16 * PANELS_Y) {
    return;
  }
  size_t offset = y * 32 * PANELS_X + x;
  if (on) {
    data[offset / 8] |= 1 << (offset % 8);
  } else {
    data[offset / 8] &= ~(1 << (offset % 8));
  }
}

static bool getpixel(uint8_t *data, uint32_t x, uint32_t y) {
  if (x >= 32 * PANELS_X || y >= 16 * PANELS_Y) {
    return false;
  }
  size_t offset = y * 32 * PANELS_X + x;
  return data[offset / 8] & (1 << (offset % 8));
}

static bool getpixel_wrap(uint8_t *data, uint32_t x, uint32_t y) {
  x = x % (32 * PANELS_X);
  y = y % (16 * PANELS_Y);
  size_t offset = y * 32 * PANELS_X + x;
  return data[offset / 8] & (1 << (offset % 8));
}

static void life_step(uint8_t *old, uint8_t *new) {
  for (int y = 0; y < 16 * PANELS_Y; y++) {
    for (int x = 0; x < 32 * PANELS_X; x++) {
      int neighbours = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
          if ((dx | dy) && getpixel_wrap(old, x + dx + 32 * PANELS_X,
                                         y + dy + 16 * PANELS_Y))
            neighbours++;

      if (getpixel(old, x, y))
        putpixel(new, x, y, neighbours == 2 || neighbours == 3);
      else
        putpixel(new, x, y, neighbours == 3);
    }
  }
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

void app_main() {
  example_configure_stdin_stdout();

  esp_timer_early_init();

  esp_timer_create_args_t args = {.callback = &ledmx_refresh,
                                  .arg = NULL,
                                  .dispatch_method = ESP_TIMER_TASK,
                                  .name = "ledmx_refresh"};
  // esp_timer_handle_t timer;
  ESP_ERROR_CHECK(esp_timer_create(&args, &timer));

  ledmx_mktopo(
      (uint8_t[]){
          // clang-format off
          1, 2, 3, 4,
          0, 5, 6, 7,
          // clang-format on
      },
      NULL);

  memset(data1, 0, sizeof data1);
  memset(data2, 0, sizeof data2);
  srand(esp_timer_get_time());
  for (size_t y = 0; y < 16 * PANELS_Y; y++) {
    for (size_t x = 0; x < 32 * PANELS_X; x++) {
      // putpixel(data1, x, y, x == 0 || y == 0 || x == y);
      putpixel(data1, x, y, rand() % 2);
    }
  }

  ledmx_init();
  bars_init();

#ifdef WIFI_ENABLED
  wifi_start();
  http_start();
  udp_start();
#endif

  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 10000));

  while (1) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    life_step(data1_active ? data1 : data2, data1_active ? data2 : data1);
    data1_active = !data1_active;
    if (config.bars_enabled) {
      bars_step();
      bars_draw();
    }
    xSemaphoreGive(data_mutex);

    vTaskDelay(1);
    ++iteration;
  }
}
