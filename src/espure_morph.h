#ifndef ESPURE_MORPH_H
#define ESPURE_MORPH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Checks if a word should be split morphologically and inserts a hyphen.
 * For example, "Kinderstube" -> "Kinder-stube".
 * 
 * @param word The input word.
 * @param out_buffer Buffer to store the hyphenated word.
 * @param max_len Maximum size of out_buffer.
 * @return true if a split was performed, false if the word was unchanged.
 */
bool espure_morph_split(const char* word, char* out_buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // ESPURE_MORPH_H
