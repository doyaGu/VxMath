#include "VxTimeProfiler.h"
#include "VxMath.h"

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <intrin.h>
#endif

extern float g_MSecondsPerCycle;

static inline DWORD64 VxReadCounter() {
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    return __rdtsc();
#else
    LARGE_INTEGER counter;
    ::QueryPerformanceCounter(&counter);
    return static_cast<DWORD64>(counter.QuadPart);
#endif
}

VxTimeProfiler &VxTimeProfiler::operator=(const VxTimeProfiler &t) {
    if (&t != this)
        memcpy(Times, t.Times, sizeof(Times));
    return *this;
}

void VxTimeProfiler::Reset() {
    const uint64_t now = static_cast<uint64_t>(VxReadCounter());
    memcpy(&Times[0], &now, sizeof(now));
    const uint64_t zero = 0;
    memcpy(&Times[2], &zero, sizeof(zero));
}

float VxTimeProfiler::Current() {
    if (g_MSecondsPerCycle <= 0.0f) {
        VxDetectProcessor();
    }

    uint64_t start = 0;
    memcpy(&start, &Times[0], sizeof(start));

    const uint64_t elapsed = static_cast<uint64_t>(VxReadCounter()) - start;
    memcpy(&Times[2], &elapsed, sizeof(elapsed));

    return static_cast<float>(elapsed) * g_MSecondsPerCycle;
}
