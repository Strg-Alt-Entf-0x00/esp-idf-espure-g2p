/**
 * @file test_common.h
 * @brief Common test infrastructure definitions
 * 
 * Shared types and structures used across all test data files.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdint.h>

/**
 * @brief Test case structure
 * 
 * Contains a word and its expected IPA phoneme output from ground truth.
 */
typedef struct {
    const char* word;           /**< Input word */
    const char* expected_ipa;   /**< Expected IPA phonemes (ground truth from espure-python) */
} test_case_t;

#endif // TEST_COMMON_H
