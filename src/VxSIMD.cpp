#include <cstdlib>

#include "VxSIMD.h"

namespace {

bool EqualsIgnoreCase(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

VxSIMDBackend PickBestBackendFromFeatures() noexcept {
#if defined(VX_SIMD_NONE)
    return VxSIMDBackend::Scalar;
#else
    const VxSIMDFeatures features = VxGetSIMDFeatures();
#if defined(VX_SIMD_AVX2)
    if (features.AVX2) {
        return VxSIMDBackend::AVX2;
    }
#endif
#if defined(VX_SIMD_SSE2)
    if (features.SSE2) {
        return VxSIMDBackend::SSE2;
    }
#endif
    return VxSIMDBackend::Scalar;
#endif
}

} // namespace

VxSIMDBackend VxGetActiveSIMDBackend() {
    const char *env = std::getenv("VXMATH_SIMD_FORCE");
    if (env == nullptr || *env == '\0' || EqualsIgnoreCase(env, "auto")) {
        return PickBestBackendFromFeatures();
    }
    if (EqualsIgnoreCase(env, "scalar")) {
        return VxSIMDBackend::Scalar;
    }
    if (EqualsIgnoreCase(env, "sse2")) {
#if defined(VX_SIMD_SSE2)
        return VxSIMDBackend::SSE2;
#else
        return VxSIMDBackend::Scalar;
#endif
    }
    if (EqualsIgnoreCase(env, "avx2")) {
#if defined(VX_SIMD_AVX2)
        const VxSIMDFeatures features = VxGetSIMDFeatures();
        if (features.AVX2) {
            return VxSIMDBackend::AVX2;
        }
#endif
#if defined(VX_SIMD_SSE2)
        return VxSIMDBackend::SSE2;
#else
        return VxSIMDBackend::Scalar;
#endif
    }
    return PickBestBackendFromFeatures();
}
