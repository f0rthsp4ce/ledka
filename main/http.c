#include <esp_http_server.h>
#include <inttypes.h>
#include <stdarg.h>

#include "main.h"

#define PRINT(BUF, P, ...) P += snprintf(P, BUF + sizeof BUF - P, __VA_ARGS__)
#define FLUSH()                                                                \
  do {                                                                         \
    if (p != buf)                                                              \
      httpd_resp_send_chunk(req, buf, p - buf);                                \
    p = buf;                                                                   \
  } while (0)
#define CHECK_ERR(ERR, MSG, ...)                                               \
  do {                                                                         \
    if ((ERR) != ESP_OK)                                                       \
      return return_error(req, (ERR), __FILE__, __LINE__, (MSG),               \
                          ##__VA_ARGS__);                                      \
  } while (0)

static esp_err_t return_error(httpd_req_t *req, esp_err_t err, const char *file,
                              int line, const char *msg, ...) {
  httpd_resp_set_status(req, "400 Bad Request");
  char buf[1024], *p = buf;
  PRINT(buf, p, "Error %d at %s:%d\n", err, file, line);

  va_list args;
  va_start(args, msg);
  p += vsnprintf(p, buf + sizeof buf - p, msg, args);
  va_end(args);
  PRINT(buf, p, "\n");

  esp_err_to_name_r(err, p, buf + sizeof buf - p);
  p += strlen(p);
  PRINT(buf, p, "\n");

  httpd_resp_send(req, buf, p - buf);
  return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req) {
  char buf[1024], *p = buf;
  PRINT(buf, p, "Hello, world!\n");
  PRINT(buf, p, "Time: %lld\n", esp_timer_get_time());
  PRINT(buf, p, "Free heap: %d\n", esp_get_free_heap_size());

  httpd_resp_send(req, buf, p - buf);
  return ESP_OK;
}

static esp_err_t get_stats_handler(httpd_req_t *req) {
  char buf[1024], *p = buf;
  PRINT(buf, p, "{\n  \"data_update_time\": [");
  const size_t n =
      sizeof stats.data_update_time / sizeof *stats.data_update_time;
  for (size_t i = 0; i < n; i++) {
    if (i != 0)
      PRINT(buf, p, ",");
    PRINT(buf, p, "%" PRIu32,
          stats.data_update_time[(i + stats.data_update_time_pos) % n]);
    if (p - buf > sizeof buf - 32)
      FLUSH();
  }
  FLUSH();
  PRINT(buf, p, "]\n}\n");
  FLUSH();
  httpd_resp_send_chunk(req, buf, 0);
  return ESP_OK;
}

static esp_err_t post_data_handler(httpd_req_t *req) {
  char buf[1024], *p = buf;
  uint8_t data[sizeof data1];
  if (req->content_len != sizeof data) {
    httpd_resp_set_status(req, "400 Bad Request");
    PRINT(buf, p, "Invalid content length: %d, expected %d\n", req->content_len,
          sizeof data);
    httpd_resp_send(req, buf, p - buf);
    return ESP_FAIL;
  }

  if (httpd_req_recv(req, (char *)data, sizeof data) != sizeof data) {
    httpd_resp_set_status(req, "400 Bad Request");
    PRINT(buf, p, "Invalid content length: %d, expected %d\n", req->content_len,
          sizeof data);
    httpd_resp_send(req, buf, p - buf);
    return ESP_FAIL;
  }

  xSemaphoreTake(data_mutex, portMAX_DELAY);
  const int64_t t = esp_timer_get_time();
  stats.data_update_time[stats.data_update_time_pos++] =
      t - stats.last_data_time;
  stats.last_data_time = t;
  stats.data_update_time_pos %=
      sizeof stats.data_update_time / sizeof *stats.data_update_time;

  data1_active = !data1_active;
  memcpy(data1_active ? data1 : data2, data, sizeof data);
  xSemaphoreGive(data_mutex);

  PRINT(buf, p, "OK\n");
  httpd_resp_send(req, buf, p - buf);
  return ESP_OK;
}

static esp_err_t post_config_handler(httpd_req_t *req) {
  // XXX: using static to avoid stack overflow
  static char query[512], value[512];

  esp_err_t err = httpd_req_get_url_query_str(req, query, sizeof query);
  CHECK_ERR(err, "Failed to get query string");

  // TODO: reduce code duplication

  err = httpd_query_key_value(query, "gol", value, sizeof value);
  if (err == ESP_OK) {
    if (strcmp(value, "on") == 0)
      config.gol_enabled = true;
    else if (strcmp(value, "off") == 0)
      config.gol_enabled = false;
    else
      return return_error(req, ESP_ERR_INVALID_ARG, __FILE__, __LINE__,
                          "Invalid value for 'gol': %s", value);
  }

  err = httpd_query_key_value(query, "bars", value, sizeof value);
  if (err == ESP_OK) {
    if (strcmp(value, "on") == 0)
      config.bars_enabled = true;
    else if (strcmp(value, "off") == 0)
      config.bars_enabled = false;
    else
      return return_error(req, ESP_ERR_INVALID_ARG, __FILE__, __LINE__,
                          "Invalid value for 'bars': %s", value);
  }

  err = httpd_query_key_value(query, "topo", value, sizeof value);
  if (err == ESP_OK) {
    uint8_t topo[PANELS_X * PANELS_Y] = {0}, *p = topo;
    for (char *pv = value; *pv; pv++) {
      printf("Handling '%c'\n", *pv);
      if (*pv == ',' && p < topo + sizeof topo - 1)
        p++;
      else if (*pv >= '0' && *pv <= '9')
        *p = *p * 10 + (*pv - '0');
      else if (*pv == 'r')
        *p |= 0x80;
      else
        goto error;
    }
    if (p != topo + sizeof topo - 1) {
    error:
      printf("Wrong size: %d\n", p - topo);
      return return_error(req, ESP_ERR_INVALID_ARG, __FILE__, __LINE__,
                          "Invalid value for 'topo': %s", value);
    }

    draw_panels_hint(topo);
    text_timeout = 500;
    bool ledmx_mktopo(uint8_t * idxes, char *error);
    char error[128];
    error[0] = 0;
    if (!ledmx_mktopo(topo, error))
      return return_error(req, ESP_ERR_INVALID_ARG, __FILE__, __LINE__,
                          "Invalid value for 'topo': %s", error);
  }

  err = httpd_query_key_value(query, "field", value, sizeof value);
  if (err == ESP_OK) {
    extern char field_config[32];
    memset(field_config, 0, sizeof field_config);
    strcpy(field_config, value);
  }

  err = httpd_query_key_value(query, "timer", value, sizeof value);
  if (err == ESP_OK) {
    uint64_t period = strtoull(value, NULL, 10);
    extern esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_stop(timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, period));
  }

  httpd_resp_send(req, "got it", -1);
  return ESP_OK;
  // esp_err_t err = httpd_query_key_value(req->query, "gol", (char *)order,
  // sizeof order);
}

static esp_err_t post_text_handler(httpd_req_t *req) {
  char query[512], value[512];
  esp_err_t err = httpd_req_get_url_query_str(req, query, sizeof query);
  CHECK_ERR(err, "Failed to get query string");

  // TODO: escapes, e.g. %0A for newline
  unsigned long long timeout = 100, pos_x = 0, pos_y = 0;

  err = httpd_query_key_value(query, "timeout", value, sizeof value);
  if (err == ESP_OK)
    timeout = strtoull(value, NULL, 10);
  err = httpd_query_key_value(query, "x", value, sizeof value);
  if (err == ESP_OK)
    pos_x = strtoull(value, NULL, 10);
  err = httpd_query_key_value(query, "y", value, sizeof value);
  if (err == ESP_OK)
    pos_y = strtoull(value, NULL, 10);

  err = httpd_query_key_value(query, "text", value, sizeof value);
  CHECK_ERR(err, "Failed to get text");

  draw_text(value, pos_x, pos_y);
  text_timeout = timeout;

  httpd_resp_send(req, "got it", -1);
  return ESP_OK;
}

static httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL,
};

static httpd_uri_t uri_get_stats = {
    .uri = "/stats",
    .method = HTTP_GET,
    .handler = get_stats_handler,
    .user_ctx = NULL,
};

static httpd_uri_t uri_post_data = {
    .uri = "/data",
    .method = HTTP_POST,
    .handler = post_data_handler,
    .user_ctx = NULL,
};

static httpd_uri_t uri_post_config = {
    .uri = "/config",
    .method = HTTP_POST,
    .handler = post_config_handler,
    .user_ctx = NULL,
};

static httpd_uri_t uri_post_text = {
    .uri = "/text",
    .method = HTTP_POST,
    .handler = post_text_handler,
    .user_ctx = NULL,
};

void http_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));
  httpd_register_uri_handler(server, &uri_get);
  httpd_register_uri_handler(server, &uri_get_stats);
  httpd_register_uri_handler(server, &uri_post_data);
  httpd_register_uri_handler(server, &uri_post_config);
  httpd_register_uri_handler(server, &uri_post_text);
}
