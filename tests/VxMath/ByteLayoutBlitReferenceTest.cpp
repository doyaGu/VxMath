#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "VxMath.h"
#include "VxSIMDMode.h"

struct ByteLayoutFormat {
    VX_PIXELFORMAT format;
    int bytes;
    int channels[4]; // R=0, G=1, B=2, A=3, unused=4, in memory order.
    XDWORD masks[4];
};

static const ByteLayoutFormat ByteFormats[] = {
    {_32_ARGB8888, 4, {2, 1, 0, 3}, {0xff0000, 0xff00, 0xff, 0xff000000}},
    {_32_RGB888,   4, {2, 1, 0, 4}, {0xff0000, 0xff00, 0xff, 0}},
    {_24_RGB888,   3, {2, 1, 0, 4}, {0xff0000, 0xff00, 0xff, 0}},
    {_32_ABGR8888, 4, {0, 1, 2, 3}, {0xff, 0xff00, 0xff0000, 0xff000000}},
    {_32_RGBA8888, 4, {3, 2, 1, 0}, {0xff000000, 0xff0000, 0xff00, 0xff}},
    {_32_BGRA8888, 4, {3, 0, 1, 2}, {0xff00, 0xff0000, 0xff000000, 0xff}}
};
static const int ByteConversions[10][2] = {{0, 1}, {1, 0}, {0, 2}, {2, 0}, {0, 3}, {3, 0}, {0, 4}, {4, 0}, {0, 5}, {5, 0}};
static const int ByteModes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
static const int ByteWidths[] = {1, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 257};

static void DescribeByteFormat(VxImageDescEx &desc, int format) {
    const ByteLayoutFormat &f = ByteFormats[format];
    desc.BitsPerPixel = f.bytes * 8;
    desc.RedMask = f.masks[0];
    desc.GreenMask = f.masks[1];
    desc.BlueMask = f.masks[2];
    desc.AlphaMask = f.masks[3];
}

static void ReferenceBytePixel(const XBYTE *source, XBYTE *target, int from, int to) {
    XBYTE rgba[] = {0, 0, 0, 255, 0};
    for (int b = 0; b < ByteFormats[from].bytes; ++b) {
        int channel = ByteFormats[from].channels[b];
        if (channel != 4) rgba[channel] = source[b];
    }
    for (int b = 0; b < ByteFormats[to].bytes; ++b) target[b] = rgba[ByteFormats[to].channels[b]];
}

struct ByteLayoutImage {
    alignas(32) XBYTE source[2400];
    alignas(32) XBYTE target[2400];
    VxImageDescEx src, dst;
    int from, to, sourcePitch, targetPitch;

    ByteLayoutImage(int conversion, int width = 257, int sourceOffset = 0, int targetOffset = 0,
                    bool reverseSource = false, bool reverseTarget = false)
        : from(ByteConversions[conversion][0]), to(ByteConversions[conversion][1]),
          sourcePitch(width * ByteFormats[from].bytes + 5), targetPitch(width * ByteFormats[to].bytes + 7) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(target, 0xa5, sizeof(target));
        DescribeByteFormat(src, from);
        DescribeByteFormat(dst, to);
        src.Width = dst.Width = width;
        src.Height = dst.Height = 2;
        src.BytesPerLine = reverseSource ? -sourcePitch : sourcePitch;
        dst.BytesPerLine = reverseTarget ? -targetPitch : targetPitch;
        src.Image = source + 32 + sourceOffset + (reverseSource ? sourcePitch : 0);
        dst.Image = target + 32 + targetOffset + (reverseTarget ? targetPitch : 0);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < width; ++x) {
                const XBYTE rgba[] = {
                    static_cast<XBYTE>(x * 37 + y * 13 + 7), static_cast<XBYTE>(x * 53 + y * 11 + 5),
                    static_cast<XBYTE>(x * 97 + y * 17 + 3), static_cast<XBYTE>(x * 29 + y * 101 + 128),
                    static_cast<XBYTE>(x * 19 + 239)
                };
                for (int b = 0; b < ByteFormats[from].bytes; ++b)
                    src.Image[y * src.BytesPerLine + x * ByteFormats[from].bytes + b] = rgba[ByteFormats[from].channels[b]];
            }
        }
    }

    void Run(bool flipped = false) {
        XBYTE before[sizeof(source)], expected[sizeof(target)];
        std::memcpy(before, source, sizeof(source));
        std::memcpy(expected, target, sizeof(target));
        VxImageDescEx beforeSrc = src, beforeDst = dst;
        ASSERT_EQ(VxImageDesc2PixelFormat(src), ByteFormats[from].format);
        ASSERT_EQ(VxImageDesc2PixelFormat(dst), ByteFormats[to].format);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < dst.Width; ++x) {
                const XBYTE *input = src.Image + (flipped ? 1 - y : y) * src.BytesPerLine + x * ByteFormats[from].bytes;
                XBYTE *output = expected + (dst.Image - target) + y * dst.BytesPerLine + x * ByteFormats[to].bytes;
                ReferenceBytePixel(input, output, from, to);
            }
        }
        if (flipped) VxDoBlitUpsideDown(src, dst);
        else VxDoBlit(src, dst);
        ASSERT_EQ(std::memcmp(target, expected, sizeof(target)), 0);
        EXPECT_EQ(std::memcmp(source, before, sizeof(source)), 0);
        EXPECT_EQ(std::memcmp(&src, &beforeSrc, sizeof(src)), 0);
        EXPECT_EQ(std::memcmp(&dst, &beforeDst, sizeof(dst)), 0);
    }
};

class ByteLayoutBlitReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    int Conversion() const { return GetParam() % 10; }
    int Mode() const { return ByteModes[GetParam() / 10]; }
    void SetUp() override {
        previous = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(Mode()));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), Mode());
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(ByteLayoutBlitReferenceTest, LiteralPixelsAndEveryByteChannelValue) {
    static const XBYTE literalSource[6][4] = {
        {3, 5, 7, 128}, {3, 5, 7, 239}, {3, 5, 7, 0},
        {7, 5, 3, 128}, {128, 3, 5, 7}, {128, 7, 5, 3}
    };
    static const XBYTE literalOpaqueTarget[6][4] = {
        {3, 5, 7, 255}, {3, 5, 7, 0}, {3, 5, 7, 0},
        {7, 5, 3, 255}, {255, 3, 5, 7}, {255, 7, 5, 3}
    };
    static const XBYTE literalAlphaTarget[6][4] = {
        {3, 5, 7, 128}, {3, 5, 7, 0}, {3, 5, 7, 0},
        {7, 5, 3, 128}, {128, 3, 5, 7}, {128, 7, 5, 3}
    };
    ByteLayoutImage literal(Conversion(), 1);
    for (int y = 0; y < 2; ++y)
        std::memcpy(literal.src.Image + y * literal.src.BytesPerLine, literalSource[literal.from], ByteFormats[literal.from].bytes);
    literal.Run();
    const XBYTE *expected = ByteFormats[literal.from].masks[3] ? literalAlphaTarget[literal.to] : literalOpaqueTarget[literal.to];
    EXPECT_EQ(std::memcmp(literal.dst.Image, expected, ByteFormats[literal.to].bytes), 0);
    ByteLayoutImage allValues(Conversion());
    allValues.Run();
}

TEST_P(ByteLayoutBlitReferenceTest, VectorChunksTailsAndIndependentUnalignedRows) {
    static const int offsets[] = {0, 1, 2, 3, 7, 15, 16, 31};
    for (int w = 0; w < 16; ++w) {
        for (int offset = 0; offset < 8; ++offset) {
            SCOPED_TRACE(ByteWidths[w]);
            SCOPED_TRACE(offsets[offset]);
            ByteLayoutImage image(Conversion(), ByteWidths[w], offsets[offset], offsets[(offset * 3) % 8]);
            image.Run();
        }
    }
}

TEST_P(ByteLayoutBlitReferenceTest, SignedPitchesAndUpsideDownPreserveEachRow) {
    for (int signs = 0; signs < 4; ++signs) {
        for (int flip = 0; flip < 2; ++flip) {
            ByteLayoutImage image(Conversion(), 33, 1, 3, (signs & 1) != 0, (signs & 2) != 0);
            image.Run(flip != 0);
        }
    }
}

TEST_P(ByteLayoutBlitReferenceTest, ChangedPixelsAtSameAddressesAreObserved) {
    ByteLayoutImage image(Conversion(), 65);
    image.Run();
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < image.src.Width; ++x) {
            for (int b = 0; b < ByteFormats[image.from].bytes; ++b)
                image.src.Image[y * image.src.BytesPerLine + x * ByteFormats[image.from].bytes + b] ^= static_cast<XBYTE>(b * 23 + 137);
        }
    }
    image.Run();
}

static XBYTE *ByteProtectedStorage(size_t &page) {
#if defined(_WIN32)
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    page = system.dwPageSize;
    XBYTE *storage = static_cast<XBYTE *>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old;
    if (!storage || !VirtualProtect(storage + page, page, PAGE_NOACCESS, &old)) std::_Exit(3);
#else
    page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *storage = static_cast<XBYTE *>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (storage == MAP_FAILED || mprotect(storage + page, page, PROT_NONE)) std::_Exit(3);
#endif
    return storage;
}

static void ByteProtectedRows(int conversion, int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (!VxSetSIMDOverride(mode) || VxGetSIMDEffectiveBackend() != mode) std::_Exit(4);
    size_t sourcePage, targetPage;
    XBYTE *source = ByteProtectedStorage(sourcePage);
    XBYTE *target = ByteProtectedStorage(targetPage);
    for (int w = 0; w < 16; ++w) {
        ByteLayoutImage image(conversion, ByteWidths[w]);
        int sourceBytes = ByteWidths[w] * ByteFormats[image.from].bytes;
        int targetBytes = ByteWidths[w] * ByteFormats[image.to].bytes;
        XBYTE *src = source + sourcePage - sourceBytes;
        XBYTE *dst = target + targetPage - targetBytes;
        std::memcpy(src, image.src.Image, sourceBytes);
        std::memset(dst - 16, 0xa5, targetBytes + 16);
        image.src.Image = src;
        image.dst.Image = dst;
        image.src.Height = image.dst.Height = 1;
        VxDoBlit(image.src, image.dst);
        for (int x = 0; x < ByteWidths[w]; ++x) {
            XBYTE expected[4];
            ReferenceBytePixel(src + x * ByteFormats[image.from].bytes, expected, image.from, image.to);
            if (std::memcmp(dst + x * ByteFormats[image.to].bytes, expected, ByteFormats[image.to].bytes)) std::_Exit(2);
        }
        for (int i = 1; i <= 16; ++i) if (dst[-i] != 0xa5) std::_Exit(5);
    }
    std::_Exit(0);
}

TEST_P(ByteLayoutBlitReferenceTest, ExactSourceAndTargetRowsEndAtProtectedPages) {
    EXPECT_EXIT(ByteProtectedRows(Conversion(), Mode()), testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2, ByteLayoutBlitReferenceTest, testing::Range(0, 40));
