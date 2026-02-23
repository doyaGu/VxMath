/**
 * @file VxQuaternion.inl
 * @brief Inline implementations for VxQuaternion.
 *
 * Part 1: Scalar inline bodies (moved from VxQuaternion.h).
 * Part 2: SIMD-accelerated high-level operations on VxQuaternion.
 *
 * Included automatically at the bottom of VxQuaternion.h - do not include directly.
 */

#pragma once

#include "VxSIMD.h"

#if defined(VX_SIMD_SSE)
VX_SIMD_INLINE float VxSIMDDotQuaternion(const VxQuaternion *a, const VxQuaternion *b) noexcept;
VX_SIMD_INLINE float VxSIMDMagnitudeQuaternion(const VxQuaternion *q) noexcept;
VX_SIMD_INLINE void VxSIMDConjugateQuaternion(VxQuaternion *result, const VxQuaternion *q) noexcept;
VX_SIMD_INLINE void VxSIMDDivideQuaternion(VxQuaternion *result, const VxQuaternion *p, const VxQuaternion *q) noexcept;
VX_SIMD_INLINE void VxSIMDScaleQuaternion(VxQuaternion *result, const VxQuaternion *q, float scale) noexcept;
VX_SIMD_INLINE void VxSIMDNormalizeQuaternion(VxQuaternion *q) noexcept;
VX_SIMD_INLINE void VxSIMDMultiplyQuaternion(VxQuaternion *result, const VxQuaternion *a, const VxQuaternion *b) noexcept;
VX_SIMD_INLINE void VxSIMDSlerpQuaternion(VxQuaternion *result, float t, const VxQuaternion *a, const VxQuaternion *b) noexcept;
#endif

// =============================================================================
// Part 1 - Scalar Inline Implementations
// =============================================================================

inline VxQuaternion::VxQuaternion() {
    x = y = z = 0.0f;
    w = 1.0f;
}

inline VxQuaternion::VxQuaternion(const VxVector &Vector, float Angle) {
    FromRotation(Vector, Angle);
}

inline VxQuaternion::VxQuaternion(float X, float Y, float Z, float W) {
    x = X;
    y = Y;
    z = Z;
    w = W;
}

inline VxQuaternion VxQuaternion::operator+(const VxQuaternion &q) const {
    return VxQuaternion(x + q.x, y + q.y, z + q.z, w + q.w);
}

inline VxQuaternion VxQuaternion::operator-(const VxQuaternion &q) const {
    return VxQuaternion(x - q.x, y - q.y, z - q.z, w - q.w);
}

inline VxQuaternion VxQuaternion::operator*(const VxQuaternion &q) const {
    return Vx3DQuaternionMultiply(*this, q);
}

inline VxQuaternion VxQuaternion::operator/(const VxQuaternion &q) const {
    return Vx3DQuaternionDivide(*this, q);
}

inline VxQuaternion &VxQuaternion::operator*=(float s) {
    x *= s;
    y *= s;
    z *= s;
    w *= s;
    return *this;
}

inline VxQuaternion VxQuaternion::operator-() const {
    return VxQuaternion(-x, -y, -z, -w);
}

inline VxQuaternion VxQuaternion::operator+() const {
    return *this;
}

inline int operator==(const VxQuaternion &q1, const VxQuaternion &q2) {
#if defined(VX_SIMD_SSE)
    __m128 a = VxSIMDLoadFloat4(&q1.x);
    __m128 b = VxSIMDLoadFloat4(&q2.x);
    return (_mm_movemask_ps(_mm_cmpeq_ps(a, b)) == 0xF) ? 1 : 0;
#else
    return (q1.x == q2.x && q1.y == q2.y && q1.z == q2.z && q1.w == q2.w);
#endif
}

inline int operator!=(const VxQuaternion &q1, const VxQuaternion &q2) {
    return (q1.x != q2.x || q1.y != q2.y || q1.z != q2.z || q1.w != q2.w);
}

inline VxQuaternion operator*(float s, const VxQuaternion &q) {
    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    VxSIMDScaleQuaternion(&result, &q, s);
#else
    result = VxQuaternion(q.x * s, q.y * s, q.z * s, q.w * s);
#endif
    return result;
}

inline VxQuaternion operator*(const VxQuaternion &q, float s) {
    return s * q;
}

inline float Magnitude(const VxQuaternion &q) {
#if defined(VX_SIMD_SSE)
    return VxSIMDMagnitudeQuaternion(&q);
#else
    return (q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
#endif
}

inline float DotProduct(const VxQuaternion &q1, const VxQuaternion &q2) {
#if defined(VX_SIMD_SSE)
    return VxSIMDDotQuaternion(&q1, &q2);
#else
    return (q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w);
#endif
}

inline const float &VxQuaternion::operator[](int i) const {
    return *((&x) + i);
}

inline float &VxQuaternion::operator[](int i) {
    return *((&x) + i);
}

inline VxQuaternion Vx3DQuaternionConjugate(const VxQuaternion &Quat) {
    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    VxSIMDConjugateQuaternion(&result, &Quat);
#else
    result = VxQuaternion(-Quat.x, -Quat.y, -Quat.z, Quat.w);
#endif
    return result;
}

inline VxQuaternion Vx3DQuaternionMultiply(const VxQuaternion &QuatL, const VxQuaternion &QuatR) {
    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    VxSIMDMultiplyQuaternion(&result, &QuatL, &QuatR);
#else
    result = VxQuaternion(
        QuatL.w * QuatR.x + QuatL.x * QuatR.w + QuatL.y * QuatR.z - QuatL.z * QuatR.y,
        QuatL.w * QuatR.y - QuatL.x * QuatR.z + QuatL.y * QuatR.w + QuatL.z * QuatR.x,
        QuatL.w * QuatR.z + QuatL.x * QuatR.y - QuatL.y * QuatR.x + QuatL.z * QuatR.w,
        QuatL.w * QuatR.w - QuatL.x * QuatR.x - QuatL.y * QuatR.y - QuatL.z * QuatR.z
    );
#endif
    return result;
}

inline VxQuaternion Vx3DQuaternionDivide(const VxQuaternion &P, const VxQuaternion &Q) {
    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    VxSIMDDivideQuaternion(&result, &P, &Q);
#else
    float newX = -P.w * Q.x + P.x * Q.w - P.y * Q.z + P.z * Q.y;
    float newY = -P.w * Q.y + P.x * Q.z + P.y * Q.w - P.z * Q.x;
    float newZ = -P.w * Q.z - P.x * Q.y + P.y * Q.x + P.z * Q.w;
    float newW = P.w * Q.w + P.x * Q.x + P.y * Q.y + P.z * Q.z;
    result = VxQuaternion(newX, newY, newZ, newW);
#endif
    return result;
}

inline void VxQuaternion::Multiply(const VxQuaternion &Quat) {
    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    VxSIMDMultiplyQuaternion(&result, this, &Quat);
#else
    float newX = w * Quat.x + x * Quat.w + y * Quat.z - z * Quat.y;
    float newY = w * Quat.y - x * Quat.z + y * Quat.w + z * Quat.x;
    float newZ = w * Quat.z + x * Quat.y - y * Quat.x + z * Quat.w;
    float newW = w * Quat.w - x * Quat.x - y * Quat.y - z * Quat.z;
    result = VxQuaternion(newX, newY, newZ, newW);
#endif
    *this = result;
}

inline void VxQuaternion::Normalize() {
#if defined(VX_SIMD_SSE)
    VxSIMDNormalizeQuaternion(this);
#else
    float norm = sqrtf(x * x + y * y + z * z + w * w);
    if (norm == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
    } else {
        float invNorm = 1.0f / norm;
        x *= invNorm;
        y *= invNorm;
        z *= invNorm;
        w *= invNorm;
    }
#endif
}

inline VxQuaternion Slerp(float t, const VxQuaternion &Quat1, const VxQuaternion &Quat2) {
#if defined(VX_SIMD_SSE)
    VxQuaternion result;
    VxSIMDSlerpQuaternion(&result, t, &Quat1, &Quat2);
    return result;
#else
    float cosOmega = Quat1.x * Quat2.x + Quat1.y * Quat2.y + Quat1.z * Quat2.z + Quat1.w * Quat2.w;

    float k0, k1;

    if (cosOmega >= 0.0f) {
        float oneMinusCos = 1.0f - cosOmega;
        if (oneMinusCos < 0.01f) {
            k0 = 1.0f - t;
            k1 = t;
        } else {
            float omega = acosf(cosOmega);
            float invSinOmega = 1.0f / sinf(omega);
            k0 = sinf((1.0f - t) * omega) * invSinOmega;
            k1 = sinf(t * omega) * invSinOmega;
        }
    } else {
        float oneMinusCosNeg = 1.0f - (-cosOmega);
        if (oneMinusCosNeg < 0.01f) {
            k0 = 1.0f - t;
            k1 = -t;
        } else {
            float omega = acosf(-cosOmega);
            float invSinOmega = 1.0f / sinf(omega);
            k0 = sinf((1.0f - t) * omega) * invSinOmega;
            k1 = -sinf(t * omega) * invSinOmega;
        }
    }

    return VxQuaternion(
        k0 * Quat1.x + k1 * Quat2.x,
        k0 * Quat1.y + k1 * Quat2.y,
        k0 * Quat1.z + k1 * Quat2.z,
        k0 * Quat1.w + k1 * Quat2.w
    );
#endif
}

inline VxQuaternion Squad(float t, const VxQuaternion &Quat1, const VxQuaternion &Quat1Out, const VxQuaternion &Quat2In,
                          const VxQuaternion &Quat2) {
    VxQuaternion slerpA = Slerp(t, Quat1Out, Quat2In);
    VxQuaternion slerpB = Slerp(t, Quat1, Quat2);
    float blendFactor = 2.0f * t * (1.0f - t);
    return Slerp(blendFactor, slerpB, slerpA);
}

inline VxQuaternion LnDif(const VxQuaternion &P, const VxQuaternion &Q) {
    VxQuaternion div = Vx3DQuaternionDivide(Q, P);
    return Ln(div);
}

inline VxQuaternion Ln(const VxQuaternion &Quat) {
    float magnitude = sqrtf(Quat.x * Quat.x + Quat.y * Quat.y + Quat.z * Quat.z);
    float scale;
    if (magnitude == 0.0f) {
        scale = 0.0f;
    } else {
        scale = atan2f(magnitude, Quat.w) / magnitude;
    }
    return VxQuaternion(scale * Quat.x, scale * Quat.y, scale * Quat.z, 0.0f);
}

inline VxQuaternion Exp(const VxQuaternion &Quat) {
    float magnitude = sqrtf(Quat.x * Quat.x + Quat.y * Quat.y + Quat.z * Quat.z);
    float scale;
    if (magnitude < EPSILON) {
        scale = 1.0f;
    } else {
        scale = sinf(magnitude) / magnitude;
    }
    return VxQuaternion(scale * Quat.x, scale * Quat.y, scale * Quat.z, cosf(magnitude));
}

// =============================================================================
// Part 2 - SIMD-Accelerated Quaternion Operations
// =============================================================================

#if defined(VX_SIMD_SSE)

#include "VxSIMD.h"

VX_SIMD_INLINE float VxSIMDDotQuaternion(const VxQuaternion *a, const VxQuaternion *b) noexcept {
    __m128 qa = VxSIMDLoadFloat4(&a->x);
    __m128 qb = VxSIMDLoadFloat4(&b->x);
    __m128 dot = VxSIMDDotProduct4(qa, qb);
    float result;
    _mm_store_ss(&result, dot);
    return result;
}

VX_SIMD_INLINE float VxSIMDMagnitudeQuaternion(const VxQuaternion *q) noexcept {
    return VxSIMDDotQuaternion(q, q);
}

VX_SIMD_INLINE void VxSIMDConjugateQuaternion(VxQuaternion *result, const VxQuaternion *q) noexcept {
    __m128 quat = VxSIMDLoadFloat4(&q->x);
    __m128 conjugate = VxSIMDQuaternionConjugate(quat);
    VxSIMDStoreFloat4(&result->x, conjugate);
}

VX_SIMD_INLINE void VxSIMDDivideQuaternion(VxQuaternion *result, const VxQuaternion *p, const VxQuaternion *q) noexcept {
    __m128 qp = VxSIMDLoadFloat4(&p->x);
    __m128 qq = VxSIMDLoadFloat4(&q->x);
    __m128 qqConj = VxSIMDQuaternionConjugate(qq);
    __m128 divided = VxSIMDQuaternionMultiply(qp, qqConj);
    VxSIMDStoreFloat4(&result->x, divided);
}

VX_SIMD_INLINE void VxSIMDScaleQuaternion(VxQuaternion *result, const VxQuaternion *q, float scale) noexcept {
    __m128 quat = VxSIMDLoadFloat4(&q->x);
    __m128 scaled = _mm_mul_ps(quat, _mm_set1_ps(scale));
    VxSIMDStoreFloat4(&result->x, scaled);
}

VX_SIMD_INLINE void VxSIMDNormalizeQuaternion(VxQuaternion *q) noexcept {
    __m128 quat = _mm_loadu_ps(&q->x);
    __m128 magSq = VxSIMDDotProduct4(quat, quat);

    __m128 isZero = _mm_cmpeq_ps(magSq, VX_SIMD_ZERO);
    int zeroMask = _mm_movemask_ps(isZero);

    if (zeroMask & 1) {
        _mm_storeu_ps(&q->x, VX_SIMD_QUAT_IDENTITY);
        return;
    }

    __m128 invNorm = VxSIMDReciprocalSqrtAccurate(magSq);
    __m128 result = _mm_mul_ps(quat, invNorm);
    _mm_storeu_ps(&q->x, result);
}

VX_SIMD_INLINE void VxSIMDMultiplyQuaternion(VxQuaternion *result, const VxQuaternion *a, const VxQuaternion *b) noexcept {
    __m128 qa = _mm_loadu_ps(&a->x);
    __m128 qb = _mm_loadu_ps(&b->x);
    __m128 r = VxSIMDQuaternionMultiply(qa, qb);
    _mm_storeu_ps(&result->x, r);
}

VX_SIMD_INLINE void VxSIMDSlerpQuaternion(VxQuaternion *result, float t, const VxQuaternion *a, const VxQuaternion *b) noexcept {
    __m128 qa = _mm_loadu_ps(&a->x);
    __m128 qb = _mm_loadu_ps(&b->x);

    __m128 cosOmegaVec = VxSIMDDotProduct4(qa, qb);
    float cosOmega;
    _mm_store_ss(&cosOmega, cosOmegaVec);

    float k0, k1;
    float sign = 1.0f;

    if (cosOmega < 0.0f) {
        cosOmega = -cosOmega;
        sign = -1.0f;
    }

    float oneMinusCos = 1.0f - cosOmega;
    if (oneMinusCos < 0.01f) {
        k0 = 1.0f - t;
        k1 = t * sign;
    } else {
        float omega = acosf(cosOmega);
        float invSinOmega = 1.0f / sinf(omega);
        k0 = sinf((1.0f - t) * omega) * invSinOmega;
        k1 = sinf(t * omega) * invSinOmega * sign;
    }

    __m128 k0Vec = _mm_set1_ps(k0);
    __m128 k1Vec = _mm_set1_ps(k1);
    __m128 r = VX_FMADD_PS(qb, k1Vec, _mm_mul_ps(qa, k0Vec));
    _mm_storeu_ps(&result->x, r);
}

#endif // VX_SIMD_SSE
