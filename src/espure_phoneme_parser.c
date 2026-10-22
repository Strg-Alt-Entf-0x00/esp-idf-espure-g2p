#include "espure_internal.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "espure_parser";

static inline bool is_phoneme_vowel_following(const char* s) {
    if (!s) return false;
    while (*s == '\'' || *s == ',' || *s == '%' || *s == '=') s++;
    return *s && (strchr("aeiouyAEIOUY29W@", *s) != NULL);
}

/**
 * @brief Parse concatenated phoneme string (greedy longest-match)
 */
espure_err_t espure_parse_phoneme_string(
    const espure_phoneme_table_t* phoneme_table,
    const char* input,
    const espure_phoneme_t** output,
    size_t max_output,
    size_t* out_count
) {
    if (!phoneme_table || !input || !output || !out_count) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    *out_count = 0;
    size_t pos = 0;
    size_t input_len = strlen(input);
    #define MAX_PHONEME_LEN 4  // Max: "oU", "tS", "dZ", etc.
    
    while (pos < input_len) {
        // Skip spaces/barriers
        if (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '|') {
            pos++;
            continue;
        }
        
        // Try longest match first (greedy!)
        const espure_phoneme_t* found = NULL;
        size_t matched_len = 0;
        
        size_t max_try = (input_len - pos < MAX_PHONEME_LEN) 
                         ? (input_len - pos) 
                         : MAX_PHONEME_LEN;
        
        for (size_t len = max_try; len > 0; len--) {
            char candidate[MAX_PHONEME_LEN + 1];
            memcpy(candidate, &input[pos], len);
            candidate[len] = '\0';
            
            // Lookup in phoneme table
            found = espure_phoneme_lookup(phoneme_table, candidate);
            
            if (found) {
                // Syllabic/schwa+sonorant coda mnemonics (@r, @n, @m, @l) cannot precede a vowel;
                // the sonorant consonant is an onset to the following syllable.
                if (len > 1 && candidate[0] == '@' &&
                    (candidate[1] == 'r' || candidate[1] == 'n' || candidate[1] == 'm' || candidate[1] == 'l')) {
                    if (pos + len < input_len && is_phoneme_vowel_following(&input[pos + len])) {
                        found = NULL;
                        continue;
                    }
                }
                matched_len = len;
                break;  // Found! Use this match
            }
        }
        
        if (found) {
            if (*out_count >= max_output) {
                ESP_LOGE(TAG, "Phoneme output capacity exceeded (%zu)", max_output);
                *out_count = 0;
                return ESPURE_ERR_BUFFER_TOO_SMALL;
            }
            output[*out_count] = found;
            (*out_count)++;
            pos += matched_len;
        } else {
            // Handle literal digits or single quotes (stress)
            if (input[pos] == '\'' || (input[pos] >= '0' && input[pos] <= '9')) {
                // Try looking up just this character
                char candidate[2] = {input[pos], '\0'};
                found = espure_phoneme_lookup(phoneme_table, candidate);
                if (found) {
                    if (*out_count >= max_output) {
                        ESP_LOGE(TAG, "Phoneme output capacity exceeded (%zu)", max_output);
                        *out_count = 0;
                        return ESPURE_ERR_BUFFER_TOO_SMALL;
                    }
                    output[*out_count] = found;
                    (*out_count)++;
                } else {
                    ESP_LOGD(TAG, "Unknown phoneme digit at pos %zu: '%c'", pos, input[pos]);
                }
            } else if (input[pos] == '#' || input[pos] == '%' || input[pos] == '-' || 
                       input[pos] == ';' || input[pos] == '~' || input[pos] == '(' || 
                       input[pos] == ')' || input[pos] == '|' || input[pos] == '/' || 
                       input[pos] == '.' || input[pos] == '_') {
                // Silently skip boundary and rule metadata
            } else {
                ESP_LOGE(TAG, "Unknown phoneme character at pos %zu: '%c'",
                         pos, input[pos]);
                *out_count = 0;
                return ESPURE_ERR_NOT_FOUND;
            }
            pos++;
        }
    }
    
    return ESPURE_OK;
}
