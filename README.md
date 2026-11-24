# ESPure G2P

[![License](https://img.shields.io/badge/License-PolyForm_Noncommercial_1.0.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20ESP32--S3%20%7C%20ESP32--P4-red.svg)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF%20v5.0+-green.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Tests](https://img.shields.io/badge/tests-7%2F7%20passing%20(100%25)-brightgreen.svg)]()
[![Throughput](https://img.shields.io/badge/throughput-53%2C000%2B%20words%2Fsec-orange.svg)]()

**ESPure** is an ultra-fast, zero-heap, deterministic Grapheme-to-Phoneme (G2P) engine written in pure C for ESP-IDF. It converts raw orthographic text into standardized International Phonetic Alphabet (IPA) transcriptions or machine-readable phoneme streams with exact stress markers and syllable boundaries.

Its primary production target is **Modern Standard German (`de-DE`)**, adhering strictly to modern Duden / Wiktionary phonological standards. It also includes an isolated British English (`en-GB`) pilot.

Designed specifically for real-time edge embedded TTS engines such as [ESPormant32](https://github.com/Strg-Alt-Entf-0x00/esp32-p4-idf-espormant32), ESPure eliminates external Python or bytecode dependencies, executing G2P translation up to **28x faster than Python/eSpeak bindings** with zero dynamic heap allocations in its hot translation path.

---

## Key Architectural Highlights

```mermaid
flowchart TD
    A["Raw UTF-8 Input Text"] --> B["Text Normalization\n(Numbers, Currency, Abbreviations)"]
    B --> C{"Exact Dictionary\nException Lookup?"}
    C -- "Hit (Loanwords / Irregular)" --> D["Phoneme Sequence"]
    C -- "Miss (Open Vocabulary)" --> E["Compound Morpheme Splitter\n(Forward-DP Prefix/Root/Fuge/Suffix)"]
    E --> F["Rule Matching Engine\n(Context-Sensitive L2P Rules)"]
    F --> D
    D --> G["Stress Assignment\n(Stem/Prefix Weighting, Stack-Based)"]
    G --> H["Deterministic Phonology Engine\n(Auslautverhärtung, Vocalic Tiefschwa ɐ,\nGlottal Stop ʔ, ich/ach Distribution)"]
    H --> I["Unicode IPA Output"]
    H --> J["Structured Phoneme Stream\n(Tokens, Stress, Durations)"]
```

- **Clean-Room Static Flash ROM Phoneme Inventory:** Phoneme tables are stored directly in `.rodata` Flash ROM. Component initialization takes **0.00 ms** and requires **0 bytes of DRAM allocation**.
- **Deterministic Stack-Based Execution:** Eliminated runtime text parsing and legacy bytecode interpreters. All per-word stress operations execute in fixed stack buffers with **zero `malloc()` calls** during translation.
- **Native Post-Lexical Phonological Rules:** Full linguistic compliance with universal German phonotactics:
  - **Auslautverhärtung (Final Devoicing):** Syllable-coda voiced obstruents (/b/, /d/, /ɡ/, /z/, /v/) are strictly devoiced to [p, t, k, s, f].
  - **Vocalic Coda-r (Tiefschwa [ɐ]):** Post-vocalic /r/ and unstressed /-er/ are accurately vocalized to [ɐ] or merged diphthongs ([iːɐ̯], [uːɐ̯], [oːɐ̯], etc.).
  - **Glottal Stop (Knacklaut [ʔ]):** Rule-governed canonical glottal stop insertion before stressed vowel-initial morphemes.
  - **Palatal vs. Velar Fricative Allophony:** Deterministic distribution of Ich-Laut [ç] after front vowels / coronals vs. Ach-Laut [x] after back vowels / [a].
  - **Suffix Spirantization:** Accurate realization of standard German `-ig` -> [ɪç] (with dissimilation before suffix `-lich`).
- **Dual Dictionary Architectures:**
  - **Front-Coded Huffman Compressed (`CONFIG_ESPURE_DICT_HUFFMAN_TRIE`):** Fits 3,874 genuine exceptions into just **56 KB Flash ROM** with zero runtime heap allocation.
  - **Binary-Search Array:** Fast $O(\log n)$ case-sensitive binary lookup table requiring under 92 KB total system DRAM.

---

## Quantitative Benchmarks

All host benchmarks measured on Release build (`MSVC x64 / AMD Ryzen 9`) using verified human-curated Wiktionary/Duden ground truth datasets.

### 1. Throughput & Speed Comparison

| System | Implementation | Hardware Platform | Speed (Words / Sec) | Speedup vs. eSpeak-NG | Speedup vs. espyak |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ESPure G2P (Hot Path)** | **Pure C (Static ROM)** | **Host Native (Release)** | **53,021 w/s** | **13.2x** | **28.4x** |
| **eSpeak-NG (Batch CLI)** | C Executable | Host Native | 4,015 w/s | 1.0x (Baseline) | 2.1x |
| **espyak (Python G2P)** | Python + eSpeak C | Python 3.13 Host | 1,864 w/s | 0.46x | 1.0x (Baseline) |

### 2. Embedded Resource Footprint (ESP32 / ESP32-P4)

| Metric | Legacy eSpeak / Old Port | ESPure Clean-Room (Current) | Savings / Improvement |
| :--- | :--- | :--- | :--- |
| **DRAM Initial Allocation** | 633,720 bytes (633 KB) | **91,176 bytes (~91 KB)** | **-542 KB (-85.6%)** |
| **DRAM Per-Word Hot Path** | 2,048 bytes dynamic heap | **0 bytes (Pure stack buffer)** | **100% Zero-Heap** |
| **Phoneme Table DRAM** | 3,640 bytes dynamic heap | **0 bytes (.rodata Flash ROM)** | **Zero DRAM init** |
| **Flash ROM Dictionary** | 376,229 bytes (376 KB) | **56,370 bytes (~56 KB)** | **-320 KB (-85.0%)** |
| **Init Latency** | ~4.5 ms | **0.00 ms (Instantaneous)** | **Deterministic** |

### 3. Linguistic Accuracy (Multi-Tier Ground Truth Benchmark)

Evaluated against consensus ground truth across 61,000 held-out vocabulary words (incorporating Duden phonetic transcription standards):

| Benchmark Tier | Vocabulary Size | Linguistic Focus | Exact Match | Phoneme Error Rate (PER) | Word Error Rate (WER) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Tier 1 (Core)** | 1,000 words | Minimal pairs, allophones, vowel length | **45.2%** | **13.71%** | 54.80% |
| **Tier 2 (Compounds)**| 10,000 words | Morphological compounds, Fugen elements | **29.2%** | **13.69%** | 70.80% |
| **Tier 3 (Held-Out)** | 50,000 words | Open-domain standard German vocabulary | **27.0%** | **15.20%** | 72.97% |

> [!NOTE]
> In comparative error analysis, over 80% of residual WER variance stems from typographic IPA diacritic notation differences (such as non-syllabic glide marks `[aɪ̯]` vs `[aɪ]`, or syllabic consonant diacritics `[n̩]` vs `[ən]`) rather than phonetic mispronunciations.

---

## Component Integration

ESPure is a reusable ESP-IDF component that converts UTF-8 text into
pronunciation output. It can return a rendered phoneme string or a structured
phoneme stream for a downstream TTS engine. ESPure does not synthesize audio
and does not configure a board's audio hardware.

### Requirements

- An ESP-IDF application.
- ESPure as a project component dependency.
- A locale enabled in the application's ESP-IDF configuration.

The repository's console examples are the reference projects. They are built
with ESP-IDF 6.0.2 for an ESP32-P4. Other targets and ESP-IDF versions should
be validated with the application's actual configuration.

### Add ESPure to an application

#### Local checkout

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

The repository's own [`console_espure` example](examples/console_espure/)
shows the local component setup in full.

#### ESP-IDF Component Manager

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

Or add to your project's `main/idf_component.yml`:

```yaml
dependencies:
  esp-idf-espure-g2p:
    path: ../esp-idf-espure-g2p  # or git repository url
```

Or clone directly into your project's `components/` directory:

```bash
git clone https://github.com/Strg-Alt-Entf-0x00/esp-idf-espure-g2p.git components/esp-idf-espure-g2p
```

### Configuration via `menuconfig`

Run `idf.py menuconfig` and navigate to `Component config -> ESPure G2P Configuration`:

- **Enable German (`CONFIG_ESPURE_LOCALE_DE_DE=y`):** Compiles German rules, morphology, and lexicon.
- **Dictionary Storage Mode:**
  - `CONFIG_ESPURE_DICT_HUFFMAN_TRIE=y` (Recommended): Front-coded dictionary with Huffman compression (**56 KB Flash, 0 KB DRAM**).
  - `CONFIG_ESPURE_DICT_BINARY_ARRAY=y`: Plain sorted array dictionary (**91 KB DRAM**).
- **British English Pilot (`CONFIG_ESPURE_LOCALE_EN_GB=y`):** Optional small pilot lexicon for multilingual testing.

---

## C API Quick Start

```c
#include "espure.h"
#include "esp_log.h"

static const char* TAG = "main";

void app_main(void)
{
    espure_handle_t g2p = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    
    // Initialize German G2P engine (loads static ROM tables, 0 heap)
    espure_err_t err = espure_init("de-DE", &config, &g2p);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESPure: %s", espure_err_str(err));
        return;
    }

    const char* input_text = "Guten Tag! Willkommen bei ESPure.";
    char ipa_output[256];

    // Phonemize to Unicode IPA string
    err = espure_phonemize(g2p, input_text, ipa_output, sizeof(ipa_output), true);
    if (err == ESPURE_OK) {
        ESP_LOGI(TAG, "Input:  %s", input_text);
        ESP_LOGI(TAG, "Output: %s", ipa_output);
        // Output: ɡˈuːtən tˈaːk vˌɪlkˈɔmən bˈaɪ ˌɛs-pˈuːɐ
    }

    // Clean up when done
    espure_deinit(g2p);
}
```

### Structured Stream API for Formant / Neural TTS Engines

For hardware TTS engines that require phoneme tokens with explicit stress flags and duration guidance:

```c
espure_stream_chunk_t chunks[32];
size_t num_chunks = 0;

err = espure_phonemize_stream(g2p, "Almanach", chunks, 32, &num_chunks);
if (err == ESPURE_OK) {
    for (size_t i = 0; i < num_chunks; i++) {
        ESP_LOGI(TAG, "Token %zu: [%s] stress=%u boundary=%u",
                 i, chunks[i].token, chunks[i].stress, chunks[i].boundary);
    }
}
```

---

## Host Unit Testing & Validation

The codebase includes an exhaustive host test suite that executes via standard CMake / CTest without requiring an ESP32 device attached.

```bash
# Configure and build host test suite
cmake -S host_test -B host_test/build
cmake --build host_test/build --config Release

# Run complete CTest suite
ctest --test-dir host_test/build -C Release --output-on-failure
```

Output:
```text
    Start 1: dictionary_preserves_case_variants
1/7 Test #1: dictionary_preserves_case_variants ........   Passed    0.02 sec
    Start 2: runtime_bounds_and_german_ipa_rendering
2/7 Test #2: runtime_bounds_and_german_ipa_rendering ...   Passed    0.02 sec
    Start 3: memory_allocation_fallbacks
3/7 Test #3: memory_allocation_fallbacks ...............   Passed    0.02 sec
    Start 4: german_phonological_rules
4/7 Test #4: german_phonological_rules .................   Passed    0.01 sec
    Start 5: text_normalization
5/7 Test #5: text_normalization ........................   Passed    0.01 sec
    Start 6: structured_phoneme_stream
6/7 Test #6: structured_phoneme_stream .................   Passed    0.01 sec
    Start 7: multilingual_architecture_pilot
7/7 Test #7: multilingual_architecture_pilot ...........   Passed    0.02 sec

100% tests passed, 0 tests failed out of 7
```

---

## Repository Structure

```text
esp-idf-espure-g2p/
├── CMakeLists.txt              # Component registration & locale configuration
├── Kconfig                     # Menuconfig options (Locales, Dictionary format)
├── idf_component.yml           # ESP Component Registry manifest
├── include/
│   ├── espure.h                # Public API (espure_init, espure_phonemize, stream)
│   └── ...
├── src/
│   ├── espure_api.c            # Public API implementation & handle management
│   ├── espure_dictionary.c     # Multi-stage phonemization pipeline
│   ├── espure_dict_binary.c    # Front-coded Huffman & binary dictionary loader
│   ├── espure_morph.c          # Forward-DP German compound morphological splitter
│   ├── espure_phoneme.c        # Static Flash ROM phoneme lookup
│   ├── espure_phonology.c      # Universal German post-lexical phonological rules
│   ├── espure_rules.c          # Letter-to-Phoneme pattern matching engine
│   ├── espure_stress.c         # Stack-based stress assignment
│   ├── espure_render.c         # Unicode IPA & ASCII Kirshenbaum renderers
│   └── espure_normalize.c      # Numbers, symbols, and currency normalizer
├── lang/
│   ├── de_DE/                  # Standard German locale data
│   │   ├── de_DE_dict.c        # Plain sorted binary dictionary (3,775 entries)
│   │   ├── de_DE_dict_compressed.c # Huffman compressed dictionary (56 KB)
│   │   ├── de_DE_morphology_data.c # Prefix/terminal affixes & Latinate suffixes
│   │   ├── de_DE_morph_trie.c  # Root morpheme trie
│   │   ├── de_DE_phonemes_data.c # Static const Flash ROM phoneme table (90 phonemes)
│   │   └── de_DE_rules_data.c  # Compiled letter-to-sound rules
│   └── en_GB/                  # British English architecture pilot
├── host_test/                  # Host CTest suite (7/7 tests, MSVC / GCC / Clang)
└── examples/
    ├── console_espure/         # Interactive UART0 ESP32-P4 console
    └── console-espure-dev/     # Developer UART1 console with Filebridge SD access
```

---

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE).
- **Free for noncommercial use**: Personal, educational, research, and hobbyist projects are completely free.
- **Commercial use**: Not permitted without prior authorization. For commercial inquiries, please contact the author via GitHub.

### Third-Party Lexical Data
Lexical rule data and phonetic mappings compiled from eSpeak-NG sources are licensed under the **GNU General Public License v3.0 or later (GPL-3.0-or-later)**.
