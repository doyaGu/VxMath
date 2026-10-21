#ifndef VX_GRAPHIC_DISPATCH_NATIVE_PROBE
#include "VxMath.h"
#include "VxSIMDMode.h"
#include <gtest/gtest.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>
#endif
// Reuse the frozen input generators and byte oracles, excluding their tests
// and private compiled kernel copies. Public calls here use the real DLL.
#define VX_NORMAL_ROW_NATIVE_PROBE
#include "NormalRowCacheReferenceTest.cpp"
#undef VX_NORMAL_ROW_NATIVE_PROBE
#define VX_MIPMAP_NATIVE_PROBE
#include "Mipmap32ReferenceTest.cpp"
#undef VX_MIPMAP_NATIVE_PROBE

static const XDWORD GraphicMasks[] = {0, 0xffu, 0xff00u, 0xff0000u, 0xff000000u};

static void CheckMaskedNormal(RowNormalCall call, const VxImageDescEx &d, int selection,
                              bool interior, RowObservations &r) {
    XBYTE source[512];
    for (int y = 0; y < d.Height; ++y) std::memcpy(source + y * d.Width * 4, d.Image + y * d.BytesPerLine, d.Width * 4);
    VxImageDescEx before = d;
    if (!call(d, GraphicMasks[selection]) || std::memcmp(&before, &d, sizeof(d))) ++r.errors;
    for (int y = 0; y < d.Height - (interior ? 1 : 0); ++y) {
        for (int x = 0; x < d.Width - (interior ? 1 : 0); ++x) {
            const XBYTE *actual = d.Image + y * d.BytesPerLine + x * 4;
            if (d.Height == 1) {
                for (int c = 0; c < 4; ++c) RowByte(r, actual[c], source[x * 4 + c], 0);
            } else {
                const int row = y == d.Height - 1 ? y - 1 : y;
                const int right = x + 1 < d.Width ? x + 1 : x;
                unsigned h0 = 0, hr = 0, hb = 0;
                if (selection) {
                    h0 = source[(row * d.Width + x) * 4 + selection - 1];
                    hr = source[(row * d.Width + right) * 4 + selection - 1];
                    hb = source[((row + 1) * d.Width + x) * 4 + selection - 1];
                }
                const double dx = (double(hr) - h0) / 255.0, dy = (double(hb) - h0) / 255.0;
                const double length = std::sqrt(1.0 + dx * dx + dy * dy);
                const unsigned expected[] = {unsigned(128.0 + 127.0 / length), unsigned(128.0 + 127.0 * dy / length),
                    unsigned(128.0 + 127.0 * dx / length), h0};
                for (int c = 0; c < 4; ++c) RowByte(r, actual[c], expected[c], selection ? 1 : 0);
            }
            if (interior) for (int c = 0; c < 4; ++c) {
                std::snprintf(r.samples + r.sampleBytes * 2, 3, "%02x", actual[c]); ++r.sampleBytes;
            }
            ++r.pixels;
        }
    }
}

static RowObservations MaskInteriors(RowNormalCall call, int selection) {
    const int widths[] = {2, 3, 5}, heights[] = {2, 3, 5}; XBYTE bytes[160]; RowObservations r;
    for (int wi = 0; wi < 3; ++wi) for (int hi = 0; hi < 3; ++hi) {
        std::memset(bytes, 0xa5, sizeof(bytes)); VxImageDescEx d = RowDesc(widths[wi], heights[hi], widths[wi] * 4, bytes + 8);
        RowInput(d, wi * 7 + hi * 3); CheckMaskedNormal(call, d, selection, true, r);
        for (int n = 0; n < int(sizeof(bytes)); ++n) {
            if (n >= 8 && n < 8 + d.Width * d.Height * 4) continue;
            ++r.guards; if (bytes[n] != 0xa5) ++r.errors;
        }
    }
    return r;
}

static RowObservations MaskPadded(RowNormalCall call) {
    const int widths[] = {1, 2, 5, 9}, heights[] = {1, 2, 5}; XBYTE bytes[512], before[512]; RowObservations r;
    for (int selection = 0; selection < 5; ++selection) for (int wi = 0; wi < 4; ++wi) for (int hi = 0; hi < 3; ++hi) {
        const int width = widths[wi], height = heights[hi], pitch = width * 4 + 5;
        for (int direction = -1; direction <= 1; direction += 2) {
            std::memset(bytes, 0xa5, sizeof(bytes));
            VxImageDescEx d = RowDesc(width, height, direction * pitch, bytes + 9 + (direction < 0 ? (height - 1) * pitch : 0));
            RowInput(d, selection * 17 + hi * 11); std::memcpy(before, bytes, sizeof(bytes));
            CheckMaskedNormal(call, d, selection, false, r);
            for (int n = 0; n < int(sizeof(bytes)); ++n) {
                const int offset = n - 9;
                if (offset >= 0 && offset < pitch * height && offset % pitch < width * 4) continue;
                ++r.guards; if (bytes[n] != before[n]) ++r.errors;
            }
        }
    }
    return r;
}

#ifndef VX_GRAPHIC_DISPATCH_NATIVE_PROBE
class GraphicDispatchReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override { previous = VxGetSIMDOverride(); ASSERT_TRUE(VxSetSIMDOverride(GetParam())); RecordProperty("effective_mode", VxGetSIMDEffectiveBackend()); }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
    void Check(const RowObservations &r, const char *prefix) {
        EXPECT_EQ(r.errors, 0u); EXPECT_GT(r.pixels, 0u);
        char key[64], digest[16]; std::snprintf(digest, sizeof(digest), "%08x", r.digest);
        std::snprintf(key, sizeof(key), "%s_pixels", prefix); RecordProperty(key, int(r.pixels));
        std::snprintf(key, sizeof(key), "%s_maximum_error", prefix); RecordProperty(key, int(r.maximumError));
        std::snprintf(key, sizeof(key), "%s_digest", prefix); RecordProperty(key, digest);
        if (r.sampleBytes) { std::snprintf(key, sizeof(key), "%s_samples", prefix); RecordProperty(key, r.samples); }
    }
};

TEST_P(GraphicDispatchReferenceTest, ByteMasksAndZeroSelectTheOriginalHeightChannel) {
    for (int selection = 0; selection < 5; ++selection) {
        char name[16]; std::snprintf(name, sizeof(name), "mask%d", selection);
        Check(MaskInteriors(VxConvertToNormalMap, selection), name);
    }
}
TEST_P(GraphicDispatchReferenceTest, MaskedFallbackKeepsPaddedNegativeAndTerminalRows) { Check(MaskPadded(VxConvertToNormalMap), "padded"); }
TEST_P(GraphicDispatchReferenceTest, UnsupportedDepthReturnsFailureWithoutChangingTheImage) {
    const int depths[] = {8, 16}; XBYTE bytes[48], before[48];
    std::memset(bytes, 0x65, sizeof(bytes)); std::memcpy(before, bytes, sizeof(bytes));
    for (int n = 0; n < 2; ++n) {
        VxImageDescEx d = RowDesc(4, 2, 16, bytes + 8); d.BitsPerPixel = depths[n]; VxImageDescEx metadata = d;
        EXPECT_FALSE(VxConvertToNormalMap(d, 0xff000000u)); EXPECT_FALSE(VxConvertToBumpMap(d));
        EXPECT_EQ(std::memcmp(bytes, before, sizeof(bytes)), 0); EXPECT_EQ(std::memcmp(&d, &metadata, sizeof(d)), 0);
    }
}
TEST_P(GraphicDispatchReferenceTest, InterleavedEntriesKeepTheirOwnImageOperation) {
    Check(RowBumpSurfaces(VxConvertToBumpMap), "bump");
    Mipmap32Observations mip = Mipmap32Sums(VxGenerateMipMap);
    EXPECT_EQ(mip.errors, 0u); EXPECT_EQ(mip.pixels, 3064u);
    char digest[16]; std::snprintf(digest, sizeof(digest), "%08x", mip.digest); RecordProperty("mip_digest", digest);
    Check(RowNormalInterior(VxConvertToNormalMap), "rgb");
}
INSTANTIATE_TEST_SUITE_P(RequestedModes, GraphicDispatchReferenceTest, testing::Range(1, 7));

TEST(GraphicDispatchTransitionReferenceTest, RepeatedModeChangesKeepAllThreeEntriesConsistent) {
    const int previous = VxGetSIMDOverride(); unsigned effective = 0;
    for (int round = 0; round < 3; ++round) for (int index = 1; index <= 6; ++index) {
        const int mode = round % 2 ? 7 - index : index;
        ASSERT_TRUE(VxSetSIMDOverride(mode)); ASSERT_EQ(VxGetSIMDOverride(), mode); effective |= 1u << VxGetSIMDEffectiveBackend();
        EXPECT_EQ(MaskInteriors(VxConvertToNormalMap, (round + index) % 5).errors, 0u);
        EXPECT_EQ(RowBumpSurfaces(VxConvertToBumpMap).errors, 0u);
        EXPECT_EQ(Mipmap32Sums(VxGenerateMipMap).errors, 0u);
    }
    testing::Test::RecordProperty("effective_modes", int(effective)); EXPECT_TRUE(VxSetSIMDOverride(previous));
}

struct GraphicDispatchWorker { SDL_Semaphore *ready, *start; int role, errors; };
static int SDLCALL RunGraphicDispatchWorker(void *pointer) {
    GraphicDispatchWorker *w = static_cast<GraphicDispatchWorker *>(pointer);
    SDL_SignalSemaphore(w->ready); SDL_WaitSemaphore(w->start);
    if (w->role == 2) {
        for (int n = 0; n < 96; ++n) if (!VxSetSIMDOverride(1 + n % 6)) ++w->errors;
    } else {
        for (int n = 0; n < 16; ++n) {
            w->errors += int(MaskInteriors(VxConvertToNormalMap, (n + w->role) % 5).errors);
            w->errors += int(RowBumpSurfaces(VxConvertToBumpMap).errors);
            w->errors += int(Mipmap32Sums(VxGenerateMipMap).errors);
        }
    }
    return 0;
}

TEST(GraphicDispatchTransitionReferenceTest, ParallelImagesRemainCorrectDuringTableReplacement) {
    const int previous = VxGetSIMDOverride(); SDL_Semaphore *ready = SDL_CreateSemaphore(0), *start = SDL_CreateSemaphore(0);
    ASSERT_NE(ready, nullptr); ASSERT_NE(start, nullptr);
    GraphicDispatchWorker work[] = {{ready, start, 0, 0}, {ready, start, 1, 0}, {ready, start, 2, 0}};
    SDL_Thread *threads[3]; int count = 0;
    for (int n = 0; n < 3; ++n) { threads[n] = SDL_CreateThread(RunGraphicDispatchWorker, "graphic-dispatch", &work[n]); if (threads[n]) ++count; }
    for (int n = 0; n < count; ++n) SDL_WaitSemaphore(ready);
    for (int n = 0; n < count; ++n) SDL_SignalSemaphore(start);
    for (int n = 0; n < 3; ++n) {
        EXPECT_NE(threads[n], nullptr); if (threads[n]) { int code = -1; SDL_WaitThread(threads[n], &code); EXPECT_EQ(code, 0); }
        EXPECT_EQ(work[n].errors, 0);
    }
    SDL_DestroySemaphore(ready); SDL_DestroySemaphore(start); EXPECT_TRUE(VxSetSIMDOverride(previous));
}
#endif
