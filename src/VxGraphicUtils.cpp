#include <limits>
#include <atomic>

#include "VxMath.h"
#include "VxAtomic.h"
#include "VxSIMD.h"
#include "VxSIMDDispatchInternal.h"
#include "VxBlitEngine.h"
#include "VxImageKernels.h"

namespace {
typedef void (*VxGenerateMipMapDispatchFn)(const VxImageDescEx &, XBYTE *);
typedef XBOOL (*VxConvertToNormalMapDispatchFn)(const VxImageDescEx &, XDWORD);
typedef XBOOL (*VxConvertToBumpMapDispatchFn)(const VxImageDescEx &);

static void GenerateMipMapScalarDispatch(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    VxGenerateMipMapKernel(src_desc, Buffer, false);
}

static XBOOL ConvertToNormalMapScalarDispatch(const VxImageDescEx &image, XDWORD ColorMask) {
    return VxConvertToNormalMapKernel(image, ColorMask, false);
}

static XBOOL ConvertToBumpMapScalarDispatch(const VxImageDescEx &image) {
    return VxConvertToBumpMapKernel(image, false);
}

#if defined(VX_SIMD_SSE2)
static void GenerateMipMapSIMDDispatch(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    VxGenerateMipMapKernel(src_desc, Buffer, true);
}

static XBOOL ConvertToNormalMapSIMDDispatch(const VxImageDescEx &image, XDWORD ColorMask) {
    return VxConvertToNormalMapKernel(image, ColorMask, true);
}

static XBOOL ConvertToBumpMapSIMDDispatch(const VxImageDescEx &image) {
    return VxConvertToBumpMapKernel(image, true);
}
#endif

struct VxGraphicDispatchTable {
    VxGenerateMipMapDispatchFn generateMipMap;
    VxConvertToNormalMapDispatchFn convertToNormalMap;
    VxConvertToBumpMapDispatchFn convertToBumpMap;
};

const VxGraphicDispatchTable kVxGraphicDispatchScalar = {
    GenerateMipMapScalarDispatch,
    ConvertToNormalMapScalarDispatch,
    ConvertToBumpMapScalarDispatch
};

#if defined(VX_SIMD_SSE2)
const VxGraphicDispatchTable kVxGraphicDispatchSIMD = {
    GenerateMipMapSIMDDispatch,
    ConvertToNormalMapSIMDDispatch,
    ConvertToBumpMapSIMDDispatch
};
#endif

std::atomic<const VxGraphicDispatchTable *> g_VxGraphicDispatch(&kVxGraphicDispatchScalar);

const VxGraphicDispatchTable *GetVxGraphicDispatchTable() {
    return g_VxGraphicDispatch.load(std::memory_order_acquire);
}
}

void VxGraphicDispatchRebuild(int effectiveMode) {
    const bool useSIMD = (effectiveMode != VX_SIMD_MODE_NONE);
#if defined(VX_SIMD_SSE2)
    const VxGraphicDispatchTable *next = useSIMD ? &kVxGraphicDispatchSIMD : &kVxGraphicDispatchScalar;
#else
    (void) useSIMD;
    const VxGraphicDispatchTable *next = &kVxGraphicDispatchScalar;
#endif
    g_VxGraphicDispatch.store(next, std::memory_order_release);
}

//------------------------------------------------------------------------------
// Bit manipulation functions
//------------------------------------------------------------------------------

XDWORD GetBitCount(XDWORD dwMask) {
    if (dwMask == 0) return 0;

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __popcnt(dwMask);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(dwMask);
#else
    XDWORD count = 0;

    // Skip trailing zeros until first set bit
    while ((dwMask & 1) == 0) {
        dwMask >>= 1;
    }

    // Count consecutive set bits
    while ((dwMask & 1) != 0) {
        dwMask >>= 1;
        ++count;
    }

    return count;
#endif
}

XDWORD GetBitShift(XDWORD dwMask) {
    if (dwMask == 0) return 0;

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    unsigned long index;
    _BitScanForward(&index, dwMask);
    return index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(dwMask);
#else
    XDWORD shift = 0;

    // Count trailing zeros until first set bit
    while ((dwMask & 1) == 0) {
        dwMask >>= 1;
        ++shift;
    }

    return shift;
#endif
}

void VxGetBitCounts(const VxImageDescEx &desc, XDWORD &Rbits, XDWORD &Gbits, XDWORD &Bbits, XDWORD &Abits) {
    Abits = GetBitCount(desc.AlphaMask);
    Rbits = GetBitCount(desc.RedMask);
    Gbits = GetBitCount(desc.GreenMask);
    Bbits = GetBitCount(desc.BlueMask);
}

void VxGetBitShifts(const VxImageDescEx &desc, XDWORD &Rshift, XDWORD &Gshift, XDWORD &Bshift, XDWORD &Ashift) {
    Ashift = GetBitShift(desc.AlphaMask);
    Rshift = GetBitShift(desc.RedMask);
    Gshift = GetBitShift(desc.GreenMask);
    Bshift = GetBitShift(desc.BlueMask);
}

//------------------------------------------------------------------------------
// Pixel format utilities
//------------------------------------------------------------------------------

VX_PIXELFORMAT VxImageDesc2PixelFormat(const VxImageDescEx &desc) {
    return VxBlitEngine::GetPixelFormat(desc);
}

void VxPixelFormat2ImageDesc(VX_PIXELFORMAT Pf, VxImageDescEx &desc) {
    VxBlitEngine::ConvertPixelFormat(Pf, desc);
}

const char *VxPixelFormat2String(VX_PIXELFORMAT Pf) {
    return VxBlitEngine::PixelFormat2String(Pf);
}

void VxBppToMask(VxImageDescEx &desc) {
    switch (desc.BitsPerPixel) {
    case 8:
        // RGB 332 format
        desc.AlphaMask = 0x00;
        desc.RedMask = 0xE0;
        desc.GreenMask = 0x1C;
        desc.BlueMask = 0x03;
        break;
    case 15:
    case 16:
        desc.AlphaMask = 0x0000;
        desc.BlueMask = 0x001F;
        desc.GreenMask = 0x03E0;
        desc.RedMask = 0x7C00;
        break;
    case 24:
        desc.AlphaMask = 0x00000000;
        desc.RedMask = 0x00FF0000;
        desc.GreenMask = 0x0000FF00;
        desc.BlueMask = 0x000000FF;
        break;
    case 32:
        desc.AlphaMask = 0xFF000000;
        desc.RedMask = 0x00FF0000;
        desc.GreenMask = 0x0000FF00;
        desc.BlueMask = 0x000000FF;
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------
// Image blitting and conversion functions
//------------------------------------------------------------------------------

void VxDoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    TheBlitter.DoBlit(src_desc, dst_desc);
}

void VxDoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    TheBlitter.DoBlitUpsideDown(src_desc, dst_desc);
}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue) {
    TheBlitter.DoAlphaBlit(dst_desc, AlphaValue);
}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues) {
    TheBlitter.DoAlphaBlit(dst_desc, AlphaValues);
}

void VxResizeImage32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {
    TheBlitter.ResizeImage(src_desc, dst_desc);
}

void VxGenerateMipMap(const VxImageDescEx &src_desc, XBYTE *Buffer) {
    GetVxGraphicDispatchTable()->generateMipMap(src_desc, Buffer);
}

XBOOL VxConvertToNormalMap(const VxImageDescEx &image, XDWORD ColorMask) {
    return GetVxGraphicDispatchTable()->convertToNormalMap(image, ColorMask);
}

XBOOL VxConvertToBumpMap(const VxImageDescEx &image) {
    return GetVxGraphicDispatchTable()->convertToBumpMap(image);
}
//------------------------------------------------------------------------------
// Color Quantization
//------------------------------------------------------------------------------

// Global quantization sampling factor (1 = best quality)
static int QuantizationSamplingFactor = 15;

int GetQuantizationSamplingFactor() {
    return QuantizationSamplingFactor;
}

void SetQuantizationSamplingFactor(int factor) {
    QuantizationSamplingFactor = factor;
}



