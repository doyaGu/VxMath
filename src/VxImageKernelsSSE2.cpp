#include "VxImageKernels.h"
#include "VxSIMD.h"

static XBOOL IsRgb565(const VxImageDescEx &desc) {
    return desc.BitsPerPixel == 16 &&
        desc.RedMask == 0xF800 &&
        desc.GreenMask == 0x07E0 &&
        desc.BlueMask == 0x001F &&
        desc.AlphaMask == 0;
}

static XWORD AverageRgb565(XWORD p0, XWORD p1) {
    const XDWORD red = (((p0 & 0xF800u) >> 11) + ((p1 & 0xF800u) >> 11)) >> 1;
    const XDWORD green = (((p0 & 0x07E0u) >> 5) + ((p1 & 0x07E0u) >> 5)) >> 1;
    const XDWORD blue = ((p0 & 0x001Fu) + (p1 & 0x001Fu)) >> 1;
    return (XWORD) ((red << 11) | (green << 5) | blue);
}

static XWORD AverageRgb565(XWORD p0, XWORD p1, XWORD p2, XWORD p3) {
    const XDWORD red = (
        ((p0 & 0xF800u) >> 11) + ((p1 & 0xF800u) >> 11) +
        ((p2 & 0xF800u) >> 11) + ((p3 & 0xF800u) >> 11)
    ) >> 2;
    const XDWORD green = (
        ((p0 & 0x07E0u) >> 5) + ((p1 & 0x07E0u) >> 5) +
        ((p2 & 0x07E0u) >> 5) + ((p3 & 0x07E0u) >> 5)
    ) >> 2;
    const XDWORD blue = ((p0 & 0x001Fu) + (p1 & 0x001Fu) + (p2 & 0x001Fu) + (p3 & 0x001Fu)) >> 2;
    return (XWORD) ((red << 11) | (green << 5) | blue);
}

#if defined(VX_SIMD_SSE2)
static XDWORD Average32HorizontalScalar(XDWORD p0, XDWORD p1) {
    const XDWORD rb0 = p0 & 0x00FF00FFu;
    const XDWORD rb1 = p1 & 0x00FF00FFu;
    const XDWORD ag0 = (p0 & 0xFF00FF00u) >> 8;
    const XDWORD ag1 = (p1 & 0xFF00FF00u) >> 8;
    const XDWORD rbAvg = ((rb0 + rb1) >> 1) & 0x00FF00FFu;
    const XDWORD agAvg = ((ag0 + ag1) << 7) & 0xFF00FF00u;
    return rbAvg | agAvg;
}

static XDWORD Average32BlockScalar(XDWORD p0, XDWORD p1, XDWORD p2, XDWORD p3) {
    const XDWORD rb = (p0 & 0x00FF00FFu) + (p1 & 0x00FF00FFu) +
        (p2 & 0x00FF00FFu) + (p3 & 0x00FF00FFu);
    const XDWORD ag = ((p0 & 0xFF00FF00u) >> 8) + ((p1 & 0xFF00FF00u) >> 8) +
        ((p2 & 0xFF00FF00u) >> 8) + ((p3 & 0xFF00FF00u) >> 8);
    const XDWORD rbAvg = (rb >> 2) & 0x00FF00FFu;
    const XDWORD agAvg = ((ag >> 2) << 8) & 0xFF00FF00u;
    return rbAvg | agAvg;
}

static void Average32Horizontal2(const XDWORD *src, XBYTE *dst) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i pixels = _mm_loadu_si128((const __m128i *) src);
    const __m128i lo = _mm_unpacklo_epi8(pixels, zero);
    const __m128i hi = _mm_unpackhi_epi8(pixels, zero);
    const __m128i p0 = _mm_unpacklo_epi64(lo, hi);
    const __m128i p1 = _mm_unpackhi_epi64(lo, hi);
    const __m128i avg = _mm_srli_epi16(_mm_add_epi16(p0, p1), 1);
    const __m128i result = _mm_packus_epi16(avg, zero);
    _mm_storel_epi64((__m128i *) dst, result);
}

static void Average32Block2(const XDWORD *row0, const XBYTE *row1Bytes, XBYTE *dst) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i src0 = _mm_loadu_si128((const __m128i *) row0);
    const __m128i src1 = _mm_loadu_si128((const __m128i *) row1Bytes);
    const __m128i lo0 = _mm_unpacklo_epi8(src0, zero);
    const __m128i hi0 = _mm_unpackhi_epi8(src0, zero);
    const __m128i lo1 = _mm_unpacklo_epi8(src1, zero);
    const __m128i hi1 = _mm_unpackhi_epi8(src1, zero);
    const __m128i p0 = _mm_unpacklo_epi64(lo0, hi0);
    const __m128i p1 = _mm_unpackhi_epi64(lo0, hi0);
    const __m128i p2 = _mm_unpacklo_epi64(lo1, hi1);
    const __m128i p3 = _mm_unpackhi_epi64(lo1, hi1);
    const __m128i sum = _mm_add_epi16(_mm_add_epi16(_mm_add_epi16(p0, p1), p2), p3);
    const __m128i avg = _mm_srli_epi16(sum, 2);
    const __m128i result = _mm_packus_epi16(avg, zero);
    _mm_storel_epi64((__m128i *) dst, result);
}

static void StoreRgb565Averages4(XWORD *dst, __m128i red, __m128i green, __m128i blue) {
    __m128i pixels = _mm_or_si128(_mm_slli_epi32(red, 11), _mm_slli_epi32(green, 5));
    pixels = _mm_or_si128(pixels, blue);

    alignas(16) XDWORD packed[4];
    _mm_store_si128((__m128i *) packed, pixels);
    dst[0] = (XWORD) packed[0];
    dst[1] = (XWORD) packed[1];
    dst[2] = (XWORD) packed[2];
    dst[3] = (XWORD) packed[3];
}

static void AverageRgb565Horizontal4(const XWORD *src, XWORD *dst) {
    const __m128i pixels = _mm_loadu_si128((const __m128i *) src);
    const __m128i ones = _mm_set1_epi16(1);
    const __m128i red = _mm_srli_epi16(_mm_and_si128(pixels, _mm_set1_epi16((short) 0xF800)), 11);
    const __m128i green = _mm_srli_epi16(_mm_and_si128(pixels, _mm_set1_epi16(0x07E0)), 5);
    const __m128i blue = _mm_and_si128(pixels, _mm_set1_epi16(0x001F));

    StoreRgb565Averages4(
        dst,
        _mm_srli_epi32(_mm_madd_epi16(red, ones), 1),
        _mm_srli_epi32(_mm_madd_epi16(green, ones), 1),
        _mm_srli_epi32(_mm_madd_epi16(blue, ones), 1)
    );
}

static void AverageRgb565Block4(const XWORD *row0, const XWORD *row1, XWORD *dst) {
    const __m128i pixels0 = _mm_loadu_si128((const __m128i *) row0);
    const __m128i pixels1 = _mm_loadu_si128((const __m128i *) row1);
    const __m128i ones = _mm_set1_epi16(1);

    const __m128i red0 = _mm_srli_epi16(_mm_and_si128(pixels0, _mm_set1_epi16((short) 0xF800)), 11);
    const __m128i red1 = _mm_srli_epi16(_mm_and_si128(pixels1, _mm_set1_epi16((short) 0xF800)), 11);
    const __m128i green0 = _mm_srli_epi16(_mm_and_si128(pixels0, _mm_set1_epi16(0x07E0)), 5);
    const __m128i green1 = _mm_srli_epi16(_mm_and_si128(pixels1, _mm_set1_epi16(0x07E0)), 5);
    const __m128i blue0 = _mm_and_si128(pixels0, _mm_set1_epi16(0x001F));
    const __m128i blue1 = _mm_and_si128(pixels1, _mm_set1_epi16(0x001F));

    StoreRgb565Averages4(
        dst,
        _mm_srli_epi32(_mm_add_epi32(_mm_madd_epi16(red0, ones), _mm_madd_epi16(red1, ones)), 2),
        _mm_srli_epi32(_mm_add_epi32(_mm_madd_epi16(green0, ones), _mm_madd_epi16(green1, ones)), 2),
        _mm_srli_epi32(_mm_add_epi32(_mm_madd_epi16(blue0, ones), _mm_madd_epi16(blue1, ones)), 2)
    );
}
#endif

#if defined(VX_SIMD_SSSE3)
static __m128i LoadRgb24x4(const XBYTE *src) {
    const __m128i shuffle = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);
    return _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *) src), shuffle);
}

static __m128i LoadRgb24x4Exact(const XBYTE *src) {
    const __m128i shuffle = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);
    const __m128i lo = _mm_loadl_epi64((const __m128i *) src);
    const __m128i hi = _mm_cvtsi32_si128(*(const int *) (src + 8));
    return _mm_shuffle_epi8(_mm_or_si128(lo, _mm_slli_si128(hi, 8)), shuffle);
}

static XDWORD AverageRgb24Pair(__m128i row0Pair, __m128i row1Pair) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i sum0 = _mm_add_epi16(row0Pair, _mm_srli_si128(row0Pair, 8));
    const __m128i sum1 = _mm_add_epi16(row1Pair, _mm_srli_si128(row1Pair, 8));
    const __m128i avg = _mm_srli_epi16(_mm_add_epi16(sum0, sum1), 2);
    const __m128i packed = _mm_packus_epi16(avg, zero);
    return (XDWORD) _mm_cvtsi128_si32(packed);
}

static void StoreRgb24Averages4(XBYTE *dst, const XDWORD *pixels) {
    for (int i = 0; i < 4; ++i) {
        const XDWORD pixel = pixels[i];
        dst[i * 3 + 0] = (XBYTE) (pixel & 0xFFu);
        dst[i * 3 + 1] = (XBYTE) ((pixel >> 8) & 0xFFu);
        dst[i * 3 + 2] = (XBYTE) ((pixel >> 16) & 0xFFu);
    }
}

static void AverageRgb24Scalar(XBYTE *dst, const XBYTE *p0, const XBYTE *p1, const XBYTE *p2, const XBYTE *p3) {
    dst[0] = (XBYTE) (((int) p0[0] + p1[0] + p2[0] + p3[0]) >> 2);
    dst[1] = (XBYTE) (((int) p0[1] + p1[1] + p2[1] + p3[1]) >> 2);
    dst[2] = (XBYTE) (((int) p0[2] + p1[2] + p2[2] + p3[2]) >> 2);
}
#endif

XBOOL VxFillLuminance32SSE2(const XBYTE *row, int width, int *luminance) {
#if defined(VX_SIMD_SSE2)
    const __m128i mask = _mm_set1_epi32(0x000000FF);
    int x = 0;
    for (; x + 4 <= width; x += 4) {
        const __m128i pixels = _mm_loadu_si128((const __m128i *) (row + x * 4));
        const __m128i blue = _mm_and_si128(pixels, mask);
        const __m128i green = _mm_and_si128(_mm_srli_epi32(pixels, 8), mask);
        const __m128i red = _mm_and_si128(_mm_srli_epi32(pixels, 16), mask);
        const __m128i sum = _mm_add_epi32(_mm_add_epi32(red, green), blue);
        _mm_storeu_si128((__m128i *) (luminance + x), sum);
    }

    for (; x < width; ++x) {
        const XDWORD value = *(const XDWORD *) (row + x * 4);
        luminance[x] = (int) ((value & 0xFF) + ((value >> 8) & 0xFF) + ((value >> 16) & 0xFF));
    }
    return TRUE;
#else
    (void) row;
    (void) width;
    (void) luminance;
    return FALSE;
#endif
}

XBOOL VxFillLuminance24SSSE3(const XBYTE *row, int width, int *luminance) {
#if defined(VX_SIMD_SSSE3)
    const __m128i mask = _mm_set1_epi32(0x000000FF);
    int x = 0;
    for (; x + 4 <= width; x += 4) {
        const __m128i pixels = LoadRgb24x4Exact(row + x * 3);
        const __m128i blue = _mm_and_si128(pixels, mask);
        const __m128i green = _mm_and_si128(_mm_srli_epi32(pixels, 8), mask);
        const __m128i red = _mm_and_si128(_mm_srli_epi32(pixels, 16), mask);
        const __m128i sum = _mm_add_epi32(_mm_add_epi32(red, green), blue);
        _mm_storeu_si128((__m128i *) (luminance + x), sum);
    }

    for (; x < width; ++x) {
        const XBYTE *pixel = row + x * 3;
        luminance[x] = (int) pixel[0] + pixel[1] + pixel[2];
    }
    return TRUE;
#else
    (void) row;
    (void) width;
    (void) luminance;
    return FALSE;
#endif
}

XBOOL VxGenerateMipMap32SSE2(const VxImageDescEx &src_desc, XBYTE *Buffer) {
#if defined(VX_SIMD_SSE2)
    if (src_desc.BitsPerPixel != 32) return FALSE;

    const int dstWidth = src_desc.Width >> 1;
    const int dstHeight = src_desc.Height >> 1;
    const int bytesPerLine = src_desc.BytesPerLine;
    XBYTE *image = src_desc.Image;

    if (dstWidth == 0) {
        if (dstHeight) {
            XBYTE *dst = Buffer;
            XBYTE *src = image;
            int h = dstHeight;
            while (h > 0) {
                *(XDWORD *) dst = Average32HorizontalScalar(*(XDWORD *) src, *(XDWORD *) (src + bytesPerLine));
                dst += bytesPerLine;
                src += bytesPerLine * 2;
                --h;
            }
        }
        return TRUE;
    }

    if (dstHeight == 0) {
        XBYTE *dst = Buffer;
        XDWORD *src = (XDWORD *) image;
        int w = dstWidth;
        while (w >= 2) {
            Average32Horizontal2(src, dst);
            src += 4;
            dst += 8;
            w -= 2;
        }
        while (w > 0) {
            *(XDWORD *) dst = Average32HorizontalScalar(src[0], src[1]);
            src += 2;
            dst += 4;
            --w;
        }
        return TRUE;
    }

    XBYTE *dst = Buffer;
    XDWORD *src = (XDWORD *) image;
    int h = dstHeight;
    while (h > 0) {
        XDWORD *rowStart = src;
        int w = dstWidth;
        while (w >= 2) {
            Average32Block2(src, (const XBYTE *) src + bytesPerLine, dst);
            src += 4;
            dst += 8;
            w -= 2;
        }
        while (w > 0) {
            const XDWORD p0 = src[0];
            const XDWORD p1 = src[1];
            const XDWORD p2 = *(XDWORD *) ((XBYTE *) src + bytesPerLine);
            const XDWORD p3 = *(XDWORD *) ((XBYTE *) src + bytesPerLine + 4);
            *(XDWORD *) dst = Average32BlockScalar(p0, p1, p2, p3);
            src += 2;
            dst += 4;
            --w;
        }
        src = (XDWORD *) ((XBYTE *) rowStart + bytesPerLine * 2);
        --h;
    }
    return TRUE;
#else
    (void) src_desc;
    (void) Buffer;
    return FALSE;
#endif
}

XBOOL VxGenerateMipMap16Rgb565SSE2(const VxImageDescEx &src_desc, XBYTE *Buffer) {
#if defined(VX_SIMD_SSE2)
    if (!IsRgb565(src_desc)) {
        return FALSE;
    }

    const int dstWidth = src_desc.Width >> 1;
    const int dstHeight = src_desc.Height >> 1;
    if (dstWidth < 4) {
        return FALSE;
    }

    if (dstHeight == 0) {
        XWORD *dst = (XWORD *) Buffer;
        const XWORD *src = (const XWORD *) src_desc.Image;
        int x = 0;
        for (; x + 4 <= dstWidth; x += 4) {
            AverageRgb565Horizontal4(src + x * 2, dst + x);
        }
        for (; x < dstWidth; ++x) {
            const XWORD *pixel = src + x * 2;
            dst[x] = AverageRgb565(pixel[0], pixel[1]);
        }
        return TRUE;
    }

    XWORD *dst = (XWORD *) Buffer;
    for (int y = 0; y < dstHeight; ++y) {
        const XBYTE *row0 = src_desc.Image + y * 2 * src_desc.BytesPerLine;
        const XBYTE *row1 = row0 + src_desc.BytesPerLine;
        int x = 0;
        for (; x + 4 <= dstWidth; x += 4) {
            AverageRgb565Block4((const XWORD *) (row0 + x * 4), (const XWORD *) (row1 + x * 4), dst + x);
        }
        for (; x < dstWidth; ++x) {
            const XWORD *p0 = (const XWORD *) (row0 + x * 4);
            const XWORD *p2 = (const XWORD *) (row1 + x * 4);
            dst[x] = AverageRgb565(p0[0], p0[1], p2[0], p2[1]);
        }
        dst += dstWidth;
    }
    return TRUE;
#else
    (void) src_desc;
    (void) Buffer;
    return FALSE;
#endif
}

XBOOL VxGenerateMipMap24Rgb888SSSE3(const VxImageDescEx &src_desc, XBYTE *Buffer) {
#if defined(VX_SIMD_SSSE3)
    const int dstWidth = src_desc.Width >> 1;
    const int dstHeight = src_desc.Height >> 1;
    if (src_desc.BitsPerPixel != 24 || dstWidth < 5 || dstHeight == 0) {
        return FALSE;
    }

    XBYTE *dst = Buffer;
    for (int y = 0; y < dstHeight; ++y) {
        const XBYTE *row0 = src_desc.Image + y * 2 * src_desc.BytesPerLine;
        const XBYTE *row1 = row0 + src_desc.BytesPerLine;
        int x = 0;
        for (; x + 4 < dstWidth; x += 4) {
            const XBYTE *src0 = row0 + x * 6;
            const XBYTE *src1 = row1 + x * 6;

            const __m128i row0First = LoadRgb24x4(src0);
            const __m128i row0Second = LoadRgb24x4(src0 + 12);
            const __m128i row1First = LoadRgb24x4(src1);
            const __m128i row1Second = LoadRgb24x4(src1 + 12);

            const __m128i row0Lo = _mm_unpacklo_epi8(row0First, _mm_setzero_si128());
            const __m128i row0Hi = _mm_unpackhi_epi8(row0First, _mm_setzero_si128());
            const __m128i row0Lo2 = _mm_unpacklo_epi8(row0Second, _mm_setzero_si128());
            const __m128i row0Hi2 = _mm_unpackhi_epi8(row0Second, _mm_setzero_si128());
            const __m128i row1Lo = _mm_unpacklo_epi8(row1First, _mm_setzero_si128());
            const __m128i row1Hi = _mm_unpackhi_epi8(row1First, _mm_setzero_si128());
            const __m128i row1Lo2 = _mm_unpacklo_epi8(row1Second, _mm_setzero_si128());
            const __m128i row1Hi2 = _mm_unpackhi_epi8(row1Second, _mm_setzero_si128());

            XDWORD pixels[4];
            pixels[0] = AverageRgb24Pair(row0Lo, row1Lo);
            pixels[1] = AverageRgb24Pair(row0Hi, row1Hi);
            pixels[2] = AverageRgb24Pair(row0Lo2, row1Lo2);
            pixels[3] = AverageRgb24Pair(row0Hi2, row1Hi2);
            StoreRgb24Averages4(dst + x * 3, pixels);
        }

        for (; x < dstWidth; ++x) {
            const XBYTE *p0 = row0 + x * 6;
            const XBYTE *p2 = row1 + x * 6;
            AverageRgb24Scalar(dst + x * 3, p0, p0 + 3, p2, p2 + 3);
        }
        dst += dstWidth * 3;
    }
    return TRUE;
#else
    (void) src_desc;
    (void) Buffer;
    return FALSE;
#endif
}
