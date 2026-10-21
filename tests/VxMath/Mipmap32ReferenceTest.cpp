#include <cstring>
#include <cstdio>
#ifndef VX_MIPMAP_NATIVE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#include "VxSIMDMode.h"
#else
#include "VxImageDescEx.h"
#endif
#ifdef _WIN32
#include <windows.h>
#endif

typedef void (*Mipmap32Call)(const VxImageDescEx &, XBYTE *);

struct Mipmap32Observations {
    unsigned pixels = 0, guards = 0, errors = 0;
    unsigned digest = 2166136261u;
};

static VxImageDescEx Mipmap32Desc(int width, int height, int pitch, XBYTE *image) {
    VxImageDescEx desc;
    desc.Width = width; desc.Height = height; desc.BytesPerLine = pitch;
    desc.BitsPerPixel = 32; desc.Image = image;
    desc.RedMask = 0xff0000; desc.GreenMask = 0xff00;
    desc.BlueMask = 0xff; desc.AlphaMask = 0xff000000;
    return desc;
}

// The oracle sums bytes independently; it shares no packed masks or SIMD code
// with the implementation. Original-DLL probes execute these same workloads.
static void CheckMipmap32(Mipmap32Call call, const VxImageDescEx &desc,
                         Mipmap32Observations &result) {
    XBYTE output[1024]; std::memset(output, 0xcd, sizeof(output));
    call(desc, output + 17);
    const int width = desc.Width == 1 ? 1 : desc.Width / 2;
    const int height = desc.Height == 1 ? 1 : desc.Height / 2;
    const int columns = desc.Width == 1 ? 1 : 2;
    const int rows = desc.Height == 1 ? 1 : 2;
    const int bytes = desc.Width == 1 && desc.Height == 1 ? 0 : width * height * 4;
    for (int y = 0; y < height && bytes; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 4; ++channel) {
                unsigned sum = 0;
                for (int dy = 0; dy < rows; ++dy)
                    for (int dx = 0; dx < columns; ++dx)
                        sum += desc.Image[(y * rows + dy) * desc.BytesPerLine +
                                          (x * columns + dx) * 4 + channel];
                const XBYTE actual = output[17 + (y * width + x) * 4 + channel];
                if (actual != sum / (rows * columns)) ++result.errors;
                result.digest = (result.digest ^ actual) * 16777619u;
            }
            ++result.pixels;
        }
    }
    for (int n = 0; n < int(sizeof(output)); ++n) {
        if (n >= 17 && n < 17 + bytes) continue;
        ++result.guards;
        if (output[n] != 0xcd) ++result.errors;
    }
}

static Mipmap32Observations Mipmap32Pairs(Mipmap32Call call) {
    Mipmap32Observations result; XBYTE pixels[24];
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned b = 0; b < 256; ++b) {
            for (int pair = 0; pair < 3; ++pair) {
                for (int channel = 0; channel < 4; ++channel) {
                    pixels[pair * 8 + channel] = XBYTE(a + pair * 73 + channel * 37);
                    pixels[pair * 8 + 4 + channel] = XBYTE(b + pair * 19 + channel * 113);
                }
            }
            CheckMipmap32(call, Mipmap32Desc(6, 1, 24, pixels), result);
        }
    }
    return result;
}

static Mipmap32Observations Mipmap32Sums(Mipmap32Call call) {
    Mipmap32Observations result; XBYTE pixels[48];
    for (unsigned sum = 0; sum <= 1020; ++sum) {
        for (int block = 0; block < 3; ++block) {
            for (int channel = 0; channel < 4; ++channel) {
                unsigned remaining = (sum + block * 313 + channel * 127) % 1021;
                for (int n = 0; n < 4; ++n) {
                    const int position = (n + block + channel) % 4;
                    const unsigned value = remaining > 255 ? 255 : remaining;
                    pixels[(position / 2) * 24 + block * 8 + (position % 2) * 4 + channel] = XBYTE(value);
                    remaining -= value;
                }
            }
        }
        CheckMipmap32(call, Mipmap32Desc(6, 2, 24, pixels), result);
    }
    // Averaging pairs first would lose the remainder and incorrectly yield 0.
    const XBYTE remainder[] = {1, 255, 0, 1, 0, 255, 0, 0,
                              0, 255, 1, 0, 3, 255, 3, 3};
    CheckMipmap32(call, Mipmap32Desc(2, 2, 8, const_cast<XBYTE *>(remainder)), result);
    return result;
}

static Mipmap32Observations Mipmap32Surfaces(Mipmap32Call call) {
    Mipmap32Observations result; XBYTE pixels[1024];
    for (int width = 2; width <= 21; ++width) {
        for (int height = 2; height <= 9; ++height) {
            const int pitch = width * 4 + 7;
            for (int direction = -1; direction <= 1; direction += 2) {
                std::memset(pixels, 0xe7, sizeof(pixels));
                XBYTE *image = pixels + 1 + (direction < 0 ? (height - 1) * pitch : 0);
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        for (int channel = 0; channel < 4; ++channel)
                            image[y * pitch * direction + x * 4 + channel] = XBYTE(y * 71 + x * 29 + channel * 53);
                XBYTE before[1024]; std::memcpy(before, pixels, sizeof(pixels));
                CheckMipmap32(call, Mipmap32Desc(width, height, pitch * direction, image), result);
                if (std::memcmp(before, pixels, sizeof(pixels))) ++result.errors;
            }
        }
    }
    return result;
}

static Mipmap32Observations Mipmap32Columns(Mipmap32Call call, bool padded) {
    Mipmap32Observations result; XBYTE pixels[128];
    for (int height = 2; height <= 17; ++height) {
        const int pitch = padded ? 7 : 4;
        for (int direction = padded ? -1 : 1; direction <= 1; direction += 2) {
            std::memset(pixels, 0xe7, sizeof(pixels));
            XBYTE *image = pixels + 1 + (direction < 0 ? (height - 1) * pitch : 0);
            for (int y = 0; y < height; ++y)
                for (int channel = 0; channel < 4; ++channel)
                    image[y * pitch * direction + channel] = XBYTE(y * 37 + channel * 53);
            CheckMipmap32(call, Mipmap32Desc(1, height, direction * pitch, image), result);
        }
    }
    return result;
}

#ifdef _WIN32
static Mipmap32Observations Mipmap32GuardPages(Mipmap32Call call) {
    Mipmap32Observations result; SYSTEM_INFO info; GetSystemInfo(&info);
    XBYTE *src = static_cast<XBYTE *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    XBYTE *dst = static_cast<XBYTE *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old = 0;
    if (!src || !dst || !VirtualProtect(src + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old) ||
        !VirtualProtect(dst + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old)) {
        ++result.errors;
    } else {
        for (int width = 2; width <= 11; ++width) {
            for (int height = 1; height <= 2; ++height) {
                const int bytes = width * height * 4, count = (width / 2) * 4;
                XBYTE *input = src + info.dwPageSize - bytes;
                XBYTE *output = dst + info.dwPageSize - count;
                for (int n = 0; n < bytes; ++n) input[n] = XBYTE(n * 71 + width * 29);
                std::memset(output - 8, 0xcd, count + 8);
                const VxImageDescEx desc = Mipmap32Desc(width, height, width * 4, input);
                call(desc, output);
                for (int x = 0; x < width / 2; ++x) {
                    for (int c = 0; c < 4; ++c) {
                        unsigned sum = input[x * 8 + c] + input[x * 8 + c + 4];
                        if (height == 2) sum += input[width * 4 + x * 8 + c] + input[width * 4 + x * 8 + c + 4];
                        XBYTE actual = output[x * 4 + c];
                        if (actual != sum / (height * 2)) ++result.errors;
                        result.digest = (result.digest ^ actual) * 16777619u;
                    }
                    ++result.pixels;
                }
                for (int n = -8; n < 0; ++n) { ++result.guards; if (output[n] != 0xcd) ++result.errors; }
            }
        }
    }
    if (src) VirtualFree(src, 0, MEM_RELEASE);
    if (dst) VirtualFree(dst, 0, MEM_RELEASE);
    return result;
}
#endif

#ifndef VX_MIPMAP_NATIVE_PROBE
static const int Mipmap32Modes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3,
                                   VX_SIMD_MODE_SSE4_1, VX_SIMD_MODE_AVX, VX_SIMD_MODE_AVX2};
class Mipmap32ReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        previous = VxGetSIMDOverride(); ASSERT_TRUE(VxSetSIMDOverride(Mipmap32Modes[GetParam()]));
        ASSERT_EQ(VxGetSIMDOverride(), Mipmap32Modes[GetParam()]);
        RecordProperty("effective_mode", VxGetSIMDEffectiveBackend());
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
    void Check(const Mipmap32Observations &result) {
        EXPECT_EQ(result.errors, 0u); EXPECT_GT(result.pixels, 0u); EXPECT_GT(result.guards, 0u);
        RecordProperty("pixels", int(result.pixels)); RecordProperty("guards", int(result.guards));
        char digest[16]; std::snprintf(digest, sizeof(digest), "%08x", result.digest);
        RecordProperty("pixel_digest", digest);
    }
};

TEST_P(Mipmap32ReferenceTest, ExhaustiveHorizontalBytePairs) { Check(Mipmap32Pairs(VxGenerateMipMap)); }
TEST_P(Mipmap32ReferenceTest, FourPixelSumsPreserveAllRemainders) { Check(Mipmap32Sums(VxGenerateMipMap)); }
TEST_P(Mipmap32ReferenceTest, OddUnalignedSurfacesAndBothPitchDirections) { Check(Mipmap32Surfaces(VxGenerateMipMap)); }
TEST_P(Mipmap32ReferenceTest, SingleColumnOutputIsTightForPaddedAndNegativePitch) { Check(Mipmap32Columns(VxGenerateMipMap, true)); }
TEST_P(Mipmap32ReferenceTest, TerminalPixelDoesNotWrite) {
    XBYTE pixel[4] = {1, 2, 3, 4}; Mipmap32Observations result;
    CheckMipmap32(VxGenerateMipMap, Mipmap32Desc(1, 1, 4, pixel), result);
    EXPECT_EQ(result.errors, 0u); EXPECT_EQ(result.pixels, 0u); EXPECT_EQ(result.guards, 1024u);
}
#ifdef _WIN32
TEST_P(Mipmap32ReferenceTest, GuardPagesBoundVectorLoadsStoresAndScalarTails) { Check(Mipmap32GuardPages(VxGenerateMipMap)); }
#endif
INSTANTIATE_TEST_SUITE_P(ActualDispatchModes, Mipmap32ReferenceTest, testing::Range(0, 6));

TEST(Mipmap32DispatchReferenceTest, RepeatedModeChangesUseTheNewTable) {
    const int previous = VxGetSIMDOverride(); unsigned effectiveModes = 0;
    for (int round = 0; round < 3; ++round) {
        for (int n = 0; n < 6; ++n) {
            const int mode = Mipmap32Modes[round % 2 ? 5 - n : n];
            ASSERT_TRUE(VxSetSIMDOverride(mode)); ASSERT_EQ(VxGetSIMDOverride(), mode);
            effectiveModes |= 1u << VxGetSIMDEffectiveBackend();
            EXPECT_EQ(Mipmap32Sums(VxGenerateMipMap).errors, 0u);
        }
    }
    testing::Test::RecordProperty("effective_modes", int(effectiveModes));
    EXPECT_TRUE(VxSetSIMDOverride(previous));
}
#endif
