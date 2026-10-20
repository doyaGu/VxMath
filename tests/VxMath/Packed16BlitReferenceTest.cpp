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

// R/G/B/A fields are independent of the production conversion code.
struct Packed16Format {
    VX_PIXELFORMAT format;
    int bytes;
    XDWORD masks[4];
    int bits[4];
    int shifts[4];
};

static const Packed16Format PackedFormats[] = {
    {_32_ARGB8888, 4, {0xff0000,0xff00,0xff,0xff000000}, {8,8,8,8}, {16,8,0,24}},
    {_16_RGB565,   2, {0xf800,0x7e0,0x1f,0}, {5,6,5,0}, {11,5,0,0}},
    {_16_RGB555,   2, {0x7c00,0x3e0,0x1f,0}, {5,5,5,0}, {10,5,0,0}},
    {_16_ARGB1555, 2, {0x7c00,0x3e0,0x1f,0x8000}, {5,5,5,1}, {10,5,0,15}},
    {_16_ARGB4444, 2, {0xf00,0xf0,0xf,0xf000}, {4,4,4,4}, {8,4,0,12}}
};
static const int PackedConversions[8][2] = {{0,1},{0,2},{0,3},{0,4},{1,0},{2,0},{3,0},{4,0}};
static const int PackedModes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
static const int PackedWidths[] = {1,2,3,4,7,8,9,15,16,17,31,32,33,63,64,65,257};

static void DescribePackedFormat(VxImageDescEx &desc, int format) {
    const Packed16Format &f = PackedFormats[format];
    desc.BitsPerPixel = f.bytes * 8;
    desc.RedMask = f.masks[0];
    desc.GreenMask = f.masks[1];
    desc.BlueMask = f.masks[2];
    desc.AlphaMask = f.masks[3];
}

static XDWORD ReadPackedPixel(const XBYTE *p, int bytes) {
    XDWORD value = 0;
    for (int b = 0; b < bytes; ++b) value |= static_cast<XDWORD>(p[b]) << (8*b);
    return value;
}

static void WritePackedPixel(XBYTE *p, int bytes, XDWORD value) {
    for (int b = 0; b < bytes; ++b) p[b] = static_cast<XBYTE>(value >> (8*b));
}

static void ReferencePackedPixel(const XBYTE *source, XBYTE *target, int from, int to) {
    XDWORD pixel = ReadPackedPixel(source, PackedFormats[from].bytes), result = 0;
    for (int c = 0; c < 4; ++c) {
        int bits = PackedFormats[from].bits[c];
        unsigned value = (pixel & PackedFormats[from].masks[c]) >> PackedFormats[from].shifts[c];
        // Full-range bit replication; reducing precision keeps the most significant bits.
        unsigned expanded = bits == 0 ? 255 : bits == 1 ? value*255 : bits == 4 ? value*17 :
                            bits == 5 ? value*8 + value/4 : bits == 6 ? value*4 + value/16 : value;
        if (PackedFormats[to].bits[c])
            result |= (expanded >> (8-PackedFormats[to].bits[c])) << PackedFormats[to].shifts[c];
    }
    WritePackedPixel(target, PackedFormats[to].bytes, result);
}

struct Packed16Image {
    alignas(32) XBYTE source[2400];
    alignas(32) XBYTE target[2400];
    VxImageDescEx src, dst;
    int from, to, sourcePitch, targetPitch;

    Packed16Image(int conversion, int width = 257, int sourceOffset = 0, int targetOffset = 0,
                    bool reverseSource = false, bool reverseTarget = false)
        : from(PackedConversions[conversion][0]), to(PackedConversions[conversion][1]),
          sourcePitch(width * PackedFormats[from].bytes + 5), targetPitch(width * PackedFormats[to].bytes + 7) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(target, 0xa5, sizeof(target));
        DescribePackedFormat(src, from);
        DescribePackedFormat(dst, to);
        src.Width = dst.Width = width;
        src.Height = dst.Height = 2;
        src.BytesPerLine = reverseSource ? -sourcePitch : sourcePitch;
        dst.BytesPerLine = reverseTarget ? -targetPitch : targetPitch;
        src.Image = source + 32 + sourceOffset + (reverseSource ? sourcePitch : 0);
        dst.Image = target + 32 + targetOffset + (reverseTarget ? targetPitch : 0);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < width; ++x) {
                XDWORD pixel = from == 0 ? static_cast<XDWORD>((x*37+y*13+7)&255) << 16 |
                    static_cast<XDWORD>((x*53+y*11+5)&255) << 8 |
                    static_cast<XDWORD>((x*97+y*17+3)&255) |
                    static_cast<XDWORD>((x*29+y*101+128)&255) << 24 :
                    static_cast<XDWORD>((x*257+y*347+0x9b65)&65535);
                WritePackedPixel(src.Image+y*src.BytesPerLine+x*PackedFormats[from].bytes, PackedFormats[from].bytes, pixel);
            }
        }
    }

    void Run(bool flipped = false) {
        XBYTE before[sizeof(source)], expected[sizeof(target)];
        std::memcpy(before, source, sizeof(source));
        std::memcpy(expected, target, sizeof(target));
        VxImageDescEx beforeSrc = src, beforeDst = dst;
        ASSERT_EQ(VxImageDesc2PixelFormat(src), PackedFormats[from].format);
        ASSERT_EQ(VxImageDesc2PixelFormat(dst), PackedFormats[to].format);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < dst.Width; ++x) {
                const XBYTE *input = src.Image + (flipped ? 1 - y : y) * src.BytesPerLine + x * PackedFormats[from].bytes;
                XBYTE *output = expected + (dst.Image - target) + y * dst.BytesPerLine + x * PackedFormats[to].bytes;
                ReferencePackedPixel(input, output, from, to);
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

class Packed16BlitReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    int Conversion() const { return GetParam() % 8; }
    int Mode() const { return PackedModes[GetParam() / 8]; }
    void SetUp() override {
        previous = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(Mode()));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), Mode());
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(Packed16BlitReferenceTest, LiteralPixelsAndExhaustivePackedOrChannelValues) {
    // Independently fixed examples: A7/9B/65/D3 -> four packed layouts and back.
    static const XDWORD literal[] = {0xa79b65d3,0x9b3a,0x4d9a,0xcd9a,0xa96d};
    static const XDWORD expanded[] = {0xa79b65d3,0xff9c65d6,0xff9c63d6,0xff9c63d6,0xaa9966dd};
    Packed16Image image(Conversion(), 1);
    for (int y = 0; y < 2; ++y)
        WritePackedPixel(image.src.Image+y*image.src.BytesPerLine, PackedFormats[image.from].bytes, literal[image.from]);
    image.Run();
    EXPECT_EQ(ReadPackedPixel(image.dst.Image, PackedFormats[image.to].bytes),
              image.from == 0 ? literal[image.to] : expanded[image.from]);
    if (image.from == 0) {
        for (int c = 0; c < 4; ++c) {
            Packed16Image values(Conversion(), 256);
            for (int y = 0; y < 2; ++y) for (int x = 0; x < 256; ++x) {
                XDWORD pixel = (0xa79b65d3u & ~PackedFormats[0].masks[c]) | static_cast<XDWORD>(x) << PackedFormats[0].shifts[c];
                WritePackedPixel(values.src.Image+y*values.src.BytesPerLine+x*4,4,pixel);
            }
            values.Run();
        }
    } else {
        for (unsigned block = 0; block < 256; ++block) {
            Packed16Image values(Conversion(),256);
            for (int y = 0; y < 2; ++y) for (unsigned x = 0; x < 256; ++x)
                WritePackedPixel(values.src.Image+y*values.src.BytesPerLine+x*2,2,block*256+x);
            values.Run();
        }
    }
}

TEST_P(Packed16BlitReferenceTest, VectorChunksTailsAndIndependentUnalignedRows) {
    static const int offsets[] = {0, 1, 2, 3, 7, 15, 16, 31};
    for (int w = 0; w < 17; ++w) {
        for (int offset = 0; offset < 8; ++offset) {
            SCOPED_TRACE(PackedWidths[w]);
            SCOPED_TRACE(offsets[offset]);
            Packed16Image image(Conversion(), PackedWidths[w], offsets[offset], offsets[(offset * 3) % 8]);
            image.Run();
        }
    }
}

TEST_P(Packed16BlitReferenceTest, SignedPitchesAndUpsideDownPreserveEachRow) {
    for (int signs = 0; signs < 4; ++signs) {
        for (int flip = 0; flip < 2; ++flip) {
            Packed16Image image(Conversion(), 33, 1, 3, (signs & 1) != 0, (signs & 2) != 0);
            image.Run(flip != 0);
        }
    }
}

TEST_P(Packed16BlitReferenceTest, ChangedPixelsAtSameAddressesAreObserved) {
    Packed16Image image(Conversion(), 65);
    image.Run();
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < image.src.Width; ++x) {
            for (int b = 0; b < PackedFormats[image.from].bytes; ++b)
                image.src.Image[y * image.src.BytesPerLine + x * PackedFormats[image.from].bytes + b] ^= static_cast<XBYTE>(b * 23 + 137);
        }
    }
    image.Run();
}

static XBYTE *PackedProtectedStorage(size_t &page) {
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

static void PackedProtectedRows(int conversion, int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (!VxSetSIMDOverride(mode) || VxGetSIMDEffectiveBackend() != mode) std::_Exit(4);
    size_t sourcePage, targetPage;
    XBYTE *source = PackedProtectedStorage(sourcePage);
    XBYTE *target = PackedProtectedStorage(targetPage);
    for (int w = 0; w < 17; ++w) {
        Packed16Image image(conversion, PackedWidths[w]);
        int sourceBytes = PackedWidths[w] * PackedFormats[image.from].bytes;
        int targetBytes = PackedWidths[w] * PackedFormats[image.to].bytes;
        XBYTE *src = source + sourcePage - sourceBytes;
        XBYTE *dst = target + targetPage - targetBytes;
        std::memcpy(src, image.src.Image, sourceBytes);
        std::memset(dst - 16, 0xa5, targetBytes + 16);
        image.src.Image = src;
        image.dst.Image = dst;
        image.src.Height = image.dst.Height = 1;
        VxDoBlit(image.src, image.dst);
        for (int x = 0; x < PackedWidths[w]; ++x) {
            XBYTE expected[4];
            ReferencePackedPixel(src + x * PackedFormats[image.from].bytes, expected, image.from, image.to);
            if (std::memcmp(dst + x * PackedFormats[image.to].bytes, expected, PackedFormats[image.to].bytes)) std::_Exit(2);
        }
        for (int i = 1; i <= 16; ++i) if (dst[-i] != 0xa5) std::_Exit(5);
    }
    std::_Exit(0);
}

TEST_P(Packed16BlitReferenceTest, ExactSourceAndTargetRowsEndAtProtectedPages) {
    EXPECT_EXIT(PackedProtectedRows(Conversion(), Mode()), testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2, Packed16BlitReferenceTest, testing::Range(0, 32));
