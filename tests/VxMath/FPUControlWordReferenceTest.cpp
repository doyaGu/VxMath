#include <fenv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#ifndef VX_FPU_WORD_PROBE
#include "VxWindowFunctions.h"
#include <gtest/gtest.h>
#endif

#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)
extern "C" void FPUSeedControlWord(unsigned short word);
extern "C" int FPURoundToInt(double value);
extern "C" int FPUPrecisionDifference(double base);
static unsigned short FPUHardwareWord() {
    alignas(16) unsigned char state[512];
    _fxsave64(state);
    unsigned short word; memcpy(&word, state, sizeof(word));
    return word;
}
static unsigned short FPUHardwareStatus() {
    alignas(16) unsigned char state[512];
    _fxsave64(state);
    unsigned short status; memcpy(&status, state + 2, sizeof(status));
    return status;
}
#elif defined(_MSC_VER) && defined(_M_IX86)
static unsigned short FPUHardwareWord() {
    unsigned short controlWord;
    __asm fnstcw controlWord
    return controlWord;
}
static unsigned short FPUHardwareStatus() {
    unsigned short status;
    __asm fnstsw status
    return status;
}
static void FPUSeedControlWord(unsigned short controlWord) {
    __asm fnclex
    __asm fldcw controlWord
}
static int FPURoundToInt(double value) {
    int result;
    __asm fld value
    __asm fistp result
    return result;
}
static int FPUPrecisionDifference(double base) {
    int result;
    __asm fld base
    __asm fld1
    __asm faddp st(1), st(0)
    __asm fsub base
    __asm fistp result
    return result;
}
#else
static unsigned short FPUHardwareWord() {
    unsigned short word;
    __asm__ __volatile__("fnstcw %0" : "=m"(word));
    return word;
}
static unsigned short FPUHardwareStatus() {
    unsigned short status;
    __asm__ __volatile__("fnstsw %0" : "=m"(status));
    return status;
}
static void FPUSeedControlWord(unsigned short word) {
    __asm__ __volatile__("fnclex\n\tfldcw %0" : : "m"(word));
}
static int FPURoundToInt(double value) {
    int result;
    __asm__ __volatile__("fldl %1\n\tfistpl %0" : "=m"(result) : "m"(value) : "st");
    return result;
}
static int FPUPrecisionDifference(double base) {
    int result;
    __asm__ __volatile__("fldl %1\n\tfld1\n\tfaddp\n\tfsubl %1\n\tfistpl %0"
        : "=m"(result) : "m"(base) : "st", "st(1)");
    return result;
}
#endif

struct FPUWordApi {
    unsigned short (*Get)();
    void (*Set)(unsigned short);
    void (*Base)();
};
struct FPUWordChecks {
    int checks = 0, errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};
static unsigned FPUInvalidParameters = 0;
#if defined(_MSC_VER)
static void FPUInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned, uintptr_t) {
    ++FPUInvalidParameters;
}
#endif
struct FPUWordScope {
    fenv_t Environment;
    unsigned short Word;
    unsigned SSE;
#if defined(_MSC_VER)
    _invalid_parameter_handler Handler;
#endif
    FPUWordScope() : Word(FPUHardwareWord()), SSE(_mm_getcsr()) {
        fegetenv(&Environment);
#if defined(_MSC_VER)
        Handler = _set_thread_local_invalid_parameter_handler(FPUInvalidParameter);
#endif
    }
    ~FPUWordScope() {
        FPUSeedControlWord(Word); fesetenv(&Environment); _mm_setcsr(SSE);
#if defined(_MSC_VER)
        _set_thread_local_invalid_parameter_handler(Handler);
#endif
    }
};

static unsigned FPUSeedSSE() {
    unsigned value = (_mm_getcsr() & ~0x6000u) | 0x2000u | 0x8000u;
    _mm_setcsr(value);
    return value;
}

static FPUWordChecks FPUGetRaw(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    const unsigned short precision[] = {0, 0x200, 0x300};
    unsigned sse = FPUSeedSSE();
    for (unsigned short pc : precision) for (int rc = 0; rc < 4; ++rc) {
        FPUSeedControlWord(static_cast<unsigned short>(0x107f | pc | (rc << 10)));
        unsigned short expected = FPUHardwareWord();
        result.Expect(api.Get() == expected);
        result.Expect(FPUHardwareWord() == expected);
        result.Expect(_mm_getcsr() == sse);
    }
    return result;
}

static FPUWordChecks FPUSetRaw(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    const unsigned short precision[] = {0, 0x200, 0x300};
    unsigned sse = FPUSeedSSE();
    unsigned invalidBefore = FPUInvalidParameters;
    for (unsigned short pc : precision) for (int rc = 0; rc < 4; ++rc) {
        FPUSeedControlWord(0x037f);
        unsigned short expected = static_cast<unsigned short>(0x107f | pc | (rc << 10));
        api.Set(expected);
        result.Expect(FPUHardwareWord() == expected);
        result.Expect(_mm_getcsr() == sse);
    }
    result.Expect(FPUInvalidParameters == invalidBefore);
    return result;
}

static FPUWordChecks FPURoundTrip(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    unsigned sse = FPUSeedSSE();
    FPUSeedControlWord(0x1a7f);
    unsigned short initial = FPUHardwareWord(), saved = api.Get();
    api.Set(0x007f); api.Set(saved);
    result.Expect(FPUHardwareWord() == initial);
    result.Expect(api.Get() == initial);
    result.Expect(_mm_getcsr() == sse);
    return result;
}

static FPUWordChecks FPUBaseWord(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    const unsigned short precision[] = {0, 0x200, 0x300};
    unsigned sse = FPUSeedSSE();
    unsigned invalidBefore = FPUInvalidParameters;
    for (unsigned short pc : precision) for (int rc = 0; rc < 4; ++rc) {
        FPUSeedControlWord(static_cast<unsigned short>(0x107e | pc | (rc << 10)));
        unsigned short before = FPUHardwareWord();
        unsigned short expected = static_cast<unsigned short>((before & 0xf0ff) | 0x3f);
        api.Base();
        result.Expect(FPUHardwareWord() == expected);
        result.Expect(api.Get() == expected);
        result.Expect(_mm_getcsr() == sse);
        result.Expect(FPUPrecisionDifference(16777216.0) == 0);
        api.Base();
        result.Expect(FPUHardwareWord() == expected);
    }
    result.Expect(FPUInvalidParameters == invalidBefore);
    return result;
}

static FPUWordChecks FPURoundingBehavior(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    const int positive[] = {2, 1, 2, 1}, negative[] = {-2, -2, -1, -1};
    unsigned sse = FPUSeedSSE();
    for (int rc = 0; rc < 4; ++rc) {
        FPUSeedControlWord(0x037f);
        api.Set(static_cast<unsigned short>(0x037f | (rc << 10)));
        result.Expect(FPURoundToInt(1.75) == positive[rc]);
        result.Expect(FPURoundToInt(-1.75) == negative[rc]);
        result.Expect(_mm_getcsr() == sse);
    }
    return result;
}

static FPUWordChecks FPUPrecisionBehavior(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    const unsigned short precision[] = {0, 0x200, 0x300};
    unsigned sse = FPUSeedSSE();
    for (int k = 0; k < 3; ++k) {
        FPUSeedControlWord(0x037f);
        api.Set(static_cast<unsigned short>(0x007f | precision[k]));
        result.Expect(FPUPrecisionDifference(16777216.0) == (k == 0 ? 0 : 1));
        result.Expect(FPUPrecisionDifference(9007199254740992.0) == (k == 2 ? 1 : 0));
        result.Expect(_mm_getcsr() == sse);
    }
    return result;
}

static FPUWordChecks FPUStatusPreserved(const FPUWordApi &api) {
    FPUWordScope restore;
    FPUWordChecks result;
    FPUSeedControlWord(0x037f);
    result.Expect(FPURoundToInt(1.75) == 2);
    unsigned short status = FPUHardwareStatus();
    result.Expect((status & 0x20) != 0);
    api.Get(); result.Expect(FPUHardwareStatus() == status);
    api.Set(0x027f); result.Expect(FPUHardwareStatus() == status);
    api.Base(); result.Expect(FPUHardwareStatus() == status);
    return result;
}

#ifndef VX_FPU_WORD_PROBE
static const FPUWordApi FPUCurrent = {VxGetFPUControlWord, VxSetFPUControlWord, VxSetBaseFPUControlWord};
TEST(FPUControlWordReferenceTest, GetterReturnsHardwareBitsWithoutChangingEitherControlUnit) { EXPECT_EQ(FPUGetRaw(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, SetterWritesRawX87FieldsAndLeavesSIMDStateAlone) { EXPECT_EQ(FPUSetRaw(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, SaveAndRestoreRoundTripKeepsIndependentSIMDControls) { EXPECT_EQ(FPURoundTrip(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, BaseMasksExceptionsAndSelectsSinglePrecisionNearest) { EXPECT_EQ(FPUBaseWord(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, RoundingModesAffectActualX87IntegerConversions) { EXPECT_EQ(FPURoundingBehavior(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, PrecisionModesAffectActualX87Arithmetic) { EXPECT_EQ(FPUPrecisionBehavior(FPUCurrent).errors, 0); }
TEST(FPUControlWordReferenceTest, ControlWordOperationsPreservePendingStatusFlags) { EXPECT_EQ(FPUStatusPreserved(FPUCurrent).errors, 0); }
#endif
