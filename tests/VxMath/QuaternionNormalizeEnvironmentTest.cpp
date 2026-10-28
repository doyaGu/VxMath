#include "VxQuaternion.h"
#include <gtest/gtest.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <xmmintrin.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static void NormalizeForTest(VxQuaternion *q, int mode) {
#if defined(VX_SIMD_SSE)
    if (mode == 1) {
        VxSIMDNormalizeQuaternion(q);
        return;
    }
    if (mode == 2) {
        _mm_storeu_ps(&q->x, VxSIMDQuaternionNormalize(_mm_loadu_ps(&q->x)));
        return;
    }
#endif
    q->Normalize();
}

#if defined(_WIN32)
static DWORD NormalizeWithoutTrap(VxQuaternion *q, int mode) {
    __try { NormalizeForTest(q, mode); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
#endif

class QuaternionNormalizeEnvironmentTest : public ::testing::Test {
    fenv_t Saved;
protected:
#if defined(VX_SIMD_SSE)
    static const int ModeCount = 3;
#else
    static const int ModeCount = 1;
#endif
    void SetUp() override {
        fegetenv(&Saved);
        fesetround(FE_TONEAREST);
        _mm_setcsr((_mm_getcsr() & ~0x8040u) | 0x1f80u);
        feclearexcept(FE_ALL_EXCEPT);
    }
    void TearDown() override { fesetenv(&Saved); }
    void CheckUniform(float input) {
        for (int mode = 0; mode < ModeCount; ++mode) {
            SCOPED_TRACE(mode);
            volatile float value = input;
            VxQuaternion q(value, value, value, value);
            feclearexcept(FE_ALL_EXCEPT);
            NormalizeForTest(&q, mode);
            const int exceptions = fetestexcept(FE_OVERFLOW | FE_UNDERFLOW | FE_INVALID | FE_DIVBYZERO);
            for (int i = 0; i < 4; ++i) EXPECT_NEAR(0.5f, q[i], 2e-7f);
            EXPECT_EQ(0, exceptions) << "input=" << input;
        }
    }
};

TEST_F(QuaternionNormalizeEnvironmentTest, FiniteUniformInputsAvoidIntermediateRangeExceptions) {
    const float inputs[] = {1e30f, 1e-30f, FLT_MAX, 0x1p-149f, 1.0f,
                            0x1p-63f, 0x1.fffffep-64f, 0x1.fffffep61f, 0x1p62f};
    for (float input : inputs) CheckUniform(input);
}

TEST_F(QuaternionNormalizeEnvironmentTest, TinyComponentsBesideUnitComponentsDoNotRaiseUnderflow) {
    for (int mode = 0; mode < ModeCount; ++mode) {
        volatile float tiny = 1e-30f;
        VxQuaternion q(1, tiny, 0, 0);
        feclearexcept(FE_ALL_EXCEPT);
        NormalizeForTest(&q, mode);
        const int exceptions = fetestexcept(FE_OVERFLOW | FE_UNDERFLOW | FE_INVALID | FE_DIVBYZERO);
        EXPECT_EQ(1, q.x); EXPECT_EQ(tiny, q.y);
        EXPECT_EQ(0, q.z); EXPECT_EQ(0, q.w);
        EXPECT_EQ(0, exceptions);
    }
}

TEST_F(QuaternionNormalizeEnvironmentTest, ZeroReturnsIdentityWithoutInvalidOrDivisionByZero) {
    for (int mode = 0; mode < ModeCount; ++mode) {
        VxQuaternion q(0, -0.0f, 0, -0.0f);
        feclearexcept(FE_ALL_EXCEPT);
        NormalizeForTest(&q, mode);
        const int exceptions = fetestexcept(FE_ALL_EXCEPT);
        EXPECT_EQ(0, q.x); EXPECT_EQ(0, q.y); EXPECT_EQ(0, q.z); EXPECT_EQ(1, q.w);
        EXPECT_EQ(0, exceptions);
    }
}

TEST_F(QuaternionNormalizeEnvironmentTest, OrdinaryInputsKeepCallerControlsAndExistingFlags) {
    for (int mode = 0; mode < ModeCount; ++mode) {
        VxQuaternion q(1, 1, 1, 1);
        fesetround(FE_DOWNWARD);
        feclearexcept(FE_ALL_EXCEPT);
        feraiseexcept(FE_DIVBYZERO);
        const unsigned before = _mm_getcsr();
        const int flags = fetestexcept(FE_ALL_EXCEPT);
        NormalizeForTest(&q, mode);
        const unsigned after = _mm_getcsr();
        const int afterFlags = fetestexcept(FE_ALL_EXCEPT);
        EXPECT_EQ(before, after);
        EXPECT_EQ(flags, afterFlags);
        EXPECT_EQ(FE_DOWNWARD, fegetround());
        for (int i = 0; i < 4; ++i) EXPECT_EQ(0.5f, q[i]);
    }
}

#if defined(_WIN32)
TEST_F(QuaternionNormalizeEnvironmentTest, FiniteNormalizationWorksWithOverflowUnmasked) {
    for (int mode = 0; mode < ModeCount; ++mode) {
        volatile float value = 1e30f;
        VxQuaternion q(value, value, value, value);
        const unsigned saved = _mm_getcsr();
        _mm_setcsr((saved & ~0x3fu) & ~0x400u);
        const DWORD fault = NormalizeWithoutTrap(&q, mode);
        _mm_setcsr(saved);
        EXPECT_EQ(0u, fault) << "mode=" << mode;
    }
}

TEST_F(QuaternionNormalizeEnvironmentTest, FiniteNormalizationWorksWithUnderflowUnmasked) {
    for (int mode = 0; mode < ModeCount; ++mode) {
        volatile float value = 1e-30f;
        VxQuaternion q(value, value, value, value);
        const unsigned saved = _mm_getcsr();
        _mm_setcsr((saved & ~0x3fu) & ~0x800u);
        const DWORD fault = NormalizeWithoutTrap(&q, mode);
        _mm_setcsr(saved);
        EXPECT_EQ(0u, fault) << "mode=" << mode;
    }
}
#endif
