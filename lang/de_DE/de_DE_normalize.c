/**
 * @file de_DE_normalize.c
 * @brief Standard German (de-DE) Text Normalization Pack
 * 
 * Contains German abbreviation expansion rules, cardinal number-to-words
 * converters (0..999,999), and German currency/symbol mappings.
 */

#include "espure_normalize.h"
#include "espure_internal.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* ========================================================================
 * German Abbreviation Tables
 * ======================================================================== */

static const espure_abbrev_entry_t GERMAN_ABBREVS[] = {
    {"z. B.", "zum Beispiel", false},
    {"z.B.", "zum Beispiel", false},
    {"d. h.", "das heißt", false},
    {"d.h.", "das heißt", false},
    {"u. a.", "unter anderem", false},
    {"u.a.", "unter anderem", false},
    {"u.s.w.", "und so weiter", false},
    {"usw.", "und so weiter", false},
    {"etc.", "et cetera", false},
    {"bzw.", "beziehungsweise", false},
    {"Dr.", "Doktor", false},
    {"Prof.", "Professor", false},
    {"Nr.", "Nummer", false},
    {"ca.", "circa", false},
    {"inkl.", "inklusive", false},
    {"exkl.", "exklusive", false},
    {"evtl.", "eventuell", false},
    {"vgl.", "vergleiche", false},
    {"Hr.", "Herr", false},
    {"Fr.", "Frau", false},
    {"km/h", "Kilometer pro Stunde", false},
    {"08/15", "nullachtfünfzehn", false},
    {"100pro", "hundertprozentig", false},
    {"2G", "zwei g", false},
    {"3G", "drei g", false},
    {"4G", "vier g", false},
    {"5G", "fünf g", false},
};

/* ========================================================================
 * German Cardinal Number Converter (0 to 999,999)
 * ======================================================================== */

static const char* const UNITS[] = {
    "", "ein", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun"
};

static const char* const TEENS[] = {
    "zehn", "elf", "zwölf", "dreizehn", "vierzehn", "fünfzehn",
    "sechzehn", "siebzehn", "achtzehn", "neunzehn"
};

static const char* const TENS[] = {
    "", "", "zwanzig", "dreißig", "vierzig", "fünfzig",
    "sechzig", "siebzig", "achtzig", "neunzig"
};

static espure_err_t sub_100_to_german(int n, char *out, size_t out_size, bool is_terminal) {
    if (n < 0 || n >= 100) return ESPURE_ERR_INVALID_ARG;
    if (n == 0) return ESPURE_OK;

    if (n == 1) {
        return (strlcat(out, is_terminal ? "eins" : "ein", out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }
    if (n < 10) {
        return (strlcat(out, UNITS[n], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }
    if (n < 20) {
        return (strlcat(out, TEENS[n - 10], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    int unit = n % 10;
    int ten = n / 10;

    if (unit == 0) {
        return (strlcat(out, TENS[ten], out_size) < out_size) ?
               ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%sund%s", UNITS[unit], TENS[ten]);
    return (strlcat(out, buf, out_size) < out_size) ?
           ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
}

static espure_err_t sub_1000_to_german(int n, char *out, size_t out_size, bool is_terminal) {
    if (n < 0 || n >= 1000) return ESPURE_ERR_INVALID_ARG;
    if (n == 0) return ESPURE_OK;

    int hundred = n / 100;
    int rem = n % 100;

    if (hundred > 0) {
        const char *h_unit = (hundred == 1) ? "ein" : UNITS[hundred];
        if (strlcat(out, h_unit, out_size) >= out_size ||
            strlcat(out, "hundert", out_size) >= out_size) {
            return ESPURE_ERR_BUFFER_TOO_SMALL;
        }
    }

    if (rem > 0) {
        return sub_100_to_german(rem, out, out_size, is_terminal);
    }
    return ESPURE_OK;
}

espure_err_t espure_number_to_german_words(int64_t number, char *output, size_t out_size) {
    if (!output || out_size == 0) return ESPURE_ERR_INVALID_ARG;
    output[0] = '\0';

    if (number == 0) {
        strlcpy(output, "null", out_size);
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
        const char *th_prefix = (thousand == 1) ? "ein" : "";
        if (thousand == 1) {
            if (strlcat(output, th_prefix, out_size) >= out_size ||
                strlcat(output, "tausend", out_size) >= out_size) {
                return ESPURE_ERR_BUFFER_TOO_SMALL;
            }
        } else {
            espure_err_t err = sub_1000_to_german(thousand, output, out_size, false);
            if (err != ESPURE_OK) return err;
            if (strlcat(output, "tausend", out_size) >= out_size) {
                return ESPURE_ERR_BUFFER_TOO_SMALL;
            }
        }
    }

    if (rem > 0) {
        return sub_1000_to_german(rem, output, out_size, true);
    }

    return ESPURE_OK;
}

/* ========================================================================
 * German Symbol Expansion (€, §, %)
 * ======================================================================== */

static espure_err_t de_DE_expand_symbol(const char *sym, char *output, size_t out_size, size_t *bytes_consumed) {
    if (!sym || !output || !bytes_consumed || out_size == 0) return ESPURE_ERR_INVALID_ARG;

    /* Euro: € (0xE2 0x82 0xAC) */
    if ((unsigned char)sym[0] == 0xE2 && (unsigned char)sym[1] == 0x82 && (unsigned char)sym[2] == 0xAC) {
        *bytes_consumed = 3;
        return (strlcpy(output, "Euro", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    /* Paragraph: § (0xC2 0xA7) */
    if ((unsigned char)sym[0] == 0xC2 && (unsigned char)sym[1] == 0xA7) {
        *bytes_consumed = 2;
        return (strlcpy(output, "Paragraph", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    /* Percent: % (0x25) */
    if (sym[0] == '%') {
        *bytes_consumed = 1;
        return (strlcpy(output, "Prozent", out_size) < out_size) ? ESPURE_OK : ESPURE_ERR_BUFFER_TOO_SMALL;
    }

    return ESPURE_ERR_NOT_FOUND;
}

/* ========================================================================
 * Exported Locale Operations
 * ======================================================================== */

const espure_normalize_ops_t ESPURE_NORMALIZE_DE_DE = {
    .locale = "de-DE",
    .abbrevs = GERMAN_ABBREVS,
    .num_abbrevs = sizeof(GERMAN_ABBREVS) / sizeof(GERMAN_ABBREVS[0]),
    .number_to_words = espure_number_to_german_words,
    .expand_symbol = de_DE_expand_symbol,
};
