#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include "VxMath.h"
#include "VxSIMDMode.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static VxImageDescEx DescriptorSample(XBYTE *pixels, XBYTE *palette) {
    VxImageDescEx d;
    d.Flags = 7; d.Width = 5; d.Height = 2; d.BytesPerLine = -24; d.BitsPerPixel = 32;
    d.RedMask = 0xff0000; d.GreenMask = 0xff00; d.BlueMask = 0xff; d.AlphaMask = 0xff000000;
    d.BytesPerColorEntry = 4; d.ColorMapEntries = 2; d.ColorMap = palette; d.Image = pixels;
    return d;
}

TEST(ImageDescriptorReferenceTest, ConstructorInitializesTheEntireNativeDescriptor) {
    alignas(VxImageDescEx) XBYTE storage[sizeof(VxImageDescEx)];
    std::memset(storage, 0xa5, sizeof(storage));
    VxImageDescEx *d = new (storage) VxImageDescEx;
    EXPECT_EQ(d->Size, sizeof(VxImageDescEx));
    for (size_t i = sizeof(d->Size); i < sizeof(storage); ++i) EXPECT_EQ(storage[i], 0);
    d->~VxImageDescEx();
}

TEST(ImageDescriptorReferenceTest, NativeLayoutKeepsTheOriginalScalarPrefixAndUnionAliases) {
    EXPECT_EQ(offsetof(VxImageDescEx, Size), 0u); EXPECT_EQ(offsetof(VxImageDescEx, Flags), 4u);
    EXPECT_EQ(offsetof(VxImageDescEx, Width), 8u); EXPECT_EQ(offsetof(VxImageDescEx, Height), 12u);
    EXPECT_EQ(offsetof(VxImageDescEx, BytesPerLine), 16u); EXPECT_EQ(offsetof(VxImageDescEx, TotalImageSize), 16u);
    EXPECT_EQ(offsetof(VxImageDescEx, BitsPerPixel), 20u);
    EXPECT_EQ(offsetof(VxImageDescEx, RedMask), 24u); EXPECT_EQ(offsetof(VxImageDescEx, BumpDuMask), 24u);
    EXPECT_EQ(offsetof(VxImageDescEx, GreenMask), 28u); EXPECT_EQ(offsetof(VxImageDescEx, BumpDvMask), 28u);
    EXPECT_EQ(offsetof(VxImageDescEx, BlueMask), 32u); EXPECT_EQ(offsetof(VxImageDescEx, BumpLumMask), 32u);
    EXPECT_EQ(offsetof(VxImageDescEx, AlphaMask), 36u);
    EXPECT_EQ(offsetof(VxImageDescEx, BytesPerColorEntry), 40u); EXPECT_EQ(offsetof(VxImageDescEx, ColorMapEntries), 42u);
    EXPECT_EQ(offsetof(VxImageDescEx, ColorMap), sizeof(void *) == 8 ? 48u : 44u);
    EXPECT_EQ(offsetof(VxImageDescEx, Image), sizeof(void *) == 8 ? 56u : 48u);
    EXPECT_EQ(sizeof(VxImageDescEx), sizeof(void *) == 8 ? 64u : 52u);
    EXPECT_EQ(alignof(VxImageDescEx), alignof(void *));
}

TEST(ImageDescriptorReferenceTest, SetCopiesFlagsAndBorrowedPointersWithoutTakingOwnership) {
    XBYTE pixels[48] = {}, palette[8] = {};
    VxImageDescEx src = DescriptorSample(pixels, palette), dst;
    dst.Size = 4; dst.Set(src);
    EXPECT_EQ(std::memcmp(&src, &dst, sizeof(src)), 0);
    EXPECT_EQ(dst.Image, pixels); EXPECT_EQ(dst.ColorMap, palette);
    dst.Image[3] = 17; dst.ColorMap[5] = 23;
    EXPECT_EQ(pixels[3], 17); EXPECT_EQ(palette[5], 23);
    EXPECT_EQ(src.Image, pixels); EXPECT_EQ(src.ColorMap, palette);
}

TEST(ImageDescriptorReferenceTest, ShortVersionsCopyOnlyTheirPrefixAndClearMissingFields) {
    XBYTE pixels[48], palette[8];
    VxImageDescEx src = DescriptorSample(pixels, palette), dst;
    const int sizes[] = {4, 8, 40, 44, static_cast<int>(offsetof(VxImageDescEx, Image))};
    for (int n = 0; n < 5; ++n) {
        src.Size = sizes[n]; std::memset(&dst, 0xa5, sizeof(dst));
        dst.Set(src); EXPECT_EQ(dst.Size, sizeof(dst));
        EXPECT_EQ(std::memcmp(reinterpret_cast<XBYTE *>(&dst) + 4, reinterpret_cast<XBYTE *>(&src) + 4, sizes[n] - 4), 0);
        const XBYTE *bytes = reinterpret_cast<const XBYTE *>(&dst);
        for (size_t i = sizes[n]; i < sizeof(dst); ++i) EXPECT_EQ(bytes[i], 0);
    }
}

TEST(ImageDescriptorReferenceTest, LargerVersionsKeepExistingFieldsAndNormalizeDestinationSize) {
    XBYTE pixels[48], palette[8];
    VxImageDescEx src, dst = DescriptorSample(pixels, palette);
    VxImageDescEx before = dst; src.Size = sizeof(src) + 16; dst.Size = 4;
    dst.Set(src);
    EXPECT_EQ(std::memcmp(&dst, &before, sizeof(dst)), 0);
}

TEST(ImageDescriptorReferenceTest, RepeatedSetClearsStalePointersThenRestoresTheFullDescriptor) {
    XBYTE pixels[48], palette[8];
    VxImageDescEx src = DescriptorSample(pixels, palette), dst;
    for (int call = 0; call < 4; ++call) {
        src.Size = sizeof(src); dst.Set(src); EXPECT_EQ(dst.Image, pixels); EXPECT_EQ(dst.ColorMap, palette);
        src.Size = 40; dst.Set(src); EXPECT_EQ(dst.Image, nullptr); EXPECT_EQ(dst.ColorMap, nullptr);
        EXPECT_EQ(dst.BytesPerColorEntry, 0); EXPECT_EQ(dst.ColorMapEntries, 0);
        EXPECT_EQ(dst.AlphaMask, 0xff000000u); EXPECT_EQ(dst.Width, 5);
    }
}

TEST(ImageDescriptorReferenceTest, ComparisonUsesElevenScalarPropertiesAndIgnoresFlagsAndPointers) {
    XBYTE first[48], second[48], palette[8];
    VxImageDescEx a = DescriptorSample(first, palette), b = a;
#define DESCRIPTOR_DIFFERENCE(field) \
    b = a; ++b.field; EXPECT_FALSE(a == b) << #field; EXPECT_TRUE(a != b) << #field; \
    EXPECT_FALSE(b == a) << #field; EXPECT_TRUE(b != a) << #field
    DESCRIPTOR_DIFFERENCE(Size); DESCRIPTOR_DIFFERENCE(Width); DESCRIPTOR_DIFFERENCE(Height);
    DESCRIPTOR_DIFFERENCE(BytesPerLine); DESCRIPTOR_DIFFERENCE(BitsPerPixel);
    DESCRIPTOR_DIFFERENCE(RedMask); DESCRIPTOR_DIFFERENCE(GreenMask); DESCRIPTOR_DIFFERENCE(BlueMask);
    DESCRIPTOR_DIFFERENCE(AlphaMask); DESCRIPTOR_DIFFERENCE(BytesPerColorEntry); DESCRIPTOR_DIFFERENCE(ColorMapEntries);
#undef DESCRIPTOR_DIFFERENCE
    b = a; b.Flags = _DXT5; b.Image = second; b.ColorMap = nullptr;
    EXPECT_TRUE(a == b); EXPECT_FALSE(a != b); EXPECT_TRUE(b == a); EXPECT_FALSE(b != a);
}

TEST(ImageDescriptorReferenceTest, AlphaQueryRetainsTheOriginalFlagThresholdContract) {
    const XDWORD flags[] = {0, 1, 18, 19, 20, 21, 22, 23, 24, 27, 0xffffffff};
    const bool expected[] = {false, false, false, true, true, true, true, true, true, true, true};
    VxImageDescEx d;
    for (int n = 0; n < 11; ++n) {
        d.Flags = flags[n]; d.AlphaMask = 0; EXPECT_EQ(d.HasAlpha() != 0, expected[n]);
        d.AlphaMask = 1; EXPECT_TRUE(d.HasAlpha());
        d.AlphaMask = 0x80000000; EXPECT_TRUE(d.HasAlpha());
    }
}

TEST(ImageDescriptorReferenceTest, CompressedAndBumpAliasesShareTheOriginalStorage) {
    VxImageDescEx d;
    d.TotalImageSize = 129; EXPECT_EQ(d.BytesPerLine, 129);
    d.BumpDuMask = 0x1f; d.BumpDvMask = 0x7e0; d.BumpLumMask = 0xf800;
    EXPECT_EQ(d.RedMask, 0x1fu); EXPECT_EQ(d.GreenMask, 0x7e0u); EXPECT_EQ(d.BlueMask, 0xf800u);
}

static void DescriptorProtectedPrefixes() {
    size_t page;
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SYSTEM_INFO info; GetSystemInfo(&info); page = info.dwPageSize;
    XBYTE *storage = static_cast<XBYTE *>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous;
    if (!storage || !VirtualProtect(storage + page, page, PAGE_NOACCESS, &previous)) std::_Exit(2);
#else
    page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *storage = static_cast<XBYTE *>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (storage == MAP_FAILED || mprotect(storage + page, page, PROT_NONE)) std::_Exit(2);
#endif
    const int sizes[] = {4, 8, 40, 44, static_cast<int>(offsetof(VxImageDescEx, Image)), sizeof(VxImageDescEx)};
    VxImageDescEx src, dst;
    src.Width = 5; src.Height = 2; src.BytesPerLine = 24; src.BitsPerPixel = 32;
    for (int n = 0; n < 6; ++n) {
        src.Size = sizes[n]; XBYTE *prefix = storage + page - sizes[n]; std::memcpy(prefix, &src, sizes[n]);
        dst.Set(*reinterpret_cast<const VxImageDescEx *>(prefix));
        if (dst.Size != sizeof(dst) || std::memcmp(reinterpret_cast<XBYTE *>(&dst) + 4, prefix + 4, sizes[n] - 4)) std::_Exit(3);
        for (size_t i = sizes[n]; i < sizeof(dst); ++i) if (reinterpret_cast<XBYTE *>(&dst)[i]) std::_Exit(4);
    }
    int futureSize = sizeof(src) + 16; std::memcpy(storage + page - 4, &futureSize, 4);
    dst.Set(*reinterpret_cast<const VxImageDescEx *>(storage + page - 4));
    std::_Exit(0);
}

TEST(ImageDescriptorReferenceTest, VersionedCopiesDoNotReadBeyondTheDeclaredSource) {
    EXPECT_EXIT(DescriptorProtectedPrefixes(), testing::ExitedWithCode(0), "");
}

class ImageDescriptorRuntimeTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        const int modes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
        previous = VxGetSIMDOverride(); ASSERT_TRUE(VxSetSIMDOverride(modes[GetParam()]));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), modes[GetParam()]);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(ImageDescriptorRuntimeTest, CopiedDescriptorKeepsBothPitchesAndImagePointersForBlit) {
    XBYTE input[48], output[40], expected[40];
    for (int i = 0; i < 48; ++i) input[i] = static_cast<XBYTE>(i * 7);
    std::memset(output, 0xa5, sizeof(output)); std::memcpy(expected, output, sizeof(output));
    VxImageDescEx src = DescriptorSample(input, nullptr), dst;
    src.Flags = 0; src.ColorMapEntries = src.BytesPerColorEntry = 0; src.BytesPerLine = 24;
    VxImageDescEx copied; copied.Set(src);
    dst.Set(copied); dst.Image = output; dst.BytesPerLine = 20; dst.BitsPerPixel = 24; dst.AlphaMask = 0;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 5; ++x) std::memcpy(expected + y * 20 + x * 3, input + y * 24 + x * 4, 3);
    VxDoBlit(copied, dst); EXPECT_EQ(std::memcmp(output, expected, sizeof(output)), 0);
    EXPECT_EQ(copied.Image, input); EXPECT_EQ(copied.BytesPerLine, 24);
}

TEST_P(ImageDescriptorRuntimeTest, CopiedDescriptorCarriesTheAlphaMaskToItsBorrowedImage) {
    XDWORD pixels[14], expected[14];
    for (int i = 0; i < 14; ++i) pixels[i] = expected[i] = 0x12345678u + i;
    VxImageDescEx src = DescriptorSample(reinterpret_cast<XBYTE *>(pixels), nullptr), copied;
    src.Flags = 0; src.BytesPerLine = 28; src.ColorMapEntries = src.BytesPerColorEntry = 0; copied.Set(src);
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 5; ++x) expected[y * 7 + x] = (expected[y * 7 + x] & 0xffffff) | 0x42000000;
    VxDoAlphaBlit(copied, static_cast<XBYTE>(0x42)); EXPECT_EQ(std::memcmp(pixels, expected, sizeof(pixels)), 0);
}

INSTANTIATE_TEST_SUITE_P(DispatchModes, ImageDescriptorRuntimeTest, testing::Range(0, 4));
