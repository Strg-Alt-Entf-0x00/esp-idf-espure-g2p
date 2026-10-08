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

size_t strlcat(char *dst, const char *src, size_t size) {
    size_t dlen = strlen(dst);
    if (dlen >= size) return size + strlen(src);
    size_t slen = strlen(src);
    size_t copy_len = (dlen + slen >= size) ? (size - dlen - 1) : slen;
    memcpy(dst + dlen, src, copy_len);
    dst[dlen + copy_len] = '\0';
    return dlen + slen;
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
