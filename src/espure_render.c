/**
 * @file espure_render.c
 * @brief Phoneme rendering to IPA/Kirshenbaum
 * 
 * Converts phoneme mnemonics to IPA (Unicode) or ASCII Kirshenbaum format.
 * Locale-specific IPA mappings are owned by each locale data directory.
 */

#include "espure_internal.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "espure_render";

typedef struct {
    const char* mnemonic;
    const char* ipa;
} phoneme_ipa_t;

static const phoneme_ipa_t COMMON_IPA_MAP[] = {
    {"%", ""},
    {",", "ˌ"},
    {",,", "ˌ"},
    {"_", ""},
    {"_|", ""},
};

/**
 * Find IPA representation for a phoneme mnemonic
 */
static const char* find_ipa(const espure_phoneme_table_t* table,
                            const char* mnemonic) {
    for (size_t i = 0; i < sizeof(COMMON_IPA_MAP) / sizeof(COMMON_IPA_MAP[0]); i++) {
        if (strcmp(COMMON_IPA_MAP[i].mnemonic, mnemonic) == 0) {
            return COMMON_IPA_MAP[i].ipa;
        }
    }
    const char* locale = espure_phoneme_table_locale(table);
    if (locale && strcmp(locale, "en-GB") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
        return espure_en_GB_ipa_lookup(mnemonic);
#else
        return NULL;
#endif
    }
    if (locale && strcmp(locale, "de-DE") == 0) {
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
        return espure_german_ipa_lookup(mnemonic);
#else
        return NULL;
#endif
    }
    return NULL;
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
    output[0] = '\0';
    
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
    const char* pending_stress = NULL;
    
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
        
        // Convert to IPA or keep as Kirshenbaum
        const char* rendered;
        const char* stress = NULL;
        bool inline_stress = false;
        if (strcmp(ph_buffer, "_^_") == 0) {
            ESP_LOGE(TAG, "Language-switch phoneme is unsupported in this build");
            output[0] = '\0';
            return ESPURE_ERR_NOT_SUPPORTED;
        }
        if (format == ESPURE_FORMAT_IPA) {
            const char* mnemonic = ph_buffer;
            if (strcmp(mnemonic, "'") == 0) {
                pending_stress = "ˈ";
                continue;
            }
            if (strcmp(mnemonic, "''") == 0 || strcmp(mnemonic, ",") == 0 || strcmp(mnemonic, ",,") == 0) {
                pending_stress = "ˌ";
                continue;
            }
            if (strcmp(mnemonic, "=") == 0) {
                pending_stress = "ˈ";
                continue;
            }
            if (mnemonic[0] == '\'') {
                inline_stress = true;
                if (mnemonic[1] == '\'') {
                    stress = "ˌ";
                    mnemonic += 2;
                } else {
                    stress = "ˈ";
                    mnemonic++;
                }
            } else if (mnemonic[0] == ',') {
                inline_stress = true;
                if (mnemonic[1] == ',') {
                    stress = "ˌ";
                    mnemonic += 2;
                } else {
                    stress = "ˌ";
                    mnemonic++;
                }
            } else {
                stress = pending_stress;
            }
            pending_stress = NULL;
            rendered = find_ipa(table, mnemonic);
            if (!rendered) {
                if (mnemonic[0] == '%' || mnemonic[0] == '_') {
                    rendered = find_ipa(table, mnemonic + 1);
                }
                if (!rendered) {
                    output[0] = '\0';
                    return ESPURE_ERR_NOT_FOUND;
                }
            }
        } else {
            rendered = ph_buffer;  // Kirshenbaum = mnemonic
        }

        size_t separator_len = first ? 0 : strlen(separator);
        size_t stress_len = stress ? strlen(stress) : 0;
        size_t rendered_len = strlen(rendered);
        if (separator_len + stress_len + rendered_len >= out_remaining) {
            ESP_LOGE(TAG, "Output buffer too small while rendering '%s'", phonemes);
            output[0] = '\0';
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }

        if (separator_len > 0) {
            memcpy(out_ptr, separator, separator_len);
            out_ptr += separator_len;
            out_remaining -= separator_len;
        }
        if (stress_len > 0) {
            memcpy(out_ptr, stress, stress_len);
            out_ptr += stress_len;
            out_remaining -= stress_len;
        }
        memcpy(out_ptr, rendered, rendered_len);
        out_ptr += rendered_len;
        out_remaining -= rendered_len;
        *out_ptr = '\0';
        first = false;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p) {
        ESP_LOGE(TAG, "Output buffer too small while rendering '%s'", phonemes);
        output[0] = '\0';
        return ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    if (pending_stress) {
        output[0] = '\0';
        return ESPURE_ERR_INVALID_ARG;
    }
    
    *out_ptr = '\0';
    
    ESP_LOGD(TAG, "Rendered '%s' to '%s' (format=%d)", 
             phonemes, output, format);
    
    return ESPURE_OK;
}
