#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

// strlcpy is a BSD extension, often missing in MSVC/glibc
size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t ret = strlen(src);
    if (size) {
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return ret;
}

// ESP-IDF memory allocation stubs not in header
void* heap_caps_realloc(void* ptr, size_t size, uint32_t caps) {
    return realloc(ptr, size);
}

void* heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
#ifdef _MSC_VER
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}
