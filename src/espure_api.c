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

#ifdef CONFIG_ESPURE_USE_PSRAM
#include <esp_psram.h>
#endif

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
    [ESPURE_ERR_INVALID_LANG] = "Invalid language code",
    [ESPURE_ERR_INIT_FAILED] = "Initialization failed",
    [ESPURE_ERR_NOT_INITIALIZED] = "Handle not initialized",
};

const char* espure_err_str(espure_err_t err) {
    if (err < 0 || err >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[err];
}

/* Language availability table (compile-time configured) */
static const char* available_languages[] = {
#ifdef CONFIG_ESPURE_LANG_DE
    "de_DE",
    "de",
#endif
#ifdef CONFIG_ESPURE_LANG_EN
    "en",
#endif
    NULL
};

const char** espure_get_languages(size_t* count) {
    if (count) {
        size_t n = 0;
        while (available_languages[n] != NULL) n++;
        *count = n;
    }
    return available_languages;
}

bool espure_lang_available(const char* lang) {
    if (!lang) return false;
    
    for (size_t i = 0; available_languages[i] != NULL; i++) {
        if (strcmp(lang, available_languages[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Internal handle structure */
struct espure_handle_s {
    uint32_t magic;                  /* Magic number for validation */
    char lang[8];                    /* Language code */
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

espure_err_t espure_init(const char* lang, const espure_config_t* config,
                         espure_handle_t* out_handle) {
    if (!lang || !out_handle) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    if (!espure_lang_available(lang)) {
        ESP_LOGE(TAG, "Language '%s' not available (not compiled in)", lang);
        return ESPURE_ERR_INVALID_LANG;
    }
    
    /* Use default config if none provided */
    espure_config_t cfg = config ? *config : (espure_config_t)ESPURE_CONFIG_DEFAULT();
    
    ESP_LOGI(TAG, "Initializing espure for language '%s'", lang);
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
    strlcpy(handle->lang, lang, sizeof(handle->lang));
    handle->config = cfg;
    handle->heap_used = sizeof(struct espure_handle_s);
    
    espure_err_t err;
    
    /* Initialize phoneme table */
    err = espure_phoneme_table_init(lang, &cfg, &handle->phoneme_table);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize phoneme table: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_phoneme_table_size(handle->phoneme_table);
    
    /* Initialize dictionary */
    err = espure_dictionary_init(lang, &cfg, &handle->dictionary);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize dictionary: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_dictionary_size(handle->dictionary);
    
    /* Load embedded dictionary data */
    err = espure_dictionary_load_embedded(handle->dictionary, lang);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to load dictionary data: %s", espure_err_str(err));
        goto cleanup;
    }
    handle->heap_used += espure_dictionary_size(handle->dictionary);
    
    /* Initialize translator */
    err = espure_translator_init(lang, &cfg, handle->phoneme_table,
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
    
    ESP_LOGI(TAG, "Deinitializing espure handle for '%s'", handle->lang);
    
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

espure_err_t espure_render(espure_handle_t handle, const char* phonemes,
                           char* output, size_t out_size, bool ipa) {
    if (!is_valid_handle(handle)) {
        return ESPURE_ERR_NOT_INITIALIZED;
    }
    
    if (!phonemes || !output || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    espure_format_t format = ipa ? ESPURE_FORMAT_IPA : ESPURE_FORMAT_KIRSHENBAUM;
    
    return espure_render_phoneme_string(
        handle->phoneme_table,
        phonemes,
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
