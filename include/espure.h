/**
 * @file espure.h
 * @brief espure - espeak-ng G2P Engine for ESP32-P4
 * 
 * Pure C reimplementation of espeak-ng's grapheme-to-phoneme (G2P) engine,
 * optimized for ESP32-P4 Rev 1.3 with hardware acceleration support.
 * 
 * @copyright GPL-3.0-or-later (derived from espeak-ng)
 * @version 1.0.0
 */

#ifndef ESPURE_H
#define ESPURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define ESPURE_VERSION_MAJOR 1
#define ESPURE_VERSION_MINOR 0
#define ESPURE_VERSION_PATCH 0
#define ESPURE_VERSION "1.0.0"

/* Error codes */
typedef enum {
    ESPURE_OK = 0,              /**< Success */
    ESPURE_ERR_INVALID_ARG,     /**< Invalid argument */
    ESPURE_ERR_NO_MEM,          /**< Out of memory */
    ESPURE_ERR_NOT_FOUND,       /**< Language/resource not found */
    ESPURE_ERR_BUFFER_TOO_SMALL,/**< Output buffer too small */
    ESPURE_ERR_INVALID_LANG,    /**< Invalid language code */
    ESPURE_ERR_INIT_FAILED,     /**< Initialization failed */
    ESPURE_ERR_NOT_INITIALIZED, /**< Handle not initialized */
    ESPURE_ERR_INVALID_SIZE,    /**< Invalid size or length */
    ESPURE_ERR_NOT_SUPPORTED,   /**< Operation not supported */
} espure_err_t;

/* Forward declarations */
typedef struct espure_handle_s* espure_handle_t;

/**
 * @brief Configuration structure for espure initialization
 */
typedef struct {
    bool use_psram;              /**< Store language data in PSRAM */
    bool enable_hwaccel;         /**< Enable hardware acceleration (PIE) */
    bool force_compat;           /**< Force espeak-ng byte-for-byte compatibility */
    uint16_t dict_cache_size;    /**< Dictionary cache size (entries) */
    uint16_t max_word_length;    /**< Maximum word length (bytes, UTF-8) */
} espure_config_t;

/**
 * @brief Default configuration initializer macro
 */
#define ESPURE_CONFIG_DEFAULT() { \
    .use_psram = true, \
    .enable_hwaccel = true, \
    .force_compat = false, \
    .dict_cache_size = 256, \
    .max_word_length = 128, \
}

/**
 * @brief Output format selection
 */
typedef enum {
    ESPURE_FORMAT_IPA,          /**< Unicode IPA with stress marks */
    ESPURE_FORMAT_KIRSHENBAUM,  /**< ASCII Kirshenbaum (espeak -x) */
} espure_format_t;

/**
 * @brief Phonemization options
 */
typedef struct {
    espure_format_t format;      /**< Output format (IPA/Kirshenbaum) */
    const char* separator;       /**< Phoneme separator (NULL = none) */
    const char* tie;             /**< Tie character for multi-char phonemes */
    int8_t tonic_stress;         /**< Force tonic stress level (-1 = auto) */
} espure_options_t;

/**
 * @brief Default phonemization options
 */
#define ESPURE_OPTIONS_DEFAULT() { \
    .format = ESPURE_FORMAT_IPA, \
    .separator = NULL, \
    .tie = NULL, \
    .tonic_stress = -1, \
}

/**
 * @brief Performance statistics (if CONFIG_ESPURE_STATS_COLLECTION=y)
 */
typedef struct {
    uint32_t phonemize_calls;    /**< Total phonemize() calls */
    uint32_t dict_hits;          /**< Dictionary cache hits */
    uint32_t dict_misses;        /**< Dictionary cache misses */
    uint32_t total_chars;        /**< Total characters processed */
    uint64_t total_time_us;      /**< Total processing time (microseconds) */
    uint32_t peak_heap_used;     /**< Peak heap usage (bytes) */
} espure_stats_t;

/* ========================================================================
 * Core API
 * ======================================================================== */

/**
 * @brief Initialize a G2P translator for a specific language
 * 
 * @param[in]  lang   Language code (e.g., "en", "es", "fr", "pt-br")
 * @param[in]  config Configuration (NULL = default)
 * @param[out] handle Output handle
 * 
 * @return ESPURE_OK on success, error code otherwise
 * 
 * @note Thread-safe: Multiple handles can coexist
 * @note Language data loaded from embedded flash/PSRAM
 * 
 * Example:
 * @code
 * espure_handle_t g2p;
 * espure_config_t config = ESPURE_CONFIG_DEFAULT();
 * if (espure_init("en", &config, &g2p) == ESPURE_OK) {
 *     // Use g2p...
 *     espure_deinit(g2p);
 * }
 * @endcode
 */
espure_err_t espure_init(const char* lang, const espure_config_t* config, 
                         espure_handle_t* handle);

/**
 * @brief Convert text to phonemes
 * 
 * @param[in]  handle   G2P handle from espure_init()
 * @param[in]  text     Input text (UTF-8)
 * @param[out] output   Output buffer for phonemes
 * @param[in]  out_size Output buffer size (bytes)
 * @param[in]  ipa      true = IPA format, false = Kirshenbaum
 * 
 * @return ESPURE_OK on success, error code otherwise
 * 
 * @note Output is NUL-terminated UTF-8 string
 * @note Thread-safe with different handles, not with same handle
 * 
 * Example:
 * @code
 * char phonemes[256];
 * espure_phonemize(g2p, "hello world", phonemes, sizeof(phonemes), true);
 * printf("IPA: %s\n", phonemes);  // həlˈəʊ wˈɜːld
 * @endcode
 */
espure_err_t espure_phonemize(espure_handle_t handle, const char* text,
                              char* output, size_t out_size, bool ipa);

/**
 * @brief Convert text to phonemes with extended options
 * 
 * @param[in]  handle  G2P handle
 * @param[in]  text    Input text (UTF-8)
 * @param[out] output  Output buffer
 * @param[in]  out_size Output buffer size
 * @param[in]  options Phonemization options (NULL = default IPA)
 * 
 * @return ESPURE_OK on success, error code otherwise
 * 
 * Example:
 * @code
 * espure_options_t opts = ESPURE_OPTIONS_DEFAULT();
 * opts.separator = "_";
 * opts.format = ESPURE_FORMAT_KIRSHENBAUM;
 * espure_phonemize_ex(g2p, "cat", output, sizeof(output), &opts);
 * // Output: "k_'a_t"
 * @endcode
 */
espure_err_t espure_phonemize_ex(espure_handle_t handle, const char* text,
                                 char* output, size_t out_size,
                                 const espure_options_t* options);

/**
 * @brief Render raw phoneme string (espeak mnemonics) to IPA/Kirshenbaum
 * 
 * @param[in]  handle   G2P handle
 * @param[in]  phonemes Raw phoneme string (e.g., "h@l'oU w'3:ld")
 * @param[out] output   Output buffer
 * @param[in]  out_size Output buffer size
 * @param[in]  ipa      true = IPA, false = Kirshenbaum
 * 
 * @return ESPURE_OK on success, error code otherwise
 * 
 * @note This is the rendering-only path for [[...]] embedded phonemes
 */
espure_err_t espure_render(espure_handle_t handle, const char* phonemes,
                           char* output, size_t out_size, bool ipa);

/**
 * @brief Deinitialize and free resources
 * 
 * @param[in] handle G2P handle to destroy
 * 
 * @note After this call, handle is invalid and must not be used
 */
void espure_deinit(espure_handle_t handle);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Get supported language list
 * 
 * @param[out] count Number of languages returned
 * @return Array of language codes (NULL-terminated strings)
 * 
 * @note Returned array is static, do not free()
 */
const char** espure_get_languages(size_t* count);

/**
 * @brief Check if a language is available
 * 
 * @param[in] lang Language code
 * @return true if language is supported and data is embedded
 */
bool espure_lang_available(const char* lang);

/**
 * @brief Get error string for error code
 * 
 * @param[in] err Error code
 * @return Human-readable error string
 */
const char* espure_err_str(espure_err_t err);

/**
 * @brief Get version string
 * 
 * @return Version string (e.g., "1.0.0")
 */
const char* espure_version(void);

/* ========================================================================
 * Performance & Debugging
 * ======================================================================== */

/**
 * @brief Get performance statistics (if CONFIG_ESPURE_STATS_COLLECTION=y)
 * 
 * @param[in]  handle G2P handle
 * @param[out] stats  Statistics structure
 * 
 * @return ESPURE_OK on success
 * 
 * @note Returns ESPURE_ERR_NOT_FOUND if stats collection is disabled
 */
espure_err_t espure_get_stats(espure_handle_t handle, espure_stats_t* stats);

/**
 * @brief Reset performance statistics
 * 
 * @param[in] handle G2P handle
 */
void espure_reset_stats(espure_handle_t handle);

/**
 * @brief Get current heap usage for this handle
 * 
 * @param[in] handle G2P handle
 * @return Heap usage in bytes
 */
size_t espure_get_heap_usage(espure_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ESPURE_H */
