#ifndef DE_DE_MORPH_TRIE_H
#define DE_DE_MORPH_TRIE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t byte_val;
    bool is_end;
    uint32_t next_state;
    uint16_t child_count;
} MorphTrieNode;

extern const MorphTrieNode de_DE_morph_trie[115948];

#endif // DE_DE_MORPH_TRIE_H
