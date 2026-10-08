/**
 * @file espure_render.c
 * @brief Phoneme rendering to IPA/Kirshenbaum
 * 
 * Converts phoneme mnemonics to IPA (Unicode) or ASCII Kirshenbaum format.
 * Simplified implementation with most common English phonemes.
 */

#include "espure_internal.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "espure_render";

/**
 * Phoneme mnemonic to IPA mapping (most common English phonemes)
 * Based on espeak-ng's IPA table
 */
typedef struct {
    const char* mnemonic;
    const char* ipa;
} phoneme_ipa_t;

static const phoneme_ipa_t PHONEME_IPA_MAP[] = {
    {"W", "œ"},
    {"3", "ɜ"},
    {"x", "x"},
    {"Y:", "øː"},
    {"y:", "yː"},
    {"Y", "ʏ"},
    {"2", "ø"},
    {"9", "œ"},
    {"C", "ç"},
    {"E", "ɛ"},
    {"O", "ɔ"},
    {"E:", "ɛː"},
    {"O:", "ɔː"},
    {"2:", "øː"},
    {"9:", "œː"},
    {"@n", "ən"},
    {"@st", "əst"},
    {"@r", "ər"},
    {"@m", "əm"},
    {"@l", "əl"},
    {"=o:", "oː"},
    {"U", "ʊ"},
    {"I", "ɪ"},
    {"V", "ʌ"},
    {"A:", "aː"},
    {"u:", "uː"},
    {"i:", "iː"},
    {"o:", "oː"},
    // Consonants
    {"b", "b"},
    {"d", "d"},
    {"f", "f"},
    {"g", "ɡ"},        // U+0261
    {"h", "h"},
    {"j", "j"},
    {"k", "k"},
    {"l", "l"},
    {"m", "m"},
    {"n", "n"},
    {"p", "p"},
    {"r", "ɹ"},        // U+0279
    {"s", "s"},
    {"t", "t"},
    {"v", "v"},
    {"w", "w"},
    {"z", "z"},
    
    // Digraphs
    {"T", "θ"},        // th (thin) - U+03B8
    {"D", "ð"},        // th (this) - U+00F0
    {"S", "ʃ"},        // sh (ship) - U+0283
    {"Z", "ʒ"},        // s (measure) - U+0292
    {"N", "ŋ"},        // ng (sing) - U+014B
    {"tS", "tʃ"},      // ch (chip)
    {"dZ", "dʒ"},      // j (jump)
    
    // Vowels - Short
    {"@", "ə"},        // schwa (about) - U+0259
    {"I", "ɪ"},        // i (bit) - U+026A
    {"E", "ɛ"},        // e (bed) - U+025B
    {"e", "e"},        // e (bed)
    {"V", "ʌ"},        // u (but) - U+028C
    {"o", "ɒ"},        // o (on) - U+0252
    {"O", "ɔ"},        // o (off) - U+0254
    {"U", "ʊ"},        // oo (book) - U+028A
    {"A", "ɑ"},        // a (father) - U+0251
    
    // Vowels - Long
    {"i:", "iː"},      // ee (see)
    {"u:", "uː"},      // oo (food)
    {"A:", "ɑː"},      // ar (far)
    {"O:", "ɔː"},      // or (for)
    {"3:", "ɜː"},      // ir (bird) - U+025C
    
    // Diphthongs
    {"eI", "eɪ"},      // ay (say)
    {"aI", "aɪ"},      // i (bite)
    {"OI", "ɔɪ"},      // oy (boy)
    {"aU", "aʊ"},      // ou (out)
    {"@U", "əʊ"},      // o (go)
    {"oU", "oʊ"},      // o (go - US)
    
    // Special
    {"kw", "kw"},      // qu (queen)
    {"ks", "ks"},      // x (box)
    {"A", "a"},        // short a (German)
    {"_!", "ʔ"},       // glottal stop
};

#define NUM_IPA_MAPPINGS (sizeof(PHONEME_IPA_MAP) / sizeof(PHONEME_IPA_MAP[0]))

/**
 * Find IPA representation for a phoneme mnemonic
 */
static const char* find_ipa(const char* mnemonic) {
    for (size_t i = 0; i < NUM_IPA_MAPPINGS; i++) {
        if (strcmp(PHONEME_IPA_MAP[i].mnemonic, mnemonic) == 0) {
            return PHONEME_IPA_MAP[i].ipa;
        }
    }
    // Not found - return mnemonic as-is
    return mnemonic;
}

/**
 * Render phoneme string to IPA or Kirshenbaum format
 * 
 * Input:  Space-separated phoneme mnemonics (e.g., "h e l @U")
 * Output: IPA (e.g., "heləʊ") or Kirshenbaum (e.g., "h e l @U")
 */
espure_err_t espure_render_phoneme_string(const espure_phoneme_table_t* table,
                                          const char* phonemes,
                                          char* output,
                                          size_t out_size,
                                          espure_format_t format,
                                          const char* separator,
                                          const char* tie) {
    if (!table || !phonemes || !output || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    (void)table;  // Unused for now (would be used for table lookup)
    
    // Default separator (none for IPA, space for Kirshenbaum)
    if (!separator) {
        separator = (format == ESPURE_FORMAT_IPA) ? "" : " ";
    }
    
    // Default tie (none)
    if (!tie) {
        tie = "";
    }
    
    char* out_ptr = output;
    size_t out_remaining = out_size;
    bool first = true;
    
    // Parse phoneme string (space-separated)
    char ph_buffer[32];
    const char* p = phonemes;
    
    while (*p && out_remaining > 1) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        // Extract phoneme mnemonic
        size_t i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < sizeof(ph_buffer) - 1) {
            ph_buffer[i++] = *p++;
        }
        ph_buffer[i] = '\0';
        
        if (i == 0) continue;  // Empty phoneme
        
        // Add separator between phonemes
        if (!first && separator[0]) {
            size_t sep_len = strlen(separator);
            if (sep_len < out_remaining) {
                memcpy(out_ptr, separator, sep_len + 1);
                out_ptr += sep_len;
                out_remaining -= sep_len;
            }
        }
        first = false;
        
        // Convert to IPA or keep as Kirshenbaum
        const char* rendered;
        if (format == ESPURE_FORMAT_IPA) {
            // Check for leading stress markers
            if (ph_buffer[0] == '\'') {
                if (ph_buffer[1] == '\'') {
                    // Secondary stress
                    if (out_remaining > 2) {
                        memcpy(out_ptr, "ˌ", 2); // U+02CC
                        out_ptr += 2;
                        out_remaining -= 2;
                    }
                    rendered = find_ipa(ph_buffer + 2);
                } else {
                    // Primary stress
                    if (out_remaining > 2) {
                        memcpy(out_ptr, "ˈ", 2); // U+02C8
                        out_ptr += 2;
                        out_remaining -= 2;
                    }
                    rendered = find_ipa(ph_buffer + 1);
                }
            } else {
                rendered = find_ipa(ph_buffer);
            }
        } else {
            rendered = ph_buffer;  // Kirshenbaum = mnemonic
        }
        
        // Append to output
        size_t len = strlen(rendered);
        if (len < out_remaining) {
            memcpy(out_ptr, rendered, len + 1);
            out_ptr += len;
            out_remaining -= len;
        } else {
            // Buffer full
            break;
        }
    }
    
    *out_ptr = '\0';
    
    ESP_LOGD(TAG, "Rendered '%s' to '%s' (format=%d)", 
             phonemes, output, format);
    
    return ESPURE_OK;
}
