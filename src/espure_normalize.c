/**
 * @file espure_normalize.c
 * @brief Stage 1: Text Normalization Engine implementation
 * 
 * Language-independent normalization pipeline:
 * - Locale dispatch via espure_normalize_get_ops()
 * - Abbreviation expansion
 * - Number-to-words expansion
 * - Currency/symbol expansion
 * - Punctuation isolation and UTF-8 sanitization
 */

#include "espure_normalize.h"
#include "espure_internal.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef CONFIG_ESPURE_LOCALE_DE_DE
extern const espure_normalize_ops_t ESPURE_NORMALIZE_DE_DE;
#endif

#ifdef CONFIG_ESPURE_LOCALE_EN_US
extern const espure_normalize_ops_t ESPURE_NORMALIZE_EN_US;
#endif

const espure_normalize_ops_t* espure_normalize_get_ops(const char *locale) {
    if (!locale) return NULL;
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    if (strcmp(locale, "de-DE") == 0 || strcmp(locale, "de") == 0) {
        return &ESPURE_NORMALIZE_DE_DE;
    }
#endif
#ifdef CONFIG_ESPURE_LOCALE_EN_US
    if (strcmp(locale, "en-US") == 0 || strcmp(locale, "en") == 0) {
        return &ESPURE_NORMALIZE_EN_US;
    }
#endif
    return NULL;
}

static bool is_word_punct(char c) {
    return (c == '.' || c == ',' || c == '!' || c == '?' ||
            c == ':' || c == ';' || c == '"' || c == '\'' ||
            c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}');
}

espure_err_t espure_normalize_text(const char *locale,
                                   const char *input,
                                   char *output,
                                   size_t out_size) {
    if (!input || !output || out_size == 0) return ESPURE_ERR_INVALID_ARG;
    output[0] = '\0';

    const espure_normalize_ops_t *ops = espure_normalize_get_ops(locale);
    if (!ops) {
        /* Passthrough if no custom normalization ops for locale */
        strlcpy(output, input, out_size);
        return ESPURE_OK;
    }

    const char *p = input;
    size_t out_pos = 0;

    while (*p) {
        /* Skip leading whitespace between tokens */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        if (!*p) break;

        /* 1. Check for locale-specific symbol expansions */
        if (ops->expand_symbol) {
            char sym_buf[64];
            size_t consumed = 0;
            if (ops->expand_symbol(p, sym_buf, sizeof(sym_buf), &consumed) == ESPURE_OK && consumed > 0) {
                if (out_pos > 0 && output[out_pos - 1] != ' ') {
                    if (out_pos + 1 >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                    output[out_pos++] = ' ';
                }
                size_t rlen = strlen(sym_buf);
                if (out_pos + rlen >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                memcpy(output + out_pos, sym_buf, rlen);
                out_pos += rlen;
                p += consumed;
                continue;
            }
        }

        /* 2. Check for multi-word or single-word abbreviations */
        bool matched_abbrev = false;
        if (ops->abbrevs && ops->num_abbrevs > 0) {
            for (size_t a = 0; a < ops->num_abbrevs; a++) {
                size_t alen = strlen(ops->abbrevs[a].abbrev);
                bool match = false;
                if (ops->abbrevs[a].case_sensitive) {
                    match = (strncmp(p, ops->abbrevs[a].abbrev, alen) == 0);
                } else {
#ifdef _MSC_VER
                    match = (_strnicmp(p, ops->abbrevs[a].abbrev, alen) == 0);
#else
                    match = (strncasecmp(p, ops->abbrevs[a].abbrev, alen) == 0);
#endif
                }

                if (match) {
                    char next = p[alen];
                    if (next == '\0' || next == ' ' || next == '\t' || next == '\n' || next == '\r' || is_word_punct(next)) {
                        if (out_pos > 0 && output[out_pos - 1] != ' ') {
                            if (out_pos + 1 >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                            output[out_pos++] = ' ';
                        }
                        size_t explen = strlen(ops->abbrevs[a].expansion);
                        if (out_pos + explen >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                        memcpy(output + out_pos, ops->abbrevs[a].expansion, explen);
                        out_pos += explen;
                        p += alen;
                        matched_abbrev = true;
                        break;
                    }
                }
            }
        }
        if (matched_abbrev) continue;

        /* 3. Check for integer numbers */
        if (isdigit((unsigned char)*p) && ops->number_to_words) {
            int64_t val = 0;
            const char *num_start = p;
            while (isdigit((unsigned char)*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }

            char num_words[128];
            if (ops->number_to_words(val, num_words, sizeof(num_words)) == ESPURE_OK) {
                if (out_pos > 0 && output[out_pos - 1] != ' ') {
                    if (out_pos + 1 >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                    output[out_pos++] = ' ';
                }
                size_t wlen = strlen(num_words);
                if (out_pos + wlen >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
                memcpy(output + out_pos, num_words, wlen);
                out_pos += wlen;
                continue;
            } else {
                p = num_start;
            }
        }

        /* 4. Normal word token - isolate word from surrounding quotes/brackets */
        if (out_pos > 0 && output[out_pos - 1] != ' ') {
            if (out_pos + 1 >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
            output[out_pos++] = ' ';
        }

        /* Skip leading non-letter punctuation */
        while (*p && (*p == '"' || *p == '\'' || *p == '(' || *p == '[' || *p == '{' ||
                      (unsigned char)*p == 0xC2 || (unsigned char)*p == 0xE2)) {
            if ((unsigned char)p[0] == 0xC2 && ((unsigned char)p[1] == 0xAB || (unsigned char)p[1] == 0xBB)) {
                p += 2; /* « or » */
            } else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80) {
                p += 3; /* „ or “ or ” */
            } else if (*p == '"' || *p == '\'' || *p == '(' || *p == '[' || *p == '{') {
                p++;
            } else {
                break;
            }
        }

        const char *w_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
        const char *w_end = p;

        /* Strip trailing sentence punctuation and typographic quotes from word */
        while (w_end > w_start) {
            if (is_word_punct(*(w_end - 1))) {
                w_end--;
            } else if (w_end - w_start >= 3 &&
                       (unsigned char)*(w_end - 3) == 0xE2 &&
                       (unsigned char)*(w_end - 2) == 0x80 &&
                       ((unsigned char)*(w_end - 1) >= 0x98 && (unsigned char)*(w_end - 1) <= 0x9F)) {
                w_end -= 3; /* Strip trailing ’, “, ”, etc. */
            } else if (w_end - w_start >= 2 &&
                       (unsigned char)*(w_end - 2) == 0xC2 &&
                       ((unsigned char)*(w_end - 1) == 0xAB || (unsigned char)*(w_end - 1) == 0xBB)) {
                w_end -= 2; /* Strip trailing « or » */
            } else {
                break;
            }
        }

        /* Copy word token with unicode apostrophe and loanword accent normalization */
        const char *wp = w_start;
        while (wp < w_end) {
            if (out_pos + 4 >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;

            if (wp + 3 <= w_end && (unsigned char)wp[0] == 0xE2 && (unsigned char)wp[1] == 0x80 &&
                ((unsigned char)wp[2] == 0x98 || (unsigned char)wp[2] == 0x99 ||
                 (unsigned char)wp[2] == 0x9A || (unsigned char)wp[2] == 0x9B)) {
                /* Normalize curly apostrophe inside word (e.g. Ku'damm) to ASCII ' */
                output[out_pos++] = '\'';
                wp += 3;
            } else if (wp + 2 <= w_end && (unsigned char)wp[0] == 0xC3) {
                unsigned char c2 = (unsigned char)wp[1];
                /* Keep German umlauts and sharp s intact: ä (0xA4), ö (0xB6), ü (0xBC), ß (0x9F), Ä (0x84), Ö (0x96), Ü (0x9C) */
                if (c2 == 0xA4 || c2 == 0xB6 || c2 == 0xBC || c2 == 0x9F ||
                    c2 == 0x84 || c2 == 0x96 || c2 == 0x9C) {
                    output[out_pos++] = *wp++;
                    output[out_pos++] = *wp++;
                } else if (c2 == 0xA8 || c2 == 0xA9 || c2 == 0xAA || c2 == 0xAB) {
                    /* è, é, ê, ë -> e */
                    output[out_pos++] = 'e';
                    wp += 2;
                } else if (c2 == 0x88 || c2 == 0x89 || c2 == 0x8A || c2 == 0x8B) {
                    /* È, É, Ê, Ë -> E */
                    output[out_pos++] = 'E';
                    wp += 2;
                } else if (c2 == 0xA0 || c2 == 0xA1 || c2 == 0xA2 || c2 == 0xA3) {
                    /* à, á, â, ã -> a */
                    output[out_pos++] = 'a';
                    wp += 2;
                } else if (c2 == 0x80 || c2 == 0x81 || c2 == 0x82 || c2 == 0x83) {
                    /* À, Á, Â, Ã -> A */
                    output[out_pos++] = 'A';
                    wp += 2;
                } else if (c2 == 0xB2 || c2 == 0xB3 || c2 == 0xB4 || c2 == 0xB5) {
                    /* ò, ó, ô, õ -> o */
                    output[out_pos++] = 'o';
                    wp += 2;
                } else if (c2 == 0x92 || c2 == 0x93 || c2 == 0x94 || c2 == 0x95) {
                    /* Ò, Ó, Ô, Õ -> O */
                    output[out_pos++] = 'O';
                    wp += 2;
                } else if (c2 == 0xB9 || c2 == 0xBA || c2 == 0xBB) {
                    /* ù, ú, û -> u */
                    output[out_pos++] = 'u';
                    wp += 2;
                } else if (c2 == 0x99 || c2 == 0x9A || c2 == 0x9B) {
                    /* Ù, Ú, Û -> U */
                    output[out_pos++] = 'U';
                    wp += 2;
                } else if (c2 == 0xAC || c2 == 0xAD || c2 == 0xAE || c2 == 0xAF) {
                    /* ì, í, î, ï -> i */
                    output[out_pos++] = 'i';
                    wp += 2;
                } else if (c2 == 0x8C || c2 == 0x8D || c2 == 0x8E || c2 == 0x8F) {
                    /* Ì, Í, Î, Ï -> I */
                    output[out_pos++] = 'I';
                    wp += 2;
                } else if (c2 == 0xA7) {
                    /* ç -> c */
                    output[out_pos++] = 'c';
                    wp += 2;
                } else if (c2 == 0x87) {
                    /* Ç -> C */
                    output[out_pos++] = 'C';
                    wp += 2;
                } else {
                    output[out_pos++] = *wp++;
                    output[out_pos++] = *wp++;
                }
            } else {
                output[out_pos++] = *wp++;
            }
        }
    }

    output[out_pos] = '\0';
    return ESPURE_OK;
}
