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

idf.py build
```

## Flash

```sh
sudo chown $USER /dev/ttyACM0
idf.py flash
```
