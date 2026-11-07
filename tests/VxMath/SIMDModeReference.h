#ifndef SIMDMODEREFERENCE_H
#define SIMDMODEREFERENCE_H

#include "VxSIMD.h"
#include "VxSIMDMode.h"

// A requested mode may fall back. Native modes require CPU support, whereas
// portable modes require the corresponding API to have been compiled in.
static bool ReferenceSIMDModeAvailable(int mode) {
#if defined(VX_SIMD_USE_SIMDE)
    switch (mode) {
        case VX_SIMD_MODE_NONE: return true;
#if defined(VX_SIMD_SSE2)
        case VX_SIMD_MODE_SSE2: return true;
#endif
#if defined(VX_SIMD_SSSE3)
        case VX_SIMD_MODE_SSSE3: return true;
#endif
#if defined(VX_SIMD_SSE4_1)
        case VX_SIMD_MODE_SSE4_1: return true;
#endif
#if defined(VX_SIMD_AVX)
        case VX_SIMD_MODE_AVX: return true;
#endif
#if defined(VX_SIMD_AVX2)
        case VX_SIMD_MODE_AVX2: return true;
#endif
        default: return false;
    }
#else
    const VxSIMDFeatures &features = VxGetSIMDFeatures();
    switch (mode) {
        case VX_SIMD_MODE_NONE: return true;
        case VX_SIMD_MODE_SSE2: return features.SSE2;
        case VX_SIMD_MODE_SSSE3: return features.SSSE3;
        case VX_SIMD_MODE_SSE4_1: return features.SSE4_1;
        case VX_SIMD_MODE_AVX: return features.AVX;
        case VX_SIMD_MODE_AVX2: return features.AVX2;
        default: return false;
    }
#endif
}

static int ReferenceSIMDMode(int requested) {
    const int levels[] = {VX_SIMD_MODE_AVX2, VX_SIMD_MODE_AVX, VX_SIMD_MODE_SSE4_1,
                          VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_NONE};
    for (int level : levels) {
        if ((requested == VX_SIMD_MODE_AUTO || level <= requested) && ReferenceSIMDModeAvailable(level))
            return level;
    }
    return VX_SIMD_MODE_NONE;
}

#endif // SIMDMODEREFERENCE_H
