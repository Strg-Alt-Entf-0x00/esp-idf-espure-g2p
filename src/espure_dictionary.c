/**
 * @file espure_dictionary.c
 * @brief Dictionary lookup using binary search
 * 
 * Native dictionary lookup system for ESP32-P4 embedded:
 * - Binary search instead of hash-table (736 KB -> 184 KB memory)
 * - Sorted dictionary arrays in FLASH
 * - Multi-language support via Kconfig
 * 
 * Performance: O(log n) ~18 µs (deterministic, zero heap allocation)
 */

#include "espure_internal.h"
#include "espure_phoneme_program.h"
#include "espure_morph.h"
#include "espure_phonology.h"
#include "espure_normalize.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "espure_dict";

/* ========================================================================
 * Dictionary Initialization (Binary Array System)
 * ======================================================================== */

espure_err_t espure_dictionary_init(const char* lang,
                                    const espure_config_t* config,
                                    espure_dictionary_t** out) {
    if (!lang || !config || !out) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing dictionary for '%s' (binary array)", lang);
    
    // Allocate dictionary structure
    espure_dictionary_t* dict = espure_calloc(1, sizeof(espure_dictionary_t), config);
    if (!dict) {
        ESP_LOGE(TAG, "Failed to allocate dictionary structure");
        return ESPURE_ERR_NO_MEM;
    }
    
    dict->config = config;
    dict->entries = NULL;
    dict->count = 0;
    dict->binary_data = NULL;
    
    ESP_LOGI(TAG, "Dictionary initialized (binary search mode)");
    
    *out = dict;
    return ESPURE_OK;
}

void espure_dictionary_deinit(espure_dictionary_t* dict) {
    if (!dict) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing dictionary (%zu entries)", dict->count);
    
    // IMPORTANT: Don't free entries!
    // For embedded dictionaries: entries point to const FLASH data
    // For binary loader: entries are allocated but we don't track ownership properly yet
    // TODO: Add a flag to track if entries were malloc'd
    // For now: NEVER free entries (small memory leak on binary deinit, but safe)
    
    // Only free if allocated from binary loader
    if (dict->binary_data) {
        espure_free(dict->entries);
    }
    
    espure_free(dict);
}

size_t espure_dictionary_size(const espure_dictionary_t* dict) {
    if (!dict) {
        return 0;
    }
    return sizeof(espure_dictionary_t) + 
           (dict->count * sizeof(espure_dict_entry_t));
}

/* ========================================================================
 * Dictionary Lookup (Binary Search)
 * ======================================================================== */

static bool lowercase_german_word(const char* word, char* output, size_t output_size) {
    size_t input_size = strlen(word);
    if (input_size >= output_size) {
        return false;
    }

    size_t out = 0;
    for (size_t in = 0; in < input_size; in++) {
        unsigned char c = (unsigned char)word[in];
        if (c >= 'A' && c <= 'Z') {
            output[out++] = (char)(c + ('a' - 'A'));
        } else if (c == 0xC3 && in + 1 < input_size) {
            /* Lowercase uppercase Latin-1 letters encoded as two-byte UTF-8. */
            unsigned char next = (unsigned char)word[in + 1];
            if ((next >= 0x80 && next <= 0x96) ||
                (next >= 0x98 && next <= 0x9E)) {
                output[out++] = (char)c;
                output[out++] = (char)(next + 0x20);
                in++;
            } else {
                output[out++] = (char)c;
            }
        } else if (c == 0xE1 && in + 2 < input_size &&
                   (unsigned char)word[in + 1] == 0xBA &&
                   (unsigned char)word[in + 2] == 0x9E) {
            output[out++] = (char)0xC3;
            output[out++] = (char)0x9F;
            in += 2;
        } else {
            output[out++] = (char)c;
        }
    }

    output[out] = '\0';
    return true;
}

#ifndef CONFIG_ESPURE_DICT_HUFFMAN_TRIE
static espure_err_t lookup_dictionary_entries(espure_dictionary_t* dict,
                                              const char* word,
                                              const char* lower_word,
                                              char* phonemes_out,
                                              size_t out_size,
                                              uint32_t* flags_out) {
    size_t left = 0;
    size_t right = dict->count;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        char entry_lower[128];
        if (!lowercase_german_word(dict->entries[mid].word,
                                   entry_lower, sizeof(entry_lower))) {
            return ESPURE_ERR_INVALID_SIZE;
        }
        int cmp = strcmp(entry_lower, lower_word);

        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    const espure_dict_entry_t* matched_entry = NULL;
    for (size_t i = left; i < dict->count; i++) {
        char entry_lower[128];
        if (!lowercase_german_word(dict->entries[i].word,
                                   entry_lower, sizeof(entry_lower))) {
            return ESPURE_ERR_INVALID_SIZE;
        }
        if (strcmp(entry_lower, lower_word) != 0) {
            break;
        }
        if (strcmp(dict->entries[i].word, word) == 0) {
            matched_entry = &dict->entries[i];
            break;
        }
        if (strcmp(dict->entries[i].word, lower_word) == 0) {
            matched_entry = &dict->entries[i];
        }
    }

    if (matched_entry) {
        size_t phoneme_size = strlen(matched_entry->phonemes);
        if (phoneme_size >= out_size) {
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
        memcpy(phonemes_out, matched_entry->phonemes, phoneme_size + 1);
        if (flags_out) {
            *flags_out = matched_entry->flags;
        }
        ESPURE_DEBUG("Dictionary hit: %s -> %s", word, phonemes_out);
        return ESPURE_OK;
    }

    return ESPURE_ERR_NOT_FOUND;
}
#endif

/**
 * @brief Lookup a word by exact spelling, then its lowercase form
 */
espure_err_t espure_dictionary_lookup(espure_dictionary_t* dict,
                                     const char* word,
                                     char* phonemes_out,
                                     size_t out_size,
                                     uint32_t* flags_out) {
    if (!dict || !word || !phonemes_out || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }

    char lower_word[128];
    if (!lowercase_german_word(word, lower_word, sizeof(lower_word))) {
        return ESPURE_ERR_INVALID_SIZE;
    }

#ifdef CONFIG_ESPURE_DICT_HUFFMAN_TRIE
    extern espure_err_t espure_dict_compressed_lookup(const char* word, char* out_phonemes, size_t out_size);
    if (flags_out) {
        *flags_out = 0;
    }

    espure_err_t err = espure_dict_compressed_lookup(word, phonemes_out, out_size);
    if (err != ESPURE_ERR_NOT_FOUND || strcmp(word, lower_word) == 0) {
        return err;
    }
    return espure_dict_compressed_lookup(lower_word, phonemes_out, out_size);
#else
    if (!dict->entries || dict->count == 0) {
        return ESPURE_ERR_NOT_FOUND;
    }

    espure_err_t err = lookup_dictionary_entries(
        dict, word, lower_word, phonemes_out, out_size, flags_out);
    return err;
#endif
}

/* ========================================================================
 * Dictionary Loading (Binary Array System)
 * ======================================================================== */

/**
 * @brief Load embedded dictionary from generated C arrays
 * 
 * This loads the dictionaries generated by dict_to_binary.py tool.
 * The arrays are already sorted at build-time for binary search.
 * 
 * Multi-language support: User selects languages via Kconfig.
 * CMake conditionally compiles only selected languages.
 */
espure_err_t espure_dictionary_load_embedded(espure_dictionary_t* dict,
                                            const char* lang) {
    if (!dict || !lang) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading embedded dictionary for locale '%s'", lang);
    
    // Load locale-specific dictionary
    // These are generated by tools/dict_to_binary.py and 
    // conditionally compiled via CMakeLists.txt based on Kconfig
    
    if (strcmp(lang, "en-GB") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
        extern const espure_dict_entry_t DICT_EN_GB[];
        extern const size_t DICT_EN_GB_SIZE;
        
        dict->entries = (espure_dict_entry_t*)DICT_EN_GB;
        dict->count = DICT_EN_GB_SIZE;
        
        ESP_LOGI(TAG, "[OK] Loaded en-GB pilot dictionary: %zu entries", dict->count);
        ESP_LOGI(TAG, "   Memory: %zu KB (pointer array only)", 
                 (dict->count * sizeof(espure_dict_entry_t)) / 1024);
#else
        ESP_LOGE(TAG, "English pilot not compiled (enable CONFIG_ESPURE_LOCALE_EN_GB)");
        return ESPURE_ERR_NOT_FOUND;
#endif
    } else if (strcmp(lang, "de-DE") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    #ifdef CONFIG_ESPURE_DICT_HUFFMAN_TRIE
        // Huffman compressed dictionary mode
        // Dictionary is accessed via espure_dict_compressed_lookup() function
        // No need to load entry array into memory
        dict->entries = NULL;
        dict->count = 0;  // Unknown in compressed mode (block-based access)
        
        ESP_LOGI(TAG, "[OK] Loaded DE dictionary: Huffman compressed (764 KB)");
    #else
        // Binary array mode (fast binary search)
        extern const espure_dict_entry_t DICT_DE_DE[];
        extern const size_t DICT_DE_DE_SIZE;
        
        dict->entries = (espure_dict_entry_t*)DICT_DE_DE;
        dict->count = DICT_DE_DE_SIZE;
        
        ESP_LOGI(TAG, "[OK] Loaded DE dictionary: %zu entries", dict->count);
    #endif
#else
        ESP_LOGE(TAG, "German dictionary not compiled (enable CONFIG_ESPURE_LOCALE_DE_DE)");
        return ESPURE_ERR_NOT_FOUND;
#endif
    } else if (strcmp(lang, "fr") == 0) {
#ifdef CONFIG_ESPURE_LANG_FR
        extern const espure_dict_entry_t DICT_FR[];
        extern const size_t DICT_FR_SIZE;
        
        dict->entries = (espure_dict_entry_t*)DICT_FR;
        dict->count = DICT_FR_SIZE;
        
        ESP_LOGI(TAG, "[OK] Loaded FR dictionary: %zu entries", dict->count);
#else
        ESP_LOGE(TAG, "French dictionary not compiled (enable CONFIG_ESPURE_LANG_FR)");
        return ESPURE_ERR_NOT_FOUND;
#endif
    } else {
        ESP_LOGW(TAG, "No embedded dictionary for locale '%s'", lang);
        ESP_LOGW(TAG, "Use menuconfig to enable: Component config -> espure -> Languages");
        return ESPURE_ERR_NOT_FOUND;
    }
    
    return ESPURE_OK;
}

/* ========================================================================
 * Translator (Rule engine state)
 * ======================================================================== */

/**
 * @brief Translator structure (per-language configuration)
 */
struct espure_translator_s {
    const char* locale;
    // Core components
    espure_phoneme_table_t* phoneme_table;
    espure_dictionary_t* dictionary;
    
    // Letter classification (for rule matching)
    uint8_t letter_bits[256];         // Bit flags per ASCII char
    const char* letter_groups[8];     // Extended character groups
    uint32_t letter_bits_offset;      // Offset for non-Latin scripts
    
    // Translator configuration
    uint32_t stress_rule;             // STRESSPOSN_* constant
    uint32_t stress_flags;            // Stress behavior flags
    uint32_t dict_condition;          // Active dictionary conditions
    
    // Per-word state
    uint32_t word_vowel_count;
    uint32_t word_stressed_count;
    uint32_t expect_verb;
    
    const espure_config_t* config;
    size_t heap_used;
};

/**
 * @brief Letter group constants (for letter_bits)
 */
#define LETTERGP_A       0  // Vowels (aeiou)
#define LETTERGP_B       1  // Consonants (bcdfg...)
#define LETTERGP_C       2  // Consonants variant
#define LETTERGP_H       3  // hlmnr
#define LETTERGP_F       4  // Voiceless (cfhkpqstx)
#define LETTERGP_G       5  // Voiced (bdgjlmnrvwyz)
#define LETTERGP_Y       6  // eiy
#define LETTERGP_VOWEL2  7  // Extended vowels (aeiouy)

/**
 * @brief Default English letter classification
 */
static const char* DEFAULT_LETTER_GROUPS[] = {
    [LETTERGP_A]      = "aeiou",
    [LETTERGP_B]      = "bcdfgjklmnpqstvxz",
    [LETTERGP_C]      = "bcdfghjklmnpqrstvwxz",
    [LETTERGP_H]      = "hlmnr",
    [LETTERGP_F]      = "cfhkpqstx",
    [LETTERGP_G]      = "bdgjlmnrvwyz",
    [LETTERGP_Y]      = "eiy",
    [LETTERGP_VOWEL2] = "aeiouy",
};

/**
 * @brief Initialize letter classification
 */
static void setup_letters(espure_translator_t* tr) {
    // Setup default English letter groups
    for (int group = 0; group < 8; group++) {
        const char* letters = DEFAULT_LETTER_GROUPS[group];
        if (!letters) continue;
        
        uint8_t bit = (1 << group);
        for (const char* p = letters; *p; p++) {
            uint8_t ch = (uint8_t)*p;
            // uint8_t is always < 256, directly index
            tr->letter_bits[ch] |= bit;
        }
    }
    
    // TODO Phase 3: Load locale-specific letter bits
}

espure_err_t espure_translator_init(const char* lang,
                                    const espure_config_t* config,
                                    espure_phoneme_table_t* phoneme_table,
                                    espure_dictionary_t* dictionary,
                                    espure_translator_t** out) {
    if (!lang || !config || !phoneme_table || !dictionary || !out) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing translator for '%s'", lang);
    
    // Allocate translator (DRAM for fast access)
    espure_translator_t* tr = espure_calloc(1, sizeof(espure_translator_t), config);
    if (!tr) {
        ESP_LOGE(TAG, "Failed to allocate translator");
        return ESPURE_ERR_NO_MEM;
    }
    
    tr->phoneme_table = phoneme_table;
    tr->dictionary = dictionary;
    tr->config = config;
    tr->locale = lang;
    tr->heap_used = sizeof(espure_translator_t);
    
    // Setup letter classification
    setup_letters(tr);
    
    // Default stress configuration (English-like)
    tr->stress_rule = 0; // STRESSPOSN_1L (German)  // STRESSPOSN_2R (penultimate stress)
    tr->stress_flags = 0;
    tr->dict_condition = 0;
    
    ESP_LOGI(TAG, "Translator initialized: %zu bytes", tr->heap_used);
    
    *out = tr;
    return ESPURE_OK;
}

void espure_translator_deinit(espure_translator_t* translator) {
    if (!translator) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing translator");
    espure_free(translator);
}

size_t espure_translator_size(const espure_translator_t* translator) {
    return translator ? translator->heap_used : 0;
}

/* ========================================================================
 * Translation Pipeline (Dictionary-only for now)
 * ======================================================================== */

/**
 * @brief Simple word splitter (UTF-8 aware)
 */
static size_t split_words(const char* text, char words[][128], size_t max_words) {
    size_t word_count = 0;
    size_t word_len = 0;
    
    for (const char* p = text; *p && word_count < max_words; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            // Whitespace - end current word
            if (word_len > 0) {
                words[word_count][word_len] = '\0';
                word_count++;
                word_len = 0;
            }
        } else {
            // Add to current word
            if (word_len < 127) {
                words[word_count][word_len++] = *p;
            }
        }
    }
    
    // Last word
    if (word_len > 0) {
        words[word_count][word_len] = '\0';
        word_count++;
    }
    
    return word_count;
}

espure_err_t espure_translator_phonemize(espure_translator_t* translator,
                                         const char* text,
                                         char* output,
                                         size_t out_size,
                                         const espure_options_t* options) {
    if (!translator || !text || !output || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    output[0] = '\0';
    
    // Stage 1: Text Normalization (expand numbers, abbreviations, symbols, strip punctuation)
    char* norm_text = espure_malloc(1024, translator->config);
    const char* text_to_split = text;
    if (norm_text) {
        if (espure_normalize_text(translator->locale, text, norm_text, 1024) == ESPURE_OK) {
            text_to_split = norm_text;
        }
    }
    
    // Allocate word buffer on heap to avoid stack overflow
    // Max 32 words, 128 bytes each = 4KB (too much for stack!)
    char (*words)[128] = espure_malloc(32 * 128, translator->config);
    if (!words) {
        if (norm_text) espure_free(norm_text);
        return ESPURE_ERR_NO_MEM;
    }
    
    size_t word_count = split_words(text_to_split, words, 32);
    if (norm_text) {
        espure_free(norm_text);
        norm_text = NULL;
    }
    
    if (word_count == 0) {
        output[0] = '\0';
        espure_free(words);
        return ESPURE_OK;
    }
    
    // Allocate result buffer on heap
    char* result = espure_malloc(1024, translator->config);
    if (!result) {
        espure_free(words);
        return ESPURE_ERR_NO_MEM;
    }
    memset(result, 0, 1024);
    size_t result_len = 0;
    
    for (size_t i = 0; i < word_count; i++) {
        char phonemes[256];
        
        uint32_t entry_flags = 0;
        espure_err_t err = ESPURE_ERR_NOT_FOUND;
        // Try dictionary lookup unless rules_only is requested
        if (!options || !options->rules_only) {
            err = espure_dictionary_lookup(
                translator->dictionary,
                words[i],
                phonemes,
                sizeof(phonemes),
                &entry_flags
            );
            if (err != ESPURE_OK && err != ESPURE_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Dictionary lookup failed for '%s': %d", words[i], err);
                output[0] = '\0';
                espure_free(result);
                espure_free(words);
                return err;
            }
        }
        
        bool from_dict = false;
        if (err == ESPURE_OK && phonemes[0] != '$') {
            // Found in dictionary
            from_dict = true;
            ESPURE_DEBUG("Dictionary hit: %s -> %s", words[i], phonemes);
        } else {
            // Not found - try rule matching with morphological splitting
            bool was_morphed = false;
            char morphed_word[128];
            if (espure_morph_split(translator->locale, words[i],
                                   morphed_word, sizeof(morphed_word))) {
                ESPURE_DEBUG("Morphological split: %s -> %s", words[i], morphed_word);
                err = espure_text_to_phonemes(translator->locale, morphed_word, phonemes, sizeof(phonemes));
                was_morphed = true;
            } else {
                err = espure_text_to_phonemes(translator->locale, words[i], phonemes, sizeof(phonemes));
            }
            
            if (err == ESPURE_OK && phonemes[0] != '$') {
                ESPURE_DEBUG("Rule match: %s -> %s", words[i], phonemes);
                
                // Downgrade subsequent primary stresses for compound words
                if (was_morphed) {
                    bool first_stress_found = false;
                    for (char* p = phonemes; *p; p++) {
                        if (*p == '\'') {
                            if (!first_stress_found) {
                                first_stress_found = true;
                            } else {
                                *p = ','; // Downgrade to secondary stress
                            }
                        }
                    }
                }
            } else {
                if (err != ESPURE_OK) {
                    ESP_LOGE(TAG, "Rule phonemization failed for '%s': %d",
                             words[i], err);
                    output[0] = '\0';
                    espure_free(result);
                    espure_free(words);
                    return err;
                }
                // Rule matching failed - fallback to copying input
                strlcpy(phonemes, words[i], sizeof(phonemes));
                
                ESPURE_DEBUG("No match: %s (copied as-is)", words[i]);
            }
        }
        
        if (entry_flags == 1) {
            size_t len = strlen(phonemes);
            if (result_len + len + 2 < 1024) {
                if (result_len > 0) { strlcat(result, " ", 1024); result_len++; }
                strlcat(result, phonemes, 1024);
                result_len += len;
            }
            continue;
        }
        
        // Parse concatenated string ("h@loU" or rule match "fVk") and add spaces ("h @ l oU")
        // We do this for BOTH dictionary and rule matches so they can be rendered to IPA correctly
        const espure_phoneme_t* parsed_ph[sizeof(phonemes)];
        size_t num_parsed = 0;
        err = espure_parse_phoneme_string(
            translator->phoneme_table, phonemes, parsed_ph,
            sizeof(parsed_ph) / sizeof(parsed_ph[0]), &num_parsed);
        if (err != ESPURE_OK) {
            ESP_LOGE(TAG, "Phoneme parsing failed for '%s': %d", words[i], err);
            output[0] = '\0';
            espure_free(result);
            espure_free(words);
            return err;
        }
        if (num_parsed > 0) {
            size_t ppos = 0;
            for (size_t p = 0; p < num_parsed; p++) {
                if (p > 0 && ppos < sizeof(phonemes) - 1) {
                    phonemes[ppos++] = ' ';
                }
                size_t mlen = strlen(parsed_ph[p]->mnemonic);
                if (ppos + mlen < sizeof(phonemes)) {
                    memcpy(phonemes + ppos, parsed_ph[p]->mnemonic, mlen);
                    ppos += mlen;
                }
            }
            phonemes[ppos] = '\0';
            ESPURE_DEBUG("Parsed and spaced phonemes: %s", phonemes);
        }
        
        // NEW: Execute phoneme programs on phoneme string
        // This implements Step 4 of the translation pipeline:
        //   1. Word split âœ“
        //   2. Dictionary/rules âœ“
        //   3. Stress assignment (TODO)
        //   4. Phoneme programs âœ“ (NEW)
        //   5. Render (below)
        if (strlen(phonemes) > 0) {
            err = espure_execute_phoneme_programs_on_string(
                translator,
                translator->phoneme_table,  // Pass phoneme table directly
                phonemes,
                sizeof(phonemes)
            );
            
            if (err != ESPURE_OK) {
                ESP_LOGW(TAG, "Phoneme programs failed for '%s': %d", words[i], err);
                // Continue anyway - programs are optional enhancement
            }
        }
        
        // Native phonological transformations (Auslautverhärtung, coda-r, -ig spirantization, ich/ach)
        // Post-lexical phonological rules apply to all words in accordance with universal German phonology
        if (strlen(phonemes) > 0) {
            err = espure_phonology_apply(translator->locale, phonemes, sizeof(phonemes));
            if (err != ESPURE_OK) {
                ESP_LOGE(TAG, "Phonology application failed for '%s': %d", words[i], err);
                output[0] = '\0';
                espure_free(result);
                espure_free(words);
                return err;
            }
        }
        
        // Render phonemes to IPA or Kirshenbaum
        if (strlen(phonemes) > 0) {
            char rendered_phonemes[256];
            err = espure_render_phoneme_string(
                translator->phoneme_table,
                phonemes,
                rendered_phonemes,
                sizeof(rendered_phonemes),
                options->format,
                options->separator,
                options->tie
            );
            
            if (err != ESPURE_OK) {
                ESP_LOGE(TAG, "Render failed for '%s': %d", words[i], err);
                output[0] = '\0';
                espure_free(result);
                espure_free(words);
                return err;
            }
            if (phonemes[0] != '$') {
                strlcpy(phonemes, rendered_phonemes, sizeof(phonemes));
            }
        }
        
        // Append to result
        if (result_len > 0 && result_len < 1024 - 1) {
            result[result_len++] = ' ';
        }
        
        size_t plen = strlen(phonemes);
        if (result_len + plen < 1024) {
            memcpy(result + result_len, phonemes, plen);
            result_len += plen;
        }
    }
    
    result[result_len] = '\0';
    
    // Copy to output buffer
    if (result_len >= out_size) {
        ESP_LOGW(TAG, "Output buffer too small: need %zu, have %zu", 
                 result_len + 1, out_size);
        espure_free(result);
        espure_free(words);
        return ESPURE_ERR_BUFFER_TOO_SMALL;
    }
    
    strlcpy(output, result, out_size);
    
    ESPURE_DEBUG("Phonemized: '%s' -> '%s'", text, output);
    
    // Cleanup
    espure_free(result);
    espure_free(words);
    
    return ESPURE_OK;
}
