/**
 * @file VxTimeProfilerSDL3.cpp
 * @brief SDL3 implementation of VxTimeProfiler high-precision timing.
 *
 * This implementation uses SDL3's high-resolution counter for portable
 * performance timing across all platforms.
 */

#include "VxTimeProfiler.h"

#include <SDL3/SDL_timer.h>
#include <string.h>

VxTimeProfiler &VxTimeProfiler::operator=(const VxTimeProfiler &t) {
    if (&t != this)
        memcpy(Times, t.Times, sizeof(Times));
    return *this;
}

void VxTimeProfiler::Reset() {
    // Store the SDL performance counter value in Times[0..1]
    Uint64 start = SDL_GetPerformanceCounter();
    memcpy(Times, &start, sizeof(start));
}

float VxTimeProfiler::Current() {
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 start;
    memcpy(&start, Times, sizeof(start));
    Uint64 elapsed = now - start;

    // Store elapsed in Times[2..3] for compatibility
    memcpy(Times + 2, &elapsed, sizeof(elapsed));

    // Convert to milliseconds using SDL's performance frequency
    Uint64 freq = SDL_GetPerformanceFrequency();
    return (float)((double)elapsed * 1000.0 / (double)freq);
}
