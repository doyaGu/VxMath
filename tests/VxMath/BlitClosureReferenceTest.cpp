#include <gtest/gtest.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include "VxMath.h"
#include "VxBlitEngine.h"
#include "VxSIMDMode.h"
#include "NeuQuant.h"
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>

static unsigned long long ClosurePaletteHash(const NeuQuant &net) {
    unsigned long long hash = 14695981039346656037ULL;
    for (int i = 0; i < 256; ++i) {
        XBYTE r, g, b;
        net.getpalette(i, r, g, b);
        const XBYTE color[] = {r, g, b};
        for (int c = 0; c < 3; ++c) hash = (hash ^ color[c]) * 1099511628211ULL;
    }
    return hash;
}

static void ClosurePicture(XBYTE *picture) {
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
        XBYTE *p = picture + (y * 32 + x) * 3;
        p[0] = static_cast<XBYTE>(17 * x + 3 * y);
        p[1] = static_cast<XBYTE>(5 * x + 29 * y);
        p[2] = static_cast<XBYTE>(11 * x + 7 * y);
    }
}

static int ClosureDistance(const NeuQuant &net, int index, int r, int g, int b) {
    XBYTE pr, pg, pb;
    net.getpalette(index, pr, pg, pb);
    return std::abs(r - pr) + std::abs(g - pg) + std::abs(b - pb);
}

class NeuQuantClosureReferenceTest : public testing::TestWithParam<int> {};

TEST_P(NeuQuantClosureReferenceTest, NativeTrainingPaletteAndBruteForceNearestColor) {
    XBYTE picture[32 * 32 * 3];
    ClosurePicture(picture);
    const int factors[] = {1, 15, 30};
    const unsigned long long nativeHashes[] = {
        0xa2e2c48baadd474bULL, 0x0a141c9e32d8f951ULL, 0xb3107991b5ebf10bULL
    };
    NeuQuant net;
    net.initnet(picture, sizeof(picture), factors[GetParam()]);
    net.learn();
    net.unbiasnet();
    ASSERT_EQ(ClosurePaletteHash(net), nativeHashes[GetParam()]);
    net.inxbuild();
    EXPECT_EQ(ClosurePaletteHash(net), nativeHashes[GetParam()]);
    for (int r = 0; r <= 255; r += 17) for (int g = 0; g <= 255; g += 17) for (int b = 0; b <= 255; b += 17) {
        int index = net.inxsearch(r, g, b);
        ASSERT_GE(index, 0); ASSERT_LT(index, 256);
        int selected = ClosureDistance(net, index, r, g, b);
        for (int i = 0; i < 256; ++i) ASSERT_LE(selected, ClosureDistance(net, i, r, g, b));
    }
    for (int i = 0; i < 256; ++i) {
        XBYTE r, g, b;
        net.getpalette(i, r, g, b);
        EXPECT_EQ(ClosureDistance(net, net.inxsearch(r, g, b), r, g, b), 0);
    }
}

TEST_P(NeuQuantClosureReferenceTest, ReinitializationAndIndependentObjectsRetainPaletteOwnership) {
    XBYTE picture[32 * 32 * 3]; ClosurePicture(picture);
    NeuQuant trained, reset;
    trained.initnet(picture, sizeof(picture), 15); trained.learn(); trained.unbiasnet(); trained.inxbuild();
    for (int call = 0; call < 3; ++call) {
        reset.initnet(picture, sizeof(picture), GetParam() + 1);
        reset.unbiasnet(); reset.inxbuild();
        for (int i = 0; i < 256; ++i) {
            XBYTE r, g, b; reset.getpalette(i, r, g, b);
            EXPECT_EQ(r, i); EXPECT_EQ(g, i); EXPECT_EQ(b, i);
            EXPECT_EQ(reset.inxsearch(i, i, i), i);
        }
        EXPECT_EQ(ClosurePaletteHash(trained), 0x0a141c9e32d8f951ULL);
        reset.initnet(picture, sizeof(picture), 30); reset.learn(); reset.unbiasnet(); reset.inxbuild();
        EXPECT_EQ(ClosurePaletteHash(reset), 0xb3107991b5ebf10bULL);
    }
}

INSTANTIATE_TEST_SUITE_P(Sampling, NeuQuantClosureReferenceTest, testing::Range(0, 3));

struct ClosureImage {
    XDWORD storage[160];
    VxImageDescEx desc;
    ClosureImage(int width, int height, bool reverse = false) {
        for (int i = 0; i < 160; ++i) storage[i] = 0xa5a5a5a5;
        desc.Width = width; desc.Height = height; desc.BitsPerPixel = 32;
        desc.BytesPerLine = (reverse ? -1 : 1) * (width + 3) * 4;
        desc.Image = reinterpret_cast<XBYTE *>(storage + 4 + (reverse ? (height - 1) * (width + 3) : 0));
        desc.RedMask = 0xff0000; desc.GreenMask = 0xff00; desc.BlueMask = 0xff; desc.AlphaMask = 0xff000000;
    }
    XDWORD *Row(int y) { return reinterpret_cast<XDWORD *>(desc.Image + y * desc.BytesPerLine); }
    void Fill() {
        for (int y = 0; y < desc.Height; ++y) for (int x = 0; x < desc.Width; ++x)
            Row(y)[x] = (40u + 8 * x + 4 * y) | ((60u + 4 * x + 8 * y) << 8)
                | ((80u + 16 * x + 8 * y) << 16) | ((120u + 8 * x + 16 * y) << 24);
    }
    void CheckPadding() {
        for (int i = 0; i < 160; ++i) {
            bool pixel = false;
            for (int y = 0; y < desc.Height; ++y) if (storage + i >= Row(y) && storage + i < Row(y) + desc.Width) pixel = true;
            if (!pixel) EXPECT_EQ(storage[i], 0xa5a5a5a5u);
        }
    }
};

static double ClosureCoordinate(int x, int targetSize) {
    if (targetSize == 2) return x * 2.0 + 0.5;
    return std::fmin(x * 4.0 / targetSize, 3.0);
}

static void ClosureCheckResize(ClosureImage &target) {
    for (int y = 0; y < target.desc.Height; ++y) for (int x = 0; x < target.desc.Width; ++x) {
        double sx = ClosureCoordinate(x, target.desc.Width), sy = ClosureCoordinate(y, target.desc.Height);
        XDWORD expected = static_cast<XDWORD>(40 + 8 * sx + 4 * sy)
            | (static_cast<XDWORD>(60 + 4 * sx + 8 * sy) << 8)
            | (static_cast<XDWORD>(80 + 16 * sx + 8 * sy) << 16)
            | (static_cast<XDWORD>(120 + 8 * sx + 16 * sy) << 24);
        EXPECT_EQ(target.Row(y)[x], expected) << x << "," << y;
    }
    target.CheckPadding();
}

class BlitClosureReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        const int modes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
        previous = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(modes[GetParam()]));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), modes[GetParam()]);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(BlitClosureReferenceTest, AllNineOriginalResizeDirectionsPreserveChannelsAndPadding) {
    const int sizes[] = {2, 4, 8};
    for (int w = 0; w < 3; ++w) for (int h = 0; h < 3; ++h) for (int signs = 0; signs < 4; ++signs) {
        ClosureImage source(4, 4, (signs & 1) != 0), target(sizes[w], sizes[h], (signs & 2) != 0);
        source.Fill(); XDWORD before[160]; std::memcpy(before, source.storage, sizeof(before));
        VxResizeImage32(source.desc, target.desc);
        ClosureCheckResize(target);
        EXPECT_EQ(std::memcmp(source.storage, before, sizeof(before)), 0);
    }
}

TEST_P(BlitClosureReferenceTest, ObjectResizeAndBlitUseSameOriginalDirectionContracts) {
    VxBlitEngine engine;
    const int sizes[] = {2, 4, 8};
    for (int w = 0; w < 3; ++w) for (int h = 0; h < 3; ++h) {
        ClosureImage source(4, 4), resized(sizes[w], sizes[h]), blitted(sizes[w], sizes[h]); source.Fill();
        engine.ResizeImage(source.desc, resized.desc); ClosureCheckResize(resized);
        engine.DoBlit(source.desc, blitted.desc); ClosureCheckResize(blitted);
    }
}

static void ClosureCheckEngine(VxBlitEngine &engine) {
    const XDWORD source[] = {0x12345678, 0xab987654, 0x01234567};
    XBYTE packed[11]; std::memset(packed, 0xa5, sizeof(packed));
    VxImageDescEx src, dst;
    src.Width = dst.Width = 3; src.Height = dst.Height = 1;
    src.BitsPerPixel = 32; src.BytesPerLine = 12; src.Image = reinterpret_cast<XBYTE *>(const_cast<XDWORD *>(source));
    src.RedMask = dst.RedMask = 0xff0000; src.GreenMask = dst.GreenMask = 0xff00; src.BlueMask = dst.BlueMask = 0xff;
    src.AlphaMask = 0xff000000; dst.AlphaMask = 0; dst.BitsPerPixel = 24; dst.BytesPerLine = 11; dst.Image = packed;
    engine.DoBlit(src, dst);
    const XBYTE expected[] = {0x78, 0x56, 0x34, 0x54, 0x76, 0x98, 0x67, 0x45, 0x23, 0xa5, 0xa5};
    EXPECT_EQ(std::memcmp(packed, expected, sizeof(expected)), 0);
    XDWORD output[] = {0, 0, 0, 0xa5a5a5a5};
    VxImageDescEx back = src; back.Image = reinterpret_cast<XBYTE *>(output);
    engine.DoBlit(dst, back);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(output[i], (source[i] & 0xffffff) | 0xff000000);
    engine.DoAlphaBlit(back, static_cast<XBYTE>(0x42));
    for (int i = 0; i < 3; ++i) EXPECT_EQ(output[i], (source[i] & 0xffffff) | 0x42000000);
    EXPECT_EQ(output[3], 0xa5a5a5a5u);
}

TEST_P(BlitClosureReferenceTest, DispatchRebuildAndModeTransitionsKeepPixelBehavior) {
    VxBlitEngine engine;
    const int modes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
    for (int pass = 0; pass < 3; ++pass) for (int mode = 0; mode < 4; ++mode) {
        engine.ApplySIMDMode(modes[mode]); ClosureCheckEngine(engine);
        engine.RebuildTables(); ClosureCheckEngine(engine);
    }
}

TEST_P(BlitClosureReferenceTest, RepeatedConstructionOwnsResizeAndDispatchState) {
    VxBlitEngine survivor;
    for (int call = 0; call < 12; ++call) {
        VxBlitEngine transient;
        ClosureImage source(4, 4), target(8, 2); source.Fill();
        transient.DoBlit(source.desc, target.desc); ClosureCheckResize(target);
        transient.ApplySIMDMode(VX_SIMD_MODE_NONE); ClosureCheckEngine(transient);
        ClosureCheckEngine(survivor);
    }
}

struct ClosureWorker {
    VxBlitEngine *engine;
    SDL_Semaphore *start;
};

static int SDLCALL ClosureConcurrentBlits(void *data) {
    ClosureWorker *worker = static_cast<ClosureWorker *>(data);
    SDL_WaitSemaphore(worker->start);
    for (int call = 0; call < 100; ++call) ClosureCheckEngine(*worker->engine);
    return 0;
}

TEST_P(BlitClosureReferenceTest, SharedEngineRebuildSerializesWithIndependentImageOperations) {
    VxBlitEngine engine;
    SDL_Semaphore *start = SDL_CreateSemaphore(0);
    ASSERT_NE(start, nullptr);
    ClosureWorker worker = {&engine, start};
    SDL_Thread *threads[3] = {};
    for (int i = 0; i < 3; ++i) threads[i] = SDL_CreateThread(ClosureConcurrentBlits, "blit-closure", &worker);
    for (int i = 0; i < 3; ++i) SDL_SignalSemaphore(start);
    for (int call = 0; call < 100; ++call) {
        engine.ApplySIMDMode(call % 2 ? VX_SIMD_MODE_NONE : VX_SIMD_MODE_AVX2);
        engine.RebuildTables(); ClosureCheckEngine(engine);
    }
    for (int i = 0; i < 3; ++i) {
        EXPECT_NE(threads[i], nullptr);
        if (threads[i]) { int result = -1; SDL_WaitThread(threads[i], &result); EXPECT_EQ(result, 0); }
    }
    SDL_DestroySemaphore(start);
}

INSTANTIATE_TEST_SUITE_P(DispatchModes, BlitClosureReferenceTest, testing::Range(0, 4));
