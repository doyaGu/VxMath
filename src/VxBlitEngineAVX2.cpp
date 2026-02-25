// =========================================================================
// VxBlitEngineAVX2.cpp -- AVX2 SIMD blit kernels (runtime-dispatched)
//
// Part of VxBlitEngine. Compiled only when VX_SIMD_AVX2
// is defined. Built with /arch:AVX2 (MSVC) or -mavx2;-mfma (GCC/Clang).
// Tail loops delegate to _Scalar() helpers in VxBlitKernels.cpp.
// =========================================================================

#include "VxBlitInternal.h"

#if defined(VX_SIMD_AVX2)

#include <immintrin.h>

// -- 32<->32 format conversions --------------------------------------------

void CopyLine_32ARGB_32RGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i mask = _mm256_set1_epi32(0x00FFFFFF);
    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        _mm256_storeu_si256((__m256i *)(dst + x), _mm256_and_si256(pixels, mask));
    }
    CopyLine_32ARGB_32RGB_Scalar(info, x);
}

void CopyLine_32RGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i alpha = _mm256_set1_epi32(0xFF000000);
    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        _mm256_storeu_si256((__m256i *)(dst + x), _mm256_or_si256(pixels, alpha));
    }
    CopyLine_32RGB_32ARGB_Scalar(info, x);
}

void CopyLine_32ARGB_24RGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XBYTE *dst = info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        const XDWORD p0 = src[x + 0];
        const XDWORD p1 = src[x + 1];
        const XDWORD p2 = src[x + 2];
        const XDWORD p3 = src[x + 3];
        const XDWORD p4 = src[x + 4];
        const XDWORD p5 = src[x + 5];
        const XDWORD p6 = src[x + 6];
        const XDWORD p7 = src[x + 7];

        dst[0] = (XBYTE)p0;  dst[1] = (XBYTE)(p0 >> 8);  dst[2] = (XBYTE)(p0 >> 16);
        dst[3] = (XBYTE)p1;  dst[4] = (XBYTE)(p1 >> 8);  dst[5] = (XBYTE)(p1 >> 16);
        dst[6] = (XBYTE)p2;  dst[7] = (XBYTE)(p2 >> 8);  dst[8] = (XBYTE)(p2 >> 16);
        dst[9] = (XBYTE)p3;  dst[10] = (XBYTE)(p3 >> 8); dst[11] = (XBYTE)(p3 >> 16);
        dst[12] = (XBYTE)p4; dst[13] = (XBYTE)(p4 >> 8); dst[14] = (XBYTE)(p4 >> 16);
        dst[15] = (XBYTE)p5; dst[16] = (XBYTE)(p5 >> 8); dst[17] = (XBYTE)(p5 >> 16);
        dst[18] = (XBYTE)p6; dst[19] = (XBYTE)(p6 >> 8); dst[20] = (XBYTE)(p6 >> 16);
        dst[21] = (XBYTE)p7; dst[22] = (XBYTE)(p7 >> 8); dst[23] = (XBYTE)(p7 >> 16);
        dst += 24;
    }

    CopyLine_32ARGB_24RGB_Scalar(info, x, (const XBYTE *)(src + x), dst);
}

void CopyLine_24RGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i alpha = _mm256_set1_epi32(0xFF000000);
    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_setr_epi32(
            src[0] | (src[1] << 8) | (src[2] << 16),
            src[3] | (src[4] << 8) | (src[5] << 16),
            src[6] | (src[7] << 8) | (src[8] << 16),
            src[9] | (src[10] << 8) | (src[11] << 16),
            src[12] | (src[13] << 8) | (src[14] << 16),
            src[15] | (src[16] << 8) | (src[17] << 16),
            src[18] | (src[19] << 8) | (src[20] << 16),
            src[21] | (src[22] << 8) | (src[23] << 16)
        );
        pixels = _mm256_or_si256(pixels, alpha);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
        src += 24;
    }

    CopyLine_24RGB_32ARGB_Scalar(info, x, src, dst + x);
}

// -- Paletted conversion --------------------------------------------------

void CopyLine_Paletted8_32ARGB_AVX2(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XBYTE *colorMap = info->colorMap;
    const int bpc = info->bytesPerColorEntry;

    if (!colorMap) {
        return;
    }

    XDWORD palette32[256];
    for (int i = 0; i < 256; ++i) {
        const XBYTE *entry = colorMap + i * bpc;
        const XDWORD b = entry[0];
        const XDWORD g = entry[1];
        const XDWORD r = entry[2];
        const XDWORD a = (bpc >= 4) ? entry[3] : 0xFFu;
        palette32[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        const __m128i idxBytes = _mm_loadl_epi64((const __m128i *)(src + x));
        const __m256i idx32 = _mm256_cvtepu8_epi32(idxBytes);
        const __m256i colors = _mm256_i32gather_epi32((const int *)palette32, idx32, 4);
        _mm256_storeu_si256((__m256i *)(dst + x), colors);
    }
    for (; x < width; ++x) {
        dst[x] = palette32[src[x]];
    }
}

// -- Channel swap conversions ---------------------------------------------

void CopyLine_32ARGB_32ABGR_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i shufMask = _mm256_setr_epi8(
        2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15,
        2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    );

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        _mm256_storeu_si256((__m256i *)(dst + x), _mm256_shuffle_epi8(pixels, shufMask));
    }
    CopyLine_32ARGB_32ABGR_Scalar(info, x);
}

void CopyLine_32ABGR_32ARGB_AVX2(const VxBlitInfo *info) {
    CopyLine_32ARGB_32ABGR_AVX2(info);
}

void CopyLine_32ARGB_32RGBA_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i p = _mm256_loadu_si256((const __m256i *)(src + x));
        __m256i res = _mm256_or_si256(_mm256_slli_epi32(p, 8), _mm256_srli_epi32(p, 24));
        _mm256_storeu_si256((__m256i *)(dst + x), res);
    }
    CopyLine_32ARGB_32RGBA_Scalar(info, x);
}

void CopyLine_32RGBA_32ARGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i p = _mm256_loadu_si256((const __m256i *)(src + x));
        __m256i res = _mm256_or_si256(_mm256_srli_epi32(p, 8), _mm256_slli_epi32(p, 24));
        _mm256_storeu_si256((__m256i *)(dst + x), res);
    }
    CopyLine_32RGBA_32ARGB_Scalar(info, x);
}

void CopyLine_32ARGB_32BGRA_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i shufMask = _mm256_setr_epi8(
        3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
        3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12
    );

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        _mm256_storeu_si256((__m256i *)(dst + x), _mm256_shuffle_epi8(pixels, shufMask));
    }
    CopyLine_32ARGB_32BGRA_Scalar(info, x);
}

void CopyLine_32BGRA_32ARGB_AVX2(const VxBlitInfo *info) {
    CopyLine_32ARGB_32BGRA_AVX2(info);
}

// -- 32<->16 format conversions --------------------------------------------

void CopyLine_32ARGB_565RGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskR = _mm256_set1_epi32(0xF800);
    const __m256i maskG = _mm256_set1_epi32(0x07E0);
    const __m256i maskB = _mm256_set1_epi32(0x001F);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i r = _mm256_and_si256(_mm256_srli_epi32(pixels, 8), maskR);
        const __m256i g = _mm256_and_si256(_mm256_srli_epi32(pixels, 5), maskG);
        const __m256i b = _mm256_and_si256(_mm256_srli_epi32(pixels, 3), maskB);
        const __m256i packed32 = _mm256_or_si256(_mm256_or_si256(r, g), b);

        const __m128i lo = _mm256_castsi256_si128(packed32);
        const __m128i hi = _mm256_extracti128_si256(packed32, 1);
        const __m128i packed16 = _mm_packus_epi32(lo, hi);
        _mm_storeu_si128((__m128i *)(dst + x), packed16);
    }
    CopyLine_32ARGB_565RGB_Scalar(info, x);
}

void CopyLine_32ARGB_555RGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskR = _mm256_set1_epi32(0x7C00);
    const __m256i maskG = _mm256_set1_epi32(0x03E0);
    const __m256i maskB = _mm256_set1_epi32(0x001F);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i r = _mm256_and_si256(_mm256_srli_epi32(pixels, 9), maskR);
        const __m256i g = _mm256_and_si256(_mm256_srli_epi32(pixels, 6), maskG);
        const __m256i b = _mm256_and_si256(_mm256_srli_epi32(pixels, 3), maskB);
        const __m256i packed32 = _mm256_or_si256(_mm256_or_si256(r, g), b);

        const __m128i lo = _mm256_castsi256_si128(packed32);
        const __m128i hi = _mm256_extracti128_si256(packed32, 1);
        const __m128i packed16 = _mm_packus_epi32(lo, hi);
        _mm_storeu_si128((__m128i *)(dst + x), packed16);
    }
    CopyLine_32ARGB_555RGB_Scalar(info, x);
}

void CopyLine_32ARGB_1555ARGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskA = _mm256_set1_epi32(0x8000);
    const __m256i maskR = _mm256_set1_epi32(0x7C00);
    const __m256i maskG = _mm256_set1_epi32(0x03E0);
    const __m256i maskB = _mm256_set1_epi32(0x001F);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i a = _mm256_and_si256(_mm256_srli_epi32(pixels, 16), maskA);
        const __m256i r = _mm256_and_si256(_mm256_srli_epi32(pixels, 9), maskR);
        const __m256i g = _mm256_and_si256(_mm256_srli_epi32(pixels, 6), maskG);
        const __m256i b = _mm256_and_si256(_mm256_srli_epi32(pixels, 3), maskB);
        const __m256i packed32 = _mm256_or_si256(_mm256_or_si256(a, r), _mm256_or_si256(g, b));

        const __m128i lo = _mm256_castsi256_si128(packed32);
        const __m128i hi = _mm256_extracti128_si256(packed32, 1);
        const __m128i packed16 = _mm_packus_epi32(lo, hi);
        _mm_storeu_si128((__m128i *)(dst + x), packed16);
    }
    CopyLine_32ARGB_1555ARGB_Scalar(info, x);
}

void CopyLine_32ARGB_4444ARGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XWORD *dst = (XWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskA = _mm256_set1_epi32(0xF000);
    const __m256i maskR = _mm256_set1_epi32(0x0F00);
    const __m256i maskG = _mm256_set1_epi32(0x00F0);
    const __m256i maskB = _mm256_set1_epi32(0x000F);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i a = _mm256_and_si256(_mm256_srli_epi32(pixels, 16), maskA);
        const __m256i r = _mm256_and_si256(_mm256_srli_epi32(pixels, 12), maskR);
        const __m256i g = _mm256_and_si256(_mm256_srli_epi32(pixels, 8), maskG);
        const __m256i b = _mm256_and_si256(_mm256_srli_epi32(pixels, 4), maskB);
        const __m256i packed32 = _mm256_or_si256(_mm256_or_si256(a, r), _mm256_or_si256(g, b));

        const __m128i lo = _mm256_castsi256_si128(packed32);
        const __m128i hi = _mm256_extracti128_si256(packed32, 1);
        const __m128i packed16 = _mm_packus_epi32(lo, hi);
        _mm_storeu_si128((__m128i *)(dst + x), packed16);
    }
    CopyLine_32ARGB_4444ARGB_Scalar(info, x);
}

void CopyLine_565RGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i alpha = _mm256_set1_epi32(0xFF000000);
    const __m256i maskF800 = _mm256_set1_epi32(0xF800);
    const __m256i maskE000 = _mm256_set1_epi32(0xE000);
    const __m256i mask07E0 = _mm256_set1_epi32(0x07E0);
    const __m256i mask0600 = _mm256_set1_epi32(0x0600);
    const __m256i mask001F = _mm256_set1_epi32(0x001F);
    const __m256i mask001C = _mm256_set1_epi32(0x001C);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m128i packed16 = _mm_loadu_si128((const __m128i *)(src + x));
        const __m256i pixels = _mm256_cvtepu16_epi32(packed16);

        const __m256i r = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, maskF800), 8),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, maskE000), 3));
        const __m256i g = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask07E0), 5),
                                          _mm256_srli_epi32(_mm256_and_si256(pixels, mask0600), 1));
        const __m256i b = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask001F), 3),
                                          _mm256_srli_epi32(_mm256_and_si256(pixels, mask001C), 2));

        const __m256i out = _mm256_or_si256(alpha, _mm256_or_si256(r, _mm256_or_si256(g, b)));
        _mm256_storeu_si256((__m256i *)(dst + x), out);
    }
    CopyLine_565RGB_32ARGB_Scalar(info, x);
}

void CopyLine_555RGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i alpha = _mm256_set1_epi32(0xFF000000);
    const __m256i mask7C00 = _mm256_set1_epi32(0x7C00);
    const __m256i mask7000 = _mm256_set1_epi32(0x7000);
    const __m256i mask03E0 = _mm256_set1_epi32(0x03E0);
    const __m256i mask0380 = _mm256_set1_epi32(0x0380);
    const __m256i mask001F = _mm256_set1_epi32(0x001F);
    const __m256i mask001C = _mm256_set1_epi32(0x001C);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m128i packed16 = _mm_loadu_si128((const __m128i *)(src + x));
        const __m256i pixels = _mm256_cvtepu16_epi32(packed16);

        const __m256i r = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask7C00), 9),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask7000), 4));
        const __m256i g = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask03E0), 6),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask0380), 1));
        const __m256i b = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask001F), 3),
                                          _mm256_srli_epi32(_mm256_and_si256(pixels, mask001C), 2));

        const __m256i out = _mm256_or_si256(alpha, _mm256_or_si256(r, _mm256_or_si256(g, b)));
        _mm256_storeu_si256((__m256i *)(dst + x), out);
    }
    CopyLine_555RGB_32ARGB_Scalar(info, x);
}

void CopyLine_1555ARGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i fullAlpha = _mm256_set1_epi32(0xFF000000);
    const __m256i zero = _mm256_setzero_si256();
    const __m256i mask8000 = _mm256_set1_epi32(0x8000);
    const __m256i mask7C00 = _mm256_set1_epi32(0x7C00);
    const __m256i mask7000 = _mm256_set1_epi32(0x7000);
    const __m256i mask03E0 = _mm256_set1_epi32(0x03E0);
    const __m256i mask0380 = _mm256_set1_epi32(0x0380);
    const __m256i mask001F = _mm256_set1_epi32(0x001F);
    const __m256i mask001C = _mm256_set1_epi32(0x001C);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m128i packed16 = _mm_loadu_si128((const __m128i *)(src + x));
        const __m256i pixels = _mm256_cvtepu16_epi32(packed16);

        const __m256i alphaMask = _mm256_cmpgt_epi32(_mm256_and_si256(pixels, mask8000), zero);
        const __m256i a = _mm256_and_si256(alphaMask, fullAlpha);
        const __m256i r = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask7C00), 9),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask7000), 4));
        const __m256i g = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask03E0), 6),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask0380), 1));
        const __m256i b = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask001F), 3),
                                          _mm256_srli_epi32(_mm256_and_si256(pixels, mask001C), 2));

        const __m256i out = _mm256_or_si256(a, _mm256_or_si256(r, _mm256_or_si256(g, b)));
        _mm256_storeu_si256((__m256i *)(dst + x), out);
    }
    CopyLine_1555ARGB_32ARGB_Scalar(info, x);
}

void CopyLine_4444ARGB_32ARGB_AVX2(const VxBlitInfo *info) {
    const XWORD *src = (const XWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskF000 = _mm256_set1_epi32(0xF000);
    const __m256i mask0F00 = _mm256_set1_epi32(0x0F00);
    const __m256i mask00F0 = _mm256_set1_epi32(0x00F0);
    const __m256i mask000F = _mm256_set1_epi32(0x000F);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m128i packed16 = _mm_loadu_si128((const __m128i *)(src + x));
        const __m256i pixels = _mm256_cvtepu16_epi32(packed16);

        const __m256i a = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, maskF000), 16),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, maskF000), 12));
        const __m256i r = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask0F00), 12),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask0F00), 8));
        const __m256i g = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask00F0), 8),
                                          _mm256_slli_epi32(_mm256_and_si256(pixels, mask00F0), 4));
        const __m256i b = _mm256_or_si256(_mm256_slli_epi32(_mm256_and_si256(pixels, mask000F), 4),
                                          _mm256_and_si256(pixels, mask000F));

        const __m256i out = _mm256_or_si256(a, _mm256_or_si256(r, _mm256_or_si256(g, b)));
        _mm256_storeu_si256((__m256i *)(dst + x), out);
    }
    CopyLine_4444ARGB_32ARGB_Scalar(info, x);
}

// -- Fill operations ------------------------------------------------------

void FillLine_32_AVX2(XDWORD *dst, int width, XDWORD color) {
    const __m256i fillColor = _mm256_set1_epi32(color);
    int x = 0;

    for (; x + 32 <= width; x += 32) {
        _mm256_storeu_si256((__m256i *)(dst + x), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 8), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 16), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 24), fillColor);
    }
    for (; x + 8 <= width; x += 8) {
        _mm256_storeu_si256((__m256i *)(dst + x), fillColor);
    }
    FillLine_32_Scalar(dst, x, width, color);
}

void FillLine_16_AVX2(XWORD *dst, int width, XWORD color) {
    const __m256i fillColor = _mm256_set1_epi16((short)color);
    int x = 0;

    for (; x + 64 <= width; x += 64) {
        _mm256_storeu_si256((__m256i *)(dst + x), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 16), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 32), fillColor);
        _mm256_storeu_si256((__m256i *)(dst + x + 48), fillColor);
    }
    for (; x + 16 <= width; x += 16) {
        _mm256_storeu_si256((__m256i *)(dst + x), fillColor);
    }
    FillLine_16_Scalar(dst, x, width, color);
}

// -- Alpha operations -----------------------------------------------------

void PremultiplyAlpha_32ARGB_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi16(1);
    const __m256i alphaMask = _mm256_set1_epi32(0xFF000000);
    const __m256i alphaReplicate = _mm256_setr_epi8(
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15
    );

    int x = 0;
    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + x));
        __m256i alphaBytes = _mm256_shuffle_epi8(pixels, alphaReplicate);

        __m256i lo16 = _mm256_unpacklo_epi8(pixels, zero);
        __m256i hi16 = _mm256_unpackhi_epi8(pixels, zero);
        __m256i aLo16 = _mm256_unpacklo_epi8(alphaBytes, zero);
        __m256i aHi16 = _mm256_unpackhi_epi8(alphaBytes, zero);

        __m256i prodLo = _mm256_mullo_epi16(lo16, aLo16);
        __m256i prodHi = _mm256_mullo_epi16(hi16, aHi16);

        __m256i tmpLo = _mm256_add_epi16(prodLo, one);
        __m256i tmpHi = _mm256_add_epi16(prodHi, one);
        tmpLo = _mm256_add_epi16(tmpLo, _mm256_srli_epi16(tmpLo, 8));
        tmpHi = _mm256_add_epi16(tmpHi, _mm256_srli_epi16(tmpHi, 8));
        __m256i resLo = _mm256_srli_epi16(tmpLo, 8);
        __m256i resHi = _mm256_srli_epi16(tmpHi, 8);

        __m256i result = _mm256_packus_epi16(resLo, resHi);
        result = _mm256_or_si256(_mm256_andnot_si256(alphaMask, result),
                                 _mm256_and_si256(alphaMask, pixels));
        _mm256_storeu_si256((__m256i *)(dst + x), result);
    }
    PremultiplyAlpha_32ARGB_Scalar(info, x);
}

void UnpremultiplyAlpha_32ARGB_AVX2(const VxBlitInfo *info) {
    // AVX2 integer division is impractical; delegate entirely to scalar.
    UnpremultiplyAlpha_32ARGB_Scalar(info, 0);
}

void ClearAlpha_32_AVX2(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m256i mask = _mm256_set1_epi32(0x00FFFFFF);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(dst + x));
        pixels = _mm256_and_si256(pixels, mask);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
    }
    ClearAlpha_32_Scalar(dst, x, width);
}

void SetFullAlpha_32_AVX2(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m256i alpha = _mm256_set1_epi32(0xFF000000);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(dst + x));
        pixels = _mm256_or_si256(pixels, alpha);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
    }
    SetFullAlpha_32_Scalar(dst, x, width);
}

// -- Image effects --------------------------------------------------------

void InvertColors_32_AVX2(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const __m256i mask = _mm256_set1_epi32(0x00FFFFFF);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(dst + x));
        pixels = _mm256_xor_si256(pixels, mask);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
    }
    InvertColors_32_Scalar(dst, x, width);
}

void Grayscale_32_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i maskAlpha = _mm256_set1_epi32(0xFF000000);
    const __m256i maskFF = _mm256_set1_epi32(0xFF);
    const __m256i coefR = _mm256_set1_epi32(77);
    const __m256i coefG = _mm256_set1_epi32(150);
    const __m256i coefB = _mm256_set1_epi32(29);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i p = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i a = _mm256_and_si256(p, maskAlpha);
        const __m256i r = _mm256_and_si256(_mm256_srli_epi32(p, 16), maskFF);
        const __m256i g = _mm256_and_si256(_mm256_srli_epi32(p, 8), maskFF);
        const __m256i b = _mm256_and_si256(p, maskFF);

        const __m256i yr = _mm256_mullo_epi32(r, coefR);
        const __m256i yg = _mm256_mullo_epi32(g, coefG);
        const __m256i yb = _mm256_mullo_epi32(b, coefB);
        const __m256i y = _mm256_srli_epi32(_mm256_add_epi32(_mm256_add_epi32(yr, yg), yb), 8);

        const __m256i out = _mm256_or_si256(
            a,
            _mm256_or_si256(y, _mm256_or_si256(_mm256_slli_epi32(y, 8), _mm256_slli_epi32(y, 16)))
        );
        _mm256_storeu_si256((__m256i *)(dst + x), out);
    }
    Grayscale_32_Scalar(src, dst, x, width);
}

void SetAlpha_32_AVX2(const VxBlitInfo *info) {
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const XDWORD alphaValue = info->alphaValue & info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;
    const __m256i vAlphaValue = _mm256_set1_epi32((int)alphaValue);
    const __m256i vAlphaMaskInv = _mm256_set1_epi32((int)alphaMaskInv);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        __m256i pixels = _mm256_loadu_si256((const __m256i *)(dst + x));
        pixels = _mm256_and_si256(pixels, vAlphaMaskInv);
        pixels = _mm256_or_si256(pixels, vAlphaValue);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
    }
    SetAlpha_32_Scalar(dst, x, width, alphaValue, alphaMaskInv);
}

void CopyAlpha_32_AVX2(const VxBlitInfo *info) {
    const XBYTE *src = info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;
    const int alphaShift = info->alphaShiftDst;
    const XDWORD alphaMask = info->dstAlphaMask;
    const XDWORD alphaMaskInv = info->alphaMaskInv;
    const __m256i vAlphaMaskInv = _mm256_set1_epi32((int)alphaMaskInv);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m128i a8 = _mm_loadl_epi64((const __m128i *)(src + x));
        __m256i a32 = _mm256_cvtepu8_epi32(a8);
        a32 = _mm256_slli_epi32(a32, alphaShift);

        __m256i pixels = _mm256_loadu_si256((const __m256i *)(dst + x));
        pixels = _mm256_and_si256(pixels, vAlphaMaskInv);
        pixels = _mm256_or_si256(pixels, a32);
        _mm256_storeu_si256((__m256i *)(dst + x), pixels);
    }
    CopyAlpha_32_Scalar(src, dst, x, width, alphaShift, alphaMask, alphaMaskInv);
}

void MultiplyBlend_32_AVX2(const VxBlitInfo *info) {
    const XDWORD *src = (const XDWORD *)info->srcLine;
    XDWORD *dst = (XDWORD *)info->dstLine;
    const int width = info->width;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i round = _mm256_set1_epi16(128);
    int x = 0;

    for (; x + 8 <= width; x += 8) {
        const __m256i srcPix = _mm256_loadu_si256((const __m256i *)(src + x));
        const __m256i dstPix = _mm256_loadu_si256((const __m256i *)(dst + x));

        __m256i srcLo = _mm256_unpacklo_epi8(srcPix, zero);
        __m256i srcHi = _mm256_unpackhi_epi8(srcPix, zero);
        __m256i dstLo = _mm256_unpacklo_epi8(dstPix, zero);
        __m256i dstHi = _mm256_unpackhi_epi8(dstPix, zero);

        __m256i prodLo = _mm256_mullo_epi16(srcLo, dstLo);
        __m256i prodHi = _mm256_mullo_epi16(srcHi, dstHi);

        __m256i tmpLo = _mm256_add_epi16(prodLo, round);
        __m256i tmpHi = _mm256_add_epi16(prodHi, round);
        tmpLo = _mm256_add_epi16(tmpLo, _mm256_srli_epi16(tmpLo, 8));
        tmpHi = _mm256_add_epi16(tmpHi, _mm256_srli_epi16(tmpHi, 8));
        __m256i resLo = _mm256_srli_epi16(tmpLo, 8);
        __m256i resHi = _mm256_srli_epi16(tmpHi, 8);

        const __m256i result = _mm256_packus_epi16(resLo, resHi);
        _mm256_storeu_si256((__m256i *)(dst + x), result);
    }
    MultiplyBlend_32_Scalar(src, dst, x, width);
}

#endif // VX_SIMD_AVX2
