/**
 * @file VxPlane.inl
 * @brief Inline implementations for VxPlane.
 *
 * Part 1: Scalar inline bodies (moved from VxPlane.h).
 * Part 2: SIMD-accelerated plane operations.
 *
 * Included automatically at the bottom of VxPlane.h - do not include directly.
 */

#pragma once

#if defined(VX_SIMD_SSE)
VX_SIMD_INLINE void VxSIMDPlaneCreateFromPoint(VxPlane *plane, const VxVector *normal, const VxVector *point) noexcept;
VX_SIMD_INLINE void VxSIMDPlaneCreateFromTriangle(VxPlane *plane, const VxVector *a, const VxVector *b, const VxVector *c) noexcept;
VX_SIMD_INLINE float VxSIMDPlaneClassifyPoint(const VxPlane *plane, const VxVector *point) noexcept;
VX_SIMD_INLINE float VxSIMDPlaneClassifyAABB(const VxPlane *plane, const VxBbox *box) noexcept;
VX_SIMD_INLINE float VxSIMDPlaneDistance(const VxPlane *plane, const VxVector *point) noexcept;
VX_SIMD_INLINE void VxSIMDPlaneNearestPoint(VxVector *outPoint, const VxPlane *plane, const VxVector *point) noexcept;
VX_SIMD_INLINE float VxSIMDPlaneXClassify(const VxPlane *plane, __m128 axis0V, __m128 axis1V, __m128 axis2V, __m128 centerV) noexcept;
VX_SIMD_INLINE float VxSIMDPlaneXClassify(const VxPlane *plane, const VxVector boxAxis[4]) noexcept;
#endif

// =============================================================================
// Part 1 - Scalar Inline Implementations
// =============================================================================

inline void VxPlaneCreateFromTriangle(VxPlane &plane, const VxVector &a, const VxVector &b, const VxVector &c) {
    const double ax = a.x, ay = a.y, az = a.z;
    const double bx = double(b.x) - ax, by = double(b.y) - ay, bz = double(b.z) - az;
    const double cx = double(c.x) - ax, cy = double(c.y) - ay, cz = double(c.z) - az;
    const double nx = by * cz - bz * cy;
    const double ny = bz * cx - bx * cz;
    const double nz = bx * cy - by * cx;
    const double lengthSquared = nx * nx + ny * ny + nz * nz;
    VxVector normal;
    if (lengthSquared != 0.0) {
        const double inverseLength = 1.0 / sqrt(lengthSquared);
        normal.Set(float(nx * inverseLength), float(ny * inverseLength), float(nz * inverseLength));
    } else {
        normal.Set(0.0f, 0.0f, 1.0f);
    }
    const VxVector anchor{float(ax), float(ay), float(az)};
    plane.Create(normal, anchor);
}

inline VxPlane::VxPlane() : m_Normal(0.0f, 0.0f, 0.0f), m_D(0.0f) {}

inline VxPlane::VxPlane(const VxVector &n, float d) : m_Normal(n), m_D(d) {}

inline VxPlane::VxPlane(float a, float b, float c, float d) : m_Normal(a, b, c), m_D(d) {}

inline VxPlane::VxPlane(const VxVector &n, const VxVector &p) {
    Create(n, p);
}

inline VxPlane::VxPlane(const VxVector &a, const VxVector &b, const VxVector &c) {
    Create(a, b, c);
}

inline const VxPlane operator-(const VxPlane &p) {
    return VxPlane(-p.m_Normal, -p.m_D);
}

inline const VxVector &VxPlane::GetNormal() const {
    return m_Normal;
}

inline float VxPlane::Classify(const VxVector &p) const {
#if defined(VX_SIMD_SSE)
    return VxSIMDPlaneClassifyPoint(this, &p);
#else
    return DotProduct(m_Normal, p) + m_D;
#endif
}

inline float VxPlane::Classify(const VxBbox &box) const {
#if defined(VX_SIMD_SSE)
    return VxSIMDPlaneClassifyAABB(this, &box);
#else
    VxVector nearPoint;
    VxVector farPoint;

    nearPoint.x = (m_Normal.x >= 0.0f) ? box.Min.x : box.Max.x;
    nearPoint.y = (m_Normal.y >= 0.0f) ? box.Min.y : box.Max.y;
    nearPoint.z = (m_Normal.z >= 0.0f) ? box.Min.z : box.Max.z;

    farPoint.x = (m_Normal.x >= 0.0f) ? box.Max.x : box.Min.x;
    farPoint.y = (m_Normal.y >= 0.0f) ? box.Max.y : box.Min.y;
    farPoint.z = (m_Normal.z >= 0.0f) ? box.Max.z : box.Min.z;

    const float nearDist = DotProduct(m_Normal, nearPoint) + m_D;
    if (nearDist > 0.0f) {
        return nearDist;
    }

    const float farDist = DotProduct(m_Normal, farPoint) + m_D;
    if (farDist < 0.0f) {
        return farDist;
    }

    return 0.0f;
#endif
}

inline float VxPlane::Classify(const VxBbox &box, const VxMatrix &mat) const {
    VxVector axis[4];
    axis[0] = mat[0] * ((box.Max.x - box.Min.x) * 0.5f);
    axis[1] = mat[1] * ((box.Max.y - box.Min.y) * 0.5f);
    axis[2] = mat[2] * ((box.Max.z - box.Min.z) * 0.5f);
    VxVector center = box.GetCenter();
    Vx3DMultiplyMatrixVector(axis + 3, mat, &center);
    return XClassify(axis);
}

inline float VxPlane::ClassifyFace(const VxVector &pt0, const VxVector &pt1, const VxVector &pt2) const {
    const float c0 = Classify(pt0);
    const float c1 = Classify(pt1);
    const float c2 = Classify(pt2);

    const bool positive = (c0 > 0.0f) && (c1 > 0.0f) && (c2 > 0.0f);
    if (positive) {
        return XMin(c0, XMin(c1, c2));
    }

    const bool negative = (c0 < 0.0f) && (c1 < 0.0f) && (c2 < 0.0f);
    if (negative) {
        return XMax(c0, XMax(c1, c2));
    }

    return 0.0f;
}

inline float VxPlane::Distance(const VxVector &p) const {
#if defined(VX_SIMD_SSE)
    return VxSIMDPlaneDistance(this, &p);
#else
    return XAbs(Classify(p));
#endif
}

inline const VxVector VxPlane::NearestPoint(const VxVector &p) const {
#if defined(VX_SIMD_SSE)
    VxVector out;
    VxSIMDPlaneNearestPoint(&out, this, &p);
    return out;
#else
    return p - m_Normal * Classify(p);
#endif
}

inline void VxPlane::Create(const VxVector &n, const VxVector &p) {
#if defined(VX_SIMD_SSE)
    VxSIMDPlaneCreateFromPoint(this, &n, &p);
#else
    const float d = float(-(double(n.x) * p.x + double(n.y) * p.y + double(n.z) * p.z));
    m_Normal = n;
    m_D = d;
#endif
}

inline void VxPlane::Create(const VxVector &a, const VxVector &b, const VxVector &c) {
#if defined(VX_SIMD_SSE)
    VxSIMDPlaneCreateFromTriangle(this, &a, &b, &c);
#else
    VxPlaneCreateFromTriangle(*this, a, b, c);
#endif
}

inline bool VxPlane::operator==(const VxPlane &iPlane) const {
    return (m_Normal == iPlane.m_Normal) && (m_D == iPlane.m_D);
}

inline float VxPlane::XClassify(const VxVector boxaxis[4]) const {
#if defined(VX_SIMD_SSE)
    return VxSIMDPlaneXClassify(this, boxaxis);
#else
    const float radius =
        XAbs(DotProduct(m_Normal, boxaxis[0])) +
        XAbs(DotProduct(m_Normal, boxaxis[1])) +
        XAbs(DotProduct(m_Normal, boxaxis[2]));

    const float centerDist = DotProduct(m_Normal, boxaxis[3]) + m_D;
    if (centerDist > radius) {
        return centerDist;
    }
    if (centerDist < -radius) {
        return centerDist;
    }
    return 0.0f;
#endif
}

// =============================================================================
// Part 2 - SIMD-Accelerated Plane Operations
// =============================================================================

#if defined(VX_SIMD_SSE)

#include "VxSIMD.h"

#if defined(VX_SIMD_SSE2)
VX_SIMD_INLINE float VxSIMDPlaneNegativeDot3(__m128 normal, __m128 point) noexcept {
    const __m128d xy = _mm_mul_pd(_mm_cvtps_pd(normal), _mm_cvtps_pd(point));
    const __m128d z = _mm_mul_sd(_mm_cvtps_pd(_mm_movehl_ps(normal, normal)),
                                _mm_cvtps_pd(_mm_movehl_ps(point, point)));
    const __m128d dot = _mm_add_sd(_mm_add_sd(xy, _mm_unpackhi_pd(xy, xy)), z);
    const __m128d negative = _mm_xor_pd(dot, _mm_set_sd(-0.0));
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), negative));
}
#endif

VX_SIMD_INLINE void VxSIMDPlaneCreateFromPoint(VxPlane *plane, const VxVector *normal, const VxVector *point) noexcept {
    const __m128 n = VxSIMDLoadFloat3(&normal->x);
    const __m128 p = VxSIMDLoadFloat3(&point->x);
#if defined(VX_SIMD_SSE2)
    const float d = VxSIMDPlaneNegativeDot3(n, p);
#else
    const float d = -_mm_cvtss_f32(VxSIMDDotProduct3(n, p));
#endif
    VxSIMDStoreFloat3(&plane->m_Normal.x, n);
    plane->m_D = d;
}

VX_SIMD_INLINE void VxSIMDPlaneCreateFromTriangle(VxPlane *plane, const VxVector *a, const VxVector *b, const VxVector *c) noexcept {
    const __m128 va = VxSIMDLoadFloat3(&a->x);
    const __m128 vb = VxSIMDLoadFloat3(&b->x);
    const __m128 vc = VxSIMDLoadFloat3(&c->x);
    __m128 normal;
#if defined(VX_SIMD_SSE2)
    const __m128d axy = _mm_cvtps_pd(va);
    const __m128d az = _mm_cvtps_pd(_mm_movehl_ps(va, va));
    const __m128d bxy = _mm_sub_pd(_mm_cvtps_pd(vb), axy);
    const __m128d bz = _mm_sub_pd(_mm_cvtps_pd(_mm_movehl_ps(vb, vb)), az);
    const __m128d cxy = _mm_sub_pd(_mm_cvtps_pd(vc), axy);
    const __m128d cz = _mm_sub_pd(_mm_cvtps_pd(_mm_movehl_ps(vc, vc)), az);
    const __m128d nxy = _mm_sub_pd(
        _mm_mul_pd(_mm_shuffle_pd(bxy, bz, 1), _mm_shuffle_pd(cz, cxy, 0)),
        _mm_mul_pd(_mm_shuffle_pd(bz, bxy, 0), _mm_shuffle_pd(cxy, cz, 1)));
    const __m128d zProducts = _mm_mul_pd(bxy, _mm_shuffle_pd(cxy, cxy, 1));
    const __m128d nz = _mm_unpacklo_pd(_mm_sub_sd(zProducts, _mm_unpackhi_pd(zProducts, zProducts)),
                                      _mm_setzero_pd());
    const __m128d xySquared = _mm_mul_pd(nxy, nxy);
    const __m128d lengthSquared = _mm_add_sd(
        _mm_add_sd(xySquared, _mm_unpackhi_pd(xySquared, xySquared)), _mm_mul_sd(nz, nz));
    if (_mm_cvtsd_f64(lengthSquared) != 0.0) {
        const __m128d inverse = _mm_div_sd(_mm_set_sd(1.0), _mm_sqrt_sd(lengthSquared, lengthSquared));
        const __m128 xy = _mm_cvtpd_ps(_mm_mul_pd(nxy, _mm_unpacklo_pd(inverse, inverse)));
        const __m128 z = _mm_cvtsd_ss(_mm_setzero_ps(), _mm_mul_sd(nz, inverse));
        normal = _mm_movelh_ps(xy, z);
    } else {
        normal = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
    }
    const float d = VxSIMDPlaneNegativeDot3(normal, va);
#else
    normal = VxSIMDCrossProduct3(_mm_sub_ps(vb, va), _mm_sub_ps(vc, va));
    const __m128 lengthSquared = VxSIMDDotProduct3(normal, normal);
    if (_mm_cvtss_f32(lengthSquared) != 0.0f) {
        normal = _mm_mul_ps(normal, VxSIMDReciprocalSqrtAccurate(lengthSquared));
    } else {
        normal = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
    }
    const float d = -_mm_cvtss_f32(VxSIMDDotProduct3(normal, va));
#endif
    VxSIMDStoreFloat3(&plane->m_Normal.x, normal);
    plane->m_D = d;
}

VX_SIMD_INLINE float VxSIMDPlaneClassifyPoint(const VxPlane *plane, const VxVector *point) noexcept {
    const __m128 n = VxSIMDLoadFloat3(&plane->m_Normal.x);
    const __m128 p = VxSIMDLoadFloat3(&point->x);
    __m128 dist = VxSIMDDotProduct3(n, p);
    dist = _mm_add_ss(dist, _mm_set_ss(plane->m_D));
    float out;
    _mm_store_ss(&out, dist);
    return out;
}

VX_SIMD_INLINE float VxSIMDPlaneClassifyAABB(const VxPlane *plane, const VxBbox *box) noexcept {
    const __m128 n = VxSIMDLoadFloat3(&plane->m_Normal.x);
    const __m128 minV = VxSIMDLoadFloat3(&box->Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&box->Max.x);
    const __m128 zero = _mm_setzero_ps();
    const __m128 geMask = _mm_cmpge_ps(n, zero);

    const __m128 nearP = _mm_or_ps(_mm_and_ps(geMask, minV), _mm_andnot_ps(geMask, maxV));
    const __m128 farP = _mm_or_ps(_mm_and_ps(geMask, maxV), _mm_andnot_ps(geMask, minV));
    const __m128 d = _mm_set_ss(plane->m_D);

    __m128 nearDist = _mm_add_ss(VxSIMDDotProduct3(n, nearP), d);
    float nearOut;
    _mm_store_ss(&nearOut, nearDist);
    if (nearOut > 0.0f) {
        return nearOut;
    }

    __m128 farDist = _mm_add_ss(VxSIMDDotProduct3(n, farP), d);
    float farOut;
    _mm_store_ss(&farOut, farDist);
    if (farOut < 0.0f) {
        return farOut;
    }

    return 0.0f;
}

VX_SIMD_INLINE float VxSIMDPlaneDistance(const VxPlane *plane, const VxVector *point) noexcept {
    const __m128 n = VxSIMDLoadFloat3(&plane->m_Normal.x);
    const __m128 p = VxSIMDLoadFloat3(&point->x);
    __m128 dist = _mm_add_ss(VxSIMDDotProduct3(n, p), _mm_set_ss(plane->m_D));
    dist = _mm_and_ps(dist, VX_SIMD_ABS_MASK);
    return _mm_cvtss_f32(dist);
}

VX_SIMD_INLINE void VxSIMDPlaneNearestPoint(VxVector *outPoint, const VxPlane *plane, const VxVector *point) noexcept {
    const __m128 p = VxSIMDLoadFloat3(&point->x);
    const __m128 n = VxSIMDLoadFloat3(&plane->m_Normal.x);
    __m128 dist = _mm_add_ss(VxSIMDDotProduct3(n, p), _mm_set_ss(plane->m_D));
    dist = _mm_shuffle_ps(dist, dist, _MM_SHUFFLE(0, 0, 0, 0));
    const __m128 projected = _mm_sub_ps(p, _mm_mul_ps(n, dist));
    VxSIMDStoreFloat3(&outPoint->x, projected);
}

VX_SIMD_INLINE float VxSIMDPlaneXClassify(const VxPlane *plane, __m128 axis0V, __m128 axis1V, __m128 axis2V, __m128 centerV) noexcept {
    const __m128 n = VxSIMDLoadFloat3(&plane->m_Normal.x);
    __m128 radius = _mm_and_ps(VxSIMDDotProduct3(n, axis0V), VX_SIMD_ABS_MASK);
    radius = _mm_add_ss(radius, _mm_and_ps(VxSIMDDotProduct3(n, axis1V), VX_SIMD_ABS_MASK));
    radius = _mm_add_ss(radius, _mm_and_ps(VxSIMDDotProduct3(n, axis2V), VX_SIMD_ABS_MASK));

    __m128 centerDist = _mm_add_ss(VxSIMDDotProduct3(n, centerV), _mm_set_ss(plane->m_D));

    float r;
    float d;
    _mm_store_ss(&r, radius);
    _mm_store_ss(&d, centerDist);

    if (d > r) {
        return d;
    }
    if (d < -r) {
        return d;
    }
    return 0.0f;
}

VX_SIMD_INLINE float VxSIMDPlaneXClassify(const VxPlane *plane, const VxVector boxAxis[4]) noexcept {
    return VxSIMDPlaneXClassify(
        plane,
        VxSIMDLoadFloat3(&boxAxis[0].x),
        VxSIMDLoadFloat3(&boxAxis[1].x),
        VxSIMDLoadFloat3(&boxAxis[2].x),
        VxSIMDLoadFloat3(&boxAxis[3].x)
    );
}

#endif // VX_SIMD_SSE
