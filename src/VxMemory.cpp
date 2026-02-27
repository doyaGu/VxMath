#include "VxMemory.h"

#include <stdlib.h>
#include <stdint.h>

// Basic memory allocation functions
void *mynew(size_t n) {
    void *ptr = operator new(n);
    return ptr;
}

void mydelete(void *a) {
    if (a) {
        operator delete(a);
    }
}

// Array allocation functions
void *mynewarray(size_t n) {
    void *ptr = operator new[](n);
    return ptr;
}

void mydeletearray(void *a) {
    if (a) {
        operator delete[](a);
    }
}

void *VxNewAligned(size_t size, size_t align) {
    size_t effectiveAlign = (align > 0) ? align : sizeof(void *);
    if (effectiveAlign < sizeof(void *)) {
        effectiveAlign = sizeof(void *);
    }
    if ((effectiveAlign & (effectiveAlign - 1)) != 0) {
        size_t pow2 = sizeof(void *);
        while (pow2 < effectiveAlign) {
            pow2 <<= 1;
        }
        effectiveAlign = pow2;
    }

#if defined(_MSC_VER)
    return _aligned_malloc(size, effectiveAlign);
#else
    const size_t totalSize = size + effectiveAlign - 1 + sizeof(void *);
    void *raw = malloc(totalSize);
    if (!raw) {
        return nullptr;
    }
    const uintptr_t rawAddr = reinterpret_cast<uintptr_t>(raw) + sizeof(void *);
    const uintptr_t alignedAddr = (rawAddr + (effectiveAlign - 1)) & ~(static_cast<uintptr_t>(effectiveAlign) - 1);
    reinterpret_cast<void **>(alignedAddr)[-1] = raw;
    return reinterpret_cast<void *>(alignedAddr);
#endif
}

void VxDeleteAligned(void *ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    if (!ptr) {
        return;
    }
    void *raw = reinterpret_cast<void **>(ptr)[-1];
    free(raw);
#endif
}
