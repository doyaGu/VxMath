#include "VxMath.h"
#include <cstring>
#include <gtest/gtest.h>

static VxImageDescEx MaskDescriptor(XBYTE *pixels, XBYTE *palette) {
    VxImageDescEx desc;
    desc.Flags = 0x6bad;
    desc.Width = 17;
    desc.Height = 3;
    desc.BytesPerLine = 84;
    desc.BitsPerPixel = 32;
    desc.RedMask = 0xf800;
    desc.GreenMask = 0x31c;
    desc.BlueMask = 0xaaaaaaaa;
    desc.AlphaMask = 0x81000000;
    desc.BytesPerColorEntry = 4;
    desc.ColorMapEntries = 16;
    desc.ColorMap = palette;
    desc.Image = pixels;
    return desc;
}

TEST(GraphicMaskReferenceTest, EveryContiguousMaskReportsItsWidthAndShift) {
    for (int width = 1; width <= 32; ++width) {
        for (int shift = 0; shift + width <= 32; ++shift) {
            const XDWORD mask = (0xffffffffu >> (32 - width)) << shift;
            EXPECT_EQ(XDWORD(width), GetBitCount(mask)) << "mask " << mask;
            EXPECT_EQ(XDWORD(shift), GetBitShift(mask)) << "mask " << mask;
        }
    }
}

TEST(GraphicMaskReferenceTest, SparseMasksStopAtTheFirstGap) {
    // Literal original VxMath.dll 2.1.0.14 results, including full/empty masks.
    const XDWORD masks[] = {0x15, 0x1b, 0x31c, 0x80000003, 0x81000000, 0xf00ff000,
        0xff00ff00, 0x55555555, 0xaaaaaaaa, 0xffffffff, 0xfffffffe, 0x80000000,
        0, 0x80f0, 0x7fff0001, 0xc000000c};
    const XDWORD counts[] = {1, 2, 3, 2, 1, 8, 8, 1, 1, 32, 31, 1, 0, 4, 1, 2};
    const XDWORD shifts[] = {0, 0, 2, 0, 24, 12, 8, 0, 1, 0, 1, 31, 0, 4, 0, 2};
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(counts[i], GetBitCount(masks[i])) << "mask " << masks[i];
        EXPECT_EQ(shifts[i], GetBitShift(masks[i])) << "mask " << masks[i];
    }
}

TEST(GraphicMaskReferenceTest, ShiftUsesTheLowestSetBitAndDefinesZero) {
    EXPECT_EQ(0u, GetBitCount(0));
    EXPECT_EQ(0u, GetBitShift(0));
    for (int shift = 0; shift < 32; ++shift) {
        EXPECT_EQ(XDWORD(shift), GetBitShift((1u << shift) | 0x80000000u));
    }
}

TEST(GraphicMaskReferenceTest, ComponentCountsUseRgbaOrderWithoutChangingTheDescriptor) {
    XBYTE pixels[8] = {1, 2, 3, 4}, palette[8] = {5, 6, 7, 8};
    VxImageDescEx desc = MaskDescriptor(pixels, palette), before;
    std::memcpy(&before, &desc, sizeof(desc));
    XDWORD red = 99, green = 99, blue = 99, alpha = 99;
    VxGetBitCounts(desc, red, green, blue, alpha);
    EXPECT_EQ(5u, red);
    EXPECT_EQ(3u, green);
    EXPECT_EQ(1u, blue);
    EXPECT_EQ(1u, alpha);
    EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
}

TEST(GraphicMaskReferenceTest, ComponentShiftsUseRgbaOrderWithoutChangingTheDescriptor) {
    VxImageDescEx desc = MaskDescriptor(NULL, NULL), before;
    std::memcpy(&before, &desc, sizeof(desc));
    XDWORD red = 99, green = 99, blue = 99, alpha = 99;
    VxGetBitShifts(desc, red, green, blue, alpha);
    EXPECT_EQ(11u, red);
    EXPECT_EQ(2u, green);
    EXPECT_EQ(1u, blue);
    EXPECT_EQ(24u, alpha);
    EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
}

TEST(GraphicMaskReferenceTest, SharedOutputReferencesKeepTheNativeAlphaRedGreenBlueWriteOrder) {
    VxImageDescEx desc = MaskDescriptor(NULL, NULL);
    XDWORD result = 99;
    VxGetBitCounts(desc, result, result, result, result);
    EXPECT_EQ(1u, result);
    VxGetBitShifts(desc, result, result, result, result);
    EXPECT_EQ(1u, result);
}

TEST(GraphicMaskReferenceTest, OutputReferencesCanAliasDescriptorMasksInNativeOrder) {
    VxImageDescEx desc;
    desc.RedMask = 0xf800;
    desc.GreenMask = 0x7e0;
    desc.BlueMask = 0x1f;
    desc.AlphaMask = 0xff00;
    XDWORD green = 99, blue = 99;
    VxGetBitCounts(desc, desc.AlphaMask, green, blue, desc.RedMask);
    EXPECT_EQ(8u, desc.RedMask);
    EXPECT_EQ(1u, desc.AlphaMask);
    EXPECT_EQ(6u, green);
    EXPECT_EQ(5u, blue);
    desc.RedMask = 0xf800;
    desc.AlphaMask = 0xff00;
    VxGetBitShifts(desc, desc.AlphaMask, green, blue, desc.RedMask);
    EXPECT_EQ(8u, desc.RedMask);
    EXPECT_EQ(3u, desc.AlphaMask);
    EXPECT_EQ(5u, green);
    EXPECT_EQ(0u, blue);
}

TEST(GraphicMaskReferenceTest, Bpp15And16Choose555AndReplaceOnlyFourMasks) {
    for (int bits = 15; bits <= 16; ++bits) {
        VxImageDescEx desc = MaskDescriptor(NULL, NULL), expected;
        desc.BitsPerPixel = bits;
        std::memcpy(&expected, &desc, sizeof(desc));
        expected.RedMask = 0x7c00;
        expected.GreenMask = 0x3e0;
        expected.BlueMask = 0x1f;
        expected.AlphaMask = 0;
        VxBppToMask(desc);
        EXPECT_EQ(0, std::memcmp(&desc, &expected, sizeof(desc)));
    }
}

TEST(GraphicMaskReferenceTest, Bpp24And32ReplaceOnlyFourMasks) {
    for (int bits = 24; bits <= 32; bits += 8) {
        VxImageDescEx desc = MaskDescriptor(NULL, NULL), expected;
        desc.BitsPerPixel = bits;
        std::memcpy(&expected, &desc, sizeof(desc));
        expected.RedMask = 0xff0000;
        expected.GreenMask = 0xff00;
        expected.BlueMask = 0xff;
        expected.AlphaMask = bits == 32 ? 0xff000000 : 0;
        VxBppToMask(desc);
        EXPECT_EQ(0, std::memcmp(&desc, &expected, sizeof(desc)));
    }
}

TEST(GraphicMaskReferenceTest, Indexed8PreservesMasksPaletteAndPixelStorage) {
    XBYTE pixels[8] = {0, 1, 2, 3, 4, 5, 6, 7}, palette[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    XBYTE beforePixels[8], beforePalette[8];
    std::memcpy(beforePixels, pixels, sizeof(pixels));
    std::memcpy(beforePalette, palette, sizeof(palette));
    VxImageDescEx desc = MaskDescriptor(pixels, palette), before;
    desc.BitsPerPixel = 8;
    std::memcpy(&before, &desc, sizeof(desc));
    VxBppToMask(desc);
    EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
    EXPECT_EQ(0, std::memcmp(pixels, beforePixels, sizeof(pixels)));
    EXPECT_EQ(0, std::memcmp(palette, beforePalette, sizeof(palette)));
    desc.RedMask = desc.GreenMask = desc.BlueMask = desc.AlphaMask = 0;
    std::memcpy(&before, &desc, sizeof(desc));
    VxBppToMask(desc);
    EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
}

TEST(GraphicMaskReferenceTest, OtherDepthsLeaveTheWholeDescriptorUntouched) {
    const int bits[] = {-1, 0, 1, 4, 12, 64};
    for (int i = 0; i < 6; ++i) {
        VxImageDescEx desc = MaskDescriptor(NULL, NULL), before;
        desc.BitsPerPixel = bits[i];
        std::memcpy(&before, &desc, sizeof(desc));
        VxBppToMask(desc);
        EXPECT_EQ(0, std::memcmp(&desc, &before, sizeof(desc)));
    }
}

TEST(GraphicMaskReferenceTest, DepthChangesDoNotInventAnEightBitDirectColorFormat) {
    VxImageDescEx desc;
    const int bits[] = {15, 8, 32, 8, 24, 16};
    const XDWORD expected[][4] = {
        {0x7c00, 0x3e0, 0x1f, 0}, {0x7c00, 0x3e0, 0x1f, 0},
        {0xff0000, 0xff00, 0xff, 0xff000000}, {0xff0000, 0xff00, 0xff, 0xff000000},
        {0xff0000, 0xff00, 0xff, 0}, {0x7c00, 0x3e0, 0x1f, 0}
    };
    for (int i = 0; i < 6; ++i) {
        desc.BitsPerPixel = bits[i];
        VxBppToMask(desc);
        EXPECT_EQ(expected[i][0], desc.RedMask);
        EXPECT_EQ(expected[i][1], desc.GreenMask);
        EXPECT_EQ(expected[i][2], desc.BlueMask);
        EXPECT_EQ(expected[i][3], desc.AlphaMask);
    }
}
