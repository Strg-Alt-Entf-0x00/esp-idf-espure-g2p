#include "espure_internal.h"
#include "espure_compiled_rules.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// VM Opcodes
#define RULE_PRE 1
#define RULE_POST 2
#define RULE_CONDITION 5
#define RULE_PRE_ATSTART 8
#define RULE_STRESSED 10
#define RULE_DOUBLE 11
#define RULE_INC_SCORE 12
#define RULE_ENDING 14
#define RULE_DIGIT 15
#define RULE_LETTERGP 17
#define RULE_LETTERGP2 18
#define RULE_SYLLABLE 21
#define RULE_NO_SUFFIX 24
#define RULE_NOTVOWEL 25
#define RULE_DOLLAR 28
#define RULE_NOVOWELS 29
#define RULE_SPACE 32

static inline bool is_vowel(unsigned char c) {
    if (c && strchr("aeiouy", c)) return true;
    if (c == 0xC3 || c == 0xA4 || c == 0xB6 || c == 0xBC) return true; // UTF-8 umlauts
    return false;
}

static inline bool is_consonant(unsigned char c) {
    if (c >= 'a' && c <= 'z' && !is_vowel(c)) return true;
    if (c == 0x9F) return true; // ß second byte
    return false;
}

static inline bool is_front_vowel(unsigned char c) {
    if (c && strchr("eiy", c)) return true;
    if (c == 0xC3 || c == 0xA4 || c == 0xB6 || c == 0xBC) return true; // ä, ö, ü
    return false;
}

static bool check_letter_group(unsigned char letter, unsigned char group) {
    if (group == 'A') return is_vowel(letter); // aeiou + äöü
    if (group == 'B') return letter && strchr("bcdfgjklmnpqstvxz", letter) != NULL;
    if (group == 'C') return letter && strchr("bcdfghjklmnpqrstvwxz", letter) != NULL;
    if (group == 'D') return letter && strchr("hlmnr", letter) != NULL;
    if (group == 'E') return letter && strchr("cfhkpqstx", letter) != NULL;
    if (group == 'F') return letter && strchr("bdgjlmnrvwyz", letter) != NULL;
    if (group == 'G') return letter && strchr("eiy", letter) != NULL;
    if (group == 'H') return is_vowel(letter); // aeiouy + äöü
    return false;
}

// VM execution
static int match_rule(const unsigned char* word, size_t word_len, size_t pos, const espure_compiled_rule_t* rule) {
    const uint8_t* prog = rule->prog;
    size_t klen = rule->prog_len;
    size_t k = 0;
    
    int match_type = 0; // 0=CONSUME, 1=PRE, 2=POST
    bool check_atstart = false;
    
    int pre_ptr = (int)pos - 1;
    int post_ptr = (int)pos + strlen(rule->match_str);
    
    int points = 1;
    
    while (k < klen) {
        uint8_t rb = prog[k++];
        
        if (rb <= 32) {
            if (rb == RULE_PRE_ATSTART) {
                check_atstart = true;
                match_type = 1; // PRE
            } else if (rb == RULE_PRE) {
                match_type = 1; // PRE
            } else if (rb == RULE_POST) {
                match_type = 2; // POST
            } else if (rb == RULE_CONDITION) {
                k++; // Skip condition number
            } else if (rb == RULE_STRESSED) {
                points++;
            } else if (rb == RULE_DOUBLE) {
                unsigned char c1 = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr] : 0) : (post_ptr < word_len ? word[post_ptr-1] : 0);
                unsigned char c2 = (match_type == 1) ? (pre_ptr >= 1 ? word[pre_ptr-1] : 0) : (post_ptr < word_len ? word[post_ptr] : 0);
                if (c1 != c2 || c1 == 0) return -1;
                if (match_type == 1) pre_ptr--; else post_ptr++;
            } else if (rb == RULE_INC_SCORE) {
                points += 20;
            } else if (rb == RULE_ENDING) {
                k += 3;
            } else if (rb == RULE_DIGIT) {
                unsigned char c = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr--] : 0) : (post_ptr < word_len ? word[post_ptr++] : 0);
                if (!(c >= '0' && c <= '9')) return -1;
            } else if (rb == RULE_LETTERGP || rb == RULE_LETTERGP2) {
                uint8_t grp = prog[k++];
                unsigned char c = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr--] : 0) : (post_ptr < word_len ? word[post_ptr++] : 0);
                if (!check_letter_group(c, grp)) return -1;
                points += 20;
            } else if (rb == RULE_SYLLABLE) {
                int syllable_count = 1;
                while (k < klen && prog[k] == RULE_SYLLABLE) {
                    k++;
                    syllable_count++;
                }
                int vowel_count = 0;
                bool in_vowel = false;
                if (match_type == 1) { // PRE: Count vowels before this word
                    for (int j = 0; j <= pre_ptr; j++) {
                        bool v = is_vowel(word[j]);
                        if (v && !in_vowel) vowel_count++;
                        in_vowel = v;
                    }
                } else if (match_type == 2) { // POST: Count vowels after this word
                    for (int j = post_ptr; j < word_len; j++) {
                        bool v = is_vowel(word[j]);
                        if (v && !in_vowel) vowel_count++;
                        in_vowel = v;
                    }
                }
                if (syllable_count > vowel_count) return -1;
                points += 20;
            } else if (rb == RULE_NOTVOWEL) {
                unsigned char c = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr--] : 0) : (post_ptr < word_len ? word[post_ptr++] : 0);
                if (is_vowel(c)) return -1;
            } else if (rb == RULE_DOLLAR) {
                k++; // skip command
            } else if (rb == RULE_SPACE) {
                unsigned char c = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr--] : 0) : (post_ptr < word_len ? word[post_ptr++] : 0);
                if (c != ' ' && c != '\0' && c != '-') return -1;
            }
        } else {
            // Literal character match
            if (match_type == 0) {
                // Already matched by strncmp!
                points += 21;
            } else {
                unsigned char c = (match_type == 1) ? (pre_ptr >= 0 ? word[pre_ptr--] : 0) : (post_ptr < word_len ? word[post_ptr++] : 0);
                if (c != rb) { 
                    /* if (pos==0) printf("fail literal %c != %c\n", c, rb); */
                    return -1; 
                }
                points += 21;
            }
        }
    }
    
    if (check_atstart && pre_ptr >= 0 && word[pre_ptr] != ' ' && word[pre_ptr] != '\0') {
        return -1; // Failed RULE_PRE_ATSTART
    }
    
    return points;
}

espure_err_t espure_text_to_phonemes(const char* lang, const char* text, char* phonemes_out, size_t out_size) {
    if (!text || !phonemes_out || out_size == 0) return ESPURE_ERR_INVALID_ARG;
    if (strcmp(lang, "de") != 0) return ESPURE_ERR_NOT_SUPPORTED;
    
    phonemes_out[0] = '\0';
    
    // Lowercase ASCII and handle UTF-8 umlauts
    unsigned char lower[256];
    size_t text_len = strlen(text);
    if (text_len > sizeof(lower) - 1) text_len = sizeof(lower) - 1;
    
    size_t j = 0;
    for (size_t i = 0; i < text_len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            lower[j++] = tolower(c);
        } else if (c == 0xC3 && i + 1 < text_len) {
            unsigned char next_c = (unsigned char)text[i+1];
            // Convert uppercase German umlauts to lowercase UTF-8
            if (next_c == 0x84) { lower[j++] = 0xC3; lower[j++] = 0xA4; i++; } // Ä -> ä
            else if (next_c == 0x96) { lower[j++] = 0xC3; lower[j++] = 0xB6; i++; } // Ö -> ö
            else if (next_c == 0x9C) { lower[j++] = 0xC3; lower[j++] = 0xBC; i++; } // Ü -> ü
            else { 
                lower[j++] = c;
            }
        } else {
            lower[j++] = c;
        }
    }
    lower[j] = '\0';
    text_len = j;
    
    size_t out_idx = 0;
    size_t i = 0;
    
    while (i < text_len && out_idx < out_size - 1) {
        unsigned char c = lower[i];
        int best_points = -1;
        const espure_compiled_rule_t* best_rule = NULL;
        
        // 1. Check DE_GROUPS2 (two-byte sequences like 'ch', UTF-8 characters like 'ä')
        if (i + 1 < text_len) {
            uint16_t hkey = ((uint16_t)lower[i] << 8) | lower[i+1];
            for (size_t g2_idx = 0; g2_idx < DE_GROUPS2_COUNT; g2_idx++) {
                if (DE_GROUPS2[g2_idx].key == hkey) {
                    const espure_rule_group_t* g2 = &DE_GROUPS2[g2_idx].group;
                    for (size_t r = 0; r < g2->count; r++) {
                        const espure_compiled_rule_t* rule = &g2->rules[r];
                        size_t mlen = strlen(rule->match_str);
                        if (strncmp((const char*)lower + i, rule->match_str, mlen) == 0) {
                            int pts = match_rule(lower, text_len, i, rule);
                            if (pts > 0) pts += 35; // groups2 boost!
                            if (pts != -1 && pts > best_points) {
                                best_points = pts;
                                best_rule = rule;
                            }
                        }
                    }
                    break;
                }
            }
        }

        // 2. Check DE_GROUPS1
        const espure_rule_group_t* group = &DE_GROUPS1[c];
        if (group->count == 0) {
            group = &DE_DEFAULT_GROUP;
        }
        
        for (size_t r = 0; r < group->count; r++) {
            const espure_compiled_rule_t* rule = &group->rules[r];
            size_t mlen = strlen(rule->match_str);
            if (strncmp((const char*)lower + i, rule->match_str, mlen) == 0) {
                int pts = match_rule(lower, text_len, i, rule);
                if (pts != -1 && pts > best_points) {
                    best_points = pts;
                    best_rule = rule;
                }
            }
        }
        
        if (best_rule && best_points >= 0) {
            printf("MATCH: '%s' -> '%s' (pts: %d)\n", best_rule->match_str, best_rule->phonemes, best_points);
            size_t ph_len = strlen(best_rule->phonemes);
            if (out_idx + ph_len < out_size) {
                strcpy(phonemes_out + out_idx, best_rule->phonemes);
                out_idx += ph_len;
            }
            i += strlen(best_rule->match_str);
        } else {
            // Unmatched character, skip or append verbatim
            if (out_idx + 1 < out_size) {
                phonemes_out[out_idx++] = lower[i];
                phonemes_out[out_idx] = '\0';
            }
            i++;
        }
    }
    return ESP_OK;
}

