#include "VxTimeProfiler.h"

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
    *(DWORD64 *) &Times[0] = VxReadCounter();
}

float VxTimeProfiler::Current() {
    *(DWORD64 *) &Times[2] = VxReadCounter() - *(DWORD64 *) &Times[0];
    return (float) *(DWORD64 *) &Times[2] * g_MSecondsPerCycle;
}
