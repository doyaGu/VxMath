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

static const int PackedPaletteModes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_AVX2};
static const XDWORD PackedTargetMasks[6][4] = {
    {0xff0000, 0xff00, 0xff, 0xff000000}, {0xff0000, 0xff00, 0xff, 0},
    {0xff, 0xff00, 0xff0000, 0xff000000}, {0xff000000, 0xff0000, 0xff00, 0xff},
    {0xff00, 0xff0000, 0xff000000, 0xff}, {0xff, 0xff00, 0xff0000, 0}
};

static XDWORD PackedReferenceColor(int index, int layout, int bytes) {
    XDWORD b = (index * 17 + 3) & 255;
    XDWORD g = (index * 29 + 5) & 255;
    XDWORD r = (index * 43 + 7) & 255;
    XDWORD a = bytes == 4 ? (index * 61 + 128) & 255 : 255;
    switch (layout) {
    case 0: return a * 0x1000000u + r * 0x10000u + g * 0x100u + b;
    case 1: return r * 0x10000u + g * 0x100u + b;
    case 2: return a * 0x1000000u + b * 0x10000u + g * 0x100u + r;
    case 3: return r * 0x1000000u + g * 0x10000u + b * 0x100u + a;
    case 4: return b * 0x1000000u + g * 0x10000u + r * 0x100u + a;
    default: return b * 0x10000u + g * 0x100u + r;
    }
}

struct PackedPaletteImage {
    XBYTE source[300], target[2200], palette[80];
    VxImageDescEx src, dst;
    int bits, bytes, entries, layout, sourcePitch, targetPitch;

    PackedPaletteImage(int depth, int entryBytes = 4, int colors = 0, int targetLayout = 0,
                       int width = 19, bool negativeSource = false, bool negativeTarget = false)
        : bits(depth), bytes(entryBytes), entries(colors ? colors : 1 << depth), layout(targetLayout),
          sourcePitch((width * depth + 7) / 8 + 3), targetPitch(width * 4 + 8) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(target, 0xa5, sizeof(target));
        std::memset(palette, 0xa5, sizeof(palette));
        src.Width = dst.Width = width;
        src.Height = dst.Height = 2;
        src.BitsPerPixel = bits;
        src.BytesPerLine = negativeSource ? -sourcePitch : sourcePitch;
        src.Image = source + 8 + (negativeSource ? sourcePitch : 0);
        src.RedMask = 0xff0000;
        src.GreenMask = 0xff00;
        src.BlueMask = 0xff;
        src.AlphaMask = bytes == 4 ? 0xff000000 : 0;
        src.ColorMapEntries = static_cast<short>(entries);
        src.BytesPerColorEntry = static_cast<short>(bytes);
        src.ColorMap = palette + 8;
        dst.BitsPerPixel = 32;
        dst.BytesPerLine = negativeTarget ? -targetPitch : targetPitch;
        dst.Image = target + 8 + (negativeTarget ? targetPitch : 0);
        dst.RedMask = PackedTargetMasks[layout][0];
        dst.GreenMask = PackedTargetMasks[layout][1];
        dst.BlueMask = PackedTargetMasks[layout][2];
        dst.AlphaMask = PackedTargetMasks[layout][3];
        for (int i = 0; i < entries; ++i) {
            XBYTE *entry = src.ColorMap + i * bytes;
            entry[0] = static_cast<XBYTE>(i * 17 + 3);
            entry[1] = static_cast<XBYTE>(i * 29 + 5);
            entry[2] = static_cast<XBYTE>(i * 43 + 7);
            if (bytes == 4) entry[3] = static_cast<XBYTE>(i * 61 + 128);
        }
        for (int y = 0; y < 2; ++y) {
            std::memset(src.Image + y * src.BytesPerLine, 0, (width * bits + 7) / 8);
            for (int x = 0; x < width; ++x) {
                int index = (x * 37 + y * 11) % entries;
                if (bits == 1) src.Image[y * src.BytesPerLine + x / 8] |= static_cast<XBYTE>(index * (128 >> (x % 8)));
                else src.Image[y * src.BytesPerLine + x / 2] |= static_cast<XBYTE>(index * (x % 2 ? 1 : 16));
            }
        }
    }

    XDWORD Pixel(int x, int y) const {
        XDWORD value;
        std::memcpy(&value, dst.Image + y * dst.BytesPerLine + x * 4, 4);
        return value;
    }

    void Run(bool flipped = false) {
        XBYTE beforeSource[sizeof(source)], beforePalette[sizeof(palette)];
        std::memcpy(beforeSource, source, sizeof(source));
        std::memcpy(beforePalette, palette, sizeof(palette));
        VxImageDescEx beforeSrc = src, beforeDst = dst;
        if (flipped) VxDoBlitUpsideDown(src, dst);
        else VxDoBlit(src, dst);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < dst.Width; ++x) {
                int index = (x * 37 + (flipped ? 1 - y : y) * 11) % entries;
                ASSERT_EQ(Pixel(x, y), PackedReferenceColor(index, layout, bytes)) << x << "," << y;
            }
            for (int x = dst.Width * 4; x < targetPitch; ++x) EXPECT_EQ(target[8 + y * targetPitch + x], 0xa5);
        }
        EXPECT_EQ(std::memcmp(source, beforeSource, sizeof(source)), 0);
        EXPECT_EQ(std::memcmp(palette, beforePalette, sizeof(palette)), 0);
        EXPECT_EQ(std::memcmp(&src, &beforeSrc, sizeof(src)), 0);
        EXPECT_EQ(std::memcmp(&dst, &beforeDst, sizeof(dst)), 0);
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(target[i], 0xa5);
            EXPECT_EQ(target[8 + 2 * targetPitch + i], 0xa5);
        }
    }
};

class PackedPaletteReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    int Bits() const { return GetParam() % 2 ? 4 : 1; }
    void SetUp() override {
        previous = VxGetSIMDOverride();
        int mode = PackedPaletteModes[GetParam() / 2];
        ASSERT_TRUE(VxSetSIMDOverride(mode));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), mode);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(PackedPaletteReferenceTest, NativePaletteLayoutsAndRowDirection) {
    for (int layout = 0; layout < 6; ++layout) {
        for (int bytes = 3; bytes <= 4; ++bytes) {
            for (int count = 1; count <= 1 << Bits(); count *= 2) {
                for (int flip = 0; flip < 2; ++flip) {
                    SCOPED_TRACE(layout);
                    SCOPED_TRACE(bytes);
                    SCOPED_TRACE(count);
                    PackedPaletteImage image(Bits(), bytes, count, layout);
                    image.Run(flip != 0);
                }
            }
        }
    }
}

TEST_P(PackedPaletteReferenceTest, LiteralBytesEstablishHighBitAndHighNibbleOrder) {
    PackedPaletteImage image(Bits(), 4, 0, 0, Bits() == 1 ? 8 : 4);
    image.src.Height = image.dst.Height = 1;
    image.src.Image[0] = Bits() == 1 ? 0xb2 : 0x2e;
    image.src.Image[1] = 0x71;
    static const int bits[] = {1, 0, 1, 1, 0, 0, 1, 0};
    static const int nibbles[] = {2, 14, 7, 1};
    VxDoBlit(image.src, image.dst);
    for (int x = 0; x < image.dst.Width; ++x)
        EXPECT_EQ(image.Pixel(x, 0), PackedReferenceColor(Bits() == 1 ? bits[x] : nibbles[x], 0, 4));
}

TEST_P(PackedPaletteReferenceTest, OddWidthsDoNotDecodePaddingOrCarryToNextRow) {
    static const int widths[] = {1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 257};
    for (int i = 0; i < 11; ++i) {
        SCOPED_TRACE(widths[i]);
        PackedPaletteImage image(Bits(), 4, 0, 0, widths[i]);
        image.Run();
        std::memset(image.target, 0xa5, sizeof(image.target));
        image.Run(true);
    }
}

TEST_P(PackedPaletteReferenceTest, SignedSourceAndDestinationPitches) {
    for (int signs = 1; signs < 4; ++signs) {
        for (int flip = 0; flip < 2; ++flip) {
            PackedPaletteImage image(Bits(), 3, 0, 3, 17, (signs & 1) != 0, (signs & 2) != 0);
            image.Run(flip != 0);
        }
    }
}

TEST_P(PackedPaletteReferenceTest, UndeclaredIndicesBecomeZero) {
    PackedPaletteImage image(Bits(), 4, 1, 0, 9);
    for (int y = 0; y < 2; ++y) std::memset(image.src.Image + y * image.src.BytesPerLine, 0xff, (9 * Bits() + 7) / 8);
    VxDoBlit(image.src, image.dst);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 9; ++x) EXPECT_EQ(image.Pixel(x, y), 0u);
}

TEST_P(PackedPaletteReferenceTest, PaletteChangesAtSameAddressAreVisible) {
    PackedPaletteImage image(Bits(), 4, 1);
    image.Run();
    image.src.ColorMap[0] = 29;
    image.src.ColorMap[1] = 61;
    image.src.ColorMap[2] = 107;
    image.src.ColorMap[3] = 193;
    VxDoBlit(image.src, image.dst);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < image.dst.Width; ++x) EXPECT_EQ(image.Pixel(x, y), 0xc16b3d1du);
}

TEST_P(PackedPaletteReferenceTest, ZeroSourceMasksPreserveBgrPaletteContract) {
    for (int layout = 0; layout < 6; ++layout) {
        PackedPaletteImage image(Bits(), 4, 0, layout);
        image.src.RedMask = image.src.GreenMask = image.src.BlueMask = image.src.AlphaMask = 0;
        image.Run();
    }
}

TEST_P(PackedPaletteReferenceTest, UnconfirmedPaletteEntrySizesAndDestinationDepthsStayUnsupported) {
    PackedPaletteImage image(Bits());
    XBYTE before[sizeof(image.target)];
    std::memcpy(before, image.target, sizeof(before));
    for (int bytes = 2; bytes <= 5; bytes += 3) {
        image.src.BytesPerColorEntry = static_cast<short>(bytes);
        VxDoBlit(image.src, image.dst);
        VxDoBlitUpsideDown(image.src, image.dst);
        EXPECT_EQ(std::memcmp(before, image.target, sizeof(before)), 0);
    }
    image.src.BytesPerColorEntry = 4;
    for (int depth = 16; depth <= 24; depth += 8) {
        image.dst.BitsPerPixel = depth;
        VxDoBlit(image.src, image.dst);
        VxDoBlitUpsideDown(image.src, image.dst);
        EXPECT_EQ(std::memcmp(before, image.target, sizeof(before)), 0);
    }
}

static XBYTE *PackedGuardedStorage(size_t &page) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    page = info.dwPageSize;
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

static void PackedGuardedDecode(int mode, int bits) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (!VxSetSIMDOverride(mode) || VxGetSIMDEffectiveBackend() != mode) std::_Exit(4);
    size_t sourcePage, palettePage;
    XBYTE *sourceStorage = PackedGuardedStorage(sourcePage);
    XBYTE *paletteStorage = PackedGuardedStorage(palettePage);
    static const int widths[] = {1, 7, 8, 9, 15, 16, 17, 31};
    for (int bytes = 3; bytes <= 4; ++bytes) {
        for (int count = 1; count <= 1 << bits; count *= 1 << bits) {
            for (int i = 0; i < 8; ++i) {
                PackedPaletteImage image(bits, bytes, count, 0, widths[i]);
                int sourceBytes = (widths[i] * bits + 7) / 8;
                XBYTE *source = sourceStorage + sourcePage - sourceBytes;
                XBYTE *palette = paletteStorage + palettePage - count * bytes;
                std::memcpy(source, image.src.Image, sourceBytes);
                std::memcpy(palette, image.src.ColorMap, count * bytes);
                image.src.Image = source;
                image.src.ColorMap = palette;
                image.src.Height = image.dst.Height = 1;
                VxDoBlit(image.src, image.dst);
                for (int x = 0; x < widths[i]; ++x)
                    if (image.Pixel(x, 0) != PackedReferenceColor((x * 37) % count, 0, bytes)) std::_Exit(2);
            }
        }
    }
    std::_Exit(0);
}

TEST_P(PackedPaletteReferenceTest, ReadsOnlyPackedRowAndDeclaredPaletteAtProtectedPages) {
    EXPECT_EXIT(PackedGuardedDecode(PackedPaletteModes[GetParam() / 2], Bits()), testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2, PackedPaletteReferenceTest, testing::Range(0, 6));
