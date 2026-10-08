/**
 * @file espure_internal.h
 * @brief Internal types and function declarations
 * 
 * Private API for espure components. Not exposed to users.
 */

#ifndef ESPURE_INTERNAL_H
#define ESPURE_INTERNAL_H

#include "espure.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <string.h>

#ifdef _MSC_VER
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
#endif

/* ========================================================================
 * Memory Management (espure_memory.c)
 * ======================================================================== */

/**
 * @brief Allocate memory with PSRAM preference if configured
 */
void* espure_malloc(size_t size, const espure_config_t* config);

/**
 * @brief Allocate and zero memory
 */
void* espure_calloc(size_t nmemb, size_t size, const espure_config_t* config);

/**
 * @brief Reallocate memory
 */
void* espure_realloc(void* ptr, size_t size, const espure_config_t* config);

/**
 * @brief Free memory
 */
void espure_free(void* ptr);

/**
 * @brief Duplicate string with PSRAM preference
 */
char* espure_strdup(const char* s, const espure_config_t* config);

/* ========================================================================
 * Phoneme Table (espure_phoneme.c)
 * ======================================================================== */

/* Phoneme types (from espeak-ng phoneme.h) */
#define ESPURE_PHONEME_INVALID    0
#define ESPURE_PHONEME_PAUSE      1
#define ESPURE_PHONEME_STRESS     2
#define ESPURE_PHONEME_VOWEL      3
#define ESPURE_PHONEME_LIQUID     4
#define ESPURE_PHONEME_STOP       5
#define ESPURE_PHONEME_VSTOP      6
#define ESPURE_PHONEME_FRICATIVE  7
#define ESPURE_PHONEME_VFRICATIVE 8
#define ESPURE_PHONEME_NASAL      9
#define ESPURE_PHONEME_VIRTUAL    10

/* Phoneme flags */
#define ESPURE_PHONEME_FLAG_UNSTRESSED   0x0001
#define ESPURE_PHONEME_FLAG_NOLINK       0x0002
#define ESPURE_PHONEME_FLAG_RHOTIC       0x0004

/**
 * Phoneme structure (internal representation)
 */

typedef struct {
    char mnemonic[8];      // Phoneme mnemonic ("@", "h", "l", etc.)
    uint8_t code;          // Numeric code (1-255)
    uint8_t type;          // PH_VOWEL, PH_STOP, etc.
    char* ipa;             // IPA string (optional, in PSRAM)
    uint8_t stress_type;   // For stress phonemes
    uint16_t flags;        // Feature flags
    char* program;         // Phoneme program text (raw, unparsed)
} espure_phoneme_t;

typedef struct espure_phoneme_table_s espure_phoneme_table_t;
typedef struct espure_translator_s espure_translator_t;
espure_err_t espure_phoneme_table_init(const char* lang, 
                                       const espure_config_t* config,
                                       espure_phoneme_table_t** out);

void espure_phoneme_table_deinit(espure_phoneme_table_t* table);

size_t espure_phoneme_table_size(const espure_phoneme_table_t* table);

/**
 * @brief Look up phoneme by mnemonic
 * 
 * @param table Phoneme table
 * @param mnemonic Phoneme mnemonic (e.g. "r", "r/", "@")
 * @return Phoneme pointer or NULL if not found
 */
const espure_phoneme_t* espure_phoneme_lookup(
    const espure_phoneme_table_t* table,
    const char* mnemonic
);

/**
 * @brief Parse concatenated phoneme string (greedy longest-match)
 * 
 * @param phoneme_table  Phoneme table for lookups
 * @param input          Input string (e.g., "h@loU")
 * @param output         Output array of phoneme pointers
 * @param max_output     Size of output array
 * @param out_count      Number of phonemes parsed (output)
 * @return ESPURE_OK on success
 */
espure_err_t espure_parse_phoneme_string(
    const espure_phoneme_table_t* phoneme_table,
    const char* input,
    const espure_phoneme_t** output,
    size_t max_output,
    size_t* out_count
);

/**
 * @brief Execute phoneme programs on a phoneme string (high-level integration)
 * 
 * Converts string â†’ list â†’ executes programs â†’ compacts â†’ converts back to string
 * 
 * @param phoneme_table Phoneme table for lookups
 * @param phoneme_str Input/output phoneme string (modified in place)
 * @param buf_size Size of phoneme_str buffer
 * @return ESPURE_OK on success
 */
espure_err_t espure_execute_phoneme_programs_on_string(
    const espure_translator_t* tr,
    const espure_phoneme_table_t* phoneme_table,
    char* phoneme_str,
    size_t buf_size
);

/* ========================================================================
 * Phsource Parser (espure_phsource_parser.c)
 * ======================================================================== */

/**
 * Parse phsource/phonemes format into phoneme array
 * 
 * @param content   Phsource file content (null-terminated)
 * @param phonemes  Output array (pre-allocated)
 * @param max_phonemes  Maximum phonemes to parse
 * @param num_parsed    Number of phonemes parsed (output)
 * @return ESPURE_OK on success
 */
espure_err_t espure_parse_phsource(const char* content, 
                                    espure_phoneme_t* phonemes,
                                    size_t max_phonemes,
                                    size_t* num_parsed);

/* ========================================================================
 * Rule Matching (espure_rules.c)
 * ======================================================================== */

/**
 * Match rules at given position in text
 * 
 * @param text Input text (lowercase)
 * @param pos Current position
 * @param out_phoneme Output phoneme buffer
 * @param out_len Output buffer size
 * @return Number of characters consumed (0 if no match)
 */
size_t espure_match_rule(const char* lang, const char* text, size_t pos,
                         char* out_phoneme, size_t out_len);

/**
 * Convert text to phonemes using rule matching
 * 
 * @param text Input text
 * @param output Output phoneme buffer
 * @param out_size Output buffer size
 * @return ESPURE_OK on success
 */
espure_err_t espure_text_to_phonemes(const char* lang, const char* text,
                                      char* output,
                                      size_t out_size);

/**
 * Get number of rules loaded
 */
size_t espure_rules_count(void);

/* ========================================================================
 * Dictionary (espure_dictionary.c)
 * ======================================================================== */

/**
 * @brief Embedded dictionary entry structure (used in static dictionary data)
 * 
 * This is the compact format for embedded dictionaries (en_dict_full.c).
 * It gets converted to runtime espure_dict_entry_t when loaded.
 */
typedef struct {
    const char* word;       // Dictionary word (UTF-8)
    const char* phonemes;   // Phoneme string (Kirshenbaum)
    uint32_t flags;         // Dictionary flags
} espure_dict_entry_embedded_t;

/**
 * @brief Runtime dictionary entry (pointer-based, FLASH references)
 */
typedef struct {
    const char* word;       // Pointer to word string in FLASH
    const char* phonemes;   // Pointer to phoneme string in FLASH
    uint32_t flags;         // Entry flags
} espure_dict_entry_t;

/**
 * @brief Dictionary structure (Binary Array + Binary Search)
 * 
 * MEMORY OPTIMIZATION: Binary array instead of hash-table
 * - Hash-table: 736 KB for 6,641 entries (DOESN'T FIT in 576 KB DRAM!)
 * - Binary array: 184 KB for 6,641 entries (FITS with 392 KB free!)
 */
typedef struct espure_dictionary_s {
    espure_dict_entry_t* entries;   // Sorted array (by word)
    size_t count;                    // Number of entries
    const uint8_t* binary_data;     // Binary data (if loaded from file)
    const espure_config_t* config;  // Configuration reference
} espure_dictionary_t;

espure_err_t espure_dictionary_init(const char* lang,
                                    const espure_config_t* config,
                                    espure_dictionary_t** out);

void espure_dictionary_deinit(espure_dictionary_t* dict);

size_t espure_dictionary_size(const espure_dictionary_t* dict);

/**
 * @brief Load embedded dictionary data for a language
 * 
 * @param dict Dictionary instance
 * @param lang Language code ("en", "de", etc.)
 * @return ESPURE_OK on success
 */
espure_err_t espure_dictionary_load_embedded(espure_dictionary_t* dict,
                                            const char* lang);

/* ========================================================================
 * Binary Dictionary Loader (espure_dict_binary.c)
 * ======================================================================== */

/**
 * @brief Load binary dictionary from memory
 * 
 * @param data Binary dictionary data (must remain valid!)
 * @param size Size of binary data
 * @param out_dict Output dictionary structure
 * @return ESP_OK on success
 */
esp_err_t espure_dict_load_binary(const uint8_t *data, size_t size, 
                                   espure_dictionary_t *out_dict);

/**
 * @brief Load binary dictionary from file (SPIFFS/LittleFS)
 * 
 * @param path File path (e.g., "/spiffs/en.dictbin")
 * @param out_dict Output dictionary structure
 * @return ESP_OK on success
 */
esp_err_t espure_dict_load_file(const char *path, espure_dictionary_t *out_dict);

/**
 * @brief Free binary dictionary
 * 
 * @param dict Dictionary to free
 */
void espure_dict_free_binary(espure_dictionary_t *dict);

/* ========================================================================
 * Translator (espure_dictionary.c)
 * ======================================================================== */



espure_err_t espure_translator_init(const char* lang,
                                    const espure_config_t* config,
                                    espure_phoneme_table_t* phoneme_table,
                                    espure_dictionary_t* dictionary,
                                    espure_translator_t** out);

void espure_translator_deinit(espure_translator_t* translator);

size_t espure_translator_size(const espure_translator_t* translator);

espure_err_t espure_translator_phonemize(espure_translator_t* translator,
                                         const char* text,
                                         char* output,
                                         size_t out_size,
                                         const espure_options_t* options);

/* ========================================================================
 * Rendering (espure_render.c)
 * ======================================================================== */

espure_err_t espure_render_phoneme_string(const espure_phoneme_table_t* table,
                                          const char* phonemes,
                                          char* output,
                                          size_t out_size,
                                          espure_format_t format,
                                          const char* separator,
                                          const char* tie);

/* ========================================================================
 * Hardware Acceleration (espure_hwaccel.c)
 * ======================================================================== */

#ifdef CONFIG_ESPURE_ENABLE_HWACCEL

/**
 * @brief Initialize PIE (Programmable IO Engine) for rule matching
 */
espure_err_t espure_hwaccel_init(void);

/**
 * @brief Cleanup hardware acceleration
 */
void espure_hwaccel_deinit(void);

/**
 * @brief Hardware-accelerated string pattern matching
 * 
 * Uses ESP32-P4 PIE for parallel pattern matching
 */
int espure_hwaccel_match(const char* text, const char* pattern, size_t text_len);

#endif /* CONFIG_ESPURE_ENABLE_HWACCEL */

/* ========================================================================
 * Utilities
 * ======================================================================== */

/**
 * @brief UTF-8 string length (character count, not bytes)
 */
size_t espure_utf8_strlen(const char* s);

/**
 * @brief UTF-8 character at position
 */
uint32_t espure_utf8_at(const char* s, size_t pos, size_t* bytes_read);

/**
 * @brief Normalize Unicode (NFD for combining marks)
 */
void espure_utf8_normalize(const char* input, char* output, size_t out_size);

/**
 * @brief Case conversion (language-aware)
 */
void espure_utf8_tolower(const char* input, char* output, size_t out_size);

/* ========================================================================
 * Debug Helpers
 * ======================================================================== */

#ifdef CONFIG_ESPURE_DEBUG_LOGGING
#define ESPURE_DEBUG(fmt, ...) ESP_LOGD("espure", fmt, ##__VA_ARGS__)
#else
#define ESPURE_DEBUG(fmt, ...) do {} while(0)
#endif

/* ESP32-P4 specific: Cache alignment for DMA buffers */
#ifdef CONFIG_ESPURE_CACHE_LINE_SIZE
#define ESPURE_CACHE_LINE_SIZE CONFIG_ESPURE_CACHE_LINE_SIZE
#else
#define ESPURE_CACHE_LINE_SIZE 64
#endif

#define ESPURE_ALIGN_CACHE __attribute__((aligned(ESPURE_CACHE_LINE_SIZE)))

/* Prefetch hint for cache optimization */
#define espure_prefetch(addr) __builtin_prefetch((addr), 0, 3)

/* IRAM placement for hot paths (if enabled) */
#ifdef CONFIG_ESPURE_IRAM_HOTPATHS
#define ESPURE_IRAM_ATTR IRAM_ATTR
#else
#define ESPURE_IRAM_ATTR
#endif

void espure_assign_word_stress(const espure_translator_t* tr, espure_phlist_entry_t* plist, size_t plist_len);

#endif /* ESPURE_INTERNAL_H */
