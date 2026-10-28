#include "VxTimeProfiler.h"
#include <gtest/gtest.h>
#include <SDL3/SDL_timer.h>
#include <math.h>
#include <new>
#include <stdint.h>
#include <string.h>

class TimeProfilerProbe : public VxTimeProfiler {
public:
    using VxTimeProfiler::Times;

    Uint64 Start() const {
        Uint64 value;
        memcpy(&value, Times, sizeof(value));
        return value;
    }
    Uint64 Sample() const {
        Uint64 value;
        memcpy(&value, Times + 2, sizeof(value));
        return value;
    }
    void SetStart(Uint64 value) { memcpy(Times, &value, sizeof(value)); }
    void SetSample(Uint64 value) { memcpy(Times + 2, &value, sizeof(value)); }
};

static void ExpectMilliseconds(float milliseconds, Uint64 ticks) {
    double expected = (double(ticks) / SDL_GetPerformanceFrequency()) * 1000.0;
    EXPECT_NEAR(milliseconds, expected, fabs(expected) * 0x1p-22 + 0x1p-40);
}

TEST(TimeProfilerReferenceTest, LayoutKeepsFourWordsAtFourByteAlignment) {
    TimeProfilerProbe timer;
    EXPECT_EQ(16u, sizeof(VxTimeProfiler));
    EXPECT_EQ(4u, alignof(VxTimeProfiler));
    EXPECT_EQ(sizeof(VxTimeProfiler), sizeof(timer));
    EXPECT_EQ(16u, sizeof(timer.Times));
    EXPECT_EQ(static_cast<void *>(&timer), static_cast<void *>(timer.Times));
}

TEST(TimeProfilerReferenceTest, ConstructionSamplesTheCounterWithoutWritingTheLastSample) {
    alignas(TimeProfilerProbe) unsigned char storage[sizeof(TimeProfilerProbe)];
    memset(storage, 0xa5, sizeof(storage));
    Uint64 before = SDL_GetPerformanceCounter();
    TimeProfilerProbe *timer = new (storage) TimeProfilerProbe;
    Uint64 after = SDL_GetPerformanceCounter();
    EXPECT_GE(timer->Start(), before);
    EXPECT_LE(timer->Start(), after);
    const unsigned char expected[8] = {0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
    EXPECT_EQ(0, memcmp(storage + 8, expected, sizeof(expected)));
    timer->~TimeProfilerProbe();
}

TEST(TimeProfilerReferenceTest, ResetUpdatesOnlyTheStartCounter) {
    TimeProfilerProbe timer;
    timer.SetSample(UINT64_C(0x1234567887654321));
    Uint64 before = SDL_GetPerformanceCounter();
    timer.Reset();
    Uint64 after = SDL_GetPerformanceCounter();
    EXPECT_GE(timer.Start(), before);
    EXPECT_LE(timer.Start(), after);
    EXPECT_EQ(UINT64_C(0x1234567887654321), timer.Sample());
}

TEST(TimeProfilerReferenceTest, CurrentIsCumulativeAndStoresTheFullCounterDifference) {
    TimeProfilerProbe timer;
    Uint64 start = timer.Start();
    Uint64 previous = 0;
    for (int i = 0; i < 8; ++i) {
        Uint64 before = SDL_GetPerformanceCounter();
        float milliseconds = timer.Current();
        Uint64 after = SDL_GetPerformanceCounter();
        EXPECT_EQ(start, timer.Start());
        EXPECT_GE(timer.Sample(), before - start);
        EXPECT_LE(timer.Sample(), after - start);
        EXPECT_GE(timer.Sample(), previous);
        ExpectMilliseconds(milliseconds, timer.Sample());
        previous = timer.Sample();
    }
}

TEST(TimeProfilerReferenceTest, ElapsedCounterKeepsHighWordsAndLowWordBorrow) {
    const Uint64 intervals[] = {UINT64_C(0x100000021), UINT64_C(0x10000000021)};
    for (Uint64 interval : intervals) {
        TimeProfilerProbe timer;
        Uint64 before = SDL_GetPerformanceCounter();
        timer.SetStart(before - interval);
        float milliseconds = timer.Current();
        Uint64 after = SDL_GetPerformanceCounter();
        EXPECT_EQ(before - interval, timer.Start());
        EXPECT_GE(timer.Sample(), interval);
        EXPECT_LE(timer.Sample(), interval + after - before);
        EXPECT_NE(0u, timer.Times[3]);
        ExpectMilliseconds(milliseconds, timer.Sample());
    }
}

TEST(TimeProfilerReferenceTest, AssignmentAndImplicitCopyPreserveAllFourWords) {
    TimeProfilerProbe source, destination;
    const XDWORD words[] = {0x12345678, 0x90abcdef, 0x76543210, 0xfedcba98};
    memcpy(source.Times, words, sizeof(words));
    VxTimeProfiler &assigned = (static_cast<VxTimeProfiler &>(destination) = source);
    EXPECT_EQ(&destination, &assigned);
    EXPECT_EQ(0, memcmp(destination.Times, words, sizeof(words)));
    EXPECT_EQ(0, memcmp(source.Times, words, sizeof(words)));
    VxTimeProfiler &self = destination;
    EXPECT_EQ(&destination, &(self = self));
    EXPECT_EQ(0, memcmp(destination.Times, words, sizeof(words)));
    TimeProfilerProbe copy(source);
    EXPECT_EQ(0, memcmp(copy.Times, words, sizeof(words)));
}

TEST(TimeProfilerReferenceTest, SplitReturnsTheSampleBeforeResetAndKeepsItStored) {
    TimeProfilerProbe timer;
    Uint64 interval = SDL_GetPerformanceFrequency();
    timer.SetStart(SDL_GetPerformanceCounter() - interval);
    Uint64 oldStart = timer.Start();
    Uint64 before = SDL_GetPerformanceCounter();
    float milliseconds = timer.Split();
    Uint64 after = SDL_GetPerformanceCounter();
    EXPECT_GE(timer.Start(), before);
    EXPECT_LE(timer.Start(), after);
    EXPECT_GE(timer.Sample(), before - oldStart);
    EXPECT_LE(timer.Sample(), after - oldStart);
    ExpectMilliseconds(milliseconds, timer.Sample());
    float restarted = timer.Current();
    Uint64 final = SDL_GetPerformanceCounter();
    EXPECT_LE(timer.Sample(), final - before);
    ExpectMilliseconds(restarted, timer.Sample());
}

TEST(TimeProfilerReferenceTest, LegalFourByteAlignedObjectsKeepTheirBoundsAndWorkNormally) {
    alignas(8) unsigned char storage[24];
    memset(storage, 0x5a, sizeof(storage));
    TimeProfilerProbe *timer = new (storage + 4) TimeProfilerProbe;
    ASSERT_EQ(4u, reinterpret_cast<uintptr_t>(timer) % 8);
    timer->SetSample(UINT64_C(0x8765432112345678));
    timer->Reset();
    EXPECT_EQ(UINT64_C(0x8765432112345678), timer->Sample());
    Uint64 start = timer->Start();
    float milliseconds = timer->Current();
    ExpectMilliseconds(milliseconds, timer->Sample());
    EXPECT_EQ(start, timer->Start());
    TimeProfilerProbe copy;
    static_cast<VxTimeProfiler &>(copy) = *timer;
    EXPECT_EQ(0, memcmp(copy.Times, timer->Times, sizeof(copy.Times)));
    const unsigned char expected[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    EXPECT_EQ(0, memcmp(storage, expected, sizeof(expected)));
    EXPECT_EQ(0, memcmp(storage + 20, expected, sizeof(expected)));
    timer->~TimeProfilerProbe();
}

TEST(TimeProfilerReferenceTest, RealElapsedTimeMatchesAnIndependentCounterInterval) {
    Uint64 before = SDL_GetPerformanceCounter();
    TimeProfilerProbe timer;
    SDL_Delay(2);
    float milliseconds = timer.Current();
    Uint64 after = SDL_GetPerformanceCounter();
    EXPECT_GE(milliseconds, 0.0f);
    EXPECT_LE(double(milliseconds), double(after - before) / SDL_GetPerformanceFrequency() * 1000.0 + 0.001);
    ExpectMilliseconds(milliseconds, timer.Sample());
}
