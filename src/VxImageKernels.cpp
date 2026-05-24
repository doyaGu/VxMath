#include <limits.h>
#include <math.h>
#include <string.h>

#include "VxImageKernels.h"
#include "VxBlitEngine.h"
#include "VxSIMD.h"

static void AveragePixel24(XBYTE *dst, const XBYTE *p0, const XBYTE *p1) {
    dst[0] = (XBYTE) (((int) p0[0] + p1[0]) >> 1);
    dst[1] = (XBYTE) (((int) p0[1] + p1[1]) >> 1);
    dst[2] = (XBYTE) (((int) p0[2] + p1[2]) >> 1);
}

static void AveragePixel24(XBYTE *dst, const XBYTE *p0, const XBYTE *p1, const XBYTE *p2, const XBYTE *p3) {
    dst[0] = (XBYTE) (((int) p0[0] + p1[0] + p2[0] + p3[0]) >> 2);
    dst[1] = (XBYTE) (((int) p0[1] + p1[1] + p2[1] + p3[1]) >> 2);
    dst[2] = (XBYTE) (((int) p0[2] + p1[2] + p2[2] + p3[2]) >> 2);
}

static void GenerateMipMap24(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    const int height = src_desc.Height;
    const int bytesPerLine = src_desc.BytesPerLine;
    XBYTE *image = src_desc.Image;

    const int dstWidth = src_desc.Width >> 1;
    const int dstHeight = height >> 1;

    if (dstWidth == 0) {
        if (dstHeight) {
            XBYTE *dst = Buffer;
            XBYTE *src = image;
            for (int y = 0; y < dstHeight; ++y) {
                AveragePixel24(dst, src, src + bytesPerLine);
                dst += 3;
                src += bytesPerLine * 2;
            }
        }
        return;
    }

    if (dstHeight == 0) {
        XBYTE *dst = Buffer;
        XBYTE *src = image;
        for (int x = 0; x < dstWidth; ++x) {
            AveragePixel24(dst, src, src + 3);
            dst += 3;
            src += 6;
        }
        return;
    }

    XBYTE *dst = Buffer;
    for (int y = 0; y < dstHeight; ++y) {
        XBYTE *row0 = image + y * 2 * bytesPerLine;
        XBYTE *row1 = row0 + bytesPerLine;
        for (int x = 0; x < dstWidth; ++x) {
            XBYTE *p0 = row0 + x * 6;
            XBYTE *p2 = row1 + x * 6;
            AveragePixel24(dst, p0, p0 + 3, p2, p2 + 3);
            dst += 3;
        }
    }
}

struct PixelChannel16 {
    XWORD Mask;
    XBYTE Shift;
};

struct PixelLayout16 {
    PixelChannel16 Channels[4];
    int Count;
};

static void AddPixelChannel16(PixelLayout16 &layout, XDWORD mask) {
    if (!mask) return;
    layout.Channels[layout.Count].Mask = (XWORD) mask;
    layout.Channels[layout.Count].Shift = (XBYTE) GetBitShift(mask);
    ++layout.Count;
}

static PixelLayout16 BuildPixelLayout16(const VxImageDescEx &desc) {
    PixelLayout16 layout = {};
    AddPixelChannel16(layout, desc.RedMask);
    AddPixelChannel16(layout, desc.GreenMask);
    AddPixelChannel16(layout, desc.BlueMask);
    AddPixelChannel16(layout, desc.AlphaMask);
    return layout;
}

static XDWORD ExtractMaskedComponent(XWORD pixel, const PixelChannel16 &channel) {
    return (pixel & channel.Mask) >> channel.Shift;
}

static XDWORD PackMaskedComponent(XDWORD value, const PixelChannel16 &channel) {
    return (value << channel.Shift) & channel.Mask;
}

static XWORD AveragePixel16(const PixelLayout16 &layout, XWORD p0, XWORD p1) {
    XWORD result = 0;
    for (int i = 0; i < layout.Count; ++i) {
        const PixelChannel16 &channel = layout.Channels[i];
        const XDWORD avg = (ExtractMaskedComponent(p0, channel) + ExtractMaskedComponent(p1, channel)) >> 1;
        result = (XWORD) (result | PackMaskedComponent(avg, channel));
    }

    return result;
}

static XWORD AveragePixel16(const PixelLayout16 &layout, XWORD p0, XWORD p1, XWORD p2, XWORD p3) {
    XWORD result = 0;
    for (int i = 0; i < layout.Count; ++i) {
        const PixelChannel16 &channel = layout.Channels[i];
        const XDWORD avg = (
            ExtractMaskedComponent(p0, channel) +
            ExtractMaskedComponent(p1, channel) +
            ExtractMaskedComponent(p2, channel) +
            ExtractMaskedComponent(p3, channel)
        ) >> 2;
        result = (XWORD) (result | PackMaskedComponent(avg, channel));
    }

    return result;
}

static XBOOL IsRgb565Layout(const VxImageDescEx &desc) {
    return desc.BitsPerPixel == 16 &&
        desc.RedMask == 0xF800 &&
        desc.GreenMask == 0x07E0 &&
        desc.BlueMask == 0x001F &&
        desc.AlphaMask == 0;
}

static void GenerateMipMap16(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    const int height = src_desc.Height;
    const int bytesPerLine = src_desc.BytesPerLine;
    XBYTE *image = src_desc.Image;

    const int dstWidth = src_desc.Width >> 1;
    const int dstHeight = height >> 1;
    const PixelLayout16 layout = BuildPixelLayout16(src_desc);

    if (dstWidth == 0) {
        if (dstHeight) {
            XWORD *dst = (XWORD *) Buffer;
            XBYTE *src = image;
            for (int y = 0; y < dstHeight; ++y) {
                *dst++ = AveragePixel16(layout, *(XWORD *) src, *(XWORD *) (src + bytesPerLine));
                src += bytesPerLine * 2;
            }
        }
        return;
    }

    if (dstHeight == 0) {
        XWORD *dst = (XWORD *) Buffer;
        XWORD *src = (XWORD *) image;

        int x = 0;
        for (; x < dstWidth; ++x) {
            XWORD *pixel = src + x * 2;
            dst[x] = AveragePixel16(layout, pixel[0], pixel[1]);
        }
        return;
    }

    XWORD *dst = (XWORD *) Buffer;
    for (int y = 0; y < dstHeight; ++y) {
        XBYTE *row0 = image + y * 2 * bytesPerLine;
        XBYTE *row1 = row0 + bytesPerLine;

        int x = 0;
        for (; x < dstWidth; ++x) {
            XWORD *p0 = (XWORD *) (row0 + x * 4);
            XWORD *p2 = (XWORD *) (row1 + x * 4);
            dst[x] = AveragePixel16(layout, p0[0], p0[1], p2[0], p2[1]);
        }
        dst += dstWidth;
    }
}

static XBOOL GenerateMipMap32Scalar(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    int Height = src_desc.Height;
    int BytesPerLine = src_desc.BytesPerLine;
    XBYTE *Image = src_desc.Image;

    int dstWidth = src_desc.Width >> 1;
    int dstHeight = Height >> 1;

    if (dstWidth == 0) {
        // Width is 0, only vertical averaging
        if (dstHeight) {
            XBYTE *dstPtr = Buffer;
            XBYTE *srcPtr = Image;
            int h = dstHeight;
            while (h > 0) {
                XDWORD p0 = *(XDWORD *) srcPtr;
                XDWORD p1 = *(XDWORD *) (srcPtr + BytesPerLine);

                XDWORD rb0 = p0 & 0x00FF00FF;
                XDWORD rb1 = p1 & 0x00FF00FF;
                XDWORD ag0 = (p0 & 0xFF00FF00) >> 8;
                XDWORD ag1 = (p1 & 0xFF00FF00) >> 8;

                XDWORD rbAvg = ((rb0 + rb1) >> 1) & 0x00FF00FF;
                XDWORD agAvg = ((ag0 + ag1) << 7) & 0xFF00FF00;

                *(XDWORD *) dstPtr = rbAvg | agAvg;

                dstPtr += BytesPerLine;
                srcPtr += BytesPerLine * 2;
                --h;
            }
        }
        return TRUE;
    }

    if (dstHeight == 0) {
        // Height is 0, only horizontal averaging
        XBYTE *dstPtr = Buffer;
        XDWORD *srcPtr = (XDWORD *) Image;

        int w = dstWidth;
        do {
            XDWORD p0 = srcPtr[0];
            XDWORD p1 = srcPtr[1];

            XDWORD rb0 = p0 & 0x00FF00FF;
            XDWORD rb1 = p1 & 0x00FF00FF;
            XDWORD ag0 = (p0 & 0xFF00FF00) >> 8;
            XDWORD ag1 = (p1 & 0xFF00FF00) >> 8;

            XDWORD rbAvg = ((rb0 + rb1) >> 1) & 0x00FF00FF;
            XDWORD agAvg = ((ag0 + ag1) << 7) & 0xFF00FF00;

            *(XDWORD *) dstPtr = rbAvg | agAvg;
            srcPtr += 2;
            dstPtr += 4;
            --w;
        } while (w);
        return TRUE;
    }

    // Both width and height > 0, 2D averaging
    XBYTE *dstPtr = Buffer;
    XDWORD *srcPtr = (XDWORD *) Image;

    int h = dstHeight;
    do {
        XDWORD *rowStart = srcPtr;
        int w = dstWidth;
        do {
            XDWORD p0 = srcPtr[0];
            XDWORD p1 = srcPtr[1];
            XDWORD p2 = *(XDWORD *) ((XBYTE *) srcPtr + BytesPerLine);
            XDWORD p3 = *(XDWORD *) ((XBYTE *) srcPtr + BytesPerLine + 4);

            XDWORD rb = (p0 & 0x00FF00FF) + (p1 & 0x00FF00FF) +
                (p2 & 0x00FF00FF) + (p3 & 0x00FF00FF);
            XDWORD ag = ((p0 & 0xFF00FF00) >> 8) + ((p1 & 0xFF00FF00) >> 8) +
                ((p2 & 0xFF00FF00) >> 8) + ((p3 & 0xFF00FF00) >> 8);

            XDWORD rbAvg = (rb >> 2) & 0x00FF00FF;
            XDWORD agAvg = ((ag >> 2) << 8) & 0xFF00FF00;

            *(XDWORD *) dstPtr = rbAvg | agAvg;
            srcPtr += 2;
            dstPtr += 4;
            --w;
        } while (w);
        srcPtr = (XDWORD *) ((XBYTE *) rowStart + BytesPerLine * 2);
        --h;
    } while (h);
    return TRUE;
}

typedef XBOOL (*VxImageMipMapKernelFn)(const VxImageDescEx &, XBYTE *);
typedef XBOOL (*VxImageNormalKernelFn)(const VxImageDescEx &, XDWORD);
typedef XBOOL (*VxImageBumpKernelFn)(const VxImageDescEx &);

struct VxImageKernelBackend {
    VxImageMipMapKernelFn MipMap32;
    VxImageMipMapKernelFn MipMap24;
    VxImageMipMapKernelFn MipMapRgb565;
    VxImageNormalKernelFn Normal32;
    VxImageNormalKernelFn Normal24;
    VxImageBumpKernelFn Bump32;
    VxImageBumpKernelFn Bump24;
};

static XBOOL ConvertToBumpMap32(const VxImageDescEx &image);
static XBOOL ConvertToBumpMap24ScalarKernel(const VxImageDescEx &image);
static XBOOL ConvertToBumpMap24CachedKernel(const VxImageDescEx &image);
static XBOOL ConvertToBumpMap32ScalarKernel(const VxImageDescEx &image);
static XBOOL ConvertToNormalMap32ScalarKernel(const VxImageDescEx &image, XDWORD ColorMask);
static XBOOL ConvertToNormalMap24ScalarKernel(const VxImageDescEx &image, XDWORD ColorMask);
static XBOOL ConvertToNormalMap32CachedSSE2Kernel(const VxImageDescEx &image, XDWORD ColorMask);
static XBOOL ConvertToNormalMap24CachedSSSE3Kernel(const VxImageDescEx &image, XDWORD ColorMask);

static XBOOL GenerateMipMap24ScalarKernel(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    if (src_desc.BitsPerPixel != 24) return FALSE;
    GenerateMipMap24(src_desc, Buffer);
    return TRUE;
}

static XBOOL GenerateMipMap16Rgb565ScalarKernel(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    if (!IsRgb565Layout(src_desc)) return FALSE;
    GenerateMipMap16(src_desc, Buffer);
    return TRUE;
}

static const VxImageKernelBackend kVxImageBackendScalar = {
    GenerateMipMap32Scalar,
    GenerateMipMap24ScalarKernel,
    GenerateMipMap16Rgb565ScalarKernel,
    ConvertToNormalMap32ScalarKernel,
    ConvertToNormalMap24ScalarKernel,
    ConvertToBumpMap32ScalarKernel,
    ConvertToBumpMap24ScalarKernel
};

static const VxImageKernelBackend kVxImageBackendSSE2 = {
    VxGenerateMipMap32SSE2,
    GenerateMipMap24ScalarKernel,
    VxGenerateMipMap16Rgb565SSE2,
    ConvertToNormalMap32CachedSSE2Kernel,
    ConvertToNormalMap24ScalarKernel,
    ConvertToBumpMap32ScalarKernel,
    ConvertToBumpMap24CachedKernel
};

static const VxImageKernelBackend kVxImageBackendSSSE3 = {
    VxGenerateMipMap32SSE2,
    VxGenerateMipMap24Rgb888SSSE3,
    VxGenerateMipMap16Rgb565SSE2,
    ConvertToNormalMap32CachedSSE2Kernel,
    ConvertToNormalMap24CachedSSSE3Kernel,
    ConvertToBumpMap32ScalarKernel,
    ConvertToBumpMap24CachedKernel
};

static const VxImageKernelBackend kVxImageBackendAVX2 = {
    VxGenerateMipMap32SSE2,
    VxGenerateMipMap24Rgb888SSSE3,
    VxGenerateMipMap16Rgb565SSE2,
    ConvertToNormalMap32CachedSSE2Kernel,
    ConvertToNormalMap24CachedSSSE3Kernel,
    ConvertToBumpMap32ScalarKernel,
    ConvertToBumpMap24CachedKernel
};

static const VxImageKernelBackend *GetVxImageBackend(int simdMode) {
    if (simdMode == VX_SIMD_MODE_AVX2) return &kVxImageBackendAVX2;
    if (simdMode == VX_SIMD_MODE_SSSE3 ||
        simdMode == VX_SIMD_MODE_SSE4_1 ||
        simdMode == VX_SIMD_MODE_AVX) {
        return &kVxImageBackendSSSE3;
    }
    if (simdMode == VX_SIMD_MODE_SSE2) return &kVxImageBackendSSE2;
    return &kVxImageBackendScalar;
}

void VxGenerateMipMapKernel(const VxImageDescEx &src_desc, XBYTE *Buffer, int simdMode) {
    const VxImageKernelBackend *backend = GetVxImageBackend(simdMode);

    if (src_desc.BitsPerPixel == 24) {
        if (backend->MipMap24 && backend->MipMap24(src_desc, Buffer)) return;
        GenerateMipMap24(src_desc, Buffer);
        return;
    }

    if (src_desc.BitsPerPixel == 16) {
        if (IsRgb565Layout(src_desc) && backend->MipMapRgb565 && backend->MipMapRgb565(src_desc, Buffer)) return;
        GenerateMipMap16(src_desc, Buffer);
        return;
    }

    if (backend->MipMap32 && backend->MipMap32(src_desc, Buffer)) return;
    GenerateMipMap32Scalar(src_desc, Buffer);
}


//------------------------------------------------------------------------------
// Normal and bump map generation
//------------------------------------------------------------------------------

// Helper function to pack normal vector into ARGB pixel
// Original formula: ((normal * 127.0 + 128.0) for each component, with alpha derived from height
static XDWORD PackNormalToPixel(const float *normal, float height) {
    // normal[0] = nx, normal[1] = ny, normal[2] = nz
    int nx_color = (int) (normal[0] * 127.0f + 128.0f);
    int ny_color = (int) (normal[1] * 127.0f + 128.0f);
    int nz_color = (int) (normal[2] * 127.0f + 128.0f);
    int alpha = (int) (height * -255.0f);

    // Pack as ARGB: (alpha << 24) | (red << 16) | (green << 8) | blue
    // Original packs: ((nx - alpha<<8) << 8 + ny) << 8 + nz
    return (XDWORD) (((((nx_color - (alpha << 8)) << 8) + ny_color) << 8) + nz_color);
}

static XDWORD ReadPixel24(const XBYTE *pixel) {
    return (XDWORD) pixel[0] | ((XDWORD) pixel[1] << 8) | ((XDWORD) pixel[2] << 16);
}

static int Luminance24(const XBYTE *pixel) {
    const XDWORD value = ReadPixel24(pixel);
    return (int) ((value & 0xFF) + ((value >> 8) & 0xFF) + ((value >> 16) & 0xFF));
}

static void FillLuminance24Row(const XBYTE *row, int width, int *luminance) {
    for (int x = 0; x < width; ++x) {
        luminance[x] = Luminance24(row + x * 3);
    }
}

static void WriteNormalPixel24(XBYTE *pixel, XDWORD packedNormal) {
    pixel[0] = (XBYTE) (packedNormal & 0xFF);
    pixel[1] = (XBYTE) ((packedNormal >> 8) & 0xFF);
    pixel[2] = (XBYTE) ((packedNormal >> 16) & 0xFF);
}

static XBOOL ConvertToNormalMapScalar(const VxImageDescEx &image, XDWORD ColorMask) {
    if (image.BitsPerPixel != 32 && image.BitsPerPixel != 24) return FALSE;
    if (image.Width == 0 || image.Height == 0) return FALSE;
    if (image.Image == nullptr) return FALSE;

    XBYTE *Image = image.Image;
    int Width = image.Width;
    int Height = image.Height;

    if (image.BitsPerPixel == 24) {
        if (ColorMask == 0xFFFFFFFF) {
            const float scale = 0.0013071896f; // 1.0 / 765.0 (255*3)

            for (int y = 0; y < Height - 1; y++) {
                for (int x = 0; x < Width; x++) {
                    XBYTE *pCurrent = Image + y * image.BytesPerLine + x * 3;
                    XBYTE *pRight = (x + 1 < Width) ? pCurrent + 3 : pCurrent;
                    XBYTE *pBelow = Image + (y + 1) * image.BytesPerLine + x * 3;

                    XDWORD p0 = ReadPixel24(pCurrent);
                    XDWORD p1 = ReadPixel24(pRight);
                    XDWORD p2 = ReadPixel24(pBelow);
                    float lum0 = ((p0 & 0xFF) + ((p0 >> 8) & 0xFF) + ((p0 >> 16) & 0xFF)) * scale;
                    float lum1 = ((p1 & 0xFF) + ((p1 >> 8) & 0xFF) + ((p1 >> 16) & 0xFF)) * scale;
                    float lum2 = ((p2 & 0xFF) + ((p2 >> 8) & 0xFF) + ((p2 >> 16) & 0xFF)) * scale;

                    float dx = lum1 - lum0;
                    float dy = lum2 - lum0;
                    float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f);
                    float normal[3] = {dx * invLen, dy * invLen, invLen};

                    WriteNormalPixel24(pCurrent, PackNormalToPixel(normal, lum0));
                }
            }
        } else {
            XBYTE BitShift = (XBYTE) GetBitShift(ColorMask);
            const float scale = 1.0f / 255.0f;

            for (int y = 0; y < Height - 1; y++) {
                for (int x = 0; x < Width; x++) {
                    XBYTE *pCurrent = Image + y * image.BytesPerLine + x * 3;
                    XBYTE *pRight = (x + 1 < Width) ? pCurrent + 3 : pCurrent;
                    XBYTE *pBelow = Image + (y + 1) * image.BytesPerLine + x * 3;

                    XDWORD v0 = (ReadPixel24(pCurrent) & ColorMask) >> BitShift;
                    XDWORD v1 = (ReadPixel24(pRight) & ColorMask) >> BitShift;
                    XDWORD v2 = (ReadPixel24(pBelow) & ColorMask) >> BitShift;
                    float h0 = (float) v0 * scale;
                    float h1 = (float) v1 * scale;
                    float h2 = (float) v2 * scale;

                    float dx = h1 - h0;
                    float dy = h2 - h0;
                    float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f);
                    float normal[3] = {dx * invLen, dy * invLen, invLen};

                    WriteNormalPixel24(pCurrent, PackNormalToPixel(normal, h0));
                }
            }
        }

        if (Height > 1) {
            memcpy(
                Image + (Height - 1) * image.BytesPerLine,
                Image + (Height - 2) * image.BytesPerLine,
                image.BytesPerLine
            );
        }

        return TRUE;
    }

    if (ColorMask == 0xFFFFFFFF) {
        // Use luminance from RGB
        const float scale = 0.0013071896f; // 1.0 / 765.0 (255*3)

        {
            // Scalar fallback
            for (int y = 0; y < Height - 1; y++) {
                for (int x = 0; x < Width; x++) {
                    XDWORD *pCurrent = (XDWORD *) (Image + y * image.BytesPerLine + x * 4);
                    XDWORD *pRight = (XDWORD *) (Image + y * image.BytesPerLine + (x + 1) * 4);
                    XDWORD *pBelow = (XDWORD *) (Image + (y + 1) * image.BytesPerLine + x * 4);

                    // Calculate luminance for current pixel (R + G + B)
                    XDWORD p0 = *pCurrent;
                    float lum0 = ((p0 & 0xFF) + ((p0 >> 8) & 0xFF) + ((p0 >> 16) & 0xFF)) * scale;

                    // Calculate luminance for right pixel
                    XDWORD p1 = (x + 1 < Width) ? *pRight : p0;
                    float lum1 = ((p1 & 0xFF) + ((p1 >> 8) & 0xFF) + ((p1 >> 16) & 0xFF)) * scale;

                    // Calculate luminance for below pixel
                    XDWORD p2 = *pBelow;
                    float lum2 = ((p2 & 0xFF) + ((p2 >> 8) & 0xFF) + ((p2 >> 16) & 0xFF)) * scale;

                    // Calculate derivatives
                    float dx = lum1 - lum0;
                    float dy = lum2 - lum0;

                    // Calculate normal vector and normalize
                    float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f);
                    float normal[3];
                    normal[0] = dx * invLen;
                    normal[1] = dy * invLen;
                    normal[2] = invLen;

                    *pCurrent = PackNormalToPixel(normal, lum0);
                }
            }
        }
    } else {
        // Use specified color mask
        XBYTE BitShift = (XBYTE) GetBitShift(ColorMask);
        const float scale = 1.0f / 255.0f;

        for (int y = 0; y < Height - 1; y++) {
            for (int x = 0; x < Width; x++) {
                XDWORD *pCurrent = (XDWORD *) (Image + y * image.BytesPerLine + x * 4);
                XDWORD *pRight = (XDWORD *) (Image + y * image.BytesPerLine + (x + 1) * 4);
                XDWORD *pBelow = (XDWORD *) (Image + (y + 1) * image.BytesPerLine + x * 4);

                // Extract masked value for current pixel
                XDWORD v0 = ((*pCurrent) & ColorMask) >> BitShift;
                float h0 = (float) v0 * scale;

                // Extract masked value for right pixel
                XDWORD v1 = (x + 1 < Width) ? (((*pRight) & ColorMask) >> BitShift) : v0;
                float h1 = (float) v1 * scale;

                // Extract masked value for below pixel
                XDWORD v2 = ((*pBelow) & ColorMask) >> BitShift;
                float h2 = (float) v2 * scale;

                // Calculate derivatives
                float dx = h1 - h0;
                float dy = h2 - h0;

                // Calculate normal vector and normalize
                float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f);
                float normal[3];
                normal[0] = dx * invLen;
                normal[1] = dy * invLen;
                normal[2] = invLen;

                *pCurrent = PackNormalToPixel(normal, h0);
            }
        }
    }

    // Copy second-to-last row to last row.
    // For single-row images there is no previous row, so keep row 0 as-is.
    if (Height > 1) {
        memcpy(
            Image + (Height - 1) * image.BytesPerLine,
            Image + (Height - 2) * image.BytesPerLine,
            image.BytesPerLine
        );
    }

    return TRUE;
}

static int Luminance32(const XBYTE *pixel) {
    const XDWORD value = *(const XDWORD *) pixel;
    return (int) ((value & 0xFF) + ((value >> 8) & 0xFF) + ((value >> 16) & 0xFF));
}

static void FillLuminance32Row(const XBYTE *row, int width, int *luminance) {
    for (int x = 0; x < width; ++x) {
        luminance[x] = Luminance32(row + x * 4);
    }
}

typedef XBOOL (*VxFillLuminanceKernelFn)(const XBYTE *, int, int *);

static void FillNormalLuminanceRow(
    const XBYTE *row,
    int width,
    int bytesPerPixel,
    int *luminance,
    VxFillLuminanceKernelFn fillLuminance
) {
    if (fillLuminance && fillLuminance(row, width, luminance)) return;
    if (bytesPerPixel == 4) {
        FillLuminance32Row(row, width, luminance);
    } else {
        FillLuminance24Row(row, width, luminance);
    }
}

static void ConvertNormalLuminanceRows(
    XBYTE *dstRow,
    int width,
    int bytesPerPixel,
    const int *currentLum,
    const int *belowLum
) {
    const float scale = 0.0013071896f; // 1.0 / 765.0 (255*3)

    for (int x = 0; x < width; ++x) {
        const int rightIndex = (x + 1 < width) ? (x + 1) : x;
        const float lum0 = (float) currentLum[x] * scale;
        const float lum1 = (float) currentLum[rightIndex] * scale;
        const float lum2 = (float) belowLum[x] * scale;
        const float dx = lum1 - lum0;
        const float dy = lum2 - lum0;
        const float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f);
        const float normal[3] = {dx * invLen, dy * invLen, invLen};
        const XDWORD packed = PackNormalToPixel(normal, lum0);
        XBYTE *dst = dstRow + x * bytesPerPixel;
        if (bytesPerPixel == 4) {
            *(XDWORD *) dst = packed;
        } else {
            WriteNormalPixel24(dst, packed);
        }
    }
}

static XBOOL ConvertToNormalMapLuminanceCached(
    const VxImageDescEx &image,
    int bytesPerPixel,
    VxFillLuminanceKernelFn fillLuminance
) {
    if (image.Image == nullptr) return FALSE;
    if (image.Width == 0 || image.Height == 0 || image.BytesPerLine <= 0) return FALSE;

    if (image.Height == 1) {
        return TRUE;
    }

    XArray<int> rowA;
    XArray<int> rowB;
    rowA.Resize(image.Width);
    rowB.Resize(image.Width);

    int *currentLum = rowA.Begin();
    int *belowLum = rowB.Begin();
    FillNormalLuminanceRow(image.Image, image.Width, bytesPerPixel, currentLum, fillLuminance);
    FillNormalLuminanceRow(image.Image + image.BytesPerLine, image.Width, bytesPerPixel, belowLum, fillLuminance);

    for (int y = 0; y < image.Height - 1; ++y) {
        XBYTE *dstRow = image.Image + y * image.BytesPerLine;
        ConvertNormalLuminanceRows(dstRow, image.Width, bytesPerPixel, currentLum, belowLum);

        int *tmp = currentLum;
        currentLum = belowLum;
        belowLum = tmp;

        if (y + 2 < image.Height) {
            FillNormalLuminanceRow(
                image.Image + (y + 2) * image.BytesPerLine,
                image.Width,
                bytesPerPixel,
                belowLum,
                fillLuminance
            );
        }
    }

    memcpy(
        image.Image + (image.Height - 1) * image.BytesPerLine,
        image.Image + (image.Height - 2) * image.BytesPerLine,
        image.BytesPerLine
    );
    return TRUE;
}

static XBOOL ConvertToNormalMap32ScalarKernel(const VxImageDescEx &image, XDWORD ColorMask) {
    if (image.BitsPerPixel != 32) return FALSE;
    return ConvertToNormalMapScalar(image, ColorMask);
}

static XBOOL ConvertToNormalMap24ScalarKernel(const VxImageDescEx &image, XDWORD ColorMask) {
    if (image.BitsPerPixel != 24) return FALSE;
    return ConvertToNormalMapScalar(image, ColorMask);
}

static XBOOL ConvertToNormalMap32CachedSSE2Kernel(const VxImageDescEx &image, XDWORD ColorMask) {
    if (image.BitsPerPixel != 32 || ColorMask != 0xFFFFFFFFu) return FALSE;
    return ConvertToNormalMapLuminanceCached(image, 4, VxFillLuminance32SSE2);
}

static XBOOL ConvertToNormalMap24CachedSSSE3Kernel(const VxImageDescEx &image, XDWORD ColorMask) {
    if (image.BitsPerPixel != 24 || ColorMask != 0xFFFFFFFFu) return FALSE;
    return ConvertToNormalMapLuminanceCached(image, 3, VxFillLuminance24SSSE3);
}

XBOOL VxConvertToNormalMapKernel(const VxImageDescEx &image, XDWORD ColorMask, int simdMode) {
    const VxImageKernelBackend *backend = GetVxImageBackend(simdMode);

    if (image.BitsPerPixel == 32) {
        if (backend->Normal32 && backend->Normal32(image, ColorMask)) return TRUE;
        return ConvertToNormalMapScalar(image, ColorMask);
    }

    if (image.BitsPerPixel == 24) {
        if (backend->Normal24 && backend->Normal24(image, ColorMask)) return TRUE;
        return ConvertToNormalMapScalar(image, ColorMask);
    }

    return FALSE;
}


static XBOOL ConvertToBumpMap24Cached(const VxImageDescEx &image) {
    if (image.Image == nullptr) return FALSE;
    if (image.Width < 2 || image.Height <= 0 || image.BytesPerLine <= 0) return FALSE;

    const size_t bytesPerLine = static_cast<size_t>(image.BytesPerLine);
    const size_t imageHeight = static_cast<size_t>(image.Height);
    if (bytesPerLine > (static_cast<size_t>(INT_MAX) / imageHeight)) {
        return FALSE;
    }

    const size_t imageSize = bytesPerLine * imageHeight;
    XArray<XBYTE> tempImage;
    tempImage.Resize(static_cast<int>(imageSize));
    memcpy(tempImage.Begin(), image.Image, imageSize);

    const int Width = image.Width;
    const int Height = image.Height;
    const int BytesPerLine = image.BytesPerLine;

    XArray<int> aboveLum;
    XArray<int> currentLum;
    XArray<int> belowLum;
    aboveLum.Resize(Width);
    currentLum.Resize(Width);
    belowLum.Resize(Width);

    XBYTE *dstRow = image.Image;
    XBYTE *lastRow = tempImage.Begin() + BytesPerLine * (Height - 1);
    for (int y = 0; y < Height; ++y) {
        XBYTE *srcRow = tempImage.Begin() + y * BytesPerLine;
        XBYTE *aboveRow = (y == 0) ? lastRow : srcRow - BytesPerLine;
        XBYTE *belowRow = (y != Height - 1) ? srcRow + BytesPerLine : srcRow;

        FillLuminance24Row(aboveRow, Width, aboveLum.Begin());
        FillLuminance24Row(srcRow, Width, currentLum.Begin());
        FillLuminance24Row(belowRow, Width, belowLum.Begin());

        dstRow[0] = (XBYTE) ((currentLum[0] <= 1) ? 127 : 63);
        dstRow[1] = (XBYTE) (aboveLum[0] - belowLum[0] + currentLum[Width - 1] - belowLum[1]);
        dstRow[2] = (XBYTE) (currentLum[Width - 1] - currentLum[0]);

        for (int x = 1; x < Width - 1; ++x) {
            XBYTE *dst = dstRow + x * 3;
            dst[0] = (XBYTE) ((currentLum[x] <= 1) ? 127 : 63);
            dst[1] = (XBYTE) (aboveLum[x] - belowLum[x]);
            dst[2] = (XBYTE) (currentLum[x - 1] - currentLum[x + 1]);
        }

        XBYTE *dst = dstRow + (Width - 1) * 3;
        dst[0] = (XBYTE) ((currentLum[Width - 1] <= 1) ? 127 : 63);
        dst[1] = (XBYTE) (aboveLum[Width - 1] - belowLum[Width - 1]);
        dst[2] = (XBYTE) (currentLum[Width - 2] - currentLum[0]);

        dstRow += BytesPerLine;
    }

    return TRUE;
}

static XBOOL ConvertToBumpMap24(const VxImageDescEx &image) {
    if (image.Image == nullptr) return FALSE;
    if (image.Width < 2 || image.Height <= 0 || image.BytesPerLine <= 0) return FALSE;

    const size_t bytesPerLine = static_cast<size_t>(image.BytesPerLine);
    const size_t imageHeight = static_cast<size_t>(image.Height);
    if (bytesPerLine > (static_cast<size_t>(INT_MAX) / imageHeight)) {
        return FALSE;
    }

    const size_t imageSize = bytesPerLine * imageHeight;
    XArray<XBYTE> tempImage;
    tempImage.Resize(static_cast<int>(imageSize));
    memcpy(tempImage.Begin(), image.Image, imageSize);

    const int Width = image.Width;
    const int Height = image.Height;
    const int BytesPerLine = image.BytesPerLine;
    XBYTE *srcPtr = tempImage.Begin();
    XBYTE *dstPtr = image.Image;
    XBYTE *lastRowPtr = tempImage.Begin() + BytesPerLine * (Height - 1);

    for (int y = 0; y < Height; ++y) {
        XBYTE *abovePtr = (y == 0) ? lastRowPtr : srcPtr - BytesPerLine;
        XBYTE *belowPtr = (y != Height - 1) ? srcPtr + BytesPerLine : srcPtr;

        int leftLum = Luminance24(srcPtr + (Width - 1) * 3);
        int belowLum = Luminance24(belowPtr);
        int aboveLum = Luminance24(abovePtr);
        int currLum = Luminance24(srcPtr);
        int rightLum = Luminance24(belowPtr + 3);

        dstPtr[0] = (XBYTE) ((currLum <= 1) ? 127 : 63);
        dstPtr[1] = (XBYTE) (aboveLum - belowLum + leftLum - rightLum);
        dstPtr[2] = (XBYTE) (leftLum - currLum);

        XBYTE *currentPtr = srcPtr + 3;
        XBYTE *currentDst = dstPtr + 3;
        abovePtr += 3;
        belowPtr += 3;

        for (int x = 1; x < Width - 1; ++x) {
            leftLum = Luminance24(currentPtr - 3);
            rightLum = Luminance24(currentPtr + 3);
            currLum = Luminance24(currentPtr);
            belowLum = Luminance24(belowPtr);
            aboveLum = Luminance24(abovePtr);

            currentDst[0] = (XBYTE) ((currLum <= 1) ? 127 : 63);
            currentDst[1] = (XBYTE) (aboveLum - belowLum);
            currentDst[2] = (XBYTE) (leftLum - rightLum);

            currentPtr += 3;
            currentDst += 3;
            abovePtr += 3;
            belowPtr += 3;
        }

        leftLum = Luminance24(srcPtr + (Width - 2) * 3);
        rightLum = Luminance24(srcPtr);
        currLum = Luminance24(currentPtr);
        belowLum = Luminance24(belowPtr);
        aboveLum = Luminance24(abovePtr);

        currentDst[0] = (XBYTE) ((currLum <= 1) ? 127 : 63);
        currentDst[1] = (XBYTE) (aboveLum - belowLum);
        currentDst[2] = (XBYTE) (leftLum - rightLum);

        srcPtr += BytesPerLine;
        dstPtr += BytesPerLine;
    }

    return TRUE;
}

static XBOOL ConvertToBumpMap24ScalarKernel(const VxImageDescEx &image) {
    if (image.BitsPerPixel != 24) return FALSE;
    return ConvertToBumpMap24(image);
}

static XBOOL ConvertToBumpMap24CachedKernel(const VxImageDescEx &image) {
    if (image.BitsPerPixel != 24) return FALSE;
    return ConvertToBumpMap24Cached(image);
}

static XBOOL ConvertToBumpMap32ScalarKernel(const VxImageDescEx &image) {
    if (image.BitsPerPixel != 32) return FALSE;
    return ConvertToBumpMap32(image);
}

static XBOOL ConvertToBumpMap32(const VxImageDescEx &image) {
    if (image.BitsPerPixel != 32) return FALSE;
    if (image.Image == nullptr) return FALSE;
    if (image.Width < 2 || image.Height <= 0 || image.BytesPerLine <= 0) return FALSE;

    // Allocate temporary copy of the image
    const size_t bytesPerLine = static_cast<size_t>(image.BytesPerLine);
    const size_t imageHeight = static_cast<size_t>(image.Height);
    if (bytesPerLine > (static_cast<size_t>(-1) / imageHeight)) {
        return FALSE;
    }

    if (bytesPerLine > (static_cast<size_t>(INT_MAX) / imageHeight)) {
        return FALSE;
    }

    const size_t imageSize = bytesPerLine * imageHeight;
    XArray<XBYTE> tempImage;
    tempImage.Resize(static_cast<int>(imageSize));
    memcpy(tempImage.Begin(), image.Image, imageSize);

    XBYTE *srcPtr = tempImage.Begin();
    XBYTE *dstPtr = image.Image;
    int Width = image.Width;
    int Height = image.Height;
    int BytesPerLine = image.BytesPerLine;

    // Calculate pointer to last row of source (for wrap-around)
    XBYTE *lastRowPtr = tempImage.Begin() + BytesPerLine * (Height - 1);

    for (int y = 0; y < Height; y++) {
        // Get pointer to row above (wrap to last row if at top)
        XBYTE *abovePtr;
        if (y == 0) {
            abovePtr = lastRowPtr;
        } else {
            abovePtr = srcPtr - BytesPerLine;
        }

        // Get pointer to row below (use current row if at bottom)
        XBYTE *belowPtr;
        if (y != Height - 1) {
            belowPtr = srcPtr + BytesPerLine;
        } else {
            belowPtr = srcPtr; // At last row, reuse current
        }

        XBYTE *currentPtr = srcPtr + 4; // Point to second pixel in row

        // Calculate left pixel luminance (last pixel of row for wrap)
        int leftPixel = *(int *) (srcPtr + (Width - 1) * 4);
        int leftLum = (leftPixel & 0xFF) + ((leftPixel >> 8) & 0xFF) + ((leftPixel >> 16) & 0xFF);

        // Calculate below-left luminance for first pixel
        int belowVal = *(int *) belowPtr;
        int belowLum = (belowVal & 0xFF) + ((belowVal >> 8) & 0xFF) + ((belowVal >> 16) & 0xFF);

        // Calculate above luminance for first pixel
        int aboveVal = *(int *) abovePtr;
        int aboveLum = (aboveVal & 0xFF) + ((aboveVal >> 8) & 0xFF) + ((aboveVal >> 16) & 0xFF);

        // Calculate current pixel luminance
        int currVal = *(int *) srcPtr;
        int currLum = (currVal & 0xFF) + ((currVal >> 8) & 0xFF) + ((currVal >> 16) & 0xFF);

        // Calculate right pixel luminance
        int rightVal = *(int *) (belowPtr + 4);
        int rightLum = (rightVal & 0xFF) + ((rightVal >> 8) & 0xFF) + ((rightVal >> 16) & 0xFF);

        // First pixel of row
        *dstPtr = (currLum <= 1) ? 127 : 63;
        dstPtr[1] = (XBYTE) (aboveLum - belowLum + leftLum - rightLum);
        dstPtr[2] = (XBYTE) (leftLum - currLum);
        dstPtr[3] = 0;
        dstPtr += 4;

        belowPtr += 4;
        abovePtr += 4;

        {
            // Scalar middle pixels
            for (unsigned int x = 1; x < (unsigned int) (Width - 1); x++) {
                // Get values
                leftPixel = *(int *) (currentPtr - 4);
                leftLum = (leftPixel & 0xFF) + ((leftPixel >> 8) & 0xFF) + ((leftPixel >> 16) & 0xFF);

                int nextRightVal = *(int *) (currentPtr + 4);
                int nextRightLum = (nextRightVal & 0xFF) + ((nextRightVal >> 8) & 0xFF) + ((nextRightVal >> 16) & 0xFF);

                currVal = *(int *) currentPtr;
                currLum = (currVal & 0xFF) + ((currVal >> 8) & 0xFF) + ((currVal >> 16) & 0xFF);

                belowVal = *(int *) belowPtr;
                belowLum = (belowVal & 0xFF) + ((belowVal >> 8) & 0xFF) + ((belowVal >> 16) & 0xFF);

                aboveVal = *(int *) abovePtr;
                aboveLum = (aboveVal & 0xFF) + ((aboveVal >> 8) & 0xFF) + ((aboveVal >> 16) & 0xFF);

                *dstPtr = (currLum <= 1) ? 127 : 63;
                dstPtr[1] = (XBYTE) (aboveLum - belowLum);
                dstPtr[2] = (XBYTE) (leftLum - nextRightLum);
                dstPtr[3] = 0;

                currentPtr += 4;
                belowPtr += 4;
                abovePtr += 4;
                dstPtr += 4;
            }
        }

        // Last pixel of row
        leftPixel = *(int *) (srcPtr + (Width - 2) * 4);
        leftLum = (leftPixel & 0xFF) + ((leftPixel >> 8) & 0xFF) + ((leftPixel >> 16) & 0xFF);

        // Right pixel wraps to first pixel
        int firstVal = *(int *) srcPtr;
        int firstLum = (firstVal & 0xFF) + ((firstVal >> 8) & 0xFF) + ((firstVal >> 16) & 0xFF);

        currVal = *(int *) currentPtr;
        currLum = (currVal & 0xFF) + ((currVal >> 8) & 0xFF) + ((currVal >> 16) & 0xFF);

        belowVal = *(int *) belowPtr;
        belowLum = (belowVal & 0xFF) + ((belowVal >> 8) & 0xFF) + ((belowVal >> 16) & 0xFF);

        aboveVal = *(int *) abovePtr;
        aboveLum = (aboveVal & 0xFF) + ((aboveVal >> 8) & 0xFF) + ((aboveVal >> 16) & 0xFF);

        *dstPtr = (currLum <= 1) ? 127 : 63;
        dstPtr[1] = (XBYTE) (aboveLum - belowLum);
        dstPtr[2] = (XBYTE) (leftLum - firstLum);
        dstPtr[3] = 0;
        dstPtr += 4;

        srcPtr += BytesPerLine;
    }

    return TRUE;
}

XBOOL VxConvertToBumpMapKernel(const VxImageDescEx &image, int simdMode) {
    const VxImageKernelBackend *backend = GetVxImageBackend(simdMode);

    if (image.BitsPerPixel == 24) {
        if (backend->Bump24 && backend->Bump24(image)) return TRUE;
        return ConvertToBumpMap24(image);
    }

    if (image.BitsPerPixel == 32) {
        if (backend->Bump32 && backend->Bump32(image)) return TRUE;
        return ConvertToBumpMap32(image);
    }

    return FALSE;
}


