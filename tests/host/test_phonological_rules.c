/**
 * @file test_phonological_rules.c
 * @brief Unit tests for native German phonological rules engine
 */

#include "espure.h"
#include "espure_phonology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_invalid_args(void) {
    char buf[64] = "t a: g";
    assert(espure_phonology_apply(NULL, buf, sizeof(buf)) == ESPURE_ERR_INVALID_ARG);
    assert(espure_phonology_apply("de-DE", NULL, sizeof(buf)) == ESPURE_ERR_INVALID_ARG);
    assert(espure_phonology_apply("de-DE", buf, 0) == ESPURE_ERR_INVALID_ARG);
}

static void test_auslautverhaertung(void) {
    char buf[128];

    /* Tag: t a: g -> t a: k */
    strcpy(buf, "t a: g");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "t a: k") == 0);

    /* Bad: b a: d -> b a: t */
    strcpy(buf, "b a: d");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "b a: t") == 0);

    /* Dieb: d i: b -> d i: p */
    strcpy(buf, "d i: b");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "d i: p") == 0);

    /* und: u n d -> u n t */
    strcpy(buf, "u n d");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "u n t") == 0);

    /* aktiv: a k t i: v -> a k t i: f */
    strcpy(buf, "a k t i: v");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "a k t i: f") == 0);

    /* Tage: t ' a: g @ -> g is in syllable onset before vowel @ -> MUST NOT DEVOICE! */
    strcpy(buf, "t ' a: g @");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "t ' a: g @") == 0);

    /* Bäder: b ' E: d 6 -> d is onset before vowel 6 -> MUST NOT DEVOICE! */
    strcpy(buf, "b ' E: d 6");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "b ' E: d 6") == 0);

    /* Bruder: ' b r u: d 6 -> b is in initial onset cluster -> MUST NOT DEVOICE! */
    strcpy(buf, "' b r u: d 6");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' b r u: d 6") == 0);

    /* Blume: b l ' u: m @ -> b is in initial onset cluster -> MUST NOT DEVOICE! */
    strcpy(buf, "b l ' u: m @");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "b l ' u: m @") == 0);

    /* Drache: d r ' a x @ -> d is in initial onset cluster -> MUST NOT DEVOICE! */
    strcpy(buf, "d r ' a x @");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "d r ' a x @") == 0);

    /* Gras: ' g r a: s -> g is in initial onset cluster -> MUST NOT DEVOICE! */
    strcpy(buf, "' g r a: s");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' g r a: s") == 0);

    /* Glas: ' g l a: s -> g is in initial onset cluster -> MUST NOT DEVOICE! */
    strcpy(buf, "' g l a: s");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' g l a: s") == 0);

    /* Februar: f ' e: b r u a: 6 -> b is onset of -bruar (MOP) -> MUST NOT DEVOICE! */
    strcpy(buf, "f ' e: b r u a: 6");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "f ' e: b r u a: 6") == 0);

    /* Adler: ' a: d l 6 -> dl is NOT a valid German onset -> d IS coda -> MUST DEVOICE to t! */
    strcpy(buf, "' a: d l 6");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' a: t l 6") == 0);

    /* ab-reisen: ' a b - r ' aI z @ n -> b is coda before morpheme boundary -> MUST DEVOICE to p! */
    strcpy(buf, "' a b - r ' aI z @ n");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' a p - r ' aI z @ n") == 0);
}

static void test_ig_spirantization(void) {
    char buf[128];

    /* wenig: v ' e: n I g -> v ' e: n I C */
    strcpy(buf, "v ' e: n I g");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "v ' e: n I C") == 0);

    /* König: k ' 2: n I g -> k ' 2: n I C */
    strcpy(buf, "k ' 2: n I g");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "k ' 2: n I C") == 0);

    /* Dissimilation before -lich: königlich -> k ' 2: n I k l I C */
    strcpy(buf, "k ' 2: n I g l I C");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "k ' 2: n I k l I C") == 0);

    /* Retain plosive before vowel suffix: wenige -> v ' e: n I g @ */
    strcpy(buf, "v ' e: n I g @");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "v ' e: n I g @") == 0);
}

static void test_coda_r_vocalization(void) {
    char buf[128];

    /* Mutter: m ' U t @ r -> m ' U t 6 */
    strcpy(buf, "m ' U t @ r");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "m ' U t 6") == 0);

    /* Kinder: k ' I n d @ r -> k ' I n d 6 */
    strcpy(buf, "k ' I n d @ r");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "k ' I n d 6") == 0);

    /* Uhr: u: r -> u: 6 */
    strcpy(buf, "u: r");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "u: 6") == 0);

    /* Onset r must remain consonantal r: rot -> r o: t */
    strcpy(buf, "r o: t");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "r o: t") == 0);

    /* Onset r between vowels: hören -> h ' 2: r @ n */
    strcpy(buf, "h ' 2: r @ n");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "h ' 2: r @ n") == 0);
}

static void test_ich_ach_laut(void) {
    char buf[128];

    /* ich: I x -> I C */
    strcpy(buf, "I x");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "I C") == 0);

    /* Milch: m ' I l x -> m ' I l C */
    strcpy(buf, "m ' I l x");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "m ' I l C") == 0);

    /* echt: ' e: x t -> ' e: C t */
    strcpy(buf, "' e: x t");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "' e: C t") == 0);

    /* ach: a x -> a x (remains velar [x]) */
    strcpy(buf, "a x");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "a x") == 0);

    /* Buch: b u: x -> b u: x (remains velar [x]) */
    strcpy(buf, "b u: x");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "b u: x") == 0);

    /* Koch: k ' O x -> k ' O x (remains velar [x]) */
    strcpy(buf, "k ' O x");
    assert(espure_phonology_apply("de-DE", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "k ' O x") == 0);
}

static void test_locale_isolation(void) {
    char buf[128] = "t a: g";
    /* en-GB does not run German phonological rules */
    assert(espure_phonology_apply("en-GB", buf, sizeof(buf)) == ESPURE_OK);
    assert(strcmp(buf, "t a: g") == 0);
}

int main(void) {
    printf("Running German Phonological Rules Tests...\n");
    test_invalid_args();
    test_auslautverhaertung();
    test_ig_spirantization();
    test_coda_r_vocalization();
    test_ich_ach_laut();
    test_locale_isolation();
    printf("All German Phonological Rules Tests PASSED!\n");
    return 0;
}
