#include "VxMath.h"
#include "VxSIMD.h"
#include <cstring>
#include <gtest/gtest.h>

static VxImageDescEx AlphaImage(int width, int height, int bpp, XDWORD mask, XBYTE *image, int pitch) {
    VxImageDescEx desc;
    desc.Width = width;
    desc.Height = height;
    desc.BitsPerPixel = bpp;
    desc.AlphaMask = mask;
    desc.BytesPerLine = pitch;
    desc.Image = image;
    return desc;
}

static const XBYTE AlphaInputs[17] = {0, 1, 15, 16, 127, 128, 200, 240, 255, 3, 32, 64, 129, 171, 254, 85, 170};
static const XBYTE AlphaNibbles[17] = {0, 0, 0, 1, 7, 8, 12, 15, 15, 0, 2, 4, 8, 10, 15, 5, 10};

class GraphicAlphaReferenceTest : public ::testing::TestWithParam<int> {
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
        for (int i = begin; i < end; ++i) {
            EXPECT_EQ(0x5a, data[i]) << "guard byte " << i;
        }
    }
    XDWORD Pixel(const XBYTE *data, int bytes) {
        XDWORD value = 0;
        std::memcpy(&value, data, bytes);
        return value;
    }
};

TEST_P(GraphicAlphaReferenceTest, ConstantArgbKeepsRgbAndIndependentRowPadding) {
    XBYTE storage[176];
    const XBYTE alpha[] = {0, 128, 255};
    const XDWORD expected[] = {0x005a5a5a, 0x805a5a5a, 0xff5a5a5a};
    for (int a = 0; a < 3; ++a) {
        std::memset(storage, 0x5a, sizeof(storage));
        VxImageDescEx desc = AlphaImage(17, 2, 32, 0xff000000, storage + 8, 76);
        VxImageDescEx before = desc;
        VxDoAlphaBlit(desc, alpha[a]);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 17; ++x) {
                EXPECT_EQ(expected[a], Pixel(storage + 8 + y * 76 + x * 4, 4));
            }
            Guard(storage, 8 + y * 76 + 68, 8 + (y + 1) * 76);
        }
        EXPECT_EQ(0, std::memcmp(&before, &desc, sizeof(desc)));
        Guard(storage, 0, 8);
        Guard(storage, 160, 176);
    }
}

TEST_P(GraphicAlphaReferenceTest, ArrayArgbUsesTightAlphaRowsWithAPaddedDestination) {
    XBYTE storage[176], alpha[34], saved[34];
    std::memset(storage, 0x5a, sizeof(storage));
    for (int x = 0; x < 17; ++x) {
        alpha[x] = AlphaInputs[x];
        alpha[17 + x] = AlphaInputs[16 - x];
    }
    std::memcpy(saved, alpha, sizeof(alpha));
    VxImageDescEx desc = AlphaImage(17, 2, 32, 0xff000000, storage + 8, 76);
    VxDoAlphaBlit(desc, alpha);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 17; ++x) {
            const XBYTE *pixel = storage + 8 + y * 76 + x * 4;
            EXPECT_EQ(alpha[y * 17 + x], pixel[3]);
            Guard(pixel, 0, 3);
        }
        Guard(storage, 8 + y * 76 + 68, 8 + (y + 1) * 76);
    }
    EXPECT_EQ(0, std::memcmp(alpha, saved, sizeof(alpha)));
    Guard(storage, 0, 8);
    Guard(storage, 160, 176);
}

TEST_P(GraphicAlphaReferenceTest, ConstantAndArrayUseTheSameHighNibbleInDwordStorage) {
    XBYTE constant[96], array[96], alpha[17];
    std::memset(constant, 0x5a, sizeof(constant));
    std::memset(array, 0x5a, sizeof(array));
    std::memset(alpha, 200, sizeof(alpha));
    VxImageDescEx desc = AlphaImage(17, 1, 32, 0x0000000f, constant + 8, 76);
    VxDoAlphaBlit(desc, XBYTE(200));
    desc.Image = array + 8;
    VxDoAlphaBlit(desc, alpha);
    for (int x = 0; x < 17; ++x) {
        EXPECT_EQ(0x5a5a5a5cu, Pixel(constant + 8 + x * 4, 4));
        EXPECT_EQ(0x5a5a5a5cu, Pixel(array + 8 + x * 4, 4));
    }
    Guard(array, 0, 8);
    Guard(array, 76, 96);
    Guard(constant, 0, 8);
    Guard(constant, 76, 96);
}

TEST_P(GraphicAlphaReferenceTest, DwordNarrowAlphaPreservesOtherBitsInVectorBodyAndTail) {
    XBYTE storage[176], alpha[34];
    std::memset(storage, 0x5a, sizeof(storage));
    for (int x = 0; x < 17; ++x) {
        alpha[x] = AlphaInputs[x];
        alpha[17 + x] = AlphaInputs[16 - x];
    }
    VxImageDescEx desc = AlphaImage(17, 2, 32, 0x0000000f, storage + 8, 76);
    VxDoAlphaBlit(desc, alpha);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 17; ++x) {
            const int sample = y ? 16 - x : x;
            EXPECT_EQ(0x5a5a5a50u | AlphaNibbles[sample], Pixel(storage + 8 + y * 76 + x * 4, 4));
        }
        Guard(storage, 8 + y * 76 + 68, 8 + (y + 1) * 76);
    }
    Guard(storage, 0, 8);
    Guard(storage, 160, 176);
}

TEST_P(GraphicAlphaReferenceTest, TwoBitAlphaCoversAllByteValuesWithFourOrderedLevels) {
    XBYTE storage[1048], alpha[256];
    std::memset(storage, 0x5a, sizeof(storage));
    for (int i = 0; i < 256; ++i) alpha[i] = XBYTE(i);
    VxImageDescEx desc = AlphaImage(256, 1, 32, 0xc0000000, storage + 8, 1032);
    VxDoAlphaBlit(desc, alpha);
    const XDWORD expected[] = {0x1a5a5a5a, 0x5a5a5a5a, 0x9a5a5a5a, 0xda5a5a5a};
    for (int level = 0; level < 4; ++level) {
        for (int i = 0; i < 64; ++i) {
            EXPECT_EQ(expected[level], Pixel(storage + 8 + (level * 64 + i) * 4, 4));
        }
    }
    Guard(storage, 0, 8);
    Guard(storage, 1032, 1048);
}

TEST_P(GraphicAlphaReferenceTest, ByteAlphaCoversAllValuesAndPreservesTheLowNibble) {
    XBYTE storage[280], alpha[256];
    std::memset(storage, 0x5a, sizeof(storage));
    for (int i = 0; i < 256; ++i) alpha[i] = XBYTE(i);
    VxImageDescEx desc = AlphaImage(256, 1, 8, 0xf0, storage + 8, 264);
    VxDoAlphaBlit(desc, alpha);
    const XBYTE expected[] = {0x0a, 0x1a, 0x2a, 0x3a, 0x4a, 0x5a, 0x6a, 0x7a,
                              0x8a, 0x9a, 0xaa, 0xba, 0xca, 0xda, 0xea, 0xfa};
    for (int level = 0; level < 16; ++level) {
        for (int i = 0; i < 16; ++i) EXPECT_EQ(expected[level], storage[8 + level * 16 + i]);
    }
    Guard(storage, 0, 8);
    Guard(storage, 264, 280);
}

TEST_P(GraphicAlphaReferenceTest, OneBitWordAlphaUsesTheHighBitThresholdForAllValues) {
    XBYTE storage[536], alpha[256];
    std::memset(storage, 0x5a, sizeof(storage));
    for (int i = 0; i < 256; ++i) alpha[i] = XBYTE(i);
    VxImageDescEx desc = AlphaImage(256, 1, 16, 0x8000, storage + 8, 520);
    VxDoAlphaBlit(desc, alpha);
    for (int i = 0; i < 128; ++i) EXPECT_EQ(0x5a5au, Pixel(storage + 8 + i * 2, 2));
    for (int i = 128; i < 256; ++i) EXPECT_EQ(0xda5au, Pixel(storage + 8 + i * 2, 2));
    Guard(storage, 0, 8);
    Guard(storage, 520, 536);
}

TEST_P(GraphicAlphaReferenceTest, WordAlphaLookupTracksConsecutiveMaskAndShiftChanges) {
    XBYTE storage[64], alpha[17];
    std::memcpy(alpha, AlphaInputs, sizeof(alpha));
    const XDWORD masks[] = {0xf000, 0x000f, 0x8000, 0xf000};
    for (int pass = 0; pass < 4; ++pass) {
        std::memset(storage, 0x5a, sizeof(storage));
        VxImageDescEx desc = AlphaImage(17, 1, 16, masks[pass], storage + 8, 42);
        VxDoAlphaBlit(desc, alpha);
        for (int x = 0; x < 17; ++x) {
            XDWORD expected = 0;
            if (pass == 0 || pass == 3) expected = 0x0a5a | (XDWORD(AlphaNibbles[x]) * 0x1000);
            if (pass == 1) expected = 0x5a50 | AlphaNibbles[x];
            if (pass == 2) expected = AlphaInputs[x] < 128 ? 0x5a5a : 0xda5a;
            EXPECT_EQ(expected, Pixel(storage + 8 + x * 2, 2));
        }
        Guard(storage, 0, 8);
        Guard(storage, 42, 64);
    }
}

TEST_P(GraphicAlphaReferenceTest, ConstantNarrowAlphaUsesHighBitsAcrossStorageDepths) {
    XBYTE storage[96];
    const int bits[] = {8, 16, 16, 32};
    const XDWORD masks[] = {0xf0, 0x8000, 0xf000, 0xc0000000};
    const XDWORD expected[] = {0xca, 0xda5a, 0xca5a, 0xda5a5a5a};
    for (int layout = 0; layout < 4; ++layout) {
        std::memset(storage, 0x5a, sizeof(storage));
        const int bytes = bits[layout] / 8;
        VxImageDescEx desc = AlphaImage(17, 1, bits[layout], masks[layout], storage + 8, 17 * bytes + 8);
        VxDoAlphaBlit(desc, XBYTE(200));
        for (int x = 0; x < 17; ++x) EXPECT_EQ(expected[layout], Pixel(storage + 8 + x * bytes, bytes));
        Guard(storage, 0, 8);
        Guard(storage, 8 + 17 * bytes, 96);
    }
}

TEST_P(GraphicAlphaReferenceTest, EightBitChannelsKeepTheirRangeAtDifferentPositions) {
    XBYTE storage[96], alpha[17];
    std::memcpy(alpha, AlphaInputs, sizeof(alpha));
    const int bits[] = {8, 16, 32, 32};
    const XDWORD masks[] = {0xff, 0xff00, 0x00ff0000, 0xff000000};
    const XDWORD places[] = {1, 0x100, 0x10000, 0x1000000};
    const XDWORD preserved[] = {0, 0x5a, 0x5a005a5a, 0x005a5a5a};
    for (int layout = 0; layout < 4; ++layout) {
        std::memset(storage, 0x5a, sizeof(storage));
        const int bytes = bits[layout] / 8;
        VxImageDescEx desc = AlphaImage(17, 1, bits[layout], masks[layout], storage + 8, 17 * bytes + 8);
        VxDoAlphaBlit(desc, alpha);
        for (int x = 0; x < 17; ++x) {
            EXPECT_EQ(preserved[layout] | (XDWORD(AlphaInputs[x]) * places[layout]), Pixel(storage + 8 + x * bytes, bytes));
        }
        Guard(storage, 0, 8);
        Guard(storage, 8 + 17 * bytes, 96);
    }
}

TEST_P(GraphicAlphaReferenceTest, NoAlphaAndUnsupported24BitStorageLeavePixelsUntouched) {
    XBYTE storage[96], alpha[17];
    std::memset(storage, 0x5a, sizeof(storage));
    std::memcpy(alpha, AlphaInputs, sizeof(alpha));
    VxImageDescEx desc = AlphaImage(17, 1, 32, 0, storage + 8, 76);
    VxDoAlphaBlit(desc, XBYTE(200));
    VxDoAlphaBlit(desc, alpha);
    desc.BitsPerPixel = 24;
    desc.AlphaMask = 0xff0000;
    desc.BytesPerLine = 60;
    VxDoAlphaBlit(desc, XBYTE(200));
    VxDoAlphaBlit(desc, alpha);
    Guard(storage, 0, 96);
}

TEST_P(GraphicAlphaReferenceTest, ZeroWidthLeavesPixelsAndInputUntouched) {
    XBYTE storage[32], alpha = 200;
    std::memset(storage, 0x5a, sizeof(storage));
    VxImageDescEx desc = AlphaImage(0, 2, 32, 0xff000000, storage + 8, 8);
    VxDoAlphaBlit(desc, XBYTE(200));
    VxDoAlphaBlit(desc, &alpha);
    EXPECT_EQ(200, alpha);
    Guard(storage, 0, 32);
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2, GraphicAlphaReferenceTest,
                         ::testing::Values(VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_AVX2));
