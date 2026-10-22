#include <string.h>

typedef struct {
    const char* mnemonic;
    const char* ipa;
} de_ipa_mapping_t;

static const de_ipa_mapping_t DE_IPA_MAP[] = {
    {"6", "ɐ"},
    {"W", "œ"},
    {"3", "ɐ"},
    {"x", "x"},
    {"Y:", "øː"},
    {"y:", "yː"},
    {"Y", "ʏ"},
    {"2", ""},
    {"9", "œ"},
    {"C", "ç"},
    {"E", "ɛ"},
    {"O", "ɔ"},
    {"E:", "ɛː"},
    {"O:", "ɔː"},
    {"2:", "øː"},
    {"9:", "œː"},
    {"@n", "ən"},
    {"@st", "əst"},
    {"@r", "ɐ"},
    {"@m", "əm"},
    {"@l", "əl"},
    {"@-", "ə"},
    {"OY", "ɔʏ"},
    {"=o:", "oː"},
    {"U", "ʊ"},
    {"I", "ɪ"},
    {"V", "ʌ"},
    {"A:", "aː"},
    {"a", "a"},
    {"u:", "uː"},
    {"i:", "iː"},
    {"i", "i"},
    {"o:", "oː"},
    {"y", "y"},
    {":", "ː"},
    {"b", "b"},
    {"d", "d"},
    {"f", "f"},
    {"g", "ɡ"},
    {"h", "h"},
    {"j", "j"},
    {"k", "k"},
    {"l", "l"},
    {"m", "m"},
    {"n", "n"},
    {"p", "p"},
    {"pF", "pf"},
    {"pf", "pf"},
    {"r", "ʁ"},
    {"s", "s"},
    {"t", "t"},
    {"ts", "ts"},
    {"u", "u"},
    {"v", "v"},
    {"w", "w"},
    {"z", "z"},
    {"T", "θ"},
    {"D", "ð"},
    {"S", "ʃ"},
    {"Z", "ʒ"},
    {"N", "ŋ"},
    {"tS", "tʃ"},
    {"dZ", "dʒ"},
    {"@", "ə"},
    {"e", "e"},
    {"o", "o"},
    {"A", "a"},
    {"3:", "ɐ"},
    {"eI", "eɪ"},
    {"aI", "aɪ"},
    {"OI", "ɔɪ"},
    {"aU", "aʊ"},
    {"@U", "əʊ"},
    {"oU", "oʊ"},
    {"kw", "kw"},
    {"ks", "ks"},
    {"!", "ʔ"},
    {"_!", "ʔ"},
};

const char* espure_german_ipa_lookup(const char* mnemonic) {
    if (!mnemonic) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(DE_IPA_MAP) / sizeof(DE_IPA_MAP[0]); i++) {
        if (strcmp(DE_IPA_MAP[i].mnemonic, mnemonic) == 0) {
            return DE_IPA_MAP[i].ipa;
        }
    }
    return NULL;
}
