#include <gtest/gtest.h>
#include <Windows.h>
#include <powrprof.h>

struct TestProcessorPowerInformation {
    ULONG number, maxMhz, currentMhz, mhzLimit, maxIdleState, currentIdleState;
};

static DWORD powerStatus;
static ULONG processorMhz;
static int powerCalls;

static BOOL WINAPI TestQueryPerformanceFrequency(LARGE_INTEGER *frequency) {
    frequency->QuadPart = 10000000;
    return TRUE;
}

static void WINAPI TestGetSystemInfo(SYSTEM_INFO *information) {
    *information = {};
    information->dwNumberOfProcessors = 2;
}

static DWORD WINAPI TestCallNtPowerInformation(POWER_INFORMATION_LEVEL level,
        PVOID input, ULONG inputSize, PVOID output, ULONG outputSize) {
    ++powerCalls;
    EXPECT_EQ(level, ProcessorInformation);
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(inputSize, 0u);
    EXPECT_EQ(outputSize, 2 * sizeof(TestProcessorPowerInformation));
    if (powerStatus == 0) {
        auto *processors = static_cast<TestProcessorPowerInformation *>(output);
        processors[0] = {0, processorMhz, processorMhz, processorMhz, 0, 0};
        processors[1] = processors[0];
    }
    return powerStatus;
}

// Compile the ARM timing path with a 10 MHz performance counter and independent
// processor power information, as reported by the Windows ARM64 hosted runner.
#define QueryPerformanceFrequency TestQueryPerformanceFrequency
#define GetSystemInfo TestGetSystemInfo
#define CallNtPowerInformation TestCallNtPowerInformation
#include "../../src/VxProcessor.cpp"
#undef CallNtPowerInformation
#undef GetSystemInfo
#undef QueryPerformanceFrequency

class ProcessorTimingWin32Test : public testing::Test {
protected:
    void SetUp() override {
        powerStatus = 0;
        processorMhz = 2400;
        powerCalls = 0;
        g_ProcessorFrequency = -1;
    }
};

TEST_F(ProcessorTimingWin32Test, PerformanceCounterRateDoesNotReplaceProcessorMHz) {
    MeasureProcessorTiming();
    EXPECT_EQ(g_ProcessorFrequency, 2400);
    EXPECT_EQ(powerCalls, 1);
    EXPECT_FLOAT_EQ(g_MSecondsPerCycle, 1000.0f / 10000000.0f);
}

TEST_F(ProcessorTimingWin32Test, FailedProcessorQueryReportsUnknownFrequency) {
    powerStatus = 0xc0000001u;
    MeasureProcessorTiming();
    EXPECT_EQ(g_ProcessorFrequency, 0);
    EXPECT_EQ(powerCalls, 1);
}

TEST_F(ProcessorTimingWin32Test, MissingProcessorClockDoesNotUseCounterFrequency) {
    processorMhz = 0;
    MeasureProcessorTiming();
    EXPECT_EQ(g_ProcessorFrequency, 0);
    EXPECT_EQ(powerCalls, 1);
}
