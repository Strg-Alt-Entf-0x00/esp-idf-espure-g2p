/**
 * @file espure_phonology.h
 * @brief Native phonological rules engine for deterministic sound changes
 * 
 * Provides rule-based post-lexical phonological transformations:
 * - Auslautverhärtung (Final Devoicing)
 * - Suffix -ig Spirantization / Dissimilation
 * - Ich-Laut vs. Ach-Laut Distribution
 * - Coda-r Vocalization
 */

#ifndef ESPURE_PHONOLOGY_H
#define ESPURE_PHONOLOGY_H

#include "espure.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply locale-specific phonological rules to a spaced phoneme string.
 * 
 * Modifies the phoneme string in-place. If the buffer is too small for
 * expansion, returns ESPURE_ERR_BUFFER_TOO_SMALL.
 * 
 * @param locale Target locale (e.g. "de-DE")
 * @param phonemes Spaced phoneme mnemonic string (e.g. "t a: g")
 * @param buf_size Capacity of phonemes buffer
 * @return ESPURE_OK on success, error code otherwise
 */
espure_err_t espure_phonology_apply(const char *locale, char *phonemes, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ESPURE_PHONOLOGY_H */
