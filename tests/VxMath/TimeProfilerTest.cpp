#include <gtest/gtest.h>
#include <SDL3/SDL_timer.h>

#include "VxTimeProfiler.h"

// Bound the sample by real counter readings, including any scheduler delay.
static void ExpectElapsedBetween(float milliseconds, Uint64 minimumTicks, Uint64 maximumTicks) {
    const double frequency = double(SDL_GetPerformanceFrequency());
    EXPECT_GE(milliseconds, float(double(minimumTicks) * 1000.0 / frequency));
    EXPECT_LE(milliseconds, float(double(maximumTicks) * 1000.0 / frequency));
}

TEST(VxTimeProfilerTest, DefaultConstructor_StartsAtConstruction) {
    const Uint64 before = SDL_GetPerformanceCounter();
    VxTimeProfiler profiler;
    const float elapsed = profiler.Current();
    const Uint64 after = SDL_GetPerformanceCounter();
    ExpectElapsedBetween(elapsed, 0, after - before);
}

TEST(VxTimeProfilerTest, Reset_ResetsTheTimer) {
    VxTimeProfiler profiler;
    SDL_Delay(2);
    const Uint64 before = SDL_GetPerformanceCounter();
    profiler.Reset();
    const Uint64 reset = SDL_GetPerformanceCounter();
    SDL_Delay(2);
    const Uint64 sample = SDL_GetPerformanceCounter();
    const float elapsed = profiler.Current();
    const Uint64 after = SDL_GetPerformanceCounter();
    ExpectElapsedBetween(elapsed, sample - reset, after - before);
}

TEST(VxTimeProfilerTest, CopyAssignment_CopiesStartTime) {
    VxTimeProfiler source;
    SDL_Delay(2);
    VxTimeProfiler copy;
    copy = source;

    for (int i = 0; i < 2; ++i) {
        const float before = source.Current();
        const float copied = copy.Current();
        const float after = source.Current();
        EXPECT_GE(copied, before);
        EXPECT_LE(copied, after);
        SDL_Delay(2);
    }
}

TEST(VxTimeProfilerTest, Current_MeasuresCumulativeTime) {
    const Uint64 before = SDL_GetPerformanceCounter();
    VxTimeProfiler profiler;
    const Uint64 start = SDL_GetPerformanceCounter();
    float previous = 0.0f;

    for (int i = 0; i < 2; ++i) {
        SDL_Delay(2);
        const Uint64 sample = SDL_GetPerformanceCounter();
        const float elapsed = profiler.Current();
        const Uint64 after = SDL_GetPerformanceCounter();
        ExpectElapsedBetween(elapsed, sample - start, after - before);
        EXPECT_GE(elapsed, previous);
        previous = elapsed;
    }
}

TEST(VxTimeProfilerTest, Split_MeasuresTimeAndResets) {
    const Uint64 before = SDL_GetPerformanceCounter();
    VxTimeProfiler profiler;
    const Uint64 start = SDL_GetPerformanceCounter();
    SDL_Delay(2);

    const Uint64 beforeSplit = SDL_GetPerformanceCounter();
    const float split = profiler.Split();
    const Uint64 afterSplit = SDL_GetPerformanceCounter();
    ExpectElapsedBetween(split, beforeSplit - start, afterSplit - before);

    for (int i = 0; i < 2; ++i) {
        const Uint64 sample = SDL_GetPerformanceCounter();
        const float restarted = profiler.Current();
        const Uint64 after = SDL_GetPerformanceCounter();
        ExpectElapsedBetween(restarted, sample - afterSplit, after - beforeSplit);
        SDL_Delay(2);
    }
}

TEST(VxTimeProfilerTest, EdgeCase_RapidCalls) {
    VxTimeProfiler profiler;
    const float first = profiler.Current();
    const float second = profiler.Current();
    EXPECT_GE(second, first);

    const Uint64 beforeReset = SDL_GetPerformanceCounter();
    profiler.Reset();
    const Uint64 afterReset = SDL_GetPerformanceCounter();
    const Uint64 beforeSplit = SDL_GetPerformanceCounter();
    const float split = profiler.Split();
    const Uint64 afterSplit = SDL_GetPerformanceCounter();
    ExpectElapsedBetween(split, beforeSplit - afterReset, afterSplit - beforeReset);

    const Uint64 sample = SDL_GetPerformanceCounter();
    const float elapsed = profiler.Current();
    const Uint64 after = SDL_GetPerformanceCounter();
    ExpectElapsedBetween(elapsed, sample - afterSplit, after - beforeSplit);
}
