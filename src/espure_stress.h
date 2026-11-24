/**
 * @file espure_stress.h
 * @brief Stress assignment rules and definitions
 */

#ifndef ESPURE_STRESS_H
#define ESPURE_STRESS_H

#include "espure_internal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const espure_phoneme_t* ph;  ///< Current phoneme
    uint8_t stresslevel;         ///< 0=diminished, 1=unstressed, 2-3=secondary, 4-5=primary
    uint8_t synthflags;          ///< SFLAG_SYLLABLE, etc.
    uint8_t newword;             ///< PHLIST_START_OF_WORD, PHLIST_START_OF_CLAUSE
} espure_phlist_entry_t;

/* Synthesis flags */
#define SFLAG_SYLLABLE      0x04
#define SFLAG_LENGTHEN      0x08

/* New-word flags */
#define PHLIST_START_OF_WORD     0x01
#define PHLIST_START_OF_CLAUSE   0x02
#define PHLIST_START_OF_SENTENCE 0x04

/* Stress levels */
#define STRESS_DIMINISHED    0
#define STRESS_UNSTRESSED    1
#define STRESS_SECONDARY     3
#define STRESS_PRIMARY       4
#define STRESS_PRIORITY      5

void espure_assign_word_stress(espure_phlist_entry_t* plist, size_t plist_len);

espure_err_t espure_stress_apply(
    const espure_phoneme_table_t* phoneme_table,
    char* phoneme_str,
    size_t buf_size
);

#ifdef __cplusplus
}
#endif

#endif /* ESPURE_STRESS_H */

