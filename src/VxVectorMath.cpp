#include "VxVector.h"

#if !VXMATH_ENABLE_X87_EMULATION
float Magnitude(const VxVector &v) { return v.Magnitude(); }

const VxVector Normalize(const VxVector &v) {
#if defined(VX_SIMD_SSE)
    const __m128 value = VxSIMDLoadFloat3(&v.x);
    const int zero = _mm_movemask_ps(_mm_cmpeq_ps(value, _mm_setzero_ps()));
    const int unordered = _mm_movemask_ps(_mm_cmpunord_ps(value, value));
    VxVector result;
    if ((zero & 7) == 7 || (unordered & 7) != 0) {
        VxSIMDStoreFloat3(&result.x, _mm_div_ps(value, _mm_sqrt_ps(VxSIMDDotProduct3(value, value))));
    } else {
        VxSIMDStoreFloat3(&result.x, VxSIMDNormalize3(value));
    }
    return result;
#else
    const double x = v.x, y = v.y, z = v.z;
    const double length = sqrt((x * x + y * y) + z * z);
    return VxVector(static_cast<float>(x / length), static_cast<float>(y / length), static_cast<float>(z / length));
#endif
}
#endif

const VxVector Normalize(const VxVector &v, float length) {
#if VXMATH_ENABLE_X87_EMULATION
    return Normalize(v) * length;
#else
    const double x = v.x, y = v.y, z = v.z;
    const double magnitude = sqrt((x * x + y * y) + z * z);
    if (!(magnitude > 0.0))
        return v;
    const double factor = length / magnitude;
    return VxVector(static_cast<float>(x * factor), static_cast<float>(y * factor), static_cast<float>(z * factor));
#endif
}
