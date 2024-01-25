# Ledka

![License](https://img.shields.io/badge/license-Unlicense%20OR%20MIT-blue)

## Prepare

In the root of the project, create file named `/credentials.h` in the following format:

```c
#define WIFI_SSID "blablabla"
#define WIFI_PASS "blablabla"
```

## Build

```console
# Open a shell containing the toolchain. Might take a while.
nix develop '.#build'

# Set target
idf.py set-target esp32c3

$PYTHON_FONTGEN ./tools/fontgen.py
idf.py build
```

## Flash

```sh
sudo chown $USER /dev/ttyACM0
idf.py flash
```

# Pinout

## P10
```
     ┌─────┐
   A │ 2  1│  OE
   B │ 4  3│  GND
     │ 6  5│  GND
 CLK │ 8  7├┐ GND
SCLK │10  9├┘ GND
DATA │12 11│  GND
     │14 13│  GND
     │16 15│  GND
     └─────┘
```

## LuatOS ESP32-C3
```
      ┌────────────┐
      │            │
      │            │
  GND ├•──────────•┤  GND    →    P10:3  GND
 IO00 ├•          •┤ 3.3V
 IO01 ├•::::      •┤ IO02    →    P10:2  A
 IO12 ├• ┌────┐   •┤ IO03    →    P10:4  B
 IO18 ├• │    │   •┤ IO10    →    P10:8  CLK
 IO19 ├• └────┘   •┤ IO06    →    P10:10 SCLK
  GND ├•  ┬────┬  •┤ IO07    →    P10:12 DATA
  RX0 ├•  ┤    ├  •┤ IO11    →    P10:1  OE
  TX0 ├•  ┴────┴  •┤ GND
 IO13 ├•  FLASH   •┤ 3.3V
   NC ├•RST   BOOT•┤ IO05
   EN ├•┌─┐    ┌─┐•┤ IO04
 3.3V ├•└─┘    └─┘•┤ IO08
  GND ├•          •┤ IO09
  PWB ├•          •┤ +5V
  +5V ├•  ┌────┐  •┤ GND
      │   │    │   │
      └───┴────┴───┘
```
