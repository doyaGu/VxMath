#include <string.h>
#include <stdlib.h>

#include <intrin.h>

#include "VxMath.h"

/**
 * Configure STB library implementations.
 * Only include this defines in one source file.
 */
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

//------------------------------------------------------------------------------
// Global variables and constants
//------------------------------------------------------------------------------

/** Global quantization factor for palette generation */
static int g_QuantizationSamplingFactor = 15;

// DXT texture format block sizes
#define DXT1_BLOCK_SIZE 8     // 8 bytes per 4x4 pixel block
#define DXT3_BLOCK_SIZE 16    // 16 bytes per 4x4 pixel block
#define DXT5_BLOCK_SIZE 16    // 16 bytes per 4x4 pixel block

/** Lookup table for fast bit counting */
static const unsigned char BIT_COUNT_TABLE[256] = {
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

/**
 * @brief Count the number of bits set in a mask
 * @param dwMask The bit mask to analyze
 * @return The number of bits set in the mask
 */
XULONG GetBitCount(XULONG dwMask) {
    if (dwMask == 0) return 0;

    // Use lookup table for faster bit counting
    return BIT_COUNT_TABLE[dwMask & 0xff] +
        BIT_COUNT_TABLE[(dwMask >> 8) & 0xff] +
        BIT_COUNT_TABLE[(dwMask >> 16) & 0xff] +
        BIT_COUNT_TABLE[(dwMask >> 24) & 0xff];
}

/**
 * @brief Find the position of the least significant bit in a mask
 * @param dwMask The bit mask to analyze
 * @return The position of the least significant bit, or 0 if mask is 0
 */
XULONG GetBitShift(XULONG dwMask) {
    if (!dwMask) return 0;

    // Use compiler intrinsics if available for better performance
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

/**
 * @brief Get the number of bits for each color component
 * @param desc The image descriptor
 * @param Rbits [out] Number of bits for red component
 * @param Gbits [out] Number of bits for green component
 * @param Bbits [out] Number of bits for blue component
 * @param Abits [out] Number of bits for alpha component
 */
void VxGetBitCounts(const VxImageDescEx &desc, XULONG &Rbits, XULONG &Gbits, XULONG &Bbits, XULONG &Abits) {
    Rbits = GetBitCount(desc.RedMask);
    Gbits = GetBitCount(desc.GreenMask);
    Bbits = GetBitCount(desc.BlueMask);
    Abits = GetBitCount(desc.AlphaMask);
}

/**
 * @brief Get the bit shifts for each color component
 * @param desc The image descriptor
 * @param Rshift [out] Bit shift for red component
 * @param Gshift [out] Bit shift for green component
 * @param Bshift [out] Bit shift for blue component
 * @param Ashift [out] Bit shift for alpha component
 */
void VxGetBitShifts(const VxImageDescEx &desc, XULONG &Rshift, XULONG &Gshift, XULONG &Bshift, XULONG &Ashift) {
    Rshift = GetBitShift(desc.RedMask);
    Gshift = GetBitShift(desc.GreenMask);
    Bshift = GetBitShift(desc.BlueMask);
    Ashift = GetBitShift(desc.AlphaMask);
}

//------------------------------------------------------------------------------
// Pixel format utilities
//------------------------------------------------------------------------------

/**
 * @brief Information about a pixel format
 */
struct PixelFormatInfo {
    XWORD BitsPerPixel;           ///< Bits per pixel
    XULONG RedMask;               ///< Bit mask for red component
    XULONG GreenMask;             ///< Bit mask for green component
    XULONG BlueMask;              ///< Bit mask for blue component
    XULONG AlphaMask;             ///< Bit mask for alpha component
    const char *Description;      ///< Human-readable description
    stbir_pixel_layout StbLayout; ///< Corresponding STB layout
};

/**
 * @brief Table of standard pixel formats
 * Maps from VX_PIXELFORMAT enum to actual format specifications
 */
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

/**
 * @brief Determine the pixel format from an image descriptor
 * @param desc The image descriptor to analyze
 * @return The corresponding pixel format, or UNKNOWN_PF if not recognized
 */
VX_PIXELFORMAT VxImageDesc2PixelFormat(const VxImageDescEx &desc) {
    // Check for compressed formats first (fast path)
    if (desc.Flags >= _DXT1 && desc.Flags <= _DXT5) {
        return static_cast<VX_PIXELFORMAT>(desc.Flags);
    }

    // Check for bump map formats (they use Flags field too)
    // Bump map formats: _16_V8U8=24, _32_V16U16=25, _16_L6V5U5=26, _32_X8L8V8U8=27
    if (desc.Flags >= 24 && desc.Flags <= 27) {
        return static_cast<VX_PIXELFORMAT>(desc.Flags);
    }

    // Linear search for standard formats (exclude compressed and bump map formats)
    for (int i = 1; i < 24; ++i) {
        // Only check formats 1-23 (standard color formats)
        const PixelFormatInfo &info = PixelFormatTable[i];

        if (desc.BitsPerPixel == info.BitsPerPixel &&
            desc.RedMask == info.RedMask &&
            desc.GreenMask == info.GreenMask &&
            desc.BlueMask == info.BlueMask &&
            desc.AlphaMask == info.AlphaMask) {
            return static_cast<VX_PIXELFORMAT>(i);
        }
    }

    return UNKNOWN_PF;
}

/**
 * @brief Initialize an image descriptor from a pixel format
 * @param Pf The pixel format to convert
 * @param desc [out] The image descriptor to initialize
 */
void VxPixelFormat2ImageDesc(VX_PIXELFORMAT Pf, VxImageDescEx &desc) {
    if (Pf >= 0 && Pf < MAX_PIXEL_FORMATS) {
        if (Pf >= _DXT1 && Pf <= _DXT5) {
            // Handle compressed formats - these are special cases
            desc.Flags = Pf;

            // Calculate bits per pixel based on format
            // DXT1 = 4 bits per pixel (RGB or RGBA with 1-bit alpha)
            // DXT2-5 = 8 bits per pixel (RGBA with variable alpha)
            desc.BitsPerPixel = (Pf == _DXT1) ? 4 : 8;

            // For compressed textures, BytesPerLine is actually the size of one row of blocks
            // A block is 4x4 pixels, so width/4 blocks per row (rounded up)
            int blocksPerRow = (desc.Width + 3) / 4;
            int bytesPerBlock = (Pf == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;
            desc.BytesPerLine = blocksPerRow * bytesPerBlock;

            // For compressed textures, TotalImageSize is used instead of BytesPerLine*Height
            int blocksPerColumn = (desc.Height + 3) / 4;
            desc.TotalImageSize = blocksPerRow * blocksPerColumn * bytesPerBlock;

            // Color masks are not used for compressed formats
            desc.RedMask = 0;
            desc.GreenMask = 0;
            desc.BlueMask = 0;
            desc.AlphaMask = 0;
        } else if (Pf >= 24 && Pf <= 27) {
            // Bump map formats: _16_V8U8, _32_V16U16, _16_L6V5U5, _32_X8L8V8U8
            // Handle bump map formats - these need special flags to distinguish from regular formats
            desc.Flags = Pf;

            // Set up the format properties
            desc.BitsPerPixel = PixelFormatTable[Pf].BitsPerPixel;
            desc.RedMask = PixelFormatTable[Pf].RedMask;     // Also sets BumpDuMask via union
            desc.GreenMask = PixelFormatTable[Pf].GreenMask; // Also sets BumpDvMask via union
            desc.BlueMask = PixelFormatTable[Pf].BlueMask;   // Also sets BumpLumMask via union
            desc.AlphaMask = PixelFormatTable[Pf].AlphaMask;

            // Calculate bytes per line (with proper padding to 4-byte alignment)
            int bytesPerPixel = (desc.BitsPerPixel + 7) / 8;
            desc.BytesPerLine = ((desc.Width * bytesPerPixel) + 3) & ~3;
        } else {
            // Standard formats
            desc.BitsPerPixel = PixelFormatTable[Pf].BitsPerPixel;
            desc.RedMask = PixelFormatTable[Pf].RedMask;
            desc.GreenMask = PixelFormatTable[Pf].GreenMask;
            desc.BlueMask = PixelFormatTable[Pf].BlueMask;
            desc.AlphaMask = PixelFormatTable[Pf].AlphaMask;
            desc.Flags = 0; // Standard formats don't use flags

            // Calculate bytes per line (with proper padding to 4-byte alignment)
            int bytesPerPixel = (desc.BitsPerPixel + 7) / 8;
            desc.BytesPerLine = ((desc.Width * bytesPerPixel) + 3) & ~3;
        }
    }
}

/**
 * @brief Get a human-readable string describing a pixel format
 * @param Pf The pixel format
 * @return A string describing the format
 */
const char *VxPixelFormat2String(VX_PIXELFORMAT Pf) {
    if (Pf >= 0 && Pf < MAX_PIXEL_FORMATS) {
        return PixelFormatTable[Pf].Description;
    }
    return "";
}

/**
 * @brief Set standard masks for a given bit depth
 * @param desc [in,out] The image descriptor to update
 */
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

/**
 * @brief Get appropriate STB pixel layout from a VxImageDescEx
 * @param desc The image descriptor
 * @return The corresponding STB pixel layout
 */
stbir_pixel_layout GetStbPixelLayout(const VxImageDescEx &desc) {
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(desc);
    if (format != UNKNOWN_PF) {
        return PixelFormatTable[format].StbLayout;
    }

    // Figure out appropriate layout based on channels and masks
    int channelCount = desc.BitsPerPixel / 8;

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

/**
 * @brief Read a pixel value from an image buffer
 * @param ptr Pointer to the pixel data
 * @param bpp Bytes per pixel
 * @return The pixel value
 */
inline XULONG ReadPixel(const XBYTE *ptr, int bpp) {
    switch (bpp) {
    case 1:
        return *ptr;
    case 2:
        return *(const unsigned short *) ptr;
    case 3:
        return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
    case 4:
        return *(const XULONG *) ptr;
    default:
        return 0;
    }
}

/**
 * @brief Write a pixel value to an image buffer
 * @param ptr Pointer to the pixel data
 * @param bpp Bytes per pixel
 * @param pixel The pixel value to write
 */
inline void WritePixel(XBYTE *ptr, int bpp, XULONG pixel) {
    switch (bpp) {
    case 1:
        *ptr = (XBYTE) pixel;
        break;
    case 2:
        *(unsigned short *) ptr = (unsigned short) pixel;
        break;
    case 3:
        ptr[0] = (XBYTE) (pixel);
        ptr[1] = (XBYTE) (pixel >> 8);
        ptr[2] = (XBYTE) (pixel >> 16);
        break;
    case 4:
        *(XULONG *) ptr = pixel;
        break;
    }
}

/**
 * @brief Safely calculate pixel offset with bounds checking
 * @param desc Image descriptor
 * @param x X coordinate
 * @param y Y coordinate
 * @param bytesPerPixel Bytes per pixel
 * @return Pixel offset or -1 if out of bounds
 */
static int SafePixelOffset(const VxImageDescEx &desc, int x, int y, int bytesPerPixel) {
    if (x < 0 || x >= desc.Width || y < 0 || y >= desc.Height) return -1;
    if (bytesPerPixel <= 0 || bytesPerPixel > 4) return -1;
    if (!desc.Image) return -1; // Critical: Check Image pointer

    if (y > 0 && desc.BytesPerLine > 0) {
        if (y > INT_MAX / desc.BytesPerLine) return -1;
    }
    if (x > 0 && bytesPerPixel > 0) {
        if (x > INT_MAX / bytesPerPixel) return -1;
    }

    long long lineOffset = (long long) y * desc.BytesPerLine;
    long long pixelOffset = (long long) x * bytesPerPixel;

    long long totalOffset = lineOffset + pixelOffset;
    if (totalOffset > INT_MAX || totalOffset < 0) return -1;

    return (int) totalOffset;
}

/**
 * @brief Read a pixel value from an image buffer with bounds checking
 */
inline XULONG ReadPixelSafe(const VxImageDescEx &desc, int x, int y, int bpp) {
    int offset = SafePixelOffset(desc, x, y, bpp);
    if (offset < 0) return 0;

    const XBYTE *ptr = desc.Image + offset;

    switch (bpp) {
    case 1:
        return *ptr;
    case 2:
        return *(const unsigned short *) ptr;
    case 3:
        return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
    case 4:
        return *(const XULONG *) ptr;
    default:
        return 0;
    }
}

/**
 * @brief Write a pixel value to an image buffer with bounds checking
 */
inline XBOOL WritePixelSafe(const VxImageDescEx &desc, int x, int y, int bpp, XULONG pixel) {
    int offset = SafePixelOffset(desc, x, y, bpp);
    if (offset < 0) return FALSE;

    XBYTE *ptr = desc.Image + offset;

    switch (bpp) {
    case 1:
        *ptr = (XBYTE) pixel;
        break;
    case 2:
        *(unsigned short *) ptr = (unsigned short) pixel;
        break;
    case 3:
        ptr[0] = (XBYTE) (pixel);
        ptr[1] = (XBYTE) (pixel >> 8);
        ptr[2] = (XBYTE) (pixel >> 16);
        break;
    case 4:
        *(XULONG *) ptr = pixel;
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Extract RGBA components from a pixel in any format
 *
 * @param pixel The source pixel
 * @param desc The source image descriptor
 * @param r [out] The extracted red component (0-255)
 * @param g [out] The extracted green component (0-255)
 * @param b [out] The extracted blue component (0-255)
 * @param a [out] The extracted alpha component (0-255)
 */
inline void ExtractRGBA(XULONG pixel, const VxImageDescEx &desc,
                        unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a) {
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

/**
 * @brief Create a pixel value from RGBA components
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 * @param desc The destination image descriptor
 * @return The created pixel value
 */
inline XULONG CreatePixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                          const VxImageDescEx &desc) {
    XULONG rShift = GetBitShift(desc.RedMask);
    XULONG gShift = GetBitShift(desc.GreenMask);
    XULONG bShift = GetBitShift(desc.BlueMask);
    XULONG aShift = GetBitShift(desc.AlphaMask);

    XULONG rBits = GetBitCount(desc.RedMask);
    XULONG gBits = GetBitCount(desc.GreenMask);
    XULONG bBits = GetBitCount(desc.BlueMask);
    XULONG aBits = GetBitCount(desc.AlphaMask);

    // Scale components to destination bit depth
    if (rBits < 8)
        r = (r * ((1 << rBits) - 1)) / 255;

    if (gBits < 8)
        g = (g * ((1 << gBits) - 1)) / 255;

    if (bBits < 8)
        b = (b * ((1 << bBits) - 1)) / 255;

    if (aBits < 8)
        a = (a * ((1 << aBits) - 1)) / 255;

    // Assemble pixel
    XULONG pixel = 0;
    if (desc.RedMask) pixel |= (r << rShift) & desc.RedMask;
    if (desc.GreenMask) pixel |= (g << gShift) & desc.GreenMask;
    if (desc.BlueMask) pixel |= (b << bShift) & desc.BlueMask;
    if (desc.AlphaMask) pixel |= (a << aShift) & desc.AlphaMask;

    return pixel;
}

/**
 * @brief Convert a pixel from one format to another
 *
 * @param srcPixel Source pixel value
 * @param srcDesc Source image descriptor
 * @param dstDesc Destination image descriptor
 * @return Converted pixel value
 */
inline XULONG ConvertPixel(XULONG srcPixel, const VxImageDescEx &srcDesc, const VxImageDescEx &dstDesc) {
    // Special case for identical formats
    if (srcDesc.RedMask == dstDesc.RedMask &&
        srcDesc.GreenMask == dstDesc.GreenMask &&
        srcDesc.BlueMask == dstDesc.BlueMask &&
        srcDesc.AlphaMask == dstDesc.AlphaMask) {
        return srcPixel;
    }

    unsigned char r, g, b, a;
    ExtractRGBA(srcPixel, srcDesc, r, g, b, a);
    return CreatePixel(r, g, b, a, dstDesc);
}

//------------------------------------------------------------------------------
// DXT Compression Utilities
//------------------------------------------------------------------------------

/**
 * @brief 565 color decompression
 */
void DecompressColor565(unsigned short color, unsigned char *rgb) {
    // Extract 5-6-5 components
    unsigned int r5 = (color >> 11) & 0x1F; // 5 bits
    unsigned int g6 = (color >> 5) & 0x3F;  // 6 bits
    unsigned int b5 = color & 0x1F;         // 5 bits

    // Convert to 8-bit with proper scaling
    // For 5-bit: multiply by 255/31 ≈ 8.226, so use (val * 255 + 15) / 31
    // For 6-bit: multiply by 255/63 ≈ 4.048, so use (val * 255 + 31) / 63
    rgb[0] = (unsigned char) ((r5 * 255 + 15) / 31); // Red
    rgb[1] = (unsigned char) ((g6 * 255 + 31) / 63); // Green
    rgb[2] = (unsigned char) ((b5 * 255 + 15) / 31); // Blue
}

int CalculateDXTSize(int width, int height, VX_PIXELFORMAT format) {
    // Validate input parameters
    if (width <= 0 || height <= 0) return 0;
    if (format < _DXT1 || format > _DXT5) return 0;

    // Compute number of 4x4 blocks (rounding up)
    int blocksWide = (width + 3) / 4;
    int blocksHigh = (height + 3) / 4;

    // Check for overflow
    if (blocksWide > INT_MAX / blocksHigh) return 0;
    int numBlocks = blocksWide * blocksHigh;

    // Determine bytes per block based on format
    int bytesPerBlock;
    switch (format) {
    case _DXT1:
        bytesPerBlock = DXT1_BLOCK_SIZE; // 8 bytes
        break;
    case _DXT2:
    case _DXT3:
    case _DXT4:
    case _DXT5:
        bytesPerBlock = DXT5_BLOCK_SIZE; // 16 bytes
        break;
    default:
        return 0; // Unsupported format
    }

    // Check for overflow in final calculation
    if (numBlocks > INT_MAX / bytesPerBlock) return 0;

    return numBlocks * bytesPerBlock;
}

/**
 * @brief Safely extract RGBA from source pixel using masks
 */
static void ExtractRGBAFromPixel(XULONG pixel, const VxImageDescEx &desc,
                                 unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a) {
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
    r = (rBits > 0 && rBits < 8) ? (unsigned char) ((rVal * 255) / ((1 << rBits) - 1)) : (unsigned char) rVal;
    g = (gBits > 0 && gBits < 8) ? (unsigned char) ((gVal * 255) / ((1 << gBits) - 1)) : (unsigned char) gVal;
    b = (bBits > 0 && bBits < 8) ? (unsigned char) ((bVal * 255) / ((1 << bBits) - 1)) : (unsigned char) bVal;
    a = (aBits > 0 && aBits < 8) ? (unsigned char) ((aVal * 255) / ((1 << aBits) - 1)) : (unsigned char) aVal;
}

/**
 * @brief Validate image descriptor and dimensions
 * @param desc Image descriptor to validate
 * @return TRUE if valid, FALSE otherwise
 */
static XBOOL ValidateImageDesc(const VxImageDescEx &desc) {
    if (!desc.Image) return FALSE;
    if (desc.Width <= 0 || desc.Height <= 0) return FALSE;
    if (desc.BitsPerPixel <= 0 || desc.BitsPerPixel > 32) return FALSE;
    if (desc.BytesPerLine <= 0) return FALSE;

    if (desc.Height > 0 && desc.BytesPerLine > 0) {
        if (desc.Height > INT_MAX / desc.BytesPerLine) return FALSE;

        // For compressed formats, check TotalImageSize instead
        VX_PIXELFORMAT format = VxImageDesc2PixelFormat(desc);
        if (format >= _DXT1 && format <= _DXT5) {
            if (desc.TotalImageSize <= 0) return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Convert an RGBA image to DXT format
 * @param src_desc Source image descriptor (must be 32-bit RGBA)
 * @param dst_desc Destination image descriptor (must be DXT format)
 * @param highQuality Use high quality compression (slower)
 * @return TRUE if successful, FALSE otherwise
 */
XBOOL VxConvertToDXT(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc, XBOOL highQuality) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) {
        return FALSE;
    }
    if (!src_desc.Image || !dst_desc.Image) return FALSE;

    // Check source format - must be uncompressed
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) {
        return FALSE; // Cannot compress already compressed format
    }

    // Check destination is a DXT format
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);
    if (dstFormat < _DXT1 || dstFormat > _DXT5) {
        return FALSE;
    }

    // Validate that source has color channels
    if (src_desc.BitsPerPixel < 8 || (!src_desc.RedMask && !src_desc.GreenMask && !src_desc.BlueMask)) {
        return FALSE;
    }

    // Calculate expected destination size
    int expectedSize = CalculateDXTSize(src_desc.Width, src_desc.Height, dstFormat);
    if (expectedSize <= 0 || dst_desc.TotalImageSize < expectedSize) {
        return FALSE;
    }

    // Set up compression parameters
    int blockSize = (dstFormat == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;

    // Overflow protection for block calculations
    if (src_desc.Width < 0 || src_desc.Height < 0) return FALSE;
    if (src_desc.Width > INT_MAX / 4 || src_desc.Height > INT_MAX / 4) return FALSE;

    int blocksWide = (src_desc.Width + 3) / 4;
    int blocksHigh = (src_desc.Height + 3) / 4;

    // Validate block counts
    if (blocksWide <= 0 || blocksHigh <= 0) return FALSE;
    if (blocksWide > INT_MAX / blocksHigh) return FALSE;

    bool hasAlpha = (dstFormat != _DXT1) || (src_desc.AlphaMask != 0);
    int mode = highQuality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    // Source pixel info
    int srcBpp = src_desc.BitsPerPixel / 8;
    if (srcBpp <= 0 || srcBpp > 4) return FALSE;

    // Allocate block buffer for conversion (4x4 pixels, 4 bytes per pixel RGBA)
    unsigned char block[64]; // 16 pixels * 4 bytes = 64 bytes

    // Process each 4x4 block
    for (int by = 0; by < blocksHigh; by++) {
        for (int bx = 0; bx < blocksWide; bx++) {
            if (bx < 0 || by < 0) continue;

            // Extract 4x4 block of source pixels
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int srcX = bx * 4 + x;
                    int srcY = by * 4 + y;

                    // Handle edge cases (pixels outside image bounds)
                    if (srcX >= src_desc.Width) srcX = src_desc.Width - 1;
                    if (srcY >= src_desc.Height) srcY = src_desc.Height - 1;

                    if (srcX < 0) srcX = 0;
                    if (srcY < 0) srcY = 0;

                    // Bounds check
                    int srcOffset = SafePixelOffset(src_desc, srcX, srcY, srcBpp);
                    if (srcOffset < 0) {
                        // Fill with black if out of bounds
                        unsigned char *blockPixel = &block[(y * 4 + x) * 4];
                        blockPixel[0] = 0; // R
                        blockPixel[1] = 0; // G
                        blockPixel[2] = 0; // B
                        blockPixel[3] = 0; // A
                        continue;
                    }

                    int maxSrcOffset = src_desc.Height * src_desc.BytesPerLine;
                    if (srcOffset + srcBpp > maxSrcOffset) {
                        unsigned char *blockPixel = &block[(y * 4 + x) * 4];
                        blockPixel[0] = 0;
                        blockPixel[1] = 0;
                        blockPixel[2] = 0;
                        blockPixel[3] = 0;
                        continue;
                    }

                    // Read source pixel
                    XULONG pixel = ReadPixel(src_desc.Image + srcOffset, srcBpp);

                    // Extract RGBA components using masks
                    unsigned char r, g, b, a;
                    ExtractRGBAFromPixel(pixel, src_desc, r, g, b, a);

                    // Store in block buffer (RGBA format expected by STB)
                    int blockIdx = (y * 4 + x) * 4;
                    if (blockIdx >= 0 && blockIdx + 3 < 64) {
                        unsigned char *blockPixel = &block[blockIdx];
                        blockPixel[0] = r;
                        blockPixel[1] = g;
                        blockPixel[2] = b;
                        blockPixel[3] = a;
                    }
                }
            }

            // Calculate destination block offset
            int blockIndex = by * blocksWide + bx;

            if (blockIndex < 0 || blockIndex >= blocksWide * blocksHigh) {
                continue;
            }

            long long dstOffset = (long long) blockIndex * blockSize;
            if (dstOffset < 0 || dstOffset + blockSize > dst_desc.TotalImageSize) {
                return FALSE; // Prevent buffer overflow
            }

            unsigned char *dstBlock = dst_desc.Image + dstOffset;

            // Compress the block using STB
            stb_compress_dxt_block(dstBlock, block, hasAlpha ? 1 : 0, mode);
        }
    }

    return TRUE;
}

/**
 * @brief DXT3 alpha decompression
 */
static void DecompressDXT3Alpha(const unsigned char *blockPtr, unsigned char *alphaValues) {
    if (!blockPtr || !alphaValues) {
        if (alphaValues) {
            for (int i = 0; i < 16; i++) {
                alphaValues[i] = 255;
            }
        }
        return;
    }

    // DXT3 alpha data starts at offset 0, color data at offset 8
    for (int i = 0; i < 8; i++) {
        unsigned char alphaByte = blockPtr[i];
        int idx1 = i * 2;
        int idx2 = i * 2 + 1;
        if (idx1 < 16) {
            alphaValues[idx1] = ((alphaByte & 0x0F) * 255) / 15; // Lower 4 bits
        }
        if (idx2 < 16) {
            alphaValues[idx2] = ((alphaByte >> 4) * 255) / 15; // Upper 4 bits
        }
    }
}

/**
 * @brief DXT5 alpha decompression
 */
static void DecompressDXT5Alpha(const unsigned char *blockPtr, unsigned char *alphaValues) {
    if (!blockPtr || !alphaValues) {
        if (alphaValues) {
            for (int i = 0; i < 16; i++) {
                alphaValues[i] = 255;
            }
        }
        return;
    }

    unsigned char alpha0 = blockPtr[0];
    unsigned char alpha1 = blockPtr[1];

    // Build the alpha lookup table
    unsigned char alphaLUT[8];
    alphaLUT[0] = alpha0;
    alphaLUT[1] = alpha1;

    if (alpha0 > alpha1) {
        // 8-alpha mode (6 interpolated values)
        for (int i = 2; i < 8; i++) {
            alphaLUT[i] = (unsigned char) (((8 - i) * alpha0 + (i - 1) * alpha1) / 7);
        }
    } else {
        // 6-alpha mode (4 interpolated + transparent + opaque)
        for (int i = 2; i < 6; i++) {
            alphaLUT[i] = (unsigned char) (((6 - i) * alpha0 + (i - 1) * alpha1) / 5);
        }
        alphaLUT[6] = 0;   // Transparent
        alphaLUT[7] = 255; // Fully opaque
    }

    // Extract 3-bit alpha indices from 6 bytes starting at offset 2
    unsigned long long alphaIndices = 0;
    for (int i = 0; i < 6; i++) {
        alphaIndices |= ((unsigned long long) blockPtr[i + 2] << (8 * i));
    }

    // Critical: Bounds-checked alpha value assignment
    for (int i = 0; i < 16; i++) {
        unsigned int alphaIndex = (unsigned int) ((alphaIndices >> (3 * i)) & 0x07);
        // Critical: Ensure index is within bounds
        if (alphaIndex < 8) {
            alphaValues[i] = alphaLUT[alphaIndex];
        } else {
            alphaValues[i] = 255; // Safe fallback for invalid indices
        }
    }
}

/**
 * @brief Create destination pixel from RGBA components using masks
 */
static XULONG CreateDestPixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a, const VxImageDescEx &desc) {
    XULONG rShift = GetBitShift(desc.RedMask);
    XULONG gShift = GetBitShift(desc.GreenMask);
    XULONG bShift = GetBitShift(desc.BlueMask);
    XULONG aShift = GetBitShift(desc.AlphaMask);

    XULONG rBits = GetBitCount(desc.RedMask);
    XULONG gBits = GetBitCount(desc.GreenMask);
    XULONG bBits = GetBitCount(desc.BlueMask);
    XULONG aBits = GetBitCount(desc.AlphaMask);

    // Scale components to destination bit depth
    XULONG rVal = r;
    XULONG gVal = g;
    XULONG bVal = b;
    XULONG aVal = a;

    if (rBits > 0 && rBits < 8) rVal = (r * ((1 << rBits) - 1)) / 255;
    if (gBits > 0 && gBits < 8) gVal = (g * ((1 << gBits) - 1)) / 255;
    if (bBits > 0 && bBits < 8) bVal = (b * ((1 << bBits) - 1)) / 255;
    if (aBits > 0 && aBits < 8) aVal = (a * ((1 << aBits) - 1)) / 255;

    // Assemble pixel
    XULONG pixel = 0;
    if (desc.RedMask) pixel |= (rVal << rShift) & desc.RedMask;
    if (desc.GreenMask) pixel |= (gVal << gShift) & desc.GreenMask;
    if (desc.BlueMask) pixel |= (bVal << bShift) & desc.BlueMask;
    if (desc.AlphaMask) pixel |= (aVal << aShift) & desc.AlphaMask;

    return pixel;
}

/**
 * @brief DXT decompression with proper format handling
 */
XBOOL VxDecompressDXT(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) {
        return FALSE;
    }

    // Check source format
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    if (srcFormat < _DXT1 || srcFormat > _DXT5) {
        return FALSE;
    }

    // Check destination format - must be uncompressed
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);
    if (dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        return FALSE; // Cannot decompress to compressed format
    }

    // Validate destination has sufficient bit depth
    if (dst_desc.BitsPerPixel < 8) {
        return FALSE;
    }

    // Check dimensions match
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) {
        return FALSE;
    }

    // Setup parameters based on format
    int blockSize = (srcFormat == _DXT1) ? DXT1_BLOCK_SIZE : DXT5_BLOCK_SIZE;
    int blocksWide = (src_desc.Width + 3) / 4;
    int blocksHigh = (src_desc.Height + 3) / 4;

    // Validate source data size
    int expectedSize = blocksWide * blocksHigh * blockSize;
    if (src_desc.TotalImageSize < expectedSize) {
        return FALSE;
    }

    int dstBpp = dst_desc.BitsPerPixel / 8;
    if (dstBpp <= 0 || dstBpp > 4) return FALSE;

    // Process each 4x4 block
    for (int by = 0; by < blocksHigh; by++) {
        for (int bx = 0; bx < blocksWide; bx++) {
            // Calculate block offset with bounds checking
            int blockIndex = by * blocksWide + bx;
            if (blockIndex * blockSize + blockSize > src_desc.TotalImageSize) {
                return FALSE;
            }

            const unsigned char *blockPtr = src_desc.Image + blockIndex * blockSize;

            // Extract color data (same for all DXT formats)
            // Color data is always at the end of the block
            const unsigned char *colorPtr = (srcFormat == _DXT1) ? blockPtr : (blockPtr + 8);

            unsigned short color0 = colorPtr[0] | (colorPtr[1] << 8);
            unsigned short color1 = colorPtr[2] | (colorPtr[3] << 8);
            unsigned int colorIndices = colorPtr[4] | (colorPtr[5] << 8) | (colorPtr[6] << 16) | (colorPtr[7] << 24);

            // Decompress the two reference colors from 565 format
            unsigned char refColors[4][4]; // [color_index][rgba]

            // Reference color 0
            DecompressColor565(color0, refColors[0]);
            refColors[0][3] = 255; // Fully opaque

            // Reference color 1
            DecompressColor565(color1, refColors[1]);
            refColors[1][3] = 255; // Fully opaque

            // Calculate interpolated colors
            if (srcFormat == _DXT1 && color0 <= color1) {
                // DXT1 with 1-bit alpha (3-color mode)
                for (int i = 0; i < 3; i++) {
                    refColors[2][i] = (unsigned char) ((refColors[0][i] + refColors[1][i]) / 2);
                }
                refColors[2][3] = 255; // Still opaque

                // Color 3: Transparent black
                refColors[3][0] = 0;
                refColors[3][1] = 0;
                refColors[3][2] = 0;
                refColors[3][3] = 0; // Transparent
            } else {
                // DXT1 without alpha, or DXT3/DXT5 (4-color mode)
                for (int i = 0; i < 3; i++) {
                    refColors[2][i] = (unsigned char) ((2 * refColors[0][i] + refColors[1][i]) / 3);
                }
                refColors[2][3] = 255;

                for (int i = 0; i < 3; i++) {
                    refColors[3][i] = (unsigned char) ((refColors[0][i] + 2 * refColors[1][i]) / 3);
                }
                refColors[3][3] = 255;
            }

            // Handle alpha channel for DXT3 and DXT5
            unsigned char alphaValues[16];

            // Initialize to fully opaque
            for (int i = 0; i < 16; i++) {
                alphaValues[i] = 255;
            }

            if (srcFormat == _DXT3) {
                DecompressDXT3Alpha(blockPtr, alphaValues);
            } else if (srcFormat == _DXT5) {
                DecompressDXT5Alpha(blockPtr, alphaValues);
            }

            // Write pixels to output
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    // Calculate pixel position in destination
                    int pixelX = bx * 4 + x;
                    int pixelY = by * 4 + y;

                    // Skip pixels outside image bounds
                    if (pixelX >= dst_desc.Width || pixelY >= dst_desc.Height) {
                        continue;
                    }

                    // Get color index for this pixel (2 bits per pixel)
                    int pixelIdx = y * 4 + x;
                    int colorIdx = (colorIndices >> (2 * pixelIdx)) & 0x3;

                    // Bounds check for color index
                    if (colorIdx > 3) colorIdx = 0;

                    // Get color from reference colors
                    unsigned char r = refColors[colorIdx][0];
                    unsigned char g = refColors[colorIdx][1];
                    unsigned char b = refColors[colorIdx][2];
                    unsigned char a = refColors[colorIdx][3];

                    // Apply alpha from alpha channel for DXT3/DXT5
                    if (srcFormat == _DXT3 || srcFormat == _DXT5) {
                        a = alphaValues[pixelIdx];
                    } else if (srcFormat == _DXT1 && colorIdx == 3 && color0 <= color1) {
                        // DXT1 1-bit alpha mode
                        a = 0; // Transparent
                    }

                    // Calculate destination pixel offset with bounds checking
                    int dstOffset = SafePixelOffset(dst_desc, pixelX, pixelY, dstBpp);
                    if (dstOffset < 0) continue;

                    // Create the output pixel based on destination format
                    XULONG destPixel = CreateDestPixel(r, g, b, a, dst_desc);

                    // Write destination pixel
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

/**
 * @brief Copy image data with identical formats
 *
 * @param src_desc Source image descriptor
 * @param dst_desc Destination image descriptor
 */
static void DirectMemoryBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    // Check if this is a compressed format
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(src_desc);
    if (format >= _DXT1 && format <= _DXT5) {
        // For compressed formats, we copy the entire compressed data
        memcpy(dst_desc.Image, src_desc.Image, src_desc.TotalImageSize);
        return;
    }

    // If source and destination have same dimensions and format, use direct memcpy
    if (src_desc.Width == dst_desc.Width &&
        src_desc.BytesPerLine == dst_desc.BytesPerLine) {
        memcpy(dst_desc.Image, src_desc.Image, src_desc.BytesPerLine * src_desc.Height);
        return;
    }

    // Otherwise, copy line by line
    const int bytesPerRow = src_desc.Width * (src_desc.BitsPerPixel / 8);
    for (int y = 0; y < src_desc.Height; y++) {
        XBYTE *srcLine = src_desc.Image + y * src_desc.BytesPerLine;
        XBYTE *dstLine = dst_desc.Image + y * dst_desc.BytesPerLine;
        memcpy(dstLine, srcLine, bytesPerRow);
    }
}

/**
 * @brief Copy and convert image data between different formats (no resizing)
 *
 * @param src_desc Source image descriptor
 * @param dst_desc Destination image descriptor
 */
static void ConvertFormats(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) {
        return;
    }

    // Check dimensions match
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) {
        return;
    }

    // Check for compressed formats
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // Handle compression/decompression cases
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) {
        if (dstFormat >= _DXT1 && dstFormat <= _DXT5) {
            return; // Cannot convert between different DXT formats directly
        } else {
            VxDecompressDXT(src_desc, dst_desc);
            return;
        }
    } else if (dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        VxConvertToDXT(src_desc, dst_desc, TRUE);
        return;
    }

    // Standard format conversion
    int srcBpp = src_desc.BitsPerPixel / 8;
    int dstBpp = dst_desc.BitsPerPixel / 8;

    if (srcBpp <= 0 || srcBpp > 4 || dstBpp <= 0 || dstBpp > 4) {
        return;
    }

    // Special case for 32-bit to 32-bit (optimized)
    if (srcBpp == 4 && dstBpp == 4) {
        for (int y = 0; y < src_desc.Height; y++) {
            // Check bounds before accessing
            if (y * src_desc.BytesPerLine + src_desc.Width * 4 > src_desc.BytesPerLine * src_desc.Height ||
                y * dst_desc.BytesPerLine + dst_desc.Width * 4 > dst_desc.BytesPerLine * dst_desc.Height) {
                continue;
            }

            XULONG *srcLine = (XULONG *) (src_desc.Image + y * src_desc.BytesPerLine);
            XULONG *dstLine = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);

            for (int x = 0; x < src_desc.Width; x++) {
                dstLine[x] = ConvertPixel(srcLine[x], src_desc, dst_desc);
            }
        }
        return;
    }

    // General case - convert each pixel with bounds checking
    for (int y = 0; y < src_desc.Height; y++) {
        for (int x = 0; x < src_desc.Width; x++) {
            XULONG pixel = ReadPixelSafe(src_desc, x, y, srcBpp);
            XULONG dstPixel = ConvertPixel(pixel, src_desc, dst_desc);
            WritePixelSafe(dst_desc, x, y, dstBpp, dstPixel);
        }
    }
}

/**
 * @brief Copy, convert, and resize an image
 *
 * Main image blitting function for general-purpose image processing.
 *
 * @param src_desc Source image descriptor
 * @param dst_desc Destination image descriptor
 */
void VxDoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    if (!ValidateImageDesc(src_desc) || !ValidateImageDesc(dst_desc)) {
        return;
    }

    // Determine pixel formats
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // Fast path for identical format and dimensions
    if (srcFormat == dstFormat && srcFormat != UNKNOWN_PF) {
        if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
            DirectMemoryBlit(src_desc, dst_desc);
            return;
        }
    }

    // Handle DXT format cases
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 ||
        dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        // Case 1: Convert from standard to DXT
        if (srcFormat < _DXT1 && dstFormat >= _DXT1 && dstFormat <= _DXT5) {
            if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
                VxConvertToDXT(src_desc, dst_desc, TRUE);
            } else {
                // Need to resize first, then compress
                VxImageDescEx tempDesc;
                tempDesc.Width = dst_desc.Width;
                tempDesc.Height = dst_desc.Height;
                tempDesc.BitsPerPixel = 32;
                tempDesc.BytesPerLine = dst_desc.Width * 4;
                tempDesc.RedMask = 0x00FF0000;
                tempDesc.GreenMask = 0x0000FF00;
                tempDesc.BlueMask = 0x000000FF;
                tempDesc.AlphaMask = 0xFF000000;

                // Check for overflow in buffer size calculation
                if (tempDesc.Height > 0 && tempDesc.BytesPerLine > INT_MAX / tempDesc.Height) {
                    return;
                }

                XBYTE *tempBuffer = new XBYTE[tempDesc.BytesPerLine * tempDesc.Height];
                if (!tempBuffer) return;

                tempDesc.Image = tempBuffer;

                // Use try-catch equivalent error handling pattern
                bool success = true;

                // Resize to the temporary buffer
                if (success) {
                    stbir_resize_uint8_linear(
                        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
                        tempDesc.Image, tempDesc.Width, tempDesc.Height, tempDesc.BytesPerLine,
                        GetStbPixelLayout(src_desc)
                    );
                }

                // Compress from temp buffer to destination
                if (success) {
                    success = VxConvertToDXT(tempDesc, dst_desc, TRUE);
                }

                // Always clean up
                delete[] tempBuffer;
            }
            return;
        }

        // Case 2: Convert from DXT to standard
        if (srcFormat >= _DXT1 && srcFormat <= _DXT5 && dstFormat < _DXT1) {
            VxImageDescEx tempDesc;
            tempDesc.Width = src_desc.Width;
            tempDesc.Height = src_desc.Height;
            tempDesc.BitsPerPixel = 32;
            tempDesc.BytesPerLine = src_desc.Width * 4;
            tempDesc.RedMask = 0x00FF0000;
            tempDesc.GreenMask = 0x0000FF00;
            tempDesc.BlueMask = 0x000000FF;
            tempDesc.AlphaMask = 0xFF000000;

            // Check for overflow
            if (tempDesc.Height > 0 && tempDesc.BytesPerLine > INT_MAX / tempDesc.Height) {
                return;
            }

            XBYTE *tempBuffer = new XBYTE[tempDesc.BytesPerLine * tempDesc.Height];
            if (!tempBuffer) return;

            tempDesc.Image = tempBuffer;

            bool success = true;

            // Decompress to the temporary buffer
            if (success) {
                success = VxDecompressDXT(src_desc, tempDesc);
            }

            // If destination size matches, convert directly
            if (success) {
                if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
                    ConvertFormats(tempDesc, dst_desc);
                } else {
                    // Need to resize
                    stbir_resize_uint8_linear(
                        tempDesc.Image, tempDesc.Width, tempDesc.Height, tempDesc.BytesPerLine,
                        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
                        GetStbPixelLayout(tempDesc)
                    );
                }
            }

            // Always clean up
            delete[] tempBuffer;
            return;
        }

        // Case 3: Convert between DXT formats (not supported)
        if (srcFormat >= _DXT1 && srcFormat <= _DXT5 &&
            dstFormat >= _DXT1 && dstFormat <= _DXT5) {
            return;
        }
    }

    // Special case for identical dimensions but different formats
    if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
        ConvertFormats(src_desc, dst_desc);
        return;
    }

    // General case: resize and convert format using STB
    stbir_pixel_layout srcLayout = GetStbPixelLayout(src_desc);
    stbir_datatype dataType;

    // Determine data type
    if (src_desc.BitsPerPixel <= 8) {
        dataType = STBIR_TYPE_UINT8;
    } else if (src_desc.BitsPerPixel == 16) {
        dataType = STBIR_TYPE_UINT16;
    } else {
        dataType = STBIR_TYPE_UINT8;
    }

    // Use the medium complexity API for resizing with format conversion
    stbir_resize(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
        srcLayout, dataType,
        STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL
    );
}

/**
 * @brief Copy an image upside-down, optionally converting formats
 *
 * @param src_desc Source image descriptor
 * @param dst_desc Destination image descriptor
 */
void VxDoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    // Verify valid image data
    if (!src_desc.Image || !dst_desc.Image) {
        return;
    }

    // Dimensions must match
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) {
        return;
    }

    // Check for compressed formats
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    // DXT formats require special handling
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 ||
        dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        // For DXT formats, we would need to decompress, flip, and possibly recompress
        // This is a complex operation and not implemented here
        return;
    }

    // Fast path for identical format
    if (srcFormat == dstFormat && srcFormat != UNKNOWN_PF) {
        // Copy scan lines in reverse order
        int bytesPerLine = src_desc.BytesPerLine;

        for (int y = 0; y < src_desc.Height; y++) {
            XBYTE *srcLine = src_desc.Image + y * bytesPerLine;
            XBYTE *dstLine = dst_desc.Image + (src_desc.Height - 1 - y) * bytesPerLine;
            memcpy(dstLine, srcLine, bytesPerLine);
        }
        return;
    }

    // Different format - convert each pixel
    int srcBpp = src_desc.BitsPerPixel / 8;
    int dstBpp = dst_desc.BitsPerPixel / 8;

    for (int y = 0; y < src_desc.Height; y++) {
        for (int x = 0; x < src_desc.Width; x++) {
            // Read source pixel
            XBYTE *srcPtr = src_desc.Image + y * src_desc.BytesPerLine + x * srcBpp;
            XBYTE *dstPtr = dst_desc.Image +
                (src_desc.Height - 1 - y) * dst_desc.BytesPerLine + x * dstBpp;

            XULONG pixel = ReadPixel(srcPtr, srcBpp);
            XULONG dstPixel = ConvertPixel(pixel, src_desc, dst_desc);

            // Write destination pixel
            WritePixel(dstPtr, dstBpp, dstPixel);
        }
    }
}

/**
 * @brief Set the alpha channel for all pixels to a specific value
 *
 * @param dst_desc Destination image descriptor
 * @param AlphaValue Alpha value to set (0-255)
 */
void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue) {
    if (!dst_desc.AlphaMask || !dst_desc.Image) {
        return;
    }

    // Check for compressed formats
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(dst_desc);
    if (format >= _DXT1 && format <= _DXT5) {
        // For DXT formats, alpha manipulation would require decompressing,
        // modifying pixels, and recompressing - not implemented here
        return;
    }

    // Get alpha channel info
    XULONG alphaShift = GetBitShift(dst_desc.AlphaMask);
    XULONG alphaBits = GetBitCount(dst_desc.AlphaMask);
    int bytesPerPixel = dst_desc.BitsPerPixel / 8;

    // Scale alpha value to the alpha channel bit depth
    if (alphaBits < 8) {
        AlphaValue = (AlphaValue * ((1 << alphaBits) - 1)) / 255;
    }

    // Fast path for 32-bit ARGB (most common case)
    if (dst_desc.BitsPerPixel == 32 && dst_desc.AlphaMask == 0xFF000000) {
        XULONG alphaComponent = (XULONG) AlphaValue << 24;
        XULONG alphaMask = 0x00FFFFFF; // Mask to keep RGB components

        for (int y = 0; y < dst_desc.Height; y++) {
            XULONG *row = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);

            for (int x = 0; x < dst_desc.Width; x++) {
                row[x] = (row[x] & alphaMask) | alphaComponent;
            }
        }
        return;
    }

    // General case for all formats
    for (int y = 0; y < dst_desc.Height; y++) {
        XBYTE *row = dst_desc.Image + y * dst_desc.BytesPerLine;

        for (int x = 0; x < dst_desc.Width; x++) {
            XBYTE *pixel = row + x * bytesPerPixel;

            switch (dst_desc.BitsPerPixel) {
            case 16: {
                unsigned short *p = (unsigned short *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) |
                    ((AlphaValue << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            case 24: {
                // Determine which byte contains the alpha component
                int byteOffset = alphaShift / 8;
                int bitOffset = alphaShift % 8;

                // Create mask for the relevant byte
                XBYTE byteMask = (dst_desc.AlphaMask >> (byteOffset * 8)) & 0xFF;

                // Update just that byte
                pixel[byteOffset] = (pixel[byteOffset] & ~byteMask) |
                    ((AlphaValue << bitOffset) & byteMask);
                break;
            }
            case 32: {
                XULONG *p = (XULONG *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) |
                    ((AlphaValue << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            }
        }
    }
}

/**
 * @brief Set the alpha channel using an array of alpha values
 *
 * @param dst_desc Destination image descriptor
 * @param AlphaValues Array of alpha values (one per pixel)
 */
void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues) {
    if (!dst_desc.AlphaMask || !dst_desc.Image || !AlphaValues) {
        return;
    }

    // Check for compressed formats
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(dst_desc);
    if (format >= _DXT1 && format <= _DXT5) {
        // For DXT formats, alpha manipulation would require decompressing,
        // modifying pixels, and recompressing - not implemented here
        return;
    }

    // Get alpha channel info
    XULONG alphaShift = GetBitShift(dst_desc.AlphaMask);
    XULONG alphaBits = GetBitCount(dst_desc.AlphaMask);
    int bytesPerPixel = dst_desc.BitsPerPixel / 8;

    // Fast path for 32-bit ARGB
    if (dst_desc.BitsPerPixel == 32 && dst_desc.AlphaMask == 0xFF000000) {
        XULONG alphaMask = 0x00FFFFFF; // Mask to keep RGB components

        for (int y = 0; y < dst_desc.Height; y++) {
            XULONG *row = (XULONG *) (dst_desc.Image + y * dst_desc.BytesPerLine);

            for (int x = 0; x < dst_desc.Width; x++) {
                int index = y * dst_desc.Width + x;
                XULONG alphaComponent = (XULONG) AlphaValues[index] << 24;
                row[x] = (row[x] & alphaMask) | alphaComponent;
            }
        }
        return;
    }

    // General case for all formats
    for (int y = 0; y < dst_desc.Height; y++) {
        XBYTE *row = dst_desc.Image + y * dst_desc.BytesPerLine;

        for (int x = 0; x < dst_desc.Width; x++) {
            XBYTE *pixel = row + x * bytesPerPixel;
            int index = y * dst_desc.Width + x;
            XBYTE alpha = AlphaValues[index];

            // Scale alpha value to the alpha channel bit depth
            if (alphaBits < 8) {
                alpha = (alpha * ((1 << alphaBits) - 1)) / 255;
            }

            switch (dst_desc.BitsPerPixel) {
            case 16: {
                unsigned short *p = (unsigned short *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) |
                    ((alpha << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            case 24: {
                // Determine which byte contains the alpha component
                int byteOffset = alphaShift / 8;
                int bitOffset = alphaShift % 8;

                // Create mask for the relevant byte
                XBYTE byteMask = (dst_desc.AlphaMask >> (byteOffset * 8)) & 0xFF;

                // Update just that byte
                pixel[byteOffset] = (pixel[byteOffset] & ~byteMask) |
                    ((alpha << bitOffset) & byteMask);
                break;
            }
            case 32: {
                XULONG *p = (XULONG *) pixel;
                *p = (*p & ~dst_desc.AlphaMask) |
                    ((alpha << alphaShift) & dst_desc.AlphaMask);
                break;
            }
            }
        }
    }
}

/**
 * @brief Resize a 32-bit image, optimized for speed
 *
 * @param src_desc Source image (must be 32-bit format)
 * @param dst_desc Destination image (must be 32-bit format)
 */
void VxResizeImage32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    // Check for 32-bit format
    if (src_desc.BitsPerPixel != 32 || dst_desc.BitsPerPixel != 32) {
        return;
    }

    // Check for valid pointers
    if (!src_desc.Image || !dst_desc.Image) {
        return;
    }

    // Check for compressed formats
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    VX_PIXELFORMAT dstFormat = VxImageDesc2PixelFormat(dst_desc);

    if (srcFormat >= _DXT1 && srcFormat <= _DXT5 ||
        dstFormat >= _DXT1 && dstFormat <= _DXT5) {
        // Cannot resize compressed formats directly
        return;
    }

    // Use the appropriate STB resize function - we want linear processing for 32-bit
    stbir_pixel_layout layout = GetStbPixelLayout(src_desc);

    stbir_resize_uint8_linear(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        dst_desc.Image, dst_desc.Width, dst_desc.Height, dst_desc.BytesPerLine,
        layout
    );
}

/**
 * @brief Generate a mipmap level (half-size) from an image
 * Only works on 32 bpp images with power-of-2 dimensions
 *
 * @param src_desc Source image (must be 32-bit, power-of-2 dimensions)
 * @param Buffer Destination buffer (must be pre-allocated to (Width/2 * Height/2) DWORDs)
 */
void VxGenerateMipMap(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    // Validate 32 bpp requirement
    if (src_desc.BitsPerPixel != 32 || !src_desc.Image || !Buffer) {
        return;
    }

    // Validate power-of-2 dimensions
    if (src_desc.Width < 2 || src_desc.Height < 2 ||
        (src_desc.Width & (src_desc.Width - 1)) != 0 ||
        (src_desc.Height & (src_desc.Height - 1)) != 0) {
        return;
        }

    // Check for compressed formats (not supported)
    VX_PIXELFORMAT srcFormat = VxImageDesc2PixelFormat(src_desc);
    if (srcFormat >= _DXT1 && srcFormat <= _DXT5) {
        return;
    }

    int dstWidth = src_desc.Width / 2;
    int dstHeight = src_desc.Height / 2;
    int dstPitch = dstWidth * 4;  // 32 bpp = 4 bytes per pixel

    // Create destination descriptor for STB
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

    // Use STB for high-quality downsampling
    stbir_pixel_layout layout = GetStbPixelLayout(src_desc);

    stbir_resize_uint8_linear(
        src_desc.Image, src_desc.Width, src_desc.Height, src_desc.BytesPerLine,
        Buffer, dstWidth, dstHeight, dstPitch,
        layout
    );
}

//------------------------------------------------------------------------------
// Normal and bump map generation
//------------------------------------------------------------------------------

/**
 * @brief Convert an image to a normal map
 * Only works for 32 bit per pixel bitmaps
 *
 * @param image Image to convert (modified in-place, must be 32 bpp)
 * @param ColorMask Mask for the component to use as height map
 * @return TRUE if successful, FALSE otherwise
 */
XBOOL VxConvertToNormalMap(const VxImageDescEx &image, XDWORD ColorMask) {
    // Validate 32 bpp requirement
    if (image.BitsPerPixel != 32 || !image.Image) {
        return FALSE;
    }

    if (image.Width <= 0 || image.Height <= 0) {
        return FALSE;
    }

    // Check for compressed formats
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(image);
    if (format >= _DXT1 && format <= _DXT5) {
        return FALSE;
    }

    // Handle special ColorMask values
    XULONG actualMask;
    if (ColorMask == 0xFFFFFFFF) {
        // Overall intensity - use all color channels
        actualMask = image.RedMask | image.GreenMask | image.BlueMask;
    } else {
        actualMask = ColorMask;
    }

    if (actualMask == 0) {
        return FALSE;
    }

    const int pixelCount = image.Width * image.Height;

    // Allocate temporary height map
    float *heightMap = new(std::nothrow) float[pixelCount];
    if (!heightMap) return FALSE;

    // Extract height values
    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            int index = y * image.Width + x;

            int offset = SafePixelOffset(image, x, y, 4);
            if (offset < 0) {
                heightMap[index] = 0.5f;
                continue;
            }

            const XULONG *pixel = (const XULONG *)(image.Image + offset);

            float height;
            if (ColorMask == 0xFFFFFFFF) {
                // Overall intensity - calculate luminance
                unsigned char r, g, b, a;
                ExtractRGBA(*pixel, image, r, g, b, a);
                height = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
            } else {
                // Extract specific component
                XULONG shift = GetBitShift(actualMask);
                XULONG bits = GetBitCount(actualMask);
                XULONG val = (*pixel & actualMask) >> shift;

                if (bits > 0 && bits < 8) {
                    val = (val * 255) / ((1 << bits) - 1);
                }
                height = (float)val / 255.0f;
            }

            heightMap[index] = height;
        }
    }

    // Calculate normals using Sobel operator
    const float scale = 2.0f;  // Normal map strength

    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            int index = y * image.Width + x;

            // Sample neighboring pixels with edge clamping
            auto getHeight = [&](int px, int py) -> float {
                px = (px < 0) ? 0 : (px >= image.Width) ? image.Width - 1 : px;
                py = (py < 0) ? 0 : (py >= image.Height) ? image.Height - 1 : py;
                return heightMap[py * image.Width + px];
            };

            // Sobel filter kernel
            float h00 = getHeight(x-1, y-1), h01 = getHeight(x, y-1), h02 = getHeight(x+1, y-1);
            float h10 = getHeight(x-1, y),                            h12 = getHeight(x+1, y);
            float h20 = getHeight(x-1, y+1), h21 = getHeight(x, y+1), h22 = getHeight(x+1, y+1);

            // Calculate gradients
            float gx = (h00 + 2.0f*h10 + h20) - (h02 + 2.0f*h12 + h22);
            float gy = (h00 + 2.0f*h01 + h02) - (h20 + 2.0f*h21 + h22);

            // Calculate normal vector
            float nx = -gx * scale;
            float ny = -gy * scale;
            float nz = 1.0f;

            // Normalize
            float length = sqrtf(nx*nx + ny*ny + nz*nz);
            if (length > 0.0001f) {
                nx /= length;
                ny /= length;
                nz /= length;
            } else {
                nx = 0.0f; ny = 0.0f; nz = 1.0f;
            }

            // Convert to [0,255] range and write back
            int offset = SafePixelOffset(image, x, y, 4);
            if (offset >= 0) {
                XULONG *pixel = (XULONG *)(image.Image + offset);

                int r = (int)((nx + 1.0f) * 127.5f);
                int g = (int)((ny + 1.0f) * 127.5f);
                int b = (int)((nz + 1.0f) * 127.5f);

                // Clamp values
                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;

                // Preserve alpha, update RGB
                *pixel = (*pixel & image.AlphaMask) |
                         CreatePixel(r, g, b, 255, image);
            }
        }
    }

    delete[] heightMap;
    return TRUE;
}

/**
 * @brief Convert an RGB displacement image to a bump map (du,dv,Luminance)
 * Only works for 32 bit per pixel bitmaps
 *
 * @param image Image to convert (modified in-place, must be 32 bpp)
 * @return TRUE if successful, FALSE otherwise
 */
XBOOL VxConvertToBumpMap(const VxImageDescEx &image) {
    // Validate 32 bpp requirement
    if (image.BitsPerPixel != 32 || !image.Image) {
        return FALSE;
    }

    if (image.Width <= 0 || image.Height <= 0) {
        return FALSE;
    }

    // Check for compressed formats
    VX_PIXELFORMAT format = VxImageDesc2PixelFormat(image);
    if (format >= _DXT1 && format <= _DXT5) {
        return FALSE;
    }

    // Convert RGB displacement to du,dv,luminance
    // du = horizontal displacement (stored in R)
    // dv = vertical displacement (stored in G)
    // luminance = brightness (stored in B)

    for (int y = 0; y < image.Height; y++) {
        for (int x = 0; x < image.Width; x++) {
            int offset = SafePixelOffset(image, x, y, 4);
            if (offset < 0) continue;

            XULONG *pixel = (XULONG *)(image.Image + offset);

            // Extract original RGB components
            unsigned char r, g, b, a;
            ExtractRGBA(*pixel, image, r, g, b, a);

            // Calculate luminance (perceived brightness)
            float luminance = 0.299f * r + 0.587f * g + 0.114f * b;

            // Calculate displacement gradients by sampling neighbors
            auto getSafePixel = [&](int px, int py) -> float {
                if (px < 0 || px >= image.Width || py < 0 || py >= image.Height) {
                    return luminance; // Use current pixel luminance for edges
                }

                int poffset = SafePixelOffset(image, px, py, 4);
                if (poffset < 0) return luminance;

                const XULONG *ppixel = (const XULONG *)(image.Image + poffset);
                unsigned char pr, pg, pb, pa;
                ExtractRGBA(*ppixel, image, pr, pg, pb, pa);
                return 0.299f * pr + 0.587f * pg + 0.114f * pb;
            };

            // Calculate du (horizontal displacement) using central difference
            float leftLum = getSafePixel(x - 1, y);
            float rightLum = getSafePixel(x + 1, y);
            float du = (rightLum - leftLum) * 0.5f;

            // Calculate dv (vertical displacement) using central difference
            float topLum = getSafePixel(x, y - 1);
            float bottomLum = getSafePixel(x, y + 1);
            float dv = (bottomLum - topLum) * 0.5f;

            // Convert to [0,255] range with 128 as neutral (no displacement)
            int duVal = (int)(du + 128.0f);
            int dvVal = (int)(dv + 128.0f);
            int lumVal = (int)luminance;

            // Clamp values
            duVal = (duVal < 0) ? 0 : (duVal > 255) ? 255 : duVal;
            dvVal = (dvVal < 0) ? 0 : (dvVal > 255) ? 255 : dvVal;
            lumVal = (lumVal < 0) ? 0 : (lumVal > 255) ? 255 : lumVal;

            // Store du,dv,luminance in R,G,B respectively, preserve alpha
            *pixel = CreatePixel(duVal, dvVal, lumVal, a, image);
        }
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// Quantization utilities
//------------------------------------------------------------------------------

/**
 * @brief Get the current sampling factor for image quantization
 *
 * @return The current sampling factor
 */
int GetQuantizationSamplingFactor() {
    return g_QuantizationSamplingFactor;
}

/**
 * @brief Set the sampling factor for image quantization
 *
 * @param factor The sampling factor (1-30, higher = better quality but slower)
 */
void SetQuantizationSamplingFactor(int factor) {
    // Clamp to reasonable range
    if (factor < 1) factor = 1;
    if (factor > 30) factor = 30;

    g_QuantizationSamplingFactor = factor;
}
