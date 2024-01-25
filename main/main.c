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

int iteration = 0;
bool show_clock = false;
esp_timer_handle_t timer;

static esp_err_t ledka_main_configure_uart(void);
static esp_timer_handle_t ledka_main_clock1_init(void);
static esp_timer_handle_t ledka_main_clock2_init(void);

void app_main() {
  ledka_main_configure_uart();
  esp_timer_early_init();

  esp_timer_create_args_t args = {.callback = &ledmx_refresh,
                                  .arg = NULL,
                                  .dispatch_method = ESP_TIMER_TASK,
                                  .name = "ledmx_refresh"};
  ESP_ERROR_CHECK(esp_timer_create(&args, &timer));

  esp_timer_handle_t clock_timer =
#if LEDKA_VERSION == 1
      ledka_main_clock1_init()
#elif LEDKA_VERSION == 2
      ledka_main_clock2_init()
#endif
      ;

  ledmx_mktopo(default_topo, NULL);

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
    /*
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
    */

    if (text_timeout)
      text_timeout--;

    // TODO: hide bars on ledka2
    /*
    if (config.bars_enabled) {
      bars_step();
      bars_draw();
    }
    */
    xSemaphoreGive(data_mutex);

    vTaskDelay(1);
    ++iteration;
  }
}

static esp_err_t ledka_main_configure_uart(void) {
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
  return ESP_OK;
}

// Clock for LEDKA 1. Clock is displayed during anniversary UNIX timestamps.

static void ledka_main_clock1_task(void *arg) {
  time_t t = time(NULL);
  const time_t delta_pre = 2 * 86400, delta_post = 86400, delta_urgent = 60;
  const time_t time1 = 1700000000, time2 = 0x66666666;
  if ((t >= time1 - delta_urgent && t <= time1 + delta_urgent) ||
      (t >= time2 - delta_urgent && t <= time2 + delta_urgent))
    text_timeout = 0;
  if (text_timeout)
    return;
  show_clock = false;
  if (t >= time1 - delta_pre && t <= time1 + delta_post)
    show_clock = true, draw_clock1(t, time1, false);
  if (t >= time2 - delta_pre && t <= time2 + delta_post)
    show_clock = true, draw_clock1(t, time2, true);
}

static esp_timer_handle_t ledka_main_clock1_init(void) {
  esp_timer_handle_t clock_timer;
  static esp_timer_create_args_t clock_args = {
      .callback = &ledka_main_clock1_task,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "clock_task",
  };
  ESP_ERROR_CHECK(esp_timer_create(&clock_args, &clock_timer));
  return clock_timer;
}

// Clock for LEDKA 2. Clock is displayed all the time.
static void ledka_main_clock2_task(void *arg) {
  time_t t = time(NULL);
  t += 4 * 3600; // UTC+4
  draw_clock2(t);
}

static esp_timer_handle_t ledka_main_clock2_init(void) {
  show_clock = true;
  esp_timer_handle_t clock_timer;
  static esp_timer_create_args_t clock_args = {
      .callback = &ledka_main_clock2_task,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "clock_task",
  };
  ESP_ERROR_CHECK(esp_timer_create(&clock_args, &clock_timer));
  return clock_timer;
}
