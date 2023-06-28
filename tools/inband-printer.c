#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_life(const uint8_t *data, size_t len) {
  size_t width = data[0] | (data[1] << 8);
  size_t height = data[2] | (data[3] << 8);
  data += 4;

  static char buf[1024 * 1024];
  char *p = buf;

  for (size_t y = 0; y < height / 2; y++) {
    const uint8_t *row1 = data + (y * 2) * width / 8;
    const uint8_t *row2 = data + (y * 2 + 1) * width / 8;

    for (size_t x = 0; x < width / 8; x++) {
      uint8_t c1 = row1[x], c2 = row2[x];
      for (int i = 0; i < 8; i++) {

        switch (((c1 >> i) & 1) | (((c2 >> i) & 1) << 1)) {
        case 0:
          *p++ = ' ';
          break;
        case 1:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x80;
          break;
        case 2:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x84;
          break;
        case 3:
          *p++ = 0xe2;
          *p++ = 0x96;
          *p++ = 0x88;
          break;
        }
      }
    }
    *p++ = '|';
    *p++ = '\n';
  }
  *p = 0;
  printf("\033[2J\033[3J\033[H%s", buf);
}

int main(int argc, char **argv) {
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  const char start_marker[] = "<INBAND{";
  const char end_marker[] = "}INBAND>";
  size_t marker_pos = 0;
  char *offband_buf = malloc(1024 * 1024);
  size_t inband_pos = 0;

  for (;;) {
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof buf);
    if (n == 0)
      break;
    if (n < 0) {
      perror("read");
      return 1;
    }

    for (size_t i = 0; i < n; i++) {
      if (marker_pos < sizeof start_marker - 1) {
        if (buf[i] == start_marker[marker_pos])
          marker_pos++;
        else {
          for (size_t i = 0; i < marker_pos; i++)
            putchar(start_marker[i]);
          putchar(buf[i]);
          if (buf[i] == '\r')
            putchar('\n');
          marker_pos = 0;
          inband_pos = 0;
        }
      } else {
        if (buf[i] == end_marker[marker_pos - sizeof start_marker + 1])
          marker_pos++;
        else
          marker_pos = sizeof start_marker - 1;
        if (marker_pos == sizeof start_marker + sizeof end_marker - 2) {
          inband_pos -= sizeof end_marker - 1;
          if (getenv("NO") == NULL)
            print_life((uint8_t *)offband_buf, inband_pos);
          inband_pos = 0;
          marker_pos = 0;
        } else {
          offband_buf[inband_pos++] = buf[i];
        }
      }
    }
    fflush(stdout);
  }
}
