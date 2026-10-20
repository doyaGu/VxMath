#include <gtest/gtest.h>

#include <climits>
#include <cstring>
#include "VxMath.h"

class QuantizationSamplingReferenceTest : public testing::Test {
protected:
    void SetUp() override { m_SavedFactor = GetQuantizationSamplingFactor(); }
    void TearDown() override { SetQuantizationSamplingFactor(m_SavedFactor); }

private:
    int m_SavedFactor;
};

// Public blit integration, with independent grayscale reconstruction checks.
// This does not certify the complete NeuQuant algorithm or all palette formats.
struct SamplingImage {
    XBYTE source[(32 * 4 + 8) * 32 + 16];
    XBYTE destination[(32 + 3) * 32 + 16];
    XBYTE palette[256 * 4 + 16];
    VxImageDescEx src;
    VxImageDescEx dst;

    explicit SamplingImage(int depth) {
        std::memset(source, 0xa5, sizeof(source));
        std::memset(destination, 0xa5, sizeof(destination));
        std::memset(palette, 0xa5, sizeof(palette));
        const int bytes = depth / 8;
        const int pitch = 32 * bytes + 8;
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                XBYTE *p = source + 8 + y * pitch + x * bytes;
                p[0] = p[1] = p[2] = Gray(x, y);
                if (bytes == 4) p[3] = static_cast<XBYTE>(x + y);
            }
        }
        src.Width = dst.Width = src.Height = dst.Height = 32;
        src.BitsPerPixel = depth;
        src.BytesPerLine = pitch;
        src.Image = source + 8;
        src.RedMask = 0xff0000;
        src.GreenMask = 0xff00;
        src.BlueMask = 0xff;
        src.AlphaMask = bytes == 4 ? 0xff000000 : 0;
        dst.BitsPerPixel = 8;
        dst.BytesPerLine = 35;
        dst.Image = destination + 8;
        dst.BytesPerColorEntry = 4;
        dst.ColorMapEntries = 256;
        dst.ColorMap = palette + 8;
    }

    static XBYTE Gray(int x, int y) {
        return static_cast<XBYTE>((x * 17 + y * 31 + (x * y) % 19) & 255);
    }

    void RunAndCheck() {
        XBYTE before[sizeof(source)];
        std::memcpy(before, source, sizeof(source));
        const VxImageDescEx beforeSrc = src;
        const VxImageDescEx beforeDst = dst;
        const int setting = GetQuantizationSamplingFactor();
        VxDoBlit(src, dst);
        EXPECT_EQ(GetQuantizationSamplingFactor(), setting);
        EXPECT_EQ(std::memcmp(source, before, sizeof(source)), 0);
        EXPECT_EQ(std::memcmp(&src, &beforeSrc, sizeof(src)), 0);
        EXPECT_EQ(std::memcmp(&dst, &beforeDst, sizeof(dst)), 0);
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                const int index = destination[8 + y * 35 + x];
                for (int c = 0; c < 3; ++c) {
                    const int error = static_cast<int>(palette[8 + index * 4 + c]) - Gray(x, y);
                    EXPECT_GE(error, -20) << x << "," << y << " channel " << c;
                    EXPECT_LE(error, 20) << x << "," << y << " channel " << c;
                }
            }
            for (int x = 32; x < 35; ++x) EXPECT_EQ(destination[8 + y * 35 + x], 0xa5);
        }
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(palette[i], 0xa5);
            EXPECT_EQ(palette[1032 + i], 0xa5);
            EXPECT_EQ(destination[i], 0xa5);
            EXPECT_EQ(destination[1128 + i], 0xa5);
        }
    }

    unsigned long long PaletteRGBHash() const {
        unsigned long long hash = 14695981039346656037ULL;
        for (int i = 0; i < 256; ++i) {
            for (int c = 0; c < 3; ++c) hash = (hash ^ palette[8 + i * 4 + c]) * 1099511628211ULL;
        }
        return hash;
    }
};

TEST_F(QuantizationSamplingReferenceTest, InitializedDefaultIsFifteen) {
    // The original module initializer 0x2429A3F0 sets the effective default to15.
    EXPECT_EQ(GetQuantizationSamplingFactor(), 15);
}

TEST_F(QuantizationSamplingReferenceTest, StoresTheFullSignedIntegerWithoutClamping) {
    // Original DLL's Get/Set exports at0x2428FE70/0x2428FE80 return these literally.
    const int settings[] = {-1, 0, 1, 2, 15, 30, 31, 999, INT_MIN, INT_MAX, -7};
    for (int i = 0; i < 11; ++i) {
        SCOPED_TRACE(settings[i]);
        SetQuantizationSamplingFactor(settings[i]);
        EXPECT_EQ(GetQuantizationSamplingFactor(), settings[i]);
        EXPECT_EQ(GetQuantizationSamplingFactor(), settings[i]);
    }
}

TEST_F(QuantizationSamplingReferenceTest, LastWriteWinsAcrossRepeatedReads) {
    SetQuantizationSamplingFactor(30);
    SetQuantizationSamplingFactor(2);
    for (int i = 0; i < 32; ++i) EXPECT_EQ(GetQuantizationSamplingFactor(), 2);
    SetQuantizationSamplingFactor(-1);
    EXPECT_EQ(GetQuantizationSamplingFactor(), -1);
}

class QuantizationSamplingPaletteTest : public QuantizationSamplingReferenceTest,
                                        public testing::WithParamInterface<int> {};

TEST_P(QuantizationSamplingPaletteTest, MatchesOriginalRGBPaletteForGrayImage) {
    // Original public VxDoBlit, 32x32 ARGB, pitch136, 256 four-byte palette entries.
    // Alpha bytes and original faulty index output are deliberately not oracle data.
    static const int factors[] = {1, 15, 30};
    static const unsigned long long hashes[] = {
        0x7977312908388267ULL, 0x63f0e594531fb353ULL, 0x92ecc2f0b5f3d934ULL
    };
    const int sample = GetParam();
    SetQuantizationSamplingFactor(factors[sample]);
    SamplingImage image(32);
    image.RunAndCheck();
    EXPECT_EQ(image.PaletteRGBHash(), hashes[sample]);
}

INSTANTIATE_TEST_SUITE_P(OriginalFactors, QuantizationSamplingPaletteTest, testing::Range(0, 3));

TEST_F(QuantizationSamplingReferenceTest, NonpositiveSettingsUseLocalFallbackWithoutChangingState) {
    const int settings[] = {-1, 0, INT_MIN};
    for (int depth = 24; depth <= 32; depth += 8) {
        SetQuantizationSamplingFactor(1);
        SamplingImage reference(depth);
        reference.RunAndCheck();
        for (int i = 0; i < 3; ++i) {
            SCOPED_TRACE(settings[i]);
            SetQuantizationSamplingFactor(settings[i]);
            SamplingImage actual(depth);
            actual.RunAndCheck();
            EXPECT_EQ(std::memcmp(actual.palette, reference.palette, sizeof(actual.palette)), 0);
            EXPECT_EQ(std::memcmp(actual.destination, reference.destination, sizeof(actual.destination)), 0);
        }
    }
}

TEST_F(QuantizationSamplingReferenceTest, LargeSettingsUseLocalUpperBoundWithoutChangingState) {
    const int settings[] = {31, INT_MAX};
    for (int depth = 24; depth <= 32; depth += 8) {
        SetQuantizationSamplingFactor(30);
        SamplingImage reference(depth);
        reference.RunAndCheck();
        for (int i = 0; i < 2; ++i) {
            SCOPED_TRACE(settings[i]);
            SetQuantizationSamplingFactor(settings[i]);
            SamplingImage actual(depth);
            actual.RunAndCheck();
            EXPECT_EQ(std::memcmp(actual.palette, reference.palette, sizeof(actual.palette)), 0);
            EXPECT_EQ(std::memcmp(actual.destination, reference.destination, sizeof(actual.destination)), 0);
        }
    }
}

TEST_F(QuantizationSamplingReferenceTest, SamplingChangesTheActualPalette) {
    SetQuantizationSamplingFactor(1);
    SamplingImage dense(32);
    dense.RunAndCheck();
    SetQuantizationSamplingFactor(30);
    SamplingImage sparse(32);
    sparse.RunAndCheck();
    EXPECT_NE(dense.PaletteRGBHash(), sparse.PaletteRGBHash());
}
