# ESPure Console

An interactive ESPure G2P example for the ESP32-P4 Rev. 1.3. It uses the
UART0 console (the board is currently enumerated as COM4 on the development
PC) and includes its own user-focused command set for phonemization, locale
selection, morphology, and benchmarking. The UTF-8 line editor is shared with
the developer example; command implementations are independent.

## Build and flash

From this directory, with ESP-IDF exported:

```text
idf.py set-target esp32p4
idf.py build
idf.py -p COM4 flash monitor
```

The example defaults to a 32 MB QIO flash configuration, octal PSRAM at
200 MHz, German and English pilot locales, and the custom partition table in
`partitions.csv`. Adjust these settings if using different hardware.

## Commands

- `help` - list commands
- `phonemize <text>` or `p <text>` - convert text to IPA
- `rules <text>` or `r <text>` - phonemize using rules without dictionary lookup
- `morph <text>` or `m <text>` - inspect morphological decomposition
- `bench <word> [iterations]` or `b <word> [iterations]` - benchmark G2P
- `locale de-DE|en-GB` - switch locale
- `info` - show memory information

An input line that is not a recognized command is treated as text to
phonemize.
