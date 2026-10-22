/**
 * @file espure.h
 * @brief ESPure - Native Embedded G2P Engine for ESP32-P4 / RISC-V
 * 
 * High-performance, clean-room G2P (grapheme-to-phoneme) engine for embedded systems,
 * optimized for ESP32-P4 Rev 1.3 with hardware acceleration support.
 * 
 * @license Unlicensed / Permissive Open Source (100% Native Architecture)
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
    ESPURE_ERR_INVALID_LANG,    /**< Invalid locale identifier */
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
    bool use_psram;              /**< Store locale data in PSRAM */
    bool enable_hwaccel;         /**< Enable hardware acceleration (PIE) */
    bool force_compat;           /**< Force strict legacy compatibility mode */
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
 * @brief Text rendering format for the current string-output APIs
 *
 * These formats render phonemization results as strings; consumers must not
 * treat either rendering as the final structured machine-readable contract.
 * The project-level pronunciation target and structured API direction are
 * documented in the repository README.
 */
typedef enum {
    ESPURE_FORMAT_IPA,          /**< Unicode IPA with stress marks */
    ESPURE_FORMAT_KIRSHENBAUM,  /**< ASCII Kirshenbaum representation */
} espure_format_t;

/**
 * @brief Phonemization options
 */
typedef struct {
    espure_format_t format;      /**< Output format (IPA/Kirshenbaum) */
    const char* separator;       /**< Phoneme separator (NULL = none) */
    const char* tie;             /**< Tie character for multi-char phonemes */
    int8_t tonic_stress;         /**< Force tonic stress level (-1 = auto) */
    bool rules_only;             /**< Bypass dictionary, evaluate morphology + rules + phonology */
} espure_options_t;

/**
 * @brief Default phonemization options
 */
#define ESPURE_OPTIONS_DEFAULT() { \
    .format = ESPURE_FORMAT_IPA, \
    .separator = NULL, \
    .tie = NULL, \
    .tonic_stress = -1, \
    .rules_only = false, \
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
 * Stage 6: Structured Phoneme Stream Types
 * ======================================================================== */

/**
 * @brief Stress level of a discrete phonetic segment.
 */
typedef enum {
    ESPURE_STRESS_NONE = 0,
    ESPURE_STRESS_PRIMARY = 1,
    ESPURE_STRESS_SECONDARY = 2,
} espure_stress_t;

/**
 * @brief Structural boundary following a discrete segment.
 */
typedef enum {
    ESPURE_BOUNDARY_NONE = 0,
    ESPURE_BOUNDARY_SYLLABLE = 1,   /**< Syllable boundary (.) */
    ESPURE_BOUNDARY_MORPHEME = 2,   /**< Morpheme boundary (-) */
    ESPURE_BOUNDARY_WORD = 3,       /**< Word boundary (space) */
    ESPURE_BOUNDARY_PAUSE = 4,      /**< Punctuation / pause */
} espure_boundary_t;

/**
 * @brief Discrete phonetic segment with linguistic annotations.
 */
typedef struct {
    uint8_t segment_id;             /**< Stable, language-independent segment enum */
    uint8_t stress : 2;             /**< Stress marker (espure_stress_t) */
    uint8_t boundary : 3;           /**< Trailing boundary (espure_boundary_t) */
    uint8_t length_modifier : 2;    /**< 0=Normal, 1=Half-long, 2=Long (:) */
    uint8_t reserved : 1;
} espure_segment_t;

/**
 * @brief Complete structured pronunciation stream.
 */
typedef struct {
    uint16_t version;               /**< API version (0x0100 for v1.0) */
    uint16_t segment_count;         /**< Number of segments in stream */
    const espure_segment_t *segments;/**< Pointer to continuous segment array */
} espure_stream_t;

/**
 * @brief Discrete dictionary entry (word -> raw phonemes).
 */
typedef struct {
    const char* word;
    const char* phonemes;
    uint32_t flags;
} espure_dict_entry_t;

/* ========================================================================
 * Core API
 * ======================================================================== */

/**
 * @brief Initialize a G2P translator for a specific locale
 * 
 * @param[in]  locale Exact BCP 47 locale identifier (e.g., "de-DE" or "en-GB"); aliases are not accepted
 * @param[in]  config Configuration (NULL = default)
 * @param[out] handle Output handle
 * 
 * @return ESPURE_OK on success, error code otherwise
 * 
 * @note Thread-safe: Multiple handles can coexist
 * @note Locale data loaded from embedded flash/PSRAM
 * 
 * Example:
 * @code
 * espure_handle_t g2p;
 * espure_config_t config = ESPURE_CONFIG_DEFAULT();
 * if (espure_init("en-GB", &config, &g2p) == ESPURE_OK) {
 *     // Use g2p...
 *     espure_deinit(g2p);
 * }
 * @endcode
 */
espure_err_t espure_init(const char* locale, const espure_config_t* config,
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
 * @brief Convert text to a structured pronunciation stream (Stage 6)
 * 
 * Populates caller-provided segment buffer with numeric phonetic segment IDs
 * and prosodic annotations. Fast path with zero heap allocations, designed
 * for embedded TTS acoustic models and formant synthesizers.
 * 
 * @param[in]  handle       G2P handle from espure_init()
 * @param[in]  text         Input text (UTF-8)
 * @param[out] seg_buf      Buffer to receive discrete phonetic segments
 * @param[in]  max_segments Capacity of seg_buf
 * @param[out] out_stream   Stream header describing populated segments
 * 
 * @return ESPURE_OK on success, error code otherwise
 */
espure_err_t espure_phonemize_stream(espure_handle_t handle,
                                     const char* text,
                                     espure_segment_t* seg_buf,
                                     size_t max_segments,
                                     espure_stream_t* out_stream);

/**
 * @brief Render raw phoneme string (phonetic mnemonics) to IPA/Kirshenbaum
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
 * @brief Split a compound word into morphemes using the morphological trie
 * 
 * @param[in]  lang        BCP 47 language code (e.g., "de-DE")
 * @param[in]  word        Input word (UTF-8)
 * @param[out] out_buffer  Buffer to receive hyphen-separated morphemes
 * @param[in]  max_len     Capacity of out_buffer
 * 
 * @return true if decomposed into multiple morphemes, false if monomorphemic
 */
bool espure_morph_split(const char* lang, const char* word,
                        char* out_buffer, size_t max_len);

/**
 * @brief Deinitialize and free resources
 * 
 * @param[in] handle G2P handle to destroy
 * 
 * @note After this call, handle is invalid and must not be used
 */
void espure_deinit(espure_handle_t handle);

/**
 * @brief Access raw dictionary entries for analysis and verification
 * 
 * @param[in]  handle       G2P handle
 * @param[out] out_entries  Pointer to array of dictionary entries
 * @param[out] out_count    Number of entries
 * @return ESPURE_OK on success
 */
espure_err_t espure_get_dictionary(espure_handle_t handle,
                                   const espure_dict_entry_t** out_entries,
                                   size_t* out_count);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Get supported locale list
 * 
 * @param[out] count Number of locales returned
 * @return Array of BCP 47 locale identifiers (NULL-terminated strings)
 * 
 * @note Returned array is static, do not free()
 */
const char** espure_get_locales(size_t* count);

/**
 * @brief Check if a locale is available
 * 
 * @param[in] locale Exact BCP 47 locale identifier
 * @return true if the locale is supported and data is embedded
 */
bool espure_locale_available(const char* locale);

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
