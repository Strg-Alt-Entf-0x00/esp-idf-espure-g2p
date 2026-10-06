#ifndef ESPURE_COMPILED_RULES_H
#define ESPURE_COMPILED_RULES_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint8_t* prog;
    size_t prog_len;
    const char* phonemes;
    const char* match_str;
} espure_compiled_rule_t;

typedef struct {
    const espure_compiled_rule_t* rules;
    size_t count;
} espure_rule_group_t;

typedef struct {
    uint16_t key;
    espure_rule_group_t group;
} espure_group2_t;

extern const espure_rule_group_t DE_GROUPS1[256];
extern const espure_group2_t DE_GROUPS2[];
extern const size_t DE_GROUPS2_COUNT;
extern const espure_rule_group_t DE_DEFAULT_GROUP;

#endif // ESPURE_COMPILED_RULES_H
