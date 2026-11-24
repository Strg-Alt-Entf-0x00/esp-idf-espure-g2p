#include <string.h>

/* Hand-curated non-rhotic British English mappings used by the pilot lexicon. */
typedef struct {
    const char* mnemonic;
    const char* ipa;
} en_ipa_mapping_t;

static const en_ipa_mapping_t EN_PILOT_IPA[] = {
    {"@", "ə"},
    {"@U", "əʊ"},
    {"3:", "ɜː"},
    {"E", "ɛ"},
    {"a", "æ"},
    {"b", "b"},
    {"d", "d"},
    {"h", "h"},
    {"j", "j"},
    {"l", "l"},
    {"s", "s"},
    {"w", "w"},
};

const char* espure_en_GB_ipa_lookup(const char* mnemonic) {
    if (!mnemonic) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(EN_PILOT_IPA) / sizeof(EN_PILOT_IPA[0]); i++) {
        if (strcmp(EN_PILOT_IPA[i].mnemonic, mnemonic) == 0) {
            return EN_PILOT_IPA[i].ipa;
        }
    }
    return NULL;
}
