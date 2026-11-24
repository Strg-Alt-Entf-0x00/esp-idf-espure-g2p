#ifndef ESP_HEAP_CAPS_H
#define ESP_HEAP_CAPS_H

#include <stdint.h>
#include <stdlib.h>

#define MALLOC_CAP_8BIT (1<<0)
#define MALLOC_CAP_SPIRAM (1<<1)
#define MALLOC_CAP_INTERNAL (1<<2)

extern int espure_test_fail_spiram_alloc;
extern size_t espure_test_spiram_alloc_attempts;
extern size_t espure_test_internal_alloc_attempts;

void* heap_caps_malloc(size_t size, uint32_t caps);
void* heap_caps_calloc(size_t n, size_t size, uint32_t caps);

static inline void heap_caps_free(void* ptr) {
    free(ptr);
}

static inline void* heap_caps_realloc(void* ptr, size_t size, uint32_t caps) {
    (void)caps;
    return realloc(ptr, size);
}

void* heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);

#endif // ESP_HEAP_CAPS_H
