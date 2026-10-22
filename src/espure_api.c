/**
 * @file espure_api.c
 * @brief Main API implementation for espure ESP32-P4
 * 
 * Core G2P translator API optimized for ESP32-P4 Rev 1.3
 */

#include "espure.h"
#include "espure_internal.h"

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

static const char* TAG = "espure";

/* Version string */
const char* espure_version(void) {
    return ESPURE_VERSION;
}

/* Error strings */
static const char* error_strings[] = {
    [ESPURE_OK] = "Success",
    [ESPURE_ERR_INVALID_ARG] = "Invalid argument",
    [ESPURE_ERR_NO_MEM] = "Out of memory",
    [ESPURE_ERR_NOT_FOUND] = "Resource not found",
    [ESPURE_ERR_BUFFER_TOO_SMALL] = "Buffer too small",
    [ESPURE_ERR_INVALID_LANG] = "Invalid locale identifier",
    [ESPURE_ERR_INIT_FAILED] = "Initialization failed",
    [ESPURE_ERR_NOT_INITIALIZED] = "Handle not initialized",
    [ESPURE_ERR_INVALID_SIZE] = "Invalid size",
    [ESPURE_ERR_NOT_SUPPORTED] = "Operation not supported",
};

const char* espure_err_str(espure_err_t err) {
    if (err < 0 || err >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[err];
}

/* Locale availability table (compile-time configured) */
static const char* available_locales[] = {
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    "de-DE",
#endif
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
    "en-GB",
#endif
    NULL
};

const char** espure_get_locales(size_t* count) {
    if (count) {
        size_t n = 0;
        while (available_locales[n] != NULL) n++;
        *count = n;
    }
    return available_locales;
}

static const char* find_available_locale(const char* locale) {
    if (!locale) return NULL;

    for (size_t i = 0; available_locales[i] != NULL; i++) {
        if (strcmp(locale, available_locales[i]) == 0) {
            return available_locales[i];
        }
    }
    return NULL;
}

bool espure_locale_available(const char* locale) {
    return find_available_locale(locale) != NULL;
}

/* Internal handle structure */
struct espure_handle_s {
    uint32_t magic;                  /* Magic number for validation */
    const char* locale;              /* Canonical BCP 47 locale identifier */
    espure_config_t config;          /* Configuration */
    
    /* Core components */
    espure_phoneme_table_t* phoneme_table;
    espure_dictionary_t* dictionary;
    espure_translator_t* translator;
    
    /* Memory tracking */
    size_t heap_used;
    
#ifdef CONFIG_ESPURE_STATS_COLLECTION
    espure_stats_t stats;
#endif
};

#define ESPURE_MAGIC 0xE5F7A12C

/* Validate handle */
static inline bool is_valid_handle(espure_handle_t handle) {
    return handle != NULL && handle->magic == ESPURE_MAGIC;
}

/* ========================================================================
 * Initialization & Cleanup
 * ======================================================================== */

espure_err_t espure_init(const char* locale, const espure_config_t* config,
                         espure_handle_t* out_handle) {
    if (!locale || !out_handle) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    const char* canonical_locale = find_available_locale(locale);
    if (!canonical_locale) {
        ESP_LOGE(TAG, "Locale '%s' not available (not compiled in)", locale);
        return ESPURE_ERR_INVALID_LANG;
    }
    locale = canonical_locale;
    
    /* Use default config if none provided */
    espure_config_t cfg = config ? *config : (espure_config_t)ESPURE_CONFIG_DEFAULT();
    
    ESP_LOGI(TAG, "Initializing espure for locale '%s'", locale);
    ESP_LOGI(TAG, "  PSRAM: %s", cfg.use_psram ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  HW Accel: %s", cfg.enable_hwaccel ? "enabled" : "disabled");
    
    /* Allocate handle (always in DRAM for fast access) */
    espure_handle_t handle = heap_caps_calloc(1, sizeof(struct espure_handle_s),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate handle");
        return ESPURE_ERR_NO_MEM;
    }
    
    handle->magic = ESPURE_MAGIC;
    handle->locale = locale;
    handle->config = cfg;
    handle->heap_used = sizeof(struct espure_handle_s);
    
    espure_err_t err;
    
    /* Initialize phoneme table */
    err = espure_phoneme_table_init(locale, &cfg, &handle->phoneme_table);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize phoneme table: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_phoneme_table_size(handle->phoneme_table);
    
    /* Initialize dictionary */
    err = espure_dictionary_init(locale, &cfg, &handle->dictionary);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize dictionary: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_dictionary_size(handle->dictionary);
    
    /* Load embedded dictionary data */
    err = espure_dictionary_load_embedded(handle->dictionary, locale);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to load dictionary data: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_dictionary_size(handle->dictionary);
    
    /* Initialize translator */
    err = espure_translator_init(locale, &cfg, handle->phoneme_table,
                                 handle->dictionary, &handle->translator);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize translator: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_translator_size(handle->translator);
    
    ESP_LOGI(TAG, "Initialization complete. Heap used: %zu bytes", handle->heap_used);
    
    *out_handle = handle;
    return ESPURE_OK;
    
cleanup:
    if (handle->translator) espure_translator_deinit(handle->translator);
    if (handle->dictionary) espure_dictionary_deinit(handle->dictionary);
    if (handle->phoneme_table) espure_phoneme_table_deinit(handle->phoneme_table);
    free(handle);
    return err;
}

void espure_deinit(espure_handle_t handle) {
    if (!is_valid_handle(handle)) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing espure handle for '%s'", handle->locale);
    
    if (handle->translator) espure_translator_deinit(handle->translator);
    if (handle->dictionary) espure_dictionary_deinit(handle->dictionary);
    if (handle->phoneme_table) espure_phoneme_table_deinit(handle->phoneme_table);
    
    handle->magic = 0;  /* Invalidate */
    free(handle);
}

/* ========================================================================
 * Phonemization
 * ======================================================================== */

espure_err_t espure_phonemize(espure_handle_t handle, const char* text,
                              char* output, size_t out_size, bool ipa) {
    espure_options_t opts = ESPURE_OPTIONS_DEFAULT();
    opts.format = ipa ? ESPURE_FORMAT_IPA : ESPURE_FORMAT_KIRSHENBAUM;
    return espure_phonemize_ex(handle, text, output, out_size, &opts);
}

espure_err_t espure_phonemize_ex(espure_handle_t handle, const char* text,
                                 char* output, size_t out_size,
                                 const espure_options_t* options) {
    if (!is_valid_handle(handle)) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    
    if (!text || !output || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    if (strlen(text) > handle->config.max_word_length) {
        ESP_LOGW(TAG, "Input text exceeds max length (%u bytes)", 
                 handle->config.max_word_length);
        return ESPURE_ERR_INVALID_ARG;
    }
    
    espure_options_t opts = options ? *options : (espure_options_t)ESPURE_OPTIONS_DEFAULT();
    
#ifdef CONFIG_ESPURE_STATS_COLLECTION
    uint64_t start = esp_timer_get_time();
    handle->stats.phonemize_calls++;
    handle->stats.total_chars += strlen(text);
#endif
    
    /* Main translation pipeline:
     * 1. Word split & normalization
     * 2. Dictionary lookup / rule matching
     * 3. Stress assignment
     * 4. Phoneme programs
     * 5. Render to IPA/Kirshenbaum
     */
    espure_err_t err = espure_translator_phonemize(
        handle->translator,
        text,
        output,
        out_size,
        &opts
    );
    
#ifdef CONFIG_ESPURE_STATS_COLLECTION
    uint64_t elapsed = esp_timer_get_time() - start;
    handle->stats.total_time_us += elapsed;
    if (handle->heap_used > handle->stats.peak_heap_used) {
        handle->stats.peak_heap_used = handle->heap_used;
    }
#endif
    
    if (err == ESPURE_OK) {
        ESP_LOGD(TAG, "Phonemized: '%s' -> '%s'", text, output);
    } else {
        ESP_LOGE(TAG, "Phonemization failed: %s", espure_err_str(err));
    }
    
    return err;
}

espure_err_t espure_phonemize_stream(espure_handle_t handle,
                                     const char* text,
                                     espure_segment_t* seg_buf,
                                     size_t max_segments,
                                     espure_stream_t* out_stream) {
    if (!is_valid_handle(handle)) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    if (!text || !seg_buf || max_segments == 0 || !out_stream) {
        return ESPURE_ERR_INVALID_ARG;
    }

    out_stream->version = 0x0100;
    out_stream->segment_count = 0;
    out_stream->segments = seg_buf;

    char ph_buf[512];
    espure_options_t opts = ESPURE_OPTIONS_DEFAULT();
    opts.format = ESPURE_FORMAT_KIRSHENBAUM;
    opts.separator = "/";
    espure_err_t err = espure_phonemize_ex(handle, text, ph_buf, sizeof(ph_buf), &opts);
    if (err != ESPURE_OK) {
        return err;
    }

    const char* p = ph_buf;
    size_t count = 0;
    bool word_has_stress = false;
    espure_stress_t pending_stress = ESPURE_STRESS_NONE;

    while (*p && count < max_segments) {
        while (*p == ' ' || *p == '/') {
            if (*p == ' ') {
                if (count > 0) {
                    seg_buf[count - 1].boundary = ESPURE_BOUNDARY_WORD;
                }
                word_has_stress = false;
                pending_stress = ESPURE_STRESS_NONE;
            }
            p++;
        }
        if (!*p) break;

        /* Stress markers */
        if (*p == '\'') {
            pending_stress = ESPURE_STRESS_PRIMARY;
            p++;
            continue;
        }
        if (*p == ',') {
            pending_stress = ESPURE_STRESS_SECONDARY;
            p++;
            continue;
        }

        /* Extract mnemonic token */
        char m_buf[16];
        size_t m_len = 0;
        while (*p && *p != ' ' && *p != '/' && *p != '\'' && *p != ',' && *p != '.' && m_len < sizeof(m_buf) - 1) {
            m_buf[m_len++] = *p++;
        }
        m_buf[m_len] = '\0';

        if (m_len == 0) continue;

        /* Check for length modifier suffix ':' */
        uint8_t length_mod = 0;
        if (m_len > 1 && m_buf[m_len - 1] == ':') {
            length_mod = 2; // Long
        }

        /* Lookup phoneme in table */
        const espure_phoneme_t* ph = espure_phoneme_lookup(handle->phoneme_table, m_buf);
        uint8_t seg_id = ph ? ph->code : 0;
        bool is_vowel = (ph && ph->type == ESPURE_PHONEME_VOWEL);

        uint8_t seg_stress = ESPURE_STRESS_NONE;
        if (is_vowel) {
            if (pending_stress != ESPURE_STRESS_NONE) {
                seg_stress = pending_stress;
                pending_stress = ESPURE_STRESS_NONE;
                word_has_stress = true;
            } else if (!word_has_stress) {
                /* Root-initial default lexical stress */
                seg_stress = ESPURE_STRESS_PRIMARY;
                word_has_stress = true;
            }
        }

        seg_buf[count].segment_id = seg_id;
        seg_buf[count].stress = seg_stress;
        seg_buf[count].boundary = ESPURE_BOUNDARY_NONE;
        seg_buf[count].length_modifier = length_mod;
        seg_buf[count].reserved = 0;

        count++;
    }

    if (count > 0) {
        seg_buf[count - 1].boundary = ESPURE_BOUNDARY_PAUSE;
    }

    out_stream->segment_count = (uint16_t)count;
    return ESPURE_OK;
}

espure_err_t espure_render(espure_handle_t handle, const char* phonemes,
                           char* output, size_t out_size, bool ipa) {
    if (!is_valid_handle(handle)) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    
    if (!phonemes || !output || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }

    char spaced[512];
    const char* to_render = phonemes;
    if (strchr(phonemes, ' ') == NULL) {
        const espure_phoneme_t* parsed_ph[128];
        size_t num_parsed = 0;
        if (espure_parse_phoneme_string(handle->phoneme_table, phonemes, parsed_ph, 128, &num_parsed) == ESPURE_OK && num_parsed > 0) {
            size_t pos = 0;
            for (size_t p = 0; p < num_parsed; p++) {
                if (p > 0 && pos < sizeof(spaced) - 1) spaced[pos++] = ' ';
                size_t mlen = strlen(parsed_ph[p]->mnemonic);
                if (pos + mlen < sizeof(spaced)) {
                    memcpy(spaced + pos, parsed_ph[p]->mnemonic, mlen);
                    pos += mlen;
                }
            }
            spaced[pos] = '\0';
            to_render = spaced;
        }
    }
    
    espure_format_t format = ipa ? ESPURE_FORMAT_IPA : ESPURE_FORMAT_KIRSHENBAUM;
    
    return espure_render_phoneme_string(
        handle->phoneme_table,
        to_render,
        output,
        out_size,
        format,
        NULL,  /* separator */
        NULL   /* tie */
    );
}

/* ========================================================================
 * Performance & Debugging
 * ======================================================================== */

#ifdef CONFIG_ESPURE_STATS_COLLECTION
espure_err_t espure_get_stats(espure_handle_t handle, espure_stats_t* stats) {
    if (!is_valid_handle(handle)) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    
    if (!stats) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    *stats = handle->stats;
    return ESPURE_OK;
}

void espure_reset_stats(espure_handle_t handle) {
    if (!is_valid_handle(handle)) {
        return;
    }
    
    memset(&handle->stats, 0, sizeof(espure_stats_t));
}
#else
espure_err_t espure_get_stats(espure_handle_t handle, espure_stats_t* stats) {
    (void)handle;
    (void)stats;
    return ESPURE_ERR_NOT_FOUND;
}

void espure_reset_stats(espure_handle_t handle) {
    (void)handle;
}
#endif

size_t espure_get_heap_usage(espure_handle_t handle) {
    if (!is_valid_handle(handle)) {
        return 0;
    }
    
    return handle->heap_used;
}

espure_err_t espure_get_dictionary(espure_handle_t handle,
                                   const espure_dict_entry_t** out_entries,
                                   size_t* out_count) {
    if (!is_valid_handle(handle) || !out_entries || !out_count) {
        return ESPURE_ERR_INVALID_ARG;
    }
    if (!handle->dictionary) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    *out_entries = handle->dictionary->entries;
    *out_count = handle->dictionary->count;
    return ESPURE_OK;
}
