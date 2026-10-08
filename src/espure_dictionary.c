/**
 * @file espure_dictionary.c
 * @brief Dictionary lookup using binary search
 * 
 * Port of espeak-ng dictionary system for ESP32-P4 embedded:
 * - Binary search instead of hash-table (736 KB â†’ 184 KB memory)
 * - Sorted dictionary arrays in FLASH
 * - Multi-language support via Kconfig
 * 
 * Performance: O(log n) ~18 Î¼s (negligible for 100 ms/phoneme TTS)
 * Decision: Agent recommends binary search (87% confidence)
 */

#include "espure_internal.h"
#include "espure_phoneme_program.h"
#include "espure_morph.h"
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

/**
 * @brief Binary search lookup (O(log n) - ~13 comparisons for 6,641 entries)
 * 
 * Performance: ~18 Î¼s @ 360 MHz (negligible for 100 ms/phoneme TTS)
 */
// Removed compare_word, using strcmp on lowercased string instead

/**
 * @brief Lookup word in sorted dictionary array
 */
espure_err_t espure_dictionary_lookup(espure_dictionary_t* dict,
                                     const char* word,
                                     char* phonemes_out,
                                     size_t out_size,
                                     uint32_t* flags_out) {
    if (!dict || !word || !phonemes_out || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    // Lowercase the input word (including German UTF-8 umlauts) once before binary search
    char lower_word[128];
    size_t j = 0;
    for (size_t i = 0; word[i] != '\0' && j < 127; i++) {
        unsigned char c = (unsigned char)word[i];
        if (c >= 'A' && c <= 'Z') {
            lower_word[j++] = c + 32;
        } else if (c == 0xC3 && word[i+1] != '\0') {
            unsigned char next_c = (unsigned char)word[i+1];
            // Ä=0x84->0xA4, Ö=0x96->0xB6, Ü=0x9C->0xBC
            if (next_c == 0x84 || next_c == 0x96 || next_c == 0x9C) {
                lower_word[j++] = 0xC3;
                lower_word[j++] = next_c + 0x20;
                i++; // skip next byte
            } else {
                lower_word[j++] = c;
            }
        } else {
            lower_word[j++] = c;
        }
    }
    lower_word[j] = '\0';
    
#ifdef CONFIG_ESPURE_DICT_HUFFMAN_TRIE
    extern espure_err_t espure_dict_compressed_lookup(const char* word, char* out_phonemes, size_t out_size);
    if (flags_out) *flags_out = 0; // Flags not supported in compressed mode yet
    return espure_dict_compressed_lookup(lower_word, phonemes_out, out_size);
#else
    if (!dict->entries || dict->count == 0) {
        return ESPURE_ERR_NOT_FOUND;
    }
    
    // Binary search
    size_t left = 0;
    size_t right = dict->count;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = strcmp(lower_word, dict->entries[mid].word);
        
        if (cmp == 0) {
            // Found!
            strlcpy(phonemes_out, dict->entries[mid].phonemes, out_size);
            
            if (flags_out) {
                *flags_out = dict->entries[mid].flags;
            }
            
            ESPURE_DEBUG("Dictionary hit: %s -> %s", word, phonemes_out);
            return ESPURE_OK;
        }
        
        if (cmp < 0) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    // Not found
    return ESPURE_ERR_NOT_FOUND;
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
    
    ESP_LOGI(TAG, "Loading embedded dictionary for '%s'", lang);
    
    // Load language-specific dictionary
    // These are generated by tools/dict_to_binary.py and 
    // conditionally compiled via CMakeLists.txt based on Kconfig
    
    if (strcmp(lang, "en") == 0) {
#ifdef CONFIG_ESPURE_LANG_EN
        // Include English dictionary data
        extern const espure_dict_entry_t DICT_EN[];
        extern const size_t DICT_EN_SIZE;
        
        dict->entries = (espure_dict_entry_t*)DICT_EN;  // Point to FLASH data
        dict->count = DICT_EN_SIZE;
        
        ESP_LOGI(TAG, "[OK] Loaded EN dictionary: %zu entries", dict->count);
        ESP_LOGI(TAG, "   Memory: %zu KB (pointer array only)", 
                 (dict->count * sizeof(espure_dict_entry_t)) / 1024);
#else
        ESP_LOGE(TAG, "English dictionary not compiled (enable CONFIG_ESPURE_LANG_EN)");
        return ESPURE_ERR_NOT_FOUND;
#endif
    } else if (strcmp(lang, "de-DE-standard") == 0 || strcmp(lang, "de") == 0 || strcmp(lang, "de_DE") == 0) {
#ifdef CONFIG_ESPURE_LANG_DE
        extern const espure_dict_entry_t DICT_DE_DE[];
        extern const size_t DICT_DE_DE_SIZE;
        
        dict->entries = (espure_dict_entry_t*)DICT_DE_DE;
        dict->count = DICT_DE_DE_SIZE;
        
        ESP_LOGI(TAG, "[OK] Loaded DE dictionary: %zu entries", dict->count);
#else
        ESP_LOGE(TAG, "German dictionary not compiled (enable CONFIG_ESPURE_LANG_DE)");
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
        ESP_LOGW(TAG, "No embedded dictionary for language '%s'", lang);
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
    char lang[8];
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
    
    // TODO Phase 3: Load language-specific letter bits
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
    if (lang) { strlcpy(tr->lang, lang, sizeof(tr->lang)); } else { strlcpy(tr->lang, "en", sizeof(tr->lang)); }
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
    
    // Allocate word buffer on heap to avoid stack overflow
    // Max 32 words, 128 bytes each = 4KB (too much for stack!)
    char (*words)[128] = espure_malloc(32 * 128, translator->config);
    if (!words) {
        return ESPURE_ERR_NO_MEM;
    }
    
    size_t word_count = split_words(text, words, 32);
    
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
        // Try dictionary lookup
        espure_err_t err = espure_dictionary_lookup(
            translator->dictionary,
            words[i],
            phonemes,
            sizeof(phonemes),
            &entry_flags
        );
        
        if (err == ESPURE_OK && phonemes[0] != '$') {
            // Found in dictionary
            ESPURE_DEBUG("Dictionary hit: %s -> %s", words[i], phonemes);
        } else {
            // Not found - try rule matching with morphological splitting
            bool was_morphed = false;
            char morphed_word[128];
            if (espure_morph_split(words[i], morphed_word, sizeof(morphed_word))) {
                ESPURE_DEBUG("Morphological split: %s -> %s", words[i], morphed_word);
                err = espure_text_to_phonemes(translator->lang, morphed_word, phonemes, sizeof(phonemes));
                was_morphed = true;
            } else {
                err = espure_text_to_phonemes(translator->lang, words[i], phonemes, sizeof(phonemes));
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
        const espure_phoneme_t* parsed_ph[32];
        size_t num_parsed = 0;
        if (espure_parse_phoneme_string(translator->phoneme_table, phonemes, parsed_ph, 32, &num_parsed) == ESPURE_OK && num_parsed > 0) {
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
        
        // NEW: Render phonemes to IPA or Kirshenbaum (Step 5)
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
            
            if (err == ESPURE_OK && phonemes[0] != '$') {
                strlcpy(phonemes, rendered_phonemes, sizeof(phonemes));
            } else {
                ESP_LOGW(TAG, "Render failed for '%s': %d", words[i], err);
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

