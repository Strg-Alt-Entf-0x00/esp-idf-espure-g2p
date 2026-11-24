#include "espure.h"
#include "espure_internal.h"
#include "espure_normalize.h"
#include <stdio.h>
#include <string.h>

static int expect_phonemization(espure_handle_t handle, const char* word,
                                const char* expected) {
    char output[64];
    espure_err_t err = espure_phonemize(handle, word, output, sizeof(output), true);
    if (err != ESPURE_OK || strcmp(output, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s' (error %d)\n",
                word, expected, output, err);
        return 1;
    }
    return 0;
}

int main(void) {
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_handle_t de_DE = NULL;
    espure_handle_t en_GB = NULL;
    char output[32];
    espure_err_t err;
    int failed = 0;

    if (!espure_locale_available("de-DE") || espure_locale_available("de") ||
        espure_locale_available("de_DE") ||
        espure_locale_available("de-DE-standard") ||
        !espure_locale_available("en-GB") || espure_locale_available("en") ||
        espure_locale_available("en_GB")) {
        fprintf(stderr, "Locale availability does not match the canonical BCP 47 identifiers\n");
        return 1;
    }
    size_t locale_count = 0;
    const char** locales = espure_get_locales(&locale_count);
    if (!locales || locale_count != 2 ||
        strcmp(locales[0], "de-DE") != 0 ||
        strcmp(locales[1], "en-GB") != 0) {
        fprintf(stderr, "Locale enumeration does not return canonical BCP 47 identifiers\n");
        return 1;
    }
    espure_handle_t alias_german = NULL;
    if (espure_init("de", &config, &alias_german) != ESPURE_ERR_INVALID_LANG ||
        espure_init("de_DE", &config, &alias_german) != ESPURE_ERR_INVALID_LANG ||
        espure_init("de-DE-standard", &config, &alias_german) != ESPURE_ERR_INVALID_LANG) {
        fprintf(stderr, "German aliases and underscore IDs should not be accepted as locales\n");
        espure_deinit(alias_german);
        return 1;
    }
    espure_handle_t ambiguous_english = NULL;
    if (espure_init("en", &config, &ambiguous_english) != ESPURE_ERR_INVALID_LANG ||
        espure_init("en_GB", &config, &ambiguous_english) != ESPURE_ERR_INVALID_LANG) {
        fprintf(stderr, "English aliases and underscore IDs should not select the en-GB pilot\n");
        espure_deinit(ambiguous_english);
        return 1;
    }

    char mutable_locale[] = "de-DE";
    if (espure_init(mutable_locale, &config, &de_DE) != ESPURE_OK ||
        espure_init("en-GB", &config, &en_GB) != ESPURE_OK) {
        fprintf(stderr, "Failed to initialize concurrent DE/EN pilot handles\n");
        espure_deinit(de_DE);
        espure_deinit(en_GB);
        return 1;
    }
    memcpy(mutable_locale, "en-GB", sizeof(mutable_locale));

    failed |= expect_phonemization(en_GB, "bad", "bˈæd");
    failed |= expect_phonemization(en_GB, "hello", "həlˈəʊ");
    failed |= expect_phonemization(en_GB, "HELLO", "həlˈəʊ");
    failed |= expect_phonemization(en_GB, "world", "wˈɜːld");
    failed |= expect_phonemization(en_GB, "yes", "jˈɛs");
    err = espure_phonemize(de_DE, "bad", output, sizeof(output), true);
    if (err != ESPURE_OK || strcmp(output, "bˈaːt") != 0) {
        fprintf(stderr, "German 'bad' should remain distinct from English: '%s' (error %d)\n",
                output, err);
        failed = 1;
    }

    err = espure_render(en_GB, "a", output, sizeof(output), true);
    if (err != ESPURE_OK || strcmp(output, "æ") != 0) {
        fprintf(stderr, "English renderer did not use its language map\n");
        failed = 1;
    }
    err = espure_render(de_DE, "a", output, sizeof(output), true);
    if (err != ESPURE_OK || strcmp(output, "a") != 0) {
        fprintf(stderr, "German renderer changed while English handle was active\n");
        failed = 1;
    }
    err = espure_render(en_GB, "A", output, sizeof(output), true);
    if (err != ESPURE_ERR_NOT_FOUND || output[0] != '\0') {
        fprintf(stderr, "English renderer unexpectedly used a German-only mapping\n");
        failed = 1;
    }
    err = espure_render(en_GB, "_^_ EN", output, sizeof(output), true);
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "English renderer did not reject an inline language switch\n");
        failed = 1;
    }
    err = espure_phonemize(en_GB, "unlisted", output, sizeof(output), true);
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "English pilot did not reject words outside its lexicon\n");
        failed = 1;
    }

    /* Test English text normalization */
    char en_norm[128];
    if (espure_normalize_text("en-GB", "Dr. Smith", en_norm, sizeof(en_norm)) != ESPURE_OK ||
        strcmp(en_norm, "Doctor Smith") != 0) {
        fprintf(stderr, "English normalization failed for 'Dr. Smith': got '%s'\n", en_norm);
        failed = 1;
    }
    if (espure_normalize_text("en-GB", "42", en_norm, sizeof(en_norm)) != ESPURE_OK ||
        strcmp(en_norm, "forty-two") != 0) {
        fprintf(stderr, "English number normalization failed for '42': got '%s'\n", en_norm);
        failed = 1;
    }

    espure_phoneme_table_t* de_table = NULL;
    espure_phoneme_table_t* en_GB_table = NULL;
    if (espure_phoneme_table_init("de-DE", &config, &de_table) != ESPURE_OK ||
        espure_phoneme_table_init("en-GB", &config, &en_GB_table) != ESPURE_OK) {
        fprintf(stderr, "Failed to initialize locale-specific phoneme tables\n");
        failed = 1;
    } else if (!espure_phoneme_lookup(de_table, "pF") ||
               espure_phoneme_lookup(en_GB_table, "pF") ||
               !espure_phoneme_lookup(en_GB_table, "@U") ||
               espure_phoneme_lookup(de_table, "@U")) {
        fprintf(stderr, "English phoneme inventory reused German-only entries\n");
        failed = 1;
    }
    espure_phoneme_table_deinit(en_GB_table);
    espure_phoneme_table_deinit(de_table);

    espure_deinit(en_GB);
    espure_deinit(de_DE);

    for (size_t i = 0; i < 3; i++) {
        en_GB = NULL;
        if (espure_init("en-GB", &config, &en_GB) != ESPURE_OK) {
            fprintf(stderr, "English pilot reinitialization failed on iteration %zu\n", i);
            failed = 1;
            break;
        }
        failed |= expect_phonemization(en_GB, "hello", "həlˈəʊ");
        espure_deinit(en_GB);
    }
    return failed;
}
