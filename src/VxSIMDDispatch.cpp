#include "VxSIMDDispatchInternal.h"

#include "VxAtomicOpsInternal.h"
#include "VxBlitDispatchBridge.h"

#include "VxMath.h"
#include "VxMutex.h"
#include "VxSIMD.h"

namespace {

struct SIMDDispatchState {
    VxMutex lock;
    volatile int requestedMode;
    volatile int effectiveMode;
    volatile int generation;
    bool initialized;

    SIMDDispatchState()
        : requestedMode(VX_SIMD_MODE_AUTO),
          effectiveMode(VX_SIMD_MODE_NONE),
          generation(0),
          initialized(false) {}
};

SIMDDispatchState &GetSIMDDispatchState() {
    static SIMDDispatchState state;
    return state;
}

bool IsValidSIMDMode(int mode) {
    return mode == VX_SIMD_MODE_AUTO ||
           mode == VX_SIMD_MODE_NONE ||
           mode == VX_SIMD_MODE_SSE2 ||
           mode == VX_SIMD_MODE_SSSE3 ||
           mode == VX_SIMD_MODE_SSE4_1 ||
           mode == VX_SIMD_MODE_AVX ||
           mode == VX_SIMD_MODE_AVX2;
}

bool IsSIMDModeAvailable(int mode, const VxSIMDFeatures &features) {
    switch (mode) {
        case VX_SIMD_MODE_NONE:
            return true;

        case VX_SIMD_MODE_SSE2:
#if defined(VX_SIMD_SSE2)
            return features.SSE2;
#else
            return false;
#endif

        case VX_SIMD_MODE_SSSE3:
#if defined(VX_SIMD_SSE2)
            return features.SSE2 && features.SSSE3;
#else
            return false;
#endif

        case VX_SIMD_MODE_SSE4_1:
#if defined(VX_SIMD_SSE2)
            return features.SSE2 && features.SSE4_1;
#else
            return false;
#endif

        case VX_SIMD_MODE_AVX:
#if defined(VX_SIMD_SSE2)
            return features.SSE2 && features.AVX;
#else
            return false;
#endif

        case VX_SIMD_MODE_AVX2:
#if defined(VX_SIMD_AVX2)
            return features.AVX2;
#else
            return false;
#endif

        default:
            return false;
    }
}

int ResolveSIMDModeFromChain(const int *chain, int count, const VxSIMDFeatures &features) {
    for (int i = 0; i < count; ++i) {
        if (IsSIMDModeAvailable(chain[i], features)) {
            return chain[i];
        }
    }
    return VX_SIMD_MODE_NONE;
}

int ResolveSIMDMode(int requestedMode, const VxSIMDFeatures &features) {
    if (!IsValidSIMDMode(requestedMode)) {
        requestedMode = VX_SIMD_MODE_AUTO;
    }

    if (requestedMode == VX_SIMD_MODE_AUTO) {
        static const int kAutoChain[] = {
            VX_SIMD_MODE_AVX2,
            VX_SIMD_MODE_AVX,
            VX_SIMD_MODE_SSE4_1,
            VX_SIMD_MODE_SSSE3,
            VX_SIMD_MODE_SSE2,
            VX_SIMD_MODE_NONE
        };
        return ResolveSIMDModeFromChain(kAutoChain, sizeof(kAutoChain) / sizeof(kAutoChain[0]), features);
    }

    if (requestedMode == VX_SIMD_MODE_NONE) {
        return VX_SIMD_MODE_NONE;
    }

    if (requestedMode == VX_SIMD_MODE_SSE2) {
        static const int kChain[] = {VX_SIMD_MODE_SSE2, VX_SIMD_MODE_NONE};
        return ResolveSIMDModeFromChain(kChain, sizeof(kChain) / sizeof(kChain[0]), features);
    }

    if (requestedMode == VX_SIMD_MODE_SSSE3) {
        static const int kChain[] = {
            VX_SIMD_MODE_SSSE3,
            VX_SIMD_MODE_SSE2,
            VX_SIMD_MODE_NONE
        };
        return ResolveSIMDModeFromChain(kChain, sizeof(kChain) / sizeof(kChain[0]), features);
    }

    if (requestedMode == VX_SIMD_MODE_SSE4_1) {
        static const int kChain[] = {
            VX_SIMD_MODE_SSE4_1,
            VX_SIMD_MODE_SSSE3,
            VX_SIMD_MODE_SSE2,
            VX_SIMD_MODE_NONE
        };
        return ResolveSIMDModeFromChain(kChain, sizeof(kChain) / sizeof(kChain[0]), features);
    }

    if (requestedMode == VX_SIMD_MODE_AVX) {
        static const int kChain[] = {
            VX_SIMD_MODE_AVX,
            VX_SIMD_MODE_SSE4_1,
            VX_SIMD_MODE_SSSE3,
            VX_SIMD_MODE_SSE2,
            VX_SIMD_MODE_NONE
        };
        return ResolveSIMDModeFromChain(kChain, sizeof(kChain) / sizeof(kChain[0]), features);
    }

    if (requestedMode == VX_SIMD_MODE_AVX2) {
        static const int kChain[] = {
            VX_SIMD_MODE_AVX2,
            VX_SIMD_MODE_AVX,
            VX_SIMD_MODE_SSE4_1,
            VX_SIMD_MODE_SSSE3,
            VX_SIMD_MODE_SSE2,
            VX_SIMD_MODE_NONE
        };
        return ResolveSIMDModeFromChain(kChain, sizeof(kChain) / sizeof(kChain[0]), features);
    }

    return VX_SIMD_MODE_NONE;
}

void RebuildAllLocked(SIMDDispatchState &state) {
    const int effectiveMode = VxAtomicLoadInt(&state.effectiveMode);
    VxMathDispatchRebuild(effectiveMode);
    VxDistanceDispatchRebuild(effectiveMode);
    VxIntersectDispatchRebuild(effectiveMode);
    VxGraphicDispatchRebuild(effectiveMode);
    VxBlitApplySIMDMode(effectiveMode);
    VxAtomicIncrementInt(&state.generation);
}

void EnsureInitializedLocked(SIMDDispatchState &state) {
    if (state.initialized) {
        return;
    }

    const int requested = VxAtomicLoadInt(&state.requestedMode);
    const int effective = ResolveSIMDMode(requested, VxGetSIMDFeatures());
    VxAtomicStoreInt(&state.effectiveMode, effective);
    state.initialized = true;
}

void EnsureInitialized() {
    SIMDDispatchState &state = GetSIMDDispatchState();
    if (state.initialized) {
        return;
    }
    VxMutexLock lock(state.lock);
    EnsureInitializedLocked(state);
}

} // namespace

void VxSIMDDispatchInitialize() {
    SIMDDispatchState &state = GetSIMDDispatchState();
    VxMutexLock lock(state.lock);
    EnsureInitializedLocked(state);
    RebuildAllLocked(state);
}

void VxSIMDDispatchRebuildAll() {
    SIMDDispatchState &state = GetSIMDDispatchState();
    VxMutexLock lock(state.lock);
    EnsureInitializedLocked(state);
    RebuildAllLocked(state);
}

int VxGetSIMDOverrideInternal() {
    EnsureInitialized();
    return VxAtomicLoadInt(&GetSIMDDispatchState().requestedMode);
}

int VxGetSIMDEffectiveBackendInternal() {
    EnsureInitialized();
    return VxAtomicLoadInt(&GetSIMDDispatchState().effectiveMode);
}

XBOOL VxSetSIMDOverride(int mode) {
    if (!IsValidSIMDMode(mode)) {
        return FALSE;
    }

    SIMDDispatchState &state = GetSIMDDispatchState();
    VxMutexLock lock(state.lock);
    EnsureInitializedLocked(state);

    if (VxAtomicLoadInt(&state.requestedMode) == mode) {
        return TRUE;
    }

    VxAtomicStoreInt(&state.requestedMode, mode);
    VxAtomicStoreInt(&state.effectiveMode, ResolveSIMDMode(mode, VxGetSIMDFeatures()));
    RebuildAllLocked(state);
    return TRUE;
}

int VxGetSIMDOverride() {
    return VxGetSIMDOverrideInternal();
}

int VxGetSIMDEffectiveBackend() {
    return VxGetSIMDEffectiveBackendInternal();
}

const char *VxGetSIMDBackendName(int mode) {
    switch (mode) {
        case VX_SIMD_MODE_AUTO:
            return "auto";
        case VX_SIMD_MODE_NONE:
            return "none";
        case VX_SIMD_MODE_SSE2:
            return "sse2";
        case VX_SIMD_MODE_SSSE3:
            return "ssse3";
        case VX_SIMD_MODE_SSE4_1:
            return "sse4_1";
        case VX_SIMD_MODE_AVX:
            return "avx";
        case VX_SIMD_MODE_AVX2:
            return "avx2";
        default:
            return "unknown";
    }
}
