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

static const int GenericModes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_AVX2};
static const XDWORD GenericSourceMasks[4][4] = {
    {0x03, 0x0c, 0x30, 0xc0}, {0x001f, 0x03e0, 0x7c00, 0x8000},
    {0xff, 0xff00, 0xff0000, 0}, {0xff, 0xff00, 0xff0000, 0xff000000}
};
static const XDWORD GenericTargetMasks[4][4] = {
    {0x0c, 0x30, 0x03, 0xc0}, {0x7c00, 0x03e0, 0x001f, 0x8000},
    {0xff0000, 0xff00, 0xff, 0}, {0xff00, 0xff0000, 0xff000000, 0xff}
};

static int GenericMaskShift(XDWORD mask) {
    int shift = 0;
    while (mask && !(mask & 1u)) { ++shift; mask /= 2; }
    return shift;
}

static int GenericMaskWidth(XDWORD mask) {
    int width = 0;
    while (mask) { width += mask & 1u; mask /= 2; }
    return width;
}

static void GenericDescribe(VxImageDescEx &desc, int bytes, const XDWORD *masks) {
    desc.BitsPerPixel = bytes * 8;
    desc.RedMask = masks[0];
    desc.GreenMask = masks[1];
    desc.BlueMask = masks[2];
    desc.AlphaMask = masks[3];
}

static XDWORD GenericRead(const XBYTE *data, int bytes) {
    XDWORD value = 0;
    for (int i = bytes - 1; i >= 0; --i) value = value * 256u + data[i];
    return value;
}

static void GenericWrite(XBYTE *data, int bytes, XDWORD value) {
    for (int i = 0; i < bytes; ++i) { data[i] = static_cast<XBYTE>(value % 256u); value /= 256u; }
}

// Independent specification: copy a repeating sequence of source bits, most significant first.
static XDWORD GenericReferencePixel(XDWORD pixel, const VxImageDescEx &src, const VxImageDescEx &dst) {
    const XDWORD source[] = {src.RedMask, src.GreenMask, src.BlueMask, src.AlphaMask};
    const XDWORD target[] = {dst.RedMask, dst.GreenMask, dst.BlueMask, dst.AlphaMask};
    XDWORD result = 0;
    for (int channel = 0; channel < 4; ++channel) {
        if (!target[channel]) continue;
        if (!source[channel]) {
            if (channel == 3) result |= target[channel];
            continue;
        }
        int sourceWidth = GenericMaskWidth(source[channel]);
        int targetWidth = GenericMaskWidth(target[channel]);
        XDWORD value = (pixel & source[channel]) >> GenericMaskShift(source[channel]);
        XDWORD component = 0;
        for (int bit = 0; bit < targetWidth; ++bit) {
            int sourceBit = sourceWidth - 1 - bit % sourceWidth;
            component = component * 2u + (value / (1u << sourceBit)) % 2u;
        }
        result |= component << GenericMaskShift(target[channel]);
    }
    return result;
}

struct GenericImage {
    XBYTE source[2200], target[2200];
    VxImageDescEx src, dst;
    int sourceBytes, targetBytes, sourcePitch, targetPitch;

    GenericImage(int from, int to, int width = 17, bool negativeSource = false, bool negativeTarget = false)
        : sourceBytes(from), targetBytes(to), sourcePitch(width * from + 3), targetPitch(width * to + 5) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(target, 0xa5, sizeof(target));
        GenericDescribe(src, from, GenericSourceMasks[from - 1]);
        GenericDescribe(dst, to, GenericTargetMasks[to - 1]);
        src.Width = dst.Width = width;
        src.Height = dst.Height = 2;
        src.BytesPerLine = negativeSource ? -sourcePitch : sourcePitch;
        dst.BytesPerLine = negativeTarget ? -targetPitch : targetPitch;
        src.Image = source + 8 + (negativeSource ? sourcePitch : 0);
        dst.Image = target + 8 + (negativeTarget ? targetPitch : 0);
        const XDWORD masks[] = {src.RedMask, src.GreenMask, src.BlueMask, src.AlphaMask};
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < width; ++x) {
                XDWORD pixel = 0;
                for (int c = 0; c < 4; ++c) {
                    int bits = GenericMaskWidth(masks[c]);
                    if (!bits) continue;
                    XDWORD value = static_cast<XDWORD>(x * 17 + y * 29 + c * 7) % (1u << bits);
                    if (x == 0) value = (1u << bits) - 1u;
                    pixel |= value << GenericMaskShift(masks[c]);
                }
                GenericWrite(src.Image + y * src.BytesPerLine + x * from, from, pixel);
            }
        }
    }

    void Run(bool flipped = false) {
        XBYTE beforeSource[sizeof(source)], expected[sizeof(target)];
        std::memcpy(beforeSource, source, sizeof(source));
        std::memcpy(expected, target, sizeof(target));
        VxImageDescEx beforeSrc = src, beforeDst = dst;
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < dst.Width; ++x) {
                const XBYTE *p = src.Image + (flipped ? 1 - y : y) * src.BytesPerLine + x * sourceBytes;
                XDWORD pixel = GenericReferencePixel(GenericRead(p, sourceBytes), src, dst);
                GenericWrite(expected + (dst.Image - target) + y * dst.BytesPerLine + x * targetBytes, targetBytes, pixel);
            }
        }
        if (flipped) VxDoBlitUpsideDown(src, dst);
        else VxDoBlit(src, dst);
        ASSERT_EQ(std::memcmp(target, expected, sizeof(target)), 0);
        EXPECT_EQ(std::memcmp(source, beforeSource, sizeof(source)), 0);
        EXPECT_EQ(std::memcmp(&src, &beforeSrc, sizeof(src)), 0);
        EXPECT_EQ(std::memcmp(&dst, &beforeDst, sizeof(dst)), 0);
    }
};

class GenericBytePairTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        previous = VxGetSIMDOverride();
        int mode = GenericModes[GetParam() / 16];
        ASSERT_TRUE(VxSetSIMDOverride(mode));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), mode);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(GenericBytePairTest, AllStoragePairsConvertMasksAndPreservePadding) {
    const int pair = GetParam() % 16;
    static const int widths[] = {1, 7, 17, 257};
    for (int i = 0; i < 4; ++i) {
        for (int flipped = 0; flipped < 2; ++flipped) {
            SCOPED_TRACE(widths[i]);
            GenericImage image(pair / 4 + 1, pair % 4 + 1, widths[i]);
            image.Run(flipped != 0);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2, GenericBytePairTest, testing::Range(0, 48));

class GenericBlitReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        previous = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(GetParam()));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), GetParam());
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(GenericBlitReferenceTest, Rgb332ExpandsAll256ColorsWithOpaqueAlpha) {
    static const XDWORD expanded2[] = {0, 85, 170, 255};
    static const XDWORD expanded3[] = {0, 36, 73, 109, 146, 182, 219, 255};
    GenericImage image(1, 4, 256);
    VxPixelFormat2ImageDesc(_8_RGB332, image.src);
    VxPixelFormat2ImageDesc(_32_ABGR8888, image.dst);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 256; ++x) image.src.Image[y * image.src.BytesPerLine + x] = static_cast<XBYTE>(x);
    image.Run();
    for (int x = 0; x < 256; ++x) {
        XDWORD expected = 0xff000000u + expanded2[x % 4] * 0x10000u + expanded3[(x / 4) % 8] * 0x100u + expanded3[x / 32];
        ASSERT_EQ(GenericRead(image.dst.Image + x * 4, 4), expected) << x;
    }
}

TEST_P(GenericBlitReferenceTest, Argb2222ExpandsEveryColorAndAlphaLevel) {
    static const XDWORD expanded[] = {0, 85, 170, 255};
    GenericImage image(1, 4, 256);
    VxPixelFormat2ImageDesc(_8_ARGB2222, image.src);
    VxPixelFormat2ImageDesc(_32_ABGR8888, image.dst);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 256; ++x) image.src.Image[y * image.src.BytesPerLine + x] = static_cast<XBYTE>(x);
    image.Run();
    for (int x = 0; x < 256; ++x) {
        XDWORD expected = expanded[x / 64] * 0x1000000u + expanded[x % 4] * 0x10000u + expanded[(x / 4) % 4] * 0x100u + expanded[(x / 16) % 4];
        ASSERT_EQ(GenericRead(image.dst.Image + x * 4, 4), expected) << x;
    }
}

TEST_P(GenericBlitReferenceTest, OneBitAlphaMeansTransparentOrFullyOpaque) {
    GenericImage image(2, 4, 2);
    VxPixelFormat2ImageDesc(_16_ARGB1555, image.src);
    VxPixelFormat2ImageDesc(_32_ABGR8888, image.dst);
    for (int y = 0; y < 2; ++y) {
        GenericWrite(image.src.Image + y * image.src.BytesPerLine, 2, 0x7fffu);
        GenericWrite(image.src.Image + y * image.src.BytesPerLine + 2, 2, 0xffffu);
    }
    VxDoBlit(image.src, image.dst);
    EXPECT_EQ(GenericRead(image.dst.Image, 4), 0x00ffffffu);
    EXPECT_EQ(GenericRead(image.dst.Image + 4, 4), 0xffffffffu);
}

TEST_P(GenericBlitReferenceTest, MissingAlphaIsOpaqueForEveryStoragePairThatHasTargetAlpha) {
    for (int from = 1; from <= 4; ++from) {
        for (int to = 1; to <= 4; ++to) {
            GenericImage image(from, to);
            image.src.AlphaMask = 0;
            image.Run();
        }
    }
}

TEST_P(GenericBlitReferenceTest, AllOneToEightBitWidthsRepeatOrTruncateTheSourcePattern) {
    for (int sourceBits = 1; sourceBits <= 8; ++sourceBits) {
        for (int targetBits = 1; targetBits <= 8; ++targetBits) {
            GenericImage image(1, 4, 1 << sourceBits);
            image.src.RedMask = (1u << sourceBits) - 1u;
            image.src.GreenMask = image.src.BlueMask = image.src.AlphaMask = 0;
            image.dst.RedMask = ((1u << targetBits) - 1u) * 0x10000u;
            image.dst.GreenMask = image.dst.BlueMask = image.dst.AlphaMask = 0;
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < image.src.Width; ++x) image.src.Image[y * image.src.BytesPerLine + x] = static_cast<XBYTE>(x);
            image.Run();
        }
    }
}

TEST_P(GenericBlitReferenceTest, WideChannelsRepeatAndTruncateThroughTheWhole32BitRange) {
    static const XDWORD values[] = {0, 1, 0xffffffffu, 0x80000000u, 0xaaaaaaaau, 0x55555555u, 0x12345678u, 0x76543210u};
    for (int sourceBits = 1; sourceBits <= 32; ++sourceBits) {
        for (int targetBits = 1; targetBits <= 32; ++targetBits) {
            GenericImage image(4, 4, 8);
            image.src.RedMask = 0xffffffffu >> (32 - sourceBits);
            image.src.GreenMask = image.src.BlueMask = image.src.AlphaMask = 0;
            image.dst.RedMask = 0xffffffffu << (32 - targetBits);
            image.dst.GreenMask = image.dst.BlueMask = image.dst.AlphaMask = 0;
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 8; ++x)
                    GenericWrite(image.src.Image + y * image.src.BytesPerLine + x * 4, 4, values[x] & image.src.RedMask);
            }
            image.Run();
        }
    }
}

TEST_P(GenericBlitReferenceTest, FullWidthAlphaPreservesOpaqueDefaultAndOneBitEndpoints) {
    GenericImage image(4, 4, 2);
    image.src.RedMask = image.src.GreenMask = image.src.BlueMask = 0;
    image.src.AlphaMask = 1;
    image.dst.RedMask = image.dst.GreenMask = image.dst.BlueMask = 0;
    image.dst.AlphaMask = 0xffffffffu;
    for (int y = 0; y < 2; ++y) {
        GenericWrite(image.src.Image + y * image.src.BytesPerLine, 4, 0);
        GenericWrite(image.src.Image + y * image.src.BytesPerLine + 4, 4, 1);
    }
    image.Run();
    EXPECT_EQ(GenericRead(image.dst.Image, 4), 0u);
    EXPECT_EQ(GenericRead(image.dst.Image + 4, 4), 0xffffffffu);
    image.src.AlphaMask = 0;
    image.Run();
    EXPECT_EQ(GenericRead(image.dst.Image, 4), 0xffffffffu);
    EXPECT_EQ(GenericRead(image.dst.Image + 4, 4), 0xffffffffu);
}

TEST_P(GenericBlitReferenceTest, SignedPitchesAndIndependentRowPadding) {
    for (int from = 1; from <= 4; ++from) {
        for (int to = 1; to <= 4; ++to) {
            for (int sign = 1; sign <= 3; ++sign) {
                for (int flip = 0; flip < 2; ++flip) {
                    GenericImage image(from, to, 17, (sign & 1) != 0, (sign & 2) != 0);
                    image.Run(flip != 0);
                }
            }
        }
    }
}

TEST_P(GenericBlitReferenceTest, ExistingFiveAndSixBitExpansionRetainsBitReplication) {
    static const XDWORD expanded5[] = {
        0, 8, 16, 24, 33, 41, 49, 57, 66, 74, 82, 90, 99, 107, 115, 123,
        132, 140, 148, 156, 165, 173, 181, 189, 198, 206, 214, 222, 231, 239, 247, 255
    };
    GenericImage image(2, 4, 256);
    VxPixelFormat2ImageDesc(_16_RGB565, image.src);
    VxPixelFormat2ImageDesc(_32_ABGR8888, image.dst);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 256; ++x) {
            XDWORD pixel = static_cast<XDWORD>((x % 32) * 2048 + (x % 64) * 32 + (31 - x % 32));
            GenericWrite(image.src.Image + y * image.src.BytesPerLine + x * 2, 2, pixel);
        }
    }
    image.Run();
    for (int x = 0; x < 256; ++x) {
        XDWORD pixel = GenericRead(image.dst.Image + x * 4, 4);
        EXPECT_EQ(pixel & 0xffu, expanded5[x % 32]);
        EXPECT_EQ((pixel / 0x10000u) & 0xffu, expanded5[31 - x % 32]);
        EXPECT_EQ((pixel / 0x100u) & 0xffu, static_cast<XDWORD>((x % 64) * 4 + (x % 64) / 16));
    }
}

static XBYTE *GenericProtectedStorage(size_t &page) {
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

static void GenericGuardedRows(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (!VxSetSIMDOverride(mode) || VxGetSIMDEffectiveBackend() != mode) std::_Exit(4);
    size_t sourcePage, targetPage;
    XBYTE *source = GenericProtectedStorage(sourcePage);
    XBYTE *target = GenericProtectedStorage(targetPage);
    static const int widths[] = {1, 7, 17};
    for (int from = 1; from <= 4; ++from) {
        for (int to = 1; to <= 4; ++to) {
            for (int i = 0; i < 3; ++i) {
                GenericImage image(from, to, widths[i]);
                XBYTE *src = source + sourcePage - widths[i] * from;
                XBYTE *dst = target + targetPage - widths[i] * to;
                std::memcpy(src, image.src.Image, widths[i] * from);
                std::memset(dst, 0xa5, widths[i] * to);
                image.src.Image = src;
                image.dst.Image = dst;
                image.src.Height = image.dst.Height = 1;
                VxDoBlit(image.src, image.dst);
                for (int x = 0; x < widths[i]; ++x) {
                    XDWORD expected = GenericReferencePixel(GenericRead(src + x * from, from), image.src, image.dst);
                    if (GenericRead(dst + x * to, to) != expected) std::_Exit(2);
                }
            }
        }
    }
    std::_Exit(0);
}

TEST_P(GenericBlitReferenceTest, SourceAndTargetRowsEndAtProtectedPages) {
    EXPECT_EXIT(GenericGuardedRows(GetParam()), testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2, GenericBlitReferenceTest, testing::ValuesIn(GenericModes));
