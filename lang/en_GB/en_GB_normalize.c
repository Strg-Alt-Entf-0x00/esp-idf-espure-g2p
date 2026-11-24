/**
 * @file en_GB_normalize.c
 * @brief British English (en-GB) Text Normalization Pack
 * 
 * Contains English abbreviation expansion rules, cardinal number-to-words
 * converters (0..999,999), and British English currency/symbol mappings.
 */

#include "espure_normalize.h"
#include "espure_internal.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* ========================================================================
 * English Abbreviation Tables
 * ======================================================================== */

static const espure_abbrev_entry_t ENGLISH_ABBREVS[] = {
    {"Dr.", "Doctor", false},
    {"Mr.", "Mister", false},
    {"Mrs.", "Missus", false},
    {"Ms.", "Miz", false},
    {"Prof.", "Professor", false},
    {"St.", "Saint", false},
    {"e.g.", "for example", false},
    {"i.e.", "that is", false},
    {"etc.", "et cetera", false},
    {"vs.", "versus", false},
    {"mph", "miles per hour", false},
};

/* ========================================================================
 * English Cardinal Number Converter (0 to 999,999)
 * ======================================================================== */

static const char* const EN_UNITS[] = {
    "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"
};

static const char* const EN_TEENS[] = {
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen"
};

static const char* const EN_TENS[] = {
    "", "", "twenty", "thirty", "forty", "fifty",
    "sixty", "seventy", "eighty", "ninety"
};

static espure_err_t sub_100_to_english(int n, char *out, size_t out_size) {
    if (n < 0 || n >= 100) return ESPURE_ERR_INVALID_ARG;
    if (n == 0) return ESPURE_OK;

    if (n < 10) {
        return (strlcat(out, EN_UNITS[n], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }
    if (n < 20) {
        return (strlcat(out, EN_TEENS[n - 10], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    int unit = n % 10;
    int ten = n / 10;

    if (unit == 0) {
        return (strlcat(out, EN_TENS[ten], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%s-%s", EN_TENS[ten], EN_UNITS[unit]);
    return (strlcat(out, buf, out_size) < out_size) ?
           ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
}

static espure_err_t sub_1000_to_english(int n, char *out, size_t out_size) {
    if (n < 0 || n >= 1000) return ESPURE_ERR_INVALID_ARG;
    if (n == 0) return ESPURE_OK;

    int hundred = n / 100;
    int rem = n % 100;

    if (hundred > 0) {
        if (strlcat(out, EN_UNITS[hundred], out_size) >= out_size ||
            strlcat(out, " hundred", out_size) >= out_size) {
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
        if (rem > 0) {
            if (strlcat(out, " and ", out_size) >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
    }

    if (rem > 0) {
        return sub_100_to_english(rem, out, out_size);
    }
    return ESPURE_OK;
}

espure_err_t espure_number_to_english_words(int64_t number, char *output, size_t out_size) {
    if (!output || out_size == 0) return ESPURE_ERR_INVALID_ARG;
    output[0] = '\0';

    if (number == 0) {
        strlcpy(output, "zero", out_size);
        return ESPURE_OK;
    }

    if (number < 0) {
        if (strlcat(output, "minus ", out_size) >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
        number = -number;
    }

    if (number > 999999) {
        return ESPURE_ERR_NOT_SUPPORTED;
    }

    int thousand = (int)(number / 1000);
    int rem = (int)(number % 1000);

    if (thousand > 0) {
        espure_err_t err = sub_1000_to_english(thousand, output, out_size);
        if (err != ESPURE_OK) return err;
        if (strlcat(output, " thousand", out_size) >= out_size) {
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
        if (rem > 0) {
            if (strlcat(output, " ", out_size) >= out_size) return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
    }

    if (rem > 0) {
        return sub_1000_to_english(rem, output, out_size);
    }

    return ESPURE_OK;
}

/* ========================================================================
 * English Symbol Expansion (£, $, €, %)
 * ======================================================================== */

static espure_err_t en_GB_expand_symbol(const char *sym, char *output, size_t out_size, size_t *bytes_consumed) {
    if (!sym || !output || !bytes_consumed || out_size == 0) return ESPURE_ERR_INVALID_ARG;

    /* Pound: £ (0xC2 0xA3) */
    if ((unsigned char)sym[0] == 0xC2 && (unsigned char)sym[1] == 0xA3) {
        *bytes_consumed = 2;
        return (strlcpy(output, "Pound", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    /* Dollar: $ (0x24) */
    if (sym[0] == '$') {
        *bytes_consumed = 1;
        return (strlcpy(output, "Dollar", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    /* Euro: € (0xE2 0x82 0xAC) */
    if ((unsigned char)sym[0] == 0xE2 && (unsigned char)sym[1] == 0x82 && (unsigned char)sym[2] == 0xAC) {
        *bytes_consumed = 3;
        return (strlcpy(output, "Euro", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    /* Percent: % (0x25) */
    if (sym[0] == '%') {
        *bytes_consumed = 1;
        return (strlcpy(output, "percent", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    return ESPURE_ERR_NOT_FOUND;
}

/* ========================================================================
 * Exported Locale Operations
 * ======================================================================== */

const espure_normalize_ops_t ESPURE_NORMALIZE_EN_GB = {
    .locale = "en-GB",
    .abbrevs = ENGLISH_ABBREVS,
    .num_abbrevs = sizeof(ENGLISH_ABBREVS) / sizeof(ENGLISH_ABBREVS[0]),
    .number_to_words = espure_number_to_english_words,
    .expand_symbol = en_GB_expand_symbol,
};
