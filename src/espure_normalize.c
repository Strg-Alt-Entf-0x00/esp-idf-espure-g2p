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

#ifdef CONFIG_ESPURE_LOCALE_EN_GB
extern const espure_normalize_ops_t ESPURE_NORMALIZE_EN_GB;
#endif

const espure_normalize_ops_t* espure_normalize_get_ops(const char *locale) {
    if (!locale) return NULL;
#ifdef CONFIG_ESPURE_LOCALE_DE_DE
    if (strcmp(locale, "de-DE") == 0 || strcmp(locale, "de") == 0) {
        return &ESPURE_NORMALIZE_DE_DE;
    }
#endif
#ifdef CONFIG_ESPURE_LOCALE_EN_GB
    if (strcmp(locale, "en-GB") == 0 || strcmp(locale, "en") == 0) {
        return &ESPURE_NORMALIZE_EN_GB;
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

        /* Strip trailing sentence punctuation from word */
        while (w_end > w_start && is_word_punct(*(w_end - 1))) {
            w_end--;
        }

        size_t wlen = w_end - w_start;
        if (wlen > 0) {
            if (out_pos + wlen >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
            memcpy(output + out_pos, w_start, wlen);
            out_pos += wlen;
        }
    }

    output[out_pos] = '\0';
    return ESPURE_OK;
}
