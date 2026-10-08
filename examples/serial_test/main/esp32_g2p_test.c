/**
 * @file esp32_g2p_test.c
 * @brief ESP32 G2P Serial Test Firmware
 * 
 * Professional test firmware for baseline parity measurement.
 * Implements simple serial protocol for word-by-word phonemization testing.
 * 
 * Protocol:
 *   RX: "PHONEMIZE <lang> <word>\n"
 *   TX: "<ipa>\n" or "ERROR\n"
 * 
 * Example:
 *   RX: "PHONEMIZE de Haus\n"
 *   TX: "hˈaʊs\n"
 * 
 * @author ESP32 G2P Professional Development Team
 * @date 2026-09-25
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "espure.h"

static const char *TAG = "G2P_TEST";

// Serial configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RX_TIMEOUT_MS (100)

// Command buffer
#define CMD_MAX_LEN 256
static char cmd_buffer[CMD_MAX_LEN];
static size_t cmd_pos = 0;

// Global G2P handles (de and en preloaded)
static espure_handle_t g2p_de = NULL;
static espure_handle_t g2p_en = NULL;

/**
 * @brief Initialize UART for serial communication
 */
static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
}

/**
 * @brief Parse command: "PHONEMIZE <lang> <word>"
 * 
 * @param cmd Command string
 * @param lang Output: language code (2-4 chars)
 * @param word Output: word to phonemize
 * @return true if parsed successfully
 */
static bool parse_command(const char *cmd, char *lang, char *word)
{
    // Check prefix
    if (strncmp(cmd, "PHONEMIZE ", 10) != 0) {
        return false;
    }
    
    const char *p = cmd + 10;
    
    // Parse language code
    char *lang_start = (char *)p;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r') {
        p++;
    }
    
    size_t lang_len = p - lang_start;
    if (lang_len == 0 || lang_len > 6) {
        return false;
    }
    
    strncpy(lang, lang_start, lang_len);
    lang[lang_len] = '\0';
    
    // Skip whitespace
    while (*p == ' ') {
        p++;
    }
    
    // Parse word
    if (*p == '\0' || *p == '\n' || *p == '\r') {
        return false;
    }
    
    char *word_start = (char *)p;
    while (*p && *p != '\n' && *p != '\r') {
        p++;
    }
    
    size_t word_len = p - word_start;
    if (word_len == 0 || word_len > 128) {
        return false;
    }
    
    strncpy(word, word_start, word_len);
    word[word_len] = '\0';
    
    return true;
}

/**
 * @brief Process phonemization command
 * 
 * @param lang Language code
 * @param word Word to phonemize
 */
static void process_phonemize(const char *lang, const char *word)
{
    char ipa_buffer[512];
    espure_handle_t handle = NULL;
    
    // Select preloaded handle (no logging)
    if (strcmp(lang, "de") == 0) {
        handle = g2p_de;
    } else if (strcmp(lang, "en") == 0) {
        handle = g2p_en;
    } else {
        uart_write_bytes(UART_NUM, "ERROR\n", 6);
        return;
    }
    
    if (!handle) {
        uart_write_bytes(UART_NUM, "ERROR\n", 6);
        return;
    }
    
    // Call espure G2P (NEW API: handle, text, output, size, ipa=true)
    espure_err_t result = espure_phonemize(handle, word, ipa_buffer, sizeof(ipa_buffer), true);
    
    if (result == ESPURE_OK) {
        // Send IPA result (ONLY this - no logs!)
        uart_write_bytes(UART_NUM, ipa_buffer, strlen(ipa_buffer));
        uart_write_bytes(UART_NUM, "\n", 1);
    } else {
        // Send error
        uart_write_bytes(UART_NUM, "ERROR\n", 6);
    }
}

/**
 * @brief Main test task
 */
static void test_task(void *arg)
{
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    char lang[8];
    char word[256];
    
    // Send ready signal (no logging)
    uart_write_bytes(UART_NUM, "READY\n", 6);
    
    while (1) {
        // Read data from UART
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(RX_TIMEOUT_MS));
        
        if (len > 0) {
            // Process each byte
            for (int i = 0; i < len; i++) {
                char c = (char)data[i];
                
                // Handle line ending
                if (c == '\n' || c == '\r') {
                    if (cmd_pos > 0) {
                        cmd_buffer[cmd_pos] = '\0';
                        
                        // Parse and execute command
                        if (parse_command(cmd_buffer, lang, word)) {
                            process_phonemize(lang, word);
                        } else {
                            uart_write_bytes(UART_NUM, "ERROR\n", 6);
                        }
                        
                        cmd_pos = 0;
                    }
                } else if (cmd_pos < CMD_MAX_LEN - 1) {
                    cmd_buffer[cmd_pos++] = c;
                } else {
                    // Buffer overflow - reset
                    cmd_pos = 0;
                    uart_write_bytes(UART_NUM, "ERROR\n", 6);
                    ESP_LOGE(TAG, "Command buffer overflow");
                }
            }
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    free(data);
    vTaskDelete(NULL);
}

/**
 * @brief Application entry point
 */
void app_main(void)
{
    // CRITICAL: Disable ALL logging to UART for clean serial protocol
    // The Python test script expects ONLY "IPA\n" or "ERROR\n" responses
    esp_log_level_set("*", ESP_LOG_NONE);
    
    // Initialize UART
    uart_init();
    
    // Initialize espure handles for German and English
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    
    // Initialize G2P handles silently (logs disabled above)
    espure_err_t result = espure_init("de", &config, &g2p_de);
    if (result != ESPURE_OK) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    result = espure_init("en", &config, &g2p_en);
    if (result != ESPURE_OK) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Start test task
    xTaskCreate(test_task, "g2p_test", 8192, NULL, 5, NULL);
}
