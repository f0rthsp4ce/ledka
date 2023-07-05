#include <esp_log.h>
#include <sys/socket.h>

#include "main.h"

#define PORT 6454

#define TAG "udp"
#define CHECK(COND, ACTION)                                                    \
  do {                                                                         \
    if (!(COND)) {                                                             \
      ESP_LOGE(TAG, "%s:%d: check '%s' failed", __FILE__, __LINE__, #COND);    \
      ACTION;                                                                  \
    }                                                                          \
  } while (0)

static void serve(void *);
static void art_net_handler(const uint8_t *, size_t);
static void data_handler(const uint8_t *, size_t);

void udp_start(void) {
  xTaskCreate(serve, "udp_server", 4096, NULL /* arg */, 5, NULL);
}

static void serve(void *arg) {
  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  CHECK(s != -1, goto cleanup);

  struct sockaddr_in si_me = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr.s_addr = htonl(INADDR_ANY),
  };
  int rc = bind(s, (struct sockaddr *)&si_me, sizeof(si_me));
  CHECK(rc != -1, goto cleanup);

  const size_t buf_len = 1024;
  uint8_t *buf = malloc(buf_len);
  CHECK(buf != NULL, goto cleanup);
  while (1) {
    struct sockaddr_in si_other;
    socklen_t slen = sizeof si_other;
    ssize_t recv_len =
        recvfrom(s, buf, buf_len, 0, (struct sockaddr *)&si_other, &slen);
    CHECK(recv_len > 1, continue);

    if (recv_len >= 8 && !memcmp(buf, "Art-Net\0", 8))
      art_net_handler(buf, recv_len);
    else if (recv_len >= 1 && !memcmp(buf, "D", 1))
      data_handler(buf + 1, recv_len - 1);

    // buf[recv_len] = 0;
    // printf("Received packet from %s:%d\nData: %s\n\n",
    //        inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
  }

cleanup:
  if (s != -1)
    close(s);
}

static void art_net_handler(const uint8_t *buf, size_t packet_len) {
  uint64_t start = esp_timer_get_time();

  if (packet_len < 12)
    return;

  if (memcmp(buf, "Art-Net\0", 8) != 0)
    return;

  uint16_t opcode = (buf[9] << 8) | buf[8];

  uint16_t version = (buf[10] << 8) | buf[11];
  if (version < 14)
    return;

  switch (opcode) {
  case 0x2000: // TODO: OpPoll
    break;
  case 0x2100: // TODO: OpPollReply
    break;
  case 0x5000: { // OpDmx
    if (packet_len < 18)
      return;

    uint8_t sequence = buf[12];
    // TODO: handle sequence by dropping packets with sequence < last_sequence?
    (void)sequence;

    uint8_t physical = buf[13];
    if (physical > 3)
      return;

    uint16_t universe = (buf[15] << 8) | buf[14];
    if (universe & 0x8000)
      return;

    uint16_t length = (buf[16] << 8) | buf[17];
    if (length < 2 || length > 512 || packet_len < 18 + length)
      return;

    const uint8_t *data = buf + 18;

    switch (universe) {
    // TODO: properly check if the packet is for us
    case 0x2017:
      bars_set(data, length);
      break;
    }
  }
  }

  uint64_t end = esp_timer_get_time();
  printf("UDP took %llu us\n", (unsigned long long)(end - start));
}

static void send_poll_reply(void) {
  // Doc: https://art-net.org.uk/how-it-works/discovery-packets/artpollreply/

  char buf[239] = {0};
#define SET_U16(POS, VAL)                                                      \
  buf[POS] = (VAL)&0xff;                                                       \
  buf[POS + 1] = (VAL) >> 8;
// TODO: static assert that sizeof(STR) - 1 <= MAX_LEN
#define SET_STR(POS, STR, MAX_LEN) memcpy(buf + POS, STR, sizeof(STR) - 1)

  SET_STR(0, "Art-Net\0", 8);
  SET_U16(8, 0x2100); // OpPollReply
  buf[10] = 0;        // TODO: IP address
  buf[11] = 0;
  buf[12] = 0;
  buf[13] = 0;
  SET_U16(14, PORT);
  SET_U16(16, 0);       // firmware revision
  SET_U16(18, 0);       // TODO: port address
  SET_U16(20, 0);       // TODO: OEM
  buf[22] = 0;          // UBEA
  buf[23] = 0;          // Status1
  SET_U16(24, 0x7FF0U); // ESTA manufacturer code (7FF? reserved for prototype)

  SET_STR(26, "P10 Led Life", 18);  // Short name
  SET_STR(44, "P10 Led Life", 64);  // Long name
  SET_STR(108, "P10 Led Life", 64); // Node report

  SET_U16(172, 1); // Num ports
  buf[174] = 0xc0; // Port type 0: DMX512 input
  buf[175] = 0x00; // Port type 1: none
  buf[176] = 0x00; // Port type 2: none
  buf[177] = 0x00; // Port type 3: none
  buf[178] = 0x00; // Good input 0: TODO
  buf[179] = 0x00; // Good input 1: TODO
  buf[180] = 0x00; // Good input 2: TODO
  buf[181] = 0x00; // Good input 3: TODO
  buf[182] = 0x00; // Good output 0: TODO
  buf[183] = 0x00; // Good output 1: TODO
  buf[184] = 0x00; // Good output 2: TODO
  buf[185] = 0x00; // Good output 3: TODO
  buf[186] = 0x00; // SwIn 0: TODO
  buf[187] = 0x00; // SwIn 1: TODO
  buf[188] = 0x00; // SwIn 2: TODO
  buf[189] = 0x00; // SwIn 3: TODO
  buf[190] = 0x00; // SwOut 0: TODO
  buf[191] = 0x00; // SwOut 1: TODO
  buf[192] = 0x00; // SwOut 2: TODO
  buf[193] = 0x00; // SwOut 3: TODO
  buf[194] = 0x00; // SwVideo (deprecated)
  buf[195] = 0x00; // SwMacro (deprecated)
  buf[196] = 0x00; // SwRemote (deprecated)

  memset(buf + 197, 0, 3); // Spare
  buf[200] = 0x00;         // Style: TODO
  memset(buf + 201, 0, 6); // MAC address
  memset(buf + 207, 0, 4); // Bind IP
  buf[211] = 1;            // Bind index (1 - root device)
  buf[212] = 0xe; // Status2: Supports Art-Net III and got an address by DHCP
  memset(buf + 213, 0, 26);
}

static void data_handler(const uint8_t *buf, size_t packet_len) {
  if (packet_len != 64 * PANELS_X * PANELS_Y) {
    printf("Unexpected packet length: %zu\n", packet_len);
    return;
  }
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  const int64_t t = esp_timer_get_time();
  stats.data_update_time[stats.data_update_time_pos++] =
      t - stats.last_data_time;
  stats.last_data_time = t;
  stats.data_update_time_pos %=
      sizeof stats.data_update_time / sizeof *stats.data_update_time;

  data1_active = !data1_active;
  memcpy(data1_active ? data1 : data2, buf, packet_len);
  xSemaphoreGive(data_mutex);
}
