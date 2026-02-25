#ifndef VXBLITINTERNAL_H
#define VXBLITINTERNAL_H

/**
 * @file VxBlitInternal.h
 * @brief Shared inline utilities for the VxBlitEngine family of translation units.
 *
 * This header is private to the VxMath library implementation.  It is included
 * by VxBlitKernels.cpp, VxBlitEngineSSE2.cpp, VxBlitEngineSSSE3.cpp,
 * VxBlitEngineAVX2.cpp, and VxBlitEngineClass.cpp.  It must NOT be included
 * from any public header.
 */

#include "VxBlitEngine.h"

#include <cstring>

//==============================================================================
// Bit Manipulation Helpers
//==============================================================================

/// Get the position of the least-significant set bit in @p dwMask.
static inline XDWORD GetBitShiftLocal(XDWORD dwMask) {
    if (!dwMask) return 0;
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    unsigned long index;
    _BitScanForward(&index, dwMask);
    return index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(dwMask);
#else
    XDWORD shift = 0;
    while ((dwMask & 1) == 0) {
        dwMask >>= 1;
        ++shift;
    }
    return shift;
#endif
}

/// Get the number of set bits (popcount) in @p dwMask.
static inline XDWORD GetBitCountLocal(XDWORD dwMask) {
    if (!dwMask) return 0;
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __popcnt(dwMask);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(dwMask);
#else
    XDWORD count = 0;
    while (dwMask) {
        count += dwMask & 1;
        dwMask >>= 1;
    }
    return count;
#endif
}

//==============================================================================
// Variable-Width Pixel I/O
//==============================================================================

/// Read a pixel of @p bpp bytes from @p ptr (1..4).
static inline XDWORD ReadPixel(const XBYTE *ptr, int bpp) {
    switch (bpp) {
    case 1: return *ptr;
    case 2: return *(const XWORD *)ptr;
    case 3: return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
    case 4: return *(const XDWORD *)ptr;
    default: return 0;
    }
}

/// Write a pixel of @p bpp bytes to @p ptr (1..4).
static inline void WritePixel(XBYTE *ptr, int bpp, XDWORD pixel) {
    switch (bpp) {
    case 1: *ptr = (XBYTE)pixel; break;
    case 2: *(XWORD *)ptr = (XWORD)pixel; break;
    case 3:
        ptr[0] = (XBYTE)pixel;
        ptr[1] = (XBYTE)(pixel >> 8);
        ptr[2] = (XBYTE)(pixel >> 16);
        break;
    case 4: *(XDWORD *)ptr = pixel; break;
    }
}

//==============================================================================
// Pixel Format Table (shared definition)
//==============================================================================

/// Compact description of a standard pixel format.
struct PixelFormatDef {
    XWORD BitsPerPixel;
    XDWORD RedMask;
    XDWORD GreenMask;
    XDWORD BlueMask;
    XDWORD AlphaMask;
    const char *Description;
};

/// Master table of all known pixel formats.  The index matches VX_PIXELFORMAT.
static const PixelFormatDef s_PixelFormats[] = {
    {0,  0,          0,          0,          0,          "Unknown"},
    {32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, "32 bits ARGB 8888"},
    {32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, "32 bits RGB  888"},
    {24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, "24 bits RGB  888"},
    {16, 0xF800,     0x07E0,     0x001F,     0x0000,     "16 bits RGB  565"},
    {16, 0x7C00,     0x03E0,     0x001F,     0x0000,     "16 bits RGB  555"},
    {16, 0x7C00,     0x03E0,     0x001F,     0x8000,     "16 bits ARGB 1555"},
    {16, 0x0F00,     0x00F0,     0x000F,     0xF000,     "16 bits ARGB 4444"},
    {8,  0xE0,       0x1C,       0x03,       0x00,       "8 bits RGB  332"},
    {8,  0x30,       0x0C,       0x03,       0xC0,       "8 bits ARGB 2222"},
    {32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000, "32 bits ABGR 8888"},
    {32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF, "32 bits RGBA 8888"},
    {32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF, "32 bits BGRA 8888"},
    {32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000, "32 bits BGR  888"},
    {24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000, "24 bits BGR  888"},
    {16, 0x001F,     0x07E0,     0xF800,     0x0000,     "16 bits BGR  565"},
    {16, 0x001F,     0x03E0,     0x7C00,     0x0000,     "16 bits BGR  555"},
    {16, 0x001F,     0x03E0,     0x7C00,     0x8000,     "16 bits ABGR 1555"},
    {16, 0x000F,     0x00F0,     0x0F00,     0xF000,     "16 bits ABGR 4444"},
    // DXT formats
    {4,  0, 0, 0, 0, "Compressed DXT1"},
    {8,  0, 0, 0, 0, "Compressed DXT2"},
    {8,  0, 0, 0, 0, "Compressed DXT3"},
    {8,  0, 0, 0, 0, "Compressed DXT4"},
    {8,  0, 0, 0, 0, "Compressed DXT5"},
    // Bump map formats
    {16, 0x00FF,     0xFF00,     0x0000,     0x0000,     "V8U8 BumpMap"},
    {32, 0xFFFF,     0xFFFF0000, 0x0000,     0x0000,     "V16U16 BumpMap"},
    {16, 0x001F,     0x03E0,     0x7C00,     0x0000,     "L6V5U5 BumpMap"},
    {32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000, "L8V8U8 BumpMap"}
};

static const int NUM_PIXEL_FORMATS = sizeof(s_PixelFormats) / sizeof(s_PixelFormats[0]);

/// Number of blittable (non-DXT/non-bump) formats: indices 0-18.
static const int NUM_BLITTABLE_FORMATS = 19;

//==============================================================================
// Forward declarations -- scalar tail-loop functions (defined in VxBlitKernels.cpp)
//
// SSE / AVX2 SIMD loops call these to process remaining pixels.
//==============================================================================

// --- Format conversion tail variants -----------------------------------------
void CopyLine_32ARGB_32RGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_24RGB_Scalar(const VxBlitInfo *info, int startX, const XBYTE *src, XBYTE *dst);
void CopyLine_24RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX, const XBYTE *src, XDWORD *dst);
void CopyLine_32ARGB_565RGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_555RGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_1555ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_4444ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_565RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_555RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_1555ARGB_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_4444ARGB_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_32ABGR_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_32RGBA_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32RGBA_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void CopyLine_32ARGB_32BGRA_Scalar(const VxBlitInfo *info, int startX);

// --- Alpha / image-operation tail variants -----------------------------------
void PremultiplyAlpha_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void UnpremultiplyAlpha_32ARGB_Scalar(const VxBlitInfo *info, int startX);
void FillLine_32_Scalar(XDWORD *dst, int startX, int width, XDWORD color);
void FillLine_16_Scalar(XWORD *dst, int startX, int width, XWORD color);
void ClearAlpha_32_Scalar(XDWORD *dst, int startX, int width);
void SetFullAlpha_32_Scalar(XDWORD *dst, int startX, int width);
void InvertColors_32_Scalar(XDWORD *dst, int startX, int width);
void Grayscale_32_Scalar(const XDWORD *src, XDWORD *dst, int startX, int width);
void MultiplyBlend_32_Scalar(const XDWORD *src, XDWORD *dst, int startX, int width);
void SetAlpha_32_Scalar(XDWORD *dst, int startX, int width, XDWORD alphaValue, XDWORD alphaMaskInv);
void CopyAlpha_32_Scalar(const XBYTE *src, XDWORD *dst, int startX, int width,
                          int alphaShift, XDWORD alphaMask, XDWORD alphaMaskInv);

//==============================================================================
// Forward declarations -- scalar dispatch-table entries (VxBlitKernels.cpp)
//
// These are the functions stored in VxBlitEngine dispatch tables.
//==============================================================================

// Generic blit functions (template instantiations)
void CopyLineSame(const VxBlitInfo *info);
void CopyLineGeneric_8_8(const VxBlitInfo *i);
void CopyLineGeneric_8_16(const VxBlitInfo *i);
void CopyLineGeneric_8_24(const VxBlitInfo *i);
void CopyLineGeneric_8_32(const VxBlitInfo *i);
void CopyLineGeneric_16_8(const VxBlitInfo *i);
void CopyLineGeneric_16_16(const VxBlitInfo *i);
void CopyLineGeneric_16_24(const VxBlitInfo *i);
void CopyLineGeneric_16_32(const VxBlitInfo *i);
void CopyLineGeneric_24_8(const VxBlitInfo *i);
void CopyLineGeneric_24_16(const VxBlitInfo *i);
void CopyLineGeneric_24_24(const VxBlitInfo *i);
void CopyLineGeneric_24_32(const VxBlitInfo *i);
void CopyLineGeneric_32_8(const VxBlitInfo *i);
void CopyLineGeneric_32_16(const VxBlitInfo *i);
void CopyLineGeneric_32_24(const VxBlitInfo *i);
void CopyLineGeneric_32_32(const VxBlitInfo *i);

// Specific format conversions (scalar optimised)
void CopyLine_32ARGB_32RGB(const VxBlitInfo *info);
void CopyLine_32RGB_32ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_24RGB(const VxBlitInfo *info);
void CopyLine_24RGB_32ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_565RGB(const VxBlitInfo *info);
void CopyLine_32ARGB_555RGB(const VxBlitInfo *info);
void CopyLine_32ARGB_1555ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_4444ARGB(const VxBlitInfo *info);
void CopyLine_565RGB_32ARGB(const VxBlitInfo *info);
void CopyLine_555RGB_32ARGB(const VxBlitInfo *info);
void CopyLine_1555ARGB_32ARGB(const VxBlitInfo *info);
void CopyLine_4444ARGB_32ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_32ABGR(const VxBlitInfo *info);
void CopyLine_32ABGR_32ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_32RGBA(const VxBlitInfo *info);
void CopyLine_32RGBA_32ARGB(const VxBlitInfo *info);
void CopyLine_32ARGB_32BGRA(const VxBlitInfo *info);
void CopyLine_32BGRA_32ARGB(const VxBlitInfo *info);

// Paletted conversions
void CopyLine_Paletted8_32ARGB(const VxBlitInfo *info);
void CopyLine_Paletted8_24RGB(const VxBlitInfo *info);
void CopyLine_Paletted8_565RGB(const VxBlitInfo *info);
void CopyLine_Paletted8_16Alpha(const VxBlitInfo *info);
void CopyLine_Paletted8_8(const VxBlitInfo *info);

// Alpha operations (scalar)
void SetAlpha_8(const VxBlitInfo *info);
void SetAlpha_16(const VxBlitInfo *info);
void SetAlpha_32(const VxBlitInfo *info);
void CopyAlpha_8(const VxBlitInfo *info);
void CopyAlpha_16(const VxBlitInfo *info);
void CopyAlpha_32(const VxBlitInfo *info);

// Premultiply/unpremultiply (scalar, dispatch-table form)
void PremultiplyAlpha_32ARGB(const VxBlitInfo *info);
void UnpremultiplyAlpha_32ARGB(const VxBlitInfo *info);

// Resize functions
void ResizeHLine_EqualY_EqualX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_EqualY_ShrinkX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_EqualY_GrowX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_ShrinkY_EqualX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_ShrinkY_ShrinkX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_ShrinkY_GrowX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_GrowY_EqualX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_GrowY_ShrinkX_32Bpp(const VxResizeInfo *info);
void ResizeHLine_GrowY_GrowX_32Bpp(const VxResizeInfo *info);

//==============================================================================
// Forward declarations -- SSE2/SSSE3 dispatch-table entries
//==============================================================================

#if defined(VX_SIMD_SSE2)
// SSE2-only kernels (VxBlitEngineSSE2.cpp)
void CopyLine_32ARGB_32RGB_SSE(const VxBlitInfo *info);
void CopyLine_32RGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_565RGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_555RGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_1555ARGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_4444ARGB_SSE(const VxBlitInfo *info);
void CopyLine_565RGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_555RGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_1555ARGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_4444ARGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_Paletted8_32ARGB_SSE(const VxBlitInfo *info);
void SetAlpha_32_SSE(const VxBlitInfo *info);
void CopyAlpha_32_SSE(const VxBlitInfo *info);
void FillLine_32_SSE(XDWORD *dst, int width, XDWORD color);
void FillLine_16_SSE(XWORD *dst, int width, XWORD color);
void PremultiplyAlpha_32ARGB_SSE(const VxBlitInfo *info);
void UnpremultiplyAlpha_32ARGB_SSE(const VxBlitInfo *info);
void ClearAlpha_32_SSE(const VxBlitInfo *info);
void SetFullAlpha_32_SSE(const VxBlitInfo *info);
void InvertColors_32_SSE(const VxBlitInfo *info);
void Grayscale_32_SSE(const VxBlitInfo *info);
void MultiplyBlend_32_SSE(const VxBlitInfo *info);

// SSSE3 kernels -- compiled with SSE2+SIMDe; installed only when hasSSSE3
// (VxBlitEngineSSSE3.cpp)
void CopyLine_24RGB_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_24RGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_32ABGR_SSE(const VxBlitInfo *info);
void CopyLine_32ABGR_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_32RGBA_SSE(const VxBlitInfo *info);
void CopyLine_32RGBA_32ARGB_SSE(const VxBlitInfo *info);
void CopyLine_32ARGB_32BGRA_SSE(const VxBlitInfo *info);
void CopyLine_32BGRA_32ARGB_SSE(const VxBlitInfo *info);
#endif // VX_SIMD_SSE2

//==============================================================================
// Forward declarations -- AVX2 dispatch-table entries (VxBlitEngineAVX2.cpp)
//==============================================================================

#if defined(VX_SIMD_AVX2)
void CopyLine_32ARGB_32RGB_AVX2(const VxBlitInfo *info);
void CopyLine_32RGB_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_32ABGR_AVX2(const VxBlitInfo *info);
void CopyLine_32ABGR_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_32RGBA_AVX2(const VxBlitInfo *info);
void CopyLine_32RGBA_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_32BGRA_AVX2(const VxBlitInfo *info);
void CopyLine_32BGRA_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_24RGB_AVX2(const VxBlitInfo *info);
void CopyLine_24RGB_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_Paletted8_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_565RGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_555RGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_1555ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_32ARGB_4444ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_565RGB_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_555RGB_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_1555ARGB_32ARGB_AVX2(const VxBlitInfo *info);
void CopyLine_4444ARGB_32ARGB_AVX2(const VxBlitInfo *info);
void FillLine_32_AVX2(XDWORD *dst, int width, XDWORD color);
void FillLine_16_AVX2(XWORD *dst, int width, XWORD color);
void PremultiplyAlpha_32ARGB_AVX2(const VxBlitInfo *info);
void UnpremultiplyAlpha_32ARGB_AVX2(const VxBlitInfo *info);
void ClearAlpha_32_AVX2(const VxBlitInfo *info);
void SetFullAlpha_32_AVX2(const VxBlitInfo *info);
void InvertColors_32_AVX2(const VxBlitInfo *info);
void Grayscale_32_AVX2(const VxBlitInfo *info);
void SetAlpha_32_AVX2(const VxBlitInfo *info);
void CopyAlpha_32_AVX2(const VxBlitInfo *info);
void MultiplyBlend_32_AVX2(const VxBlitInfo *info);
#endif // VX_SIMD_AVX2

#endif // VXBLITINTERNAL_H
