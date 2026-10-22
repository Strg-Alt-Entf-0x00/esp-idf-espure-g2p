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

// Phoneme types (from phoneme.h)

/**
 * Phoneme table structure
 */
struct espure_phoneme_table_s {
    const char* locale;
    size_t num_phonemes;
    espure_phoneme_t* phonemes;  // Array in PSRAM
    size_t heap_used;
    const espure_config_t* config;
};

extern const char espure_phsource_phonemes_de_DE[];
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
extern const char espure_phsource_phonemes_en_GB[];
#endif

// Reserved phonemes (codes 1-33) - same in every language
static const struct {
    const char* mnemonic;
    uint8_t code;
} RESERVED_PHONEMES[] = {
    {"\x01", 1}, {"%", 2}, {"%%", 3}, {",", 4}, {",,", 5},
    {"'", 6}, {"''", 7}, {"=", 8}, {"_:", 9}, {"_", 10},
    {"_!", 11}, {":", 12}, {"@", 13}, {"@-", 14}, {"||", 15},
    {"*", 16}, {"1", 17}, {"#X1", 18}, {"?", 19}, {"-", 20},
    {"_^_", 21}, {"_X1", 22}, {"_|", 23}, {"_::", 24}, {"t#", 25},
    {"'!", 26}, {"_;_", 27}, {"#@", 28}, {"#a", 29}, {"#e", 30},
    {"#i", 31}, {"#o", 32}, {"#u", 33}
};

#define NUM_RESERVED_PHONEMES (sizeof(RESERVED_PHONEMES) / sizeof(RESERVED_PHONEMES[0]))

/**
 * Parse phoneme type from keyword
 */
/**
 * Initialize phoneme table with reserved phonemes
 */
static espure_err_t init_reserved_phonemes(espure_phoneme_table_t* table) {
    table->num_phonemes = NUM_RESERVED_PHONEMES;
    
    // Allocate phoneme array in PSRAM (Agent: 94% confidence)
    size_t alloc_size = NUM_RESERVED_PHONEMES * sizeof(espure_phoneme_t);
    table->phonemes = espure_calloc(NUM_RESERVED_PHONEMES, 
                                     sizeof(espure_phoneme_t),
                                     table->config);
    if (!table->phonemes) {
        ESP_LOGE(TAG, "Failed to allocate phoneme array (%zu bytes)", alloc_size);
        return ESPURE_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Allocated phoneme array: %zu bytes in %s", 
             alloc_size, table->config->use_psram ? "PSRAM" : "DRAM");
    
    // Initialize reserved phonemes
    for (size_t i = 0; i < NUM_RESERVED_PHONEMES; i++) {
        espure_phoneme_t* ph = &table->phonemes[i];
        
        // Copy mnemonic (max 7 chars + null terminator)
        strlcpy(ph->mnemonic, RESERVED_PHONEMES[i].mnemonic,
                sizeof(ph->mnemonic));
        ph->mnemonic[7] = '\0';
        
        ph->code = RESERVED_PHONEMES[i].code;
        ph->type = ESPURE_PHONEME_INVALID;  // Will be set when parsing phsource
        ph->ipa = NULL;
        ph->stress_type = 0;
        ph->flags = 0;
    }
    
    table->heap_used += alloc_size;
    
    ESP_LOGI(TAG, "Initialized %zu reserved phonemes", NUM_RESERVED_PHONEMES);
    return ESPURE_OK;
}

static size_t find_phoneme_index(const espure_phoneme_table_t* table,
                                 const char* mnemonic) {
    for (size_t i = 0; i < table->num_phonemes; i++) {
        if (strcmp(table->phonemes[i].mnemonic, mnemonic) == 0) {
            return i;
        }
    }
    return table->num_phonemes;
}

static void merge_phoneme(espure_phoneme_t* existing,
                          espure_phoneme_t* incoming) {
    if (existing->type == ESPURE_PHONEME_INVALID) {
        existing->type = incoming->type;
        existing->stress_type = incoming->stress_type;
        existing->flags = incoming->flags;
        existing->ipa = incoming->ipa;
        existing->program = incoming->program;
        incoming->ipa = NULL;
        incoming->program = NULL;
        return;
    }

    if (incoming->type != ESPURE_PHONEME_INVALID &&
        existing->type != incoming->type) {
        ESP_LOGW(TAG, "Conflicting duplicate phoneme '%s' (keeping type %u, ignoring %u)",
                 existing->mnemonic, existing->type, incoming->type);
    }
    existing->flags |= incoming->flags;
    if (!existing->ipa) {
        existing->ipa = incoming->ipa;
        incoming->ipa = NULL;
    }
    if (!existing->program) {
        existing->program = incoming->program;
        incoming->program = NULL;
    }
}

static void free_parsed_phonemes(espure_phoneme_t* phonemes, size_t count) {
    if (!phonemes) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(phonemes[i].ipa);
        free(phonemes[i].program);
    }
    espure_free(phonemes);
}

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
    
    // Initialize reserved phonemes
    espure_err_t err = init_reserved_phonemes(table);
    if (err != ESPURE_OK) {
        espure_free(table->phonemes);
        espure_free(table);
        return err;
    }
    
    const char* source = NULL;
    if (strcmp(lang, "de-DE") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
        source = espure_phsource_phonemes_de_DE;
#endif
    } else if (strcmp(lang, "en-GB") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
        source = espure_phsource_phonemes_en_GB;
#endif
    }
    if (!source) {
        ESP_LOGE(TAG, "No phoneme inventory compiled for locale '%s'", lang);
        espure_phoneme_table_deinit(table);
        return ESPURE_ERR_NOT_SUPPORTED;
    }

    // Parse language-selected phsource data and merge with reserved phonemes
    ESP_LOGI(TAG, "Parsing phsource/phonemes data...");
    
    const size_t max_parsed = 128;
    espure_phoneme_t* temp_phonemes = espure_calloc(
        max_parsed, sizeof(*temp_phonemes), config);
    if (!temp_phonemes) {
        espure_phoneme_table_deinit(table);
        return ESPURE_ERR_NO_MEM;
    }

    size_t num_parsed = 0;
    err = espure_parse_phsource(source,
                                 temp_phonemes,
                                 max_parsed,
                                 &num_parsed);
    
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to parse phoneme source: %d", err);
        free_parsed_phonemes(temp_phonemes, num_parsed);
        espure_phoneme_table_deinit(table);
        return err;
    }
    ESP_LOGI(TAG, "Parsed %zu phoneme declarations", num_parsed);

    size_t additional = 0;
    for (size_t i = 0; i < num_parsed; i++) {
        if (find_phoneme_index(table, temp_phonemes[i].mnemonic) == table->num_phonemes) {
            bool already_counted = false;
            for (size_t j = 0; j < i; j++) {
                if (strcmp(temp_phonemes[j].mnemonic,
                           temp_phonemes[i].mnemonic) == 0) {
                    already_counted = true;
                    break;
                }
            }
            if (!already_counted) {
                additional++;
            }
        }
    }

    if (additional > UINT8_MAX - table->num_phonemes) {
        ESP_LOGE(TAG, "Phoneme inventory exceeds the available 8-bit code space");
        free_parsed_phonemes(temp_phonemes, num_parsed);
        espure_phoneme_table_deinit(table);
        return ESPURE_ERR_INVALID_SIZE;
    }

    if (additional > 0) {
        size_t total = table->num_phonemes + additional;
        espure_phoneme_t* resized = espure_realloc(
            table->phonemes, total * sizeof(*resized), config);
        if (!resized) {
            free_parsed_phonemes(temp_phonemes, num_parsed);
            espure_phoneme_table_deinit(table);
            return ESPURE_ERR_NO_MEM;
        }
        table->phonemes = resized;
        memset(&table->phonemes[table->num_phonemes], 0,
               additional * sizeof(*table->phonemes));
    }

    for (size_t i = 0; i < num_parsed; i++) {
        size_t index = find_phoneme_index(table, temp_phonemes[i].mnemonic);
        if (index < table->num_phonemes) {
            if (index < NUM_RESERVED_PHONEMES) {
                ESP_LOGD(TAG, "Applying language metadata to reserved phoneme '%s'",
                         temp_phonemes[i].mnemonic);
            } else {
                ESP_LOGW(TAG, "Duplicate language phoneme declaration '%s'; merging metadata",
                         temp_phonemes[i].mnemonic);
            }
            merge_phoneme(&table->phonemes[index], &temp_phonemes[i]);
            continue;
        }

        espure_phoneme_t* destination = &table->phonemes[table->num_phonemes];
        *destination = temp_phonemes[i];
        destination->code = (uint8_t)(table->num_phonemes + 1);
        temp_phonemes[i].ipa = NULL;
        temp_phonemes[i].program = NULL;
        table->num_phonemes++;
    }

    table->heap_used += additional * sizeof(*table->phonemes);
    free_parsed_phonemes(temp_phonemes, num_parsed);

    ESP_LOGI(TAG, "Phoneme table initialized: %zu phonemes, %zu bytes", 
             table->num_phonemes, table->heap_used);
    
    *out = table;
    return ESPURE_OK;
}

void espure_phoneme_table_deinit(espure_phoneme_table_t* table) {
    if (!table) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing phoneme table");
    
    // Free phoneme array (in PSRAM)
    if (table->phonemes) {
        // Free IPA strings
        for (size_t i = 0; i < table->num_phonemes; i++) {
            if (table->phonemes[i].ipa) {
                espure_free(table->phonemes[i].ipa);
            }
            free(table->phonemes[i].program);
        }
        espure_free(table->phonemes);
    }
    
    // Free table structure
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

/**
 * @brief Look up phoneme by mnemonic
 * 
 * Linear search through phoneme table (~60 phonemes, fast enough).
 * 
 * @param table Phoneme table
 * @param mnemonic Phoneme mnemonic (e.g. "r", "r/", "@")
 * @return Phoneme pointer or NULL if not found
 */
const espure_phoneme_t* espure_phoneme_lookup(
    const espure_phoneme_table_t* table,
    const char* mnemonic
) {
    if (!table || !mnemonic) {
        return NULL;
    }
    
    // Linear search (table is small, ~60 phonemes)
    for (size_t i = 0; i < table->num_phonemes; i++) {
        if (strcmp(table->phonemes[i].mnemonic, mnemonic) == 0) {
            return &table->phonemes[i];
        }
    }
    
    ESP_LOGD(TAG, "Phoneme not found: '%s'", mnemonic);
    return NULL;
}
