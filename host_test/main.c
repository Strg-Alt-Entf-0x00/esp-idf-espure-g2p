#include <stdio.h>
#include "espure.h"
#include <string.h>

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    espure_handle_t handle = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = false;
    
    printf("Initializing espure...\n");
    espure_err_t err = espure_init("de_DE", &config, &handle);
    if (err != ESPURE_OK) {
        printf("Init failed: %d\n", err);
        return 1;
    }
    
    const char* words[] = {
        "könig", "könige", "königlich",
        "fünfzig", "wenig", "wenige", "richtig",
        "tag", "tage", "zug", "züge", "flug",
        "hund", "hunde", "bad", "bäder", "geld", "gelder", "abend",
        "ab", "dieb", "diebe", "gelb", "gelbe", "halb", "halbe"
    };
    char ipa_buf[128];
    int count = sizeof(words) / sizeof(words[0]);
    
    for (int i = 0; i < count; i++) {
        err = espure_phonemize(handle, words[i], ipa_buf, sizeof(ipa_buf), false);
        if (err == ESPURE_OK) {
            printf("[TEST] %-12s -> %s\n", words[i], ipa_buf);
        } else {
            printf("[TEST] %-12s -> ERROR %d\n", words[i], err);
        }
    }
    
    espure_deinit(handle);
    return 0;
}
