/* Hand-curated static const phoneme table for en-GB architecture pilot */
#include "espure_internal.h"

const espure_phoneme_t ESPURE_PHONEMES_EN_GB[44] = {
    {"\x01" ""   ,  1, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 0 */
    {"%"         ,  2, ESPURE_PHONEME_STRESS   , NULL, 1, 0, NULL}, /* 1 */
    {"%%"        ,  3, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 2 */
    {","         ,  4, ESPURE_PHONEME_STRESS   , NULL, 5, 0, NULL}, /* 3 */
    {",,"        ,  5, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 4 */
    {"'"         ,  6, ESPURE_PHONEME_STRESS   , NULL, 4, 0, NULL}, /* 5 */
    {"''"        ,  7, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 6 */
    {"="         ,  8, ESPURE_PHONEME_STRESS   , NULL, 4, 0, NULL}, /* 7 */
    {"_:"        ,  9, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 8 */
    {"_"         , 10, ESPURE_PHONEME_PAUSE    , NULL, 0, 0, NULL}, /* 9 */
    {"_!"        , 11, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 10 */
    {":"         , 12, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 11 */
    {"@"         , 13, ESPURE_PHONEME_VOWEL    , NULL, 0, ESPURE_PHONEME_FLAG_UNSTRESSED, NULL}, /* 12 */
    {"@-"        , 14, ESPURE_PHONEME_VOWEL    , NULL, 0, ESPURE_PHONEME_FLAG_UNSTRESSED, NULL}, /* 13 */
    {"||"        , 15, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 14 */
    {"*"         , 16, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 15 */
    {"1"         , 17, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 16 */
    {"#X1"       , 18, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 17 */
    {"?"         , 19, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 18 */
    {"-"         , 20, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 19 */
    {"_^_"       , 21, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 20 */
    {"_X1"       , 22, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 21 */
    {"_|"        , 23, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 22 */
    {"_::"       , 24, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 23 */
    {"t#"        , 25, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 24 */
    {"'!"        , 26, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 25 */
    {"_;_"       , 27, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 26 */
    {"#@"        , 28, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 27 */
    {"#a"        , 29, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 28 */
    {"#e"        , 30, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 29 */
    {"#i"        , 31, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 30 */
    {"#o"        , 32, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 31 */
    {"#u"        , 33, ESPURE_PHONEME_INVALID  , NULL, 0, 0, NULL}, /* 32 */
    {"@U"        , 34, ESPURE_PHONEME_VOWEL    , NULL, 0, 0, NULL}, /* 33 */
    {"3:"        , 35, ESPURE_PHONEME_VOWEL    , NULL, 0, 0, NULL}, /* 34 */
    {"E"         , 36, ESPURE_PHONEME_VOWEL    , NULL, 0, 0, NULL}, /* 35 */
    {"a"         , 37, ESPURE_PHONEME_VOWEL    , NULL, 0, 0, NULL}, /* 36 */
    {"b"         , 38, ESPURE_PHONEME_STOP     , NULL, 0, 0, NULL}, /* 37 */
    {"d"         , 39, ESPURE_PHONEME_STOP     , NULL, 0, 0, NULL}, /* 38 */
    {"h"         , 40, ESPURE_PHONEME_FRICATIVE, NULL, 0, 0, NULL}, /* 39 */
    {"j"         , 41, ESPURE_PHONEME_LIQUID   , NULL, 0, 0, NULL}, /* 40 */
    {"l"         , 42, ESPURE_PHONEME_LIQUID   , NULL, 0, 0, NULL}, /* 41 */
    {"s"         , 43, ESPURE_PHONEME_FRICATIVE, NULL, 0, 0, NULL}, /* 42 */
    {"w"         , 44, ESPURE_PHONEME_LIQUID   , NULL, 0, 0, NULL}, /* 43 */
};

const size_t ESPURE_NUM_PHONEMES_EN_GB = 44;
