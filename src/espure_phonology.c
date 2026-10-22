/**
 * @file espure_phonology.c
 * @brief Native phonological rules engine for deterministic sound changes
 * 
 * Implements:
 * - Auslautverhärtung (Final Devoicing of voiced obstruents in syllable codas)
 * - Suffix -ig Spirantization and Dissimilation
 * - Ich-Laut vs. Ach-Laut Distribution
 * - Coda-r Vocalization (Tiefschwa [ɐ])
 */

#include "espure_phonology.h"
#include "espure_internal.h"
#include <string.h>
#include <stdbool.h>
#include <esp_log.h>


#define MAX_TOKENS 128
#define MAX_TOKEN_LEN 16

static bool is_stress_mark(const char* t) {
    if (!t) return false;
    return (strcmp(t, "'") == 0 || strcmp(t, ",") == 0 || strcmp(t, "%") == 0);
}

static bool is_boundary_mark(const char* t) {
    if (!t) return false;
    return (strcmp(t, "=") == 0 || strcmp(t, "_") == 0 ||
            strcmp(t, "_|") == 0 || strcmp(t, ".") == 0 ||
            strcmp(t, "-") == 0);
}

static bool is_stress_or_boundary(const char* t) {
    return is_stress_mark(t) || is_boundary_mark(t);
}

static bool is_valid_german_voiced_onset_cluster(const char* c1, const char* c2) {
    if (!c1 || !c2) return false;
    if (strcmp(c1, "b") == 0) {
        return (strcmp(c2, "r") == 0 || strcmp(c2, "l") == 0);
    }
    if (strcmp(c1, "d") == 0) {
        return (strcmp(c2, "r") == 0);
    }
    if (strcmp(c1, "g") == 0) {
        return (strcmp(c2, "r") == 0 || strcmp(c2, "l") == 0 || strcmp(c2, "n") == 0);
    }
    if (strcmp(c1, "v") == 0) {
        return (strcmp(c2, "r") == 0);
    }
    return false;
}

static bool is_german_vowel(const char* t) {
    if (!t) return false;
    while (*t == '\'' || *t == ',' || *t == '%' || *t == '=') t++;
    static const char* const VOWELS[] = {
        "a", "a:", "A", "A:", "e", "e:", "E", "E:",
        "i", "i:", "I", "o", "o:", "O", "O:",
        "u", "u:", "U", "y", "y:", "Y",
        "2", "2:", "9", "9:", "W", "3", "3:",
        "@", "@-", "@n", "@st", "@r", "@m", "@l",
        "aI", "aU", "OY", "eI", "oU", "@U", "6"
    };
    for (size_t i = 0; i < sizeof(VOWELS) / sizeof(VOWELS[0]); i++) {
        if (strcmp(t, VOWELS[i]) == 0) return true;
    }
    return false;
}

static bool is_german_front_vowel(const char* t) {
    if (!t) return false;
    while (*t == '\'' || *t == ',' || *t == '%' || *t == '=') t++;
    static const char* const FRONT_VOWELS[] = {
        "i", "i:", "I", "e", "e:", "E", "E:",
        "y", "y:", "Y", "2", "2:", "9", "9:",
        "W", "aI", "OY", "eI"
    };
    for (size_t i = 0; i < sizeof(FRONT_VOWELS) / sizeof(FRONT_VOWELS[0]); i++) {
        if (strcmp(t, FRONT_VOWELS[i]) == 0) return true;
    }
    return false;
}

static bool is_german_long_vowel(const char* t) {
    if (!t) return false;
    while (*t == '\'' || *t == ',' || *t == '%' || *t == '=') t++;
    static const char* const LONG_VOWELS[] = {
        "a:", "A:", "e:", "E:", "i:", "o:", "O:",
        "u:", "y:", "2:", "9:", "3:"
    };
    for (size_t i = 0; i < sizeof(LONG_VOWELS) / sizeof(LONG_VOWELS[0]); i++) {
        if (strcmp(t, LONG_VOWELS[i]) == 0) return true;
    }
    return false;
}

static bool is_liquid_or_nasal(const char* t) {
    if (!t) return false;
    return (strcmp(t, "l") == 0 || strcmp(t, "r") == 0 ||
            strcmp(t, "m") == 0 || strcmp(t, "n") == 0 ||
            strcmp(t, "N") == 0);
}

/**
 * @brief Apply German phonological sound change rules
 */
static espure_err_t apply_german_phonology(char tokens[MAX_TOKENS][MAX_TOKEN_LEN], size_t *token_count) {
    size_t n = *token_count;
    if (n == 0) return ESPURE_OK;

    /* Pass 0: Glottal Stop and Stress Alignment
     * - Word-initial glottal stops (_!, !, ?) are omitted in standard German IPA
     * - Internal glottal stops before stressed syllables must follow the stress marker:
     *   (_! followed by ' or , becomes , followed by _!)
     */
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tokens[i], "_!") == 0 || strcmp(tokens[i], "!") == 0 || strcmp(tokens[i], "?") == 0) {
            bool has_prev_sound = false;
            for (int p = (int)i - 1; p >= 0; p--) {
                if (!is_stress_or_boundary(tokens[p])) {
                    has_prev_sound = true;
                    break;
                }
            }

            if (!has_prev_sound) {
                /* Word-initial glottal stop is silent / omitted in standard German IPA */
                for (size_t k = i; k + 1 < n; k++) {
                    strlcpy(tokens[k], tokens[k + 1], MAX_TOKEN_LEN);
                }
                n--;
                i--;
                continue;
            }

            /* Morpheme-internal glottal stop: if followed by stress, swap order so stress marker
             * precedes the glottal stop and downgrade compound primary stress to secondary stress (',') */
            if (i + 1 < n && is_stress_mark(tokens[i + 1])) {
                strlcpy(tokens[i], ",", MAX_TOKEN_LEN);
                strlcpy(tokens[i + 1], "_!", MAX_TOKEN_LEN);
            }
        }
    }

    /* Pass 1: Ich-Laut vs. Ach-Laut (/x/ -> [ç] after front vowels, liquids, nasals) */
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tokens[i], "x") == 0) {
            /* Look back to find preceding sound (skipping stress/boundary markers) */
            int prev = (int)i - 1;
            while (prev >= 0 && is_stress_or_boundary(tokens[prev])) {
                prev--;
            }
            if (prev >= 0) {
                if (is_german_front_vowel(tokens[prev]) || is_liquid_or_nasal(tokens[prev])) {
                    strlcpy(tokens[i], "C", MAX_TOKEN_LEN);
                }
            }
        }
    }

    /* Pass 2: Suffix -ig Spirantization & Dissimilation */
    for (size_t i = 1; i < n; i++) {
        if (strcmp(tokens[i - 1], "I") == 0 && strcmp(tokens[i], "g") == 0) {
            /* Check what follows 'g' */
            size_t next = i + 1;
            while (next < n && is_stress_or_boundary(tokens[next])) {
                next++;
            }
            if (next >= n) {
                /* End of word: König -> [køːnɪç], wenig -> [veːnɪç] */
                strlcpy(tokens[i], "C", MAX_TOKEN_LEN);
            } else if (strcmp(tokens[next], "l") == 0) {
                /* Before suffix -lich: königlich -> [køːnɪklɪç] (dissimilation) */
                strlcpy(tokens[i], "k", MAX_TOKEN_LEN);
            } else if (!is_german_vowel(tokens[next])) {
                /* Before consonant: wichtigste -> [vɪçtɪçstə] */
                strlcpy(tokens[i], "C", MAX_TOKEN_LEN);
            }
            /* Note: If next is a vowel (e.g. Könige: I g @), plosive [ɡ] is preserved! */
        }
    }

    /* Pass 3: Coda-r Vokalisation (/ʁ/ -> [ɐ] = "6" in syllable coda) */
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tokens[i], "@r") == 0) {
            /* Unstressed @r is always vocalized tiefschwa [ɐ] in German */
            strlcpy(tokens[i], "6", MAX_TOKEN_LEN);
            continue;
        }
        if (strcmp(tokens[i], "r") == 0) {
            /* Check what follows 'r' */
            size_t next = i + 1;
            while (next < n && is_stress_or_boundary(tokens[next])) {
                next++;
            }
            bool is_coda = (next >= n) || (!is_german_vowel(tokens[next]));

            if (is_coda) {
                int prev = (int)i - 1;
                while (prev >= 0 && is_stress_or_boundary(tokens[prev])) {
                    prev--;
                }
                if (prev >= 0) {
                    if (strcmp(tokens[prev], "@") == 0 || strcmp(tokens[prev], "@-") == 0) {
                        /* Unstressed schwa + r (Mutter -> [mʊtɐ], Kinder -> [kɪndɐ]) */
                        /* Replace "@" with "6" and remove "r" */
                        strlcpy(tokens[prev], "6", MAX_TOKEN_LEN);
                        /* Shift remaining tokens left to delete token i */
                        for (size_t k = i; k + 1 < n; k++) {
                            strlcpy(tokens[k], tokens[k + 1], MAX_TOKEN_LEN);
                        }
                        n--;
                        i--;
                    } else if (is_german_long_vowel(tokens[prev]) || is_german_vowel(tokens[prev]) ||
                               (strcmp(tokens[prev], ":") == 0 && prev > 0 && is_german_vowel(tokens[prev - 1]))) {
                        /* Postvocalic r after long/short vowel (Uhr -> [uːɐ̯], sehr -> [zeːɐ̯]) */
                        strlcpy(tokens[i], "6", MAX_TOKEN_LEN);
                    }
                }
            }
        }
    }

    /* Pass 4: Auslautverhärtung (Final Devoicing of voiced obstruents in coda) */
    for (size_t i = 0; i < n; i++) {
        const char* t = tokens[i];
        const char* devoiced = NULL;

        if (strcmp(t, "b") == 0) devoiced = "p";
        else if (strcmp(t, "d") == 0) devoiced = "t";
        else if (strcmp(t, "g") == 0) devoiced = "k";
        else if (strcmp(t, "v") == 0) devoiced = "f";
        else if (strcmp(t, "z") == 0) devoiced = "s";

        if (devoiced) {
            /* Check 1: Syllable onset without preceding vowel in this morpheme */
            bool has_prev_vowel = false;
            int prev = (int)i - 1;
            while (prev >= 0) {
                if (is_boundary_mark(tokens[prev])) {
                    break;
                }
                if (is_german_vowel(tokens[prev])) {
                    has_prev_vowel = true;
                    break;
                }
                prev--;
            }

            if (!has_prev_vowel) {
                /* Consonant precedes the first vowel of the morpheme/word -> ONSET! */
                continue;
            }

            /* Check 2: Simplex onset directly before a vowel (skipping stress marks) */
            size_t next = i + 1;
            while (next < n && is_stress_mark(tokens[next])) {
                next++;
            }

            if (next < n && is_german_vowel(tokens[next])) {
                /* Followed directly by a vowel -> ONSET! */
                continue;
            }

            /* Check 3: Complex onset cluster (Maximal Onset Principle) */
            if (next < n && !is_boundary_mark(tokens[next])) {
                const char* c2 = tokens[next];
                if (is_valid_german_voiced_onset_cluster(t, c2)) {
                    /* Check if c2 is followed by a vowel */
                    size_t after_c2 = next + 1;
                    while (after_c2 < n && is_stress_mark(tokens[after_c2])) {
                        after_c2++;
                    }
                    if (after_c2 < n && is_german_vowel(tokens[after_c2])) {
                        /* c1 + c2 form a valid onset cluster before vowel -> ONSET! */
                        continue;
                    }
                }
            }

            /* If not an onset, the obstruent is in the syllable CODA -> Auslautverhärtung */
            strlcpy(tokens[i], devoiced, MAX_TOKEN_LEN);
        }
    }

    *token_count = n;
    return ESPURE_OK;
}

espure_err_t espure_phonology_apply(const char *locale, char *phonemes, size_t buf_size) {
    if (!locale || !phonemes || buf_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }

    /* Only de-DE has active phonological rules in this phase */
    if (strcmp(locale, "de-DE") != 0) {
        return ESPURE_OK;
    }

    /* Tokenize space-separated phoneme mnemonics */
    char tokens[MAX_TOKENS][MAX_TOKEN_LEN];
    size_t token_count = 0;

    const char *p = phonemes;
    while (*p && token_count < MAX_TOKENS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        size_t len = p - start;

        if (len >= MAX_TOKEN_LEN) len = MAX_TOKEN_LEN - 1;
        memcpy(tokens[token_count], start, len);
        tokens[token_count][len] = '\0';
        token_count++;
    }

    if (token_count == 0) {
        return ESPURE_OK;
    }

    /* Apply linguistic rules */
    espure_err_t err = apply_german_phonology(tokens, &token_count);
    if (err != ESPURE_OK) {
        return err;
    }

    /* Reconstruct spaced string */
    size_t pos = 0;
    for (size_t i = 0; i < token_count; i++) {
        size_t tlen = strlen(tokens[i]);
        if (pos + tlen + (i > 0 ? 1 : 0) + 1 > buf_size) {
            phonemes[0] = '\0';
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
        if (i > 0) {
            phonemes[pos++] = ' ';
        }
        memcpy(phonemes + pos, tokens[i], tlen);
        pos += tlen;
    }
    phonemes[pos] = '\0';

    return ESPURE_OK;
}
