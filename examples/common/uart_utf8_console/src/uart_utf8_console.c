#include "uart_utf8_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"

#define MAX_LINE_LEN 512
#define HISTORY_SIZE 8
#define ESC_TIMEOUT_MS 25

typedef struct {
    char line[MAX_LINE_LEN];
    size_t length;
    size_t cursor;
    char history[HISTORY_SIZE][MAX_LINE_LEN];
    size_t history_count;
    size_t history_write_idx;
    size_t history_offset;
    char history_draft[MAX_LINE_LEN];
    bool browsing_history;
} line_editor_t;

static const char *s_prompt = "\x1b[36mESP-G2P>\x1b[0m ";
static uart_utf8_command_handler_t s_handler = NULL;

static inline bool is_continuation_byte(char c) {
    return (((unsigned char)c) & 0xC0) == 0x80;
}

static size_t previous_character(const char *text, size_t position) {
    if (position == 0) return 0;
    position--;
    while (position > 0 && is_continuation_byte(text[position])) {
        position--;
    }
    return position;
}

static size_t next_character(const char *text, size_t length, size_t position) {
    if (position >= length) return length;
    position++;
    while (position < length && is_continuation_byte(text[position])) {
        position++;
    }
    return position;
}

static size_t character_count(const char *text, size_t length) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < length) {
        pos = next_character(text, length, pos);
        count++;
    }
    return count;
}

static void redraw_line(const line_editor_t *editor) {
    printf("\r\x1b[2K%s", s_prompt);
    if (editor->length > 0) {
        fwrite(editor->line, 1, editor->length, stdout);
    }
    size_t cursor_from_end = character_count(editor->line + editor->cursor,
                                             editor->length - editor->cursor);
    if (cursor_from_end > 0) {
        printf("\x1b[%uD", (unsigned)cursor_from_end);
    }
    fflush(stdout);
}

static void insert_byte(line_editor_t *editor, uint8_t byte) {
    if (editor->length >= sizeof(editor->line) - 1) {
        putchar('\a');
        fflush(stdout);
        return;
    }
    memmove(editor->line + editor->cursor + 1,
            editor->line + editor->cursor,
            editor->length - editor->cursor);
    editor->line[editor->cursor++] = (char)byte;
    editor->length++;
    editor->line[editor->length] = '\0';
    editor->browsing_history = false;
    redraw_line(editor);
}

static void delete_previous_character(line_editor_t *editor) {
    if (editor->cursor == 0) return;
    size_t prev = previous_character(editor->line, editor->cursor);
    size_t bytes_to_remove = editor->cursor - prev;
    memmove(editor->line + prev,
            editor->line + editor->cursor,
            editor->length - editor->cursor + 1);
    editor->cursor = prev;
    editor->length -= bytes_to_remove;
    editor->browsing_history = false;
    redraw_line(editor);
}

static void delete_current_character(line_editor_t *editor) {
    if (editor->cursor >= editor->length) return;
    size_t next = next_character(editor->line, editor->length, editor->cursor);
    size_t bytes_to_remove = next - editor->cursor;
    memmove(editor->line + editor->cursor,
            editor->line + next,
            editor->length - next + 1);
    editor->length -= bytes_to_remove;
    editor->browsing_history = false;
    redraw_line(editor);
}

static void move_cursor_left(line_editor_t *editor) {
    if (editor->cursor == 0) return;
    editor->cursor = previous_character(editor->line, editor->cursor);
    redraw_line(editor);
}

static void move_cursor_right(line_editor_t *editor) {
    if (editor->cursor >= editor->length) return;
    editor->cursor = next_character(editor->line, editor->length, editor->cursor);
    redraw_line(editor);
}

static void add_history(line_editor_t *editor, const char *line) {
    if (!line || strlen(line) == 0) return;
    if (editor->history_count > 0) {
        size_t last_idx = (editor->history_write_idx + HISTORY_SIZE - 1) % HISTORY_SIZE;
        if (strcmp(editor->history[last_idx], line) == 0) return;
    }
    strlcpy(editor->history[editor->history_write_idx], line, sizeof(editor->history[0]));
    editor->history_write_idx = (editor->history_write_idx + 1) % HISTORY_SIZE;
    if (editor->history_count < HISTORY_SIZE) {
        editor->history_count++;
    }
}

static void set_line(line_editor_t *editor, const char *text) {
    strlcpy(editor->line, text, sizeof(editor->line));
    editor->length = strlen(editor->line);
    editor->cursor = editor->length;
    redraw_line(editor);
}

static void history_up(line_editor_t *editor) {
    if (editor->history_count == 0) return;
    if (!editor->browsing_history) {
        strlcpy(editor->history_draft, editor->line, sizeof(editor->history_draft));
        editor->browsing_history = true;
        editor->history_offset = 1;
    } else if (editor->history_offset < editor->history_count) {
        editor->history_offset++;
    } else {
        return;
    }
    size_t idx = (editor->history_write_idx + HISTORY_SIZE - editor->history_offset) % HISTORY_SIZE;
    set_line(editor, editor->history[idx]);
}

static void history_down(line_editor_t *editor) {
    if (!editor->browsing_history) return;
    if (editor->history_offset > 1) {
        editor->history_offset--;
        size_t idx = (editor->history_write_idx + HISTORY_SIZE - editor->history_offset) % HISTORY_SIZE;
        set_line(editor, editor->history[idx]);
    } else {
        editor->browsing_history = false;
        editor->history_offset = 0;
        set_line(editor, editor->history_draft);
    }
}

static int read_byte_with_timeout(TickType_t timeout_ticks) {
    uint8_t byte = 0;
    int len = uart_read_bytes(UART_NUM_0, &byte, 1, timeout_ticks);
    return (len > 0) ? (int)byte : -1;
}

static void console_task(void *pvParameters) {
    line_editor_t editor;
    memset(&editor, 0, sizeof(editor));

    printf("\n%s", s_prompt);
    fflush(stdout);

    while (1) {
        int c = read_byte_with_timeout(portMAX_DELAY);
        if (c < 0) continue;

        if (c == 0x1B) {
            // Escape sequence
            int c1 = read_byte_with_timeout(pdMS_TO_TICKS(ESC_TIMEOUT_MS));
            if (c1 == '[') {
                int c2 = read_byte_with_timeout(pdMS_TO_TICKS(ESC_TIMEOUT_MS));
                if (c2 == 'A') {
                    history_up(&editor);
                } else if (c2 == 'B') {
                    history_down(&editor);
                } else if (c2 == 'C') {
                    move_cursor_right(&editor);
                } else if (c2 == 'D') {
                    move_cursor_left(&editor);
                } else if (c2 == 'H' || c2 == '1') {
                    editor.cursor = 0;
                    redraw_line(&editor);
                } else if (c2 == 'F' || c2 == '4') {
                    editor.cursor = editor.length;
                    redraw_line(&editor);
                } else if (c2 == '3') {
                    int c3 = read_byte_with_timeout(pdMS_TO_TICKS(ESC_TIMEOUT_MS));
                    if (c3 == '~') {
                        delete_current_character(&editor);
                    }
                }
            }
            continue;
        }

        if (c == '\r' || c == '\n') {
            printf("\r\n");
            fflush(stdout);
            if (editor.length > 0) {
                add_history(&editor, editor.line);
                if (s_handler) {
                    s_handler(editor.line);
                }
            }
            editor.line[0] = '\0';
            editor.length = 0;
            editor.cursor = 0;
            editor.browsing_history = false;
            printf("%s", s_prompt);
            fflush(stdout);
        } else if (c == '\b' || c == 0x7F) {
            delete_previous_character(&editor);
        } else if (c == 0x01) { // Ctrl+A
            editor.cursor = 0;
            redraw_line(&editor);
        } else if (c == 0x05) { // Ctrl+E
            editor.cursor = editor.length;
            redraw_line(&editor);
        } else if (c == 0x15) { // Ctrl+U
            editor.line[0] = '\0';
            editor.length = 0;
            editor.cursor = 0;
            editor.browsing_history = false;
            redraw_line(&editor);
        } else if (c == 0x03) { // Ctrl+C
            printf("^C\r\n%s", s_prompt);
            editor.line[0] = '\0';
            editor.length = 0;
            editor.cursor = 0;
            editor.browsing_history = false;
            fflush(stdout);
        } else if (c >= 0x20 || (unsigned char)c >= 0x80) {
            insert_byte(&editor, (uint8_t)c);
        }
    }
}

esp_err_t uart_utf8_console_start(const char *prompt, uart_utf8_command_handler_t handler) {
    if (prompt) s_prompt = prompt;
    s_handler = handler;

    // Configure UART0 in raw mode
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(UART_NUM_0, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    uart_vfs_dev_use_driver(UART_NUM_0);

    BaseType_t ret = xTaskCreate(console_task, "utf8_console", 8192, NULL, 5, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
