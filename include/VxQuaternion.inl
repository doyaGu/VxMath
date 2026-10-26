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
    return q1.x == q2.x && q1.y == q2.y && q1.z == q2.z && q1.w == q2.w;
#endif
}

inline int operator!=(const VxQuaternion &q1, const VxQuaternion &q2) {
    return q1.x != q2.x || q1.y != q2.y || q1.z != q2.z || q1.w != q2.w;
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
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
#endif
}

inline float DotProduct(const VxQuaternion &q1, const VxQuaternion &q2) {
#if defined(VX_SIMD_SSE)
    return VxSIMDDotQuaternion(&q1, &q2);
#else
    return q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
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
        QuatL.x * QuatR.w + QuatL.w * QuatR.x + QuatL.y * QuatR.z - QuatL.z * QuatR.y,
        QuatL.y * QuatR.w + QuatL.w * QuatR.y + QuatL.z * QuatR.x - QuatL.x * QuatR.z,
        QuatL.z * QuatR.w + QuatL.w * QuatR.z + QuatL.x * QuatR.y - QuatL.y * QuatR.x,
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
    // 0x2429D650 has its own accumulation order, including z,y,x,w for
    // the scalar component. Multiplying conjugate(Q) * P reassociates it.
    result = VxQuaternion(
        Q.w*P.x - Q.x*P.w - P.z*Q.y + P.y*Q.z,
        P.y*Q.w - P.w*Q.y - P.x*Q.z + P.z*Q.x,
        P.z*Q.w - P.w*Q.z - P.y*Q.x + P.x*Q.y,
        P.z*Q.z + P.y*Q.y + P.x*Q.x + Q.w*P.w);
#endif
    return result;
}

inline void VxQuaternion::Multiply(const VxQuaternion &Quat) {
    *this = Vx3DQuaternionMultiply(*this, Quat);
}

#if defined(_MSC_VER) && defined(_M_IX86)
VX_EXPORT void VxQuaternionSinCos(double angle, double &sine, double &cosine);
#else
inline void VxQuaternionSinCos(double angle, double &sine, double &cosine) {
    sine = sin(angle);
    cosine = cos(angle);
}
#endif

template <typename SineType>
inline void VxRotationSinCos(float angle, SineType &sine, float &cosine) {
    double wideSine, wideCosine;
    VxQuaternionSinCos(angle, wideSine, wideCosine);
    sine = static_cast<SineType>(wideSine);
    cosine = static_cast<float>(wideCosine);
}

inline double VxQuaternionInterpolationDot(const VxQuaternion &a, const VxQuaternion &b) {
    const double z = static_cast<double>(a.z)*b.z;
    const double y = static_cast<double>(a.y)*b.y;
    const double x = static_cast<double>(a.x)*b.x;
    const double w = static_cast<double>(a.w)*b.w;
    return ((z+y)+x)+w;
}

inline VxQuaternion VxQuaternionInterpolate(double t, const VxQuaternion &a, const VxQuaternion &b) {
    const double dot = VxQuaternionInterpolationDot(a, b);
    const double sign = dot < 0.0 ? -1.0 : 1.0;
    const double cosine = dot < 0.0 ? -dot : dot;
    double weightA = 1.0 - t;
    double weightB = sign * t;
    if (cosine < 0.99) {
        const double angle = acos(cosine);
        const double inverseSine = 1.0 / sin(angle);
        weightA = sin((1.0 - t) * angle) * inverseSine;
        weightB = sign * sin(t * angle) * inverseSine;
    }

    VxQuaternion result;
#if defined(VX_SIMD_SSE)
    const __m128 qa = _mm_loadu_ps(&a.x);
    const __m128 qb = _mm_loadu_ps(&b.x);
    const __m128d scaleA = _mm_set1_pd(weightA);
    const __m128d scaleB = _mm_set1_pd(weightB);
    const __m128d xy = _mm_add_pd(_mm_mul_pd(_mm_cvtps_pd(qa), scaleA), _mm_mul_pd(_mm_cvtps_pd(qb), scaleB));
    const __m128d zw = _mm_add_pd(
        _mm_mul_pd(_mm_cvtps_pd(_mm_movehl_ps(qa, qa)), scaleA),
        _mm_mul_pd(_mm_cvtps_pd(_mm_movehl_ps(qb, qb)), scaleB));
    _mm_storeu_ps(&result.x, _mm_movelh_ps(_mm_cvtpd_ps(xy), _mm_cvtpd_ps(zw)));
#else
    result.x = static_cast<float>(a.x * weightA + b.x * weightB);
    result.y = static_cast<float>(a.y * weightA + b.y * weightB);
    result.z = static_cast<float>(a.z * weightA + b.z * weightB);
    result.w = static_cast<float>(a.w * weightA + b.w * weightB);
#endif
    return result;
}

inline double VxQuaternionSquaredLength(float x, float y, float z, float w = 0.0f) {
    // All finite float squares and their sum fit in double, including
    // subnormal inputs and inputs whose squared length exceeds FLT_MAX.
    const double x2 = static_cast<double>(x)*x;
    const double y2 = static_cast<double>(y)*y;
    const double z2 = static_cast<double>(z)*z;
    const double w2 = static_cast<double>(w)*w;
    return ((x2+y2)+z2)+w2;
}

inline double VxQuaternionLength(float x, float y, float z, float w = 0.0f) {
    return sqrt(VxQuaternionSquaredLength(x, y, z, w));
}

inline void VxQuaternionNormalize(VxQuaternion *q) {
    const double length = VxQuaternionLength(q->x, q->y, q->z, q->w);
    if (length == 0.0) {
        *q = VxQuaternion();
        return;
    }
    // Keep the reciprocal wide until each normalized component is stored.
    const double inverse = 1.0/length;
    q->x = static_cast<float>(q->x*inverse);
    q->y = static_cast<float>(q->y*inverse);
    q->z = static_cast<float>(q->z*inverse);
    q->w = static_cast<float>(q->w*inverse);
}

inline void VxQuaternion::Normalize() {
#if defined(VX_SIMD_SSE)
    VxSIMDNormalizeQuaternion(this);
#else
    VxQuaternionNormalize(this);
#endif
}

inline VxQuaternion Slerp(float t, const VxQuaternion &Quat1, const VxQuaternion &Quat2) {
    return VxQuaternionInterpolate(t, Quat1, Quat2);
}

inline VxQuaternion Squad(float t, const VxQuaternion &Quat1, const VxQuaternion &Quat1Out, const VxQuaternion &Quat2In,
                          const VxQuaternion &Quat2) {
    VxQuaternion slerpA = Slerp(t, Quat1Out, Quat2In);
    VxQuaternion slerpB = Slerp(t, Quat1, Quat2);
    const double blendFactor = 2.0 * t * (1.0 - static_cast<double>(t));
    return VxQuaternionInterpolate(blendFactor, slerpB, slerpA);
}

inline VxQuaternion LnDif(const VxQuaternion &P, const VxQuaternion &Q) {
    VxQuaternion div = Vx3DQuaternionDivide(Q, P);
    return Ln(div);
}

inline VxQuaternion Ln(const VxQuaternion &Quat) {
    const double magnitude = VxQuaternionLength(Quat.x, Quat.y, Quat.z);
    // Skip division for zero or unordered lengths; retain double precision
    // through the angle and component scaling.
    const double scale = magnitude > 0.0 ?
        (atan2(magnitude, static_cast<double>(Quat.w))/magnitude) : magnitude;
    return VxQuaternion(static_cast<float>(scale*Quat.x),
                        static_cast<float>(scale*Quat.y),
                        static_cast<float>(scale*Quat.z), 0.0f);
}

inline VxQuaternion Exp(const VxQuaternion &Quat) {
    const double magnitude = VxQuaternionLength(Quat.x, Quat.y, Quat.z);
    double sine, cosine;
    VxQuaternionSinCos(magnitude, sine, cosine);
    const double scale = magnitude == 0.0 ? 1.0 : sine/magnitude;
    return VxQuaternion(static_cast<float>(scale*Quat.x),
                        static_cast<float>(scale*Quat.y),
                        static_cast<float>(scale*Quat.z),
                        static_cast<float>(cosine));
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
    const __m128 qp = VxSIMDLoadFloat4(&p->x), qq = VxSIMDLoadFloat4(&q->x);
    const __m128 negateXYZ = _mm_setr_ps(-0.0f, -0.0f, -0.0f, 0.0f);
    const __m128 a = _mm_mul_ps(_mm_shuffle_ps(qp, qp, _MM_SHUFFLE(2, 2, 1, 0)),
                                _mm_shuffle_ps(qq, qq, _MM_SHUFFLE(2, 3, 3, 3)));
    const __m128 b = _mm_xor_ps(negateXYZ,
        _mm_mul_ps(_mm_shuffle_ps(qp, qp, _MM_SHUFFLE(1, 3, 3, 3)),
                   _mm_shuffle_ps(qq, qq, _MM_SHUFFLE(1, 2, 1, 0))));
    const __m128 c = _mm_xor_ps(negateXYZ,
        _mm_mul_ps(_mm_shuffle_ps(qp, qp, _MM_SHUFFLE(0, 1, 0, 2)),
                   _mm_shuffle_ps(qq, qq, _MM_SHUFFLE(0, 0, 2, 1))));
    const __m128 d = _mm_mul_ps(_mm_shuffle_ps(qp, qp, _MM_SHUFFLE(3, 0, 2, 1)),
                                _mm_shuffle_ps(qq, qq, _MM_SHUFFLE(3, 1, 0, 2)));
    const __m128 divided = _mm_add_ps(_mm_add_ps(_mm_add_ps(a, b), c), d);
    VxSIMDStoreFloat4(&result->x, divided);
}

VX_SIMD_INLINE void VxSIMDScaleQuaternion(VxQuaternion *result, const VxQuaternion *q, float scale) noexcept {
    __m128 quat = VxSIMDLoadFloat4(&q->x);
    __m128 scaled = _mm_mul_ps(quat, _mm_set1_ps(scale));
    VxSIMDStoreFloat4(&result->x, scaled);
}

VX_SIMD_INLINE void VxSIMDNormalizeQuaternion(VxQuaternion *q) noexcept {
    _mm_storeu_ps(&q->x, VxSIMDQuaternionNormalize(_mm_loadu_ps(&q->x)));
}

VX_SIMD_INLINE void VxSIMDMultiplyQuaternion(VxQuaternion *result, const VxQuaternion *a, const VxQuaternion *b) noexcept {
    __m128 qa = _mm_loadu_ps(&a->x);
    __m128 qb = _mm_loadu_ps(&b->x);
    __m128 r = VxSIMDQuaternionMultiply(qa, qb);
    _mm_storeu_ps(&result->x, r);
}

VX_SIMD_INLINE void VxSIMDSlerpQuaternion(VxQuaternion *result, float t, const VxQuaternion *a, const VxQuaternion *b) noexcept {
    *result = VxQuaternionInterpolate(t, *a, *b);
}

#endif // VX_SIMD_SSE
