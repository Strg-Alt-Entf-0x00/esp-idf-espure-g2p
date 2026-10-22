#include "espure_morph.h"
#include "espure_internal.h"

#include <string.h>

#define INF_SCORE (-1000000)

typedef struct {
    int score;
    int next_i;
    int root_end;
    int link_len;
} dp_state_t;

static const espure_morph_language_data_t* get_language_data(const char* lang) {
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    if (lang && strcmp(lang, "de-DE") == 0) {
        return espure_morph_data_de_DE();
    }
#else
    (void)lang;
#endif
    return NULL;
}

static uint8_t byte_tolower(uint8_t byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (uint8_t)(byte + ('a' - 'A'));
    }
    return byte;
}

static bool is_valid_root(const espure_morph_language_data_t* data,
                          const char* str, size_t len) {
    if (len == 0 || data->root_trie_count == 0) {
        return false;
    }

    uint32_t state = 0;
    for (size_t i = 0; i < len; i++) {
        if (state >= data->root_trie_count) {
            return false;
        }

        const espure_morph_trie_node_t* node = &data->root_trie[state];
        uint32_t next_state = node->next_state;
        uint16_t child_count = node->child_count;
        if (next_state > data->root_trie_count ||
            child_count > data->root_trie_count - next_state) {
            return false;
        }

        bool found = false;
        uint8_t byte = byte_tolower((uint8_t)str[i]);
        for (uint16_t j = 0; j < child_count; j++) {
            if (data->root_trie[next_state + j].byte_val == byte) {
                state = next_state + j;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return state < data->root_trie_count && data->root_trie[state].is_end;
}

static bool is_in_set(const espure_morph_string_set_t* set,
                      const char* str, size_t len) {
    for (size_t i = 0; i < set->count; i++) {
        const char* candidate = set->items[i];
        if (strlen(candidate) != len) {
            continue;
        }

        bool match = true;
        for (size_t j = 0; j < len; j++) {
            if (byte_tolower((uint8_t)str[j]) != (uint8_t)candidate[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

bool espure_morph_split(const char* lang, const char* word,
                        char* out_buffer, size_t max_len) {
    if (!word || !out_buffer || max_len == 0) {
        return false;
    }

    const espure_morph_language_data_t* data = get_language_data(lang);
    if (!data) {
        strlcpy(out_buffer, word, max_len);
        return false;
    }

    size_t n = strlen(word);
    if (n >= 128 || n < 5 || max_len < n + 10) {
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
        int best_root_end = -1;
        int best_link_len = 0;

        for (int j = i + 1; j <= (int)n; j++) {
            int root_len = j - i;
            bool valid = false;
            if (root_len >= 3) {
                valid = is_valid_root(data, word + i, (size_t)root_len) ||
                        is_in_set(&data->medial_candidates, word + i, (size_t)root_len) ||
                        (j == (int)n &&
                         is_in_set(&data->terminal_candidates, word + i, (size_t)root_len));
            } else if (root_len == 2) {
                valid = is_in_set(&data->initial_candidates, word + i, 2) ||
                        is_in_set(&data->medial_candidates, word + i, 2) ||
                        (j == (int)n &&
                         is_in_set(&data->terminal_candidates, word + i, 2)) ||
                        is_in_set(&data->short_roots, word + i, 2);
            } else if (root_len == 1) {
                valid = j == (int)n &&
                        is_in_set(&data->terminal_candidates, word + i, 1);
            }

            if (!valid) {
                continue;
            }

            if (j == (int)n) {
                int score = root_len * root_len;
                if (score > best_score) {
                    best_score = score;
                    best_next = (int)n;
                    best_root_end = (int)n;
                    best_link_len = 0;
                }
                continue;
            }

            for (size_t l = 0; l < data->linking_elements.count; l++) {
                const char* link = data->linking_elements.items[l];
                size_t link_len = strlen(link);
                if (link_len > n - (size_t)j) {
                    continue;
                }

                bool link_matches = true;
                for (size_t k = 0; k < link_len; k++) {
                    if (byte_tolower((uint8_t)word[(size_t)j + k]) !=
                        (uint8_t)link[k]) {
                        link_matches = false;
                        break;
                    }
                }
                if (!link_matches) {
                    continue;
                }

                int next_i = (int)((size_t)j + link_len);
                if (dp[next_i].score == INF_SCORE) {
                    continue;
                }

                int score = root_len * root_len - 10 + dp[next_i].score;
                if (score > best_score) {
                    best_score = score;
                    best_next = next_i;
                    best_root_end = j;
                    best_link_len = (int)link_len;
                }
            }
        }
        dp[i].score = best_score;
        dp[i].next_i = best_next;
        dp[i].root_end = best_root_end;
        dp[i].link_len = best_link_len;
    }

    if (dp[0].score != INF_SCORE && dp[0].next_i != (int)n) {
        char result[256];
        size_t result_len = 0;
        bool previous_was_end = false;
        size_t current = 0;

        while (current < n) {
            int root_end = dp[current].root_end;
            int next_i = dp[current].next_i;
            size_t root_len = (size_t)(root_end - (int)current);

            bool is_prefix = is_in_set(&data->initial_candidates,
                                       word + current, root_len);
            bool is_suffix =
                is_in_set(&data->medial_candidates, word + current, root_len) ||
                is_in_set(&data->terminal_candidates, word + current, root_len);
            bool is_root = !is_prefix && !is_suffix;

            if (result_len > 0 && previous_was_end && !is_suffix &&
                result_len < sizeof(result) - 1) {
                result[result_len++] = '-';
            }

            size_t part_len = (size_t)(next_i - (int)current);
            if (result_len + part_len >= sizeof(result)) {
                break;
            }
            memcpy(result + result_len, word + current, part_len);
            result_len += part_len;

            bool is_inseparable = is_in_set(&data->inseparable_prefixes,
                                            word + current, root_len);
            previous_was_end =
                is_root || is_suffix || (is_prefix && !is_inseparable);
            current = (size_t)next_i;
        }
        result[result_len] = '\0';

        if (strchr(result, '-') && result_len < max_len) {
            strlcpy(out_buffer, result, max_len);
            return true;
        }
    }

    strlcpy(out_buffer, word, max_len);
    return false;
}
