#ifndef VXSIMDATOMICOPSINTERNAL_H
#define VXSIMDATOMICOPSINTERNAL_H

#include "VxMathDefines.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

static inline int VxAtomicLoadInt(volatile int *value) {
#if defined(_WIN32)
    return InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(value), 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#else
    return *value;
#endif
}

static inline void VxAtomicStoreInt(volatile int *value, int next) {
#if defined(_WIN32)
    InterlockedExchange(reinterpret_cast<volatile LONG *>(value), next);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#else
    *value = next;
#endif
}

static inline int VxAtomicIncrementInt(volatile int *value) {
#if defined(_WIN32)
    return InterlockedIncrement(reinterpret_cast<volatile LONG *>(value));
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(value, 1, __ATOMIC_ACQ_REL);
#else
    return ++(*value);
#endif
}

#endif // VXSIMDATOMICOPSINTERNAL_H
