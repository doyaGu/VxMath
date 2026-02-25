#include <cstdint>
#include <cstring>
#include <vector>

#include "BlitEngineTestHelpers.h"

using namespace BlitEngineTest;

namespace {

class BlitEngineSIMDBackendDiffTest : public BlitEngineTestBase {};

uint32_t NextRand(uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

XDWORD LoadPixel32(const XBYTE *ptr) {
    XDWORD p = 0;
    std::memcpy(&p, ptr, sizeof(XDWORD));
    return p;
}

void StorePixel32(XBYTE *ptr, XDWORD p) {
    std::memcpy(ptr, &p, sizeof(XDWORD));
}

TEST_F(BlitEngineSIMDBackendDiffTest, FillImage32_OddWidthMisaligned_BitExact) {
    const int width = 1919;
    const int height = 7;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u, 0xCDu);
    XBYTE *image = storage.data() + 3;

    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.FillImage(desc, 0x7FA0C0E0u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(0x7FA0C0E0u, p);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, PremultiplyAlpha_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 5;
    uint32_t seed = 0x12345678u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);

    blitter.PremultiplyAlpha(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD a = (src >> 24) & 0xFFu;
            const XDWORD r = (((src >> 16) & 0xFFu) * a) / 255u;
            const XDWORD g = (((src >> 8) & 0xFFu) * a) / 255u;
            const XDWORD b = ((src & 0xFFu) * a) / 255u;
            const XDWORD expected = (a << 24) | (r << 16) | (g << 8) | b;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, SwapRedBlue_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 1;
    uint32_t seed = 0x89ABCDEFu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);

    blitter.SwapRedBlue(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD expected = (src & 0xFF00FF00u) | ((src & 0x00FF0000u) >> 16) | ((src & 0x000000FFu) << 16);
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, MultiplyBlend_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int pitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(pitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *srcImage = srcStorage.data() + 2;
    XBYTE *dstImage = dstStorage.data() + 6;

    uint32_t seedSrc = 0x31415926u;
    uint32_t seedDst = 0x27182818u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * pitch + x * 4, NextRand(seedSrc));
            StorePixel32(dstImage + y * pitch + x * 4, NextRand(seedDst));
        }
    }

    std::vector<XBYTE> srcCopy(srcImage, srcImage + static_cast<std::size_t>(pitch) * height);
    std::vector<XBYTE> dstCopy(dstImage, dstImage + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);

    blitter.MultiplyBlend(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD s = LoadPixel32(srcCopy.data() + y * pitch + x * 4);
            const XDWORD d = LoadPixel32(dstCopy.data() + y * pitch + x * 4);
            const XDWORD r = (((s >> 16) & 0xFFu) * ((d >> 16) & 0xFFu) + 127u) / 255u;
            const XDWORD g = (((s >> 8) & 0xFFu) * ((d >> 8) & 0xFFu) + 127u) / 255u;
            const XDWORD b = ((s & 0xFFu) * (d & 0xFFu) + 127u) / 255u;
            const XDWORD a = (((s >> 24) & 0xFFu) * ((d >> 24) & 0xFFu) + 127u) / 255u;
            const XDWORD expected = (a << 24) | (r << 16) | (g << 8) | b;
            const XDWORD actual = LoadPixel32(dstImage + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_32RGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 5;
    XBYTE *dstImage = dstStorage.data() + 11;

    uint32_t seed = 0xABCDEF01u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitRGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XDWORD expected = p & 0x00FFFFFFu;
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32RGB_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 9;
    XBYTE *dstImage = dstStorage.data() + 3;

    uint32_t seed = 0x13579BDFu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitRGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XDWORD expected = p | 0xFF000000u;
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_565_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 2;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 7;
    XBYTE *dstImage = dstStorage.data() + 5;

    uint32_t seed = 0xA1B2C3D4u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create16Bit565(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XWORD expected = (XWORD)(((p >> 8) & 0xF800u) | ((p >> 5) & 0x07E0u) | ((p >> 3) & 0x001Fu));
            XWORD actual = 0;
            std::memcpy(&actual, dstImage + y * dstPitch + x * 2, sizeof(XWORD));
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_565_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 2;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 3;
    XBYTE *dstImage = dstStorage.data() + 1;

    uint32_t seed = 0x55AA7711u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XWORD p = (XWORD)(NextRand(seed) & 0xFFFFu);
            std::memcpy(srcImage + y * srcPitch + x * 2, &p, sizeof(XWORD));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create16Bit565(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            XWORD p = 0;
            std::memcpy(&p, srcImage + y * srcPitch + x * 2, sizeof(XWORD));
            const XDWORD expected = 0xFF000000u |
                                    ((p & 0xF800u) << 8) | ((p & 0xE000u) << 3) |
                                    ((p & 0x07E0u) << 5) | ((p & 0x0600u) >> 1) |
                                    ((p & 0x001Fu) << 3) | ((p & 0x001Cu) >> 2);
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_555_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 2;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 11;
    XBYTE *dstImage = dstStorage.data() + 9;

    uint32_t seed = 0xCAFEBABEu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create16Bit555(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XWORD expected = (XWORD)(((p >> 9) & 0x7C00u) | ((p >> 6) & 0x03E0u) | ((p >> 3) & 0x001Fu));
            XWORD actual = 0;
            std::memcpy(&actual, dstImage + y * dstPitch + x * 2, sizeof(XWORD));
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_1555_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 2;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 4;
    XBYTE *dstImage = dstStorage.data() + 13;

    uint32_t seed = 0x10293847u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create16Bit1555(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XWORD expected = (XWORD)(((p >> 16) & 0x8000u) | ((p >> 9) & 0x7C00u) | ((p >> 6) & 0x03E0u) | ((p >> 3) & 0x001Fu));
            XWORD actual = 0;
            std::memcpy(&actual, dstImage + y * dstPitch + x * 2, sizeof(XWORD));
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_4444_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 2;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 6;
    XBYTE *dstImage = dstStorage.data() + 3;

    uint32_t seed = 0x55667788u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create16Bit4444(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XWORD expected = (XWORD)(((p >> 16) & 0xF000u) | ((p >> 12) & 0x0F00u) | ((p >> 8) & 0x00F0u) | ((p >> 4) & 0x000Fu));
            XWORD actual = 0;
            std::memcpy(&actual, dstImage + y * dstPitch + x * 2, sizeof(XWORD));
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_555_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 2;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 8;
    XBYTE *dstImage = dstStorage.data() + 5;

    uint32_t seed = 0x13572468u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XWORD p = (XWORD)(NextRand(seed) & 0xFFFFu);
            std::memcpy(srcImage + y * srcPitch + x * 2, &p, sizeof(XWORD));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create16Bit555(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            XWORD p = 0;
            std::memcpy(&p, srcImage + y * srcPitch + x * 2, sizeof(XWORD));
            const XDWORD expected = 0xFF000000u |
                                    ((p & 0x7C00u) << 9) | ((p & 0x7000u) << 4) |
                                    ((p & 0x03E0u) << 6) | ((p & 0x0380u) << 1) |
                                    ((p & 0x001Fu) << 3) | ((p & 0x001Cu) >> 2);
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_1555_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 2;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 2;
    XBYTE *dstImage = dstStorage.data() + 10;

    uint32_t seed = 0x89ABC001u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XWORD p = (XWORD)(NextRand(seed) & 0xFFFFu);
            std::memcpy(srcImage + y * srcPitch + x * 2, &p, sizeof(XWORD));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create16Bit1555(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            XWORD p = 0;
            std::memcpy(&p, srcImage + y * srcPitch + x * 2, sizeof(XWORD));
            const XDWORD expected = ((p & 0x8000u) ? 0xFF000000u : 0x00000000u) |
                                    ((p & 0x7C00u) << 9) | ((p & 0x7000u) << 4) |
                                    ((p & 0x03E0u) << 6) | ((p & 0x0380u) << 1) |
                                    ((p & 0x001Fu) << 3) | ((p & 0x001Cu) >> 2);
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_4444_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 2;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 12;
    XBYTE *dstImage = dstStorage.data() + 14;

    uint32_t seed = 0x0BADF00Du;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XWORD p = (XWORD)(NextRand(seed) & 0xFFFFu);
            std::memcpy(srcImage + y * srcPitch + x * 2, &p, sizeof(XWORD));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create16Bit4444(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            XWORD p = 0;
            std::memcpy(&p, srcImage + y * srcPitch + x * 2, sizeof(XWORD));
            const XDWORD expected = ((p & 0xF000u) << 16) | ((p & 0xF000u) << 12) |
                                    ((p & 0x0F00u) << 12) | ((p & 0x0F00u) << 8) |
                                    ((p & 0x00F0u) << 8)  | ((p & 0x00F0u) << 4) |
                                    ((p & 0x000Fu) << 4)  | (p & 0x000Fu);
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_32ARGB_To_24RGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 4;
    const int dstPitch = width * 3;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 5;
    XBYTE *dstImage = dstStorage.data() + 7;

    uint32_t seed = 0xDEADBEEFu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(srcImage + y * srcPitch + x * 4, NextRand(seed));
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create32BitARGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create24BitRGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD p = LoadPixel32(srcImage + y * srcPitch + x * 4);
            const XBYTE *actual = dstImage + y * dstPitch + x * 3;
            EXPECT_EQ((XBYTE)p, actual[0]);
            EXPECT_EQ((XBYTE)(p >> 8), actual[1]);
            EXPECT_EQ((XBYTE)(p >> 16), actual[2]);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_24RGB_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width * 3;
    const int dstPitch = width * 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    XBYTE *srcImage = srcStorage.data() + 9;
    XBYTE *dstImage = dstStorage.data() + 3;

    uint32_t seed = 0x1234FEDCu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t r = NextRand(seed);
            XBYTE *px = srcImage + y * srcPitch + x * 3;
            px[0] = (XBYTE)(r & 0xFFu);
            px[1] = (XBYTE)((r >> 8) & 0xFFu);
            px[2] = (XBYTE)((r >> 16) & 0xFFu);
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create24BitRGB(width, height, srcImage);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XBYTE *srcPx = srcImage + y * srcPitch + x * 3;
            const XDWORD expected = (XDWORD)srcPx[0] |
                                    ((XDWORD)srcPx[1] << 8) |
                                    ((XDWORD)srcPx[2] << 16) |
                                    0xFF000000u;
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, ClearAlpha_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 2;
    uint32_t seed = 0xFEED1234u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.ClearAlpha(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD expected = src & 0x00FFFFFFu;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, SetFullAlpha_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 6;
    uint32_t seed = 0xBEEFCAFEu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.SetFullAlpha(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD expected = src | 0xFF000000u;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, InvertColors_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 1;
    uint32_t seed = 0xA5A55A5Au;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.InvertColors(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD expected = src ^ 0x00FFFFFFu;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, ConvertToGrayscale_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 5;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 4;
    uint32_t seed = 0xC001D00Du;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.ConvertToGrayscale(desc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD a = src & 0xFF000000u;
            const XDWORD r = (src >> 16) & 0xFFu;
            const XDWORD g = (src >> 8) & 0xFFu;
            const XDWORD b = src & 0xFFu;
            const XDWORD yv = (77u * r + 150u * g + 29u * b) >> 8;
            const XDWORD expected = a | (yv << 16) | (yv << 8) | yv;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, DoAlphaBlit_Constant_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 4;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    XBYTE *image = storage.data() + 3;
    uint32_t seed = 0x13579BDFu;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.DoAlphaBlit(desc, (XBYTE)0x7Bu);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD expected = (src & 0x00FFFFFFu) | 0x7B000000u;
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, DoAlphaBlit_Array_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 4;
    const int pitch = width * 4;

    std::vector<XBYTE> storage(static_cast<std::size_t>(pitch) * height + 16u);
    std::vector<XBYTE> alphaValues(static_cast<std::size_t>(width) * height);
    XBYTE *image = storage.data() + 11;

    uint32_t seed = 0x2468ACE1u;
    uint32_t alphaSeed = 0x55AA1234u;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            StorePixel32(image + y * pitch + x * 4, NextRand(seed));
            alphaValues[static_cast<std::size_t>(y) * width + x] = (XBYTE)(NextRand(alphaSeed) & 0xFFu);
        }
    }

    std::vector<XBYTE> original(image, image + static_cast<std::size_t>(pitch) * height);
    VxImageDescEx desc = ImageDescFactory::Create32BitARGB(width, height, image);
    blitter.DoAlphaBlit(desc, alphaValues.data());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XDWORD src = LoadPixel32(original.data() + y * pitch + x * 4);
            const XDWORD a = (XDWORD)alphaValues[static_cast<std::size_t>(y) * width + x];
            const XDWORD expected = (src & 0x00FFFFFFu) | (a << 24);
            const XDWORD actual = LoadPixel32(image + y * pitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

TEST_F(BlitEngineSIMDBackendDiffTest, Blit_Paletted8_To_32ARGB_OddWidthMisaligned_MatchesReference) {
    const int width = 1919;
    const int height = 3;
    const int srcPitch = width;
    const int dstPitch = width * 4;
    const int bytesPerColorEntry = 4;

    std::vector<XBYTE> srcStorage(static_cast<std::size_t>(srcPitch) * height + 16u);
    std::vector<XBYTE> dstStorage(static_cast<std::size_t>(dstPitch) * height + 16u, 0u);
    std::vector<XBYTE> palette(256u * bytesPerColorEntry);
    XBYTE *srcImage = srcStorage.data() + 5;
    XBYTE *dstImage = dstStorage.data() + 9;

    uint32_t seed = 0xABCDEF01u;
    uint32_t palSeed = 0x11223344u;
    for (int i = 0; i < 256; ++i) {
        palette[i * bytesPerColorEntry + 0] = (XBYTE)(NextRand(palSeed) & 0xFFu); // B
        palette[i * bytesPerColorEntry + 1] = (XBYTE)(NextRand(palSeed) & 0xFFu); // G
        palette[i * bytesPerColorEntry + 2] = (XBYTE)(NextRand(palSeed) & 0xFFu); // R
        palette[i * bytesPerColorEntry + 3] = (XBYTE)(NextRand(palSeed) & 0xFFu); // A
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            srcImage[y * srcPitch + x] = (XBYTE)(NextRand(seed) & 0xFFu);
        }
    }

    VxImageDescEx srcDesc = ImageDescFactory::Create8BitPaletted(width, height, srcImage, palette.data(), bytesPerColorEntry);
    VxImageDescEx dstDesc = ImageDescFactory::Create32BitARGB(width, height, dstImage);
    blitter.DoBlit(srcDesc, dstDesc);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const XBYTE idx = srcImage[y * srcPitch + x];
            const XBYTE *entry = palette.data() + static_cast<std::size_t>(idx) * bytesPerColorEntry;
            const XDWORD expected = ((XDWORD)entry[3] << 24) | ((XDWORD)entry[2] << 16) |
                                    ((XDWORD)entry[1] << 8) | (XDWORD)entry[0];
            const XDWORD actual = LoadPixel32(dstImage + y * dstPitch + x * 4);
            EXPECT_EQ(expected, actual);
        }
    }
}

} // namespace
