# Moment
A project to play with ESP32. End goal is to connect an ESP32 to an e-ink screen and embed it into a photo frame. Pulls images from NATS object storage over Tailscale.

- [x] Connect to wifi
- [x] Connect to Tailscale
- [x] Connect to NATS (working but unreliable)
- [ ] Pull objects from NATS
- [ ] Write images to e-ink screen
- [ ] Report status back over NATS

## ESP Docs

https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html#introduction

## Environment

To activate the ESP-IDF environment via CLI:

```sh
source ~/.espressif/tools/activate_idf_v6.1.sh
```

### VSCode

To get code completion and navigation run "ESP-IDF: Add VS Code Configuration Files" once in the root of the project.

Open VsCode from moment/station for include paths to resolve correctly.

## Connection
Connect a data-capable USB-C cable to the COM connector of the board.

The serial port will be something like /dev/ttyUSB0

## Flashing

```sh
idf.py set-target esp32s3
idf.py menuconfig # (for project configuration like wifi settings)
idf.py build
idf.py -p PORT flash
idf.py -p PORT monitor

`flash` runs `build` implicitly. You can also combine the `flash` and `monitor` commands into one command line statement.
```
