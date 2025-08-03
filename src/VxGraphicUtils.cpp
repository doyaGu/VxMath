#include <string.h>
#include <stdlib.h>

#include <intrin.h>

#include "VxMath.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

//------------------------------------------------------------------------------
// Global variables and constants
//------------------------------------------------------------------------------

static int g_QuantizationSamplingFactor = 15;

#define DXT1_BLOCK_SIZE 8
#define DXT3_BLOCK_SIZE 16
#define DXT5_BLOCK_SIZE 16

// Bit counting lookup table
static const XBYTE BIT_COUNT_TABLE[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

//------------------------------------------------------------------------------
// Bit manipulation functions
//------------------------------------------------------------------------------

XULONG GetBitCount(XULONG dwMask) {
    if (dwMask == 0) return 0;

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __popcnt(dwMask);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(dwMask);
#else
    return BIT_COUNT_TABLE[dwMask & 0xff] +
           BIT_COUNT_TABLE[(dwMask >> 8) & 0xff] +
           BIT_COUNT_TABLE[(dwMask >> 16) & 0xff] +
           BIT_COUNT_TABLE[(dwMask >> 24) & 0xff];
#endif
}

XULONG GetBitShift(XULONG dwMask) {
    if (!dwMask) return 0;

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    unsigned long index;
    _BitScanForward(&index, dwMask);
    return index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(dwMask);
#else
    // Fallback to standard approach
    XULONG shift = 0;
    while ((dwMask & 1) == 0) {
        shift++;
        dwMask >>= 1;
    }
    return shift;
#endif
}

void VxGetBitCounts(const VxImageDescEx &desc, XULONG &Rbits, XULONG &Gbits, XULONG &Bbits, XULONG &Abits) {
    Rbits = GetBitCount(desc.RedMask);
    Gbits = GetBitCount(desc.GreenMask);
    Bbits = GetBitCount(desc.BlueMask);
    Abits = GetBitCount(desc.AlphaMask);
}

void VxGetBitShifts(const VxImageDescEx &desc, XULONG &Rshift, XULONG &Gshift, XULONG &Bshift, XULONG &Ashift) {
    Rshift = GetBitShift(desc.RedMask);
    Gshift = GetBitShift(desc.GreenMask);
    Bshift = GetBitShift(desc.BlueMask);
    Ashift = GetBitShift(desc.AlphaMask);
}

//------------------------------------------------------------------------------
// Pixel format utilities
//------------------------------------------------------------------------------

struct PixelFormatInfo {
    XWORD BitsPerPixel;
    XULONG RedMask;
    XULONG GreenMask;
    XULONG BlueMask;
    XULONG AlphaMask;
    const char *Description;
    stbir_pixel_layout StbLayout;
};

static const PixelFormatInfo PixelFormatTable[] = {
    {0, 0, 0, 0, 0, "Unknown", STBIR_1CHANNEL},                                               // UNKNOWN_PF
    {32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, "32 bits ARGB 8888", STBIR_RGBA},    // _32_ARGB8888
    {32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, "32 bits RGB  888", STBIR_4CHANNEL}, // _32_RGB888
    {24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, "24 bits RGB  888", STBIR_RGB},      // _24_RGB888
    {16, 0xF800, 0x07E0, 0x001F, 0x0000, "16 bits RGB  565", STBIR_RGB},                      // _16_RGB565
    {16, 0x7C00, 0x03E0, 0x001F, 0x0000, "16 bits RGB  555", STBIR_RGB},                      // _16_RGB555
    {16, 0x7C00, 0x03E0, 0x001F, 0x8000, "16 bits ARGB 1555", STBIR_RGBA},                    // _16_ARGB1555
    {16, 0x0F00, 0x00F0, 0x000F, 0xF000, "16 bits ARGB 4444", STBIR_RGBA},                    // _16_ARGB4444
    {8, 0xE0, 0x1C, 0x03, 0x00, "8 bits RGB  332", STBIR_RGB},                                // _8_RGB332
    {8, 0x30, 0x0C, 0x03, 0xC0, "8 bits ARGB 2222", STBIR_RGBA},                              // _8_ARGB2222
    {32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000, "32 bits ABGR 8888", STBIR_ABGR},    // _32_ABGR8888
    {32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF, "32 bits RGBA 8888", STBIR_RGBA},    // _32_RGBA8888
    {32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF, "32 bits BGRA 8888", STBIR_BGRA},    // _32_BGRA8888
    {32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000, "32 bits BGR  888", STBIR_4CHANNEL}, // _32_BGR888
    {24, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000, "24 bits BGR  888", STBIR_BGR},      // _24_BGR888
    {16, 0x001F, 0x07E0, 0xF800, 0x0000, "16 bits BGR  565", STBIR_BGR},                      // _16_BGR565
    {16, 0x001F, 0x03E0, 0x7C00, 0x0000, "16 bits BGR  555", STBIR_BGR},                      // _16_BGR555
    {16, 0x001F, 0x03E0, 0x7C00, 0x8000, "16 bits ABGR 1555", STBIR_ABGR},                    // _16_ABGR1555
    {16, 0x000F, 0x00F0, 0x0F00, 0xF000, "16 bits ABGR 4444", STBIR_ABGR},                    // _16_ABGR4444
    // DXT formats (compressed formats)
    {4, 0, 0, 0, 0, "Compressed DXT1", STBIR_1CHANNEL}, // _DXT1
    {8, 0, 0, 0, 0, "Compressed DXT2", STBIR_1CHANNEL}, // _DXT2
    {8, 0, 0, 0, 0, "Compressed DXT3", STBIR_1CHANNEL}, // _DXT3
    {8, 0, 0, 0, 0, "Compressed DXT4", STBIR_1CHANNEL}, // _DXT4
    {8, 0, 0, 0, 0, "Compressed DXT5", STBIR_1CHANNEL}, // _DXT5
    // Bump map formats
    {16, 0x00FF, 0xFF00, 0x0000, 0x0000, "V8U8 BumpMap", STBIR_2CHANNEL},                  // _16_V8U8
    {32, 0xFFFF, 0xFFFF0000, 0x0000, 0x0000, "V16U16 BumpMap", STBIR_2CHANNEL},            // _32_V16U16
    {16, 0x001F, 0x03E0, 0x7C00, 0x0000, "L6V5U5 BumpMap", STBIR_RGB},                     // _16_L6V5U5
    {32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000, "L8V8U8 BumpMap", STBIR_4CHANNEL} // _32_X8L8V8U8
};

VX_PIXELFORMAT VxImageDesc2PixelFormat(const VxImageDescEx &desc) {
    // Fast path for compressed/bump formats
    if (desc.Flags >= _DXT1 && desc.Flags <= _DXT5) return static_cast<VX_PIXELFORMAT>(desc.Flags);
    if (desc.Flags >= _16_V8U8 && desc.Flags <= _32_X8L8V8U8) return static_cast<VX_PIXELFORMAT>(desc.Flags);

    // Cache values for better performance
    const XULONG bpp = desc.BitsPerPixel;
    const XULONG rmask = desc.RedMask;
    const XULONG gmask = desc.GreenMask;
    const XULONG bmask = desc.BlueMask;
    const XULONG amask = desc.AlphaMask;

    // Linear search with early termination
    for (int i = 1; i < _16_V8U8; ++i) {
        const PixelFormatInfo &info = PixelFormatTable[i];
        if (info.BitsPerPixel == bpp &&
            info.RedMask == rmask &&
            info.GreenMask == gmask &&
            info.BlueMask == bmask &&
            info.AlphaMask == amask) {
            return static_cast<VX_PIXELFORMAT>(i);
        }
    }
    return UNKNOWN_PF;
}

void VxPixelFormat2ImageDesc(VX_PIXELFORMAT Pf, VxImageDescEx &desc) {
    if (Pf < 0 || Pf >= MAX_PIXEL_FORMATS) return;

    if (Pf >= _DXT1 && Pf <= _DXT5) {
        // Compressed formats
        desc.Flags = Pf;
        desc.BitsPerPixel = (Pf == _DXT1) ? 4 : 8;

        const int blocksPerRow = (desc.Width + 3) / 4;
        const int bytesPerBlock = (Pf == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;
        desc.BytesPerLine = blocksPerRow * bytesPerBlock;
        desc.TotalImageSize = blocksPerRow * ((desc.Height + 3) / 4) * bytesPerBlock;
        desc.RedMask = desc.GreenMask = desc.BlueMask = desc.AlphaMask = 0;
    } else if (Pf >= _16_V8U8 && Pf <= _32_X8L8V8U8) {
        // Bump map formats
        desc.Flags = Pf;
        const PixelFormatInfo &info = PixelFormatTable[Pf];
        desc.BitsPerPixel = info.BitsPerPixel;
        desc.RedMask = info.RedMask;
        desc.GreenMask = info.GreenMask;
        desc.BlueMask = info.BlueMask;
        desc.AlphaMask = info.AlphaMask;
        desc.BytesPerLine = ((desc.Width * ((desc.BitsPerPixel + 7) / 8)) + 3) & ~3;
    } else {
        // Standard formats
        const PixelFormatInfo &info = PixelFormatTable[Pf];
        desc.BitsPerPixel = info.BitsPerPixel;
        desc.RedMask = info.RedMask;
        desc.GreenMask = info.GreenMask;
        desc.BlueMask = info.BlueMask;
        desc.AlphaMask = info.AlphaMask;
        desc.Flags = 0;
        desc.BytesPerLine = ((desc.Width * ((desc.BitsPerPixel + 7) / 8)) + 3) & ~3;
    }
}

const char *VxPixelFormat2String(VX_PIXELFORMAT Pf) {
    return (Pf >= 0 && Pf < MAX_PIXEL_FORMATS) ? PixelFormatTable[Pf].Description : "";
}

void VxBppToMask(VxImageDescEx &desc) {
    switch (desc.BitsPerPixel) {
    case 8:
        desc.RedMask = 0xE0;   // 111 00000
        desc.GreenMask = 0x1C; // 000 11100
        desc.BlueMask = 0x03;  // 000 00011
        desc.AlphaMask = 0x00;
        break;
    case 15:
    case 16:
        desc.RedMask = 0x7C00;   // 0111 1100 0000 0000
        desc.GreenMask = 0x03E0; // 0000 0011 1110 0000
        desc.BlueMask = 0x001F;  // 0000 0000 0001 1111
        desc.AlphaMask = 0x0000;
        break;
    case 24:
    case 32:
        desc.RedMask = 0x00FF0000;
        desc.GreenMask = 0x0000FF00;
        desc.BlueMask = 0x000000FF;
        desc.AlphaMask = (desc.BitsPerPixel == 32) ? 0xFF000000 : 0x00000000;
        break;
    default:
        // Unsupported bit depth, don't change anything
        break;
    }
}

stbir_pixel_layout GetStbPixelLayout(const VxImageDescEx &desc) {
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(desc);
    if (format != UNKNOWN_PF) return PixelFormatTable[format].StbLayout;

    const int channelCount = desc.BitsPerPixel / 8;
    switch (channelCount) {
    case 1:
        return STBIR_1CHANNEL;
    case 2:
        return STBIR_2CHANNEL;
    case 3:
        if (desc.RedMask == 0x00FF0000 && desc.GreenMask == 0x0000FF00 && desc.BlueMask == 0x000000FF)
            return STBIR_RGB;
        else if (desc.RedMask == 0x000000FF && desc.GreenMask == 0x0000FF00 && desc.BlueMask == 0x00FF0000)
            return STBIR_BGR;
        else
            return STBIR_RGB; // Default
    case 4:
        if (desc.AlphaMask) {
            if (desc.RedMask == 0x00FF0000 && desc.GreenMask == 0x0000FF00 &&
                desc.BlueMask == 0x000000FF && desc.AlphaMask == 0xFF000000)
                return STBIR_RGBA;
            else if (desc.RedMask == 0x000000FF && desc.GreenMask == 0x0000FF00 &&
                desc.BlueMask == 0x00FF0000 && desc.AlphaMask == 0xFF000000)
                return STBIR_BGRA;
            else if (desc.RedMask == 0x0000FF00 && desc.GreenMask == 0x00FF0000 &&
                desc.BlueMask == 0xFF000000 && desc.AlphaMask == 0x000000FF)
                return STBIR_ABGR;
            else if (desc.RedMask == 0xFF000000 && desc.GreenMask == 0x00FF0000 &&
                desc.BlueMask == 0x0000FF00 && desc.AlphaMask == 0x000000FF)
                return STBIR_ARGB;
            else
                return STBIR_RGBA; // Default with alpha
        } else {
            return STBIR_4CHANNEL; // 4 channels but not using alpha
        }
    default:
        return STBIR_1CHANNEL; // Fallback
    }
}

//------------------------------------------------------------------------------
// Pixel conversion utilities
//------------------------------------------------------------------------------

inline XULONG ReadPixel(const XBYTE *ptr, int bpp) {
    switch (bpp) {
    case 1: return *ptr;
    case 2: return *(const XWORD *) ptr;
    case 3: return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
    case 4: return *(const XULONG *) ptr;
    default: return 0;
    }
}

inline void WritePixel(XBYTE *ptr, int bpp, XULONG pixel) {
    switch (bpp) {
    case 1: *ptr = (XBYTE) pixel;
        break;
    case 2: *(XWORD *) ptr = (XWORD) pixel;
        break;
    case 3:
        ptr[0] = (XBYTE) pixel;
        ptr[1] = (XBYTE) (pixel >> 8);
        ptr[2] = (XBYTE) (pixel >> 16);
        break;
    case 4: *(XULONG *) ptr = pixel;
        break;
    }
}

inline int SafePixelOffset(const VxImageDescEx &desc, int x, int y, int bytesPerPixel) {
    if (x >= desc.Width || y >= desc.Height || bytesPerPixel <= 0 || bytesPerPixel > 4 || !desc.Image)
        return -1;

    const XULONG totalOffset = (XULONG) y * desc.BytesPerLine + (XULONG) x * bytesPerPixel;
    return (totalOffset <= INT_MAX) ? (int) totalOffset : -1;
}

inline XULONG ReadPixelSafe(const VxImageDescEx &desc, int x, int y, int bpp) {
    const int offset = SafePixelOffset(desc, x, y, bpp);
    return (offset >= 0) ? ReadPixel(desc.Image + offset, bpp) : 0;
}

inline XBOOL WritePixelSafe(const VxImageDescEx &desc, int x, int y, int bpp, XULONG pixel) {
    const int offset = SafePixelOffset(desc, x, y, bpp);
    if (offset < 0) return FALSE;
    WritePixel(desc.Image + offset, bpp, pixel);
    return TRUE;
}

inline void ExtractRGBA(XULONG pixel, const VxImageDescEx &desc, XBYTE &r, XBYTE &g, XBYTE &b, XBYTE &a) {
    XULONG rShift = GetBitShift(desc.RedMask);
    XULONG gShift = GetBitShift(desc.GreenMask);
    XULONG bShift = GetBitShift(desc.BlueMask);
    XULONG aShift = GetBitShift(desc.AlphaMask);

    XULONG rBits = GetBitCount(desc.RedMask);
    XULONG gBits = GetBitCount(desc.GreenMask);
    XULONG bBits = GetBitCount(desc.BlueMask);
    XULONG aBits = GetBitCount(desc.AlphaMask);

    // Extract components
    r = desc.RedMask ? ((pixel & desc.RedMask) >> rShift) : 0;
    g = desc.GreenMask ? ((pixel & desc.GreenMask) >> gShift) : 0;
    b = desc.BlueMask ? ((pixel & desc.BlueMask) >> bShift) : 0;
    a = desc.AlphaMask ? ((pixel & desc.AlphaMask) >> aShift) : 255;

    // Scale to 8-bit range if needed
    if (rBits > 0 && rBits < 8)
        r = (r * 255) / ((1 << rBits) - 1);

    if (gBits > 0 && gBits < 8)
        g = (g * 255) / ((1 << gBits) - 1);

    if (bBits > 0 && bBits < 8)
        b = (b * 255) / ((1 << bBits) - 1);

    if (aBits > 0 && aBits < 8)
        a = (a * 255) / ((1 << aBits) - 1);
}

static void ExtractRGBAFromPixel(XULONG pixel, const VxImageDescEx &desc, XBYTE &r, XBYTE &g, XBYTE &b, XBYTE &a) {
    // Extract using masks and shifts
    XULONG rShift = GetBitShift(desc.RedMask);
    XULONG gShift = GetBitShift(desc.GreenMask);
    XULONG bShift = GetBitShift(desc.BlueMask);
    XULONG aShift = GetBitShift(desc.AlphaMask);

    XULONG rBits = GetBitCount(desc.RedMask);
    XULONG gBits = GetBitCount(desc.GreenMask);
    XULONG bBits = GetBitCount(desc.BlueMask);
    XULONG aBits = GetBitCount(desc.AlphaMask);

    // Extract components
    XULONG rVal = desc.RedMask ? ((pixel & desc.RedMask) >> rShift) : 0;
    XULONG gVal = desc.GreenMask ? ((pixel & desc.GreenMask) >> gShift) : 0;
    XULONG bVal = desc.BlueMask ? ((pixel & desc.BlueMask) >> bShift) : 0;
    XULONG aVal = desc.AlphaMask ? ((pixel & desc.AlphaMask) >> aShift) : ((1 << 8) - 1);

    // Scale to 8-bit range
    r = (rBits > 0 && rBits < 8) ? (XBYTE) ((rVal * 255) / ((1 << rBits) - 1)) : (XBYTE) rVal;
    g = (gBits > 0 && gBits < 8) ? (XBYTE) ((gVal * 255) / ((1 << gBits) - 1)) : (XBYTE) gVal;
    b = (bBits > 0 && bBits < 8) ? (XBYTE) ((bVal * 255) / ((1 << bBits) - 1)) : (XBYTE) bVal;
    a = (aBits > 0 && aBits < 8) ? (XBYTE) ((aVal * 255) / ((1 << aBits) - 1)) : (XBYTE) aVal;
}

inline XULONG CreatePixel(XBYTE r, XBYTE g, XBYTE b, XBYTE a, const VxImageDescEx &desc) {
    const XULONG rShift = GetBitShift(desc.RedMask);
    const XULONG gShift = GetBitShift(desc.GreenMask);
    const XULONG bShift = GetBitShift(desc.BlueMask);
    const XULONG aShift = GetBitShift(desc.AlphaMask);

    const XULONG rBits = GetBitCount(desc.RedMask);
    const XULONG gBits = GetBitCount(desc.GreenMask);
    const XULONG bBits = GetBitCount(desc.BlueMask);
    const XULONG aBits = GetBitCount(desc.AlphaMask);

    const XULONG rVal = (rBits < 8) ? (r * ((1 << rBits) - 1)) / 255 : r;
    const XULONG gVal = (gBits < 8) ? (g * ((1 << gBits) - 1)) / 255 : g;
    const XULONG bVal = (bBits < 8) ? (b * ((1 << bBits) - 1)) / 255 : b;
    const XULONG aVal = (aBits < 8) ? (a * ((1 << aBits) - 1)) / 255 : a;

    XULONG pixel = 0;
    pixel |= desc.RedMask ? ((rVal << rShift) & desc.RedMask) : 0;
    pixel |= desc.GreenMask ? ((gVal << gShift) & desc.GreenMask) : 0;
    pixel |= desc.BlueMask ? ((bVal << bShift) & desc.BlueMask) : 0;
    pixel |= desc.AlphaMask ? ((aVal << aShift) & desc.AlphaMask) : 0;

    return pixel;
}

inline XULONG ConvertPixel(XULONG srcPixel, const VxImageDescEx &srcDesc, const VxImageDescEx &dstDesc) {
    // Fast path for identical formats
    if (srcDesc.RedMask == dstDesc.RedMask && srcDesc.GreenMask == dstDesc.GreenMask &&
        srcDesc.BlueMask == dstDesc.BlueMask && srcDesc.AlphaMask == dstDesc.AlphaMask) {
        return srcPixel;
    }

    XBYTE r, g, b, a;
    ExtractRGBA(srcPixel, srcDesc, r, g, b, a);
    return CreatePixel(r, g, b, a, dstDesc);
}

//------------------------------------------------------------------------------
// DXT Compression Utilities
//------------------------------------------------------------------------------

// 565 conversion lookup tables
static const XBYTE R5_TO_R8[32] = {
    0, 8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99, 107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255
};
static const XBYTE G6_TO_G8[64] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 45, 49, 53, 57, 61,
    65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125,
    130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
    194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
};
static const XBYTE B5_TO_B8[32] = {
    0, 8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99, 107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255
};

inline void DecompressColor565(XWORD color, XBYTE *rgb) {
    rgb[0] = R5_TO_R8[(color >> 11) & 0x1F];
    rgb[1] = G6_TO_G8[(color >> 5) & 0x3F];
    rgb[2] = B5_TO_B8[color & 0x1F];
}

inline int CalculateDXTSize(int width, int height, VX_PIXELFORMAT format) {
    if (width <= 0 || height <= 0 || format < _DXT1 || format > _DXT5) return 0;

    const int blocksWide = (width + 3) / 4;
    const int blocksHigh = (height + 3) / 4;

    if (blocksWide > INT_MAX / blocksHigh) return 0;
    const int numBlocks = blocksWide * blocksHigh;
    const int bytesPerBlock = (format == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;

    return (numBlocks > INT_MAX / bytesPerBlock) ? 0 : numBlocks * bytesPerBlock;
}

inline XBOOL ValidateImageDesc(const VxImageDescEx &desc) {
    return desc.Image && desc.Width > 0 && desc.Height > 0 &&
        desc.BitsPerPixel > 0 && desc.BitsPerPixel <= 32 &&
        desc.BytesPerLine > 0 && (desc.Height <= INT_MAX / desc.BytesPerLine);
}

XBOOL VxConvertToDXT(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc, XBOOL highQuality) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) return FALSE;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) return FALSE;
    if (dstFormat < _DXT1 || dstFormat > _DXT5) return FALSE;
    if (src_desc.BitsPerPixel < 8 || (!src_desc.RedMask && !src_desc.GreenMask && !src_desc.BlueMask)) return FALSE;

    const int expectedSize = CalculateDXTSize(src_desc.Width, src_desc.Height, dstFormat);
    if (expectedSize <= 0 || dst_desc.TotalImageSize < expectedSize) return FALSE;

    const int blockSize = (dstFormat == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;
    const int blocksWide = (src_desc.Width + 3) / 4;
    const int blocksHigh = (src_desc.Height + 3) / 4;

    if (blocksWide <= 0 || blocksHigh <= 0 || blocksWide > INT_MAX / blocksHigh) return FALSE;

    const bool hasAlpha = (dstFormat != _DXT1) || (src_desc.AlphaMask != 0);
    const int mode = highQuality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;
    const int srcBpp = src_desc.BitsPerPixel / 8;

    if (srcBpp <= 0 || srcBpp > 4) return FALSE;

    unsigned char block[64]; // 16 pixels * 4 bytes = 64 bytes

    for (int by = 0; by < blocksHigh; by++) {
        for (int bx = 0; bx < blocksWide; bx++) {
            // Extract 4x4 block
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    const int srcX = (bx * 4 + x < src_desc.Width) ? bx * 4 + x : src_desc.Width - 1;
                    const int srcY = (by * 4 + y < src_desc.Height) ? by * 4 + y : src_desc.Height - 1;

                    const int srcOffset = SafePixelOffset(src_desc, srcX, srcY, srcBpp);
                    const int blockIdx = (y * 4 + x) * 4;

                    if (srcOffset < 0) {
                        block[blockIdx] = block[blockIdx + 1] = block[blockIdx + 2] = block[blockIdx + 3] = 0;
                        continue;
                    }

                    XULONG pixel = ReadPixel(src_desc.Image + srcOffset, srcBpp);

                    unsigned char r, g, b, a;
                    ExtractRGBAFromPixel(pixel, src_desc, r, g, b, a);

                    block[blockIdx] = r;
                    block[blockIdx + 1] = g;
                    block[blockIdx + 2] = b;
                    block[blockIdx + 3] = a;
                }
            }

            const int blockIndex = by * blocksWide + bx;
            const XULONG dstOffset = (XULONG) blockIndex * blockSize;

            if (dstOffset < 0 || dstOffset + blockSize > dst_desc.TotalImageSize) return FALSE;

            stb_compress_dxt_block(dst_desc.Image + dstOffset, block, hasAlpha ? 1 : 0, mode);
        }
    }

    return TRUE;
}

// DXT decompression alpha functions
static void DecompressDXT3Alpha(const XBYTE *blockPtr, XBYTE *alphaValues) {
    if (!blockPtr || !alphaValues) {
        if (alphaValues) {
            for (int i = 0; i < 16; i++) alphaValues[i] = 255;
        }
        return;
    }

    for (int i = 0; i < 8; i++) {
        const XBYTE alphaByte = blockPtr[i];
        const int idx1 = i * 2;
        const int idx2 = i * 2 + 1;
        if (idx1 < 16) alphaValues[idx1] = ((alphaByte & 0x0F) * 255) / 15; // Lower 4 bits
        if (idx2 < 16) alphaValues[idx2] = ((alphaByte >> 4) * 255) / 15; // Upper 4 bits
    }
}

static void DecompressDXT5Alpha(const XBYTE *blockPtr, XBYTE *alphaValues) {
    if (!blockPtr || !alphaValues) {
        if (alphaValues) {
            for (int i = 0; i < 16; i++) alphaValues[i] = 255;
        }
        return;
    }

    const XBYTE alpha0 = blockPtr[0];
    const XBYTE alpha1 = blockPtr[1];

    XBYTE alphaLUT[8];
    alphaLUT[0] = alpha0;
    alphaLUT[1] = alpha1;

    if (alpha0 > alpha1) {
        for (int i = 2; i < 8; i++) {
            alphaLUT[i] = (XBYTE) (((8 - i) * alpha0 + (i - 1) * alpha1) / 7);
        }
    } else {
        for (int i = 2; i < 6; i++) {
            alphaLUT[i] = (XBYTE) (((6 - i) * alpha0 + (i - 1) * alpha1) / 5);
        }
        alphaLUT[6] = 0;
        alphaLUT[7] = 255;
    }

    XULONG alphaIndices = 0;
    for (int i = 0; i < 6; i++) {
        alphaIndices |= ((XULONG) blockPtr[i + 2] << (8 * i));
    }

    for (int i = 0; i < 16; i++) {
        const XDWORD alphaIndex = (XDWORD) ((alphaIndices >> (3 * i)) & 0x07);
        alphaValues[i] = (alphaIndex < 8) ? alphaLUT[alphaIndex] : 255;
    }
}

XBOOL VxDecompressDXT(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) return FALSE;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    if (srcFormat < _DXT1 || srcFormat > _DXT5) return FALSE;
    if (dstFormat >= _DXT1 && dstFormat <= _DXT5) return FALSE;
    if (dst_desc.BitsPerPixel < 8) return FALSE;
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) return FALSE;

    const int blockSize = (srcFormat == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;
    const int blocksWide = (src_desc.Width + 3) / 4;
    const int blocksHigh = (src_desc.Height + 3) / 4;
    const int expectedSize = blocksWide * blocksHigh * blockSize;

    if (src_desc.TotalImageSize < expectedSize) return FALSE;

    const int dstBpp = dst_desc.BitsPerPixel / 8;
    if (dstBpp <= 0 || dstBpp > 4) return FALSE;

    for (int by = 0; by < blocksHigh; by++) {
        for (int bx = 0; bx < blocksWide; bx++) {
            const int blockIndex = by * blocksWide + bx;
            if (blockIndex * blockSize + blockSize > src_desc.TotalImageSize) return FALSE;

            const XBYTE *blockPtr = src_desc.Image + blockIndex * blockSize;
            const XBYTE *colorPtr = (srcFormat == _DXT1) ? blockPtr : (blockPtr + 8);

            const XWORD color0 = colorPtr[0] | (colorPtr[1] << 8);
            const XWORD color1 = colorPtr[2] | (colorPtr[3] << 8);
            const XDWORD colorIndices = colorPtr[4] | (colorPtr[5] << 8) | (colorPtr[6] << 16) | (colorPtr[7] << 24);

            XBYTE refColors[4][4];

            DecompressColor565(color0, refColors[0]);
            refColors[0][3] = 255;
            DecompressColor565(color1, refColors[1]);
            refColors[1][3] = 255;

            if (srcFormat == _DXT1 && color0 <= color1) {
                // 3-color mode
                for (int i = 0; i < 3; i++) {
                    refColors[2][i] = (XBYTE) ((refColors[0][i] + refColors[1][i]) / 2);
                }
                refColors[2][3] = 255;
                refColors[3][0] = refColors[3][1] = refColors[3][2] = refColors[3][3] = 0;
            } else {
                // 4-color mode
                for (int i = 0; i < 3; i++) {
                    refColors[2][i] = (XBYTE) ((2 * refColors[0][i] + refColors[1][i]) / 3);
                    refColors[3][i] = (XBYTE) ((refColors[0][i] + 2 * refColors[1][i]) / 3);
                }
                refColors[2][3] = refColors[3][3] = 255;
            }

            XBYTE alphaValues[16];
            for (int i = 0; i < 16; i++) alphaValues[i] = 255;

            if (srcFormat == _DXT3) {
                DecompressDXT3Alpha(blockPtr, alphaValues);
            } else if (srcFormat == _DXT5) {
                DecompressDXT5Alpha(blockPtr, alphaValues);
            }

            // Write pixels
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    const int pixelX = bx * 4 + x;
                    const int pixelY = by * 4 + y;

                    if (pixelX >= dst_desc.Width || pixelY >= dst_desc.Height) continue;

                    const int pixelIdx = y * 4 + x;
                    int colorIdx = (colorIndices >> (2 * pixelIdx)) & 0x3;
                    if (colorIdx > 3) colorIdx = 0;

                    XBYTE r = refColors[colorIdx][0];
                    XBYTE g = refColors[colorIdx][1];
                    XBYTE b = refColors[colorIdx][2];
                    XBYTE a = refColors[colorIdx][3];

                    if (srcFormat == _DXT3 || srcFormat == _DXT5) {
                        a = alphaValues[pixelIdx];
                    } else if (srcFormat == _DXT1 && colorIdx == 3 && color0 <= color1) {
                        a = 0;
                    }

                    const int dstOffset = SafePixelOffset(dst_desc, pixelX, pixelY, dstBpp);
                    if (dstOffset < 0) continue;

                    const XULONG destPixel = CreatePixel(r, g, b, a, dst_desc);
                    WritePixel(dst_desc.Image + dstOffset, dstBpp, destPixel);
                }
            }
        }
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// Image blitting and conversion functions
//------------------------------------------------------------------------------

static void DirectMemoryBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(src_desc);

    if (format >= _DXT1 && format <= _DXT5) {
        memcpy(dst_desc.Image, src_desc.Image, src_desc.TotalImageSize);
        return;
    }

    if (src_desc.Width == dst_desc.Width && src_desc.BytesPerLine == dst_desc.BytesPerLine) {
        memcpy(dst_desc.Image, src_desc.Image, src_desc.BytesPerLine * src_desc.Height);
        return;
    }

    const int bytesPerRow = src_desc.Width * (src_desc.BitsPerPixel / 8);
    const XBYTE *srcPtr = src_desc.Image;
    XBYTE *dstPtr = dst_desc.Image;

    for (int y = 0; y < src_desc.Height; y++) {
        memcpy(dstPtr, srcPtr, bytesPerRow);
        srcPtr += src_desc.BytesPerLine;
        dstPtr += dst_desc.BytesPerLine;
    }
}

static void ConvertFormats(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) return;
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) return;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // Handle DXT cases
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) {
        if (dstFormat >= _DXT1 && dstFormat <= _DXT5) return;
        VxDecompressDXT(src_desc, dst_desc);
        return;
    } else if (dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        VxConvertToDXT(src_desc, dst_desc, TRUE);
        return;
    }

    const int srcBpp = src_desc.BitsPerPixel / 8;
    const int dstBpp = dst_desc.BitsPerPixel / 8;
    if (srcBpp <= 0 || srcBpp > 4 || dstBpp <= 0 || dstBpp > 4) return;

    // 32-bit to 32-bit conversion
    if (srcBpp == 4 && dstBpp == 4) {
        for (int y = 0; y < src_desc.Height; y++) {
            if (y * src_desc.BytesPerLine + src_desc.Width * 4 > src_desc.BytesPerLine * src_desc.Height ||
                y * dst_desc.BytesPerLine + dst_desc.Width * 4 > dst_desc.BytesPerLine * dst_desc.Height) {
                continue;
            }

            const XULONG *srcLine = (const XULONG *) (src_desc.Image + y * src_desc.BytesPerLine);
            XULONG *dstLine = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);

            for (int x = 0; x < src_desc.Width; x++) {
                dstLine[x] = ConvertPixel(srcLine[x], src_desc, dst_desc);
            }
        }
        return;
    }

    // General case
    for (int y = 0; y < src_desc.Height; y++) {
        for (int x = 0; x < src_desc.Width; x++) {
            const XULONG pixel = ReadPixelSafe(src_desc, x, y, srcBpp);
            const XULONG dstPixel = ConvertPixel(pixel, src_desc, dst_desc);
            WritePixelSafe(dst_desc, x, y, dstBpp, dstPixel);
        }
    }
}

void VxDoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) return;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // Fast path for identical format and dimensions
    if (srcFormat == dstFormat && srcFormat != UNKNOWN_PF &&
        src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
        DirectMemoryBlit(src_desc, dst_desc);
        return;
    }

    // Handle DXT format cases
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 || dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        if (srcFormat < _DXT1 && dstFormat >= _DXT1 && dstFormat <= _DXT5) {
            // Convert to DXT
            if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
                VxConvertToDXT(src_desc, dst_desc, TRUE);
            } else {
                // Resize then compress
                VxImageDescEx tempDesc;
                tempDesc.Width = dst_desc.Width;
                tempDesc.Height = dst_desc.Height;
                tempDesc.BitsPerPixel = 32;
                tempDesc.BytesPerLine = dst_desc.Width * 4;
                tempDesc.RedMask = R_MASK;
                tempDesc.GreenMask = G_MASK;
                tempDesc.BlueMask = B_MASK;
                tempDesc.AlphaMask = A_MASK;

                if (tempDesc.Height > 0 && tempDesc.BytesPerLine > INT_MAX / tempDesc.Height) return;

                XBYTE *tempBuffer = new XBYTE[tempDesc.BytesPerLine * tempDesc.Height];
                if (!tempBuffer) return;

                tempDesc.Image = tempBuffer;

                stbir_resize_uint8_linear(
                    src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
                    tempDesc.Image, tempDesc.Width, tempDesc.Height, tempDesc.BytesPerLine,
                    GetStbPixelLayout(src_desc)
                );

                VxConvertToDXT(tempDesc, dst_desc, TRUE);
                delete[] tempBuffer;
            }
            return;
        } else if (srcFormat >= _DXT1 && srcFormat <= _DXT5 && dstFormat < _DXT1) {
            // Decompress DXT
            VxImageDescEx tempDesc;
            tempDesc.Width = src_desc.Width;
            tempDesc.Height = src_desc.Height;
            tempDesc.BitsPerPixel = 32;
            tempDesc.BytesPerLine = src_desc.Width * 4;
            tempDesc.RedMask = R_MASK;
            tempDesc.GreenMask = G_MASK;
            tempDesc.BlueMask = B_MASK;
            tempDesc.AlphaMask = A_MASK;

            if (tempDesc.Height > 0 && tempDesc.BytesPerLine > INT_MAX / tempDesc.Height) return;

            XBYTE *tempBuffer = new XBYTE[tempDesc.BytesPerLine * tempDesc.Height];
            if (!tempBuffer) return;

            tempDesc.Image = tempBuffer;

            if (VxDecompressDXT(src_desc, tempDesc)) {
                if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
                    ConvertFormats(tempDesc, dst_desc);
                } else {
                    stbir_resize_uint8_linear(
                        tempDesc.Image, tempDesc.Width, tempDesc.Height, tempDesc.BytesPerLine,
                        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
                        GetStbPixelLayout(tempDesc)
                    );
                }
            }

            delete[] tempBuffer;
            return;
        }

        return; // Can't convert between DXT formats
    }

    // Same dimensions, different format
    if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
        ConvertFormats(src_desc, dst_desc);
        return;
    }

    // General resize with format conversion
    const stbir_pixel_layout srcLayout = GetStbPixelLayout(src_desc);
    const stbir_datatype dataType = (src_desc.BitsPerPixel <= 8) ? STBIR_TYPE_UINT8
                                        : (src_desc.BitsPerPixel == 16) ? STBIR_TYPE_UINT16 : STBIR_TYPE_UINT8;

    stbir_resize(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
        srcLayout, dataType, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL
    );
}

void VxDoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!src_desc.Image || !dst_desc.Image) return;
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) return;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // DXT not supported for flipping
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 || dstFormat >= _DXT1 && dstFormat <= _DXT5) return;

    // Fast path for identical format
    if (srcFormat == dstFormat && srcFormat != UNKNOWN_PF) {
        const int bytesPerLine = src_desc.BytesPerLine;
        for (int y = 0; y < src_desc.Height; y++) {
            const XBYTE *srcLine = src_desc.Image + y * bytesPerLine;
            XBYTE *dstLine = dst_desc.Image + (src_desc.Height - 1 - y) * bytesPerLine;
            memcpy(dstLine, srcLine, bytesPerLine);
        }
        return;
    }

    // Different format conversion
    const int srcBpp = src_desc.BitsPerPixel / 8;
    const int dstBpp = dst_desc.BitsPerPixel / 8;

    for (int y = 0; y < src_desc.Height; y++) {
        for (int x = 0; x < src_desc.Width; x++) {
            const XBYTE *srcPtr = src_desc.Image + y * src_desc.BytesPerLine + x * srcBpp;
            XBYTE *dstPtr = dst_desc.Image + (src_desc.Height - 1 - y) * dst_desc.BytesPerLine + x * dstBpp;

            const XULONG pixel = ReadPixel(srcPtr, srcBpp);
            const XULONG dstPixel = ConvertPixel(pixel, src_desc, dst_desc);
            WritePixel(dstPtr, dstBpp, dstPixel);
        }
    }
}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue) {
    if (!dst_desc.AlphaMask || !dst_desc.Image) return;

    const VX_PIXELFORMAT format = VxImageDesc2PixelFormat(dst_desc);
    if (format >= _DXT1 && format <= _DXT5) return; // DXT not supported

    const XULONG alphaShift = GetBitShift(dst_desc.AlphaMask);
    const XULONG alphaBits = GetBitCount(dst_desc.AlphaMask);
    const int bytesPerPixel = dst_desc.BitsPerPixel / 8;

    // Scale alpha to bit depth
    if (alphaBits < 8) {
        AlphaValue = (AlphaValue * ((1 << alphaBits) - 1)) / 255;
    }

    // Fast path for 32-bit ARGB
    if (dst_desc.BitsPerPixel == 32 && dst_desc.AlphaMask == A_MASK) {
        const XULONG alphaComponent = (XULONG) AlphaValue << 24;
        const XULONG alphaMask = 0x00FFFFFF;

        for (int y = 0; y < dst_desc.Height; y++) {
            XULONG *row = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);
            for (int x = 0; x < dst_desc.Width; x++) {
                row[x] = (row[x] & alphaMask) | alphaComponent;
            }
        }
        return;
    }

    // General case
    for (int y = 0; y < dst_desc.Height; y++) {
        XBYTE *row = dst_desc.Image + y * dst_desc.BytesPerLine;
        for (int x = 0; x < dst_desc.Width; x++) {
            XBYTE *pixel = row + x * bytesPerPixel;

            switch (dst_desc.BitsPerPixel) {
            case 16: {
                XWORD *p = (XWORD *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) | ((AlphaValue << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            case 24: {
                int byteOffset = alphaShift / 8;
                int bitOffset = alphaShift % 8;

                XBYTE byteMask = (dst_desc.AlphaMask >> (byteOffset * 8)) & 0xFF;
                // Update just that byte
                pixel[byteOffset] = (pixel[byteOffset] & ~byteMask) | ((AlphaValue << bitOffset) & byteMask);
                break;
            }
            case 32: {
                XULONG *p = (XULONG *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) | ((AlphaValue << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            }
        }
    }
}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues) {
    if (!dst_desc.AlphaMask || !dst_desc.Image || !AlphaValues) return;

    const VX_PIXELFORMAT format = VxImageDesc2PixelFormat(dst_desc);
    if (format >= _DXT1 && format <= _DXT5) return;

    const XULONG alphaShift = GetBitShift(dst_desc.AlphaMask);
    const XULONG alphaBits = GetBitCount(dst_desc.AlphaMask);
    const int bytesPerPixel = dst_desc.BitsPerPixel / 8;

    // Fast path for 32-bit ARGB
    if (dst_desc.BitsPerPixel == 32 && dst_desc.AlphaMask == A_MASK) {
        const XULONG alphaMask = 0x00FFFFFF;

        for (int y = 0; y < dst_desc.Height; y++) {
            XULONG *row = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);
            for (int x = 0; x < dst_desc.Width; x++) {
                const int index = y * dst_desc.Width + x;
                const XULONG alphaComponent = (XULONG) AlphaValues[index] << 24;
                row[x] = (row[x] & alphaMask) | alphaComponent;
            }
        }
        return;
    }

    // General case
    for (int y = 0; y < dst_desc.Height; y++) {
        XBYTE *row = dst_desc.Image + y * dst_desc.BytesPerLine;
        for (int x = 0; x < dst_desc.Width; x++) {
            XBYTE *pixel = row + x * bytesPerPixel;
            const int index = y * dst_desc.Width + x;
            XBYTE alpha = AlphaValues[index];

            if (alphaBits < 8) {
                alpha = (alpha * ((1 << alphaBits) - 1)) / 255;
            }

            switch (dst_desc.BitsPerPixel) {
            case 16: {
                XWORD *p = (XWORD *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) | ((alpha << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            case 24: {
                int byteOffset = alphaShift / 8;
                int bitOffset = alphaShift % 8;

                XBYTE byteMask = (dst_desc.AlphaMask >> (byteOffset * 8)) & 0xFF;

                pixel[byteOffset] = (pixel[byteOffset] & ~byteMask) | ((alpha << bitOffset) & byteMask);
                break;
            }
            case 32: {
                XULONG *p = (XULONG *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) | ((alpha << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            }
        }
    }
}

void VxResizeImage32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (src_desc.BitsPerPixel != 32 || dst_desc.BitsPerPixel != 32) return;
    if (!src_desc.Image || !dst_desc.Image) return;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    const VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 || dstFormat >= _DXT1 && dstFormat <= _DXT5) return;

    const stbir_pixel_layout layout = GetStbPixelLayout(src_desc);

    stbir_resize_uint8_linear(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
        layout
    );
}

void VxGenerateMipMap(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    if (src_desc.BitsPerPixel != 32 || !src_desc.Image || !Buffer) return;
    if (src_desc.Width < 2 || src_desc.Height < 2) return;
    if ((src_desc.Width & (src_desc.Width - 1)) != 0 || (src_desc.Height & (src_desc.Height - 1)) != 0) return;

    const VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) return;

    const int dstWidth = src_desc.Width / 2;
    const int dstHeight = src_desc.Height / 2;
    const int dstPitch = dstWidth * 4;

    VxImageDescEx dst_desc;
    dst_desc.Width = dstWidth;
    dst_desc.Height = dstHeight;
    dst_desc.BitsPerPixel = 32;
    dst_desc.BytesPerLine = dstPitch;
    dst_desc.RedMask = src_desc.RedMask;
    dst_desc.GreenMask = src_desc.GreenMask;
    dst_desc.BlueMask = src_desc.BlueMask;
    dst_desc.AlphaMask = src_desc.AlphaMask;
    dst_desc.Image = Buffer;

    const stbir_pixel_layout layout = GetStbPixelLayout(src_desc);

    stbir_resize_uint8_linear(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        Buffer, dstWidth, dstHeight, dstPitch, layout
    );
}

//------------------------------------------------------------------------------
// Normal and bump map generation
//------------------------------------------------------------------------------

XBOOL VxConvertToNormalMap(const VxImageDescEx &image, XULONG ColorMask) {
    if (image.BitsPerPixel != 32 || !image.Image) return FALSE;
    if (image.Width <= 0 || image.Height <= 0) return FALSE;

    const VX_PIXELFORMAT format = VxImageDesc2PixelFormat(image);
    if (format >= _DXT1 && format <= _DXT5) return FALSE;

    const XULONG actualMask = (ColorMask == 0xFFFFFFFF)
                                  ? (image.RedMask | image.GreenMask | image.BlueMask)
                                  : ColorMask;
    if (actualMask == 0) return FALSE;

    const int pixelCount = image.Width * image.Height;
    float *heightMap = new(std::nothrow) float[pixelCount];
    if (!heightMap) return FALSE;

    // Extract height values
    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            const int index = y * image.Width + x;
            const int offset = SafePixelOffset(image, x, y, 4);

            if (offset < 0) {
                heightMap[index] = 0.5f;
                continue;
            }

            const XULONG *pixel = (const XULONG *) (image.Image + offset);

            if (ColorMask == 0xFFFFFFFF) {
                XBYTE r, g, b, a;
                ExtractRGBA(*pixel, image, r, g, b, a);
                heightMap[index] = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
            } else {
                const XULONG shift = GetBitShift(actualMask);
                const XULONG bits = GetBitCount(actualMask);
                XULONG val = (*pixel & actualMask) >> shift;

                if (bits > 0 && bits < 8) {
                    val = (val * 255) / ((1 << bits) - 1);
                }
                heightMap[index] = (float) val / 255.0f;
            }
        }
    }

    // Calculate normals using Sobel operator
    const float scale = 2.0f;

    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            const int index = y * image.Width + x;

            auto getHeight = [&](int px, int py) -> float {
                px = (px < 0) ? 0 : (px >= image.Width) ? image.Width - 1 : px;
                py = (py < 0) ? 0 : (py >= image.Height) ? image.Height - 1 : py;
                return heightMap[py * image.Width + px];
            };

            // Sobel filter
            const float h00 = getHeight(x - 1, y - 1), h01 = getHeight(x, y - 1), h02 = getHeight(x + 1, y - 1);
            const float h10 = getHeight(x - 1, y),                                h12 = getHeight(x + 1, y);
            const float h20 = getHeight(x - 1, y + 1), h21 = getHeight(x, y + 1), h22 = getHeight(x + 1, y + 1);

            const float gx = (h00 + 2.0f * h10 + h20) - (h02 + 2.0f * h12 + h22);
            const float gy = (h00 + 2.0f * h01 + h02) - (h20 + 2.0f * h21 + h22);

            float nx = -gx * scale;
            float ny = -gy * scale;
            float nz = 1.0f;

            const float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0001f) {
                nx /= length;
                ny /= length;
                nz /= length;
            } else {
                nx = ny = 0.0f;
                nz = 1.0f;
            }

            const int offset = SafePixelOffset(image, x, y, 4);
            if (offset >= 0) {
                XULONG *pixel = (XULONG *) (image.Image + offset);

                int r = (int) ((nx + 1.0f) * 127.5f);
                int g = (int) ((ny + 1.0f) * 127.5f);
                int b = (int) ((nz + 1.0f) * 127.5f);

                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;

                *pixel = (*pixel & image.AlphaMask) | CreatePixel(r, g, b, 255, image);
            }
        }
    }

    delete[] heightMap;
    return TRUE;
}

XBOOL VxConvertToBumpMap(const VxImageDescEx &image) {
    if (image.BitsPerPixel != 32 || !image.Image) return FALSE;
    if (image.Width <= 0 || image.Height <= 0) return FALSE;

    const VX_PIXELFORMAT format = VxImageDesc2PixelFormat(image);
    if (format >= _DXT1 && format <= _DXT5) return FALSE;

    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            const int offset = SafePixelOffset(image, x, y, 4);
            if (offset < 0) continue;

            XULONG *pixel = (XULONG *) (image.Image + offset);

            XBYTE r, g, b, a;
            ExtractRGBA(*pixel, image, r, g, b, a);

            const float luminance = 0.299f * r + 0.587f * g + 0.114f * b;

            auto getSafePixel = [&](int px, int py) -> float {
                if (px < 0 || px >= image.Width || py < 0 || py >= image.Height) {
                    return luminance;
                }

                const int poffset = SafePixelOffset(image, px, py, 4);
                if (poffset < 0) return luminance;

                const XULONG *ppixel = (const XULONG *) (image.Image + poffset);
                XBYTE pr, pg, pb, pa;
                ExtractRGBA(*ppixel, image, pr, pg, pb, pa);
                return 0.299f * pr + 0.587f * pg + 0.114f * pb;
            };

            const float leftLum = getSafePixel(x - 1, y);
            const float rightLum = getSafePixel(x + 1, y);
            const float du = (rightLum - leftLum) * 0.5f;

            const float topLum = getSafePixel(x, y - 1);
            const float bottomLum = getSafePixel(x, y + 1);
            const float dv = (bottomLum - topLum) * 0.5f;

            int duVal = (int) (du + 128.0f);
            int dvVal = (int) (dv + 128.0f);
            int lumVal = (int) luminance;

            duVal = (duVal < 0) ? 0 : (duVal > 255) ? 255 : duVal;
            dvVal = (dvVal < 0) ? 0 : (dvVal > 255) ? 255 : dvVal;
            lumVal = (lumVal < 0) ? 0 : (lumVal > 255) ? 255 : lumVal;

            *pixel = CreatePixel(duVal, dvVal, lumVal, a, image);
        }
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// Quantization utilities
//------------------------------------------------------------------------------

int GetQuantizationSamplingFactor() {
    return g_QuantizationSamplingFactor;
}

void SetQuantizationSamplingFactor(int factor) {
    g_QuantizationSamplingFactor = (factor < 1) ? 1 : (factor > 30) ? 30 : factor;
}
