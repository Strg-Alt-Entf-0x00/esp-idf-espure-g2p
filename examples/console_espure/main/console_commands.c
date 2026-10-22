/**
 * @file main.c
 * @brief Interactive UTF-8 Developer Console for ESPure G2P
 *
 * Provides a live interactive terminal on ESP32 / ESP32-P4 for real-time
 * G2P evaluation, phonetic inspection, morphological breakdown, and benchmarking.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "espure.h"
#include "uart_utf8_console.h"

static const char *TAG = "interactive_g2p";

static espure_handle_t s_g2p_de = NULL;
static espure_handle_t s_g2p_en = NULL;
static espure_handle_t s_current_g2p = NULL;
static char s_current_locale[16] = "de-DE";

static void print_banner(void) {
    printf("\r\n");
    printf("\x1b[36m=================================================================\x1b[0m\r\n");
    printf("\x1b[36m   ESPURE-G2P: Clean-Room Embedded Multilingual G2P Engine       \x1b[0m\r\n");
    printf("\x1b[36m   Hardware Target: ESP32 / ESP32-P4 | UTF-8 Interactive Console \x1b[0m\r\n");
    printf("\x1b[36m=================================================================\x1b[0m\r\n");
    printf("Type \x1b[33mhelp\x1b[0m for available commands.\r\n\r\n");
}

static void print_help(void) {
    printf("\r\n\x1b[33mAvailable Commands:\x1b[0m\r\n");
    printf("  \x1b[32mphonemize <word>\x1b[0m (or \x1b[32mp <word>\x1b[0m)  : Full 6-stage G2P + phoneme stream analysis\r\n");
    printf("  \x1b[32mrules <word>\x1b[0m     (or \x1b[32mr <word>\x1b[0m)  : Pure rules-only bypass (no dictionary)\r\n");
    printf("  \x1b[32mmorph <word>\x1b[0m     (or \x1b[32mm <word>\x1b[0m)  : Morphological decomposition & compound split\r\n");
    printf("  \x1b[32mbench <word> [N]\x1b[0m (or \x1b[32mb <word>\x1b[0m)  : Benchmark N phonemizations (throughput & latency)\r\n");
    printf("  \x1b[32mlocale <loc>\x1b[0m                    : Switch active locale (de-DE, en-GB)\r\n");
    printf("  \x1b[32minfo\x1b[0m                            : Show memory, PSRAM, and dictionary statistics\r\n");
    printf("  \x1b[32mhelp\x1b[0m                            : Display this help message\r\n\r\n");
}

static void cmd_phonemize(const char *word, bool rules_only) {
    if (!word || strlen(word) == 0) {
        printf("Usage: phonemize <word>\r\n");
        return;
    }

    espure_options_t opts = ESPURE_OPTIONS_DEFAULT();
    opts.format = ESPURE_FORMAT_IPA;
    opts.rules_only = rules_only;

    char ipa[256] = {0};
    int64_t t0 = esp_timer_get_time();
    espure_err_t err = espure_phonemize_ex(s_current_g2p, word, ipa, sizeof(ipa), &opts);
    int64_t elapsed_us = esp_timer_get_time() - t0;

    if (err != ESPURE_OK) {
        printf("\x1b[31mError:\x1b[0m Phonemization failed: %s (%d)\r\n", espure_err_str(err), err);
        return;
    }

    printf("\r\nWord: \x1b[1m%s\x1b[0m [%s%s]\r\n", word, s_current_locale, rules_only ? " (rules-only)" : "");
    printf("  \x1b[32m-> IPA\x1b[0m: /\x1b[1m%s\x1b[0m/\r\n", ipa);
    printf("  Latency: %lld us\r\n", (long long)elapsed_us);

    // Phoneme stream analysis
    espure_segment_t seg_buf[64];
    espure_stream_t stream;
    err = espure_phonemize_stream(s_current_g2p, word, seg_buf, 64, &stream);
    if (err == ESPURE_OK && stream.segment_count > 0) {
        printf("  \x1b[33m-> Stream Segments\x1b[0m (%d): [", (int)stream.segment_count);
        for (size_t i = 0; i < stream.segment_count; i++) {
            printf("%d%s", seg_buf[i].segment_id, (i + 1 < stream.segment_count) ? ", " : "");
        }
        printf("]\r\n");
    }
    printf("\r\n");
}

static void cmd_morph(const char *word) {
    if (!word || strlen(word) == 0) {
        printf("Usage: morph <word>\r\n");
        return;
    }

    char morphed[256] = {0};
    bool split = espure_morph_split(s_current_locale, word, morphed, sizeof(morphed));
    printf("\r\nWord: \x1b[1m%s\x1b[0m\r\n", word);
    if (split) {
        printf("  \x1b[32m-> Morphemes\x1b[0m: %s\r\n\r\n", morphed);
    } else {
        printf("  \x1b[33m-> Monomorphemic / No decomposition\x1b[0m\r\n\r\n");
    }
}

static void cmd_bench(const char *word, int iterations) {
    if (!word || strlen(word) == 0) {
        printf("Usage: bench <word> [iterations]\r\n");
        return;
    }
    if (iterations <= 0) iterations = 1000;

    printf("\r\nBenchmarking '%s' across %d iterations...\r\n", word, iterations);
    char buf[256];
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        espure_phonemize(s_current_g2p, word, buf, sizeof(buf), true);
    }
    int64_t total_us = esp_timer_get_time() - t0;
    double avg_us = (double)total_us / iterations;
    double wps = (iterations * 1000000.0) / total_us;

    printf("  Result: /%s/\r\n", buf);
    printf("  Total time : %lld us\r\n", (long long)total_us);
    printf("  Avg latency: \x1b[32m%.2f us/word\x1b[0m\r\n", avg_us);
    printf("  Throughput : \x1b[36m%.0f words/second\x1b[0m\r\n\r\n", wps);
}

static void cmd_info(void) {
    printf("\r\n\x1b[33mSystem & Engine Status:\x1b[0m\r\n");
    printf("  Active Locale  : %s\r\n", s_current_locale);
    printf("  Internal Free  : %d bytes\r\n", (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("  PSRAM Free     : %d bytes\r\n", (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("  Min Free Heap  : %d bytes\r\n\r\n", (int)esp_get_minimum_free_heap_size());
}

static void handle_command(const char *line) {
    while (*line == ' ') line++;
    if (!*line) return;

    char cmd[32] = {0};
    const char *arg = NULL;
    const char *space = strchr(line, ' ');
    if (space) {
        size_t cmd_len = space - line;
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        strncpy(cmd, line, cmd_len);
        arg = space + 1;
        while (*arg == ' ') arg++;
    } else {
        strlcpy(cmd, line, sizeof(cmd));
        arg = "";
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        print_help();
    } else if (strcmp(cmd, "phonemize") == 0 || strcmp(cmd, "p") == 0) {
        if (!*arg) {
            cmd_phonemize(cmd, false);
        } else {
            cmd_phonemize(arg, false);
        }
    } else if (strcmp(cmd, "rules") == 0 || strcmp(cmd, "r") == 0) {
        cmd_phonemize(arg, true);
    } else if (strcmp(cmd, "morph") == 0 || strcmp(cmd, "m") == 0) {
        cmd_morph(arg);
    } else if (strcmp(cmd, "bench") == 0 || strcmp(cmd, "b") == 0) {
        char word[128] = {0};
        int iters = 1000;
        if (sscanf(arg, "%127s %d", word, &iters) >= 1) {
            cmd_bench(word, iters);
        } else {
            printf("Usage: bench <word> [iterations]\r\n");
        }
    } else if (strcmp(cmd, "locale") == 0) {
        if (strcmp(arg, "de-DE") == 0) {
            s_current_g2p = s_g2p_de;
            strlcpy(s_current_locale, "de-DE", sizeof(s_current_locale));
            printf("Switched to locale: de-DE\r\n");
        } else if (strcmp(arg, "en-GB") == 0) {
            s_current_g2p = s_g2p_en;
            strlcpy(s_current_locale, "en-GB", sizeof(s_current_locale));
            printf("Switched to locale: en-GB\r\n");
        } else {
            printf("Unknown locale '%s'. Supported: de-DE, en-GB\r\n", arg);
        }
    } else if (strcmp(cmd, "info") == 0) {
        cmd_info();
    } else {
        // Direct input convenience: treat unknown command as a word to phonemize!
        cmd_phonemize(line, false);
    }
}

void app_main(void) {
    print_banner();

    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = true;
    config.enable_hwaccel = true;

    ESP_LOGI(TAG, "Initializing German G2P engine (de-DE)...");
    espure_err_t err = espure_init("de-DE", &config, &s_g2p_de);
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize de-DE: %s (%d)", espure_err_str(err), err);
        return;
    }

    ESP_LOGI(TAG, "Initializing English G2P engine (en-GB)...");
    err = espure_init("en-GB", &config, &s_g2p_en);
    if (err != ESPURE_OK) {
        ESP_LOGW(TAG, "en-GB not available (%d)", err);
    }

    s_current_g2p = s_g2p_de;
    strlcpy(s_current_locale, "de-DE", sizeof(s_current_locale));

    ESP_LOGI(TAG, "Starting interactive UTF-8 console...");
    ESP_ERROR_CHECK(uart_utf8_console_start("\x1b[36mESP-G2P>\x1b[0m ", handle_command));
}
