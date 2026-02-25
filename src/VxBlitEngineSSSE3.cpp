/**
 * @file VxBlitEngineSSSE3.cpp
 * @brief SSSE3 SIMD implementations of VxBlitEngine line kernels.
 *
 * This file contains kernels that rely on SSSE3 intrinsics
 * (`_mm_shuffle_epi8`, `_mm_alignr_epi8`). It is built under
 * VX_SIMD_SSE2 so SIMDe can still provide compatible emulation when
 * native SSSE3 code generation is unavailable.
 */

#include "VxBlitInternal.h"

#if defined(VX_SIMD_SSE2)

#include <emmintrin.h>  // SSE2
#if defined(__SSSE3__) || defined(_MSC_VER)
/* Include the native SSSE3 header only when the compiler generates real SSSE3
 * instructions (GCC/Clang with at least -mssse3, or any MSVC version which
 * always exposes intrinsic declarations regardless of /arch).
 * On GCC/Clang without -mssse3 the SSSE3 intrinsics are already reachable
 * through the SIMDe native aliases pulled in via
 * VxBlitInternal.h -> VxBlitEngine.h -> VxSIMD.h. */
#include <tmmintrin.h>  // SSSE3 (_mm_shuffle_epi8, _mm_alignr_epi8)
#endif

#include "VxMath.h"

//==============================================================================
//  #1  24<->32 Conversions
//==============================================================================

// --- 24-bit RGB -> 32-bit ARGB -----------------------------------------------

void CopyLine_24RGB_32ARGB_SSE(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i alpha = _mm_set1_epi32(0xFF000000);
    int x = 0;

    // Process 16 pixels at a time (48 bytes) using SSSE3 shuffle
    for (; x + 16 <= width; x += 16) {
        __m128i chunk0 = _mm_loadu_si128((const __m128i *)(src));
        __m128i chunk1 = _mm_loadu_si128((const __m128i *)(src + 16));
        __m128i chunk2 = _mm_loadu_si128((const __m128i *)(src + 32));

        const __m128i shuf0 = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);

        __m128i out0 = _mm_shuffle_epi8(chunk0, shuf0);
        out0 = _mm_or_si128(out0, alpha);
        _mm_storeu_si128((__m128i *)(dst + x), out0);

        __m128i blend1 = _mm_alignr_epi8(chunk1, chunk0, 12);
        __m128i out1 = _mm_shuffle_epi8(blend1, shuf0);
        out1 = _mm_or_si128(out1, alpha);
        _mm_storeu_si128((__m128i *)(dst + x + 4), out1);

        __m128i blend2 = _mm_alignr_epi8(chunk2, chunk1, 8);
        __m128i out2 = _mm_shuffle_epi8(blend2, shuf0);
        out2 = _mm_or_si128(out2, alpha);
        _mm_storeu_si128((__m128i *)(dst + x + 8), out2);

        __m128i out3 = _mm_shuffle_epi8(_mm_srli_si128(chunk2, 4), shuf0);
        out3 = _mm_or_si128(out3, alpha);
        _mm_storeu_si128((__m128i *)(dst + x + 12), out3);

        src += 48;
    }

    // Scalar 4-at-a-time cleanup
    for (; x + 4 <= width; x += 4) {
        XDWORD p0 = src[0] | (src[1] << 8) | (src[2] << 16);
        XDWORD p1 = src[3] | (src[4] << 8) | (src[5] << 16);
        XDWORD p2 = src[6] | (src[7] << 8) | (src[8] << 16);
        XDWORD p3 = src[9] | (src[10] << 8) | (src[11] << 16);

        __m128i pixels = _mm_set_epi32(p3, p2, p1, p0);
        pixels = _mm_or_si128(pixels, alpha);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);

        src += 12;
    }

    // Final tail pixels
    CopyLine_24RGB_32ARGB_Scalar(info, x, src, dst + x);
}

// --- 32-bit ARGB -> 24-bit RGB -----------------------------------------------

void CopyLine_32ARGB_24RGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;

    int x = 0;

    // Process 16 pixels at a time (64 bytes in, 48 bytes out)
    for (; x + 16 <= width; x += 16) {
        __m128i p0 = _mm_loadu_si128((const __m128i *)(src + x));
        __m128i p1 = _mm_loadu_si128((const __m128i *)(src + x + 4));
        __m128i p2 = _mm_loadu_si128((const __m128i *)(src + x + 8));
        __m128i p3 = _mm_loadu_si128((const __m128i *)(src + x + 12));

        const __m128i shuf = _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1);

        __m128i rgb0 = _mm_shuffle_epi8(p0, shuf);
        __m128i rgb1 = _mm_shuffle_epi8(p1, shuf);
        __m128i rgb2 = _mm_shuffle_epi8(p2, shuf);
        __m128i rgb3 = _mm_shuffle_epi8(p3, shuf);

        __m128i out0 = _mm_or_si128(rgb0, _mm_slli_si128(rgb1, 12));
        __m128i out1 = _mm_or_si128(_mm_srli_si128(rgb1, 4), _mm_slli_si128(rgb2, 8));
        __m128i out2 = _mm_or_si128(_mm_srli_si128(rgb2, 8), _mm_slli_si128(rgb3, 4));

        _mm_storeu_si128((__m128i *)(dst), out0);
        _mm_storeu_si128((__m128i *)(dst + 16), out1);
        _mm_storeu_si128((__m128i *)(dst + 32), out2);

        dst += 48;
    }

    // Tail pixels
    CopyLine_32ARGB_24RGB_Scalar(info, x, nullptr, dst);
}

//==============================================================================
//  #2  Channel Swap / Byte-Rotate (SSSE3 _mm_shuffle_epi8)
//==============================================================================

// --- ARGB <-> ABGR (swap R and B) --------------------------------------------

void CopyLine_32ARGB_32ABGR_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i shufMask = _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_shuffle_epi8(pixels, shufMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32ARGB_32ABGR_Scalar(info, x);
}

void CopyLine_32ABGR_32ARGB_SSE(const VxBlitInfo *info) {
    CopyLine_32ARGB_32ABGR_SSE(info); // Symmetric
}

// --- ARGB -> RGBA (rotate bytes left) ----------------------------------------

void CopyLine_32ARGB_32RGBA_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i shufMask = _mm_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_shuffle_epi8(pixels, shufMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32ARGB_32RGBA_Scalar(info, x);
}

// --- RGBA -> ARGB (rotate bytes right) ---------------------------------------

void CopyLine_32RGBA_32ARGB_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i shufMask = _mm_setr_epi8(1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_shuffle_epi8(pixels, shufMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32RGBA_32ARGB_Scalar(info, x);
}

// --- ARGB <-> BGRA (full byte reversal) --------------------------------------

void CopyLine_32ARGB_32BGRA_SSE(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m128i shufMask = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    int x = 0;

    for (; x + 4 <= width; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)(src + x));
        pixels = _mm_shuffle_epi8(pixels, shufMask);
        _mm_storeu_si128((__m128i *)(dst + x), pixels);
    }

    CopyLine_32ARGB_32BGRA_Scalar(info, x);
}

void CopyLine_32BGRA_32ARGB_SSE(const VxBlitInfo *info) {
    CopyLine_32ARGB_32BGRA_SSE(info); // Symmetric
}

#endif // VX_SIMD_SSE2
