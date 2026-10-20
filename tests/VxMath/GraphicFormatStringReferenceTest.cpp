#include <gtest/gtest.h>

#include <climits>
#include "VxMath.h"

// VxMath.dll 2.1.0.14, export 0x2428FCF0 / lookup 0x242924C0.
// Literal original export results, including the RGB/BGR trailing spaces.
static const char *const OriginalFormatNames[] = {
    "",
    "32 bits ARGB 8888",
    "32 bits RGB  888 ",
    "24 bits RGB  888 ",
    "16 bits RGB  565 ",
    "16 bits RGB  555 ",
    "16 bits ARGB 1555",
    "16 bits ARGB 4444",
    "8 bits RGB  332 ",
    "8 bits ARGB 2222",
    "32 bits ABGR 8888",
    "32 bits RGBA 8888",
    "32 bits BGRA 8888",
    "32 bits BGR  888 ",
    "24 bits BGR  888 ",
    "16 bits BGR  565 ",
    "16 bits BGR  555 ",
    "16 bits ABGR 1555",
    "16 bits ABGR 4444",
    "Compressed DXT1",
    "Compressed DXT2",
    "Compressed DXT3",
    "Compressed DXT4",
    "Compressed DXT5",
    "V8U8 BumpMap",
    "V16U16 BumpMap",
    "L6V5U5 BumpMap",
    "L8V8U8 BumpMap"
};

class GraphicFormatStringReferenceTest : public testing::TestWithParam<int> {};

TEST_P(GraphicFormatStringReferenceTest, MatchesOriginalBytes) {
    const int format = GetParam();
    const char *name = VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(format));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, OriginalFormatNames[format]);
}

INSTANTIATE_TEST_SUITE_P(OriginalFormats, GraphicFormatStringReferenceTest, testing::Range(1, 28));

TEST(GraphicFormatStringTest, UnknownReturnsEmptyInsteadOfTableLabel) {
    const char *name = VxPixelFormat2String(UNKNOWN_PF);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "");
}

TEST(GraphicFormatStringTest, UpperBoundaryAndLargeValuesReturnEmpty) {
    const int invalid[] = {28, 29, 31, 999, INT_MAX};
    for (int i = 0; i < 5; ++i) {
        SCOPED_TRACE(invalid[i]);
        const char *name = VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(invalid[i]));
        ASSERT_NE(name, nullptr);
        EXPECT_STREQ(name, "");
    }
}

TEST(GraphicFormatStringTest, NegativeValuesReturnEmptyWithoutReadingBeforeTable) {
    // Correctness difference: the original signed upper bound admits negatives.
    const int invalid[] = {-1, -28, -999, INT_MIN};
    for (int i = 0; i < 4; ++i) {
        SCOPED_TRACE(invalid[i]);
        const char *name = VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(invalid[i]));
        ASSERT_NE(name, nullptr);
        EXPECT_STREQ(name, "");
    }
}

TEST(GraphicFormatStringTest, ReturnedNamesRemainValidAcrossOtherLookups) {
    const char *names[28];
    for (int format = 0; format < 28; ++format) {
        names[format] = VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(format));
        ASSERT_NE(names[format], nullptr);
    }
    for (int format = 27; format >= 0; --format) {
        SCOPED_TRACE(format);
        EXPECT_STREQ(VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(999)), "");
        EXPECT_EQ(names[format], VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(format)));
        EXPECT_STREQ(names[format], OriginalFormatNames[format]);
    }
}
