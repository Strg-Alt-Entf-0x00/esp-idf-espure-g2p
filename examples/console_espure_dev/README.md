# ESPure Developer Console

The developer firmware has its own command implementation so development,
diagnostic, and future AI-automation commands can evolve independently from
the user-facing console. It reuses only the UTF-8 line editor and adds the
UART filebridge, mounting its SD card at `/sd` for development tools and
automated workflows.

The developer command set also includes `bridge`, which reports whether the
UART filebridge initialized successfully, and `ls [path]`, which lists files
on the mounted SD card (defaults to `/sd`).

## ESP32-P4 Rev. 1.3 serial connections

- UART0 console: currently enumerated as COM4 on the development PC at
  115200 baud.
- UART1 filebridge: currently enumerated as COM13 through the board's
  FT232R adapter at 3 Mbaud, with RTS/CTS flow control.
- The filebridge defaults use TX GPIO30, RX GPIO31, RTS GPIO50, and CTS GPIO29.
  Confirm these pins against the actual board wiring before connecting.
- The COM numbers are assigned by Windows and can change; they are not
  firmware settings.

The filebridge mounts the SD card at `/sd`. If initialization or mounting
fails, the firmware logs the error and still starts the interactive console
on UART0.

## Build and flash

From this directory, with ESP-IDF exported:

```text
idf.py set-target esp32p4
idf.py build
idf.py -p COM4 flash monitor
```

The filebridge dependency uses the sibling
`esp-idf-uart-filebridge` checkout when available. For a standalone checkout,
remove its `override_path` entry from `main/idf_component.yml` so the
Component Manager can fetch it from Git.

The board defaults match the ESP32-P4 Rev. 1.3 setup: 32 MB QIO flash, octal
PSRAM at 200 MHz, and the custom partition table in `partitions.csv`.
