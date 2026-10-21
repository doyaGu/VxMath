#include <cstring>
#include <cstdio>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#endif
#ifndef VX_NORMAL_ROW_NATIVE_PROBE
#include "VxMath.h"
#include "VxSIMDMode.h"
#include <gtest/gtest.h>
// Execute the actual private fallback helpers, which the SIMD DLL normally
// bypasses. Rename only internal external symbols to keep these compiled copies
// separate from the real DLL calls below, including on ELF platforms.
#define VxGenerateMipMapKernel RowTestGenerateMipMapKernel
#define VxConvertToNormalMapKernel RowTestConvertToNormalMapKernel
#define VxConvertToBumpMapKernel RowTestConvertToBumpMapKernel
#define VxGenerateMipMap32SSE2 RowTestGenerateMipMap32SSE2
#define VxGenerateMipMap16Rgb565SSE2 RowTestGenerateMipMap16Rgb565SSE2
#define VxGenerateMipMap24Rgb888SSSE3 RowTestGenerateMipMap24Rgb888SSSE3
#define VxFillLuminance32SSE2 RowTestFillLuminance32SSE2
#define VxFillLuminance24SSSE3 RowTestFillLuminance24SSSE3
#include "../../src/VxImageKernels.cpp"
#include "../../src/VxImageKernelsSSE2.cpp"
#undef VxGenerateMipMapKernel
#undef VxConvertToNormalMapKernel
#undef VxConvertToBumpMapKernel
#undef VxGenerateMipMap32SSE2
#undef VxGenerateMipMap16Rgb565SSE2
#undef VxGenerateMipMap24Rgb888SSSE3
#undef VxFillLuminance32SSE2
#undef VxFillLuminance24SSSE3
#else
#include "VxImageDescEx.h"
#endif

typedef int (*RowNormalCall)(const VxImageDescEx &, XDWORD);
typedef int (*RowBumpCall)(const VxImageDescEx &);
struct RowObservations {
    unsigned pixels = 0, guards = 0, errors = 0, maximumError = 0, digest = 2166136261u;
    char samples[1024] = {}; int sampleBytes = 0;
};

static VxImageDescEx RowDesc(int width, int height, int pitch, XBYTE *image) {
    VxImageDescEx d; d.Width = width; d.Height = height; d.BytesPerLine = pitch;
    d.BitsPerPixel = 32; d.Image = image; return d;
}

static void RowInput(const VxImageDescEx &d, int seed) {
    for (int y = 0; y < d.Height; ++y)
        for (int x = 0; x < d.Width; ++x)
            for (int c = 0; c < 4; ++c)
                d.Image[y * d.BytesPerLine + x * 4 + c] = XBYTE(x * 71 + y * y * 19 + y * 37 + c * 53 + seed);
}

static void RowByte(RowObservations &r, XBYTE actual, unsigned expected, unsigned tolerance) {
    unsigned difference = actual > expected ? actual - expected : expected - actual;
    if (difference > r.maximumError) r.maximumError = difference;
    if (difference > tolerance) ++r.errors;
    r.digest = (r.digest ^ actual) * 16777619u;
}

// Reference uses double geometry and original, immutable byte samples. A one
// level bound accounts for original x87 and current float packing at thresholds.
static void RowCheckNormal(RowNormalCall call, const VxImageDescEx &d, bool interior,
                           RowObservations &r, bool saveSamples = false) {
    XBYTE source[1024];
    for (int y = 0; y < d.Height; ++y) std::memcpy(source + y * d.Width * 4, d.Image + y * d.BytesPerLine, d.Width * 4);
    VxImageDescEx before = d;
    if (!call(d, 0xffffffffu) || std::memcmp(&before, &d, sizeof(d))) ++r.errors;
    const int rows = interior ? d.Height - 1 : d.Height;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < d.Width - (interior ? 1 : 0); ++x) {
            const XBYTE *actual = d.Image + y * d.BytesPerLine + x * 4;
            if (d.Height == 1) {
                for (int c = 0; c < 4; ++c) RowByte(r, actual[c], source[x * 4 + c], 0);
            } else {
                const int row = y == d.Height - 1 ? y - 1 : y;
                const int right = x + 1 < d.Width ? x + 1 : x;
                const XBYTE *p = source + (row * d.Width + x) * 4;
                const XBYTE *q = source + (row * d.Width + right) * 4;
                const XBYTE *s = source + ((row + 1) * d.Width + x) * 4;
                const unsigned height = unsigned(p[0]) + p[1] + p[2];
                const double dx = (double(q[0]) + q[1] + q[2] - height) / 765.0;
                const double dy = (double(s[0]) + s[1] + s[2] - height) / 765.0;
                const double length = std::sqrt(1.0 + dx * dx + dy * dy);
                const unsigned expected[] = {unsigned(128.0 + 127.0 / length), unsigned(128.0 + 127.0 * dy / length),
                                             unsigned(128.0 + 127.0 * dx / length), height / 3};
                for (int c = 0; c < 4; ++c) RowByte(r, actual[c], expected[c], 1);
            }
            if (saveSamples) {
                for (int c = 0; c < 4; ++c) {
                    std::snprintf(r.samples + r.sampleBytes * 2, 3, "%02x", actual[c]); ++r.sampleBytes;
                }
            }
            ++r.pixels;
        }
    }
}

static RowObservations RowNormalInterior(RowNormalCall call) {
    RowObservations r; const int widths[] = {2, 3, 4, 5, 9}, heights[] = {2, 3, 5}; XBYTE bytes[256];
    for (int wi = 0; wi < 5; ++wi) for (int hi = 0; hi < 3; ++hi) {
        std::memset(bytes, 0xa5, sizeof(bytes)); VxImageDescEx d = RowDesc(widths[wi], heights[hi], widths[wi] * 4, bytes + 8);
        RowInput(d, wi * 7 + hi * 3); RowCheckNormal(call, d, true, r, true);
        for (int n = 0; n < 8; ++n) { ++r.guards; if (bytes[n] != 0xa5) ++r.errors; }
        for (int n = 8 + d.Width * d.Height * 4; n < int(sizeof(bytes)); ++n) { ++r.guards; if (bytes[n] != 0xa5) ++r.errors; }
    }
    return r;
}

static RowObservations RowNormalPadded(RowNormalCall call) {
    RowObservations r; XBYTE bytes[1024], before[1024];
    for (int width = 1; width <= 17; ++width) for (int height = 1; height <= 9; ++height) {
        const int pitch = width * 4 + 5;
        for (int direction = -1; direction <= 1; direction += 2) {
            std::memset(bytes, 0xa5, sizeof(bytes));
            VxImageDescEx d = RowDesc(width, height, pitch * direction, bytes + 9 + (direction < 0 ? (height - 1) * pitch : 0));
            RowInput(d, height * 11); std::memcpy(before, bytes, sizeof(bytes));
            RowCheckNormal(call, d, false, r);
            for (int n = 0; n < int(sizeof(bytes)); ++n) {
                const int offset = n - 9;
                if (offset >= 0 && offset < pitch * height && offset % pitch < width * 4) continue;
                ++r.guards; if (bytes[n] != before[n]) ++r.errors;
            }
        }
    }
    return r;
}

static RowObservations RowBumpSurfaces(RowBumpCall call) {
    RowObservations r; XBYTE bytes[1024], before[1024];
    for (int width = 2; width <= 17; ++width) for (int height = 1; height <= 9; ++height) {
        const int pitch = width * 4 + 5;
        std::memset(bytes, 0xa5, sizeof(bytes)); VxImageDescEx d = RowDesc(width, height, pitch, bytes + 9);
        RowInput(d, height * 11); std::memcpy(before, bytes, sizeof(bytes)); VxImageDescEx metadata = d;
        if (!call(d) || std::memcmp(&metadata, &d, sizeof(d))) ++r.errors;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            const int above = y ? y - 1 : height - 1, below = (y + 1) % height;
            const int left = x ? (x == width - 1 ? width - 1 : x - 1) : width - 1;
            const int right = x == width - 1 ? 0 : x + 1;
            const XBYTE *p = before + 9 + y * pitch;
            const unsigned expected[] = {p[x * 4] <= 1 ? 127u : 63u,
                XBYTE(before[9 + above * pitch + x * 4] - before[9 + below * pitch + x * 4]),
                XBYTE(p[left * 4] - p[right * 4]), 0};
            for (int c = 0; c < 4; ++c) RowByte(r, d.Image[y * pitch + x * 4 + c], expected[c], 0);
            ++r.pixels;
        }
        for (int n = 0; n < int(sizeof(bytes)); ++n) {
            int offset = n - 9;
            if (offset >= 0 && offset < pitch * height && offset % pitch < width * 4) continue;
            ++r.guards; if (bytes[n] != before[n]) ++r.errors;
        }
    }
    return r;
}

#ifndef VX_NORMAL_ROW_NATIVE_PROBE
static XBOOL RejectRowFill(const XBYTE *, int, int *out) { out[0] = -77; return FALSE; }
static XBOOL AcceptRowFill(const XBYTE *, int width, int *out) {
    for (int x = 0; x < width; ++x) out[x] = 1000 + x;
    return TRUE;
}

TEST(NormalRowPrivateReferenceTest, ScalarLuminanceKeepsAllSumsAndIgnoresAlpha) {
    XBYTE bytes[8] = {};
    for (int sum = 0; sum <= 765; ++sum) for (int offset = 0; offset < 4; ++offset) {
        XBYTE *p = bytes + offset; int remaining = sum;
        for (int c = 0; c < 3; ++c) { int v = remaining > 255 ? 255 : remaining; p[c] = XBYTE(v); remaining -= v; }
        for (int alpha = 0; alpha < 256; ++alpha) { p[3] = XBYTE(alpha); ASSERT_EQ(Luminance32(p), sum); }
    }
}

TEST(NormalRowPrivateReferenceTest, ScalarRowsAndRejectedFillReplaceEveryOutputAndKeepGuards) {
    XBYTE pixels[140], before[140]; for (int n = 0; n < 140; ++n) pixels[n] = XBYTE(n * 71);
    std::memcpy(before, pixels, sizeof(pixels));
    for (int width = 1; width <= 33; ++width) for (int offset = 0; offset < 4; ++offset) {
        int output[35]; for (int n = 0; n < 35; ++n) output[n] = -991;
        FillLuminance32Row(pixels + offset, width, output + 1);
        for (int x = 0; x < width; ++x) EXPECT_EQ(output[x + 1], int(before[offset + x * 4]) + before[offset + x * 4 + 1] + before[offset + x * 4 + 2]);
        EXPECT_EQ(output[0], -991); EXPECT_EQ(output[width + 1], -991);
        EXPECT_EQ(std::memcmp(pixels, before, sizeof(pixels)), 0);
        FillCachedLuminanceRow(pixels + offset, width, 4, output + 1, RejectRowFill);
        for (int x = 0; x < width; ++x) EXPECT_EQ(output[x + 1], int(before[offset + x * 4]) + before[offset + x * 4 + 1] + before[offset + x * 4 + 2]);
        EXPECT_EQ(output[0], -991); EXPECT_EQ(output[width + 1], -991);
        EXPECT_EQ(std::memcmp(pixels, before, sizeof(pixels)), 0);
    }
}

TEST(NormalRowPrivateReferenceTest, AcceptedFillIsPreservedAndNullFillUsesScalar) {
    XBYTE pixels[] = {1, 2, 3, 255, 250, 251, 252, 3}; int output[2];
    FillCachedLuminanceRow(pixels, 2, 4, output, AcceptRowFill); EXPECT_EQ(output[0], 1000); EXPECT_EQ(output[1], 1001);
    FillCachedLuminanceRow(pixels, 2, 4, output, nullptr); EXPECT_EQ(output[0], 6); EXPECT_EQ(output[1], 753);
}

TEST(NormalRowPrivateReferenceTest, AllocationCreatesIndependentRowsForNormalAndBump) {
    for (int count = 2; count <= 3; ++count) for (int width = 1; width <= 33; ++width) {
        VxLuminanceRowBuffers rows; ASSERT_TRUE(AllocateLuminanceRows(rows, width, count));
        ASSERT_EQ(rows.Row0.Size(), width); ASSERT_EQ(rows.Row1.Size(), width); ASSERT_EQ(rows.Row2.Size(), count == 3 ? width : 0);
        EXPECT_NE(rows.Row0.Begin(), rows.Row1.Begin());
        if (count == 3) { EXPECT_NE(rows.Row2.Begin(), rows.Row0.Begin()); EXPECT_NE(rows.Row2.Begin(), rows.Row1.Begin()); }
        rows.Row0[width - 1] = 111; rows.Row1[width - 1] = 222;
        if (count == 3) rows.Row2[width - 1] = 333;
        EXPECT_EQ(rows.Row0[width - 1], 111); EXPECT_EQ(rows.Row1[width - 1], 222);
    }
}

TEST(NormalRowPrivateReferenceTest, SwapAndRotationKeepOwnershipAndCachedValues) {
    VxLuminanceRowBuffers rows; ASSERT_TRUE(AllocateLuminanceRows(rows, 7, 3));
    int *original[] = {rows.Row0.Begin(), rows.Row1.Begin(), rows.Row2.Begin()};
    for (int row = 0; row < 3; ++row) for (int x = 0; x < 7; ++x) original[row][x] = row * 100 + x;
    int *a = original[0], *b = original[1], *c = original[2];
    SwapLuminanceRows(a, b); EXPECT_EQ(a, original[1]); EXPECT_EQ(b, original[0]); SwapLuminanceRows(a, b);
    for (int step = 1; step <= 9; ++step) {
        RotateLuminanceRowsForward(a, b, c);
        EXPECT_EQ(a, original[step % 3]); EXPECT_EQ(b, original[(step + 1) % 3]); EXPECT_EQ(c, original[(step + 2) % 3]);
        EXPECT_EQ(a[6], (step % 3) * 100 + 6);
    }
    EXPECT_EQ(rows.Row0.Begin(), original[0]); EXPECT_EQ(rows.Row1.Begin(), original[1]); EXPECT_EQ(rows.Row2.Begin(), original[2]);
}

class NormalRowCacheReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override { previous = VxGetSIMDOverride(); ASSERT_TRUE(VxSetSIMDOverride(GetParam())); RecordProperty("effective_mode", VxGetSIMDEffectiveBackend()); }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
    void Check(const RowObservations &r) {
        EXPECT_EQ(r.errors, 0u); EXPECT_GT(r.pixels, 0u); EXPECT_GT(r.guards, 0u);
        RecordProperty("pixels", int(r.pixels)); RecordProperty("maximum_error", int(r.maximumError)); RecordProperty("guards", int(r.guards));
        char digest[16]; std::snprintf(digest, sizeof(digest), "%08x", r.digest); RecordProperty("pixel_digest", digest);
        if (r.sampleBytes) RecordProperty("samples", r.samples);
    }
};
TEST_P(NormalRowCacheReferenceTest, OriginalInteriorKeepsRgbHeightAndForwardDifferences) { Check(RowNormalInterior(VxConvertToNormalMap)); }
TEST_P(NormalRowCacheReferenceTest, PaddedAndNegativeRowsKeepSourceCacheAndLastRowRules) { Check(RowNormalPadded(VxConvertToNormalMap)); }
TEST_P(NormalRowCacheReferenceTest, BumpRotatingRowsKeepOriginalBlueSamplesAndWrappedEdges) { Check(RowBumpSurfaces(VxConvertToBumpMap)); }
#ifdef _WIN32
TEST_P(NormalRowCacheReferenceTest, GuardPageBoundsLuminanceVectorsAndFinalRowCopy) {
    SYSTEM_INFO info; GetSystemInfo(&info); XBYTE *memory = static_cast<XBYTE *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    ASSERT_NE(memory, nullptr); DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    for (int width = 1; width <= 11; ++width) for (int height = 2; height <= 3; ++height) {
        VxImageDescEx d = RowDesc(width, height, width * 4, memory + info.dwPageSize - width * height * 4);
        RowInput(d, height * 11); RowObservations r; RowCheckNormal(VxConvertToNormalMap, d, false, r); EXPECT_EQ(r.errors, 0u);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif
INSTANTIATE_TEST_SUITE_P(RequestedModes, NormalRowCacheReferenceTest, testing::Range(1, 7));
#endif
