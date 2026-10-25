/**
 * @file VxSIMD.inl
 * @brief Low-level SIMD primitives operating on raw __m128 registers.
 *
 * @remarks
 * This file provides the foundational SIMD building blocks (load/store,
 * dot product, cross product, normalization, reciprocal sqrt, basic
 * arithmetic) used internally by the per-type .inl files.
 *
 * Included automatically by VxSIMD.h - do not include directly.
 *
 * Follows the DirectXMath pattern of separating raw vector math from
 * high-level type wrappers.
 */

#pragma once

#include <string.h>

#if defined(VX_SIMD_SSE)

// ============================================================================
// Vector Load / Store
// ============================================================================

/**
 * @brief Loads 3 floats into a SIMD register (w = 0).
 */
VX_SIMD_INLINE __m128 VxSIMDLoadFloat3(const float *ptr) noexcept {
    return _mm_set_ps(0.0f, ptr[2], ptr[1], ptr[0]);
}

/**
 * @brief Stores the first 3 components of a SIMD register.
 */
VX_SIMD_INLINE void VxSIMDStoreFloat3(float *ptr, __m128 v) noexcept {
    _mm_storel_epi64(reinterpret_cast<__m128i *>(ptr), _mm_castps_si128(v));
    _mm_store_ss(ptr + 2, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

/**
 * @brief Loads 4 floats (unaligned) into a SIMD register.
 */
VX_SIMD_INLINE __m128 VxSIMDLoadFloat4(const float *ptr) noexcept {
    return _mm_loadu_ps(ptr);
}

/**
 * @brief Stores 4 floats (unaligned) from a SIMD register.
 */
VX_SIMD_INLINE void VxSIMDStoreFloat4(float *ptr, __m128 v) noexcept {
    _mm_storeu_ps(ptr, v);
}

/**
 * @brief Loads 4 aligned floats into a SIMD register.
 */
VX_SIMD_INLINE __m128 VxSIMDLoadFloat4Aligned(const float *ptr) noexcept {
    return _mm_load_ps(ptr);
}

/**
 * @brief Stores 4 aligned floats from a SIMD register.
 */
VX_SIMD_INLINE void VxSIMDStoreFloat4Aligned(float *ptr, __m128 v) noexcept {
    _mm_store_ps(ptr, v);
}

// ============================================================================
// Dot Product
// ============================================================================

/**
 * @brief 3-component dot product (result broadcast to all lanes).
 */
VX_SIMD_INLINE __m128 VxSIMDDotProduct3(__m128 a, __m128 b) noexcept {
#if defined(VX_SIMD_SSE4_1)
    return _mm_dp_ps(a, b, 0x7F);
#else
    __m128 mul = _mm_mul_ps(a, b);
    __m128 sum_xy = _mm_add_ss(mul, _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 1, 1, 1)));
    __m128 sum_xyz = _mm_add_ss(sum_xy, _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 2, 2, 2)));
    return _mm_shuffle_ps(sum_xyz, sum_xyz, _MM_SHUFFLE(0, 0, 0, 0));
#endif
}

/**
 * @brief 4-component dot product (result broadcast to all lanes).
 */
VX_SIMD_INLINE __m128 VxSIMDDotProduct4(__m128 a, __m128 b) noexcept {
#if defined(VX_SIMD_SSE4_1)
    return _mm_dp_ps(a, b, 0xFF);
#else
    __m128 mul = _mm_mul_ps(a, b);
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ps(sums, shuf);
    return _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(0, 0, 0, 0));
#endif
}

// ============================================================================
// Cross Product
// ============================================================================

/**
 * @brief 3-component cross product.
 */
VX_SIMD_INLINE __m128 VxSIMDCrossProduct3(__m128 a, __m128 b) noexcept {
    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
    return VX_FNMADD_PS(a_zxy, b_yzx, _mm_mul_ps(a_yzx, b_zxy));
}

// ============================================================================
// Reciprocal Square Root / Reciprocal
// ============================================================================

/**
 * @brief Fast reciprocal square root (approx).
 */
VX_SIMD_INLINE __m128 VxSIMDReciprocalSqrt(__m128 v) noexcept {
    return _mm_rsqrt_ps(v);
}

/**
 * @brief Accurate reciprocal square root using packed square root and division.
 */
VX_SIMD_INLINE __m128 VxSIMDReciprocalSqrtAccurate(__m128 v) noexcept {
    const __m128i magnitude = _mm_and_si128(_mm_castps_si128(v), _mm_set1_epi32(0x7fffffff));
    if (_mm_movemask_epi8(_mm_cmplt_epi32(magnitude, _mm_set1_epi32(0x00800000))) != 0) {
        // Widen subnormal inputs before taking their square roots.
        const __m128d low = _mm_cvtps_pd(v), high = _mm_cvtps_pd(_mm_movehl_ps(v, v));
        const __m128d one = _mm_set1_pd(1);
        return _mm_movelh_ps(_mm_cvtpd_ps(_mm_div_pd(one, _mm_sqrt_pd(low))),
                            _mm_cvtpd_ps(_mm_div_pd(one, _mm_sqrt_pd(high))));
    }
    return _mm_div_ps(VX_SIMD_ONE, _mm_sqrt_ps(v));
}

/**
 * @brief Accurate reciprocal (1/x) using packed division.
 */
VX_SIMD_INLINE __m128 VxSIMDReciprocalAccurate(__m128 v) noexcept {
    const __m128i magnitude = _mm_and_si128(_mm_castps_si128(v), _mm_set1_epi32(0x7fffffff));
    const __m128i tiny = _mm_cmplt_epi32(magnitude, _mm_set1_epi32(0x00800000));
    const __m128i large = _mm_cmpgt_epi32(magnitude, _mm_set1_epi32(0x7e800000));
    if (_mm_movemask_epi8(_mm_or_si128(tiny, large)) != 0) {
        const __m128d low = _mm_cvtps_pd(v), high = _mm_cvtps_pd(_mm_movehl_ps(v, v));
        const __m128d one = _mm_set1_pd(1);
        return _mm_movelh_ps(_mm_cvtpd_ps(_mm_div_pd(one, low)), _mm_cvtpd_ps(_mm_div_pd(one, high)));
    }
    return _mm_div_ps(VX_SIMD_ONE, v);
}

// ============================================================================
// Normalization
// ============================================================================

VX_SIMD_INLINE __m128 VxSIMDNormalizeChecked(__m128 v, __m128 dot) noexcept {
    __m128 mask = _mm_cmpgt_ps(dot, VX_SIMD_EPSILON);
    __m128 safeDot = _mm_max_ps(dot, VX_SIMD_EPSILON);
    __m128 invLen = VxSIMDReciprocalSqrtAccurate(safeDot);
    __m128 normalized = _mm_mul_ps(v, invLen);
    __m128 keepOriginal = _mm_andnot_ps(mask, v);
    __m128 useNormalized = _mm_and_ps(mask, normalized);
    return _mm_or_ps(keepOriginal, useNormalized);
}

/**
 * @brief Scales all lanes by the XYZ length, preserving zero or unordered XYZ.
 */
VX_SIMD_INLINE __m128 VxSIMDNormalize3(__m128 v) noexcept {
    const __m128 dot = VxSIMDDotProduct3(v, v);
    const float squareFloat = _mm_cvtss_f32(dot);
    if (squareFloat >= 0x1p-100f && squareFloat <= 0x1p100f) {
        return _mm_div_ps(v, _mm_sqrt_ps(dot));
    }
    const __m128d xy = _mm_cvtps_pd(v);
    const __m128d zw = _mm_cvtps_pd(_mm_movehl_ps(v, v));
    const __m128d products = _mm_mul_pd(xy, xy);
    const __m128d square = _mm_add_sd(_mm_add_sd(products, _mm_unpackhi_pd(products, products)), _mm_mul_sd(zw, zw));
    if (_mm_cvtsd_f64(square) > 0) {
        const __m128d length = _mm_sqrt_sd(square, square);
        const __m128d inverse = _mm_div_sd(_mm_set_sd(1), length);
        const __m128d scale = _mm_unpacklo_pd(inverse, inverse);
        return _mm_movelh_ps(_mm_cvtpd_ps(_mm_mul_pd(xy, scale)), _mm_cvtpd_ps(_mm_mul_pd(zw, scale)));
    }
    return v;
}

/**
 * @brief Normalizes a 4D vector register.
 */
VX_SIMD_INLINE __m128 VxSIMDNormalize4(__m128 v) noexcept {
    return VxSIMDNormalizeChecked(v, VxSIMDDotProduct4(v, v));
}

// ============================================================================
// Matrix-Vector Multiplication (raw float* matrix)
// ============================================================================

/**
 * @brief Transforms XYZ with implicit w=1; returns all four homogeneous lanes without division.
 */
VX_SIMD_INLINE __m128 VxSIMDMatrixMultiplyVector3(const float *mat, __m128 v) noexcept {
    const __m128 row0 = _mm_loadu_ps(mat), row1 = _mm_loadu_ps(mat + 4), row2 = _mm_loadu_ps(mat + 8);
    const __m128 row3 = _mm_loadu_ps(mat + 12);
    const __m128d vx = _mm_set1_pd(double(_mm_cvtss_f32(v)));
    const __m128d vy = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)))));
    const __m128d vz = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)))));

    __m128 parts[2];
    for (int half = 0; half < 2; ++half) {
        const __m128d x = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row0, row0) : row0), vx);
        const __m128d y = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row1, row1) : row1), vy);
        const __m128d z = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row2, row2) : row2), vz);
        const __m128d w = _mm_cvtps_pd(half ? _mm_movehl_ps(row3, row3) : row3);
        const __m128d ordinary = _mm_add_pd(_mm_add_pd(_mm_add_pd(x, y), z), w);
        const __m128d terms[] = {y, z, w};
        __m128d sum = x, error = _mm_setzero_pd();
        // Float products are exact in double; preserve addition tails until the final store.
        for (int term = 0; term < 3; ++term) {
            const __m128d next = _mm_add_pd(sum, terms[term]), delta = _mm_sub_pd(next, sum);
            error = _mm_add_pd(error, _mm_add_pd(_mm_sub_pd(sum, _mm_sub_pd(next, delta)), _mm_sub_pd(terms[term], delta)));
            sum = next;
        }
        const __m128d finite = _mm_cmple_pd(_mm_andnot_pd(_mm_set1_pd(-0.0), ordinary), _mm_set1_pd(DBL_MAX));
        const __m128d compensated = _mm_add_pd(sum, error);
        parts[half] = _mm_cvtpd_ps(_mm_or_pd(_mm_and_pd(finite, compensated), _mm_andnot_pd(finite, ordinary)));
    }
    return _mm_movelh_ps(parts[0], parts[1]);
}

/**
 * @brief Transforms XYZ by the first three matrix rows; returns four lanes without translation or division.
 */
VX_SIMD_INLINE __m128 VxSIMDMatrixRotateVector3(const float *mat, __m128 v) noexcept {
    const __m128 row0 = _mm_loadu_ps(mat), row1 = _mm_loadu_ps(mat + 4), row2 = _mm_loadu_ps(mat + 8);

    const __m128d vx = _mm_set1_pd(double(_mm_cvtss_f32(v)));
    const __m128d vy = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)))));
    const __m128d vz = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)))));

    __m128 parts[2];
    for (int half = 0; half < 2; ++half) {
        const __m128d x = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row0, row0) : row0), vx);
        const __m128d y = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row1, row1) : row1), vy);
        const __m128d z = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row2, row2) : row2), vz);
        const __m128d w = _mm_setzero_pd();
        const __m128d ordinary = _mm_add_pd(_mm_add_pd(_mm_add_pd(x, y), z), w);
        const __m128d terms[] = {y, z, w};
        __m128d sum = x, error = _mm_setzero_pd();
        // Float products are exact in double; preserve addition tails until the final store.
        for (int term = 0; term < 3; ++term) {
            const __m128d next = _mm_add_pd(sum, terms[term]), delta = _mm_sub_pd(next, sum);
            error = _mm_add_pd(error, _mm_add_pd(_mm_sub_pd(sum, _mm_sub_pd(next, delta)), _mm_sub_pd(terms[term], delta)));
            sum = next;
        }
        const __m128d finite = _mm_cmple_pd(_mm_andnot_pd(_mm_set1_pd(-0.0), ordinary), _mm_set1_pd(DBL_MAX));
        const __m128d compensated = _mm_add_pd(sum, error);
        parts[half] = _mm_cvtpd_ps(_mm_or_pd(_mm_and_pd(finite, compensated), _mm_andnot_pd(finite, ordinary)));
    }
    return _mm_movelh_ps(parts[0], parts[1]);
}

/**
 * @brief Transforms all XYZW using the actual input w; returns four lanes without division.
 */
VX_SIMD_INLINE __m128 VxSIMDMatrixMultiplyVector4(const float *mat, __m128 v) noexcept {
    const __m128 row0 = _mm_loadu_ps(mat), row1 = _mm_loadu_ps(mat + 4), row2 = _mm_loadu_ps(mat + 8);
    const __m128 row3 = _mm_loadu_ps(mat + 12);
    const __m128d vx = _mm_set1_pd(double(_mm_cvtss_f32(v)));
    const __m128d vy = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)))));
    const __m128d vz = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)))));
    const __m128d vw = _mm_set1_pd(double(_mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)))));
    __m128 parts[2];
    for (int half = 0; half < 2; ++half) {
        const __m128d x = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row0, row0) : row0), vx);
        const __m128d y = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row1, row1) : row1), vy);
        const __m128d z = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row2, row2) : row2), vz);
        const __m128d w = _mm_mul_pd(_mm_cvtps_pd(half ? _mm_movehl_ps(row3, row3) : row3), vw);
        const __m128d ordinary = _mm_add_pd(_mm_add_pd(_mm_add_pd(x, y), z), w);
        const __m128d terms[] = {y, z, w};
        __m128d sum = x, error = _mm_setzero_pd();
        // Float products are exact in double; preserve addition tails until the final store.
        for (int term = 0; term < 3; ++term) {
            const __m128d next = _mm_add_pd(sum, terms[term]), delta = _mm_sub_pd(next, sum);
            error = _mm_add_pd(error, _mm_add_pd(_mm_sub_pd(sum, _mm_sub_pd(next, delta)), _mm_sub_pd(terms[term], delta)));
            sum = next;
        }
        const __m128d finite = _mm_cmple_pd(_mm_andnot_pd(_mm_set1_pd(-0.0), ordinary), _mm_set1_pd(DBL_MAX));
        const __m128d compensated = _mm_add_pd(sum, error);
        parts[half] = _mm_cvtpd_ps(_mm_or_pd(_mm_and_pd(finite, compensated), _mm_andnot_pd(finite, ordinary)));
    }
    return _mm_movelh_ps(parts[0], parts[1]);
}

// ============================================================================
// Register-Level Arithmetic
// ============================================================================

VX_SIMD_INLINE __m128 VxSIMDAdd3(__m128 a, __m128 b) noexcept {
    return _mm_add_ps(a, b);
}

VX_SIMD_INLINE __m128 VxSIMDSubtract3(__m128 a, __m128 b) noexcept {
    return _mm_sub_ps(a, b);
}

VX_SIMD_INLINE __m128 VxSIMDScale(__m128 v, float scalar) noexcept {
    return _mm_mul_ps(v, _mm_set1_ps(scalar));
}

VX_SIMD_INLINE float VxSIMDLength3(__m128 v) noexcept {
    const __m128d xy = _mm_cvtps_pd(v);
    const __m128d z = _mm_cvtps_pd(_mm_movehl_ps(v, v));
    const __m128d products = _mm_mul_pd(xy, xy);
    const __m128d square = _mm_add_sd(_mm_add_sd(products, _mm_unpackhi_pd(products, products)), _mm_mul_sd(z, z));
    const __m128d length = _mm_sqrt_sd(square, square);
#if !defined(VX_SIMD_X64)
    // Avoid the slow subnormal return conversion measured on Win32.
    if (_mm_cvtsd_f64(length) < 0x1p-126) {
        const int bits = _mm_cvtsd_si32(_mm_mul_sd(length, _mm_set_sd(0x1p149)));
        return _mm_cvtss_f32(_mm_castsi128_ps(_mm_cvtsi32_si128(bits)));
    }
#endif
    return static_cast<float>(_mm_cvtsd_f64(length));
}

VX_SIMD_INLINE float VxSIMDLengthSquared3(__m128 v) noexcept {
    const __m128d xy = _mm_cvtps_pd(v);
    const __m128d z = _mm_cvtps_pd(_mm_movehl_ps(v, v));
    const __m128d products = _mm_mul_pd(xy, xy);
    const __m128d square = _mm_add_sd(_mm_add_sd(products, _mm_unpackhi_pd(products, products)), _mm_mul_sd(z, z));
#if defined(VX_SIMD_X64)
    return static_cast<float>(_mm_cvtsd_f64(square));
#else
    // The high word identifies doubles in [2^-126, 2^128), the normal
    // float conversion range. Win32's extreme conversions need a separate path.
    const unsigned high = static_cast<unsigned>(_mm_cvtsi128_si32(_mm_srli_si128(_mm_castpd_si128(square), 4)));
    if (high - 0x38100000u < 0x0fe00000u) {
        return static_cast<float>(_mm_cvtsd_f64(square));
    }
    if (high < 0x38100000u) {
        // Round subnormal results as integer multiples of the minimum float,
        // avoiding an underflowing double-to-float conversion.
        const int bits = _mm_cvtsd_si32(_mm_mul_sd(square, _mm_set_sd(0x1p149)));
        return _mm_cvtss_f32(_mm_castsi128_ps(_mm_cvtsi32_si128(bits)));
    }
    if (high >= 0x7ff00000u) {
        return static_cast<float>(_mm_cvtsd_f64(square));
    }
    return _mm_cvtss_f32(_mm_castsi128_ps(_mm_set1_epi32(0x7f800000)));
#endif
}

VX_SIMD_INLINE __m128 VxSIMDLerp(__m128 a, __m128 b, float t) noexcept {
    __m128 tVec = _mm_set1_ps(t);
    __m128 diff = _mm_sub_ps(b, a);
    return VX_FMADD_PS(diff, tVec, a);
}

VX_SIMD_INLINE __m128 VxSIMDMin(__m128 a, __m128 b) noexcept {
    return _mm_min_ps(a, b);
}

VX_SIMD_INLINE __m128 VxSIMDMax(__m128 a, __m128 b) noexcept {
    return _mm_max_ps(a, b);
}

VX_SIMD_INLINE __m128 VxSIMDReflect3(__m128 incident, __m128 normal) noexcept {
    __m128 dot = VxSIMDDotProduct3(incident, normal);
    __m128 two_dot = _mm_add_ps(dot, dot);
    return VX_FNMADD_PS(two_dot, normal, incident);
}

// ============================================================================
// Raw Quaternion Operations (__m128 xyzw layout)
// ============================================================================

VX_SIMD_INLINE __m128 VxSIMDQuaternionNormalize(__m128 q) noexcept {
    __m128 magSq = VxSIMDDotProduct4(q, q);
    __m128 isZero = _mm_cmpeq_ps(magSq, _mm_setzero_ps());
    int zeroMask = _mm_movemask_ps(isZero);
    if (zeroMask & 1) {
        return VX_SIMD_QUAT_IDENTITY;
    }
    __m128 invNorm = VxSIMDReciprocalSqrtAccurate(magSq);
    return _mm_mul_ps(q, invNorm);
}

VX_SIMD_INLINE __m128 VxSIMDQuaternionMultiply(__m128 qa, __m128 qb) noexcept {
    // Match 0x2429D5C0: for xyz, add the two scalar/vector products,
    // then the positive cross term, then subtract the negative cross term.
    const __m128 negateW = _mm_setr_ps(0.0f, 0.0f, 0.0f, -0.0f);
    const __m128 a = _mm_mul_ps(qa, _mm_shuffle_ps(qb, qb, _MM_SHUFFLE(3, 3, 3, 3)));
    const __m128 b = _mm_xor_ps(negateW,
        _mm_mul_ps(_mm_shuffle_ps(qa, qa, _MM_SHUFFLE(0, 3, 3, 3)),
                   _mm_shuffle_ps(qb, qb, _MM_SHUFFLE(0, 2, 1, 0))));
    const __m128 c = _mm_xor_ps(negateW,
        _mm_mul_ps(_mm_shuffle_ps(qa, qa, _MM_SHUFFLE(1, 0, 2, 1)),
                   _mm_shuffle_ps(qb, qb, _MM_SHUFFLE(1, 1, 0, 2))));
    const __m128 d = _mm_mul_ps(_mm_shuffle_ps(qa, qa, _MM_SHUFFLE(2, 1, 0, 2)),
                                _mm_shuffle_ps(qb, qb, _MM_SHUFFLE(2, 0, 2, 1)));
    return _mm_sub_ps(_mm_add_ps(_mm_add_ps(a, b), c), d);
}

VX_SIMD_INLINE __m128 VxSIMDQuaternionConjugate(__m128 q) noexcept {
    static const __m128 conjugateMask = _mm_setr_ps(-1.0f, -1.0f, -1.0f, 1.0f);
    return _mm_mul_ps(q, conjugateMask);
}

// ============================================================================
// Raw Matrix Operations (float[16])
// ============================================================================

VX_SIMD_INLINE void VxSIMDMatrixMultiply(float *result, const float *a, const float *b) noexcept {
    // Full B*A composition; preserve A before result can overwrite either operand.
    alignas(16) float matrix[16];
    _mm_storeu_ps(matrix, _mm_loadu_ps(a));
    _mm_storeu_ps(matrix + 4, _mm_loadu_ps(a + 4));
    _mm_storeu_ps(matrix + 8, _mm_loadu_ps(a + 8));
    _mm_storeu_ps(matrix + 12, _mm_loadu_ps(a + 12));
    for (int i = 0; i < 4; ++i) {
        const __m128 bRow = _mm_loadu_ps(b + i * 4);
        _mm_storeu_ps(result + i * 4, VxSIMDMatrixMultiplyVector4(matrix, bRow));
    }
}

VX_SIMD_INLINE void VxSIMDMatrixTranspose(float *result, const float *src) noexcept {
    __m128 r0 = _mm_loadu_ps(src + 0);
    __m128 r1 = _mm_loadu_ps(src + 4);
    __m128 r2 = _mm_loadu_ps(src + 8);
    __m128 r3 = _mm_loadu_ps(src + 12);

    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);

    _mm_storeu_ps(result + 0, r0);
    _mm_storeu_ps(result + 4, r1);
    _mm_storeu_ps(result + 8, r2);
    _mm_storeu_ps(result + 12, r3);
}

// ============================================================================
// VxMath Utility Operations
// ============================================================================

VX_SIMD_INLINE void VxSIMDCopyDwordAlignedBlock(void *dst, const void *src, XDWORD sizeBytes) noexcept {
    XBYTE *dstBytes = static_cast<XBYTE *>(dst);
    const XBYTE *srcBytes = static_cast<const XBYTE *>(src);

    XDWORD offset = 0;
    for (; offset + 16 <= sizeBytes; offset += 16) {
        const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset));
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset), chunk);
    }

    for (; offset < sizeBytes; offset += 4) {
        std::memcpy(dstBytes + offset, srcBytes + offset, sizeof(XDWORD));
    }
}

VX_SIMD_INLINE XBOOL VxSIMDFillStructure(int count, void *dst, XDWORD stride, XDWORD sizeSrc, const void *src) noexcept {
    if (!src || !dst || count <= 0 || !sizeSrc || !stride || (sizeSrc & 3) != 0)
        return FALSE;
    const XBYTE *srcBytes = static_cast<const XBYTE *>(src);
    XBYTE *dstBytes = static_cast<XBYTE *>(dst);
    switch (sizeSrc) {
    case 4: {
        int value;
        memcpy(&value, src, 4);
        const __m128 pattern = _mm_castsi128_ps(_mm_set1_epi32(value));
        if (stride == 4) {
            while (count >= 4) {
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), _mm_castps_si128(pattern));
                dstBytes += 16;
                count -= 4;
            }
            if (count == 0) return TRUE;
        }
        for (;;) {
            _mm_store_ss(reinterpret_cast<float *>(dstBytes), pattern);
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    }
    case 8: {
        const __m128i pattern = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src));
        if (stride == 8) {
            const __m128i pair = _mm_unpacklo_epi64(pattern, pattern);
            while (count >= 2) {
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), pair);
                dstBytes += 16;
                count -= 2;
            }
            if (count == 0) return TRUE;
        }
        for (;;) {
            _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes), pattern);
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    }
    case 12: {
        const __m128i xy = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src));
        const __m128 z = _mm_load_ss(reinterpret_cast<const float *>(srcBytes + 8));
        if (stride == 12) {
            const __m128 xyz = _mm_movelh_ps(_mm_castsi128_ps(xy), z);
            const __m128 p0 = _mm_shuffle_ps(xyz, xyz, _MM_SHUFFLE(0, 2, 1, 0));
            const __m128 p1 = _mm_shuffle_ps(xyz, xyz, _MM_SHUFFLE(1, 0, 2, 1));
            const __m128 p2 = _mm_shuffle_ps(xyz, xyz, _MM_SHUFFLE(2, 1, 0, 2));
            while (count >= 4) {
                _mm_storeu_ps(reinterpret_cast<float *>(dstBytes), p0);
                _mm_storeu_ps(reinterpret_cast<float *>(dstBytes + 16), p1);
                _mm_storeu_ps(reinterpret_cast<float *>(dstBytes + 32), p2);
                dstBytes += 48;
                count -= 4;
            }
            if (count == 0) return TRUE;
        }
        for (;;) {
            _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes), xy);
            _mm_store_ss(reinterpret_cast<float *>(dstBytes + 8), z);
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    }
    case 16: {
        const __m128i pattern = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
        if (stride == 16) {
            while (count >= 4) {
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), pattern);
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 16), pattern);
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 32), pattern);
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 48), pattern);
                dstBytes += 64;
                count -= 4;
            }
            if (count == 0) return TRUE;
        }
        for (;;) {
            _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), pattern);
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    }
    case 20:
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes);
            const uintptr_t source = reinterpret_cast<uintptr_t>(srcBytes);
            if (address > source && address - source < 16) {
                for (XDWORD offset = 0; offset < 20; offset += 4) {
                    XDWORD value;
                    memcpy(&value, srcBytes + offset, 4);
                    memcpy(dstBytes + offset, &value, 4);
                }
            } else {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), value);
                // The preceding store can feed this last source word.
                XDWORD tail;
                memcpy(&tail, srcBytes + 16, 4);
                memcpy(dstBytes + 16, &tail, 4);
            }
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    default: {
        const XDWORD end64 = sizeSrc & ~XDWORD(63);
        const XDWORD end16 = sizeSrc & ~XDWORD(15);
        const XDWORD end8 = sizeSrc & ~XDWORD(7);
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes);
            const uintptr_t source = reinterpret_cast<uintptr_t>(srcBytes);
            const uintptr_t distance = address > source ? address - source : 16;
            XDWORD offset = 0;
            // Do not preload across a forward DWORD dependency.
            if (distance >= 16) {
                for (; offset < end64; offset += 64) {
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset),
                                     _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 16),
                                     _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 16)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 32),
                                     _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 32)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 48),
                                     _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 48)));
                }
                for (; offset < end16; offset += 16) {
                    const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset), value);
                }
            }
            if (distance >= 8) {
                for (; offset < end8; offset += 8) {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes + offset));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes + offset), value);
                }
            }
            for (; offset < sizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, srcBytes + offset, 4);
                memcpy(dstBytes + offset, &value, 4);
            }
            if (--count == 0) return TRUE;
            dstBytes += stride;
        }
    }
    }
}

VX_SIMD_INLINE XBOOL VxSIMDCopyStructure(int count, void *dst, XDWORD outStride, XDWORD sizeSrc, const void *src, XDWORD inStride) noexcept {
    if (!src || !dst || count <= 0 || !sizeSrc || !outStride || !inStride || (sizeSrc & 3) != 0)
        return FALSE;
    const XBYTE *srcBytes = static_cast<const XBYTE *>(src);
    XBYTE *dstBytes = static_cast<XBYTE *>(dst);
    if (outStride == sizeSrc && inStride == sizeSrc) {
        const uint64_t length = static_cast<uint64_t>(static_cast<unsigned>(count))*sizeSrc;
        const uintptr_t address = reinterpret_cast<uintptr_t>(dst), source = reinterpret_cast<uintptr_t>(src);
        const uintptr_t gap = address > source ? address - source : source - address;
        // Only disjoint complete arrays may ignore record-specific read order.
        if (length <= static_cast<size_t>(-1) && gap >= length) {
            size_t bytes = static_cast<size_t>(length);
            while (bytes >= 64) {
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes)));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 16), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + 16)));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 32), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + 32)));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 48), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + 48)));
                dstBytes += 64;
                srcBytes += 64;
                bytes -= 64;
            }
            while (bytes >= 16) {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), value);
                dstBytes += 16;
                srcBytes += 16;
                bytes -= 16;
            }
            while (bytes) {
                XDWORD value;
                memcpy(&value, srcBytes, 4);
                memcpy(dstBytes, &value, 4);
                dstBytes += 4;
                srcBytes += 4;
                bytes -= 4;
            }
            return TRUE;
        }
    }
    switch (sizeSrc) {
    case 4:
        for (;;) {
            const __m128 value = _mm_load_ss(reinterpret_cast<const float *>(srcBytes));
            _mm_store_ss(reinterpret_cast<float *>(dstBytes), value);
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    case 8:
        for (;;) {
            const __m128i pair = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes));
            _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes), pair);
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    case 12:
        if (outStride == inStride) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            if (address <= source || address - source <= 4 || address - source >= 12) {
                for (;;) {
                    const __m128i xy = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes));
                    const __m128 z = _mm_load_ss(reinterpret_cast<const float *>(srcBytes + 8));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes), xy);
                    _mm_store_ss(reinterpret_cast<float *>(dstBytes + 8), z);
                    if (--count == 0) return TRUE;
                    srcBytes += inStride;
                    dstBytes += outStride;
                }
            }
        }
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            const __m128 xy = _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes)));
            if (address > source && address - source > 4 && address - source < 12) {
                _mm_store_ss(reinterpret_cast<float *>(dstBytes), xy);
                const __m128 z = _mm_load_ss(reinterpret_cast<const float *>(srcBytes + 8));
                _mm_store_ss(reinterpret_cast<float *>(dstBytes + 4), _mm_shuffle_ps(xy, xy, _MM_SHUFFLE(1, 1, 1, 1)));
                _mm_store_ss(reinterpret_cast<float *>(dstBytes + 8), z);
            } else {
                const __m128 z = _mm_load_ss(reinterpret_cast<const float *>(srcBytes + 8));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes), _mm_castps_si128(xy));
                _mm_store_ss(reinterpret_cast<float *>(dstBytes + 8), z);
            }
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    case 16:
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            if (address > source && address - source == 4) {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), _mm_shuffle_epi32(value, _MM_SHUFFLE(3, 1, 1, 0)));
            } else if (address > source && address - source < 16) {
                // DWORD transfers avoid partial store forwarding stalls while
                // retaining the two original pair snapshots.
                XDWORD x, y, z, w;
                memcpy(&x, srcBytes, 4);
                memcpy(&y, srcBytes + 4, 4);
                memcpy(dstBytes, &x, 4);
                memcpy(dstBytes + 4, &y, 4);
                memcpy(&z, srcBytes + 8, 4);
                memcpy(&w, srcBytes + 12, 4);
                memcpy(dstBytes + 8, &z, 4);
                memcpy(dstBytes + 12, &w, 4);
            } else {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), value);
            }
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    case 20:
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            if (address > source && address - source < 16) {
                XDWORD value;
                memcpy(&value, srcBytes, 4);
                memcpy(dstBytes, &value, 4);
                memcpy(&value, srcBytes + 4, 4);
                memcpy(dstBytes + 4, &value, 4);
                memcpy(&value, srcBytes + 8, 4);
                memcpy(dstBytes + 8, &value, 4);
                memcpy(&value, srcBytes + 12, 4);
                memcpy(dstBytes + 12, &value, 4);
                memcpy(&value, srcBytes + 16, 4);
                memcpy(dstBytes + 16, &value, 4);
            } else {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), value);
                XDWORD tail;
                memcpy(&tail, srcBytes + 16, 4);
                memcpy(dstBytes + 16, &tail, 4);
            }
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    case 32:
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            const uintptr_t distance = address > source ? address - source : 16;
            if (distance >= 16) {
                const __m128i lo = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes), lo);
                const __m128i hi = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + 16));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + 16), hi);
            } else if (distance >= 8) {
                for (XDWORD offset = 0; offset < 32; offset += 8) {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes + offset));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes + offset), value);
                }
            } else {
                for (XDWORD offset = 0; offset < 32; offset += 4) {
                    XDWORD value;
                    memcpy(&value, srcBytes + offset, 4);
                    memcpy(dstBytes + offset, &value, 4);
                }
            }
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    default: {
        const XDWORD end64 = sizeSrc & ~XDWORD(63), end16 = sizeSrc & ~XDWORD(15), end8 = sizeSrc & ~XDWORD(7);
        for (;;) {
            const uintptr_t address = reinterpret_cast<uintptr_t>(dstBytes), source = reinterpret_cast<uintptr_t>(srcBytes);
            const uintptr_t distance = address > source ? address - source : 16;
            XDWORD offset = 0;
            if (distance >= 16) {
                for (; offset < end64; offset += 64) {
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 16), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 16)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 32), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 32)));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset + 48), _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset + 48)));
                }
                for (; offset < end16; offset += 16) {
                    const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcBytes + offset));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(dstBytes + offset), value);
                }
            }
            if (distance >= 8) {
                for (; offset < end8; offset += 8) {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcBytes + offset));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(dstBytes + offset), value);
                }
            }
            for (; offset < sizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, srcBytes + offset, 4);
                memcpy(dstBytes + offset, &value, 4);
            }
            if (--count == 0) return TRUE;
            srcBytes += inStride;
            dstBytes += outStride;
        }
    }
    }
}

VX_SIMD_INLINE XBOOL VxSIMDIndexedCopy(const VxStridedData &dst, const VxStridedData &src, XDWORD sizeSrc, const int *indices, int indexCount) noexcept {
    if (indexCount <= 0 || sizeSrc == 0 || (sizeSrc & 3) != 0)
        return FALSE;
    if (dst.Ptr == nullptr || src.Ptr == nullptr || indices == nullptr)
        return FALSE;

    XBYTE *out = static_cast<XBYTE *>(dst.Ptr);
    const XDWORD outStride = dst.Stride;
    const uintptr_t source = reinterpret_cast<uintptr_t>(src.Ptr);
    const uintptr_t sourceStride = src.Stride;
    const XBYTE *indexBytes = reinterpret_cast<const XBYTE *>(indices);
    // Descriptor snapshots and live indices also apply when output aliases them.
    switch (sizeSrc) {
    case 4: {
        const XBYTE *endIndices = reinterpret_cast<const XBYTE *>(reinterpret_cast<uintptr_t>(indexBytes) + static_cast<uintptr_t>(indexCount) * sizeof(int));
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value;
                memcpy(&value, in + 0, 4);
                memcpy(out + 0, &value, 4);
            }
            indexBytes += sizeof(int);
            if (indexBytes == endIndices) return TRUE;
            out += outStride;
        }
    }
    case 8: {
        const XBYTE *endIndices = reinterpret_cast<const XBYTE *>(reinterpret_cast<uintptr_t>(indexBytes) + static_cast<uintptr_t>(indexCount) * sizeof(int));
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 0));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 0), value);
            }
            indexBytes += sizeof(int);
            if (indexBytes == endIndices) return TRUE;
            out += outStride;
        }
    }
    case 12: {
        while (indexCount >= 2) {
            {
                int index;
                memcpy(&index, indexBytes, sizeof(index));
                const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
                {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 0));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 0), value);
                }
                {
                    XDWORD value;
                    memcpy(&value, in + 8, 4);
                    memcpy(out + 8, &value, 4);
                }
            }
            out += outStride;
            indexBytes += sizeof(int);
            {
                int index;
                memcpy(&index, indexBytes, sizeof(index));
                const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
                {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 0));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 0), value);
                }
                {
                    XDWORD value;
                    memcpy(&value, in + 8, 4);
                    memcpy(out + 8, &value, 4);
                }
            }
            indexCount -= 2;
            if (indexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
        {
                int index;
                memcpy(&index, indexBytes, sizeof(index));
                const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
                {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 0));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 0), value);
                }
                {
                    XDWORD value;
                    memcpy(&value, in + 8, 4);
                    memcpy(out + 8, &value, 4);
                }
        }
        return TRUE;
    }
    case 16: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            const uintptr_t outAddress = reinterpret_cast<uintptr_t>(out);
            const uintptr_t inAddress = reinterpret_cast<uintptr_t>(in);
            if (outAddress <= inAddress || outAddress - inAddress >= 16) {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), value);
            } else if (outAddress - inAddress == 4) {
                // Ordered pairs at +4 produce X,Y,Y,W without partial-store forwarding.
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                const __m128i pairs = _mm_shuffle_epi32(value, _MM_SHUFFLE(3, 1, 1, 0));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), pairs);
            } else {
                const __m128i first = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out), first);
                const __m128i second = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 8));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 8), second);
            }
            if (--indexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 20: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            const uintptr_t outAddress = reinterpret_cast<uintptr_t>(out);
            const uintptr_t inAddress = reinterpret_cast<uintptr_t>(in);
            if (outAddress <= inAddress || outAddress - inAddress >= 16) {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), value);
            } else if (outAddress - inAddress == 4) {
                // Ordered pairs at +4 produce X,Y,Y,W without partial-store forwarding.
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                const __m128i pairs = _mm_shuffle_epi32(value, _MM_SHUFFLE(3, 1, 1, 0));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), pairs);
            } else {
                const __m128i first = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out), first);
                const __m128i second = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 8));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 8), second);
            }
            {
                XDWORD value;
                memcpy(&value, in + 16, 4);
                memcpy(out + 16, &value, 4);
            }
            if (--indexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 24: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            const uintptr_t outAddress = reinterpret_cast<uintptr_t>(out);
            const uintptr_t inAddress = reinterpret_cast<uintptr_t>(in);
            if (outAddress <= inAddress || outAddress - inAddress >= 16) {
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), value);
            } else if (outAddress - inAddress == 4) {
                // Ordered pairs at +4 produce X,Y,Y,W without partial-store forwarding.
                const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
                const __m128i tail = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 16));
                const __m128i pairs = _mm_shuffle_epi32(value, _MM_SHUFFLE(3, 1, 1, 0));
                const __m128i last = _mm_unpacklo_epi32(_mm_shuffle_epi32(value, _MM_SHUFFLE(3, 3, 3, 3)), _mm_srli_si128(tail, 4));
                _mm_storeu_si128(reinterpret_cast<__m128i *>(out), pairs);
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 16), last);
                if (--indexCount == 0) return TRUE;
                out += outStride;
                indexBytes += sizeof(int);
                continue;
            } else {
                const __m128i first = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out), first);
                const __m128i second = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 8));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 8), second);
            }
            {
                const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + 16));
                _mm_storel_epi64(reinterpret_cast<__m128i *>(out + 16), value);
            }
            if (--indexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    default: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            const uintptr_t outAddress = reinterpret_cast<uintptr_t>(out);
            const uintptr_t inAddress = reinterpret_cast<uintptr_t>(in);
            const uintptr_t distance = outAddress > inAddress ? outAddress - inAddress : 16;
            XDWORD offset = 0;
            // Wider batches are safe only beyond the forward feedback distance.
            if (distance >= 16) {
                for (; sizeSrc - offset >= 64; offset += 64) {
                    const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + offset));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(out + offset), a);
                    const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + offset + 16));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(out + offset + 16), b);
                    const __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + offset + 32));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(out + offset + 32), c);
                    const __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + offset + 48));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(out + offset + 48), d);
                }
                for (; sizeSrc - offset >= 16; offset += 16) {
                    const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + offset));
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(out + offset), value);
                }
            }
            if (distance >= 8) {
                for (; sizeSrc - offset >= 8; offset += 8) {
                    const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(in + offset));
                    _mm_storel_epi64(reinterpret_cast<__m128i *>(out + offset), value);
                }
            }
            for (; offset < sizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, in + offset, 4);
                memcpy(out + offset, &value, 4);
            }
            if (--indexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    }
}

VX_SIMD_INLINE XBOOL VxSIMDPtInRect(const CKRECT *rect, const CKPOINT *pt) noexcept {
    const __m128i pointXYXY = _mm_setr_epi32(pt->x, pt->x, pt->y, pt->y);
    const __m128i minXYXY = _mm_setr_epi32(rect->left, rect->left, rect->top, rect->top);
    const __m128i maxXYXY = _mm_setr_epi32(rect->right, rect->right, rect->bottom, rect->bottom);

    const __m128i geMin = _mm_or_si128(_mm_cmpgt_epi32(pointXYXY, minXYXY), _mm_cmpeq_epi32(pointXYXY, minXYXY));
    const __m128i leMax = _mm_or_si128(_mm_cmpgt_epi32(maxXYXY, pointXYXY), _mm_cmpeq_epi32(maxXYXY, pointXYXY));
    const __m128i insideMask = _mm_and_si128(geMin, leMax);

    return (_mm_movemask_ps(_mm_castsi128_ps(insideMask)) == 0xF) ? TRUE : FALSE;
}

#endif // VX_SIMD_SSE
