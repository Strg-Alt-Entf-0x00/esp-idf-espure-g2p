/**
 * @file main.c
 * @brief Basic Usage Example for ESPURE G2P
 * 
 * Demonstrates how to initialize the espure library and phonemize a single word.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "espure.h"

static const char* TAG = "BASIC_G2P";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESPURE G2P Basic Example...");

    // 1. Configure the engine
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    // (Optional) config.use_psram = true; // if you have PSRAM and want to save internal RAM
    
    espure_handle_t g2p = NULL;
    
    // 2. Initialize the library for German ("de")
    // Note: Requires CONFIG_ESPURE_LANG_DE=y in menuconfig
    ESP_LOGI(TAG, "Initializing G2P engine for 'de'...");
    espure_err_t err = espure_init("de", &config, &g2p);
    
    if (err != ESPURE_OK) {
        ESP_LOGE(TAG, "Failed to initialize G2P engine: %s", espure_err_str(err));
        return;
    }

    ESP_LOGI(TAG, "G2P engine initialized successfully!");

    // 3. Phonemize a word
    const char* word = "Beispiel";
    char ipa_buffer[128] = {0};
    
    // The last parameter 'true' selects IPA format. 'false' would select Kirshenbaum.
    err = espure_phonemize(g2p, word, ipa_buffer, sizeof(ipa_buffer), true);
    
    if (err == ESPURE_OK) {
        ESP_LOGI(TAG, "Success! Word: '%s' -> IPA: '%s'", word, ipa_buffer);
    } else {
        ESP_LOGE(TAG, "Phonemization failed: %s", espure_err_str(err));
    }

    // 4. Clean up
    ESP_LOGI(TAG, "Deinitializing G2P engine...");
    espure_deinit(g2p);
    
    ESP_LOGI(TAG, "Example finished.");
}
