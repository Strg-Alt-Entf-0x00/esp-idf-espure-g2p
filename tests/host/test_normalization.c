#include "espure.h"
#include "espure_normalize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

static void test_number_to_words(void) {
    char buf[128];

    TEST_ASSERT(espure_number_to_german_words(0, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "null") == 0);

    TEST_ASSERT(espure_number_to_german_words(1, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "eins") == 0);

    TEST_ASSERT(espure_number_to_german_words(12, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "zwölf") == 0);

    TEST_ASSERT(espure_number_to_german_words(21, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "einundzwanzig") == 0);

    TEST_ASSERT(espure_number_to_german_words(100, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "einhundert") == 0);

    TEST_ASSERT(espure_number_to_german_words(105, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "einhundertfünf") == 0);

    TEST_ASSERT(espure_number_to_german_words(999, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "neunhundertneunundneunzig") == 0);

    TEST_ASSERT(espure_number_to_german_words(1000, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "eintausend") == 0);

    TEST_ASSERT(espure_number_to_german_words(1001, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "eintausendeins") == 0);

    TEST_ASSERT(espure_number_to_german_words(1999, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "eintausendneunhundertneunundneunzig") == 0);

    TEST_ASSERT(espure_number_to_german_words(42000, buf, sizeof(buf)) == ESPURE_OK);
    TEST_ASSERT(strcmp(buf, "zweiundvierzigtausend") == 0);

    printf("[PASS] test_number_to_words\n");
}

static void test_text_normalization(void) {
    char out[256];

    TEST_ASSERT(espure_normalize_text("de-DE", "z. B.", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "zum Beispiel") == 0);

    TEST_ASSERT(espure_normalize_text("de-DE", "Dr. Müller", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "Doktor Müller") == 0);

    TEST_ASSERT(espure_normalize_text("de-DE", "42 €", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "zweiundvierzig Euro") == 0);

    TEST_ASSERT(espure_normalize_text("de-DE", "50%", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "fünfzig Prozent") == 0);

    TEST_ASSERT(espure_normalize_text("de-DE", "08/15", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "nullachtfünfzehn") == 0);

    TEST_ASSERT(espure_normalize_text("de-DE", "\"Hallo!\"", out, sizeof(out)) == ESPURE_OK);
    TEST_ASSERT(strcmp(out, "Hallo") == 0);

    printf("[PASS] test_text_normalization\n");
}

static void test_e2e_phonemization_with_normalization(void) {
    espure_handle_t handle = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = false;

    TEST_ASSERT(espure_init("de-DE", &config, &handle) == ESPURE_OK);

    char ipa[256];
    // "42" should normalize to "zweiundvierzig" and phonemize without error
    TEST_ASSERT(espure_phonemize(handle, "42", ipa, sizeof(ipa), true) == ESPURE_OK);
    TEST_ASSERT(strlen(ipa) > 0);
    printf("Phonemized '42': %s\n", ipa);

    // "Dr." should normalize to "Doktor" and phonemize
    TEST_ASSERT(espure_phonemize(handle, "Dr.", ipa, sizeof(ipa), true) == ESPURE_OK);
    TEST_ASSERT(strlen(ipa) > 0);
    printf("Phonemized 'Dr.': %s\n", ipa);

    espure_deinit(handle);
    printf("[PASS] test_e2e_phonemization_with_normalization\n");
}

int main(void) {
    test_number_to_words();
    test_text_normalization();
    test_e2e_phonemization_with_normalization();
    printf("ALL NORMALIZATION TESTS PASSED\n");
    return 0;
}
