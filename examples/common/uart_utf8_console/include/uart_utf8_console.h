#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*uart_utf8_command_handler_t)(const char *line);

/**
 * @brief Initialize and start the interactive UART UTF-8 console task
 *
 * Handles UTF-8 multi-byte characters (ä, ö, ü, ß), VT100 navigation
 * escape sequences, backspace/delete, line redraws, and command history.
 *
 * @param prompt Prompt string to display (e.g. "\x1b[36mESP-G2P>\x1b[0m ")
 * @param handler Function pointer to invoke when a full line is entered
 * @return esp_err_t ESP_OK on success
 */
esp_err_t uart_utf8_console_start(const char *prompt, uart_utf8_command_handler_t handler);

#ifdef __cplusplus
}
#endif
