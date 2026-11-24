#include "espure_internal.h"
#include "espure_stress.h"
#include <string.h>
#include <esp_log.h>

#define STRESS_IS_PRIMARY   4
#define STRESS_IS_SECONDARY 3
#define STRESS_IS_UNSTRESSED 1
#define STRESS_IS_DIMINISHED 0
#define STRESS_IS_NOT_STRESSED -1

void espure_assign_word_stress(espure_phlist_entry_t* plist, size_t plist_len) {
    if (plist_len == 0 || !plist) return;

    size_t vowel_indices[32];
    int vowel_stress[33];
    int vowel_count = 1;   
    
    for (size_t i = 0; i < sizeof(vowel_stress) / sizeof(vowel_stress[0]); i++) {
        vowel_stress[i] = STRESS_IS_NOT_STRESSED;
    }
    
    int max_stress = STRESS_IS_NOT_STRESSED;
    
    for (size_t i = 0; i < plist_len; i++) {
        if (plist[i].ph->type == ESPURE_PHONEME_VOWEL) {
            if (vowel_count < 32) {
                vowel_indices[vowel_count] = i;
                int current_stress = plist[i].stresslevel;
                if (current_stress == 0) {
                    if ((plist[i].ph->flags & ESPURE_PHONEME_FLAG_UNSTRESSED) ||
                        strcmp(plist[i].ph->mnemonic, "@") == 0 ||
                        strcmp(plist[i].ph->mnemonic, "@-") == 0) {
                        current_stress = STRESS_IS_UNSTRESSED;
                    } else {
                        current_stress = STRESS_IS_NOT_STRESSED;
                    }
                } else if (current_stress >= STRESS_PRIMARY) {
                    current_stress = STRESS_IS_PRIMARY;
                } else if (current_stress == STRESS_SECONDARY) {
                    current_stress = STRESS_IS_SECONDARY;
                } else if (current_stress == STRESS_DIMINISHED) {
                    current_stress = STRESS_IS_DIMINISHED;
                }
                
                vowel_stress[vowel_count] = current_stress;
                if (current_stress > max_stress) {
                    max_stress = current_stress;
                }
                
                vowel_count++;
            }
        }
    }
    
    vowel_stress[vowel_count] = STRESS_IS_UNSTRESSED; 
    
    if (vowel_count <= 1) return;

    // Detect German inseparable prefixes (be-, ge-, ver-, zer-, ent-, emp-, er-)
    if (vowel_count >= 2 && vowel_indices[1] < plist_len) {
        bool is_inseparable_prefix = false;
        
        // Check "be-": starts with 'b' followed by schwa '@'
        if (vowel_indices[1] == 1 && strcmp(plist[0].ph->mnemonic, "b") == 0 &&
            (strcmp(plist[vowel_indices[1]].ph->mnemonic, "@") == 0 ||
             strcmp(plist[vowel_indices[1]].ph->mnemonic, "@-") == 0)) {
            is_inseparable_prefix = true;
        }
        // Check "ge-": starts with 'g' followed by schwa '@'
        else if (vowel_indices[1] == 1 && strcmp(plist[0].ph->mnemonic, "g") == 0 &&
                 (strcmp(plist[vowel_indices[1]].ph->mnemonic, "@") == 0 ||
                  strcmp(plist[vowel_indices[1]].ph->mnemonic, "@-") == 0)) {
            is_inseparable_prefix = true;
        }
        // Check "ver-": starts with 'f' followed by vowel 1, followed by 'r' or '6'
        else if (vowel_indices[1] == 1 && strcmp(plist[0].ph->mnemonic, "f") == 0) {
            size_t r_idx = vowel_indices[1] + 1;
            if (r_idx < plist_len && (strcmp(plist[r_idx].ph->mnemonic, "r") == 0 ||
                                     strcmp(plist[r_idx].ph->mnemonic, "6") == 0)) {
                is_inseparable_prefix = true;
            }
        }
        // Check "zer-": starts with "ts" followed by vowel 1, followed by 'r' or '6'
        else if (vowel_indices[1] == 1 && strcmp(plist[0].ph->mnemonic, "ts") == 0) {
            size_t r_idx = vowel_indices[1] + 1;
            if (r_idx < plist_len && (strcmp(plist[r_idx].ph->mnemonic, "r") == 0 ||
                                     strcmp(plist[r_idx].ph->mnemonic, "6") == 0)) {
                is_inseparable_prefix = true;
            }
        }
        // Check "ent-": vowel 1 at index 0 (or after glottal/pause), followed by 'n', 't'
        else if (vowel_indices[1] <= 1) {
            size_t v_idx = vowel_indices[1];
            if (v_idx + 2 < plist_len && strcmp(plist[v_idx + 1].ph->mnemonic, "n") == 0 &&
                                         strcmp(plist[v_idx + 2].ph->mnemonic, "t") == 0) {
                is_inseparable_prefix = true;
            }
            // Check "emp-": vowel 1 followed by 'm', 'p'/'pf'
            else if (v_idx + 2 < plist_len && strcmp(plist[v_idx + 1].ph->mnemonic, "m") == 0 &&
                                         (strcmp(plist[v_idx + 2].ph->mnemonic, "p") == 0 ||
                                          strcmp(plist[v_idx + 2].ph->mnemonic, "pf") == 0)) {
                is_inseparable_prefix = true;
            }
            // Check "er-": vowel 1 followed by 'r'/'6' and a consonant
            else if (v_idx + 1 < plist_len && (strcmp(plist[v_idx + 1].ph->mnemonic, "r") == 0 ||
                                               strcmp(plist[v_idx + 1].ph->mnemonic, "6") == 0)) {
                if (v_idx + 2 < plist_len && plist[v_idx + 2].ph->type != ESPURE_PHONEME_VOWEL) {
                    is_inseparable_prefix = true;
                }
            }
        }
        
        if (is_inseparable_prefix && vowel_stress[1] <= STRESS_IS_UNSTRESSED) {
            vowel_stress[1] = STRESS_IS_UNSTRESSED;
        }
    }

    int stress = (max_stress < STRESS_IS_PRIMARY) ? STRESS_IS_PRIMARY : STRESS_IS_SECONDARY;
    
    for (int v = 1; v < vowel_count; v++) {
        if (vowel_stress[v] < STRESS_IS_DIMINISHED) {
            if ((vowel_stress[v - 1] <= STRESS_IS_UNSTRESSED) &&
                ((vowel_stress[v + 1] <= STRESS_IS_UNSTRESSED) || 
                 (stress == STRESS_IS_PRIMARY && vowel_stress[v + 1] <= STRESS_IS_NOT_STRESSED))) {
                
                vowel_stress[v] = stress;
                stress = STRESS_IS_SECONDARY;
            }
        }
    }
    
    for (int v = 1; v < vowel_count; v++) {
        int v_stress = vowel_stress[v];
        if (v_stress <= STRESS_IS_UNSTRESSED) {
            if (v == 1 || v == vowel_count - 1) {
                v_stress = STRESS_IS_UNSTRESSED;
            } else if (v == vowel_count - 2 && vowel_stress[vowel_count - 1] <= STRESS_IS_UNSTRESSED) {
                v_stress = STRESS_IS_UNSTRESSED;
            } else {
                v_stress = STRESS_IS_DIMINISHED;
            }
            vowel_stress[v] = v_stress;
        }
        
        size_t plist_idx = vowel_indices[v];
        if (v_stress == STRESS_IS_PRIMARY) {
            plist[plist_idx].stresslevel = STRESS_PRIMARY;
        } else if (v_stress == STRESS_IS_SECONDARY) {
            plist[plist_idx].stresslevel = STRESS_SECONDARY;
        } else if (v_stress == STRESS_IS_UNSTRESSED) {
            plist[plist_idx].stresslevel = STRESS_UNSTRESSED;
        } else if (v_stress == STRESS_IS_DIMINISHED) {
            plist[plist_idx].stresslevel = STRESS_DIMINISHED;
        }
    }
}

#define MAX_STRESS_PHONEMES 64

espure_err_t espure_stress_apply(
    const espure_phoneme_table_t* phoneme_table,
    char* phoneme_str,
    size_t buf_size
) {
    if (!phoneme_table || !phoneme_str || buf_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }
    if (phoneme_str[0] == '\0') {
        return ESPURE_OK;
    }

    espure_phlist_entry_t plist[MAX_STRESS_PHONEMES];
    size_t plist_len = 0;
    size_t pos = 0;
    size_t str_len = strlen(phoneme_str);
    bool is_first_phoneme = true;
    uint8_t pending_stress = 0;

    while (pos < str_len && plist_len < MAX_STRESS_PHONEMES) {
        char mnemonic[8] = {0};
        size_t mnem_len = 0;
        uint8_t stress_level = pending_stress;

        while (pos < str_len && phoneme_str[pos] == ' ') {
            pos++;
        }
        if (pos >= str_len) {
            break;
        }

        if (phoneme_str[pos] == '\'') {
            if (pos + 1 < str_len && phoneme_str[pos + 1] == '\'') {
                stress_level = STRESS_SECONDARY;
                pos += 2;
            } else {
                stress_level = STRESS_PRIMARY;
                pos++;
            }
        } else if (phoneme_str[pos] == ',') {
            stress_level = STRESS_SECONDARY;
            pos++;
        } else if (phoneme_str[pos] == '%') {
            stress_level = STRESS_UNSTRESSED;
            pos++;
        }
        pending_stress = stress_level;

        while (pos < str_len && phoneme_str[pos] != ' ' && mnem_len < sizeof(mnemonic) - 1) {
            mnemonic[mnem_len++] = phoneme_str[pos++];
        }
        if (mnem_len == 0) {
            continue;
        }

        pending_stress = 0;
        mnemonic[mnem_len] = '\0';

        const espure_phoneme_t* ph = espure_phoneme_lookup(phoneme_table, mnemonic);
        if (!ph) {
            continue;
        }

        espure_phlist_entry_t* entry = &plist[plist_len++];
        entry->ph = ph;
        entry->stresslevel = stress_level;
        entry->synthflags = (ph->type == ESPURE_PHONEME_VOWEL) ? SFLAG_SYLLABLE : 0;
        entry->newword = is_first_phoneme ? PHLIST_START_OF_WORD : 0;
        is_first_phoneme = false;
    }

    if (plist_len == 0) {
        return ESPURE_OK;
    }

    espure_assign_word_stress(plist, plist_len);

    /* Format back to phoneme_str */
    size_t out_pos = 0;
    for (size_t i = 0; i < plist_len; i++) {
        const espure_phoneme_t* ph = plist[i].ph;
        if (!ph) {
            continue;
        }

        if (plist[i].stresslevel >= STRESS_PRIMARY) {
            if (out_pos < buf_size - 1) {
                phoneme_str[out_pos++] = '\'';
            }
        } else if (plist[i].stresslevel == STRESS_SECONDARY) {
            if (out_pos < buf_size - 2) {
                phoneme_str[out_pos++] = '\'';
                phoneme_str[out_pos++] = '\'';
            }
        }

        size_t mlen = strlen(ph->mnemonic);
        if (out_pos + mlen >= buf_size) {
            phoneme_str[0] = '\0';
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
        memcpy(phoneme_str + out_pos, ph->mnemonic, mlen);
        out_pos += mlen;

        if (i < plist_len - 1 && out_pos < buf_size - 1) {
            phoneme_str[out_pos++] = ' ';
        }
    }
    phoneme_str[out_pos] = '\0';

    return ESPURE_OK;
}
