#include <gtest/gtest.h>
#include "VxMath.h"
#include "XP.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef VX_LIB
static bool FailArrayAllocation = false;
#endif

static const XDWORD Colors[] = {
    0xff102030, 0xffabcdef, 0xff334455, 0xffa0b0c0, 0xff778899, 0xfffedcba
};

static VxImageDescEx Image(void *pixels) {
    VxImageDescEx desc;
    desc.Width = 2; desc.Height = 3;
    desc.BitsPerPixel = 32; desc.BytesPerLine = 8;
    desc.RedMask = 0xff0000; desc.GreenMask = 0xff00;
    desc.BlueMask = 0xff; desc.AlphaMask = 0xff000000;
    desc.Image = static_cast<XBYTE *>(pixels);
    return desc;
}

class BitmapOwnershipTestBitmap {
public:
    BITMAP_HANDLE handle;
    BitmapOwnershipTestBitmap() : handle(VxCreateBitmap(Image(const_cast<XDWORD *>(Colors)))) {}
    ~BitmapOwnershipTestBitmap() { VxDeleteBitmap(handle); }
private:
    BitmapOwnershipTestBitmap(const BitmapOwnershipTestBitmap &);
    BitmapOwnershipTestBitmap &operator=(const BitmapOwnershipTestBitmap &);
};

static void ExpectColors(const VxImageDescEx &desc) {
    ASSERT_NE(nullptr, desc.Image);
    ASSERT_EQ(2, desc.Width); ASSERT_EQ(3, desc.Height);
    ASSERT_EQ(32, desc.BitsPerPixel); ASSERT_EQ(8, desc.BytesPerLine);
    EXPECT_EQ(0xff0000u, desc.RedMask); EXPECT_EQ(0xff00u, desc.GreenMask);
    EXPECT_EQ(0xffu, desc.BlueMask); EXPECT_EQ(0xff000000u, desc.AlphaMask);
    EXPECT_EQ(0, std::memcmp(Colors, desc.Image, sizeof(Colors)));
}

#ifdef VX_LIB
static int BitmapResources() {
#ifdef _WIN32
    return static_cast<int>(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
#else
    return SDL_GetNumAllocations();
#endif
}
#endif

#ifdef VX_LIB
void *operator new[](std::size_t bytes) {
    if (FailArrayAllocation) {
        FailArrayAllocation = false;
        throw std::bad_alloc();
    }
    void *result = std::malloc(bytes ? bytes : 1);
    if (!result) throw std::bad_alloc();
    return result;
}
void operator delete[](void *address) noexcept { std::free(address); }
void operator delete[](void *address, std::size_t) noexcept { std::free(address); }
#endif

TEST(BitmapOwnershipTest, ConversionsOwnIndependentTopDownPixelsAfterBitmapDeletion) {
    BitmapOwnershipTestBitmap bitmap;
    ASSERT_NE(nullptr, bitmap.handle);
    VxImageDescEx first, second;
    XAP<XBYTE> firstPixels(VxConvertBitmap(bitmap.handle, first));
    XAP<XBYTE> secondPixels(VxConvertBitmap(bitmap.handle, second));
    ExpectColors(first); ExpectColors(second);
    ASSERT_NE(first.Image, second.Image);
    first.Image[0] ^= 255;
    ExpectColors(second);
    VxDeleteBitmap(bitmap.handle); bitmap.handle = nullptr;
    ExpectColors(second);
}

TEST(BitmapOwnershipTest, CopyReadsBitmapWithoutChangingItAndConvertsColorMasks) {
    BitmapOwnershipTestBitmap bitmap;
    ASSERT_NE(nullptr, bitmap.handle);
    XDWORD output[6] = {};
    VxImageDescEx target = Image(output);
    target.RedMask = 0xff; target.BlueMask = 0xff0000;
    target.AlphaMask = 0;
    ASSERT_TRUE(VxCopyBitmap(bitmap.handle, target));
    for (int i = 0; i < 6; ++i) {
        const XDWORD expected = (Colors[i] & 0xff00) |
            ((Colors[i] & 0xff0000) >> 16) | ((Colors[i] & 0xff) << 16);
        EXPECT_EQ(expected, output[i]);
    }
    VxImageDescEx unchanged;
    XAP<XBYTE> pixels(VxConvertBitmap(bitmap.handle, unchanged));
    ExpectColors(unchanged);
}

#ifdef VX_LIB
TEST(BitmapOwnershipTest, PixelAllocationFailureReleasesTemporaryBitmapAndRetainsSource) {
    BitmapOwnershipTestBitmap bitmap;
    ASSERT_NE(nullptr, bitmap.handle);
#ifdef _WIN32
    // VxCreateBitmap returns 24-bit GDI storage. An external 32-bit bitmap
    // exercises the temporary bitmap allocated by VxConvertBitmapTo24.
    VxDeleteBitmap(bitmap.handle); bitmap.handle = nullptr;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 2; info.bmiHeader.biHeight = 3;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void *bits = nullptr;
    bitmap.handle = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ASSERT_NE(nullptr, bitmap.handle); ASSERT_NE(nullptr, bits);
    for (int row = 0; row < 3; ++row)
        std::memcpy(static_cast<XBYTE *>(bits) + row * 8, Colors + (2 - row) * 2, 8);
#endif
    VxImageDescEx desc;
    // Populate any SDL conversion cache before counting temporary resources.
    { XAP<XBYTE> warmup(VxConvertBitmap(bitmap.handle, desc)); ExpectColors(desc); }
    const int before = BitmapResources();
    for (int i = 0; i < 8; ++i) {
        bool threw = false;
        FailArrayAllocation = true;
        try { XAP<XBYTE> pixels(VxConvertBitmap(bitmap.handle, desc)); }
        catch (const std::bad_alloc &) { threw = true; }
        catch (...) { FailArrayAllocation = false; throw; }
        FailArrayAllocation = false;
        ASSERT_TRUE(threw);
        EXPECT_EQ(before, BitmapResources());
    }
    XAP<XBYTE> retained(VxConvertBitmap(bitmap.handle, desc));
    ExpectColors(desc);
}
#endif
