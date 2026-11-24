/**
 * @file espure_phoneme.c
 * @brief Phoneme table management
 * 
 * Implements phoneme table management and runtime symbol mapping.
 */

#include "espure_internal.h"
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>

static const char* TAG = "espure_phoneme";

/**
 * Phoneme table structure
 */
struct espure_phoneme_table_s {
    const char* locale;
    size_t num_phonemes;
    espure_phoneme_t* phonemes;  // Array in static Flash ROM
    size_t heap_used;
    const espure_config_t* config;
    bool is_static;
};

#ifdef CONFIG_ESPURE_LOCALE_DE_DE
extern const espure_phoneme_t ESPURE_PHONEMES_DE_DE[];
extern const size_t ESPURE_NUM_PHONEMES_DE_DE;
#endif

#ifdef CONFIG_ESPURE_LOCALE_EN_US
extern const espure_phoneme_t ESPURE_PHONEMES_EN_US[];
extern const size_t ESPURE_NUM_PHONEMES_EN_US;
#endif

espure_err_t espure_phoneme_table_init(const char* lang,
                                       const espure_config_t* config,
                                       espure_phoneme_table_t** out) {
    if (!lang || !config || !out) {
        return ESPURE_ERR_INVALID_ARG;
    }
    *out = NULL;
    
    ESP_LOGI(TAG, "Initializing phoneme table for locale '%s'", lang);
    
    // Allocate table structure in DRAM (small, accessed frequently)
    espure_phoneme_table_t* table = espure_calloc(1, 
        sizeof(espure_phoneme_table_t), config);
    if (!table) {
        ESP_LOGE(TAG, "Failed to allocate phoneme table structure");
        return ESPURE_ERR_NO_MEM;
    }
    
    table->locale = lang;
    table->config = config;
    table->heap_used = sizeof(espure_phoneme_table_t);
    
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    if (strcmp(lang, "de-DE") == 0) {
        table->phonemes = (espure_phoneme_t*)ESPURE_PHONEMES_DE_DE;
        table->num_phonemes = ESPURE_NUM_PHONEMES_DE_DE;
        table->is_static = true;
        *out = table;
        ESP_LOGI(TAG, "Phoneme table loaded from static Flash ROM: %zu phonemes (0 heap used)", table->num_phonemes);
        return ESPURE_OK;
    }
#endif

#ifdef CONFIG_ESPURE_LOCALE_EN_US
    if (strcmp(lang, "en-US") == 0) {
        table->phonemes = (espure_phoneme_t*)ESPURE_PHONEMES_EN_US;
        table->num_phonemes = ESPURE_NUM_PHONEMES_EN_US;
        table->is_static = true;
        *out = table;
        ESP_LOGI(TAG, "Phoneme table loaded from static Flash ROM: %zu phonemes (0 heap used)", table->num_phonemes);
        return ESPURE_OK;
    }
#endif

    ESP_LOGE(TAG, "No phoneme inventory compiled for locale '%s'", lang);
    espure_free(table);
    return ESPURE_ERR_NOT_SUPPORTED;
}

void espure_phoneme_table_deinit(espure_phoneme_table_t* table) {
    if (!table) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing phoneme table");
    
    if (table->phonemes && !table->is_static) {
        for (size_t i = 0; i < table->num_phonemes; i++) {
            free(table->phonemes[i].ipa);
            free(table->phonemes[i].program);
        }
        espure_free(table->phonemes);
    }
    
    espure_free(table);
}

size_t espure_phoneme_table_size(const espure_phoneme_table_t* table) {
    return table ? table->heap_used : 0;
}

size_t espure_phoneme_table_count(const espure_phoneme_table_t* table) {
    return table ? table->num_phonemes : 0;
}

const char* espure_phoneme_table_locale(const espure_phoneme_table_t* table) {
    return table ? table->locale : NULL;
}

const espure_phoneme_t* espure_phoneme_lookup(
    const espure_phoneme_table_t* table,
    const char* mnemonic
) {
    if (!table || !mnemonic) {
        return NULL;
    }
    
    for (size_t i = 0; i < table->num_phonemes; i++) {
        if (strcmp(table->phonemes[i].mnemonic, mnemonic) == 0) {
            return &table->phonemes[i];
        }
    }
    
    return NULL;
}
