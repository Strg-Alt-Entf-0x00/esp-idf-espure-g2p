# ESPure G2P Integration

ESPure is a reusable ESP-IDF component that converts UTF-8 text into
pronunciation output. It can return a rendered phoneme string or a structured
phoneme stream for a downstream TTS engine. ESPure does not synthesize audio
and does not configure a board's audio hardware.

## Requirements

- An ESP-IDF application.
- ESPure as a project component dependency.
- A locale enabled in the application's ESP-IDF configuration.

The repository's console examples are the reference projects. They are built
with ESP-IDF 6.0.2 for an ESP32-P4. Other targets and ESP-IDF versions should
be validated with the application's actual configuration.

## Add ESPure to an application

### Local checkout

For a local sibling checkout, add the ESPure repository root to the
application's component search path:

```cmake
get_filename_component(ESPURE_COMPONENT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../esp-idf-espure-g2p"
    ABSOLUTE
)
set(EXTRA_COMPONENT_DIRS "${ESPURE_COMPONENT_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_g2p_app)
```

Adjust the relative path to match the directory layout. The application
component then declares ESPure by its ESP-IDF component name:

```cmake
idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp-idf-espure-g2p
)
```

The repository's own [`console_espure` example](../examples/console_espure/)
shows the local component setup in full.

### ESP-IDF Component Manager

For a release published to the ESP Component Registry, declare the dependency
in the consuming project's component manifest:

```yaml
dependencies:
  Strg-Alt-Entf-0x00/esp-idf-espure-g2p:
    version: "^1.0.0"
```

The `version` constraint is appropriate once that release is available from
the registry. For local multi-repository development, use an `override_path`
to the ESPure checkout instead; do not commit a machine-specific path to a
standalone project.

## Configure language data

ESPure language resources are selected at build time through Kconfig. Enable
the locale required by the application, for example:

```text
CONFIG_ESPURE_LOCALE_DE_DE=y
```

The optional `en-GB` locale is an architecture pilot with limited vocabulary,
not full English support:

```text
CONFIG_ESPURE_LOCALE_EN_GB=y
```

Choose the German dictionary representation in menuconfig, or enable the
compressed trie in `sdkconfig.defaults`:

```text
CONFIG_ESPURE_DICT_HUFFMAN_TRIE=y
```

Only selected locale sources are compiled. The dictionary and language data
are embedded in the firmware image; they are not loaded from the SD card at
runtime. `espure_config_t.use_psram` controls eligible runtime allocations,
not where the embedded dictionary is stored. Runtime dictionary replacement
is not part of the current API.

## Initialize and use ESPure

Include the public header and check every result. The same handle is not
documented as safe for simultaneous calls from multiple tasks; serialize
calls that share a handle.

```c
#include "espure.h"

void use_espure(void)
{
    espure_handle_t g2p = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();

    espure_err_t err = espure_init("de-DE", &config, &g2p);
    if (err != ESPURE_OK) {
        printf("ESPure initialization failed: %s\n", espure_err_str(err));
        return;
    }

    char ipa[256];
    err = espure_phonemize(g2p, "Hallo Welt", ipa, sizeof(ipa), true);
    if (err == ESPURE_OK) {
        printf("IPA: %s\n", ipa);
    } else {
        printf("Phonemization failed: %s\n", espure_err_str(err));
    }

    espure_deinit(g2p);
}
```

For TTS integrations that consume discrete segments, use
`espure_phonemize_stream()` and provide the segment buffer from the caller.
The returned stream points to that caller-owned buffer. The existing string
API can also produce IPA or Kirshenbaum output; rendered strings should not
be parsed as a stable machine interface when the structured stream is suitable.

Other useful public functions include:

- `espure_get_locales()` and `espure_locale_available()` for compiled locales.
- `espure_morph_split()` for morphological decomposition.
- `espure_version()` and `espure_err_str()` for diagnostics.

See [`espure.h`](../include/espure.h) for the complete public API.

## Reference examples

The user-facing console uses UART0 and contains the regular ESPure command
set. The developer console has a separate command implementation and adds
UART Filebridge and SD-card inspection. Each example README owns its exact
build, flash, board, and serial instructions:

- [User console example](../examples/console_espure/README.md)
- [Developer console example](../examples/console_espure_dev/README.md)

The developer example's `ls [path]` command lists files on the mounted SD
card (default `/sd`); it does not load those files as ESPure dictionaries.

## ESPormant32 TTS integration

[ESPormant32](../../esp32-p4-idf-espormant32/documentation/INTEGRATION.md)
is a separate consumer of ESPure. ESPure owns text-to-pronunciation
conversion and language resources; ESPormant32 owns its adapter, phoneme
mapping, prosody, Klatt synthesis, and audio output pipeline. Keep fixes in
the repository that owns the behavior.