# ESPure Developer Console

Interactive developer console for ESPure G2P on ESP32-P4.

## Features

- Real-time G2P phonemization with IPA output
- Morphological decomposition and compound splitting
- Performance benchmarking
- Memory and system diagnostics
- Multi-locale support (de-DE, en-GB)

## Available Commands

- `phonemize <word>` (or `p <word>`) - Full G2P pipeline with phoneme stream analysis
- `rules <word>` (or `r <word>`) - Pure rules-only bypass (no dictionary)
- `morph <word>` (or `m <word>`) - Morphological decomposition
- `bench <word> [N]` (or `b <word>`) - Benchmark N phonemizations (default: 1000)
- `locale <loc>` - Switch active locale (de-DE, en-GB)
- `info` - Show memory, PSRAM, and dictionary statistics
- `help` - Display command help

## ESP32-P4 Serial Connection

- UART0 console: 115200 baud (typically COM4 on Windows)
- The COM port number is assigned by Windows and may vary

## Build and Flash

From this directory, with ESP-IDF exported:

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p COM4 flash monitor
```

Replace `COM4` with your actual serial port.

For bounded serial capture with automatic log saving:

```bash
esp-build.bat monitor
esp-build.bat flash monitor
```

Logs are saved to `.data/esp-build-logs/`. Configure `monitor.save_log` and 
`monitor.max_log_size_bytes` in `esp-build-config.json`.

## Hardware Requirements

- ESP32-P4 Rev. 1.3 or later
- 32 MB QIO flash
- 16-line HEX PSRAM at 200 MHz
- USB serial connection for console

## Dependencies

- `esp-idf-espure-g2p` - ESPure G2P component (parent directory)
- `esp-idf-uart-utf8-console` - UTF-8 console line editor

The example expects sibling checkouts for local development. For standalone 
builds, update `main/idf_component.yml` to fetch from Component Manager or Git.

## Example Session

```
ESPURE-G2P: Developer Console | ESP32-P4 Rev. 1.3
═════════════════════════════════════════════════════════════
Type help for available commands.

ESP-G2P-DEV> p Willkommen
Word: Willkommen [de-DE]
  -> IPA: /ˈvɪlˌkɔmən/
  Latency: 245 us
  -> Stream Segments (9): [23, 8, 15, 15, 11, 18, 16, 7, 19]

ESP-G2P-DEV> bench Willkommen 5000
Benchmarking 'Willkommen' across 5000 iterations...
  Result: /ˈvɪlˌkɔmən/
  Total time : 1234567 us
  Avg latency: 246.91 us/word
  Throughput : 4050 words/second
```
