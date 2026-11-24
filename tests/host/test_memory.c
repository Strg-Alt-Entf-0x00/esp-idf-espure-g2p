#include <stdint.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "espure_internal.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void reset_allocation_counters(void) {
    espure_test_spiram_alloc_attempts = 0;
    espure_test_internal_alloc_attempts = 0;
}

int main(void) {
    espure_config_t config = ESPURE_CONFIG_DEFAULT();
    config.use_psram = true;
    espure_test_fail_spiram_alloc = 1;

    reset_allocation_counters();
    if (heap_caps_malloc(2048, MALLOC_CAP_SPIRAM) != NULL ||
        espure_test_spiram_alloc_attempts != 1) {
        return fail("host PSRAM allocator stub did not apply its failure hook");
    }

    reset_allocation_counters();
    void* allocation = espure_malloc(2048, &config);
    if (!allocation || espure_test_spiram_alloc_attempts != 1 ||
        espure_test_internal_alloc_attempts != 1) {
        fprintf(stderr,
                "malloc fallback mismatch: allocation=%p, PSRAM attempts=%zu, "
                "internal attempts=%zu, use_psram=%d\n",
                allocation, espure_test_spiram_alloc_attempts,
                espure_test_internal_alloc_attempts, config.use_psram);
        return fail("malloc did not fall back from PSRAM to internal memory");
    }
    espure_free(allocation);

    reset_allocation_counters();
    allocation = espure_calloc(2, 1024, &config);
    if (!allocation || espure_test_spiram_alloc_attempts != 1 ||
        espure_test_internal_alloc_attempts != 1) {
        return fail("calloc did not fall back from PSRAM to internal memory");
    }
    for (size_t i = 0; i < 2048; i++) {
        if (((const unsigned char*)allocation)[i] != 0) {
            return fail("calloc returned non-zeroed memory");
        }
    }
    espure_free(allocation);

    reset_allocation_counters();
    allocation = espure_malloc_aligned(2048, sizeof(void*), &config);
    if (!allocation || (uintptr_t)allocation % sizeof(void*) != 0 ||
        espure_test_spiram_alloc_attempts != 1 ||
        espure_test_internal_alloc_attempts != 1) {
        return fail("aligned allocation did not fall back to aligned internal memory");
    }
    espure_free(allocation);

    config.use_psram = false;
    reset_allocation_counters();
    allocation = espure_malloc(2048, &config);
    if (!allocation || espure_test_spiram_alloc_attempts != 0 ||
        espure_test_internal_alloc_attempts != 1) {
        return fail("malloc ignored the runtime PSRAM configuration");
    }
    espure_free(allocation);
    config.use_psram = true;

    reset_allocation_counters();
    if (espure_calloc(SIZE_MAX, 2, &config) != NULL ||
        espure_test_spiram_alloc_attempts != 0 ||
        espure_test_internal_alloc_attempts != 0) {
        return fail("calloc did not reject multiplication overflow before allocation");
    }

    espure_test_fail_spiram_alloc = 0;
    puts("Memory allocation tests passed");
    return 0;
}
