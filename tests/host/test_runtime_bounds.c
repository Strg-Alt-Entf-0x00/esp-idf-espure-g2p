#include <stdio.h>
#include <string.h>

#include "espure_internal.h"
#include "espure_morph.h"
#include "espure_stress.h"

static int test_morphology_length_limit(void) {
    char word[129];
    char output[160];
    memset(word, 'a', sizeof(word) - 1);
    word[sizeof(word) - 1] = '\0';

    if (espure_morph_split("de-DE", word, output, sizeof(output))) {
        fprintf(stderr, "Morphology unexpectedly split an over-limit word\n");
        return 1;
    }
    if (strcmp(word, output) != 0) {
        fprintf(stderr, "Morphology changed an over-limit word\n");
        return 1;
    }
    return 0;
}

static int test_morphology_language_selection(void) {
    char output[64];
    const char* word = "Kinderstube";

    if (!espure_morph_split("de-DE", word, output, sizeof(output)) ||
        strcmp(output, "Kinder-stube") != 0) {
        fprintf(stderr, "German morphology did not split Kinderstube as expected: %s\n",
                output);
        return 1;
    }

    if (espure_morph_split("en-GB", word, output, sizeof(output)) ||
        strcmp(output, word) != 0) {
        fprintf(stderr, "Morphology used German data for an unsupported language\n");
        return 1;
    }
    return 0;
}

static int test_german_ipa_rendering(void) {
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_phoneme_table_t* table = NULL;
    if (espure_phoneme_table_init("de-DE", &config, &table) != ESPURE_OK) {
        fprintf(stderr, "Could not initialize phoneme table for renderer test\n");
        return 1;
    }

    const espure_phoneme_t* schwa = espure_phoneme_lookup(table, "@");
    const espure_phoneme_t* short_schwa = espure_phoneme_lookup(table, "@-");
    const espure_phoneme_t* pf_affricate = espure_phoneme_lookup(table, "pF");
    if (espure_phoneme_table_count(table) != 90 ||
        !schwa || schwa->type != ESPURE_PHONEME_VOWEL || schwa->code != 13) {
        fprintf(stderr, "Phoneme declarations were not merged uniquely or reserved data was not updated\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }
    if (!short_schwa || short_schwa->type != ESPURE_PHONEME_VOWEL ||
        !(short_schwa->flags & ESPURE_PHONEME_FLAG_UNSTRESSED)) {
        fprintf(stderr, "German @- phoneme was not classified as unstressed schwa\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }
    if (!pf_affricate || pf_affricate->type != ESPURE_PHONEME_AFFRICATE) {
        fprintf(stderr, "German pF phoneme was not classified as an affricate\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    char output[64];
    espure_err_t err = espure_render_phoneme_string(
        table, "A A: a i y o pf pF ts OY u : , % ! _ 'a ' A: @-", output, sizeof(output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_OK ||
        strcmp(output, "aaːaiyopfpftsɔʏuːˌʔˈaˈaːə") != 0) {
        fprintf(stderr, "Unexpected German IPA rendering: %s (error %d)\n",
                output, err);
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_render_phoneme_string(
        table, "= A: A _| A", output, sizeof(output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_OK || strcmp(output, "ˈaːaa") != 0) {
        fprintf(stderr, "Rule stress or short-pause control rendered incorrectly: %s\n",
                output);
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_render_phoneme_string(
        table, "A =", output, sizeof(output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_ERR_INVALID_ARG || output[0] != '\0') {
        fprintf(stderr, "Renderer accepted a dangling stress marker\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    char short_stress_output[4];
    err = espure_render_phoneme_string(
        table, "= A:", short_stress_output, sizeof(short_stress_output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_ERR_BUFFER_TOO_SMALL || short_stress_output[0] != '\0') {
        fprintf(stderr, "Renderer did not check capacity for pending stress\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_render_phoneme_string(
        table, "_^_ EN", output, sizeof(output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "Renderer accepted an unsupported language-switch control\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_render_phoneme_string(
        table, "unknown", output, sizeof(output),
        ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_ERR_NOT_FOUND || output[0] != '\0') {
        fprintf(stderr, "Renderer did not reject an unmapped mnemonic\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_render_phoneme_string(
        table, "A A", output, 2, ESPURE_FORMAT_IPA, NULL, NULL);
    if (err != ESPURE_ERR_BUFFER_TOO_SMALL || output[0] != '\0') {
        fprintf(stderr, "Renderer did not report a truncated output buffer\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }
    espure_phoneme_table_deinit(table);
    return 0;
}

static int test_phoneme_parser_rejects_invalid_and_over_capacity_input(void) {
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_phoneme_table_t* table = NULL;
    if (espure_phoneme_table_init("de-DE", &config, &table) != ESPURE_OK) {
        fprintf(stderr, "Could not initialize phoneme parser test table\n");
        return 1;
    }

    const espure_phoneme_t* parsed[1] = {0};
    size_t count = 0;
    espure_err_t err = espure_parse_phoneme_string(
        table, "q", parsed, 1, &count);
    if (err != ESPURE_ERR_NOT_FOUND || count != 0) {
        fprintf(stderr, "Phoneme parser accepted an unknown character\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    err = espure_parse_phoneme_string(table, "aa", parsed, 1, &count);
    if (err != ESPURE_ERR_BUFFER_TOO_SMALL || count != 0) {
        fprintf(stderr, "Phoneme parser accepted output beyond its capacity\n");
        espure_phoneme_table_deinit(table);
        return 1;
    }

    espure_phoneme_table_deinit(table);
    return 0;
}

static int test_german_pf_rule_output(void) {
    char phonemes[32];
    espure_err_t err = espure_text_to_phonemes(
        "de-DE", "pfund", phonemes, sizeof(phonemes));
    if (err != ESPURE_OK || strncmp(phonemes, "pF", 2) != 0) {
        fprintf(stderr, "German pf rule did not produce pF: %s (error %d)\n",
                phonemes, err);
        return 1;
    }
    strlcpy(phonemes, "pF", sizeof(phonemes));

    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_phoneme_table_t* table = NULL;
    if (espure_phoneme_table_init("de-DE", &config, &table) != ESPURE_OK) {
        fprintf(stderr, "Could not initialize German pF rule test\n");
        return 1;
    }

    const espure_phoneme_t* parsed[1] = {0};
    size_t count = 0;
    err = espure_parse_phoneme_string(
        table, phonemes, parsed, 1, &count);
    char ipa[8];
    if (err == ESPURE_OK && count == 1) {
        err = espure_render_phoneme_string(
            table, parsed[0]->mnemonic, ipa, sizeof(ipa),
            ESPURE_FORMAT_IPA, NULL, NULL);
    } else {
        ipa[0] = '\0';
    }
    espure_phoneme_table_deinit(table);
    if (err != ESPURE_OK || strcmp(ipa, "pf") != 0) {
        fprintf(stderr, "German pF rule output did not render as IPA pf: %s\n",
                ipa);
        return 1;
    }
    return 0;
}

static int test_german_rule_stress_end_to_end(void) {
    char phonemes[32];
    espure_err_t err = espure_text_to_phonemes(
        "de-DE", "basta", phonemes, sizeof(phonemes));
    if (err != ESPURE_OK || strcmp(phonemes, "bast=A:") != 0) {
        fprintf(stderr, "German contextual a-stress rule changed unexpectedly: %s "
                "(error %d)\n", phonemes, err);
        return 1;
    }

    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_phoneme_table_t* table = NULL;
    if (espure_phoneme_table_init("de-DE", &config, &table) != ESPURE_OK) {
        fprintf(stderr, "Could not initialize German stress test table\n");
        return 1;
    }

    char ipa[16];
    err = espure_render_phoneme_string(
        table, "b a s t = A:", ipa, sizeof(ipa),
        ESPURE_FORMAT_IPA, NULL, NULL);
    espure_phoneme_table_deinit(table);
    if (err != ESPURE_OK || strcmp(ipa, "bastˈaː") != 0) {
        fprintf(stderr, "German rule stress was not rendered on its next vowel: "
                "%s (error %d)\n", ipa, err);
        return 1;
    }
    return 0;
}

static int test_rule_engine_rejects_truncation_and_invalid_languages(void) {
    char output[16];
    espure_err_t err = espure_text_to_phonemes(
        NULL, "pfund", output, sizeof(output));
    if (err != ESPURE_ERR_INVALID_ARG) {
        fprintf(stderr, "Rule engine accepted a null locale identifier\n");
        return 1;
    }

    err = espure_text_to_phonemes(
        "de-DE-typo", "pfund", output, sizeof(output));
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "Rule engine accepted a noncanonical German locale identifier\n");
        return 1;
    }

    char oversized_word[257];
    memset(oversized_word, 'a', sizeof(oversized_word) - 1);
    oversized_word[sizeof(oversized_word) - 1] = '\0';
    err = espure_text_to_phonemes(
        "de-DE", oversized_word, output, sizeof(output));
    if (err != ESPURE_ERR_INVALID_SIZE || output[0] != '\0') {
        fprintf(stderr, "Rule engine silently truncated an over-limit input\n");
        return 1;
    }

    char short_output[2];
    err = espure_text_to_phonemes(
        "de-DE", "pfund", short_output, sizeof(short_output));
    if (err != ESPURE_ERR_BUFFER_TOO_SMALL || short_output[0] != '\0') {
        fprintf(stderr, "Rule engine silently truncated its phoneme output\n");
        return 1;
    }

    err = espure_text_to_phonemes("de-DE", "air", output, sizeof(output));
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "Rule engine accepted a German rule's language switch\n");
        return 1;
    }
    return 0;
}


static int test_german_api_ipa_words(void) {
    static const char* const words[] = {
        "könig", "könige", "fünfzig", "richtig", "tag", "zug", "hund",
        "bad", "abend", "dieb", "halb", "flug",
        "neuntausendneunhundertneunundneunzig"
    };
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    espure_handle_t handle = NULL;
    espure_err_t err = espure_init("de-DE", &config, &handle);
    if (err != ESPURE_OK) {
        fprintf(stderr, "Could not initialize German G2P API test: %d\n", err);
        return 1;
    }

    char output[128];
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        err = espure_phonemize(handle, words[i], output, sizeof(output), true);
        if (err != ESPURE_OK) {
            fprintf(stderr, "German IPA failed for '%s': error %d\n",
                    words[i], err);
            espure_deinit(handle);
            return 1;
        }
        if (strcmp(words[i], "neuntausendneunhundertneunundneunzig") == 0 &&
            strcmp(output, "nointa\xC9\x90sntnoinhunt\xCA\x81tnoinuntnointsi\xC3\xA7") != 0 &&
            strcmp(output, "nointa\xCA\x8Asntnoinhunt\xCA\x81tnoinuntnointsi\xC3\xA7") != 0 &&
            strcmp(output, "n\xCB\x88oint\xCB\x8C" "a\xCA\x8Asntno\xCB\x8Cinhunt\xCA\x81tn\xCB\x8Coin\xCB\x8Cuntno\xCB\x8Cintsi\xC3\xA7") != 0) {
            fprintf(stderr, "Long German dictionary pronunciation was truncated: %s\n",
                    output);
            espure_deinit(handle);
            return 1;
        }
    }

    err = espure_phonemize(handle, "air", output, sizeof(output), true);
    if (err != ESPURE_ERR_NOT_SUPPORTED || output[0] != '\0') {
        fprintf(stderr, "German API did not propagate the unsupported language switch: "
                "error %d, output '%s'\n", err, output);
        espure_deinit(handle);
        return 1;
    }
    espure_deinit(handle);
    return 0;
}

static int test_stress_vowel_limit(void) {
    espure_phoneme_t vowel = {0};
    espure_phlist_entry_t entries[40] = {0};
    vowel.type = ESPURE_PHONEME_VOWEL;
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        entries[i].ph = &vowel;
    }

    espure_assign_word_stress(entries,
                              sizeof(entries) / sizeof(entries[0]));

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (entries[i].stresslevel > STRESS_PRIORITY) {
            fprintf(stderr, "Stress assignment wrote an invalid stress value\n");
            return 1;
        }
    }
    return 0;
}

int main(void) {
    if (test_morphology_length_limit() != 0) {
        return 1;
    }
    if (test_morphology_language_selection() != 0) {
        return 1;
    }
    if (test_german_ipa_rendering() != 0) {
        return 1;
    }
    if (test_phoneme_parser_rejects_invalid_and_over_capacity_input() != 0) {
        return 1;
    }
    if (test_german_pf_rule_output() != 0) {
        return 1;
    }
    if (test_german_rule_stress_end_to_end() != 0) {
        return 1;
    }
    if (test_rule_engine_rejects_truncation_and_invalid_languages() != 0) {
        return 1;
    }
    if (test_german_api_ipa_words() != 0) {
        return 1;
    }
    return test_stress_vowel_limit();
}
