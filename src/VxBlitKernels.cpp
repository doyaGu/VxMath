/**
 * @file VxBlitKernels.cpp
 * @brief Scalar (C++) blit kernels for VxBlitEngine.
 *
 * This file contains all non-SIMD pixel format conversion, alpha, resize,
 * and utility kernels.  Each function that has a corresponding SIMD variant
 * provides two entry points:
 *
 *   FuncName(const VxBlitInfo *)        -- dispatch-table entry (full scanline)
 *   FuncName_Scalar(info, startX, ...)  -- SIMD tail-loop entry (partial)
 *
 * Only ASCII characters are used in this file (no extended Unicode) to avoid
 * MSVC C4819 warnings on CJK code pages.
 */

#include "VxBlitInternal.h"

#include <algorithm>

#include "VxMath.h"

//==============================================================================
//  Section 1 -- Generic Blit Functions
//==============================================================================

// Same format copy -- just memcpy
void CopyLineSame(const VxBlitInfo *info) {
    memcpy(info->dstLine, info->srcLine, info->copyBytes);
}

// Helper to convert a component from one bit width to another
static inline XDWORD ConvertComponent(XDWORD value, int srcBits, int dstBits, int dstShift) {
    if (srcBits == 0) return 0;
    if (dstBits == 0) return 0;

    // Scale the value from srcBits to dstBits
    if (srcBits > dstBits) {
        // Truncate: take the MSBs (shift right)
        value >>= (srcBits - dstBits);
    } else if (srcBits < dstBits) {
        // Expand: replicate bits to fill
        // E.g., 5-bit 0x1F to 8-bit should give 0xFF
        int shift = dstBits - srcBits;
        XDWORD expanded = value << shift;
        // Replicate lower bits for proper expansion
        if (srcBits > 0 && shift > 0) {
            XDWORD rep = value >> (srcBits - std::min(srcBits, shift));
            expanded |= rep;
        }
        value = expanded;
    }
    // Position the value at the destination shift
    return value << dstShift;
}

// Generic pixel conversion with proper bit width handling
template<int SrcBpp, int DstBpp>
static void CopyLineGeneric(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XDWORD pixel = ReadPixel(src, SrcBpp);

        // Extract components (shift to bit 0)
        XDWORD r = info->srcRedMask ? ((pixel & info->srcRedMask) >> info->redShiftSrc) : 0;
        XDWORD g = info->srcGreenMask ? ((pixel & info->srcGreenMask) >> info->greenShiftSrc) : 0;
        XDWORD b = info->srcBlueMask ? ((pixel & info->srcBlueMask) >> info->blueShiftSrc) : 0;
        XDWORD a = info->srcAlphaMask ? ((pixel & info->srcAlphaMask) >> info->alphaShiftSrc) :
                   (info->alphaBitsDst > 0 ? ((1u << info->alphaBitsDst) - 1) : 0);

        // Convert bit widths and position
        XDWORD dstPixel = ConvertComponent(r, info->redBitsSrc, info->redBitsDst, info->redShiftDst) |
                          ConvertComponent(g, info->greenBitsSrc, info->greenBitsDst, info->greenShiftDst) |
                          ConvertComponent(b, info->blueBitsSrc, info->blueBitsDst, info->blueShiftDst) |
                          ConvertComponent(a, info->alphaBitsSrc, info->alphaBitsDst, info->alphaShiftDst);

        WritePixel(dst, DstBpp, dstPixel);

        src += SrcBpp;
        dst += DstBpp;
    }
}

// Explicit instantiations (dispatch-table wrappers)
void CopyLineGeneric_8_8(const VxBlitInfo *info)   { CopyLineGeneric<1, 1>(info); }
void CopyLineGeneric_8_16(const VxBlitInfo *info)  { CopyLineGeneric<1, 2>(info); }
void CopyLineGeneric_8_24(const VxBlitInfo *info)  { CopyLineGeneric<1, 3>(info); }
void CopyLineGeneric_8_32(const VxBlitInfo *info)  { CopyLineGeneric<1, 4>(info); }
void CopyLineGeneric_16_8(const VxBlitInfo *info)  { CopyLineGeneric<2, 1>(info); }
void CopyLineGeneric_16_16(const VxBlitInfo *info) { CopyLineGeneric<2, 2>(info); }
void CopyLineGeneric_16_24(const VxBlitInfo *info) { CopyLineGeneric<2, 3>(info); }
void CopyLineGeneric_16_32(const VxBlitInfo *info) { CopyLineGeneric<2, 4>(info); }
void CopyLineGeneric_24_8(const VxBlitInfo *info)  { CopyLineGeneric<3, 1>(info); }
void CopyLineGeneric_24_16(const VxBlitInfo *info) { CopyLineGeneric<3, 2>(info); }
void CopyLineGeneric_24_24(const VxBlitInfo *info) { CopyLineGeneric<3, 3>(info); }
void CopyLineGeneric_24_32(const VxBlitInfo *info) { CopyLineGeneric<3, 4>(info); }
void CopyLineGeneric_32_8(const VxBlitInfo *info)  { CopyLineGeneric<4, 1>(info); }
void CopyLineGeneric_32_16(const VxBlitInfo *info) { CopyLineGeneric<4, 2>(info); }
void CopyLineGeneric_32_24(const VxBlitInfo *info) { CopyLineGeneric<4, 3>(info); }
void CopyLineGeneric_32_32(const VxBlitInfo *info) { CopyLineGeneric<4, 4>(info); }

//==============================================================================
//  Section 2 -- 32-bit <-> 32-bit Conversions
//==============================================================================

// --- 32-bit ARGB -> 32-bit RGB (clear alpha) ---------------------------------

void CopyLine_32ARGB_32RGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        dst[x + 0] = src[x + 0] & 0x00FFFFFF;
        dst[x + 1] = src[x + 1] & 0x00FFFFFF;
        dst[x + 2] = src[x + 2] & 0x00FFFFFF;
        dst[x + 3] = src[x + 3] & 0x00FFFFFF;
    }
    for (; x < width; ++x) {
        dst[x] = src[x] & 0x00FFFFFF;
    }
}

void CopyLine_32ARGB_32RGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        dst[x] = src[x] & 0x00FFFFFF;
    }
}

// --- 32-bit RGB -> 32-bit ARGB (add full alpha) ------------------------------

void CopyLine_32RGB_32ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        dst[x + 0] = src[x + 0] | 0xFF000000;
        dst[x + 1] = src[x + 1] | 0xFF000000;
        dst[x + 2] = src[x + 2] | 0xFF000000;
        dst[x + 3] = src[x + 3] | 0xFF000000;
    }
    for (; x < width; ++x) {
        dst[x] = src[x] | 0xFF000000;
    }
}

void CopyLine_32RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        dst[x] = src[x] | 0xFF000000;
    }
}

//==============================================================================
//  Section 3 -- 32-bit <-> 24-bit Conversions
//==============================================================================

// --- 32-bit ARGB -> 24-bit RGB -----------------------------------------------

void CopyLine_32ARGB_24RGB(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        dst[0] = src[0]; // B
        dst[1] = src[1]; // G
        dst[2] = src[2]; // R
        src += 4;
        dst += 3;
    }
}

// Tail variant for SSE/AVX2.
// @param src  Source bytes (4 per pixel).  If nullptr, computed from info.
// @param dst  Destination bytes (3 per pixel), already advanced.
void CopyLine_32ARGB_24RGB_Scalar(const VxBlitInfo *info, int startX,
                                   const XBYTE *src, XBYTE *dst) {
    const int width = info->width;
    if (!src)
        src = info->srcLine + startX * 4;
    for (int x = startX; x < width; ++x) {
        dst[0] = src[0]; // B
        dst[1] = src[1]; // G
        dst[2] = src[2]; // R
        src += 4;
        dst += 3;
    }
}

// --- 24-bit RGB -> 32-bit ARGB -----------------------------------------------

void CopyLine_24RGB_32ARGB(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        dst[0] = src[0]; // B
        dst[1] = src[1]; // G
        dst[2] = src[2]; // R
        dst[3] = 0xFF;   // A
        src += 3;
        dst += 4;
    }
}

// Tail variant for SSE/AVX2.
// @param src  Source bytes (3 per pixel), already advanced.
// @param dst  Destination 32-bit pixels, already advanced.
void CopyLine_24RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX,
                                   const XBYTE *src, XDWORD *dst) {
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        *dst = src[0] | ((XDWORD)src[1] << 8) | ((XDWORD)src[2] << 16) | 0xFF000000;
        src += 3;
        ++dst;
    }
}

//==============================================================================
//  Section 4 -- 32-bit <-> 16-bit Conversions
//==============================================================================

// --- 32-bit ARGB -> 16-bit 565 -----------------------------------------------

void CopyLine_32ARGB_565RGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0], p1 = src[x + 1], p2 = src[x + 2], p3 = src[x + 3];
        dst[x + 0] = (XWORD)(((p0 >> 8) & 0xF800) | ((p0 >> 5) & 0x07E0) | ((p0 >> 3) & 0x001F));
        dst[x + 1] = (XWORD)(((p1 >> 8) & 0xF800) | ((p1 >> 5) & 0x07E0) | ((p1 >> 3) & 0x001F));
        dst[x + 2] = (XWORD)(((p2 >> 8) & 0xF800) | ((p2 >> 5) & 0x07E0) | ((p2 >> 3) & 0x001F));
        dst[x + 3] = (XWORD)(((p3 >> 8) & 0xF800) | ((p3 >> 5) & 0x07E0) | ((p3 >> 3) & 0x001F));
    }
    for (; x < width; ++x) {
        XDWORD pixel = src[x];
        dst[x] = (XWORD)(((pixel >> 8) & 0xF800) | ((pixel >> 5) & 0x07E0) | ((pixel >> 3) & 0x001F));
    }
}

void CopyLine_32ARGB_565RGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD pixel = src[x];
        dst[x] = (XWORD)(((pixel >> 8) & 0xF800) | ((pixel >> 5) & 0x07E0) | ((pixel >> 3) & 0x001F));
    }
}

// --- 32-bit ARGB -> 16-bit 555 -----------------------------------------------

void CopyLine_32ARGB_555RGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0], p1 = src[x + 1], p2 = src[x + 2], p3 = src[x + 3];
        dst[x + 0] = (XWORD)(((p0 >> 9) & 0x7C00) | ((p0 >> 6) & 0x03E0) | ((p0 >> 3) & 0x001F));
        dst[x + 1] = (XWORD)(((p1 >> 9) & 0x7C00) | ((p1 >> 6) & 0x03E0) | ((p1 >> 3) & 0x001F));
        dst[x + 2] = (XWORD)(((p2 >> 9) & 0x7C00) | ((p2 >> 6) & 0x03E0) | ((p2 >> 3) & 0x001F));
        dst[x + 3] = (XWORD)(((p3 >> 9) & 0x7C00) | ((p3 >> 6) & 0x03E0) | ((p3 >> 3) & 0x001F));
    }
    for (; x < width; ++x) {
        XDWORD pixel = src[x];
        dst[x] = (XWORD)(((pixel >> 9) & 0x7C00) | ((pixel >> 6) & 0x03E0) | ((pixel >> 3) & 0x001F));
    }
}

void CopyLine_32ARGB_555RGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD pixel = src[x];
        dst[x] = (XWORD)(((pixel >> 9) & 0x7C00) | ((pixel >> 6) & 0x03E0) | ((pixel >> 3) & 0x001F));
    }
}

// --- 32-bit ARGB -> 16-bit 1555 ARGB -----------------------------------------

void CopyLine_32ARGB_1555ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XDWORD pixel = src[x];
        XWORD a = ((pixel >> 16) & 0x8000);
        XWORD r = ((pixel >> 9)  & 0x7C00);
        XWORD g = ((pixel >> 6)  & 0x03E0);
        XWORD b = ((pixel >> 3)  & 0x001F);
        dst[x] = a | r | g | b;
    }
}

void CopyLine_32ARGB_1555ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD pixel = src[x];
        XWORD a = ((pixel >> 16) & 0x8000);
        XWORD r = ((pixel >> 9)  & 0x7C00);
        XWORD g = ((pixel >> 6)  & 0x03E0);
        XWORD b = ((pixel >> 3)  & 0x001F);
        dst[x] = a | r | g | b;
    }
}

// --- 32-bit ARGB -> 16-bit 4444 ARGB -----------------------------------------

void CopyLine_32ARGB_4444ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XDWORD pixel = src[x];
        XWORD a = ((pixel >> 16) & 0xF000);
        XWORD r = ((pixel >> 12) & 0x0F00);
        XWORD g = ((pixel >> 8)  & 0x00F0);
        XWORD b = ((pixel >> 4)  & 0x000F);
        dst[x] = a | r | g | b;
    }
}

void CopyLine_32ARGB_4444ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD pixel = src[x];
        XWORD a = ((pixel >> 16) & 0xF000);
        XWORD r = ((pixel >> 12) & 0x0F00);
        XWORD g = ((pixel >> 8)  & 0x00F0);
        XWORD b = ((pixel >> 4)  & 0x000F);
        dst[x] = a | r | g | b;
    }
}

// --- 16-bit 565 -> 32-bit ARGB -----------------------------------------------

void CopyLine_565RGB_32ARGB(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XWORD p0 = src[x + 0], p1 = src[x + 1], p2 = src[x + 2], p3 = src[x + 3];
        dst[x + 0] = 0xFF000000 | ((p0 & 0xF800) << 8) | ((p0 & 0xE000) << 3) | ((p0 & 0x07E0) << 5) | ((p0 & 0x0600) >> 1) | ((p0 & 0x001F) << 3) | ((p0 & 0x001C) >> 2);
        dst[x + 1] = 0xFF000000 | ((p1 & 0xF800) << 8) | ((p1 & 0xE000) << 3) | ((p1 & 0x07E0) << 5) | ((p1 & 0x0600) >> 1) | ((p1 & 0x001F) << 3) | ((p1 & 0x001C) >> 2);
        dst[x + 2] = 0xFF000000 | ((p2 & 0xF800) << 8) | ((p2 & 0xE000) << 3) | ((p2 & 0x07E0) << 5) | ((p2 & 0x0600) >> 1) | ((p2 & 0x001F) << 3) | ((p2 & 0x001C) >> 2);
        dst[x + 3] = 0xFF000000 | ((p3 & 0xF800) << 8) | ((p3 & 0xE000) << 3) | ((p3 & 0x07E0) << 5) | ((p3 & 0x0600) >> 1) | ((p3 & 0x001F) << 3) | ((p3 & 0x001C) >> 2);
    }
    for (; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD r = ((pixel & 0xF800) << 8) | ((pixel & 0xE000) << 3);
        XDWORD g = ((pixel & 0x07E0) << 5) | ((pixel & 0x0600) >> 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = 0xFF000000 | r | g | b;
    }
}

void CopyLine_565RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD r = ((pixel & 0xF800) << 8) | ((pixel & 0xE000) << 3);
        XDWORD g = ((pixel & 0x07E0) << 5) | ((pixel & 0x0600) >> 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = 0xFF000000 | r | g | b;
    }
}

// --- 16-bit 555 -> 32-bit ARGB -----------------------------------------------

void CopyLine_555RGB_32ARGB(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XWORD p0 = src[x + 0], p1 = src[x + 1], p2 = src[x + 2], p3 = src[x + 3];
        dst[x + 0] = 0xFF000000 | ((p0 & 0x7C00) << 9) | ((p0 & 0x7000) << 4) | ((p0 & 0x03E0) << 6) | ((p0 & 0x0380) << 1) | ((p0 & 0x001F) << 3) | ((p0 & 0x001C) >> 2);
        dst[x + 1] = 0xFF000000 | ((p1 & 0x7C00) << 9) | ((p1 & 0x7000) << 4) | ((p1 & 0x03E0) << 6) | ((p1 & 0x0380) << 1) | ((p1 & 0x001F) << 3) | ((p1 & 0x001C) >> 2);
        dst[x + 2] = 0xFF000000 | ((p2 & 0x7C00) << 9) | ((p2 & 0x7000) << 4) | ((p2 & 0x03E0) << 6) | ((p2 & 0x0380) << 1) | ((p2 & 0x001F) << 3) | ((p2 & 0x001C) >> 2);
        dst[x + 3] = 0xFF000000 | ((p3 & 0x7C00) << 9) | ((p3 & 0x7000) << 4) | ((p3 & 0x03E0) << 6) | ((p3 & 0x0380) << 1) | ((p3 & 0x001F) << 3) | ((p3 & 0x001C) >> 2);
    }
    for (; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD r = ((pixel & 0x7C00) << 9) | ((pixel & 0x7000) << 4);
        XDWORD g = ((pixel & 0x03E0) << 6) | ((pixel & 0x0380) << 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = 0xFF000000 | r | g | b;
    }
}

void CopyLine_555RGB_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD r = ((pixel & 0x7C00) << 9) | ((pixel & 0x7000) << 4);
        XDWORD g = ((pixel & 0x03E0) << 6) | ((pixel & 0x0380) << 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = 0xFF000000 | r | g | b;
    }
}

// --- 16-bit 1555 -> 32-bit ARGB ----------------------------------------------

void CopyLine_1555ARGB_32ARGB(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD a = (pixel & 0x8000) ? 0xFF000000 : 0x00000000;
        XDWORD r = ((pixel & 0x7C00) << 9) | ((pixel & 0x7000) << 4);
        XDWORD g = ((pixel & 0x03E0) << 6) | ((pixel & 0x0380) << 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = a | r | g | b;
    }
}

void CopyLine_1555ARGB_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD a = (pixel & 0x8000) ? 0xFF000000 : 0x00000000;
        XDWORD r = ((pixel & 0x7C00) << 9) | ((pixel & 0x7000) << 4);
        XDWORD g = ((pixel & 0x03E0) << 6) | ((pixel & 0x0380) << 1);
        XDWORD b = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
        dst[x] = a | r | g | b;
    }
}

// --- 16-bit 4444 -> 32-bit ARGB ----------------------------------------------

void CopyLine_4444ARGB_32ARGB(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD a = ((pixel & 0xF000) << 16) | ((pixel & 0xF000) << 12);
        XDWORD r = ((pixel & 0x0F00) << 12) | ((pixel & 0x0F00) << 8);
        XDWORD g = ((pixel & 0x00F0) << 8)  | ((pixel & 0x00F0) << 4);
        XDWORD b = ((pixel & 0x000F) << 4)  |  (pixel & 0x000F);
        dst[x] = a | r | g | b;
    }
}

void CopyLine_4444ARGB_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XWORD pixel = src[x];
        XDWORD a = ((pixel & 0xF000) << 16) | ((pixel & 0xF000) << 12);
        XDWORD r = ((pixel & 0x0F00) << 12) | ((pixel & 0x0F00) << 8);
        XDWORD g = ((pixel & 0x00F0) << 8)  | ((pixel & 0x00F0) << 4);
        XDWORD b = ((pixel & 0x000F) << 4)  |  (pixel & 0x000F);
        dst[x] = a | r | g | b;
    }
}

//==============================================================================
//  Section 5 -- Paletted Image Blitting Functions
//==============================================================================

// 8-bit paletted -> 32-bit ARGB
void CopyLine_Paletted8_32ARGB(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) return;

    // Pre-build lookup table for the palette
    XDWORD palette32[256];
    if (bpc >= 4) {
        for (int i = 0; i < 256; ++i) {
            const XBYTE *entry = colorMap + i * bpc;
            palette32[i] = (entry[3] << 24) | (entry[2] << 16) | (entry[1] << 8) | entry[0];
        }
    } else {
        for (int i = 0; i < 256; ++i) {
            const XBYTE *entry = colorMap + i * bpc;
            palette32[i] = 0xFF000000 | (entry[2] << 16) | (entry[1] << 8) | entry[0];
        }
    }

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        dst[x + 0] = palette32[src[x + 0]];
        dst[x + 1] = palette32[src[x + 1]];
        dst[x + 2] = palette32[src[x + 2]];
        dst[x + 3] = palette32[src[x + 3]];
    }
    for (; x < width; ++x) {
        dst[x] = palette32[src[x]];
    }
}

// 8-bit paletted -> 24-bit RGB
void CopyLine_Paletted8_24RGB(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) return;

    // Pre-build lookup table (store BGR in each entry)
    XDWORD palette32[256];
    for (int i = 0; i < 256; ++i) {
        const XBYTE *entry = colorMap + i * bpc;
        palette32[i] = (entry[2] << 16) | (entry[1] << 8) | entry[0];
    }

    for (int x = 0; x < width; ++x) {
        XDWORD color = palette32[src[x]];
        dst[0] = (XBYTE)color;          // B
        dst[1] = (XBYTE)(color >> 8);   // G
        dst[2] = (XBYTE)(color >> 16);  // R
        dst += 3;
    }
}

// 8-bit paletted -> 16-bit 565
void CopyLine_Paletted8_565RGB(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) return;

    // Pre-build 565 lookup table
    XWORD palette16[256];
    for (int i = 0; i < 256; ++i) {
        const XBYTE *entry = colorMap + i * bpc;
        palette16[i] = ((entry[2] >> 3) << 11) | ((entry[1] >> 2) << 5) | (entry[0] >> 3);
    }

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        dst[x + 0] = palette16[src[x + 0]];
        dst[x + 1] = palette16[src[x + 1]];
        dst[x + 2] = palette16[src[x + 2]];
        dst[x + 3] = palette16[src[x + 3]];
    }
    for (; x < width; ++x) {
        dst[x] = palette16[src[x]];
    }
}

// 8-bit paletted -> 16-bit with alpha (1555, 4444)
void CopyLine_Paletted8_16Alpha(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) return;

    const int rShift = info->redShiftDst;
    const int gShift = info->greenShiftDst;
    const int bShift = info->blueShiftDst;
    const int aShift = info->alphaShiftDst;
    const int rWidth = static_cast<int>(GetBitCountLocal(info->dstRedMask));
    const int gWidth = static_cast<int>(GetBitCountLocal(info->dstGreenMask));
    const int bWidth = static_cast<int>(GetBitCountLocal(info->dstBlueMask));
    const int aWidth = static_cast<int>(GetBitCountLocal(info->dstAlphaMask));

    XWORD palette16[256];
    auto quantize = [](XBYTE v, int width, int shift) -> XWORD {
        if (width <= 0) return 0;
        return (XWORD)((v >> (8 - width)) << shift);
    };

    for (int i = 0; i < 256; ++i) {
        const XBYTE *entry = colorMap + i * bpc;
        const XBYTE b_val = entry[0];
        const XBYTE g_val = entry[1];
        const XBYTE r_val = entry[2];
        const XBYTE a_val = (bpc >= 4) ? entry[3] : 0xFF;
        palette16[i] = quantize(r_val, rWidth, rShift) |
                       quantize(g_val, gWidth, gShift) |
                       quantize(b_val, bWidth, bShift) |
                       quantize(a_val, aWidth, aShift);
    }

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        dst[x + 0] = palette16[src[x + 0]];
        dst[x + 1] = palette16[src[x + 1]];
        dst[x + 2] = palette16[src[x + 2]];
        dst[x + 3] = palette16[src[x + 3]];
    }
    for (; x < width; ++x) {
        dst[x] = palette16[src[x]];
    }
}

// 8-bit paletted -> 8-bit (index copy)
void CopyLine_Paletted8_8(const VxBlitInfo *info) {
    memcpy(info->dstLine, info->srcLine, info->width);
}

//==============================================================================
//  Section 6 -- Channel Swap / Byte-Rotate Functions
//==============================================================================

// --- ARGB <-> ABGR (swap R and B) --------------------------------------------

void CopyLine_32ARGB_32ABGR(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0];
        XDWORD p1 = src[x + 1];
        XDWORD p2 = src[x + 2];
        XDWORD p3 = src[x + 3];
        dst[x + 0] = (p0 & 0xFF00FF00) | ((p0 & 0x00FF0000) >> 16) | ((p0 & 0x000000FF) << 16);
        dst[x + 1] = (p1 & 0xFF00FF00) | ((p1 & 0x00FF0000) >> 16) | ((p1 & 0x000000FF) << 16);
        dst[x + 2] = (p2 & 0xFF00FF00) | ((p2 & 0x00FF0000) >> 16) | ((p2 & 0x000000FF) << 16);
        dst[x + 3] = (p3 & 0xFF00FF00) | ((p3 & 0x00FF0000) >> 16) | ((p3 & 0x000000FF) << 16);
    }
    for (; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
}

void CopyLine_32ARGB_32ABGR_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
}

// ABGR -> ARGB (same operation -- swap is symmetric)
void CopyLine_32ABGR_32ARGB(const VxBlitInfo *info) {
    CopyLine_32ARGB_32ABGR(info);
}

// --- ARGB -> RGBA (rotate bytes left) ----------------------------------------

void CopyLine_32ARGB_32RGBA(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0];
        XDWORD p1 = src[x + 1];
        XDWORD p2 = src[x + 2];
        XDWORD p3 = src[x + 3];
        dst[x + 0] = (p0 << 8) | (p0 >> 24);
        dst[x + 1] = (p1 << 8) | (p1 >> 24);
        dst[x + 2] = (p2 << 8) | (p2 >> 24);
        dst[x + 3] = (p3 << 8) | (p3 >> 24);
    }
    for (; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p << 8) | (p >> 24);
    }
}

void CopyLine_32ARGB_32RGBA_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p << 8) | (p >> 24);
    }
}

// --- RGBA -> ARGB (rotate bytes right) ----------------------------------------

void CopyLine_32RGBA_32ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0];
        XDWORD p1 = src[x + 1];
        XDWORD p2 = src[x + 2];
        XDWORD p3 = src[x + 3];
        dst[x + 0] = (p0 >> 8) | (p0 << 24);
        dst[x + 1] = (p1 >> 8) | (p1 << 24);
        dst[x + 2] = (p2 >> 8) | (p2 << 24);
        dst[x + 3] = (p3 >> 8) | (p3 << 24);
    }
    for (; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p >> 8) | (p << 24);
    }
}

void CopyLine_32RGBA_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = (p >> 8) | (p << 24);
    }
}

// --- ARGB <-> BGRA (full byte reversal) --------------------------------------

void CopyLine_32ARGB_32BGRA(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[x + 0];
        XDWORD p1 = src[x + 1];
        XDWORD p2 = src[x + 2];
        XDWORD p3 = src[x + 3];
        dst[x + 0] = ((p0 >> 24) & 0xFF) | ((p0 >> 8) & 0xFF00) | ((p0 << 8) & 0xFF0000) | ((p0 << 24) & 0xFF000000);
        dst[x + 1] = ((p1 >> 24) & 0xFF) | ((p1 >> 8) & 0xFF00) | ((p1 << 8) & 0xFF0000) | ((p1 << 24) & 0xFF000000);
        dst[x + 2] = ((p2 >> 24) & 0xFF) | ((p2 >> 8) & 0xFF00) | ((p2 << 8) & 0xFF0000) | ((p2 << 24) & 0xFF000000);
        dst[x + 3] = ((p3 >> 24) & 0xFF) | ((p3 >> 8) & 0xFF00) | ((p3 << 8) & 0xFF0000) | ((p3 << 24) & 0xFF000000);
    }
    for (; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = ((p >> 24) & 0xFF) | ((p >> 8) & 0xFF00) | ((p << 8) & 0xFF0000) | ((p << 24) & 0xFF000000);
    }
}

void CopyLine_32ARGB_32BGRA_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        dst[x] = ((p >> 24) & 0xFF) | ((p >> 8) & 0xFF00) | ((p << 8) & 0xFF0000) | ((p << 24) & 0xFF000000);
    }
}

// BGRA -> ARGB (same operation -- reversal is symmetric)
void CopyLine_32BGRA_32ARGB(const VxBlitInfo *info) {
    CopyLine_32ARGB_32BGRA(info);
}

//==============================================================================
//  Section 7 -- Premultiplied Alpha Functions
//==============================================================================

void PremultiplyAlpha_32ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XDWORD p = src[x];
        XDWORD a = (p >> 24) & 0xFF;
        XDWORD r = ((p >> 16) & 0xFF) * a / 255;
        XDWORD g = ((p >> 8) & 0xFF) * a / 255;
        XDWORD b = (p & 0xFF) * a / 255;
        dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void PremultiplyAlpha_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        XDWORD a = (p >> 24) & 0xFF;
        XDWORD r = ((p >> 16) & 0xFF) * a / 255;
        XDWORD g = ((p >> 8) & 0xFF) * a / 255;
        XDWORD b = (p & 0xFF) * a / 255;
        dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void UnpremultiplyAlpha_32ARGB(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    for (int x = 0; x < width; ++x) {
        XDWORD p = src[x];
        XDWORD a = (p >> 24) & 0xFF;
        if (a == 0) {
            dst[x] = 0;
        } else if (a == 255) {
            dst[x] = p;
        } else {
            XDWORD r = XMin((XDWORD)255, ((p >> 16) & 0xFF) * 255 / a);
            XDWORD g = XMin((XDWORD)255, ((p >> 8) & 0xFF) * 255 / a);
            XDWORD b = XMin((XDWORD)255, (p & 0xFF) * 255 / a);
            dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void UnpremultiplyAlpha_32ARGB_Scalar(const VxBlitInfo *info, int startX) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        XDWORD a = (p >> 24) & 0xFF;
        if (a == 0) {
            dst[x] = 0;
        } else if (a == 255) {
            dst[x] = p;
        } else {
            XDWORD r = XMin((XDWORD)255, ((p >> 16) & 0xFF) * 255 / a);
            XDWORD g = XMin((XDWORD)255, ((p >> 8) & 0xFF) * 255 / a);
            XDWORD b = XMin((XDWORD)255, (p & 0xFF) * 255 / a);
            dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

//==============================================================================
//  Section 8 -- Alpha Channel Operations (dispatch-table entries)
//==============================================================================

// --- SetAlpha (8/16/32) -- write constant alpha into destination line ---------

void SetAlpha_8(const VxBlitInfo *info) {
    XBYTE *dst = info->dstLine;
    const int width = info->width;
    const XBYTE alphaValue = (XBYTE)(info->alphaValue & info->dstAlphaMask);
    const XBYTE alphaMaskInv = (XBYTE)info->alphaMaskInv;

    for (int x = 0; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaValue;
    }
}

void SetAlpha_16(const VxBlitInfo *info) {
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    const XWORD alphaValue = (XWORD)(info->alphaValue & info->dstAlphaMask);
    const XWORD alphaMaskInv = (XWORD)info->alphaMaskInv;

    for (int x = 0; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaValue;
    }
}

void SetAlpha_32(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XDWORD alphaValue = info->alphaValue & info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;

    for (int x = 0; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaValue;
    }
}

// Tail variant for SSE/AVX2.
void SetAlpha_32_Scalar(XDWORD *dst, int startX, int width,
                        XDWORD alphaValue, XDWORD alphaMaskInv) {
    for (int x = startX; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaValue;
    }
}

// --- CopyAlpha (8/16/32) -- copy alpha from byte array into destination ------

void CopyAlpha_8(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;
    const int alphaShift = info->alphaShiftDst;
    const XBYTE alphaMask = (XBYTE)info->dstAlphaMask;
    const XBYTE alphaMaskInv = (XBYTE)info->alphaMaskInv;

    XBYTE alphaLut[256];
    for (int i = 0; i < 256; ++i) {
        alphaLut[i] = (XBYTE)(((i << alphaShift) & alphaMask));
    }

    for (int x = 0; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaLut[src[x]];
    }
}

void CopyAlpha_16(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;
    const int alphaShift = info->alphaShiftDst;
    const XWORD alphaMask = (XWORD)info->dstAlphaMask;
    const XWORD alphaMaskInv = (XWORD)info->alphaMaskInv;

    const int alphaBits = static_cast<int>(GetBitCountLocal(alphaMask));
    XWORD alphaLut[256];
    if (alphaBits == 1) {
        for (int i = 0; i < 256; ++i) {
            alphaLut[i] = (XWORD)(((i >> 7) & 0x1) << alphaShift);
        }
    } else if (alphaBits == 4) {
        for (int i = 0; i < 256; ++i) {
            alphaLut[i] = (XWORD)(((i >> 4) << alphaShift) & alphaMask);
        }
    } else if (alphaBits > 0) {
        const int scaleShift = (alphaBits >= 8) ? 0 : (8 - alphaBits);
        for (int i = 0; i < 256; ++i) {
            alphaLut[i] = (XWORD)(((i >> scaleShift) << alphaShift) & alphaMask);
        }
    } else {
        memset(alphaLut, 0, sizeof(alphaLut));
    }

    for (int x = 0; x < width; ++x) {
        dst[x] = (dst[x] & alphaMaskInv) | alphaLut[src[x]];
    }
}

void CopyAlpha_32(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const int alphaShift = info->alphaShiftDst;
    const XDWORD alphaMask = info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;

    for (int x = 0; x < width; ++x) {
        XDWORD alpha = ((XDWORD)src[x] << alphaShift) & alphaMask;
        dst[x] = (dst[x] & alphaMaskInv) | alpha;
    }
}

// Tail variant for SSE/AVX2.
void CopyAlpha_32_Scalar(const XBYTE *src, XDWORD *dst, int startX, int width,
                          int alphaShift, XDWORD alphaMask, XDWORD alphaMaskInv) {
    for (int x = startX; x < width; ++x) {
        XDWORD alpha = ((XDWORD)src[x] << alphaShift) & alphaMask;
        dst[x] = (dst[x] & alphaMaskInv) | alpha;
    }
}

//==============================================================================
//  Section 9 -- Utility Operations (scalar tail variants for SIMD code)
//==============================================================================

// --- FillLine ----------------------------------------------------------------

void FillLine_32_Scalar(XDWORD *dst, int startX, int width, XDWORD color) {
    for (int x = startX; x < width; ++x) {
        dst[x] = color;
    }
}

void FillLine_16_Scalar(XWORD *dst, int startX, int width, XWORD color) {
    for (int x = startX; x < width; ++x) {
        dst[x] = color;
    }
}

// --- ClearAlpha (set alpha to 0 on 32-bit pixels) ----------------------------

void ClearAlpha_32_Scalar(XDWORD *dst, int startX, int width) {
    for (int x = startX; x < width; ++x) {
        dst[x] &= 0x00FFFFFF;
    }
}

// --- SetFullAlpha (set alpha to 0xFF on 32-bit pixels) -----------------------

void SetFullAlpha_32_Scalar(XDWORD *dst, int startX, int width) {
    for (int x = startX; x < width; ++x) {
        dst[x] |= 0xFF000000;
    }
}

// --- InvertColors (bitwise NOT RGB, keep alpha) ------------------------------

void InvertColors_32_Scalar(XDWORD *dst, int startX, int width) {
    for (int x = startX; x < width; ++x) {
        dst[x] ^= 0x00FFFFFF;
    }
}

// --- Grayscale (luminance: R*77 + G*150 + B*29 >> 8) ------------------------

void Grayscale_32_Scalar(const XDWORD *src, XDWORD *dst, int startX, int width) {
    for (int x = startX; x < width; ++x) {
        XDWORD p = src[x];
        XDWORD a = p & 0xFF000000;
        XDWORD r = (p >> 16) & 0xFF;
        XDWORD g = (p >> 8) & 0xFF;
        XDWORD b = p & 0xFF;
        XDWORD lum = (r * 77 + g * 150 + b * 29) >> 8;
        dst[x] = a | (lum << 16) | (lum << 8) | lum;
    }
}

// --- MultiplyBlend (dst = src * dst / 255) -----------------------------------

void MultiplyBlend_32_Scalar(const XDWORD *src, XDWORD *dst, int startX, int width) {
    for (int x = startX; x < width; ++x) {
        XDWORD sp = src[x];
        XDWORD dp = dst[x];
        XDWORD sa = (sp >> 24) & 0xFF;
        XDWORD sr = (sp >> 16) & 0xFF;
        XDWORD sg = (sp >> 8) & 0xFF;
        XDWORD sb = sp & 0xFF;
        XDWORD da = (dp >> 24) & 0xFF;
        XDWORD dr = (dp >> 16) & 0xFF;
        XDWORD dg = (dp >> 8) & 0xFF;
        XDWORD db = dp & 0xFF;
        XDWORD oa = (sa * da + 127) / 255;
        XDWORD or_ = (sr * dr + 127) / 255;
        XDWORD og = (sg * dg + 127) / 255;
        XDWORD ob = (sb * db + 127) / 255;
        dst[x] = (oa << 24) | (or_ << 16) | (og << 8) | ob;
    }
}

//==============================================================================
//  Section 10 -- Resize Functions (32-bit bilinear)
//==============================================================================

// Equal Y, Equal X -- just copy
void ResizeHLine_EqualY_EqualX_32Bpp(const VxResizeInfo *info) {
    memcpy(info->dstRow, info->srcRow, 4 * info->w2);
}

// Equal Y, Shrink X -- horizontal downscale
void ResizeHLine_EqualY_ShrinkX_32Bpp(const VxResizeInfo *info) {
    const XDWORD *src = info->srcRow;
    XDWORD *dst = info->dstRow;

    int srcX = 0; // Fixed point 16.16
    for (int x = 0; x < info->w2; ++x) {
        int srcIdx = srcX >> 16;
        int frac = srcX & 0xFFFF;

        if (frac == 0 || srcIdx + 1 >= info->w1) {
            dst[x] = src[srcIdx];
        } else {
            // Bilinear interpolation
            XDWORD p0 = src[srcIdx];
            XDWORD p1 = src[srcIdx + 1];

            XDWORD r = (((p0 >> 16) & 0xFF) * (0x10000 - frac) + ((p1 >> 16) & 0xFF) * frac) >> 16;
            XDWORD g = (((p0 >> 8) & 0xFF) * (0x10000 - frac) + ((p1 >> 8) & 0xFF) * frac) >> 16;
            XDWORD b = ((p0 & 0xFF) * (0x10000 - frac) + (p1 & 0xFF) * frac) >> 16;
            XDWORD a = (((p0 >> 24) & 0xFF) * (0x10000 - frac) + ((p1 >> 24) & 0xFF) * frac) >> 16;

            dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        srcX += info->wr1;
    }
}

// Equal Y, Grow X -- horizontal upscale
void ResizeHLine_EqualY_GrowX_32Bpp(const VxResizeInfo *info) {
    const XDWORD *src = info->srcRow;
    XDWORD *dst = info->dstRow;

    int srcX = 0; // Fixed point 16.16
    for (int x = 0; x < info->w2; ++x) {
        int srcIdx = srcX >> 16;
        int frac = srcX & 0xFFFF;

        if (frac == 0 || srcIdx + 1 >= info->w1) {
            dst[x] = src[XMin(srcIdx, info->w1 - 1)];
        } else {
            // Bilinear interpolation
            XDWORD p0 = src[srcIdx];
            XDWORD p1 = src[XMin(srcIdx + 1, info->w1 - 1)];

            XDWORD r = (((p0 >> 16) & 0xFF) * (0x10000 - frac) + ((p1 >> 16) & 0xFF) * frac) >> 16;
            XDWORD g = (((p0 >> 8) & 0xFF) * (0x10000 - frac) + ((p1 >> 8) & 0xFF) * frac) >> 16;
            XDWORD b = ((p0 & 0xFF) * (0x10000 - frac) + (p1 & 0xFF) * frac) >> 16;
            XDWORD a = (((p0 >> 24) & 0xFF) * (0x10000 - frac) + ((p1 >> 24) & 0xFF) * frac) >> 16;

            dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        srcX += info->wr1;
    }
}

// Shrink Y, Equal X -- vertical downscale (row copy)
void ResizeHLine_ShrinkY_EqualX_32Bpp(const VxResizeInfo *info) {
    memcpy(info->dstRow, info->srcRow, 4 * info->w2);
}

// Shrink Y, Shrink X -- both axes downscale
void ResizeHLine_ShrinkY_ShrinkX_32Bpp(const VxResizeInfo *info) {
    ResizeHLine_EqualY_ShrinkX_32Bpp(info);
}

// Shrink Y, Grow X -- vertical down, horizontal up
void ResizeHLine_ShrinkY_GrowX_32Bpp(const VxResizeInfo *info) {
    ResizeHLine_EqualY_GrowX_32Bpp(info);
}

// Grow Y, Equal X -- vertical upscale (row copy)
void ResizeHLine_GrowY_EqualX_32Bpp(const VxResizeInfo *info) {
    memcpy(info->dstRow, info->srcRow, 4 * info->w2);
}

// Grow Y, Shrink X -- vertical up, horizontal down
void ResizeHLine_GrowY_ShrinkX_32Bpp(const VxResizeInfo *info) {
    ResizeHLine_EqualY_ShrinkX_32Bpp(info);
}

// Grow Y, Grow X -- both axes upscale
void ResizeHLine_GrowY_GrowX_32Bpp(const VxResizeInfo *info) {
    ResizeHLine_EqualY_GrowX_32Bpp(info);
}
