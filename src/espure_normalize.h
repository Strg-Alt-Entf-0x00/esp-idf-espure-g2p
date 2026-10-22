/**
 * @file espure_normalize.h
 * @brief Stage 1: Text Normalization Engine for ESPure G2P
 * 
 * Language-independent normalization framework. Language-specific abbreviation
 * tables, number-to-words converters, and currency/unit expansions are owned
 * by each locale directory under lang/<locale>/.
 */

#ifndef ESPURE_NORMALIZE_H
#define ESPURE_NORMALIZE_H

#include "espure.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Abbreviation entry mapping short text to expansion.
 */
typedef struct {
    const char *abbrev;
    const char *expansion;
    bool case_sensitive;
} espure_abbrev_entry_t;

/**
 * @brief Locale-specific normalization operations interface.
 */
typedef struct {
    const char *locale;
    const espure_abbrev_entry_t *abbrevs;
    size_t num_abbrevs;
    espure_err_t (*number_to_words)(int64_t number, char *output, size_t out_size);
    espure_err_t (*expand_symbol)(const char *sym, char *output, size_t out_size, size_t *bytes_consumed);
} espure_normalize_ops_t;

/**
 * @brief Get normalization operations for a given BCP 47 locale.
 * 
 * @param locale Exact BCP 47 locale (e.g. "de-DE", "en-GB")
 * @return Operations pointer or NULL if locale does not have custom normalization
 */
const espure_normalize_ops_t* espure_normalize_get_ops(const char *locale);

/**
 * @brief Normalize text for speech synthesis (Stage 1).
 * 
 * @param locale Target locale (e.g. "de-DE", "en-GB")
 * @param input Raw UTF-8 input text
 * @param output Destination buffer for normalized text
 * @param out_size Size of destination buffer
 * @return ESPURE_OK on success, error code otherwise
 */
espure_err_t espure_normalize_text(const char *locale,
                                   const char *input,
                                   char *output,
                                   size_t out_size);

/**
 * @brief Convert cardinal integer (0 to 999,999) to German words.
 */
espure_err_t espure_number_to_german_words(int64_t number,
                                           char *output,
                                           size_t out_size);

/**
 * @brief Convert cardinal integer (0 to 999,999) to English words.
 */
espure_err_t espure_number_to_english_words(int64_t number,
                                            char *output,
                                            size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* ESPURE_NORMALIZE_H */
