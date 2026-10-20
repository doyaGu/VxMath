#include <gtest/gtest.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "VxMath.h"
#include "VxBlitEngine.h"

struct QuantizeReferenceImage {
    XBYTE source[136 * 32 + 16];
    XBYTE indices[40 * 32 + 16];
    XBYTE palette[256 * 8 + 16];
    VxImageDescEx src, dst;
    int pattern;

    QuantizeReferenceImage(int depth = 32, int width = 32, int height = 32,
                          int entryBytes = 4, int imagePattern = 1,
                          bool reverseSource = false, bool reverseDestination = false)
        : pattern(imagePattern) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(indices, 0xa5, sizeof(indices));
        std::memset(palette, 0xa5, sizeof(palette));
        int bytes = depth / 8, sourcePitch = width * bytes + 8, targetPitch = width + 3;
        src.Width = dst.Width = width;
        src.Height = dst.Height = height;
        src.BitsPerPixel = depth;
        src.BytesPerLine = reverseSource ? -sourcePitch : sourcePitch;
        src.Image = source + 8 + (reverseSource ? (height - 1) * sourcePitch : 0);
        src.RedMask = 0xff0000; src.GreenMask = 0xff00; src.BlueMask = 0xff;
        src.AlphaMask = bytes == 4 ? 0xff000000 : 0;
        dst.BitsPerPixel = 8;
        dst.BytesPerLine = reverseDestination ? -targetPitch : targetPitch;
        dst.Image = indices + 8 + (reverseDestination ? (height - 1) * targetPitch : 0);
        dst.ColorMapEntries = 256; dst.BytesPerColorEntry = static_cast<short>(entryBytes);
        dst.ColorMap = palette + 8;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            XBYTE *p = src.Image + y * src.BytesPerLine + x * bytes;
            int r, g, b; Color(x, y, r, g, b);
            p[0] = static_cast<XBYTE>(b); p[1] = static_cast<XBYTE>(g); p[2] = static_cast<XBYTE>(r);
            if (bytes == 4) p[3] = static_cast<XBYTE>(x + y);
        }
    }

    void Color(int x, int y, int &r, int &g, int &b) const {
        if (pattern == 0) r = g = b = (17 * x + 31 * y + (x * y) % 19) & 255;
        else if (pattern == 1) { r = (17 * x + 3 * y) & 255; g = (5 * x + 29 * y) & 255; b = (11 * x + 7 * y) & 255; }
        else if (pattern == 2) { r = 235; g = 25; b = 75; }
        else {
            static const int colors[4][3] = {{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
            int i = (x + y * src.Width) % 4;
            r = colors[i][0]; g = colors[i][1]; b = colors[i][2];
        }
    }

    static int Abs(int n) { return n < 0 ? -n : n; }

    void CheckPixels(bool upsideDown = false, int maximumError = INT_MAX) const {
        for (int y = 0; y < dst.Height; ++y) for (int x = 0; x < dst.Width; ++x) {
            int r, g, b; Color(x, upsideDown ? src.Height - 1 - y : y, r, g, b);
            int index = dst.Image[y * dst.BytesPerLine + x];
            const XBYTE *actual = dst.ColorMap + index * dst.BytesPerColorEntry;
            int distance = Abs(r - actual[2]) + Abs(g - actual[1]) + Abs(b - actual[0]);
            int closest = INT_MAX;
            for (int i = 0; i < 256; ++i) {
                const XBYTE *p = dst.ColorMap + i * dst.BytesPerColorEntry;
                int d = Abs(r - p[2]) + Abs(g - p[1]) + Abs(b - p[0]);
                if (d < closest) closest = d;
            }
            EXPECT_EQ(distance, closest) << "pixel " << x << "," << y;
            EXPECT_LE(distance, maximumError) << "pixel " << x << "," << y;
        }
    }

    void CheckStorage() const {
        int pitch = Abs(dst.BytesPerLine), bytes = dst.BytesPerColorEntry;
        for (int y = 0; y < dst.Height; ++y) for (int x = dst.Width; x < pitch; ++x)
            EXPECT_EQ(indices[8 + y * pitch + x], 0xa5);
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(indices[i], 0xa5); EXPECT_EQ(indices[8 + pitch * dst.Height + i], 0xa5);
            EXPECT_EQ(palette[i], 0xa5); EXPECT_EQ(palette[8 + 256 * bytes + i], 0xa5);
        }
        for (int i = 0; i < 256; ++i) {
            if (bytes >= 4) EXPECT_EQ(dst.ColorMap[i * bytes + 3], 0xff);
            for (int c = 4; c < bytes; ++c) EXPECT_EQ(dst.ColorMap[i * bytes + c], 0xa5);
        }
    }

    void Run() {
        XBYTE before[sizeof(source)]; std::memcpy(before, source, sizeof(source));
        VxImageDescEx beforeSrc = src, beforeDst = dst;
        int factor = GetQuantizationSamplingFactor();
        ASSERT_TRUE(TheBlitter.QuantizeImage(src, dst));
        EXPECT_EQ(GetQuantizationSamplingFactor(), factor);
        EXPECT_EQ(std::memcmp(source, before, sizeof(source)), 0);
        EXPECT_EQ(std::memcmp(&src, &beforeSrc, sizeof(src)), 0);
        EXPECT_EQ(std::memcmp(&dst, &beforeDst, sizeof(dst)), 0);
        CheckStorage();
    }

    unsigned long long RGBPaletteHash() const {
        // Original private preparation writes R/G/B; the current image palette is B/G/R.
        unsigned long long hash = 14695981039346656037ULL;
        for (int i = 0; i < 256; ++i) for (int c = 2; c >= 0; --c)
            hash = (hash ^ dst.ColorMap[i * dst.BytesPerColorEntry + c]) * 1099511628211ULL;
        return hash;
    }
};

class QuantizeImageReferenceTest : public testing::Test {
protected:
    void SetUp() override { m_SavedFactor = GetQuantizationSamplingFactor(); }
    void TearDown() override { SetQuantizationSamplingFactor(m_SavedFactor); }
private:
    int m_SavedFactor;
};

class OriginalQuantizePaletteTest : public QuantizeImageReferenceTest,
                                    public testing::WithParamInterface<int> {};

TEST_P(OriginalQuantizePaletteTest, NativeTrainingAndIndependentNearestIndices) {
    static const int factors[] = {1,15,30}, entryBytes[] = {3,4,8};
    // VxMath.dll2.1.0.14 private stdcall preparation0x242917B0, full32x32 color image.
    static const unsigned long long originalRGB[] = {
        0xa2e2c48baadd474bULL, 0x0a141c9e32d8f951ULL, 0xb3107991b5ebf10bULL
    };
    int sample = GetParam(), factorIndex = sample % 3, paletteIndex = (sample / 3) % 3;
    SetQuantizationSamplingFactor(factors[factorIndex]);
    QuantizeReferenceImage image(sample < 9 ? 24 : 32, 32, 32, entryBytes[paletteIndex]);
    image.Run();
    EXPECT_EQ(image.RGBPaletteHash(), originalRGB[factorIndex]);
    image.CheckPixels();
}

INSTANTIATE_TEST_SUITE_P(OriginalLayouts, OriginalQuantizePaletteTest, testing::Range(0, 18));

class TinyQuantizeImageTest : public QuantizeImageReferenceTest,
                              public testing::WithParamInterface<int> {};

TEST_P(TinyQuantizeImageTest, TrainsHighChromaPixelsAtDefaultSampling) {
    static const int sizes[6][2] = {{1,1},{2,1},{1,2},{2,2},{4,3},{8,8}};
    int sample = GetParam(), geometry = sample % 6;
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage image(sample < 6 ? 24 : 32, sizes[geometry][0], sizes[geometry][1], 4, geometry == 0 ? 2 : 3);
    image.Run();
    image.CheckPixels(false, geometry == 0 ? 0 : 30);
}

INSTANTIATE_TEST_SUITE_P(SmallImages, TinyQuantizeImageTest, testing::Range(0, 12));

TEST_F(QuantizeImageReferenceTest, RejectsNonIndexedDestinationBeforeWriting) {
    const int depths[] = {1,4,16,24,32};
    for (int i = 0; i < 5; ++i) {
        SCOPED_TRACE(depths[i]);
        QuantizeReferenceImage image; image.dst.BitsPerPixel = depths[i];
        XBYTE beforePalette[sizeof(image.palette)], beforeIndices[sizeof(image.indices)];
        std::memcpy(beforePalette,image.palette,sizeof(beforePalette)); std::memcpy(beforeIndices,image.indices,sizeof(beforeIndices));
        EXPECT_FALSE(TheBlitter.QuantizeImage(image.src,image.dst));
        EXPECT_EQ(std::memcmp(beforePalette,image.palette,sizeof(beforePalette)),0);
        EXPECT_EQ(std::memcmp(beforeIndices,image.indices,sizeof(beforeIndices)),0);
    }
}

TEST_F(QuantizeImageReferenceTest, RejectsSourceDepthsOtherThan24Or32) {
    const int depths[] = {8,16,23,25,31,33,40};
    for (int i = 0; i < 7; ++i) {
        SCOPED_TRACE(depths[i]);
        QuantizeReferenceImage image; image.src.BitsPerPixel = depths[i];
        EXPECT_FALSE(TheBlitter.QuantizeImage(image.src,image.dst));
        for (unsigned j = 0; j < sizeof(image.palette); ++j) EXPECT_EQ(image.palette[j],0xa5);
        for (unsigned j = 0; j < sizeof(image.indices); ++j) EXPECT_EQ(image.indices[j],0xa5);
    }
}

TEST_F(QuantizeImageReferenceTest, ExistingFailurePathsLeaveOutputsUntouched) {
    for (int scenario = 0; scenario < 11; ++scenario) {
        SCOPED_TRACE(scenario); QuantizeReferenceImage image;
        switch (scenario) {
        case 0: image.dst.ColorMapEntries = 0; break;
        case 1: image.dst.ColorMapEntries = 255; break;
        case 2: image.dst.ColorMapEntries = 257; break;
        case 3: image.dst.ColorMap = nullptr; break;
        case 4: image.dst.BytesPerColorEntry = 2; break;
        case 5: image.src.Image = nullptr; break;
        case 6: image.dst.Image = nullptr; break;
        case 7: image.dst.Width = 31; break;
        case 8: image.dst.Height = 31; break;
        case 9: image.src.Width = image.dst.Width = 0; break;
        case 10: image.src.Height = image.dst.Height = -1; break;
        }
        EXPECT_FALSE(TheBlitter.QuantizeImage(image.src,image.dst));
        for (unsigned j = 0; j < sizeof(image.palette); ++j) EXPECT_EQ(image.palette[j],0xa5);
        for (unsigned j = 0; j < sizeof(image.indices); ++j) EXPECT_EQ(image.indices[j],0xa5);
    }
}

TEST_F(QuantizeImageReferenceTest, NegativeSourcePitchKeepsLogicalRowOrder) {
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage image(32,32,32,4,1,true,false);
    image.Run(); image.CheckPixels(); EXPECT_EQ(image.RGBPaletteHash(),0x0a141c9e32d8f951ULL);
}

TEST_F(QuantizeImageReferenceTest, NegativeDestinationPitchKeepsLogicalRowOrder) {
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage image(24,32,32,3,1,false,true);
    image.Run(); image.CheckPixels(); EXPECT_EQ(image.RGBPaletteHash(),0x0a141c9e32d8f951ULL);
}

TEST_F(QuantizeImageReferenceTest, PublicBlitCompletesTinyColorIndices) {
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage image(32,2,2,4,3);
    VxDoBlit(image.src,image.dst); image.CheckStorage(); image.CheckPixels(false,30);
}

TEST_F(QuantizeImageReferenceTest, PublicUpsideDownBlitCompletesTinyColorIndices) {
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage image(24,2,2,4,3);
    VxDoBlitUpsideDown(image.src,image.dst); image.CheckStorage(); image.CheckPixels(true,30);
}

TEST_F(QuantizeImageReferenceTest, RepeatedQuantizationHasNoPreviousImageState) {
    SetQuantizationSamplingFactor(15);
    QuantizeReferenceImage first, unrelated(24,32,32,3,0), repeated;
    first.Run(); unrelated.Run(); repeated.Run();
    EXPECT_EQ(std::memcmp(first.palette,repeated.palette,sizeof(first.palette)),0);
    EXPECT_EQ(std::memcmp(first.indices,repeated.indices,sizeof(first.indices)),0);
    repeated.CheckPixels();
}

TEST_F(QuantizeImageReferenceTest, TinyImageWithLargeSamplingStillLearnsItsColor) {
    SetQuantizationSamplingFactor(INT_MAX);
    QuantizeReferenceImage image(32,1,1,4,2);
    image.Run(); image.CheckPixels(false,0);
    EXPECT_EQ(GetQuantizationSamplingFactor(),INT_MAX);
}

static void CheckUnrepresentableQuantizeImage(int dimension) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    XBYTE pixel[4] = {};
    XBYTE index = 0;
    XBYTE palette[1024] = {};
    VxImageDescEx src, dst;
    src.Width = dst.Width = dimension == 0 ? INT_MAX / 3 + 1 : 2;
    src.Height = dst.Height = dimension == 0 ? 1 : INT_MAX / 6 + 1;
    src.BitsPerPixel = 32; src.BytesPerLine = 8; src.Image = pixel;
    dst.BitsPerPixel = 8; dst.BytesPerLine = 2; dst.Image = &index;
    dst.BytesPerColorEntry = 4; dst.ColorMapEntries = 256; dst.ColorMap = palette;
    bool result = TheBlitter.QuantizeImage(src,dst) != FALSE;
    std::_Exit(result ? 2 : 0);
}

TEST_F(QuantizeImageReferenceTest, RejectsUnrepresentableRgbLengthBeforeReadingPixels) {
    // Isolate the old arithmetic-overflow failure; allocate only the real tiny buffers.
    for (int dimension = 0; dimension < 2; ++dimension)
        EXPECT_EXIT(CheckUnrepresentableQuantizeImage(dimension), testing::ExitedWithCode(0), "");
}
