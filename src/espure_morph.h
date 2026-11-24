#ifndef ESPURE_MORPH_H
#define ESPURE_MORPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t byte_val;
    bool is_end;
    uint32_t next_state;
    uint16_t child_count;
} espure_morph_trie_node_t;

typedef struct {
    const char* const* items;
    size_t count;
} espure_morph_string_set_t;

typedef struct {
    const espure_morph_trie_node_t* root_trie;
    size_t root_trie_count;
    espure_morph_string_set_t initial_candidates;
    espure_morph_string_set_t inseparable_prefixes;
    espure_morph_string_set_t medial_candidates;
    espure_morph_string_set_t terminal_candidates;
    espure_morph_string_set_t short_roots;
    espure_morph_string_set_t linking_elements;
} espure_morph_language_data_t;

const espure_morph_language_data_t* espure_morph_data_de_DE(void);

/**
 * Inserts a morphological boundary for a language with registered morphology
 * data. Returns false and copies the input unchanged when no split is found.
 *
 * @param lang BCP 47 locale identifier; currently "de-DE" is supported.
 * @param word Input word.
 * @param out_buffer Output buffer.
 * @param max_len Capacity of out_buffer in bytes.
 */
bool espure_morph_split(const char* lang, const char* word,
                        char* out_buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // ESPURE_MORPH_H
