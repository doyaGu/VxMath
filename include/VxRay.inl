/**
 * @file VxRay.inl
 * @brief Inline implementations for VxRay.
 *
 * Part 1: Scalar inline bodies (moved from VxRay.h).
 * Part 2: SIMD-accelerated ray operations.
 *
 * Included automatically at the bottom of VxRay.h - do not include directly.
 */

#pragma once

#if defined(VX_SIMD_SSE)

#include "VxSIMD.h"

VX_SIMD_INLINE void VxSIMDRayTransform(VxRay *dest, const VxRay *ray, const VxMatrix *mat) noexcept;
VX_SIMD_INLINE void VxSIMDRayInterpolate(VxVector *point, const VxRay *ray, float t) noexcept;
VX_SIMD_INLINE float VxSIMDRaySquareDistance(const VxRay *ray, const VxVector *point) noexcept;
#if defined(VX_SIMD_SSE2)
VX_SIMD_INLINE __m128d VxSIMDRaySquareDistanceDouble(const VxRay *ray, const VxVector *point) noexcept;
#endif

#endif

// =============================================================================
// Part 1 - Scalar Inline Implementations
// =============================================================================

// The cross product avoids subtracting nearly equal squared lengths.
inline double VxRaySquareDistanceDouble(const VxRay &ray, const VxVector &point) {
    const double x = double(point.x) - ray.m_Origin.x;
    const double y = double(point.y) - ray.m_Origin.y;
    const double z = double(point.z) - ray.m_Origin.z;
    const double dx = ray.m_Direction.x, dy = ray.m_Direction.y, dz = ray.m_Direction.z;
    const double directionSquared = dx * dx + dy * dy + dz * dz;
    if (directionSquared == 0.0) {
        return x * x + y * y + z * z;
    }
    const double cx = y * dz - z * dy;
    const double cy = z * dx - x * dz;
    const double cz = x * dy - y * dx;
    return (cx * cx + cy * cy + cz * cz) / directionSquared;
}

inline VxRay::VxRay() : m_Origin(0.0f, 0.0f, 0.0f), m_Direction(0.0f, 0.0f, 0.0f) {}

inline VxRay::VxRay(const VxVector &start, const VxVector &end) : m_Origin(start), m_Direction(end - start) {}

inline VxRay::VxRay(const VxVector &start, const VxVector &dir, int *dummy) : m_Origin(start), m_Direction(dir) {
    (void) dummy;
}

inline void VxRay::Transform(VxRay &dest, const VxMatrix &mat) {
#if defined(VX_SIMD_SSE)
    VxSIMDRayTransform(&dest, this, &mat);
#else
    Vx3DMultiplyMatrixVector(&dest.m_Origin, mat, &m_Origin);
    Vx3DRotateVector(&dest.m_Direction, mat, &m_Direction);
#endif
}

inline void VxRay::Interpolate(VxVector &p, float t) const {
#if defined(VX_SIMD_SSE)
    VxSIMDRayInterpolate(&p, this, t);
#else
    p = m_Origin + m_Direction * t;
#endif
}

inline float VxRay::SquareDistance(const VxVector &p) const {
#if defined(VX_SIMD_SSE)
    return VxSIMDRaySquareDistance(this, &p);
#else
    return float(VxRaySquareDistanceDouble(*this, p));
#endif
}

inline float VxRay::Distance(const VxVector &p) const {
#if defined(VX_SIMD_SSE2)
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(),
        _mm_sqrt_sd(_mm_setzero_pd(), VxSIMDRaySquareDistanceDouble(this, &p))));
#elif defined(VX_SIMD_SSE)
    return sqrtf(VxSIMDRaySquareDistance(this, &p));
#else
    return float(sqrt(VxRaySquareDistanceDouble(*this, p)));
#endif
}

inline bool VxRay::operator==(const VxRay &iRay) const {
    return (m_Origin == iRay.m_Origin) && (m_Direction == iRay.m_Direction);
}

inline const VxVector &VxRay::GetOrigin() const {
    return m_Origin;
}

inline VxVector &VxRay::GetOrigin() {
    return m_Origin;
}

inline const VxVector &VxRay::GetDirection() const {
    return m_Direction;
}

inline VxVector &VxRay::GetDirection() {
    return m_Direction;
}

// =============================================================================
// Part 2 - SIMD-Accelerated Ray Operations
// =============================================================================

#if defined(VX_SIMD_SSE)

VX_SIMD_INLINE void VxSIMDRayTransform(VxRay *dest, const VxRay *ray, const VxMatrix *mat) noexcept {
    VxSIMDMultiplyMatrixVector(&dest->m_Origin, mat, &ray->m_Origin);
    VxSIMDRotateVector(&dest->m_Direction, mat, &ray->m_Direction);
}

VX_SIMD_INLINE void VxSIMDRayInterpolate(VxVector *point, const VxRay *ray, float t) noexcept {
    __m128 origin = VxSIMDLoadFloat3(&ray->m_Origin.x);
    __m128 dir = VxSIMDLoadFloat3(&ray->m_Direction.x);
    __m128 tv = _mm_set1_ps(t);
    __m128 out = VX_FMADD_PS(dir, tv, origin);
    VxSIMDStoreFloat3(&point->x, out);
}

#if defined(VX_SIMD_SSE2)
VX_SIMD_INLINE __m128d VxSIMDRaySquareDistanceDouble(const VxRay *ray, const VxVector *point) noexcept {
    const __m128 p = VxSIMDLoadFloat3(&point->x);
    const __m128 o = VxSIMDLoadFloat3(&ray->m_Origin.x);
    const __m128 d = VxSIMDLoadFloat3(&ray->m_Direction.x);
    const __m128d xy = _mm_sub_pd(_mm_cvtps_pd(p), _mm_cvtps_pd(o));
    const __m128d z = _mm_sub_pd(_mm_cvtps_pd(_mm_movehl_ps(p, p)), _mm_cvtps_pd(_mm_movehl_ps(o, o)));
    const __m128d dxy = _mm_cvtps_pd(d);
    const __m128d dz = _mm_cvtps_pd(_mm_movehl_ps(d, d));
    const __m128d directionXY = _mm_mul_pd(dxy, dxy);
    const __m128d directionSquared = _mm_add_sd(
        _mm_add_sd(directionXY, _mm_unpackhi_pd(directionXY, directionXY)), _mm_mul_sd(dz, dz));
    if (_mm_cvtsd_f64(directionSquared) == 0.0) {
        const __m128d xySquared = _mm_mul_pd(xy, xy);
        return _mm_add_sd(_mm_add_sd(xySquared, _mm_unpackhi_pd(xySquared, xySquared)), _mm_mul_sd(z, z));
    }
    const __m128d crossXY = _mm_sub_pd(
        _mm_mul_pd(_mm_shuffle_pd(xy, z, 1), _mm_shuffle_pd(dz, dxy, 0)),
        _mm_mul_pd(_mm_shuffle_pd(z, xy, 0), _mm_shuffle_pd(dxy, dz, 1)));
    const __m128d zProducts = _mm_mul_pd(xy, _mm_shuffle_pd(dxy, dxy, 1));
    const __m128d crossZ = _mm_sub_sd(zProducts, _mm_unpackhi_pd(zProducts, zProducts));
    const __m128d crossSquared = _mm_mul_pd(crossXY, crossXY);
    const __m128d numerator = _mm_add_sd(
        _mm_add_sd(crossSquared, _mm_unpackhi_pd(crossSquared, crossSquared)), _mm_mul_sd(crossZ, crossZ));
    return _mm_div_sd(numerator, directionSquared);
}
#endif

VX_SIMD_INLINE float VxSIMDRaySquareDistance(const VxRay *ray, const VxVector *point) noexcept {
#if defined(VX_SIMD_SSE2)
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), VxSIMDRaySquareDistanceDouble(ray, point)));
#else
    const __m128 p = VxSIMDLoadFloat3(&point->x);
    const __m128 o = VxSIMDLoadFloat3(&ray->m_Origin.x);
    const __m128 d = VxSIMDLoadFloat3(&ray->m_Direction.x);
    const __m128 v = _mm_sub_ps(p, o);
    const __m128 directionSquared = VxSIMDDotProduct3(d, d);
    if (_mm_cvtss_f32(directionSquared) == 0.0f) {
        return _mm_cvtss_f32(VxSIMDDotProduct3(v, v));
    }
    const __m128 cross = VxSIMDCrossProduct3(v, d);
    return _mm_cvtss_f32(_mm_div_ss(VxSIMDDotProduct3(cross, cross), directionSquared));
#endif
}

#endif // VX_SIMD_SSE
