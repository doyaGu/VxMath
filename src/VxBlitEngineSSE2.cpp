/**
 * @file VxBlitEngineSSE2.cpp
 * @brief SSE2-only SIMD implementations of VxBlitEngine line kernels.
 *
 * Every SSE function processes the bulk of the scanline using 128-bit SSE2
 * intrinsics, then delegates the remaining (tail) pixels to the corresponding
 * `*_Scalar()` function defined in VxBlitKernels.cpp.
 *
 * The entire file is compiled only when VX_SIMD_SSE2 is defined.
 */

#include "VxBlitInternal.h"

#if defined(VX_SIMD_SSE2)

#include <emmintrin.h>  // SSE2

#include "VxMath.h"

//==============================================================================
//  #1  32<->32 Conversions
//==============================================================================

// --- 32-bit ARGB -> 32-bit RGB (clear alpha) ---------------------------------

void CopyLine_32ARGB_32RGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i mask = _mm_set1_epi32(0x00FFFFFF);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_and_si128(pixels, mask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32ARGB_32RGB_Scalar(info, x);
}

// --- 32-bit RGB -> 32-bit ARGB (add full alpha) ------------------------------

void CopyLine_32RGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i alphaMask = _mm_set1_epi32(0xFF000000);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_or_si128(pixels, alphaMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32RGB_32ARGB_Scalar(info, x);
}

//==============================================================================
//  #2  32<->16 Conversions
//==============================================================================

// --- 32-bit ARGB -> 16-bit 565 -----------------------------------------------

void CopyLine_32ARGB_565RGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        __m128i r = _mm_and_si128(_mm_srli_epi32(pixels, 8), _mm_set1_epi32(0xF800));
        __m128i g = _mm_and_si128(_mm_srli_epi32(pixels, 5), _mm_set1_epi32(0x07E0));
        __m128i b = _mm_and_si128(_mm_srli_epi32(pixels, 3), _mm_set1_epi32(0x001F));

        __m128i result = _mm_or_si128(_mm_or_si128(r, g), b);

        // Pack 32-bit -> 16-bit using shuffle (SSE2 compatible)
        result = _mm_shufflelo_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shufflehi_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shuffle_epi32(result, _MM_SHUFFLE(2, 0, 2, 0));
        _mm_storel_epi64((__m128i *)(dst + x), result);
    }

    CopyLine_32ARGB_565RGB_Scalar(info, x);
}

// --- 32-bit ARGB -> 16-bit 555 -----------------------------------------------

void CopyLine_32ARGB_555RGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        __m128i r = _mm_and_si128(_mm_srli_epi32(pixels, 9), _mm_set1_epi32(0x7C00));
        __m128i g = _mm_and_si128(_mm_srli_epi32(pixels, 6), _mm_set1_epi32(0x03E0));
        __m128i b = _mm_and_si128(_mm_srli_epi32(pixels, 3), _mm_set1_epi32(0x001F));

        __m128i result = _mm_or_si128(_mm_or_si128(r, g), b);

        result = _mm_shufflelo_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shufflehi_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shuffle_epi32(result, _MM_SHUFFLE(2, 0, 2, 0));
        _mm_storel_epi64((__m128i *)(dst + x), result);
    }

    CopyLine_32ARGB_555RGB_Scalar(info, x);
}

// --- 32-bit ARGB -> 16-bit 1555 ARGB -----------------------------------------

void CopyLine_32ARGB_1555ARGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        __m128i a = _mm_and_si128(_mm_srli_epi32(pixels, 16), _mm_set1_epi32(0x8000));
        __m128i r = _mm_and_si128(_mm_srli_epi32(pixels, 9),  _mm_set1_epi32(0x7C00));
        __m128i g = _mm_and_si128(_mm_srli_epi32(pixels, 6),  _mm_set1_epi32(0x03E0));
        __m128i b = _mm_and_si128(_mm_srli_epi32(pixels, 3),  _mm_set1_epi32(0x001F));

        __m128i result = _mm_or_si128(_mm_or_si128(a, r), _mm_or_si128(g, b));

        result = _mm_shufflelo_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shufflehi_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shuffle_epi32(result, _MM_SHUFFLE(2, 0, 2, 0));
        _mm_storel_epi64((__m128i *)(dst + x), result);
    }

    CopyLine_32ARGB_1555ARGB_Scalar(info, x);
}

// --- 32-bit ARGB -> 16-bit 4444 ARGB -----------------------------------------

void CopyLine_32ARGB_4444ARGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        __m128i a = _mm_and_si128(_mm_srli_epi32(pixels, 16), _mm_set1_epi32(0xF000));
        __m128i r = _mm_and_si128(_mm_srli_epi32(pixels, 12), _mm_set1_epi32(0x0F00));
        __m128i g = _mm_and_si128(_mm_srli_epi32(pixels, 8),  _mm_set1_epi32(0x00F0));
        __m128i b = _mm_and_si128(_mm_srli_epi32(pixels, 4),  _mm_set1_epi32(0x000F));

        __m128i result = _mm_or_si128(_mm_or_si128(a, r), _mm_or_si128(g, b));

        result = _mm_shufflelo_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shufflehi_epi16(result, _MM_SHUFFLE(2, 0, 2, 0));
        result = _mm_shuffle_epi32(result, _MM_SHUFFLE(2, 0, 2, 0));
        _mm_storel_epi64((__m128i *)(dst + x), result);
    }

    CopyLine_32ARGB_4444ARGB_Scalar(info, x);
}

// --- 16-bit 565 -> 32-bit ARGB -----------------------------------------------

void CopyLine_565RGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i alpha = _mm_set1_epi32(0xFF000000);
    const __m128i zero = _mm_setzero_si128();
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadl_epi64((const __m128i *)(src + x));
        pixels = _mm_unpacklo_epi16(pixels, zero);

        __m128i r = _mm_and_si128(pixels, _mm_set1_epi32(0xF800));
        r = _mm_slli_epi32(r, 8);
        r = _mm_or_si128(r, _mm_srli_epi32(r, 5));
        r = _mm_and_si128(r, _mm_set1_epi32(0x00FF0000));

        __m128i g = _mm_and_si128(pixels, _mm_set1_epi32(0x07E0));
        g = _mm_slli_epi32(g, 5);
        g = _mm_or_si128(g, _mm_srli_epi32(g, 6));
        g = _mm_and_si128(g, _mm_set1_epi32(0x0000FF00));

        __m128i b = _mm_and_si128(pixels, _mm_set1_epi32(0x001F));
        b = _mm_slli_epi32(b, 3);
        b = _mm_or_si128(b, _mm_srli_epi32(b, 5));
        b = _mm_and_si128(b, _mm_set1_epi32(0x000000FF));

        __m128i result = _mm_or_si128(_mm_or_si128(alpha, r), _mm_or_si128(g, b));
        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    CopyLine_565RGB_32ARGB_Scalar(info, x);
}

// --- 16-bit 555 -> 32-bit ARGB -----------------------------------------------

void CopyLine_555RGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i alpha = _mm_set1_epi32(0xFF000000);
    const __m128i zero = _mm_setzero_si128();
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadl_epi64((const __m128i *)(src + x));
        pixels = _mm_unpacklo_epi16(pixels, zero);

        __m128i r = _mm_and_si128(pixels, _mm_set1_epi32(0x7C00));
        r = _mm_slli_epi32(r, 9);
        r = _mm_or_si128(r, _mm_srli_epi32(r, 5));
        r = _mm_and_si128(r, _mm_set1_epi32(0x00FF0000));

        __m128i g = _mm_and_si128(pixels, _mm_set1_epi32(0x03E0));
        g = _mm_slli_epi32(g, 6);
        g = _mm_or_si128(g, _mm_srli_epi32(g, 5));
        g = _mm_and_si128(g, _mm_set1_epi32(0x0000FF00));

        __m128i b = _mm_and_si128(pixels, _mm_set1_epi32(0x001F));
        b = _mm_slli_epi32(b, 3);
        b = _mm_or_si128(b, _mm_srli_epi32(b, 5));
        b = _mm_and_si128(b, _mm_set1_epi32(0x000000FF));

        __m128i result = _mm_or_si128(_mm_or_si128(alpha, r), _mm_or_si128(g, b));
        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    CopyLine_555RGB_32ARGB_Scalar(info, x);
}

// --- 16-bit 1555 -> 32-bit ARGB ----------------------------------------------

void CopyLine_1555ARGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i zero = _mm_setzero_si128();
    const __m128i alphaMask16 = _mm_set1_epi32(0x8000);
    const __m128i alphaFull = _mm_set1_epi32(0xFF000000);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadl_epi64((const __m128i *)(src + x));
        pixels = _mm_unpacklo_epi16(pixels, zero);

        __m128i a = _mm_and_si128(pixels, alphaMask16);
        __m128i aMask = _mm_cmpeq_epi32(a, alphaMask16);
        a = _mm_and_si128(aMask, alphaFull);

        __m128i r = _mm_and_si128(pixels, _mm_set1_epi32(0x7C00));
        r = _mm_slli_epi32(r, 9);
        r = _mm_or_si128(r, _mm_srli_epi32(r, 5));
        r = _mm_and_si128(r, _mm_set1_epi32(0x00FF0000));

        __m128i g = _mm_and_si128(pixels, _mm_set1_epi32(0x03E0));
        g = _mm_slli_epi32(g, 6);
        g = _mm_or_si128(g, _mm_srli_epi32(g, 5));
        g = _mm_and_si128(g, _mm_set1_epi32(0x0000FF00));

        __m128i b = _mm_and_si128(pixels, _mm_set1_epi32(0x001F));
        b = _mm_slli_epi32(b, 3);
        b = _mm_or_si128(b, _mm_srli_epi32(b, 5));
        b = _mm_and_si128(b, _mm_set1_epi32(0x000000FF));

        __m128i result = _mm_or_si128(_mm_or_si128(a, r), _mm_or_si128(g, b));
        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    CopyLine_1555ARGB_32ARGB_Scalar(info, x);
}

// --- 16-bit 4444 -> 32-bit ARGB ----------------------------------------------

void CopyLine_4444ARGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i zero = _mm_setzero_si128();
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadl_epi64((const __m128i *)(src + x));
        pixels = _mm_unpacklo_epi16(pixels, zero);

        __m128i a = _mm_and_si128(pixels, _mm_set1_epi32(0xF000));
        a = _mm_slli_epi32(a, 16);
        a = _mm_or_si128(a, _mm_srli_epi32(a, 4));
        a = _mm_and_si128(a, _mm_set1_epi32(0xFF000000));

        __m128i r = _mm_and_si128(pixels, _mm_set1_epi32(0x0F00));
        r = _mm_slli_epi32(r, 12);
        r = _mm_or_si128(r, _mm_srli_epi32(r, 4));
        r = _mm_and_si128(r, _mm_set1_epi32(0x00FF0000));

        __m128i g = _mm_and_si128(pixels, _mm_set1_epi32(0x00F0));
        g = _mm_slli_epi32(g, 8);
        g = _mm_or_si128(g, _mm_srli_epi32(g, 4));
        g = _mm_and_si128(g, _mm_set1_epi32(0x0000FF00));

        __m128i b = _mm_and_si128(pixels, _mm_set1_epi32(0x000F));
        b = _mm_slli_epi32(b, 4);
        b = _mm_or_si128(b, _mm_and_si128(pixels, _mm_set1_epi32(0x000F)));
        b = _mm_and_si128(b, _mm_set1_epi32(0x000000FF));

        __m128i result = _mm_or_si128(_mm_or_si128(a, r), _mm_or_si128(g, b));
        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    CopyLine_4444ARGB_32ARGB_Scalar(info, x);
}

//==============================================================================
//  #5  Alpha Channel Operations
//==============================================================================

// --- Set constant alpha on 32-bit image --------------------------------------

void SetAlpha_32_SSE(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XDWORD alphaValue = info->alphaValue & info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;

    const __m128i vAlphaValue = _mm_set1_epi32(alphaValue);
    const __m128i vAlphaMaskInv = _mm_set1_epi32(alphaMaskInv);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(dst + x));
        pixels = _mm_and_si128(pixels, vAlphaMaskInv);
        pixels = _mm_or_si128(pixels, vAlphaValue);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    SetAlpha_32_Scalar(dst, x, width, alphaValue, alphaMaskInv);
}

// --- Copy alpha from byte array into 32-bit image ----------------------------

void CopyAlpha_32_SSE(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const int alphaShift = info->alphaShiftDst;
    const XDWORD alphaMask = info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;

    const __m128i vAlphaMaskInv = _mm_set1_epi32(alphaMaskInv);
    const __m128i zero = _mm_setzero_si128();
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i alphas = _mm_cvtsi32_si128(*(const int *)(src + x));
        alphas = _mm_unpacklo_epi8(alphas, zero);
        alphas = _mm_unpacklo_epi16(alphas, zero);
        alphas = _mm_slli_epi32(alphas, alphaShift);

        __m128i pixels = _mm_loadu_si128((const __m128i *)(dst + x));
        pixels = _mm_and_si128(pixels, vAlphaMaskInv);
        pixels = _mm_or_si128(pixels, alphas);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyAlpha_32_Scalar(src, dst, x, width, alphaShift, alphaMask, alphaMaskInv);
}

//==============================================================================
//  #6  Paletted Conversion (SSE-assisted lookup)
//==============================================================================

void CopyLine_Paletted8_32ARGB_SSE(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) return;

    // Pre-build palette as 32-bit ARGB
    XDWORD palette32[256];
    for (int i = 0; i < 256; ++i) {
        const XBYTE *entry = colorMap + i * bpc;
        XDWORD b = entry[0];
        XDWORD g = entry[1];
        XDWORD r = entry[2];
        XDWORD a = (bpc >= 4) ? entry[3] : 0xFF;
        palette32[i] = (a << 24) | (r << 16) | (g << 8) | b;
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

//==============================================================================
//  #7  Premultiply / Unpremultiply Alpha
//==============================================================================

// Exact floor division by 255: floor(x/255) = ((x+1) + ((x+1)>>8)) >> 8

void PremultiplyAlpha_32ARGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i zero = _mm_setzero_si128();
    const __m128i one = _mm_set1_epi16(1);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        __m128i alpha32 = _mm_srli_epi32(pixels, 24);

        __m128i lo16 = _mm_unpacklo_epi8(pixels, zero);
        __m128i hi16 = _mm_unpackhi_epi8(pixels, zero);

        __m128i a01 = _mm_unpacklo_epi32(alpha32, alpha32);
        __m128i a23 = _mm_unpackhi_epi32(alpha32, alpha32);
        __m128i alo = _mm_packs_epi32(a01, a01);
        __m128i ahi = _mm_packs_epi32(a23, a23);

        __m128i prodlo = _mm_mullo_epi16(lo16, alo);
        __m128i prodhi = _mm_mullo_epi16(hi16, ahi);

        // floor(x/255) = ((x+1) + ((x+1)>>8)) >> 8
        __m128i tmplo = _mm_add_epi16(prodlo, one);
        __m128i tmphi = _mm_add_epi16(prodhi, one);
        tmplo = _mm_add_epi16(tmplo, _mm_srli_epi16(tmplo, 8));
        tmphi = _mm_add_epi16(tmphi, _mm_srli_epi16(tmphi, 8));
        __m128i reslo = _mm_srli_epi16(tmplo, 8);
        __m128i reshi = _mm_srli_epi16(tmphi, 8);

        __m128i result = _mm_packus_epi16(reslo, reshi);

        // Restore original alpha
        __m128i alphaMask = _mm_set1_epi32(0xFF000000);
        result = _mm_or_si128(_mm_andnot_si128(alphaMask, result),
                              _mm_and_si128(alphaMask, pixels));

        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    PremultiplyAlpha_32ARGB_Scalar(info, x);
}

// Unpremultiply requires per-pixel division -- simply delegate to scalar.
void UnpremultiplyAlpha_32ARGB_SSE(const VxBlitInfo *info) {
    UnpremultiplyAlpha_32ARGB_Scalar(info, 0);
}

//==============================================================================
//  #8  Fill / Clear / SetFull / Invert / Grayscale / MultiplyBlend
//==============================================================================

// --- Fill 32-bit line --------------------------------------------------------

void FillLine_32_SSE(XDWORD *dst, int width, XDWORD color) {
    __m128i fillColor = _mm_set1_epi32(color);
    int x = 0;

    for (; x + 16 <= width; x += 16) {
        _mm_storeu_si128((__m128i *)(dst + x), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 4), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 8), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 12), fillColor);
    }
    for (; x + 4 <= width; x += 4) {
        _mm_storeu_si128((__m128i *)(dst + x), fillColor);
    }

    FillLine_32_Scalar(dst, x, width, color);
}

// --- Fill 16-bit line --------------------------------------------------------

void FillLine_16_SSE(XWORD *dst, int width, XWORD color) {
    __m128i fillColor = _mm_set1_epi16(color);
    int x = 0;

    for (; x + 32 <= width; x += 32) {
        _mm_storeu_si128((__m128i *)(dst + x), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 8), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 16), fillColor);
        _mm_storeu_si128((__m128i *)(dst + x + 24), fillColor);
    }
    for (; x + 8 <= width; x += 8) {
        _mm_storeu_si128((__m128i *)(dst + x), fillColor);
    }

    FillLine_16_Scalar(dst, x, width, color);
}

// --- Clear alpha (set to 0) --------------------------------------------------

void ClearAlpha_32_SSE(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m128i mask = _mm_set1_epi32(0x00FFFFFF);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((__m128i *)(dst + x));
        pixels = _mm_and_si128(pixels, mask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    ClearAlpha_32_Scalar(dst, x, width);
}

// --- Set full alpha (255) ----------------------------------------------------

void SetFullAlpha_32_SSE(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m128i alphaMask = _mm_set1_epi32(0xFF000000);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((__m128i *)(dst + x));
        pixels = _mm_or_si128(pixels, alphaMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    SetFullAlpha_32_Scalar(dst, x, width);
}

// --- Invert RGB channels (keep alpha) ----------------------------------------

void InvertColors_32_SSE(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m128i invertMask = _mm_set1_epi32(0x00FFFFFF);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((__m128i *)(dst + x));
        __m128i inverted = _mm_xor_si128(pixels, invertMask);
        _mm_storeu_si128((__m128i *)(dst + x), inverted);
    }

    InvertColors_32_Scalar(dst, x, width);
}

// --- Grayscale (luminance formula) -------------------------------------------

void Grayscale_32_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i zero = _mm_setzero_si128();
    const __m128i coeff = _mm_setr_epi16(29, 150, 77, 0, 29, 150, 77, 0);
    const __m128i alphaMask = _mm_set1_epi32(0xFF000000);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));

        // Expand BGRA bytes to 16-bit lanes for two pixels at a time.
        __m128i lo16 = _mm_unpacklo_epi8(pixels, zero);
        __m128i hi16 = _mm_unpackhi_epi8(pixels, zero);

        // For each pixel: (B*29 + G*150) and (R*77 + A*0), then add the pair.
        __m128i loPairs = _mm_madd_epi16(lo16, coeff);
        __m128i hiPairs = _mm_madd_epi16(hi16, coeff);

        __m128i loSum = _mm_add_epi32(loPairs, _mm_srli_si128(loPairs, 4));
        __m128i hiSum = _mm_add_epi32(hiPairs, _mm_srli_si128(hiPairs, 4));

        // Gather luminance numerators for pixels 0..3 into 32-bit lanes.
        __m128i loLum = _mm_shuffle_epi32(loSum, _MM_SHUFFLE(0, 0, 2, 0));
        __m128i hiLum = _mm_shuffle_epi32(hiSum, _MM_SHUFFLE(0, 0, 2, 0));
        __m128i lum = _mm_unpacklo_epi64(loLum, hiLum);

        // Final luminance = (R*77 + G*150 + B*29) >> 8.
        lum = _mm_srli_epi32(lum, 8);

        // Replicate luminance into RGB and preserve source alpha.
        __m128i gray = _mm_or_si128(lum, _mm_slli_epi32(lum, 8));
        gray = _mm_or_si128(gray, _mm_slli_epi32(lum, 16));
        __m128i alpha = _mm_and_si128(pixels, alphaMask);
        __m128i result = _mm_or_si128(alpha, gray);

        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    Grayscale_32_Scalar(src, dst, x, width);
}

// --- Multiply blend (dst = src * dst / 255) ----------------------------------

void MultiplyBlend_32_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i zero = _mm_setzero_si128();
    const __m128i round = _mm_set1_epi16(128);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i srcPix = _mm_loadu_si128((const __m128i *)(src + x));
        __m128i dstPix = _mm_loadu_si128((const __m128i *)(dst + x));

        __m128i srcLo = _mm_unpacklo_epi8(srcPix, zero);
        __m128i srcHi = _mm_unpackhi_epi8(srcPix, zero);
        __m128i dstLo = _mm_unpacklo_epi8(dstPix, zero);
        __m128i dstHi = _mm_unpackhi_epi8(dstPix, zero);

        __m128i prodLo = _mm_mullo_epi16(srcLo, dstLo);
        __m128i prodHi = _mm_mullo_epi16(srcHi, dstHi);

        // (prod + 128 + ((prod + 128) >> 8)) >> 8
        __m128i tmpLo = _mm_add_epi16(prodLo, round);
        __m128i tmpHi = _mm_add_epi16(prodHi, round);
        tmpLo = _mm_add_epi16(tmpLo, _mm_srli_epi16(tmpLo, 8));
        tmpHi = _mm_add_epi16(tmpHi, _mm_srli_epi16(tmpHi, 8));
        __m128i resLo = _mm_srli_epi16(tmpLo, 8);
        __m128i resHi = _mm_srli_epi16(tmpHi, 8);

        __m128i result = _mm_packus_epi16(resLo, resHi);
        _mm_storeu_si128((__m128i *)(dst + x), result);
    }

    MultiplyBlend_32_Scalar(src, dst, x, width);
}

#endif // VX_SIMD_SSE2
