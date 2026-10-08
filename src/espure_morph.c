#include "espure_morph.h"
#include "espure_internal.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "../data/dict/de_DE_morph_trie.h"

static const char* const prefixes[] = {
    "ab", "an", "auf", "aus", "be", "bei", "da", "dar", "durch", "ein", "emp", 
    "ent", "er", "fort", "ge", "her", "hin", "hinter", "mit", "nach", "nieder", 
    "ober", "um", "un", "unter", "ur", "ver", "vor", "voran", "voraus", "vorbei", "weg", 
    "weiter", "wider", "wieder", "zer", "zu", "zurecht", "zurueck", "zusammen", "zwischen",
    "bundes", "kinder", "haupt", "sonder", "super", "halb", "lieblings"
};
static const size_t num_prefixes = sizeof(prefixes) / sizeof(prefixes[0]);

static const char* const inseparable_prefixes[] = {
    "be", "emp", "ent", "er", "ge", "ver", "zer"
};
static const size_t num_inseparable = sizeof(inseparable_prefixes) / sizeof(inseparable_prefixes[0]);

static const char* const suffixes[] = {
    "bar", "chen", "ei", "en", "end", "er", "haft", "heit", "ie", "ig", "in", 
    "isch", "keit", "lein", "lich", "ling", "nis", "sal", "sam", "schaft", 
    "tum", "ung", "werk", "los", "voll", "mäßig", "innen",
    "der", "dem", "den", "des", "ren", "test", "est", "este", "esten", "ester", "estes", "ers", "ern"
};
static const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);

static const char* const terminal_suffixes[] = {
    "e", "en", "er", "es", "em", "s", "t", "st", "te", "ten", "tet", "test", 
    "ete", "eten", "etet", "etest",
    "nd", "nde", "nden", "ndem", "ndes", "nder", "n", "est", "este", "esten", 
    "ester", "estes", "der", "dem", "den", "des",
    "ende", "enden", "endem", "endes", "ender"
};
static const size_t num_term_suffixes = sizeof(terminal_suffixes) / sizeof(terminal_suffixes[0]);

static const char* const short_roots[] = {
    "ei", "öl", "au"
};
static const size_t num_short_roots = sizeof(short_roots) / sizeof(short_roots[0]);

static const char* const linking_elements[] = {"", "s", "es", "n", "en", "e", "er"};
static const size_t num_links = sizeof(linking_elements) / sizeof(linking_elements[0]);

static uint8_t byte_tolower(uint8_t b) {
    if (b >= 'A' && b <= 'Z') return b + ('a' - 'A');
    return b;
}

static bool is_valid_root(const char* str, size_t len) {
    if (len == 0) return false;
    uint32_t state = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = byte_tolower((uint8_t)str[i]);
        bool found = false;
        uint32_t next_state = de_DE_morph_trie[state].next_state;
        uint16_t child_count = de_DE_morph_trie[state].child_count;
        
        for (uint16_t j = 0; j < child_count; j++) {
            if (de_DE_morph_trie[next_state + j].byte_val == b) {
                state = next_state + j;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return de_DE_morph_trie[state].is_end;
}

static bool is_in_array(const char* const arr[], size_t size, const char* str, size_t len) {
    for (size_t i = 0; i < size; i++) {
        size_t alen = strlen(arr[i]);
        if (alen == len) {
            bool match = true;
            for (size_t j = 0; j < len; j++) {
                if (byte_tolower((uint8_t)str[j]) != arr[i][j]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

#define INF_SCORE -1000000

typedef struct {
    int score;
    int next_i;
    int root_end; // end of root (before link)
    int link_len;
} dp_state_t;

bool espure_morph_split(const char* word, char* out_buffer, size_t max_len) {
    size_t n = strlen(word);
    if (n < 5 || max_len < n + 10) {
        strlcpy(out_buffer, word, max_len);
        return false;
    }

    dp_state_t dp[128];
    for (size_t i = 0; i <= n; i++) {
        dp[i].score = INF_SCORE;
        dp[i].next_i = -1;
        dp[i].root_end = -1;
        dp[i].link_len = 0;
    }
    
    dp[n].score = 0;

    for (int i = (int)n - 1; i >= 0; i--) {
        int best_score = INF_SCORE;
        int best_next = -1;
        int best_rend = -1;
        int best_link = 0;
        
        for (int j = i + 1; j <= (int)n; j++) {
            int root_len = j - i;
            bool is_valid = false;
            
            if (root_len >= 3) {
                is_valid = is_valid_root(word + i, root_len) || 
                           (j == n && is_in_array(terminal_suffixes, num_term_suffixes, word + i, root_len));
            } else if (root_len == 2) {
                is_valid = is_in_array(prefixes, num_prefixes, word + i, root_len) ||
                           is_in_array(suffixes, num_suffixes, word + i, root_len) ||
                           (j == n && is_in_array(terminal_suffixes, num_term_suffixes, word + i, root_len)) ||
                           is_in_array(short_roots, num_short_roots, word + i, root_len);
            } else if (root_len == 1) {
                is_valid = (j == n && is_in_array(terminal_suffixes, num_term_suffixes, word + i, root_len));
            }
            
            if (is_valid) {
                if (j == n) {
                    int score = (j - i) * (j - i);
                    if (score > best_score) {
                        best_score = score;
                        best_next = n;
                        best_rend = n;
                        best_link = 0;
                    }
                } else {
                    for (size_t l = 0; l < num_links; l++) {
                        const char* link = linking_elements[l];
                        size_t link_len = strlen(link);
                        if (j + link_len <= n) {
                            bool link_match = true;
                            for (size_t k = 0; k < link_len; k++) {
                                if (byte_tolower((uint8_t)word[j + k]) != link[k]) {
                                    link_match = false;
                                    break;
                                }
                            }
                            if (link_match) {
                                int next_i = j + link_len;
                                if (dp[next_i].score != INF_SCORE) {
                                    int score = (j - i) * (j - i) - 10 + dp[next_i].score;
                                    if (score > best_score) {
                                        best_score = score;
                                        best_next = next_i;
                                        best_rend = j;
                                        best_link = link_len;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        dp[i].score = best_score;
        dp[i].next_i = best_next;
        dp[i].root_end = best_rend;
        dp[i].link_len = best_link;
    }
    
    if (dp[0].score != INF_SCORE && dp[0].next_i != n) {
        // We found a split of more than 1 part.
        // Reconstruct string using Compound Split logic.
        char result[256];
        size_t res_len = 0;
        bool prev_was_end = false;
        
        size_t curr = 0;
        while (curr < n) {
            int rend = dp[curr].root_end;
            int next_i = dp[curr].next_i;
            
            bool is_prefix = is_in_array(prefixes, num_prefixes, word + curr, rend - curr);
            bool is_suffix = is_in_array(suffixes, num_suffixes, word + curr, rend - curr) ||
                             is_in_array(terminal_suffixes, num_term_suffixes, word + curr, rend - curr);
            bool is_root = !is_prefix && !is_suffix;
            
            if (res_len > 0) {
                if (prev_was_end && !is_suffix) {
                    if (res_len < sizeof(result) - 1) {
                        result[res_len++] = '-';
                    }
                }
            }
            
            size_t part_len = next_i - curr;
            if (res_len + part_len < sizeof(result)) {
                memcpy(result + res_len, word + curr, part_len);
                res_len += part_len;
            }
            
            bool is_inseparable = is_in_array(inseparable_prefixes, num_inseparable, word + curr, rend - curr);
            
            if (is_root || is_suffix || (is_prefix && !is_inseparable)) {
                prev_was_end = true;
            } else {
                prev_was_end = false;
            }
            
            curr = next_i;
        }
        
        result[res_len] = '\0';
        
        // If the result actually contains a hyphen, we copy it and return true.
        if (strchr(result, '-')) {
            strlcpy(out_buffer, result, max_len);
            return true;
        }
    }
    
    strlcpy(out_buffer, word, max_len);
    return false;
}
