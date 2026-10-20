#include "VxMath.h"
#include "VxSIMD.h"
#include <cstring>
#include <gtest/gtest.h>

static VxImageDescEx GraphicImageDesc(int width, int height, int pitch, int bpp, XBYTE *pixels) {
    VxImageDescEx d;
    d.Width = width;
    d.Height = height;
    d.BytesPerLine = pitch;
    d.BitsPerPixel = bpp;
    d.Image = pixels;
    d.RedMask = bpp == 16 ? 0xf800u : 0xff0000u;
    d.GreenMask = bpp == 16 ? 0x07e0u : 0xff00u;
    d.BlueMask = bpp == 16 ? 0x001fu : 0xffu;
    d.AlphaMask = bpp == 32 ? 0xff000000u : 0;
    return d;
}

class GraphicImageReferenceTest : public ::testing::TestWithParam<int> {
  protected:
    int PreviousMode;
    void SetUp() override {
        PreviousMode = VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(GetParam()));
    }
    void TearDown() override {
        EXPECT_TRUE(VxSetSIMDOverride(PreviousMode));
    }
    void Guard(const XBYTE *data, int begin, int end) {
        for (int i = begin; i < end; ++i)
            EXPECT_EQ(0xcdu, data[i]) << "guard byte " << i;
    }
    void CheckHorizontal(bool blit) {
        XDWORD source[] = {0xff000000u, 0xffffffffu, 0xff000000u, 0xffffffffu};
        XBYTE output[40];
        std::memset(output, 0xcd, sizeof(output));
        VxImageDescEx s = GraphicImageDesc(4, 1, 16, 32, (XBYTE *)source),
                      d = GraphicImageDesc(2, 1, 12, 32, output + 8);
        if (blit)
            VxDoBlit(s, d);
        else
            VxResizeImage32(s, d);
        const XDWORD expected[] = {0xff7f7f7fu, 0xff7f7f7fu};
        EXPECT_EQ(0, std::memcmp(expected, output + 8, sizeof(expected)));
        Guard(output, 0, 8);
        Guard(output, 16, 40);
    }
    void CheckVertical(bool blit) {
        XDWORD source[] = {0xff000000u, 0xeeeeeeeeu, 0xffffffffu, 0xeeeeeeeeu,
                           0xff000000u, 0xeeeeeeeeu, 0xffffffffu, 0xeeeeeeeeu};
        XBYTE output[48];
        std::memset(output, 0xcd, sizeof(output));
        VxImageDescEx s = GraphicImageDesc(1, 4, 8, 32, (XBYTE *)source),
                      d = GraphicImageDesc(1, 2, 12, 32, output + 8);
        if (blit)
            VxDoBlit(s, d);
        else
            VxResizeImage32(s, d);
        EXPECT_EQ(0xff7f7f7fu, *(XDWORD *)(output + 8));
        EXPECT_EQ(0xff7f7f7fu, *(XDWORD *)(output + 20));
        Guard(output, 0, 8);
        Guard(output, 12, 20);
        Guard(output, 24, 48);
    }
};

TEST_P(GraphicImageReferenceTest, SameSizeBlitPreservesPixelsAndPitches) {
    XDWORD source[] = {0x40102030u, 0x80506070u, 0xeeeeeeeeu, 0xa090a0b0u, 0xffc0d0e0u, 0xeeeeeeeeu};
    XBYTE output[48];
    std::memset(output, 0xcd, sizeof(output));
    VxImageDescEx s = GraphicImageDesc(2, 2, 12, 32, (XBYTE *)source), d = GraphicImageDesc(2, 2, 16, 32, output + 8);
    VxImageDescEx savedSource = s, savedDestination = d;
    VxDoBlit(s, d);
    EXPECT_EQ(0, std::memcmp(source, output + 8, 8));
    EXPECT_EQ(0, std::memcmp(source + 3, output + 24, 8));
    EXPECT_EQ(0, std::memcmp(&s, &savedSource, sizeof(s)));
    EXPECT_EQ(0, std::memcmp(&d, &savedDestination, sizeof(d)));
    Guard(output, 0, 8);
    Guard(output, 16, 24);
    Guard(output, 32, 48);
}
TEST_P(GraphicImageReferenceTest, UpsideDownBlitReversesRowsWithoutCopyingPadding) {
    XDWORD source[] = {0xff010203u, 0xff040506u, 0xeeeeeeeeu, 0xff070809u, 0xff0a0b0cu, 0xeeeeeeeeu};
    XBYTE output[48];
    std::memset(output, 0xcd, sizeof(output));
    VxImageDescEx s = GraphicImageDesc(2, 2, 12, 32, (XBYTE *)source), d = GraphicImageDesc(2, 2, 16, 32, output + 8);
    VxDoBlitUpsideDown(s, d);
    EXPECT_EQ(0, std::memcmp(source + 3, output + 8, 8));
    EXPECT_EQ(0, std::memcmp(source, output + 24, 8));
    Guard(output, 0, 8);
    Guard(output, 16, 24);
    Guard(output, 32, 48);
}
TEST_P(GraphicImageReferenceTest, UpsideDownSizeMismatchLeavesDestinationUntouched) {
    XDWORD source[4] = {};
    XBYTE output[24];
    std::memset(output, 0xcd, sizeof(output));
    VxImageDescEx s = GraphicImageDesc(2, 2, 8, 32, (XBYTE *)source), d = GraphicImageDesc(1, 2, 4, 32, output + 4);
    VxDoBlitUpsideDown(s, d);
    Guard(output, 0, 24);
}
TEST_P(GraphicImageReferenceTest, Blit24To32ConvertsPixelsAndRowOrder) {
    XBYTE source[] = {3, 2, 1, 6, 5, 4, 0xee, 0xee, 9, 8, 7, 12, 11, 10, 0xee, 0xee};
    XDWORD output[4] = {};
    VxImageDescEx s = GraphicImageDesc(2, 2, 8, 24, source), d = GraphicImageDesc(2, 2, 8, 32, (XBYTE *)output);
    VxDoBlit(s, d);
    const XDWORD forward[] = {0xff010203u, 0xff040506u, 0xff070809u, 0xff0a0b0cu};
    EXPECT_EQ(0, std::memcmp(output, forward, sizeof(output)));
    VxDoBlitUpsideDown(s, d);
    const XDWORD reversed[] = {0xff070809u, 0xff0a0b0cu, 0xff010203u, 0xff040506u};
    EXPECT_EQ(0, std::memcmp(output, reversed, sizeof(output)));
}
TEST_P(GraphicImageReferenceTest, ResizeHorizontalReductionAveragesCoveredPixels) {
    CheckHorizontal(false);
}
TEST_P(GraphicImageReferenceTest, BlitHorizontalReductionAveragesCoveredPixels) {
    CheckHorizontal(true);
}
TEST_P(GraphicImageReferenceTest, ResizeVerticalReductionAveragesCoveredRows) {
    CheckVertical(false);
}
TEST_P(GraphicImageReferenceTest, BlitVerticalReductionAveragesCoveredRows) {
    CheckVertical(true);
}
TEST_P(GraphicImageReferenceTest, BothAxesReductionKeepsCheckerboardEnergyAndAlpha) {
    XDWORD source[16];
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            source[y * 4 + x] = (x + y) % 2 ? 0xffffffffu : 0;
    XDWORD output[4] = {};
    VxImageDescEx s = GraphicImageDesc(4, 4, 16, 32, (XBYTE *)source),
                  d = GraphicImageDesc(2, 2, 8, 32, (XBYTE *)output);
    VxResizeImage32(s, d);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(0x7f7f7f7fu, output[i]);
    std::memset(output, 0, sizeof(output));
    VxDoBlit(s, d);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(0x7f7f7f7fu, output[i]);
}
TEST_P(GraphicImageReferenceTest, FractionalReductionUsesTheWholeFootprint) {
    XDWORD source[] = {0xff000000u, 0xff000000u, 0xffffffffu};
    XDWORD output[2] = {};
    VxImageDescEx s = GraphicImageDesc(3, 1, 12, 32, (XBYTE *)source),
                  d = GraphicImageDesc(2, 1, 8, 32, (XBYTE *)output);
    VxResizeImage32(s, d);
    EXPECT_EQ(0xff000000u, output[0]);
    EXPECT_EQ(0xffaaaaaau, output[1]);
    VxDoBlit(s, d);
    EXPECT_EQ(0xff000000u, output[0]);
    EXPECT_EQ(0xffaaaaaau, output[1]);
}
TEST_P(GraphicImageReferenceTest, BlitVerticalEnlargementInterpolatesAndClampsLastRow) {
    XDWORD source[] = {0xff000000u, 0xff646464u};
    XDWORD output[4] = {};
    VxImageDescEx s = GraphicImageDesc(1, 2, 4, 32, (XBYTE *)source),
                  d = GraphicImageDesc(1, 4, 4, 32, (XBYTE *)output);
    VxDoBlit(s, d);
    const XDWORD expected[] = {0xff000000u, 0xff323232u, 0xff646464u, 0xff646464u};
    EXPECT_EQ(0, std::memcmp(expected, output, sizeof(output)));
}
TEST_P(GraphicImageReferenceTest, MixedShrinkAndGrowFiltersBothAxes) {
    XDWORD source[] = {0xff000000u, 0xffffffffu, 0xff000000u, 0xffffffffu,
                       0xff646464u, 0xff646464u, 0xff646464u, 0xff646464u};
    XDWORD output[8] = {};
    VxImageDescEx s = GraphicImageDesc(4, 2, 16, 32, (XBYTE *)source),
                  d = GraphicImageDesc(2, 4, 8, 32, (XBYTE *)output);
    const XDWORD expected[] = {0xff7f7f7fu, 0xff7f7f7fu, 0xff717171u, 0xff717171u,
                               0xff646464u, 0xff646464u, 0xff646464u, 0xff646464u};
    VxResizeImage32(s, d);
    EXPECT_EQ(0, std::memcmp(expected, output, sizeof(output)));
    VxDoBlit(s, d);
    EXPECT_EQ(0, std::memcmp(expected, output, sizeof(output)));
}
TEST_P(GraphicImageReferenceTest, ResizedBlitConvertsFiltered32BitRowsTo24Bit) {
    XDWORD source[] = {0xff000000u, 0xffffffffu, 0xff000000u, 0xffffffffu};
    XBYTE output[32];
    std::memset(output, 0xcd, sizeof(output));
    VxImageDescEx s = GraphicImageDesc(2, 2, 8, 32, (XBYTE *)source), d = GraphicImageDesc(1, 1, 4, 24, output + 8);
    VxDoBlit(s, d);
    EXPECT_EQ(127, output[8]);
    EXPECT_EQ(127, output[9]);
    EXPECT_EQ(127, output[10]);
    Guard(output, 0, 8);
    Guard(output, 11, 32);
}
INSTANTIATE_TEST_SUITE_P(ScalarAndSSE2, GraphicImageReferenceTest,
                         ::testing::Values(VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2));
