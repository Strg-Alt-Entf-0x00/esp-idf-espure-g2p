#include "espure_morph.h"
#include "de_DE_morph_trie.h"

/* These are legacy segmentation candidates, not a complete grammatical taxonomy. */
static const char* const initial_candidates[] = {
    "ab", "an", "auf", "aus", "be", "bei", "da", "dar", "durch", "ein", "emp",
    "ent", "er", "fort", "ge", "her", "hin", "hinter", "mit", "nach", "nieder",
    "ober", "um", "un", "unter", "ur", "ver", "vor", "voran", "voraus", "vorbei", "weg",
    "weiter", "wider", "wieder", "zer", "zu", "zurecht", "zurueck", "zusammen", "zwischen",
    "bundes", "kinder", "haupt", "sonder", "super", "halb", "lieblings"
};

static const char* const inseparable_prefixes[] = {
    "be", "emp", "ent", "er", "ge", "ver", "zer"
};

static const char* const medial_candidates[] = {
    "bar", "chen", "ei", "en", "end", "er", "haft", "heit", "ie", "ig", "in",
    "isch", "keit", "lein", "lich", "ling", "nis", "sal", "sam", "schaft",
    "tum", "ung", "werk", "los", "voll", "mäßig", "innen",
    "der", "dem", "den", "des", "ren", "test", "est", "este", "esten", "ester", "estes", "ers", "ern"
};

static const char* const terminal_candidates[] = {
    "e", "en", "er", "es", "em", "s", "t", "st", "te", "ten", "tet", "test",
    "tem", "ter", "tes", "etem", "eter", "etes",
    "ete", "eten", "etet", "etest",
    "nd", "nde", "nden", "ndem", "ndes", "nder", "n", "est", "este", "esten",
    "ester", "estes", "estem", "der", "dem", "den", "des",
    "ende", "enden", "endem", "endes", "ender",
    "elnd", "elnde", "elnden", "elndem", "elndes", "elnder",
    "ernd", "ernde", "ernden", "erndem", "erndes", "ernder",
    "keit", "heit", "ung", "schaft", "nis", "tum", "sal", "sam", "los", "bar",
    "ungen", "heiten", "keiten", "schaften", "nisse", "nissen", "ereien", "eleien",
    "tuemer", "tuemern", "tümer", "tümern", "linge", "lingen",
    "lich", "isch", "ig", "lein", "chen", "ling", "werk", "voll", "mäßig", "innen"
};

static const char* const short_roots[] = {
    "ei", "öl", "au"
};

static const char* const linking_elements[] = {
    "", "s", "es", "n", "en", "e", "er"
};

#define STRING_SET(items_) { items_, sizeof(items_) / sizeof((items_)[0]) }

const espure_morph_language_data_t* espure_morph_data_de_DE(void) {
    static const espure_morph_language_data_t data = {
        .root_trie = de_DE_morph_trie,
        .root_trie_count = sizeof(de_DE_morph_trie) / sizeof(de_DE_morph_trie[0]),
        .initial_candidates = STRING_SET(initial_candidates),
        .inseparable_prefixes = STRING_SET(inseparable_prefixes),
        .medial_candidates = STRING_SET(medial_candidates),
        .terminal_candidates = STRING_SET(terminal_candidates),
        .short_roots = STRING_SET(short_roots),
        .linking_elements = STRING_SET(linking_elements)
    };
    return &data;
}

#undef STRING_SET
