#include "VxMath.h"
#include "VxSIMD.h"
#include <cstring>
#include <gtest/gtest.h>

static VxImageDescEx MapImage(int width, int height, int pitch, XBYTE *image, int bits = 32) {
    VxImageDescEx desc;
    desc.Width = width;
    desc.Height = height;
    desc.BytesPerLine = pitch;
    desc.BitsPerPixel = bits;
    desc.Image = image;
    return desc;
}

static const XBYTE BumpHeights[12] = {0, 1, 10, 50, 70, 100, 130, 160, 190, 220, 250, 255};
// Literal results from original VxMath.dll 2.1.0.14, not a second implementation.
static const XDWORD NativeBumpGrid[12] = {
    0x0031787f, 0x00f6787f, 0x00cf783f, 0x00325f3f,
    0x003c423f, 0x00c4253f, 0x00c4103f, 0x005a333f,
    0x0023463f, 0x00c4633f, 0x00dd783f, 0x00416e3f
};

class GraphicNormalBumpReferenceTest : public ::testing::TestWithParam<int> {
protected:
    int PreviousMode;
    void SetUp() override {
        PreviousMode = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(GetParam()));
    }
    void TearDown() override {
        EXPECT_TRUE(VxSetSIMDOverride(PreviousMode));
    }
    void Guard(const XBYTE *bytes, int begin, int end, XBYTE value = 0xa5) {
        for (int i = begin; i < end; ++i) EXPECT_EQ(value, bytes[i]) << "byte " << i;
    }
    void Store(XBYTE *image, XDWORD value) {
        std::memcpy(image, &value, 4);
    }
    XDWORD Pixel(const XBYTE *image) {
        XDWORD value;
        std::memcpy(&value, image, 4);
        return value;
    }
};

TEST_P(GraphicNormalBumpReferenceTest, NormalFlatMaskedHeightEncodesHeightAndKeepsDescriptor) {
    XDWORD pixels[6];
    for (int i = 0; i < 6; ++i) pixels[i] = 0x91171180;
    VxImageDescEx desc = MapImage(3, 2, 12, (XBYTE *)pixels), before = desc;
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xff));
    for (int i = 0; i < 6; ++i) EXPECT_EQ(0x808080ffu, pixels[i]);
    EXPECT_EQ(0, std::memcmp(&before, &desc, sizeof(desc)));
}

TEST_P(GraphicNormalBumpReferenceTest, NormalMaskedHorizontalSlopeClampsTheRightEdge) {
    XDWORD pixels[9];
    for (int y = 0; y < 3; ++y) {
        pixels[y * 3] = 0x91231700;
        pixels[y * 3 + 1] = pixels[y * 3 + 2] = 0x815432ff;
    }
    VxImageDescEx desc = MapImage(3, 3, 12, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xff));
    const XDWORD expected[] = {0x00d980d9, 0xff8080ff, 0xff8080ff};
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) EXPECT_EQ(expected[x], pixels[y * 3 + x]);
    }
}

TEST_P(GraphicNormalBumpReferenceTest, NormalLuminanceEncodesPositiveAndNegativeVerticalSlopes) {
    XDWORD pixels[] = {0x99000000, 0xff000000, 0x00ffffff, 0x8fffffff, 0x12000000, 0x24000000};
    VxImageDescEx desc = MapImage(2, 3, 8, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xffffffff));
    const XDWORD expected[] = {0x0080d9d9, 0x0080d9d9, 0xff8026d9, 0xff8026d9, 0xff8026d9, 0xff8026d9};
    EXPECT_EQ(0, std::memcmp(expected, pixels, sizeof(pixels)));
}

TEST_P(GraphicNormalBumpReferenceTest, NormalLuminanceUsesTheRgbMeanAndIgnoresInputAlpha) {
    XDWORD pixels[15];
    const XDWORD expected[] = {0x00a880f8, 0x558080ff, 0x558080ff, 0x558080ff, 0x558080ff};
    for (int channel = 0; channel < 3; ++channel) {
        for (int y = 0; y < 3; ++y) {
            pixels[y * 5] = XDWORD(y * 91) << 24;
            for (int x = 1; x < 5; ++x) {
                pixels[y * 5 + x] = (XDWORD(y * 73 + x) << 24) | (0xffu << (channel * 8));
            }
        }
        VxImageDescEx desc = MapImage(5, 3, 20, (XBYTE *)pixels);
        ASSERT_TRUE(VxConvertToNormalMap(desc, 0xffffffff));
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 5; ++x) EXPECT_EQ(expected[x], pixels[y * 5 + x]);
        }
    }
}

TEST_P(GraphicNormalBumpReferenceTest, NormalPaddedRowsKeepTheirOwnPaddingWhenTheLastRowIsDuplicated) {
    XBYTE storage[112];
    std::memset(storage, 0xa5, sizeof(storage));
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 5; ++x) Store(storage + 8 + y * 28 + x * 4, x ? 0xffffffff : 0x99000000);
        std::memset(storage + 8 + y * 28 + 20, 0x31 + y, 8);
    }
    VxImageDescEx desc = MapImage(5, 3, 28, storage + 8);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xffffffff));
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 5; ++x) EXPECT_EQ(x ? 0xff8080ffu : 0x00d980d9u, Pixel(storage + 8 + y * 28 + x * 4));
        Guard(storage, 8 + y * 28 + 20, 8 + (y + 1) * 28, XBYTE(0x31 + y));
    }
    Guard(storage, 0, 8);
    Guard(storage, 92, 112);
}

TEST_P(GraphicNormalBumpReferenceTest, NormalSingleColumnHasNoHorizontalGradient) {
    XDWORD pixels[] = {0, 0xffffffff, 0xffffffff};
    VxImageDescEx desc = MapImage(1, 3, 4, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xff));
    const XDWORD expected[] = {0x0080d9d9, 0xff8080ff, 0xff8080ff};
    EXPECT_EQ(0, std::memcmp(expected, pixels, sizeof(pixels)));
}

TEST_P(GraphicNormalBumpReferenceTest, NormalSingleRowLeavesItsPixelsAndGuardsUntouched) {
    XBYTE storage[48], before[48];
    std::memset(storage, 0xa5, sizeof(storage));
    Store(storage + 16, 0x91231700);
    Store(storage + 20, 0x12345678);
    Store(storage + 24, 0x815432ff);
    std::memcpy(before, storage, sizeof(storage));
    VxImageDescEx desc = MapImage(3, 1, 16, storage + 16);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xffffffff));
    EXPECT_EQ(0, std::memcmp(before, storage, sizeof(storage)));
}

TEST_P(GraphicNormalBumpReferenceTest, NormalZeroMaskWritesAFlatZeroHeightNormal) {
    XDWORD pixels[] = {0x12345678, 0x87654321, 0x00ffffff, 0xffffffff};
    VxImageDescEx desc = MapImage(2, 2, 8, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0));
    for (int i = 0; i < 4; ++i) EXPECT_EQ(0x008080ffu, pixels[i]);
}

TEST_P(GraphicNormalBumpReferenceTest, BumpBlueHeightGridMatchesNativeEdgesAndVerticalWrap) {
    XDWORD pixels[12];
    for (int i = 0; i < 12; ++i) pixels[i] = 0x9b000000 | BumpHeights[i];
    VxImageDescEx desc = MapImage(4, 3, 16, (XBYTE *)pixels), before = desc;
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    for (int i = 0; i < 12; ++i) EXPECT_EQ(NativeBumpGrid[i], pixels[i]) << "pixel " << i;
    EXPECT_EQ(0, std::memcmp(&before, &desc, sizeof(desc)));
}

TEST_P(GraphicNormalBumpReferenceTest, BumpIgnoresOtherChannelsAndPreservesRowPadding) {
    XBYTE storage[104];
    std::memset(storage, 0xa5, sizeof(storage));
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const XDWORD value = (XDWORD(7 + x * 47) << 24) | (XDWORD(201 - y * 31) << 16)
                | (XDWORD(13 + x * 7) << 8) | BumpHeights[y * 4 + x];
            Store(storage + 8 + y * 24 + x * 4, value);
        }
        std::memset(storage + 8 + y * 24 + 16, 0x31 + y, 8);
    }
    VxImageDescEx desc = MapImage(4, 3, 24, storage + 8);
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) EXPECT_EQ(NativeBumpGrid[y * 4 + x], Pixel(storage + 8 + y * 24 + x * 4));
        Guard(storage, 8 + y * 24 + 16, 8 + (y + 1) * 24, XBYTE(0x31 + y));
    }
    Guard(storage, 0, 8);
    Guard(storage, 80, 104);
}

TEST_P(GraphicNormalBumpReferenceTest, BumpTwoPixelRowsKeepTheNativeEndpointRule) {
    XDWORD pixels[] = {3, 13, 23, 33};
    VxImageDescEx desc = MapImage(2, 2, 8, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    const XDWORD expected[] = {0x0000003f, 0x000a003f, 0x0000003f, 0x000a003f};
    EXPECT_EQ(0, std::memcmp(expected, pixels, sizeof(pixels)));
}

TEST_P(GraphicNormalBumpReferenceTest, BumpSingleRowUsesItsOwnVerticalNeighbors) {
    XDWORD pixels[] = {0, 1, 10, 50};
    VxImageDescEx desc = MapImage(4, 1, 16, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    const XDWORD expected[] = {0x0031007f, 0x00f6007f, 0x00cf003f, 0x0032003f};
    EXPECT_EQ(0, std::memcmp(expected, pixels, sizeof(pixels)));
}

TEST_P(GraphicNormalBumpReferenceTest, BumpBlueZeroAndOneUseTheNativeBrightnessClass) {
    XDWORD pixels[] = {0x00ffff00, 0x00ffff01, 0x00ffff02, 0x00ffffff,
                       0x00ffff00, 0x00ffff01, 0x00ffff02, 0x00ffffff};
    VxImageDescEx desc = MapImage(4, 2, 16, (XBYTE *)pixels);
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    const XDWORD expected[] = {0x00fe007f, 0x00fe007f, 0x0002003f, 0x00ff003f};
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) EXPECT_EQ(expected[x], pixels[y * 4 + x]);
    }
}

TEST_P(GraphicNormalBumpReferenceTest, UnsupportedDepthsAndBumpSingleColumnDoNotPartiallyWrite) {
    XBYTE storage[64];
    std::memset(storage, 0xa5, sizeof(storage));
    const int depths[] = {8, 16};
    for (int i = 0; i < 2; ++i) {
        VxImageDescEx desc = MapImage(4, 2, 16, storage + 8, depths[i]);
        EXPECT_FALSE(VxConvertToNormalMap(desc, 0xff));
        EXPECT_FALSE(VxConvertToBumpMap(desc));
        Guard(storage, 0, 64);
    }
    VxImageDescEx desc = MapImage(1, 3, 4, storage + 8);
    EXPECT_FALSE(VxConvertToBumpMap(desc));
    Guard(storage, 0, 64);
}

TEST_P(GraphicNormalBumpReferenceTest, Existing24BitExtensionKeepsFlatOutputAndPadding) {
    XBYTE storage[40];
    std::memset(storage, 0xa5, sizeof(storage));
    std::memset(storage + 8, 128, 6);
    std::memset(storage + 16, 128, 6);
    std::memset(storage + 22, 0x33, 2);
    VxImageDescEx desc = MapImage(2, 2, 8, storage + 8, 24);
    ASSERT_TRUE(VxConvertToNormalMap(desc, 0xffffffff));
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            EXPECT_EQ(255, storage[8 + y * 8 + x * 3]);
            EXPECT_EQ(128, storage[9 + y * 8 + x * 3]);
            EXPECT_EQ(128, storage[10 + y * 8 + x * 3]);
        }
    }
    Guard(storage, 14, 16);
    Guard(storage, 22, 24, 0x33);
    std::memset(storage + 8, 0, 6);
    std::memset(storage + 16, 0, 6);
    ASSERT_TRUE(VxConvertToBumpMap(desc));
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            EXPECT_EQ(127, storage[8 + y * 8 + x * 3]);
            EXPECT_EQ(0, storage[9 + y * 8 + x * 3]);
            EXPECT_EQ(0, storage[10 + y * 8 + x * 3]);
        }
    }
    Guard(storage, 0, 8);
    Guard(storage, 14, 16);
    Guard(storage, 22, 24, 0x33);
    Guard(storage, 24, 40);
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2, GraphicNormalBumpReferenceTest,
                         ::testing::Values(VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_AVX2));
