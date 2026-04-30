/**
 * @file VxBlitEngineClass.cpp
 * @brief VxBlitEngine class implementation -- dispatch table setup, high-level
 *        image operations (blit, resize, quantize, alpha, fill, etc.).
 *
 * All per-pixel kernel work is delegated to:
 *   - VxBlitKernels.cpp     (scalar / C++)
 *   - VxBlitEngineSSE2.cpp  (SSE2)
 *   - VxBlitEngineSSSE3.cpp (SSSE3 -- runtime-dispatched)
 *   - VxBlitEngineAVX2.cpp  (AVX2 -- runtime-dispatched)
 *
 * This file owns the global `TheBlitter` instance.
 */

#include "VxBlitInternal.h"

#include "VxMath.h"
#include "VxSIMD.h"
#include "NeuQuant.h"

int GetQuantizationSamplingFactor();

#if defined(VX_SIMD_SSE2)
#include <emmintrin.h>
#endif

//------------------------------------------------------------------------------
// Global Blitter Instance
//------------------------------------------------------------------------------

VxBlitEngine TheBlitter;

namespace {

int CollapseSIMDModeToBlitKernelTier(int mode) {
    switch (mode) {
        case VX_SIMD_MODE_AVX2:
            return VX_SIMD_MODE_AVX2;
        case VX_SIMD_MODE_AVX:
        case VX_SIMD_MODE_SSE4_1:
        case VX_SIMD_MODE_SSSE3:
            return VX_SIMD_MODE_SSSE3;
        case VX_SIMD_MODE_SSE2:
            return VX_SIMD_MODE_SSE2;
        case VX_SIMD_MODE_NONE:
        default:
            return VX_SIMD_MODE_NONE;
    }
}

} // namespace

//==============================================================================
//  VxBlitEngine -- Construction / Destruction
//==============================================================================

VxBlitEngine::VxBlitEngine() {
    static_assert(FORMAT_TABLE_SIZE >= NUM_BLITTABLE_FORMATS,
                  "FORMAT_TABLE_SIZE must cover all blittable pixel format indices");
    memset(&m_BlitInfo, 0, sizeof(m_BlitInfo));
    m_EffectiveBlitKernelMode = CollapseSIMDModeToBlitKernelTier(VxGetSIMDEffectiveBackend());
    m_OperationStamp = 1;
    RebuildTablesUnlocked();
}

VxBlitEngine::~VxBlitEngine() {
    // Nothing to free -- XArray handles its own memory.
}

//==============================================================================
//  Dispatch-Table Setup
//==============================================================================

void VxBlitEngine::BuildGenericTables() {
    // Generic blitting functions indexed by [srcBpp-1][dstBpp-1]
    m_GenericBlitTable[0][0] = CopyLineGeneric_8_8;
    m_GenericBlitTable[0][1] = CopyLineGeneric_8_16;
    m_GenericBlitTable[0][2] = CopyLineGeneric_8_24;
    m_GenericBlitTable[0][3] = CopyLineGeneric_8_32;

    m_GenericBlitTable[1][0] = CopyLineGeneric_16_8;
    m_GenericBlitTable[1][1] = CopyLineGeneric_16_16;
    m_GenericBlitTable[1][2] = CopyLineGeneric_16_24;
    m_GenericBlitTable[1][3] = CopyLineGeneric_16_32;

    m_GenericBlitTable[2][0] = CopyLineGeneric_24_8;
    m_GenericBlitTable[2][1] = CopyLineGeneric_24_16;
    m_GenericBlitTable[2][2] = CopyLineGeneric_24_24;
    m_GenericBlitTable[2][3] = CopyLineGeneric_24_32;

    m_GenericBlitTable[3][0] = CopyLineGeneric_32_8;
    m_GenericBlitTable[3][1] = CopyLineGeneric_32_16;
    m_GenericBlitTable[3][2] = CopyLineGeneric_32_24;
    m_GenericBlitTable[3][3] = CopyLineGeneric_32_32;

    // Alpha functions
    m_SetAlphaTable[0] = SetAlpha_8;
    m_SetAlphaTable[1] = SetAlpha_16;
    m_SetAlphaTable[2] = nullptr; // 24-bit rarely has alpha
    m_SetAlphaTable[3] = SetAlpha_32;

    m_CopyAlphaTable[0] = CopyAlpha_8;
    m_CopyAlphaTable[1] = CopyAlpha_16;
    m_CopyAlphaTable[2] = nullptr;
    m_CopyAlphaTable[3] = CopyAlpha_32;
}

void VxBlitEngine::BuildX86Table() {
    // Specific format conversion functions for common cases

    // 32-bit ARGB (format 1) to other formats
    m_SpecificBlitTable[1][2] = CopyLine_32ARGB_32RGB;    // ARGB -> RGB (32)
    m_SpecificBlitTable[1][3] = CopyLine_32ARGB_24RGB;    // ARGB -> RGB (24)
    m_SpecificBlitTable[1][4] = CopyLine_32ARGB_565RGB;   // ARGB -> 565
    m_SpecificBlitTable[1][5] = CopyLine_32ARGB_555RGB;   // ARGB -> 555
    m_SpecificBlitTable[1][6] = CopyLine_32ARGB_1555ARGB; // ARGB -> 1555
    m_SpecificBlitTable[1][7] = CopyLine_32ARGB_4444ARGB; // ARGB -> 4444
    m_SpecificBlitTable[1][10] = CopyLine_32ARGB_32ABGR;  // ARGB -> ABGR
    m_SpecificBlitTable[1][11] = CopyLine_32ARGB_32RGBA;  // ARGB -> RGBA
    m_SpecificBlitTable[1][12] = CopyLine_32ARGB_32BGRA;  // ARGB -> BGRA

    // 32-bit RGB (format 2) to ARGB
    m_SpecificBlitTable[2][1] = CopyLine_32RGB_32ARGB;

    // 24-bit RGB (format 3) to 32-bit
    m_SpecificBlitTable[3][1] = CopyLine_24RGB_32ARGB;

    // 16-bit to 32-bit
    m_SpecificBlitTable[4][1] = CopyLine_565RGB_32ARGB;
    m_SpecificBlitTable[5][1] = CopyLine_555RGB_32ARGB;
    m_SpecificBlitTable[6][1] = CopyLine_1555ARGB_32ARGB;
    m_SpecificBlitTable[7][1] = CopyLine_4444ARGB_32ARGB;

    // 32-bit ABGR (format 10) to other formats
    m_SpecificBlitTable[10][1] = CopyLine_32ABGR_32ARGB;  // ABGR -> ARGB

    // 32-bit RGBA (format 11) to other formats
    m_SpecificBlitTable[11][1] = CopyLine_32RGBA_32ARGB;  // RGBA -> ARGB

    // 32-bit BGRA (format 12) to other formats
    m_SpecificBlitTable[12][1] = CopyLine_32BGRA_32ARGB;  // BGRA -> ARGB

    // Paletted image functions
    m_PalettedBlitTable[0][0] = CopyLine_Paletted8_8;      // 8-bit pal -> 8-bit
    m_PalettedBlitTable[0][1] = CopyLine_Paletted8_565RGB; // 8-bit pal -> 16-bit 565
    m_PalettedBlitTable[0][2] = CopyLine_Paletted8_24RGB;  // 8-bit pal -> 24-bit
    m_PalettedBlitTable[0][3] = CopyLine_Paletted8_32ARGB; // 8-bit pal -> 32-bit
}

void VxBlitEngine::ResetDispatchTables() {
    memset(m_GenericBlitTable, 0, sizeof(m_GenericBlitTable));
    memset(m_SpecificBlitTable, 0, sizeof(m_SpecificBlitTable));
    memset(m_SetAlphaTable, 0, sizeof(m_SetAlphaTable));
    memset(m_CopyAlphaTable, 0, sizeof(m_CopyAlphaTable));
    memset(m_PalettedBlitTable, 0, sizeof(m_PalettedBlitTable));
}

void VxBlitEngine::ResetHotPathKernels() {
    m_FillLine32 = nullptr;
    m_FillLine16 = nullptr;
    m_PremultiplyAlpha32 = PremultiplyAlpha_32ARGB;
    m_UnpremultiplyAlpha32 = UnpremultiplyAlpha_32ARGB;
    m_SwapRedBlue32 = CopyLine_32ARGB_32ABGR;
    m_ClearAlpha32 = nullptr;
    m_SetFullAlpha32 = nullptr;
    m_InvertColors32 = nullptr;
    m_Grayscale32 = nullptr;
    m_MultiplyBlend32 = nullptr;
}

XDWORD VxBlitEngine::NextOperationStamp() {
    ++m_OperationStamp;
    if (m_OperationStamp == 0) {
        m_OperationStamp = 1;
    }
    return m_OperationStamp;
}

//==============================================================================
//  SIMD Override Registration
//==============================================================================

void VxBlitEngine::ApplySSE2Overrides() {
#if defined(VX_SIMD_SSE2)
    if (m_EffectiveBlitKernelMode == VX_SIMD_MODE_NONE) {
        return;
    }

    // SSE2-optimized functions override scalar versions for better performance

    // 32-bit ARGB <-> RGB conversions
    m_SpecificBlitTable[1][2] = CopyLine_32ARGB_32RGB_SSE;    // ARGB -> RGB (32)
    m_SpecificBlitTable[2][1] = CopyLine_32RGB_32ARGB_SSE;    // RGB -> ARGB (32)

    // 32-bit to 16-bit conversions
    m_SpecificBlitTable[1][4] = CopyLine_32ARGB_565RGB_SSE;   // ARGB -> 565
    m_SpecificBlitTable[1][5] = CopyLine_32ARGB_555RGB_SSE;   // ARGB -> 555
    m_SpecificBlitTable[1][6] = CopyLine_32ARGB_1555ARGB_SSE; // ARGB -> 1555
    m_SpecificBlitTable[1][7] = CopyLine_32ARGB_4444ARGB_SSE; // ARGB -> 4444

    // 16-bit to 32-bit conversions
    m_SpecificBlitTable[4][1] = CopyLine_565RGB_32ARGB_SSE;   // 565 -> ARGB
    m_SpecificBlitTable[5][1] = CopyLine_555RGB_32ARGB_SSE;   // 555 -> ARGB
    m_SpecificBlitTable[6][1] = CopyLine_1555ARGB_32ARGB_SSE; // 1555 -> ARGB
    m_SpecificBlitTable[7][1] = CopyLine_4444ARGB_32ARGB_SSE; // 4444 -> ARGB

    // Alpha functions
    m_SetAlphaTable[3] = SetAlpha_32_SSE;
    m_CopyAlphaTable[3] = CopyAlpha_32_SSE;

    // Paletted with SSE optimization
    m_PalettedBlitTable[0][3] = CopyLine_Paletted8_32ARGB_SSE; // 8-bit pal -> 32-bit (SSE)

    m_FillLine32 = FillLine_32_SSE;
    m_FillLine16 = FillLine_16_SSE;
    // Keep scalar here: legacy SSE path is not bit-exact for all inputs.
    m_PremultiplyAlpha32 = PremultiplyAlpha_32ARGB;
    m_UnpremultiplyAlpha32 = UnpremultiplyAlpha_32ARGB_SSE;
    m_ClearAlpha32 = ClearAlpha_32_SSE;
    m_SetFullAlpha32 = SetFullAlpha_32_SSE;
    m_InvertColors32 = InvertColors_32_SSE;
    m_Grayscale32 = Grayscale_32_SSE;
    m_MultiplyBlend32 = MultiplyBlend_32_SSE;
#endif
}

void VxBlitEngine::ApplySSSE3Overrides() {
#if defined(VX_SIMD_SSE2)
    if (m_EffectiveBlitKernelMode != VX_SIMD_MODE_SSSE3 &&
        m_EffectiveBlitKernelMode != VX_SIMD_MODE_AVX2) {
        return;
    }
    if (!VxGetSIMDFeatures().SSSE3) {
        return;
    }

    // 32-bit to 24-bit and vice versa
    m_SpecificBlitTable[1][3] = CopyLine_32ARGB_24RGB_SSE;    // ARGB -> RGB (24)
    m_SpecificBlitTable[3][1] = CopyLine_24RGB_32ARGB_SSE;    // RGB (24) -> ARGB

    // 32-bit channel swap conversions
    m_SpecificBlitTable[1][10] = CopyLine_32ARGB_32ABGR_SSE;  // ARGB -> ABGR
    m_SpecificBlitTable[1][11] = CopyLine_32ARGB_32RGBA_SSE;  // ARGB -> RGBA
    m_SpecificBlitTable[1][12] = CopyLine_32ARGB_32BGRA_SSE;  // ARGB -> BGRA
    m_SpecificBlitTable[10][1] = CopyLine_32ABGR_32ARGB_SSE;  // ABGR -> ARGB
    m_SpecificBlitTable[11][1] = CopyLine_32RGBA_32ARGB_SSE;  // RGBA -> ARGB
    m_SpecificBlitTable[12][1] = CopyLine_32BGRA_32ARGB_SSE;  // BGRA -> ARGB

    m_SwapRedBlue32 = CopyLine_32ARGB_32ABGR_SSE;
#endif
}

void VxBlitEngine::ApplyAVX2Overrides() {
#if defined(VX_SIMD_AVX2)
    if (m_EffectiveBlitKernelMode != VX_SIMD_MODE_AVX2) {
        return;
    }

    // Hybrid fastest policy (measured): keep AVX2 only where it wins while
    // preserving bit-exact output against lower SIMD tiers.
    //
    // Leave these from earlier tiers:
    // - 32RGB -> 32ARGB: SSE2 outperforms current AVX2 on our benchmark matrix.
    // - 32ARGB <-> 24RGB: SSSE3 outperforms current AVX2 on our benchmark matrix.
    m_SpecificBlitTable[1][2] = CopyLine_32ARGB_32RGB_AVX2;
    m_SpecificBlitTable[1][10] = CopyLine_32ARGB_32ABGR_AVX2;
    m_SpecificBlitTable[10][1] = CopyLine_32ABGR_32ARGB_AVX2;
    m_SpecificBlitTable[1][11] = CopyLine_32ARGB_32RGBA_AVX2;
    m_SpecificBlitTable[11][1] = CopyLine_32RGBA_32ARGB_AVX2;
    m_SpecificBlitTable[1][12] = CopyLine_32ARGB_32BGRA_AVX2;
    m_SpecificBlitTable[12][1] = CopyLine_32BGRA_32ARGB_AVX2;
    m_SpecificBlitTable[1][4] = CopyLine_32ARGB_565RGB_AVX2;
    m_SpecificBlitTable[1][5] = CopyLine_32ARGB_555RGB_AVX2;
    m_SpecificBlitTable[1][6] = CopyLine_32ARGB_1555ARGB_AVX2;
    m_SpecificBlitTable[1][7] = CopyLine_32ARGB_4444ARGB_AVX2;
    m_SpecificBlitTable[4][1] = CopyLine_565RGB_32ARGB_AVX2;
    m_SpecificBlitTable[5][1] = CopyLine_555RGB_32ARGB_AVX2;
    m_SpecificBlitTable[6][1] = CopyLine_1555ARGB_32ARGB_AVX2;
    m_SpecificBlitTable[7][1] = CopyLine_4444ARGB_32ARGB_AVX2;
    m_SetAlphaTable[3] = SetAlpha_32_AVX2;
    m_CopyAlphaTable[3] = CopyAlpha_32_AVX2;
    m_PalettedBlitTable[0][3] = CopyLine_Paletted8_32ARGB_AVX2;

    m_FillLine32 = FillLine_32_AVX2;
    m_FillLine16 = FillLine_16_AVX2;
    m_PremultiplyAlpha32 = PremultiplyAlpha_32ARGB_AVX2;
    m_UnpremultiplyAlpha32 = UnpremultiplyAlpha_32ARGB_AVX2;
    m_SwapRedBlue32 = CopyLine_32ARGB_32ABGR_AVX2;
    m_ClearAlpha32 = ClearAlpha_32_AVX2;
    m_SetFullAlpha32 = SetFullAlpha_32_AVX2;
    m_InvertColors32 = InvertColors_32_AVX2;
    m_Grayscale32 = Grayscale_32_AVX2;
    m_MultiplyBlend32 = MultiplyBlend_32_AVX2;
#endif
}

void VxBlitEngine::RebuildTablesUnlocked() {
    ResetDispatchTables();

    BuildGenericTables();
    BuildX86Table();
    ResetHotPathKernels();

    ApplySSE2Overrides();
    ApplySSSE3Overrides();
    ApplyAVX2Overrides();
}

void VxBlitEngine::RebuildTables() {
    VxMutexLock lock(m_Lock);
    RebuildTablesUnlocked();
}

void VxBlitEngine::ApplySIMDMode(int effectiveMode) {
    VxMutexLock lock(m_Lock);
    m_EffectiveBlitKernelMode = CollapseSIMDModeToBlitKernelTier(effectiveMode);
    RebuildTablesUnlocked();
}

//==============================================================================
//  Pixel Format Utilities
//==============================================================================

VX_PIXELFORMAT VxBlitEngine::GetPixelFormat(const VxImageDescEx &desc) {
    // Check for DXT/bump formats first
    if (desc.Flags >= _DXT1 && desc.Flags < MAX_PIXEL_FORMATS) {
        return static_cast<VX_PIXELFORMAT>(desc.Flags);
    }

    // Search through standard formats
    const XDWORD bpp = desc.BitsPerPixel;
    const XDWORD rmask = desc.RedMask;
    const XDWORD gmask = desc.GreenMask;
    const XDWORD bmask = desc.BlueMask;
    const XDWORD amask = desc.AlphaMask;

    for (int i = 1; i < NUM_PIXEL_FORMATS && i < _DXT1; ++i) {
        const PixelFormatDef &fmt = s_PixelFormats[i];
        if (fmt.BitsPerPixel == bpp &&
            fmt.RedMask == rmask &&
            fmt.GreenMask == gmask &&
            fmt.BlueMask == bmask &&
            fmt.AlphaMask == amask) {
            return static_cast<VX_PIXELFORMAT>(i);
        }
    }

    return UNKNOWN_PF;
}

void VxBlitEngine::ConvertPixelFormat(VX_PIXELFORMAT Pf, VxImageDescEx &desc) {
    if (Pf <= UNKNOWN_PF || Pf >= MAX_PIXEL_FORMATS) return;

    const PixelFormatDef &fmt = s_PixelFormats[Pf];
    desc.BitsPerPixel = fmt.BitsPerPixel;
    desc.RedMask = fmt.RedMask;
    desc.GreenMask = fmt.GreenMask;
    desc.BlueMask = fmt.BlueMask;
    desc.AlphaMask = fmt.AlphaMask;

    if (Pf >= _DXT1) {
        desc.Flags = Pf;
    } else {
        desc.Flags = 0;
    }
}

const char *VxBlitEngine::PixelFormat2String(VX_PIXELFORMAT Pf) {
    if (Pf < 0 || Pf >= NUM_PIXEL_FORMATS) {
        return "";
    }
    return s_PixelFormats[Pf].Description;
}

void VxBlitEngine::SetupBlitInfo(VxBlitInfo &info, const VxImageDescEx &src_desc,
                                  const VxImageDescEx &dst_desc) {
    info.srcBytesPerPixel = src_desc.BitsPerPixel / 8;
    info.dstBytesPerPixel = dst_desc.BitsPerPixel / 8;
    info.width = src_desc.Width;
    info.copyBytes = src_desc.Width * info.srcBytesPerPixel;

    info.srcRedMask = src_desc.RedMask;
    info.srcGreenMask = src_desc.GreenMask;
    info.srcBlueMask = src_desc.BlueMask;
    info.srcAlphaMask = src_desc.AlphaMask;

    info.dstRedMask = dst_desc.RedMask;
    info.dstGreenMask = dst_desc.GreenMask;
    info.dstBlueMask = dst_desc.BlueMask;
    info.dstAlphaMask = dst_desc.AlphaMask;

    // Pre-compute shifts (position of LSB)
    info.redShiftSrc = static_cast<int>(GetBitShiftLocal(src_desc.RedMask));
    info.greenShiftSrc = static_cast<int>(GetBitShiftLocal(src_desc.GreenMask));
    info.blueShiftSrc = static_cast<int>(GetBitShiftLocal(src_desc.BlueMask));
    info.alphaShiftSrc = static_cast<int>(GetBitShiftLocal(src_desc.AlphaMask));

    info.redShiftDst = static_cast<int>(GetBitShiftLocal(dst_desc.RedMask));
    info.greenShiftDst = static_cast<int>(GetBitShiftLocal(dst_desc.GreenMask));
    info.blueShiftDst = static_cast<int>(GetBitShiftLocal(dst_desc.BlueMask));
    info.alphaShiftDst = static_cast<int>(GetBitShiftLocal(dst_desc.AlphaMask));

    // Compute bit widths for each channel
    info.redBitsSrc = static_cast<int>(GetBitCountLocal(src_desc.RedMask));
    info.greenBitsSrc = static_cast<int>(GetBitCountLocal(src_desc.GreenMask));
    info.blueBitsSrc = static_cast<int>(GetBitCountLocal(src_desc.BlueMask));
    info.alphaBitsSrc = static_cast<int>(GetBitCountLocal(src_desc.AlphaMask));

    info.redBitsDst = static_cast<int>(GetBitCountLocal(dst_desc.RedMask));
    info.greenBitsDst = static_cast<int>(GetBitCountLocal(dst_desc.GreenMask));
    info.blueBitsDst = static_cast<int>(GetBitCountLocal(dst_desc.BlueMask));
    info.alphaBitsDst = static_cast<int>(GetBitCountLocal(dst_desc.AlphaMask));

    info.alphaMaskInv = ~dst_desc.AlphaMask;

    info.colorMap = src_desc.ColorMap;
    info.colorMapEntries = src_desc.ColorMapEntries;
    info.bytesPerColorEntry = src_desc.BytesPerColorEntry;
}

//==============================================================================
//  Dispatch Lookup
//==============================================================================

VxBlitLineFunc VxBlitEngine::GetBlitFunction(const VxImageDescEx &src_desc,
                                              const VxImageDescEx &dst_desc) {
    VX_PIXELFORMAT srcFmt = GetPixelFormat(src_desc);
    VX_PIXELFORMAT dstFmt = GetPixelFormat(dst_desc);

    // Check for paletted source image
    if (src_desc.ColorMapEntries > 0 && src_desc.ColorMap != nullptr) {
        int srcBpp = src_desc.BitsPerPixel / 8;
        int dstBpp = dst_desc.BitsPerPixel / 8;

        // Only 8-bit paletted sources are currently supported
        if (srcBpp == 1 && dstBpp >= 1 && dstBpp <= 4) {
            // Index copy does not require palette entry expansion.
            if (dstBpp == 1) {
                VxBlitLineFunc func = m_PalettedBlitTable[0][0];
                if (func) return func;
            }

            // RGB expansion paths read B,G,R from each entry.
            if (src_desc.BytesPerColorEntry < 3) {
                return nullptr;
            }

            // For 16-bit destinations with alpha (1555, 4444), use alpha-aware function
            if (dstBpp == 2 && dst_desc.AlphaMask != 0) {
                return CopyLine_Paletted8_16Alpha;
            }
            VxBlitLineFunc func = m_PalettedBlitTable[0][dstBpp - 1];
            if (func) return func;
        }
    }

    // Same format -- use memcpy
    if (srcFmt == dstFmt && srcFmt != UNKNOWN_PF) {
        return CopyLineSame;
    }

    // Check for specific optimized function
    if (srcFmt > UNKNOWN_PF && srcFmt < FORMAT_TABLE_SIZE &&
        dstFmt > UNKNOWN_PF && dstFmt < FORMAT_TABLE_SIZE) {
        VxBlitLineFunc func = m_SpecificBlitTable[srcFmt][dstFmt];
        if (func) return func;
    }

    // DXT formats not supported for direct blitting
    if ((srcFmt >= _DXT1 && srcFmt <= _DXT5) ||
        (dstFmt >= _DXT1 && dstFmt <= _DXT5)) {
        return nullptr;
    }

    // Fall back to generic function
    int srcBpp = src_desc.BitsPerPixel / 8;
    int dstBpp = dst_desc.BitsPerPixel / 8;

    if (srcBpp >= 1 && srcBpp <= 4 && dstBpp >= 1 && dstBpp <= 4) {
        return m_GenericBlitTable[srcBpp - 1][dstBpp - 1];
    }

    return nullptr;
}

VxBlitLineFunc VxBlitEngine::GetSetAlphaFunction(const VxImageDescEx &dst_desc) {
    if (!dst_desc.AlphaMask) return nullptr;

    int bpp = dst_desc.BitsPerPixel / 8;
    if (bpp >= 1 && bpp <= 4) {
        return m_SetAlphaTable[bpp - 1];
    }
    return nullptr;
}

VxBlitLineFunc VxBlitEngine::GetCopyAlphaFunction(const VxImageDescEx &dst_desc) {
    if (!dst_desc.AlphaMask) return nullptr;

    int bpp = dst_desc.BitsPerPixel / 8;
    if (bpp >= 1 && bpp <= 4) {
        return m_CopyAlphaTable[bpp - 1];
    }
    return nullptr;
}

//==============================================================================
//  High-Level Blit Operations
//==============================================================================

void VxBlitEngine::DoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    // Original binary precondition: only allow if 32-bit OR same dimensions.
    // Non-32-bit images with different dimensions are rejected.
    if (src_desc.BitsPerPixel != 32) {
        if (dst_desc.Width != src_desc.Width || dst_desc.Height != src_desc.Height) {
            return;
        }
    }

    if (!src_desc.Image) return;
    if (!dst_desc.Image) return;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return;
    if (dst_desc.Width <= 0 || dst_desc.Height <= 0) return;

    // Truecolor -> paletted conversion goes through quantization and is complete.
    // Do not continue with a second blit pass (it would overwrite palette indices).
    if (dst_desc.ColorMapEntries > 0 && src_desc.ColorMapEntries == 0) {
        QuantizeImage(src_desc, dst_desc);
        return;
    }

    // Get blit function
    VxBlitLineFunc blitFunc = GetBlitFunction(src_desc, dst_desc);
    if (!blitFunc) return;

    // Setup blit info
    SetupBlitInfo(m_BlitInfo, src_desc, dst_desc);
    m_BlitInfo.srcBytesPerLine = src_desc.BytesPerLine;
    m_BlitInfo.dstBytesPerLine = dst_desc.BytesPerLine;
    m_BlitInfo.operationStamp = NextOperationStamp();

    // Check if resize is needed (only for 32-bit)
    if (dst_desc.Width != src_desc.Width || dst_desc.Height != src_desc.Height) {
        // At this point src must be 32-bit (per precondition above)
        DoBlitWithResize(src_desc, dst_desc, blitFunc);
        return;
    }

    // Same-size blit: process each scanline
    if (src_desc.Height <= 0) return;

    const XBYTE *srcRow = src_desc.Image;
    XBYTE *dstRow = dst_desc.Image;

    for (int y = 0; y < src_desc.Height; ++y) {
        m_BlitInfo.srcLine = srcRow;
        m_BlitInfo.dstLine = dstRow;

        blitFunc(&m_BlitInfo);

        srcRow += src_desc.BytesPerLine;
        dstRow += dst_desc.BytesPerLine;
    }
}

void VxBlitEngine::DoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    if (src_desc.Width != dst_desc.Width) return;
    if (src_desc.Height != dst_desc.Height) return;
    if (!src_desc.Image || !dst_desc.Image) return;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return;

    // Truecolor -> paletted upside-down path:
    // quantize into a temporary paletted buffer, then run regular upside-down copy.
    if (dst_desc.ColorMapEntries > 0 && src_desc.ColorMapEntries == 0) {
        const int tempSize = dst_desc.BytesPerLine * dst_desc.Height;
        if (tempSize <= 0) {
            return;
        }

        XArray<XBYTE> quantizedIndices;
        quantizedIndices.Resize(tempSize);

        VxImageDescEx quantizedDesc = dst_desc;
        quantizedDesc.Image = quantizedIndices.Begin();
        if (!QuantizeImage(src_desc, quantizedDesc)) {
            return;
        }

        DoBlitUpsideDown(quantizedDesc, dst_desc);
        return;
    }

    VxBlitLineFunc blitFunc = GetBlitFunction(src_desc, dst_desc);
    if (!blitFunc) return;

    SetupBlitInfo(m_BlitInfo, src_desc, dst_desc);
    m_BlitInfo.srcBytesPerLine = src_desc.BytesPerLine;
    m_BlitInfo.dstBytesPerLine = dst_desc.BytesPerLine;
    m_BlitInfo.operationStamp = NextOperationStamp();

    // Source starts at last row, destination at first row (matching original binary)
    const XBYTE *srcRow = src_desc.Image + (src_desc.Height - 1) * src_desc.BytesPerLine;
    XBYTE *dstRow = dst_desc.Image;

    for (int y = 0; y < src_desc.Height; ++y) {
        m_BlitInfo.srcLine = srcRow;
        m_BlitInfo.dstLine = dstRow;

        blitFunc(&m_BlitInfo);

        srcRow -= src_desc.BytesPerLine;  // Source goes backwards
        dstRow += dst_desc.BytesPerLine;  // Destination goes forward
    }
}

//==============================================================================
//  Alpha Operations
//==============================================================================

void VxBlitEngine::DoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue) {
    VxMutexLock lock(m_Lock);

    if (!dst_desc.AlphaMask) return;
    if (!dst_desc.Image) return;
    if (dst_desc.Width < 0) return;

    VxBlitLineFunc alphaFunc = GetSetAlphaFunction(dst_desc);
    if (!alphaFunc) return;

    // Setup blit info for alpha operation
    m_BlitInfo.dstBytesPerPixel = dst_desc.BitsPerPixel / 8;
    m_BlitInfo.dstAlphaMask = dst_desc.AlphaMask;
    m_BlitInfo.alphaMaskInv = ~dst_desc.AlphaMask;
    m_BlitInfo.width = dst_desc.Width;
    m_BlitInfo.dstBytesPerLine = dst_desc.BytesPerLine;
    m_BlitInfo.operationStamp = NextOperationStamp();

    // Scale 8-bit alpha to target bit depth
    XDWORD alphaMask = dst_desc.AlphaMask;
    int alphaShift = static_cast<int>(GetBitShiftLocal(alphaMask));

    // Count bits in the alpha mask
    XDWORD tempMask = alphaMask >> alphaShift;
    int alphaBits = 0;
    while (tempMask & 1) {
        ++alphaBits;
        tempMask >>= 1;
    }

    // Scale the 8-bit alpha value to the target bit depth
    XDWORD scaledAlpha;
    if (alphaBits >= 8) {
        // Direct placement for 8+ bit alpha
        scaledAlpha = (XDWORD)AlphaValue << alphaShift;
    } else {
        // Scale down from 8-bit to target bits by taking top bits
        int shiftAmount = 8 - alphaBits;
        scaledAlpha = ((XDWORD)AlphaValue >> shiftAmount) << alphaShift;
    }
    m_BlitInfo.alphaValue = scaledAlpha & alphaMask;

    XBYTE *row = dst_desc.Image;
    for (int y = 0; y < dst_desc.Height; ++y) {
        m_BlitInfo.dstLine = row;
        alphaFunc(&m_BlitInfo);
        row += dst_desc.BytesPerLine;
    }
}

void VxBlitEngine::DoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues) {
    VxMutexLock lock(m_Lock);

    if (!dst_desc.AlphaMask || !dst_desc.Image || !AlphaValues) return;
    if (dst_desc.Width < 0) return;

    VxBlitLineFunc alphaFunc = GetCopyAlphaFunction(dst_desc);
    if (!alphaFunc) return;

    m_BlitInfo.dstBytesPerPixel = dst_desc.BitsPerPixel / 8;
    m_BlitInfo.dstAlphaMask = dst_desc.AlphaMask;
    m_BlitInfo.alphaMaskInv = ~dst_desc.AlphaMask;
    m_BlitInfo.width = dst_desc.Width;
    m_BlitInfo.dstBytesPerLine = dst_desc.BytesPerLine;
    m_BlitInfo.alphaShiftDst = static_cast<int>(GetBitShiftLocal(dst_desc.AlphaMask));
    m_BlitInfo.operationStamp = NextOperationStamp();

    XBYTE *row = dst_desc.Image;
    const XBYTE *alphaRow = AlphaValues;

    for (int y = 0; y < dst_desc.Height; ++y) {
        m_BlitInfo.srcLine = alphaRow;
        m_BlitInfo.dstLine = row;

        alphaFunc(&m_BlitInfo);

        row += dst_desc.BytesPerLine;
        alphaRow += dst_desc.Width;
    }
}

//==============================================================================
//  Image Fill / Transform Operations
//==============================================================================

void VxBlitEngine::FillImage(const VxImageDescEx &dst_desc, XDWORD color) {
    VxMutexLock lock(m_Lock);

    if (!dst_desc.Image) return;
    if (dst_desc.Width <= 0 || dst_desc.Height <= 0) return;

    const int bpp = dst_desc.BitsPerPixel / 8;
    XBYTE *row = dst_desc.Image;

    if (bpp == 4) {
        // 32-bit fill with runtime-selected SIMD kernel when available.
        for (int y = 0; y < dst_desc.Height; ++y) {
            XDWORD *dst = (XDWORD *)row;
            if (m_FillLine32) {
                m_FillLine32(dst, dst_desc.Width, color);
            } else {
                for (int x = 0; x < dst_desc.Width; ++x) {
                    dst[x] = color;
                }
            }
            row += dst_desc.BytesPerLine;
        }
    } else if (bpp == 3) {
        // 24-bit fill
        XBYTE b = (XBYTE)(color & 0xFF);
        XBYTE g = (XBYTE)((color >> 8) & 0xFF);
        XBYTE r = (XBYTE)((color >> 16) & 0xFF);
        for (int y = 0; y < dst_desc.Height; ++y) {
            XBYTE *dst = row;
            for (int x = 0; x < dst_desc.Width; ++x) {
                dst[0] = b;
                dst[1] = g;
                dst[2] = r;
                dst += 3;
            }
            row += dst_desc.BytesPerLine;
        }
    } else if (bpp == 2) {
        // 16-bit fill with runtime-selected SIMD kernel when available.
        XWORD color16 = (XWORD)(color & 0xFFFF);
        for (int y = 0; y < dst_desc.Height; ++y) {
            XWORD *dst = (XWORD *)row;
            if (m_FillLine16) {
                m_FillLine16(dst, dst_desc.Width, color16);
            } else {
                for (int x = 0; x < dst_desc.Width; ++x) {
                    dst[x] = color16;
                }
            }
            row += dst_desc.BytesPerLine;
        }
    } else if (bpp == 1) {
        // 8-bit fill
        XBYTE color8 = (XBYTE)(color & 0xFF);
        for (int y = 0; y < dst_desc.Height; ++y) {
            memset(row, color8, dst_desc.Width);
            row += dst_desc.BytesPerLine;
        }
    }
}

void VxBlitEngine::PremultiplyAlpha(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32 || desc.AlphaMask == 0) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    VxBlitLineFunc premulFunc = m_PremultiplyAlpha32 ? m_PremultiplyAlpha32 : PremultiplyAlpha_32ARGB;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.srcLine = row;
        info.dstLine = row;  // In-place
        premulFunc(&info);
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::UnpremultiplyAlpha(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32 || desc.AlphaMask == 0) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    VxBlitLineFunc unpremulFunc = m_UnpremultiplyAlpha32 ? m_UnpremultiplyAlpha32 : UnpremultiplyAlpha_32ARGB;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.srcLine = row;
        info.dstLine = row;  // In-place
        unpremulFunc(&info);
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::SwapRedBlue(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    VxBlitLineFunc swapFunc = m_SwapRedBlue32 ? m_SwapRedBlue32 : CopyLine_32ARGB_32ABGR;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.srcLine = row;
        info.dstLine = row;  // In-place
        swapFunc(&info);
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::ClearAlpha(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.dstLine = row;
        if (m_ClearAlpha32) {
            m_ClearAlpha32(&info);
        } else {
            XDWORD *dst = (XDWORD *)row;
            for (int x = 0; x < desc.Width; ++x) {
                dst[x] &= 0x00FFFFFF;
            }
        }
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::SetFullAlpha(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.dstLine = row;
        if (m_SetFullAlpha32) {
            m_SetFullAlpha32(&info);
        } else {
            XDWORD *dst = (XDWORD *)row;
            for (int x = 0; x < desc.Width; ++x) {
                dst[x] |= 0xFF000000;
            }
        }
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::InvertColors(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.dstLine = row;
        if (m_InvertColors32) {
            m_InvertColors32(&info);
        } else {
            XDWORD *dst = (XDWORD *)row;
            for (int x = 0; x < desc.Width; ++x) {
                dst[x] ^= 0x00FFFFFF;
            }
        }
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::ConvertToGrayscale(const VxImageDescEx &desc) {
    VxMutexLock lock(m_Lock);

    if (!desc.Image) return;
    if (desc.BitsPerPixel != 32) return;
    if (desc.Width <= 0 || desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = desc.Width;

    XBYTE *row = desc.Image;
    for (int y = 0; y < desc.Height; ++y) {
        info.srcLine = row;
        info.dstLine = row;
        if (m_Grayscale32) {
            m_Grayscale32(&info);
        } else {
            XDWORD *dst = (XDWORD *)row;
            for (int x = 0; x < desc.Width; ++x) {
                XDWORD p = dst[x];
                XDWORD a = p & 0xFF000000;
                XDWORD r = (p >> 16) & 0xFF;
                XDWORD g = (p >> 8) & 0xFF;
                XDWORD b = p & 0xFF;
                XDWORD y_val = (77 * r + 150 * g + 29 * b) >> 8;
                dst[x] = a | (y_val << 16) | (y_val << 8) | y_val;
            }
        }
        row += desc.BytesPerLine;
    }
}

void VxBlitEngine::MultiplyBlend(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    if (!src_desc.Image || !dst_desc.Image) return;
    if (src_desc.BitsPerPixel != 32 || dst_desc.BitsPerPixel != 32) return;
    if (src_desc.Width != dst_desc.Width || src_desc.Height != dst_desc.Height) return;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return;

    VxBlitInfo info = {};
    info.width = src_desc.Width;

    const XBYTE *srcRow = src_desc.Image;
    XBYTE *dstRow = dst_desc.Image;
    for (int y = 0; y < src_desc.Height; ++y) {
        info.srcLine = srcRow;
        info.dstLine = dstRow;
        if (m_MultiplyBlend32) {
            m_MultiplyBlend32(&info);
        } else {
            // Bug E fix: corrected indentation of else block.
            const XDWORD *src = (const XDWORD *)srcRow;
            XDWORD *dst = (XDWORD *)dstRow;
            for (int x = 0; x < src_desc.Width; ++x) {
                XDWORD s = src[x];
                XDWORD d = dst[x];
                XDWORD r = (((s >> 16) & 0xFF) * ((d >> 16) & 0xFF) + 127) / 255;
                XDWORD g = (((s >> 8) & 0xFF) * ((d >> 8) & 0xFF) + 127) / 255;
                XDWORD b = ((s & 0xFF) * (d & 0xFF) + 127) / 255;
                XDWORD a = (((s >> 24) & 0xFF) * ((d >> 24) & 0xFF) + 127) / 255;
                dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        srcRow += src_desc.BytesPerLine;
        dstRow += dst_desc.BytesPerLine;
    }
}

//==============================================================================
//  Resize Operations
//==============================================================================

void VxBlitEngine::DoBlitWithResize(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc,
                                     VxBlitLineFunc blitFunc) {
    // Store destination width for line blit
    m_BlitInfo.width = dst_desc.Width;
    m_BlitInfo.copyBytes = dst_desc.Width * 4;

    // Ensure resize buffer is large enough
    int bufferSize = src_desc.Width + dst_desc.Width + 1;
    if (m_ResizeBuffer.Size() < bufferSize) {
        m_ResizeBuffer.Reserve(bufferSize);
    }

    // Setup resize info (fixed-point 16.16)
    VxResizeInfo resizeInfo;
    resizeInfo.w1 = src_desc.Width;
    resizeInfo.w2 = dst_desc.Width;
    resizeInfo.h1 = src_desc.Height;
    resizeInfo.h2 = dst_desc.Height;
    resizeInfo.wr1 = (src_desc.Width << 16) / dst_desc.Width;
    resizeInfo.hr1 = (src_desc.Height << 16) / dst_desc.Height;
    resizeInfo.srcPitch = src_desc.BytesPerLine >> 2;  // Source pitch in DWORDs

    // Intermediate buffer: resized horizontal line stored here before format conversion
    resizeInfo.dstRow = m_ResizeBuffer.Begin() + src_desc.Width;  // Output buffer for resize

    // Setup pointers for the blit function
    m_BlitInfo.srcLine = (const XBYTE *)resizeInfo.dstRow;
    XBYTE *dstRow = dst_desc.Image;

    const XDWORD *srcImage = (const XDWORD *)src_desc.Image;
    int srcHeight = src_desc.Height;
    int dstHeight = dst_desc.Height;

    // Select resize function based on X dimension relationship
    typedef void (*ResizeLineFunc)(const VxResizeInfo *);
    ResizeLineFunc growYFunc, shrinkYFunc, equalYFunc;

    if (src_desc.Width == dst_desc.Width) {
        growYFunc = ResizeHLine_GrowY_EqualX_32Bpp;
        shrinkYFunc = ResizeHLine_ShrinkY_EqualX_32Bpp;
        equalYFunc = ResizeHLine_EqualY_EqualX_32Bpp;
    } else if (src_desc.Width < dst_desc.Width) {
        growYFunc = ResizeHLine_GrowY_GrowX_32Bpp;
        shrinkYFunc = ResizeHLine_ShrinkY_GrowX_32Bpp;
        equalYFunc = ResizeHLine_EqualY_GrowX_32Bpp;
    } else {
        growYFunc = ResizeHLine_GrowY_ShrinkX_32Bpp;
        shrinkYFunc = ResizeHLine_ShrinkY_ShrinkX_32Bpp;
        equalYFunc = ResizeHLine_EqualY_ShrinkX_32Bpp;
    }

    if (srcHeight < dstHeight) {
        // Growing Y: interpolate vertically
        int yAccum = 0;
        int yMax = (srcHeight << 16) - 0x10000;
        int outY = 0;

        // Phase 1: while we haven't reached the last source row
        while (yAccum < yMax && yAccum >= 0 && outY < dstHeight) {
            int srcY = yAccum >> 16;
            resizeInfo.srcRow = srcImage + srcY * resizeInfo.srcPitch;
            growYFunc(&resizeInfo);

            // Blit the resized line to destination
            m_BlitInfo.dstLine = dstRow;
            blitFunc(&m_BlitInfo);

            dstRow += dst_desc.BytesPerLine;
            yAccum += resizeInfo.hr1;
            ++outY;
        }

        // Phase 2: fill remaining rows with last source row (equal Y copy)
        if (outY < dstHeight) {
            resizeInfo.srcRow = srcImage + (srcHeight - 1) * resizeInfo.srcPitch;
            while (outY < dstHeight) {
                equalYFunc(&resizeInfo);

                m_BlitInfo.dstLine = dstRow;
                blitFunc(&m_BlitInfo);

                dstRow += dst_desc.BytesPerLine;
                yAccum += resizeInfo.hr1;
                ++outY;
            }
        }
    } else if (srcHeight > dstHeight) {
        // Shrinking Y: skip source rows
        int yAccum = 0;
        for (int y = 0; y < dstHeight; ++y) {
            int srcY = yAccum >> 16;
            resizeInfo.srcRow = srcImage + srcY * resizeInfo.srcPitch;
            shrinkYFunc(&resizeInfo);

            m_BlitInfo.dstLine = dstRow;
            blitFunc(&m_BlitInfo);

            dstRow += dst_desc.BytesPerLine;
            yAccum += resizeInfo.hr1;
        }
    } else {
        // Equal Y: 1:1 vertical mapping
        resizeInfo.srcRow = srcImage;
        for (int y = 0; y < srcHeight; ++y) {
            equalYFunc(&resizeInfo);

            m_BlitInfo.dstLine = dstRow;
            blitFunc(&m_BlitInfo);

            dstRow += dst_desc.BytesPerLine;
            resizeInfo.srcRow += resizeInfo.srcPitch;
        }
    }
}

void VxBlitEngine::ResizeImage(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    if (!src_desc.Image || !dst_desc.Image) return;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return;
    if (dst_desc.Width <= 0 || dst_desc.Height <= 0) return;

    // Hybrid resize policy:
    // - direct bilinear for 24/32 same-format
    // - nearest-neighbor for same-format low-bpp (8/16) for performance
    // - 32-bit conversion path for mixed-format resize
    int srcChannels = src_desc.BitsPerPixel / 8;
    int dstChannels = dst_desc.BitsPerPixel / 8;

    // Handle 32-bit images directly with optimized bilinear interpolation
    if (srcChannels == 4 && dstChannels == 4) {
        ResizeBilinear32(src_desc, dst_desc);
        return;
    }

    // Handle 24-bit images directly
    if (srcChannels == 3 && dstChannels == 3) {
        ResizeBilinear24(src_desc, dst_desc);
        return;
    }

    // Fast path: same low-bpp formats (8/16) use direct nearest-neighbor.
    // This avoids expensive 32-bit conversion round-trips for 16-bit surfaces.
    if (srcChannels == dstChannels && srcChannels > 0 && srcChannels != 3 && srcChannels != 4) {
        ResizeNearestNeighbor(src_desc, dst_desc);
        return;
    }

    // For mixed bit depths, convert to 32-bit, resize, then convert back
    if (srcChannels >= 2) {
        // Allocate temporary 32-bit buffers
        XArray<XBYTE> srcTemp, dstTemp;
        srcTemp.Reserve(src_desc.Width * src_desc.Height * 4);
        dstTemp.Reserve(dst_desc.Width * dst_desc.Height * 4);

        // Convert source to 32-bit ARGB
        VxImageDescEx src32;
        src32.Width = src_desc.Width;
        src32.Height = src_desc.Height;
        src32.BitsPerPixel = 32;
        src32.BytesPerLine = src_desc.Width * 4;
        src32.RedMask = 0x00FF0000;
        src32.GreenMask = 0x0000FF00;
        src32.BlueMask = 0x000000FF;
        src32.AlphaMask = 0xFF000000;
        src32.Image = srcTemp.Begin();

        // Blit to 32-bit format
        DoBlit(src_desc, src32);

        // Create destination 32-bit descriptor
        VxImageDescEx dst32;
        dst32.Width = dst_desc.Width;
        dst32.Height = dst_desc.Height;
        dst32.BitsPerPixel = 32;
        dst32.BytesPerLine = dst_desc.Width * 4;
        dst32.RedMask = 0x00FF0000;
        dst32.GreenMask = 0x0000FF00;
        dst32.BlueMask = 0x000000FF;
        dst32.AlphaMask = 0xFF000000;
        dst32.Image = dstTemp.Begin();

        // Resize in 32-bit
        ResizeBilinear32(src32, dst32);

        // Convert back to destination format
        DoBlit(dst32, dst_desc);
        return;
    }

    // Fallback: simple nearest-neighbor for 8-bit and non-standard formats
    ResizeNearestNeighbor(src_desc, dst_desc);
}

//==============================================================================
//  Bilinear Resize Implementation
//==============================================================================

void VxBlitEngine::ResizeBilinear32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    const int srcW = src_desc.Width;
    const int srcH = src_desc.Height;
    const int dstW = dst_desc.Width;
    const int dstH = dst_desc.Height;

    // Fixed-point scale factors (16.16 format)
    const int scaleX = (srcW << 16) / dstW;
    const int scaleY = (srcH << 16) / dstH;

#if defined(VX_SIMD_SSE2)
    if (m_EffectiveBlitKernelMode != VX_SIMD_MODE_NONE) {
        XArray<int> srcX0Lut(dstW);
        XArray<int> srcX1Lut(dstW);
        XArray<int> wX0Lut(dstW);
        XArray<int> wX1Lut(dstW);
        srcX0Lut.Resize(dstW);
        srcX1Lut.Resize(dstW);
        wX0Lut.Resize(dstW);
        wX1Lut.Resize(dstW);
        int srcXFixed = 0;
        for (int x = 0; x < dstW; ++x) {
            const int srcX0 = srcXFixed >> 16;
            const int srcX1 = XMin(srcX0 + 1, srcW - 1);
            const int fracX = srcXFixed & 0xFFFF;
            const int invFracX = 0x10000 - fracX;
            const int wX0 = (invFracX + 128) >> 8;
            srcX0Lut[x] = srcX0;
            srcX1Lut[x] = srcX1;
            wX0Lut[x] = wX0;
            wX1Lut[x] = 256 - wX0;
            srcXFixed += scaleX;
        }

        // SSE2 optimized bilinear resize
        for (int y = 0; y < dstH; ++y) {
            // Calculate source Y coordinate and fraction
            const int srcYFixed = y * scaleY;
            const int srcY0 = srcYFixed >> 16;
            const int srcY1 = XMin(srcY0 + 1, srcH - 1);
            const int fracY = srcYFixed & 0xFFFF;
            const int invFracY = 0x10000 - fracY;

            const XDWORD *srcRow0 = (const XDWORD *)(src_desc.Image + srcY0 * src_desc.BytesPerLine);
            const XDWORD *srcRow1 = (const XDWORD *)(src_desc.Image + srcY1 * src_desc.BytesPerLine);
            XDWORD *dstRow = (XDWORD *)(dst_desc.Image + y * dst_desc.BytesPerLine);

            // SSE constants for this row
            const int wY0 = (invFracY + 128) >> 8;
            const int wY1 = 256 - wY0;
            const __m128i zero = _mm_setzero_si128();

            for (int x = 0; x < dstW; ++x) {
                const int srcX0 = srcX0Lut[x];
                const int srcX1 = srcX1Lut[x];
                const int wX0 = wX0Lut[x];
                const int wX1 = wX1Lut[x];

                // Load 4 corner pixels
                XDWORD p00 = srcRow0[srcX0];
                XDWORD p10 = srcRow0[srcX1];
                XDWORD p01 = srcRow1[srcX0];
                XDWORD p11 = srcRow1[srcX1];

                // Unpack to 16-bit and multiply
                __m128i px00 = _mm_cvtsi32_si128(p00);
                __m128i px10 = _mm_cvtsi32_si128(p10);
                __m128i px01 = _mm_cvtsi32_si128(p01);
                __m128i px11 = _mm_cvtsi32_si128(p11);

                px00 = _mm_unpacklo_epi8(px00, zero);
                px10 = _mm_unpacklo_epi8(px10, zero);
                px01 = _mm_unpacklo_epi8(px01, zero);
                px11 = _mm_unpacklo_epi8(px11, zero);

                const __m128i vwX0 = _mm_set1_epi16((short)wX0);
                const __m128i vwX1 = _mm_set1_epi16((short)wX1);
                const __m128i vwY0 = _mm_set1_epi16((short)wY0);
                const __m128i vwY1 = _mm_set1_epi16((short)wY1);

                __m128i row0 = _mm_mullo_epi16(px00, vwX0);
                row0 = _mm_add_epi16(row0, _mm_mullo_epi16(px10, vwX1));
                row0 = _mm_srli_epi16(row0, 8);

                __m128i row1 = _mm_mullo_epi16(px01, vwX0);
                row1 = _mm_add_epi16(row1, _mm_mullo_epi16(px11, vwX1));
                row1 = _mm_srli_epi16(row1, 8);

                __m128i result = _mm_mullo_epi16(row0, vwY0);
                result = _mm_add_epi16(result, _mm_mullo_epi16(row1, vwY1));
                result = _mm_srli_epi16(result, 8);
                result = _mm_packus_epi16(result, zero);

                dstRow[x] = _mm_cvtsi128_si32(result);
            }
        }
        return;
    }
#endif

    // Scalar bilinear resize (fallback)
    for (int y = 0; y < dstH; ++y) {
        const int srcYFixed = y * scaleY;
        const int srcY0 = srcYFixed >> 16;
        const int srcY1 = XMin(srcY0 + 1, srcH - 1);
        const int fracY = srcYFixed & 0xFFFF;
        const int invFracY = 0x10000 - fracY;

        const XDWORD *srcRow0 = (const XDWORD *)(src_desc.Image + srcY0 * src_desc.BytesPerLine);
        const XDWORD *srcRow1 = (const XDWORD *)(src_desc.Image + srcY1 * src_desc.BytesPerLine);
        XDWORD *dstRow = (XDWORD *)(dst_desc.Image + y * dst_desc.BytesPerLine);

        int srcXFixed = 0;
        for (int x = 0; x < dstW; ++x) {
            const int srcX0 = srcXFixed >> 16;
            const int srcX1 = XMin(srcX0 + 1, srcW - 1);
            const int fracX = srcXFixed & 0xFFFF;
            const int invFracX = 0x10000 - fracX;

            XDWORD p00 = srcRow0[srcX0];
            XDWORD p10 = srcRow0[srcX1];
            XDWORD p01 = srcRow1[srcX0];
            XDWORD p11 = srcRow1[srcX1];

            // Bilinear interpolation for each channel
            XDWORD a = ((((p00 >> 24) & 0xFF) * invFracX + ((p10 >> 24) & 0xFF) * fracX) >> 16) * invFracY +
                       ((((p01 >> 24) & 0xFF) * invFracX + ((p11 >> 24) & 0xFF) * fracX) >> 16) * fracY;
            XDWORD r = ((((p00 >> 16) & 0xFF) * invFracX + ((p10 >> 16) & 0xFF) * fracX) >> 16) * invFracY +
                       ((((p01 >> 16) & 0xFF) * invFracX + ((p11 >> 16) & 0xFF) * fracX) >> 16) * fracY;
            XDWORD g = ((((p00 >> 8) & 0xFF) * invFracX + ((p10 >> 8) & 0xFF) * fracX) >> 16) * invFracY +
                       ((((p01 >> 8) & 0xFF) * invFracX + ((p11 >> 8) & 0xFF) * fracX) >> 16) * fracY;
            XDWORD b = (((p00 & 0xFF) * invFracX + (p10 & 0xFF) * fracX) >> 16) * invFracY +
                       (((p01 & 0xFF) * invFracX + (p11 & 0xFF) * fracX) >> 16) * fracY;

            dstRow[x] = ((a >> 16) << 24) | ((r >> 16) << 16) | ((g >> 16) << 8) | (b >> 16);
            srcXFixed += scaleX;
        }
    }
}

void VxBlitEngine::ResizeBilinear24(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    const int srcW = src_desc.Width;
    const int srcH = src_desc.Height;
    const int dstW = dst_desc.Width;
    const int dstH = dst_desc.Height;

    const int scaleX = (srcW << 16) / dstW;
    const int scaleY = (srcH << 16) / dstH;

    for (int y = 0; y < dstH; ++y) {
        const int srcYFixed = y * scaleY;
        const int srcY0 = srcYFixed >> 16;
        const int srcY1 = XMin(srcY0 + 1, srcH - 1);
        const int fracY = srcYFixed & 0xFFFF;
        const int invFracY = 0x10000 - fracY;

        const XBYTE *srcRow0 = src_desc.Image + srcY0 * src_desc.BytesPerLine;
        const XBYTE *srcRow1 = src_desc.Image + srcY1 * src_desc.BytesPerLine;
        XBYTE *dstRow = dst_desc.Image + y * dst_desc.BytesPerLine;

        int srcXFixed = 0;
        for (int x = 0; x < dstW; ++x) {
            const int srcX0 = srcXFixed >> 16;
            const int srcX1 = XMin(srcX0 + 1, srcW - 1);
            const int fracX = srcXFixed & 0xFFFF;
            const int invFracX = 0x10000 - fracX;

            const XBYTE *p00 = srcRow0 + srcX0 * 3;
            const XBYTE *p10 = srcRow0 + srcX1 * 3;
            const XBYTE *p01 = srcRow1 + srcX0 * 3;
            const XBYTE *p11 = srcRow1 + srcX1 * 3;

            // Bilinear interpolation for each channel (BGR order)
            for (int c = 0; c < 3; ++c) {
                XDWORD v = (((p00[c] * invFracX + p10[c] * fracX) >> 16) * invFracY +
                            ((p01[c] * invFracX + p11[c] * fracX) >> 16) * fracY) >> 16;
                dstRow[x * 3 + c] = (XBYTE)v;
            }

            srcXFixed += scaleX;
        }
    }
}

void VxBlitEngine::ResizeNearestNeighbor(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    const int srcW = src_desc.Width;
    const int srcH = src_desc.Height;
    const int dstW = dst_desc.Width;
    const int dstH = dst_desc.Height;
    const int srcBpp = src_desc.BitsPerPixel / 8;
    const int dstBpp = dst_desc.BitsPerPixel / 8;

    const int scaleX = (srcW << 16) / dstW;
    const int scaleY = (srcH << 16) / dstH;
    const int copyBpp = XMin(srcBpp, dstBpp);

    XArray<int> srcXMap(dstW);
    srcXMap.Resize(dstW);
    int srcXFixed = 0;
    for (int x = 0; x < dstW; ++x) {
        int srcX = srcXFixed >> 16;
        if (srcX >= srcW) srcX = srcW - 1;
        srcXMap[x] = srcX;
        srcXFixed += scaleX;
    }

    int srcYFixed = 0;
    for (int y = 0; y < dstH; ++y) {
        int srcY = srcYFixed >> 16;
        if (srcY >= srcH) srcY = srcH - 1;
        const XBYTE *srcRow = src_desc.Image + srcY * src_desc.BytesPerLine;
        XBYTE *dstRow = dst_desc.Image + y * dst_desc.BytesPerLine;

        if (srcBpp == dstBpp) {
            if (srcBpp == 1) {
                for (int x = 0; x < dstW; ++x) {
                    dstRow[x] = srcRow[srcXMap[x]];
                }
            } else if (srcBpp == 2) {
                for (int x = 0; x < dstW; ++x) {
                    const XBYTE *srcPx = srcRow + (srcXMap[x] * 2);
                    XBYTE *dstPx = dstRow + (x * 2);
                    dstPx[0] = srcPx[0];
                    dstPx[1] = srcPx[1];
                }
            } else if (srcBpp == 3) {
                for (int x = 0; x < dstW; ++x) {
                    const XBYTE *srcPx = srcRow + (srcXMap[x] * 3);
                    XBYTE *dstPx = dstRow + (x * 3);
                    dstPx[0] = srcPx[0];
                    dstPx[1] = srcPx[1];
                    dstPx[2] = srcPx[2];
                }
            } else if (srcBpp == 4) {
                for (int x = 0; x < dstW; ++x) {
                    const XBYTE *srcPx = srcRow + (srcXMap[x] * 4);
                    XBYTE *dstPx = dstRow + (x * 4);
                    dstPx[0] = srcPx[0];
                    dstPx[1] = srcPx[1];
                    dstPx[2] = srcPx[2];
                    dstPx[3] = srcPx[3];
                }
            } else {
                for (int x = 0; x < dstW; ++x) {
                    const XBYTE *srcPx = srcRow + srcXMap[x] * srcBpp;
                    XBYTE *dstPx = dstRow + x * dstBpp;
                    memcpy(dstPx, srcPx, static_cast<size_t>(srcBpp));
                }
            }
        } else {
            for (int x = 0; x < dstW; ++x) {
                const XBYTE *srcPx = srcRow + srcXMap[x] * srcBpp;
                XBYTE *dstPx = dstRow + x * dstBpp;

                for (int b = 0; b < copyBpp; ++b) {
                    dstPx[b] = srcPx[b];
                }
                for (int b = copyBpp; b < dstBpp; ++b) {
                    dstPx[b] = 0xFF;
                }
            }
        }
        srcYFixed += scaleY;
    }
}

//==============================================================================
//  Quantization
//==============================================================================

//------------------------------------------------------------------------------
// Median-Cut Algorithm -- helper types (file-local)
//------------------------------------------------------------------------------

namespace {

struct ColorBox {
    int rMin, rMax;
    int gMin, gMax;
    int bMin, bMax;
    int count;
    int pixelStartIdx;
    int pixelCount;
};

struct ColorPixel {
    XBYTE r, g, b;
    int count;
};

int ComparePixelByR(const void *a, const void *b) {
    const ColorPixel *pa = reinterpret_cast<const ColorPixel *>(a);
    const ColorPixel *pb = reinterpret_cast<const ColorPixel *>(b);
    return static_cast<int>(pa->r) - static_cast<int>(pb->r);
}

int ComparePixelByG(const void *a, const void *b) {
    const ColorPixel *pa = reinterpret_cast<const ColorPixel *>(a);
    const ColorPixel *pb = reinterpret_cast<const ColorPixel *>(b);
    return static_cast<int>(pa->g) - static_cast<int>(pb->g);
}

int ComparePixelByB(const void *a, const void *b) {
    const ColorPixel *pa = reinterpret_cast<const ColorPixel *>(a);
    const ColorPixel *pb = reinterpret_cast<const ColorPixel *>(b);
    return static_cast<int>(pa->b) - static_cast<int>(pb->b);
}

inline int ColorDistSq(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    // Weighted by human perception: green > red > blue
    return dr * dr * 2 + dg * dg * 4 + db * db;
}

} // anonymous namespace

//------------------------------------------------------------------------------
// NeuQuant-based Quantization (Default)
//------------------------------------------------------------------------------

XBOOL VxBlitEngine::QuantizeImage(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    // Quantization requires 256 color palette
    if (dst_desc.ColorMapEntries != 256) return FALSE;
    if (!dst_desc.ColorMap) return FALSE;
    if (dst_desc.BytesPerColorEntry < 3) return FALSE;

    int srcBpp = src_desc.BitsPerPixel / 8;
    if (srcBpp != 3 && srcBpp != 4) return FALSE;

    if (!src_desc.Image || !dst_desc.Image) return FALSE;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return FALSE;
    if (dst_desc.Width != src_desc.Width || dst_desc.Height != src_desc.Height) return FALSE;

    const int totalPixels = src_desc.Width * src_desc.Height;

    // Step 1: Convert image to RGB buffer for NeuQuant
    // NeuQuant expects RGB triplets (R, G, B order)
    const int rgbLen = totalPixels * 3;
    XArray<XBYTE> rgbBuffer;
    rgbBuffer.Resize(rgbLen);

    const XBYTE *srcRow = src_desc.Image;
    XBYTE *rgbPtr = rgbBuffer.Begin();

    for (int y = 0; y < src_desc.Height; ++y) {
        const XBYTE *src = srcRow;
        for (int x = 0; x < src_desc.Width; ++x) {
            // Source is BGR(A), NeuQuant expects RGB
            rgbPtr[0] = src[2]; // R
            rgbPtr[1] = src[1]; // G
            rgbPtr[2] = src[0]; // B
            rgbPtr += 3;
            src += srcBpp;
        }
        srcRow += src_desc.BytesPerLine;
    }

    // Step 2: Run NeuQuant
    NeuQuant nq;
    int sampleFac = GetQuantizationSamplingFactor();
    if (sampleFac < 1) sampleFac = 1;
    if (sampleFac > 30) sampleFac = 30;

    nq.initnet(rgbBuffer.Begin(), rgbLen, sampleFac);
    nq.learn();
    nq.unbiasnet();
    nq.inxbuild();

    // Step 3: Write palette
    XBYTE *palette = dst_desc.ColorMap;
    int paletteBytesPer = dst_desc.BytesPerColorEntry;
    if (paletteBytesPer < 3) paletteBytesPer = 3;

    for (int i = 0; i < 256; ++i) {
        XBYTE r, g, b;
        nq.getpalette(i, r, g, b);
        XBYTE *entry = palette + i * paletteBytesPer;
        entry[0] = b;
        entry[1] = g;
        entry[2] = r;
        if (paletteBytesPer >= 4) {
            entry[3] = 0xFF;
        }
    }

    // Step 4: Map pixels to palette indices
    srcRow = src_desc.Image;
    XBYTE *dstRow = dst_desc.Image;

    for (int y = 0; y < src_desc.Height; ++y) {
        const XBYTE *src = srcRow;
        XBYTE *dst = dstRow;

        for (int x = 0; x < src_desc.Width; ++x) {
            // Source is BGR(A)
            int b = src[0];
            int g = src[1];
            int r = src[2];

            // Find best matching palette index
            int idx = nq.inxsearch(r, g, b);
            *dst = (XBYTE)idx;

            src += srcBpp;
            dst++;
        }

        srcRow += src_desc.BytesPerLine;
        dstRow += dst_desc.BytesPerLine;
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// Median-Cut Quantization
//------------------------------------------------------------------------------

XBOOL VxBlitEngine::QuantizeImageMedianCut(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    VxMutexLock lock(m_Lock);

    // Quantization requires 256 color palette
    if (dst_desc.ColorMapEntries != 256) return FALSE;
    if (!dst_desc.ColorMap) return FALSE;
    if (dst_desc.BytesPerColorEntry < 3) return FALSE;

    int srcBpp = src_desc.BitsPerPixel / 8;
    if (srcBpp != 3 && srcBpp != 4) return FALSE;

    if (!src_desc.Image || !dst_desc.Image) return FALSE;
    if (src_desc.Width <= 0 || src_desc.Height <= 0) return FALSE;
    if (dst_desc.Width != src_desc.Width || dst_desc.Height != src_desc.Height) return FALSE;

    const int totalPixels = src_desc.Width * src_desc.Height;
    const int maxColors = 256;

    // Step 1: Build color histogram
    struct ColorEntry {
        XDWORD color;
        int count;
        int next;
    };

    const int hashSize = 65536;
    XArray<int> hashTable;
    hashTable.Resize(hashSize);
    for (int i = 0; i < hashSize; ++i) {
        hashTable[i] = -1;
    }

    XArray<ColorEntry> colorEntries;
    colorEntries.Resize(totalPixels / 4 + 256);
    int numUniqueColors = 0;

    // Scan all pixels and build histogram
    const XBYTE *srcRow = src_desc.Image;
    for (int y = 0; y < src_desc.Height; ++y) {
        const XBYTE *src = srcRow;
        for (int x = 0; x < src_desc.Width; ++x) {
            XBYTE b = src[0];
            XBYTE g = src[1];
            XBYTE r = src[2];

            // Create color key (5-6-5 quantized for hash)
            XDWORD colorKey = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            XDWORD fullColor = (r << 16) | (g << 8) | b;

            // Search hash chain
            int idx = hashTable[colorKey];
            bool found = false;
            while (idx >= 0) {
                if (colorEntries[idx].color == fullColor) {
                    colorEntries[idx].count++;
                    found = true;
                    break;
                }
                idx = colorEntries[idx].next;
            }

            if (!found) {
                // Add new color
                ColorEntry entry;
                entry.color = fullColor;
                entry.count = 1;
                entry.next = hashTable[colorKey];

                int newIdx = numUniqueColors++;
                if (newIdx >= (int)colorEntries.Size()) {
                    colorEntries.Resize(colorEntries.Size() + 1024);
                }
                colorEntries[newIdx] = entry;
                hashTable[colorKey] = newIdx;
            }

            src += srcBpp;
        }
        srcRow += src_desc.BytesPerLine;
    }

    // Step 2: Build pixel array for median-cut
    XArray<ColorPixel> pixels;
    pixels.Resize(numUniqueColors);

    for (int i = 0; i < numUniqueColors; ++i) {
        ColorPixel cp;
        cp.r = (colorEntries[i].color >> 16) & 0xFF;
        cp.g = (colorEntries[i].color >> 8) & 0xFF;
        cp.b = colorEntries[i].color & 0xFF;
        cp.count = colorEntries[i].count;
        pixels[i] = cp;
    }

    // Step 3: Median-cut algorithm
    XArray<ColorBox> boxes;
    boxes.Resize(maxColors);

    // Initialize first box containing all colors
    ColorBox initialBox;
    initialBox.rMin = initialBox.gMin = initialBox.bMin = 255;
    initialBox.rMax = initialBox.gMax = initialBox.bMax = 0;
    initialBox.count = 0;
    initialBox.pixelStartIdx = 0;
    initialBox.pixelCount = numUniqueColors;

    for (int i = 0; i < numUniqueColors; ++i) {
        initialBox.rMin = XMin(initialBox.rMin, (int)pixels[i].r);
        initialBox.rMax = XMax(initialBox.rMax, (int)pixels[i].r);
        initialBox.gMin = XMin(initialBox.gMin, (int)pixels[i].g);
        initialBox.gMax = XMax(initialBox.gMax, (int)pixels[i].g);
        initialBox.bMin = XMin(initialBox.bMin, (int)pixels[i].b);
        initialBox.bMax = XMax(initialBox.bMax, (int)pixels[i].b);
        initialBox.count += pixels[i].count;
    }

    boxes[0] = initialBox;
    int numBoxes = 1;

    // Split boxes until we have maxColors
    while (numBoxes < maxColors) {
        // Find box with largest range to split
        int splitIdx = -1;
        int maxRange = 0;

        for (int i = 0; i < numBoxes; ++i) {
            int rRange = boxes[i].rMax - boxes[i].rMin;
            int gRange = boxes[i].gMax - boxes[i].gMin;
            int bRange = boxes[i].bMax - boxes[i].bMin;
            int largestRange = XMax(rRange, XMax(gRange, bRange));

            if (boxes[i].pixelCount <= 1) continue;
            if (largestRange == 0) continue;

            if (largestRange > maxRange) {
                maxRange = largestRange;
                splitIdx = i;
            }
        }

        if (splitIdx < 0) break;

        ColorBox &box = boxes[splitIdx];

        // Find longest axis
        int rRange = box.rMax - box.rMin;
        int gRange = box.gMax - box.gMin;
        int bRange = box.bMax - box.bMin;

        // Sort pixels by longest axis
        ColorPixel *boxPixels = pixels.Begin() + box.pixelStartIdx;
        if (rRange >= gRange && rRange >= bRange) {
            qsort(boxPixels, static_cast<size_t>(box.pixelCount), sizeof(ColorPixel), ComparePixelByR);
        } else if (gRange >= bRange) {
            qsort(boxPixels, static_cast<size_t>(box.pixelCount), sizeof(ColorPixel), ComparePixelByG);
        } else {
            qsort(boxPixels, static_cast<size_t>(box.pixelCount), sizeof(ColorPixel), ComparePixelByB);
        }

        // Split at median
        int splitPoint = box.pixelCount / 2;
        if (splitPoint == 0) splitPoint = 1;

        // Create new box for second half
        ColorBox newBox;
        newBox.pixelStartIdx = box.pixelStartIdx + splitPoint;
        newBox.pixelCount = box.pixelCount - splitPoint;

        // Update original box
        box.pixelCount = splitPoint;

        // Recalculate bounds for both boxes
        auto recalcBounds = [&pixels](ColorBox &b) {
            b.rMin = b.gMin = b.bMin = 255;
            b.rMax = b.gMax = b.bMax = 0;
            b.count = 0;
            for (int i = 0; i < b.pixelCount; ++i) {
                ColorPixel &cp = pixels[b.pixelStartIdx + i];
                b.rMin = XMin(b.rMin, (int)cp.r);
                b.rMax = XMax(b.rMax, (int)cp.r);
                b.gMin = XMin(b.gMin, (int)cp.g);
                b.gMax = XMax(b.gMax, (int)cp.g);
                b.bMin = XMin(b.bMin, (int)cp.b);
                b.bMax = XMax(b.bMax, (int)cp.b);
                b.count += cp.count;
            }
        };

        recalcBounds(box);
        recalcBounds(newBox);

        boxes[numBoxes++] = newBox;
    }

    // Step 4: Calculate palette colors (weighted average of each box)
    XBYTE *palette = dst_desc.ColorMap;
    int paletteBytesPer = dst_desc.BytesPerColorEntry;
    if (paletteBytesPer < 3) paletteBytesPer = 3;

    for (int i = 0; i < numBoxes; ++i) {
        ColorBox &box = boxes[i];
        long long rSum = 0, gSum = 0, bSum = 0;
        int totalWeight = 0;

        for (int j = 0; j < box.pixelCount; ++j) {
            ColorPixel &cp = pixels[box.pixelStartIdx + j];
            rSum += cp.r * cp.count;
            gSum += cp.g * cp.count;
            bSum += cp.b * cp.count;
            totalWeight += cp.count;
        }

        XBYTE *entry = palette + i * paletteBytesPer;
        if (totalWeight > 0) {
            entry[0] = (XBYTE)(bSum / totalWeight);
            entry[1] = (XBYTE)(gSum / totalWeight);
            entry[2] = (XBYTE)(rSum / totalWeight);
        } else {
            entry[0] = entry[1] = entry[2] = 0;
        }
        if (paletteBytesPer >= 4) {
            entry[3] = 0xFF;
        }
    }

    // Fill remaining palette entries with black
    for (int i = numBoxes; i < maxColors; ++i) {
        XBYTE *entry = palette + i * paletteBytesPer;
        entry[0] = entry[1] = entry[2] = 0;
        if (paletteBytesPer >= 4) {
            entry[3] = 0xFF;
        }
    }

    // Step 5: Map pixels to palette
    srcRow = src_desc.Image;
    XBYTE *dstRow = dst_desc.Image;

    for (int y = 0; y < src_desc.Height; ++y) {
        const XBYTE *src = srcRow;
        XBYTE *dst = dstRow;

        for (int x = 0; x < src_desc.Width; ++x) {
            XBYTE b = src[0];
            XBYTE g = src[1];
            XBYTE r = src[2];

            // Find closest palette entry
            int bestIdx = 0;
            int bestDist = 0x7FFFFFFF;

            for (int i = 0; i < numBoxes; ++i) {
                XBYTE *entry = palette + i * paletteBytesPer;
                int dist = ColorDistSq(r, g, b, entry[2], entry[1], entry[0]);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                    if (dist == 0) break;
                }
            }

            *dst = (XBYTE)bestIdx;

            src += srcBpp;
            dst++;
        }

        srcRow += src_desc.BytesPerLine;
        dstRow += dst_desc.BytesPerLine;
    }

    return TRUE;
}
