/**
 * @file espure_phsource_parser.c
 * @brief Parser for phsource/phonemes files
 * 
 * Parses phoneme definition format:
 * 
 * phoneme X
 *   keyword value
 *   ...
 * endphoneme
 * 
 * Parser state is per invocation so initialization can safely run concurrently.
 */

#include "espure_internal.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <esp_log.h>

static const char* TAG = "espure_parser";

// Parser state
typedef struct {
    const char* input;
    size_t pos;
    size_t line;
    char keyword_buf[32];
    char value_buf[64];
    char line_buf[128];
    bool failed;
} parser_state_t;

/**
 * Skip whitespace and comments
 */
static void skip_whitespace(parser_state_t* state) {
    while (state->input[state->pos]) {
        char c = state->input[state->pos];
        
        // Skip whitespace
        if (isspace((unsigned char)c)) {
            if (c == '\n') state->line++;
            state->pos++;
            continue;
        }
        
        // Skip // comments
        if (c == '/' && state->input[state->pos + 1] == '/') {
            while (state->input[state->pos] && state->input[state->pos] != '\n') {
                state->pos++;
            }
            continue;
        }
        
        break;
    }
}

/**
 * Read a token (word or identifier)
 */
static bool read_token(parser_state_t* state, char* buffer, size_t buf_size) {
    if (buf_size == 0) {
        state->failed = true;
        return false;
    }
    skip_whitespace(state);
    
    size_t i = 0;
    bool truncated = false;
    while (state->input[state->pos] &&
           !isspace((unsigned char)state->input[state->pos]) &&
           state->input[state->pos] != '/') {
        if (i < buf_size - 1) {
            buffer[i++] = state->input[state->pos];
        } else {
            truncated = true;
        }
        state->pos++;
    }
    buffer[i] = '\0';

    if (truncated) {
        ESP_LOGE(TAG, "Line %zu: token exceeds %zu bytes", state->line, buf_size - 1);
        state->failed = true;
        return false;
    }
    if (i == 0 && state->input[state->pos] == '/') {
        ESP_LOGE(TAG, "Line %zu: unexpected '/' outside a comment", state->line);
        state->failed = true;
        return false;
    }
    return i > 0;
}

/**
 * Read rest of line
 */
static void read_line(parser_state_t* state, char* buffer, size_t buf_size) {
    if (buf_size == 0) {
        state->failed = true;
        return;
    }
    while (state->input[state->pos] == ' ' ||
           state->input[state->pos] == '\t' ||
           state->input[state->pos] == '\r') {
        state->pos++;
    }
    
    size_t i = 0;
    bool truncated = false;
    while (state->input[state->pos] && state->input[state->pos] != '\n') {
        if (i < buf_size - 1) {
            buffer[i++] = state->input[state->pos];
        } else {
            truncated = true;
        }
        state->pos++;
    }
    buffer[i] = '\0';
    
    // Trim trailing whitespace
    while (i > 0 && isspace((unsigned char)buffer[i - 1])) {
        buffer[--i] = '\0';
    }
    if (truncated) {
        ESP_LOGE(TAG, "Line %zu: line exceeds %zu bytes", state->line, buf_size - 1);
        state->failed = true;
    }
}

/**
 * Parse phoneme type keyword
 */
static uint8_t parse_type_keyword(const char* keyword) {
    if (strcmp(keyword, "stress") == 0) return 2;  // PH_STRESS
    if (strcmp(keyword, "pause") == 0) return 1;   // PH_PAUSE
    if (strcmp(keyword, "vwl") == 0 || strcmp(keyword, "vowel") == 0) return 3;  // PH_VOWEL
    if (strcmp(keyword, "liquid") == 0) return 4;  // PH_LIQUID
    if (strcmp(keyword, "stop") == 0 || strcmp(keyword, "stp") == 0) return 5;  // PH_STOP
    if (strcmp(keyword, "frc") == 0) return 7;     // PH_FRICATIVE
    if (strcmp(keyword, "nas") == 0 || strcmp(keyword, "nasal") == 0) return 9;  // PH_NASAL
    if (strcmp(keyword, "virtual") == 0) return 10; // PH_VIRTUAL
    if (strcmp(keyword, "afr") == 0) return ESPURE_PHONEME_AFFRICATE;
    
    return 0;  // INVALID
}

/**
 * Parse a single phoneme attribute
 */
static void parse_attribute(parser_state_t* state, espure_phoneme_t* phoneme) {
    if (!read_token(state, state->keyword_buf, sizeof(state->keyword_buf))) {
        return;
    }
    
    // Check for program: block
    if (strcmp(state->keyword_buf, "program:") == 0) {
        // Parse program block: multiple lines until "endprogram"
        // Store as raw text (will be interpreted at runtime)
        
        // Estimate size needed
        size_t program_start = state->pos;
        size_t program_end = program_start;
        
        // Find endprogram
        while (state->input[program_end]) {
            if (strncmp(&state->input[program_end], "endprogram", 10) == 0) {
                break;
            }
            program_end++;
        }
        
        if (state->input[program_end]) {
            // Allocate and copy program text
            size_t program_len = program_end - program_start;
            if (program_len > 0) {
                // Note: Using malloc directly (not espure_malloc) since we don't have config here
                // TODO: Pass config to parser for proper PSRAM allocation
                phoneme->program = (char*)malloc(program_len + 1);
                if (!phoneme->program) {
                    ESP_LOGE(TAG, "Line %zu: failed to allocate phoneme program",
                             state->line);
                    state->failed = true;
                    return;
                }
                memcpy(phoneme->program, &state->input[program_start], program_len);
                phoneme->program[program_len] = '\0';
                    
                char* p = phoneme->program;
                while (*p && isspace((unsigned char)*p)) p++;
                size_t len = strlen(p);
                while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
                p[len] = '\0';
                if (p != phoneme->program) {
                    memmove(phoneme->program, p, len + 1);
                }

                ESP_LOGD(TAG, "  program: %zu bytes", strlen(phoneme->program));
            }
            
            // Skip to after "endprogram"
            state->pos = program_end + 10;
            
            // Skip rest of line
            while (state->input[state->pos] && state->input[state->pos] != '\n') {
                state->pos++;
            }
        } else {
            ESP_LOGE(TAG, "Line %zu: unterminated phoneme program", state->line);
            state->failed = true;
        }
        
        return;
    }
    
    // Check if it's a type keyword
    uint8_t type = parse_type_keyword(state->keyword_buf);
    if (type != 0) {
        phoneme->type = type;
        return;
    }
    
    // Other attributes
    if (strcmp(state->keyword_buf, "stress_type") == 0) {
        if (read_token(state, state->value_buf, sizeof(state->value_buf))) {
            phoneme->stress_type = atoi(state->value_buf);
        } else {
            state->failed = true;
        }
    }
    else if (strcmp(state->keyword_buf, "ipa") == 0) {
        read_line(state, state->line_buf, sizeof(state->line_buf));
        // IPA allocation skipped for now (Phase 3 completion)
    }
    else if (strcmp(state->keyword_buf, "unstressed") == 0) {
        phoneme->flags |= 0x0001;  // FLAG_UNSTRESSED
    }
    else if (strcmp(state->keyword_buf, "nolink") == 0) {
        phoneme->flags |= 0x0002;  // FLAG_NOLINK
    }
    else if (strcmp(state->keyword_buf, "starttype") == 0 ||
             strcmp(state->keyword_buf, "endtype") == 0 ||
             strcmp(state->keyword_buf, "length") == 0 ||
             strcmp(state->keyword_buf, "lengthmod") == 0) {
        // Read and ignore for now
        read_token(state, state->value_buf, sizeof(state->value_buf));
        if (state->failed) {
            return;
        }
    }
    else {
        // Unknown keyword - skip rest of line
        read_line(state, state->line_buf, sizeof(state->line_buf));
    }
}

/**
 * Parse a phoneme block:
 * phoneme X
 *   ...
 * endphoneme
 */
static bool parse_phoneme(parser_state_t* state, espure_phoneme_t* phoneme) {
    // Read "phoneme"
    if (!read_token(state, state->keyword_buf, sizeof(state->keyword_buf)) ||
        strcmp(state->keyword_buf, "phoneme") != 0) {
        return false;
    }
    
    // Read mnemonic
    if (!read_token(state, state->value_buf, sizeof(state->value_buf))) {
        ESP_LOGE(TAG, "Line %zu: Expected phoneme mnemonic", state->line);
        state->failed = true;
        return false;
    }
    
    // Unescape backslashes: \, -> ,
    size_t j = 0;
    size_t i = 0;
    for (; state->value_buf[i] && j < sizeof(phoneme->mnemonic) - 1; i++) {
        if (state->value_buf[i] == '\\' && state->value_buf[i + 1]) {
            phoneme->mnemonic[j++] = state->value_buf[++i];
        } else {
            phoneme->mnemonic[j++] = state->value_buf[i];
        }
    }
    phoneme->mnemonic[j] = '\0';
    if (state->value_buf[i]) {
        ESP_LOGE(TAG, "Line %zu: phoneme mnemonic exceeds %zu bytes",
                 state->line, sizeof(phoneme->mnemonic) - 1);
        state->failed = true;
        return false;
    }
    
    ESP_LOGD(TAG, "Parsing phoneme '%s'", phoneme->mnemonic);
    
    // Parse attributes until "endphoneme"
    while (true) {
        skip_whitespace(state);
        
        // Check for endphoneme
        size_t saved_pos = state->pos;
        if (read_token(state, state->keyword_buf, sizeof(state->keyword_buf))) {
            // DEBUG
            // printf("DEBUG: Read token '%s'\\n", state->keyword_buf);
            if (strcmp(state->keyword_buf, "endphoneme") == 0) {
                break;
            }
            // Not endphoneme, restore position
            state->pos = saved_pos;
        } else if (!state->input[state->pos]) {
            ESP_LOGE(TAG, "Line %zu: unterminated phoneme '%s'",
                     state->line, phoneme->mnemonic);
            state->failed = true;
            return false;
        }
        
        parse_attribute(state, phoneme);
        if (state->failed) {
            return false;
        }
    }
    
    return true;
}

/**
 * Parse entire phsource file
 */
espure_err_t espure_parse_phsource(const char* content, 
                                    espure_phoneme_t* phonemes,
                                    size_t max_phonemes,
                                    size_t* num_parsed) {
    if (!content || !phonemes || !num_parsed) {
        return ESPURE_ERR_INVALID_ARG;
    }
    
    parser_state_t state = {
        .input = content,
        .pos = 0,
        .line = 1
    };
    
    *num_parsed = 0;
    
    ESP_LOGI(TAG, "Parsing phsource file...");
    
    while (state.input[state.pos]) {
        skip_whitespace(&state);
        
        if (!state.input[state.pos]) break;
        
        // Feed watchdog to prevent timeout during long parsing
        // Removed esp_task_wdt_reset(); to avoid "task not found" spam if task is not subscribed
        
        // Check for "phoneme" keyword
        size_t saved_pos = state.pos;
        if (!read_token(&state, state.keyword_buf, sizeof(state.keyword_buf))) {
            if (state.failed) {
                return ESPURE_ERR_INVALID_ARG;
            }
            break;
        }
        
        if (strcmp(state.keyword_buf, "phoneme") == 0) {
            // Restore position and parse phoneme
            state.pos = saved_pos;
            
            if (*num_parsed >= max_phonemes) {
                ESP_LOGE(TAG, "Maximum phoneme count exceeded (%zu)", max_phonemes);
                return ESPURE_ERR_INVALID_SIZE;
            }
            
            espure_phoneme_t* ph = &phonemes[*num_parsed];
            memset(ph, 0, sizeof(espure_phoneme_t));
            
            if (parse_phoneme(&state, ph)) {
                (*num_parsed)++;
            } else {
                free(ph->ipa);
                free(ph->program);
                memset(ph, 0, sizeof(*ph));
                return ESPURE_ERR_INVALID_ARG;
            }
            if (state.failed) {
                return ESPURE_ERR_INVALID_ARG;
            }
        }
        else if (strcmp(state.keyword_buf, "phonemetable") == 0) {
            // Skip phonemetable declaration
            read_line(&state, state.line_buf, sizeof(state.line_buf));
            if (state.failed) {
                return ESPURE_ERR_INVALID_SIZE;
            }
        }
        else {
            // Unknown keyword - skip line
            read_line(&state, state.line_buf, sizeof(state.line_buf));
            if (state.failed) {
                return ESPURE_ERR_INVALID_SIZE;
            }
        }
    }
    
    ESP_LOGI(TAG, "Parsed %zu phonemes", *num_parsed);
    return ESPURE_OK;
}
