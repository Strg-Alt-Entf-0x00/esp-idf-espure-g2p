#include "espure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

static void test_stream_api(void) {
    espure_handle_t handle = NULL;
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = false;

    TEST_ASSERT(espure_init("de-DE", &config, &handle) == ESPURE_OK);

    /* 1. Invalid args */
    espure_segment_t segs[64];
    espure_stream_t stream;
    TEST_ASSERT(espure_phonemize_stream(NULL, "test", segs, 64, &stream) == ESPURE_ERR_NOT_INITIALIZED);
    TEST_ASSERT(espure_phonemize_stream(handle, NULL, segs, 64, &stream) == ESPURE_ERR_INVALID_ARG);
    TEST_ASSERT(espure_phonemize_stream(handle, "test", NULL, 64, &stream) == ESPURE_ERR_INVALID_ARG);
    TEST_ASSERT(espure_phonemize_stream(handle, "test", segs, 0, &stream) == ESPURE_ERR_INVALID_ARG);
    TEST_ASSERT(espure_phonemize_stream(handle, "test", segs, 64, NULL) == ESPURE_ERR_INVALID_ARG);

    /* 2. Normal stream phonemization */
    TEST_ASSERT(espure_phonemize_stream(handle, "könig", segs, 64, &stream) == ESPURE_OK);
    TEST_ASSERT(stream.version == 0x0100);
    TEST_ASSERT(stream.segment_count > 0);
    TEST_ASSERT(stream.segments == segs);

    printf("Word 'könig' -> %u segments:\n", stream.segment_count);
    bool found_stress = false;
    for (uint16_t i = 0; i < stream.segment_count; i++) {
        const espure_segment_t *s = &stream.segments[i];
        printf("  [%u] seg_id=%u, stress=%u, boundary=%u, length=%u\n",
               i, s->segment_id, s->stress, s->boundary, s->length_modifier);
        TEST_ASSERT(s->segment_id > 0);
        if (s->stress == ESPURE_STRESS_PRIMARY) {
            found_stress = true;
        }
    }
    TEST_ASSERT(found_stress);
    TEST_ASSERT(stream.segments[stream.segment_count - 1].boundary == ESPURE_BOUNDARY_PAUSE);

    /* 3. Multi-word stream phonemization with normalization */
    TEST_ASSERT(espure_phonemize_stream(handle, "Dr. Müller", segs, 64, &stream) == ESPURE_OK);
    TEST_ASSERT(stream.segment_count > 4);

    bool found_word_boundary = false;
    for (uint16_t i = 0; i < stream.segment_count; i++) {
        if (stream.segments[i].boundary == ESPURE_BOUNDARY_WORD) {
            found_word_boundary = true;
            break;
        }
    }
    TEST_ASSERT(found_word_boundary);

    espure_deinit(handle);
    printf("[PASS] test_stream_api\n");
}

int main(void) {
    test_stream_api();
    printf("ALL STREAM TESTS PASSED\n");
    return 0;
}
