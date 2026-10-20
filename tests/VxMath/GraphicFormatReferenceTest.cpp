#include "VxMath.h"
#include <cstring>
#include <gtest/gtest.h>

struct FormatReference {
    int Id;
    int Bits;
    XDWORD Red, Green, Blue, Alpha, Flags;
    int Detected;
};
// Literal fields from original VxMath.dll 2.1.0.14, independently called natively.
static const FormatReference NativeFormats[] = {
    {1, 32, 0xff0000u, 0xff00u, 0xffu, 0xff000000u, 0, 1},
    {2, 32, 0xff0000u, 0xff00u, 0xffu, 0x0u, 0, 2},
    {3, 24, 0xff0000u, 0xff00u, 0xffu, 0x0u, 0, 3},
    {4, 16, 0xf800u, 0x7e0u, 0x1fu, 0x0u, 0, 4},
    {5, 16, 0x7c00u, 0x3e0u, 0x1fu, 0x0u, 0, 5},
    {6, 16, 0x7c00u, 0x3e0u, 0x1fu, 0x8000u, 0, 6},
    {7, 16, 0xf00u, 0xf0u, 0xfu, 0xf000u, 0, 7},
    {8, 8, 0xe0u, 0x1cu, 0x3u, 0x0u, 0, 8},
    {9, 8, 0x30u, 0xcu, 0x3u, 0xc0u, 0, 9},
    {10, 32, 0xffu, 0xff00u, 0xff0000u, 0xff000000u, 0, 10},
    {11, 32, 0xff000000u, 0xff0000u, 0xff00u, 0xffu, 0, 11},
    {12, 32, 0xff00u, 0xff0000u, 0xff000000u, 0xffu, 0, 12},
    {13, 32, 0xffu, 0xff00u, 0xff0000u, 0x0u, 0, 13},
    {14, 24, 0xffu, 0xff00u, 0xff0000u, 0x0u, 0, 14},
    {15, 16, 0x1fu, 0x7e0u, 0xf800u, 0x0u, 0, 15},
    {16, 16, 0x1fu, 0x3e0u, 0x7c00u, 0x0u, 0, 16},
    {17, 16, 0x1fu, 0x3e0u, 0x7c00u, 0x8000u, 0, 17},
    {18, 16, 0xfu, 0xf0u, 0xf00u, 0xf000u, 0, 18},
    {19, 0, 0x0u, 0x0u, 0x0u, 0x0u, 19, 19},
    {20, 0, 0x0u, 0x0u, 0x0u, 0x0u, 20, 20},
    {21, 0, 0x0u, 0x0u, 0x0u, 0x0u, 21, 21},
    {22, 0, 0x0u, 0x0u, 0x0u, 0x0u, 22, 22},
    {23, 0, 0x0u, 0x0u, 0x0u, 0x0u, 23, 23},
    {24, 16, 0xffu, 0xff00u, 0x0u, 0x0u, 24, 24},
    {25, 32, 0xffffu, 0xffff0000u, 0x0u, 0x0u, 25, 25},
    {26, 16, 0x1fu, 0x3e0u, 0xfc00u, 0x0u, 26, 26},
    {27, 32, 0xffu, 0xff00u, 0xff0000u, 0x0u, 27, 13}
};

static VxImageDescEx FormatDescriptor() {
    VxImageDescEx desc;
    desc.Flags = 0x6bad;
    desc.Width = 17;
    desc.Height = 3;
    desc.BytesPerLine = 84;
    desc.BitsPerPixel = 7;
    desc.RedMask = 0x11223344;
    desc.GreenMask = 0x55667788;
    desc.BlueMask = 0x99aabbcc;
    desc.AlphaMask = 0x12345678;
    desc.BytesPerColorEntry = 4;
    desc.ColorMapEntries = 16;
    return desc;
}

static VxImageDescEx ReferenceDescriptor(int pf) {
    VxImageDescEx desc = FormatDescriptor();
    const FormatReference &ref = NativeFormats[pf - 1];
    desc.BitsPerPixel = ref.Bits;
    desc.RedMask = ref.Red;
    desc.GreenMask = ref.Green;
    desc.BlueMask = ref.Blue;
    desc.AlphaMask = ref.Alpha;
    desc.Flags = ref.Flags;
    return desc;
}

TEST(GraphicFormatReferenceTest, EveryOriginalFormatWritesOnlySixDescriptorFields) {
    XBYTE pixels[8] = {1, 2, 3, 4}, palette[8] = {5, 6, 7, 8};
    XBYTE savedPixels[8], savedPalette[8];
    std::memcpy(savedPixels, pixels, sizeof(pixels));
    std::memcpy(savedPalette, palette, sizeof(palette));
    for (int i = 0; i < 27; ++i) {
        const FormatReference &ref = NativeFormats[i];
        VxImageDescEx desc = FormatDescriptor(), expected;
        desc.Image = pixels;
        desc.ColorMap = palette;
        std::memcpy(&expected, &desc, sizeof(desc));
        expected.BitsPerPixel = ref.Bits;
        expected.RedMask = ref.Red;
        expected.GreenMask = ref.Green;
        expected.BlueMask = ref.Blue;
        expected.AlphaMask = ref.Alpha;
        expected.Flags = ref.Flags;
        VxPixelFormat2ImageDesc((VX_PIXELFORMAT)ref.Id, desc);
        EXPECT_EQ(0, std::memcmp(&desc, &expected, sizeof(desc))) << "format " << ref.Id;
    }
    EXPECT_EQ(0, std::memcmp(pixels, savedPixels, sizeof(pixels)));
    EXPECT_EQ(0, std::memcmp(palette, savedPalette, sizeof(palette)));
}

TEST(GraphicFormatReferenceTest, LinearFormatsClearStaleSpecialFlags) {
    for (int pf = 1; pf <= 18; ++pf) {
        VxImageDescEx desc = FormatDescriptor();
        desc.Flags = _DXT5;
        VxPixelFormat2ImageDesc((VX_PIXELFORMAT)pf, desc);
        EXPECT_EQ(0u, desc.Flags);
    }
}

TEST(GraphicFormatReferenceTest, CompressedFormatsUseFlagsAndZeroBitDepth) {
    for (int pf = 19; pf <= 23; ++pf) {
        VxImageDescEx desc = FormatDescriptor();
        VxPixelFormat2ImageDesc((VX_PIXELFORMAT)pf, desc);
        EXPECT_EQ(0, desc.BitsPerPixel);
        EXPECT_EQ(XDWORD(pf), desc.Flags);
        EXPECT_EQ(0u, desc.RedMask | desc.GreenMask | desc.BlueMask | desc.AlphaMask);
        EXPECT_EQ(pf, VxImageDesc2PixelFormat(desc));
    }
}

TEST(GraphicFormatReferenceTest, BumpDescriptorsUseNativeMasksAndStorageDepths) {
    for (int pf = 24; pf <= 27; ++pf) {
        VxImageDescEx desc = FormatDescriptor();
        const FormatReference &ref = NativeFormats[pf - 1];
        VxPixelFormat2ImageDesc((VX_PIXELFORMAT)pf, desc);
        EXPECT_EQ(ref.Bits, desc.BitsPerPixel);
        EXPECT_EQ(ref.Red, desc.BumpDuMask);
        EXPECT_EQ(ref.Green, desc.BumpDvMask);
        EXPECT_EQ(ref.Blue, desc.BumpLumMask);
        EXPECT_EQ(0u, desc.AlphaMask);
        EXPECT_EQ(XDWORD(pf), desc.Flags);
    }
}

TEST(GraphicFormatReferenceTest, InvalidFormatsLeaveTheWholeDescriptorUntouched) {
    const int invalid[] = {0, 28, 29, 30, 999, -1, (-2147483647 - 1), 2147483647};
    for (int i = 0; i < 8; ++i) {
        VxImageDescEx desc = FormatDescriptor(), before;
        std::memcpy(&before, &desc, sizeof(desc));
        VxPixelFormat2ImageDesc((VX_PIXELFORMAT)invalid[i], desc);
        EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc))) << invalid[i];
    }
}

TEST(GraphicFormatReferenceTest, StandardMasksTakePriorityOverStaleSpecialFlags) {
    const XDWORD flags[] = {0, 19, 27, 28, 0xffffffff};
    for (int pf = 1; pf <= 18; ++pf) {
        for (int i = 0; i < 5; ++i) {
            VxImageDescEx desc = ReferenceDescriptor(pf), before;
            desc.Flags = flags[i];
            std::memcpy(&before, &desc, sizeof(desc));
            EXPECT_EQ(pf, VxImageDesc2PixelFormat(desc)) << "format " << pf << " flags " << flags[i];
            EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
        }
    }
}

TEST(GraphicFormatReferenceTest, BumpMasksAreRecognizedWithoutFlagsAndIgnoreAlpha) {
    const XDWORD alpha[] = {0, 0x12345678, 0xff000000};
    for (int pf = 24; pf <= 26; ++pf) {
        for (int i = 0; i < 3; ++i) {
            VxImageDescEx desc = ReferenceDescriptor(pf), before;
            desc.Flags = 0;
            desc.AlphaMask = alpha[i];
            std::memcpy(&before, &desc, sizeof(desc));
            EXPECT_EQ(pf, VxImageDesc2PixelFormat(desc));
            EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
        }
    }
}

TEST(GraphicFormatReferenceTest, SharedBgrAndLuminanceMasksUseTheNativeCanonicalFormat) {
    const XDWORD alpha[] = {0, 0xff000000, 0x12345678};
    const int expected[] = {13, 10, 27};
    const XDWORD flags[] = {0, 19, 27, 28, 0xffffffff};
    for (int a = 0; a < 3; ++a) {
        for (int i = 0; i < 5; ++i) {
            VxImageDescEx desc = ReferenceDescriptor(27);
            desc.AlphaMask = alpha[a];
            desc.Flags = flags[i];
            EXPECT_EQ(expected[a], VxImageDesc2PixelFormat(desc));
        }
    }
}

TEST(GraphicFormatReferenceTest, UnknownMasksUseOnlyTheNativeSpecialFlagRange) {
    const XDWORD flags[] = {0, 1, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 0x80000000, 0xffffffff};
    const int expected[] = {0, 0, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 0, 0, 0};
    for (int i = 0; i < 15; ++i) {
        VxImageDescEx desc = FormatDescriptor();
        desc.Flags = flags[i];
        EXPECT_EQ(expected[i], VxImageDesc2PixelFormat(desc));
    }
}

TEST(GraphicFormatReferenceTest, NearMissesDoNotMatchAStandardColorFormat) {
    for (int i = 0; i < 5; ++i) {
        VxImageDescEx desc = ReferenceDescriptor(1);
        if (i == 0) desc.BitsPerPixel = 31;
        if (i == 1) desc.RedMask ^= 1;
        if (i == 2) desc.GreenMask ^= 1;
        if (i == 3) desc.BlueMask ^= 1;
        if (i == 4) desc.AlphaMask ^= 1;
        EXPECT_EQ(UNKNOWN_PF, VxImageDesc2PixelFormat(desc));
    }
}

TEST(GraphicFormatReferenceTest, L6V5U5HasSixLuminanceBitsAndDoesNotCollapseToBgr555) {
    VxImageDescEx desc;
    VxPixelFormat2ImageDesc(_16_L6V5U5, desc);
    XDWORD du, dv, lum, alpha;
    VxGetBitCounts(desc, du, dv, lum, alpha);
    EXPECT_EQ(5u, du);
    EXPECT_EQ(5u, dv);
    EXPECT_EQ(6u, lum);
    EXPECT_EQ(0u, alpha);
    desc.Flags = 0;
    EXPECT_EQ(_16_L6V5U5, VxImageDesc2PixelFormat(desc));
    desc.BumpLumMask = 0x7c00;
    desc.Flags = _16_L6V5U5;
    EXPECT_EQ(_16_BGR555, VxImageDesc2PixelFormat(desc));
}

TEST(GraphicFormatReferenceTest, RecognitionIgnoresDimensionsPitchAndPaletteMetadata) {
    VxImageDescEx desc = ReferenceDescriptor(8), before;
    desc.Width = 0;
    desc.Height = -1;
    desc.BytesPerLine = 91;
    desc.ColorMapEntries = 256;
    desc.BytesPerColorEntry = 3;
    std::memcpy(&before, &desc, sizeof(desc));
    EXPECT_EQ(_8_RGB332, VxImageDesc2PixelFormat(desc));
    EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
}

TEST(GraphicFormatReferenceTest, CorrectedBgrDescriptorDrivesActualColorConversion) {
    const XDWORD source[] = {0x80112233, 0x99446688, 0x00abcdef};
    XDWORD output[] = {0xa5a5a5a5, 0xa5a5a5a5, 0xa5a5a5a5, 0xa5a5a5a5};
    VxImageDescEx src, dst;
    VxPixelFormat2ImageDesc(_32_ARGB8888, src);
    VxPixelFormat2ImageDesc(_32_BGR888, dst);
    src.Width = dst.Width = 3;
    src.Height = dst.Height = 1;
    src.BytesPerLine = 12;
    dst.BytesPerLine = 16;
    src.Image = (XBYTE *)source;
    dst.Image = (XBYTE *)output;
    VxDoBlit(src, dst);
    EXPECT_EQ(0x00332211u, output[0]);
    EXPECT_EQ(0x00886644u, output[1]);
    EXPECT_EQ(0x00efcdabu, output[2]);
    EXPECT_EQ(0xa5a5a5a5u, output[3]);
}
