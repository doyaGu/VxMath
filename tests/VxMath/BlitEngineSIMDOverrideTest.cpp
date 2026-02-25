#include <gtest/gtest.h>

#include "VxMath.h"

namespace {

struct BackendCaps {
    bool sse2;
    bool ssse3;
    bool sse4_1;
    bool avx;
    bool avx2;
};

int ProbeEffectiveMode(int requestedMode) {
    EXPECT_TRUE(VxSetSIMDOverride(requestedMode));
    return VxGetSIMDEffectiveBackend();
}

BackendCaps ProbeCaps() {
    BackendCaps caps = {};
    caps.sse2 = (ProbeEffectiveMode(VX_SIMD_MODE_SSE2) == VX_SIMD_MODE_SSE2);
    caps.ssse3 = (ProbeEffectiveMode(VX_SIMD_MODE_SSSE3) == VX_SIMD_MODE_SSSE3);
    caps.sse4_1 = (ProbeEffectiveMode(VX_SIMD_MODE_SSE4_1) == VX_SIMD_MODE_SSE4_1);
    caps.avx = (ProbeEffectiveMode(VX_SIMD_MODE_AVX) == VX_SIMD_MODE_AVX);
    caps.avx2 = (ProbeEffectiveMode(VX_SIMD_MODE_AVX2) == VX_SIMD_MODE_AVX2);
    return caps;
}

int ResolveExpectedMode(int requestedMode, const BackendCaps &caps) {
    if (requestedMode == VX_SIMD_MODE_AUTO) {
        if (caps.avx2) return VX_SIMD_MODE_AVX2;
        if (caps.avx) return VX_SIMD_MODE_AVX;
        if (caps.sse4_1) return VX_SIMD_MODE_SSE4_1;
        if (caps.ssse3) return VX_SIMD_MODE_SSSE3;
        if (caps.sse2) return VX_SIMD_MODE_SSE2;
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_NONE) {
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_SSE2) {
        return caps.sse2 ? VX_SIMD_MODE_SSE2 : VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_SSSE3) {
        if (caps.ssse3) return VX_SIMD_MODE_SSSE3;
        if (caps.sse2) return VX_SIMD_MODE_SSE2;
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_SSE4_1) {
        if (caps.sse4_1) return VX_SIMD_MODE_SSE4_1;
        if (caps.ssse3) return VX_SIMD_MODE_SSSE3;
        if (caps.sse2) return VX_SIMD_MODE_SSE2;
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_AVX) {
        if (caps.avx) return VX_SIMD_MODE_AVX;
        if (caps.sse4_1) return VX_SIMD_MODE_SSE4_1;
        if (caps.ssse3) return VX_SIMD_MODE_SSSE3;
        if (caps.sse2) return VX_SIMD_MODE_SSE2;
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_AVX2) {
        if (caps.avx2) return VX_SIMD_MODE_AVX2;
        if (caps.avx) return VX_SIMD_MODE_AVX;
        if (caps.sse4_1) return VX_SIMD_MODE_SSE4_1;
        if (caps.ssse3) return VX_SIMD_MODE_SSSE3;
        if (caps.sse2) return VX_SIMD_MODE_SSE2;
        return VX_SIMD_MODE_NONE;
    }

    return VX_SIMD_MODE_NONE;
}

class BlitEngineSIMDOverrideTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_PreviousRequestedMode = VxGetSIMDOverride();
        m_Caps = ProbeCaps();
    }

    void TearDown() override {
        VxSetSIMDOverride(m_PreviousRequestedMode);
    }

    int ExpectedFor(int requestedMode) const {
        return ResolveExpectedMode(requestedMode, m_Caps);
    }

private:
    int m_PreviousRequestedMode = VX_SIMD_MODE_AUTO;
    BackendCaps m_Caps = {};
};

} // namespace

TEST_F(BlitEngineSIMDOverrideTest, SetAutoUsesHighestAvailableBackend) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_AUTO));
    EXPECT_EQ(VX_SIMD_MODE_AUTO, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_AUTO), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetNoneForcesScalarBackend) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_NONE));
    EXPECT_EQ(VX_SIMD_MODE_NONE, VxGetSIMDOverride());
    EXPECT_EQ(VX_SIMD_MODE_NONE, VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetSSE2FallsBackByLadder) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_SSE2));
    EXPECT_EQ(VX_SIMD_MODE_SSE2, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_SSE2), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetSSSE3FallsBackByLadder) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_SSSE3));
    EXPECT_EQ(VX_SIMD_MODE_SSSE3, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_SSSE3), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetSSE41FallsBackByLadder) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_SSE4_1));
    EXPECT_EQ(VX_SIMD_MODE_SSE4_1, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_SSE4_1), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetAVXFallsBackByLadder) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_AVX));
    EXPECT_EQ(VX_SIMD_MODE_AVX, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_AVX), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, SetAVX2FallsBackByLadder) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_AVX2));
    EXPECT_EQ(VX_SIMD_MODE_AVX2, VxGetSIMDOverride());
    EXPECT_EQ(ExpectedFor(VX_SIMD_MODE_AVX2), VxGetSIMDEffectiveBackend());
}

TEST_F(BlitEngineSIMDOverrideTest, InvalidModeIsRejectedWithoutStateChange) {
    ASSERT_TRUE(VxSetSIMDOverride(VX_SIMD_MODE_SSE2));
    const int beforeRequested = VxGetSIMDOverride();
    const int beforeEffective = VxGetSIMDEffectiveBackend();

    EXPECT_FALSE(VxSetSIMDOverride(12345));
    EXPECT_EQ(beforeRequested, VxGetSIMDOverride());
    EXPECT_EQ(beforeEffective, VxGetSIMDEffectiveBackend());
}

TEST(BlitEngineSIMDOverrideNameTest, ReturnsExpectedNames) {
    EXPECT_STREQ("auto", VxGetSIMDBackendName(VX_SIMD_MODE_AUTO));
    EXPECT_STREQ("none", VxGetSIMDBackendName(VX_SIMD_MODE_NONE));
    EXPECT_STREQ("sse2", VxGetSIMDBackendName(VX_SIMD_MODE_SSE2));
    EXPECT_STREQ("ssse3", VxGetSIMDBackendName(VX_SIMD_MODE_SSSE3));
    EXPECT_STREQ("sse4_1", VxGetSIMDBackendName(VX_SIMD_MODE_SSE4_1));
    EXPECT_STREQ("avx", VxGetSIMDBackendName(VX_SIMD_MODE_AVX));
    EXPECT_STREQ("avx2", VxGetSIMDBackendName(VX_SIMD_MODE_AVX2));
    EXPECT_STREQ("unknown", VxGetSIMDBackendName(-1));
}
