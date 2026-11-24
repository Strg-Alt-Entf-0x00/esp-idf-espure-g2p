#include <stdio.h>
#include <string.h>

#include "espure.h"
#include "espure_internal.h"

static int test_binary_dictionary_case_lookup(void) {
#ifndef CONFIG_ESPURE_DICT_HUFFMAN_TRIE
    espure_dict_entry_t entries[] = {
        {"ELSTER", "AllCaps", 0},
        {"Elster", "TitleCase", 0},
        {"Foo", "UpperASCII", 0},
        {"foo", "lowerASCII", 0},
        {"Stra\xC3\x9F" "e", "UpperSharpS", 0},
        {"stra\xC3\x9F" "e", "lowerSharpS", 0},
        {"\xC3\x84rger", "UpperUmlaut", 0},
        {"\xC3\xA4rger", "lowerUmlaut", 0},
    };
    espure_dictionary_t dictionary = {
        .entries = entries,
        .count = sizeof(entries) / sizeof(entries[0]),
    };
    char output[32];

    if (espure_dictionary_lookup(&dictionary, "Foo", output,
                                 sizeof(output), NULL) != ESPURE_OK ||
        strcmp(output, "UpperASCII") != 0) {
        fprintf(stderr, "Exact capitalization was not preferred\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "elster", output,
                                 sizeof(output), NULL) != ESPURE_ERR_NOT_FOUND) {
        fprintf(stderr, "Ambiguous casing was silently selected as lowercase\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "FOO", output,
                                 sizeof(output), NULL) != ESPURE_OK ||
        strcmp(output, "lowerASCII") != 0) {
        fprintf(stderr, "ASCII lowercase fallback failed\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary,
                                 "\x53\x54\x52\x41\xE1\xBA\x9E" "E",
                                 output, sizeof(output), NULL) != ESPURE_OK ||
        strcmp(output, "lowerSharpS") != 0) {
        fprintf(stderr, "Capital sharp S lowercase fallback failed\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "\xC3\x84rger", output,
                                 sizeof(output), NULL) != ESPURE_OK ||
        strcmp(output, "UpperUmlaut") != 0) {
        fprintf(stderr, "Exact umlaut capitalization was not preferred\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "\xC3\x84RGER", output,
                                 sizeof(output), NULL) != ESPURE_OK ||
        strcmp(output, "lowerUmlaut") != 0) {
        fprintf(stderr, "UTF-8 lowercase fallback failed\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "Foo", output, 2, NULL) !=
        ESPURE_ERR_BUFFER_TOO_SMALL) {
        fprintf(stderr, "Dictionary lookup did not report a short output buffer\n");
        return 1;
    }
#else
    espure_dictionary_t dictionary = {
        .use_compressed = true,
    };
    char output[2];
    if (espure_dictionary_lookup(&dictionary, "stricken", output,
                                 sizeof(output), NULL) !=
        ESPURE_ERR_BUFFER_TOO_SMALL) {
        fprintf(stderr, "Compressed lookup did not report a short output buffer\n");
        return 1;
    }
    if (espure_dictionary_lookup(&dictionary, "stricken", output, 0, NULL) !=
        ESPURE_ERR_INVALID_ARG) {
        fprintf(stderr, "Compressed lookup accepted a zero-sized output buffer\n");
        return 1;
    }
#endif
    return 0;
}

int main(void) {
    if (strcmp(espure_err_str(ESPURE_ERR_INVALID_SIZE), "Invalid size") != 0 ||
        strcmp(espure_err_str(ESPURE_ERR_NOT_SUPPORTED),
               "Operation not supported") != 0) {
        fprintf(stderr, "Public error descriptions are incomplete\n");
        return 1;
    }

    if (test_binary_dictionary_case_lookup() != 0) {
        return 1;
    }

    espure_handle_t handle = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = false;

    espure_err_t err = espure_init("de-DE", &config, &handle);
    if (err != ESPURE_OK) {
        fprintf(stderr, "espure_init failed: %s\n", espure_err_str(err));
        return 1;
    }

    char lowercase[128];
    char capitalized[128];
    char uppercase[128];
    const struct {
        const char* input;
        char* output;
    } cases[] = {
        {"stricken", lowercase},
        {"Stricken", capitalized},
        {"STRICKEN", uppercase},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        err = espure_phonemize(handle, cases[i].input, cases[i].output,
                               128, false);
        if (err != ESPURE_OK) {
            fprintf(stderr, "espure_phonemize failed for '%s': %s\n",
                    cases[i].input, espure_err_str(err));
            espure_deinit(handle);
            return 1;
        }
    }

    espure_deinit(handle);

    if (strcmp(lowercase, capitalized) == 0) {
        fprintf(stderr, "Case-sensitive dictionary entries were conflated: %s\n",
                lowercase);
        return 1;
    }
    if (strcmp(lowercase, uppercase) != 0) {
        fprintf(stderr, "Uppercase fallback did not use lowercase entry: "
                "'%s' vs '%s'\n", lowercase, uppercase);
        return 1;
    }

    return 0;
}
