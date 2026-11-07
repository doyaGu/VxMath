#include "VxMatrix.h"
#include "VxFloat80.h"
#include "VxFPUControlWord.h"
#include "VxSIMD.h"

#if !defined(VX_SIMD_SSE)
#include "vx_softfloat.h"
#include <string.h>

static float VxFloat32Add(float first, float second) {
    softfloat_status_t status = {};
    status.softfloat_exceptionMasks = softfloat_all_exceptions_mask;
    status.softfloat_roundingMode = softfloat_round_near_even;
    uint32_t a, b;
    memcpy(&a, &first, sizeof(a));
    memcpy(&b, &second, sizeof(b));
    const uint32_t bits = f32_add(a, b, &status);
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static float VxFloat32Multiply(float first, float second) {
    softfloat_status_t status = {};
    status.softfloat_exceptionMasks = softfloat_all_exceptions_mask;
    status.softfloat_roundingMode = softfloat_round_near_even;
    uint32_t a, b;
    memcpy(&a, &first, sizeof(a));
    memcpy(&b, &second, sizeof(b));
    const uint32_t bits = f32_mul(a, b, &status);
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}
#endif

static VxFloat80 VxFloat80Product(VxFloatContext &context, float first, float second) {
    return VxFloat80Multiply(context, VxFloat80Load(context, first), VxFloat80Load(context, second));
}

float Magnitude(const VxVector &axis) {
    VxFloatContext math;
    VxFloatInitContext(math, VxGetFPUControlWord());
    const VxFloat80 x = VxFloat80Product(math, axis.x, axis.x);
    const VxFloat80 y = VxFloat80Product(math, axis.y, axis.y);
    const VxFloat80 z = VxFloat80Product(math, axis.z, axis.z);
    const float square = VxFloat80StoreFloat(math, VxFloat80Add(math, VxFloat80Add(math, x, y), z));
    return VxFloat80StoreFloat(math, VxFloat80SquareRoot(math, VxFloat80Load(math, square)));
}
const VxVector Normalize(const VxVector &input) {
    VxVector axis = input;
    VxFloatContext math;
    VxFloatInitContext(math, VxGetFPUControlWord());
    const VxFloat80 x = VxFloat80Product(math, axis.x, axis.x);
    const VxFloat80 y = VxFloat80Product(math, axis.y, axis.y);
    const VxFloat80 z = VxFloat80Product(math, axis.z, axis.z);
    const float square = VxFloat80StoreFloat(math, VxFloat80Add(math, VxFloat80Add(math, x, y), z));
    const VxFloat80 length = VxFloat80SquareRoot(math, VxFloat80Load(math, square));
    const float inverse = VxFloat80StoreFloat(math, VxFloat80Divide(math, VxFloat80Load(math, 1.0f), length));
    axis.Set(VxFloat80StoreFloat(math, VxFloat80Product(math, axis.x, inverse)),
             VxFloat80StoreFloat(math, VxFloat80Product(math, axis.y, inverse)),
             VxFloat80StoreFloat(math, VxFloat80Product(math, axis.z, inverse)));
    return axis;
}
void Vx3DMultiplyMatrix(VxMatrix &result, const VxMatrix &a, const VxMatrix &b) {
    VxMatrix out;
    if (&result == &a || &result == &b) {
        VxFloatContext math;
        VxFloatInitContext(math, VxGetFPUControlWord());
        // The original alias branch uses x87 and a different summation order.
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) {
                const VxFloat80 x = VxFloat80Product(math, a[0][c], b[r][0]);
                const VxFloat80 y = VxFloat80Product(math, a[1][c], b[r][1]);
                const VxFloat80 z = VxFloat80Product(math, a[2][c], b[r][2]);
                VxFloat80 sum;
                if (r == 0 && c == 0)
                    sum = VxFloat80Add(math, VxFloat80Add(math, z, x), y);
                else if ((r == 0 && c != 0) || (r != 0 && c == 0))
                    sum = VxFloat80Add(math, VxFloat80Add(math, y, z), x);
                else
                    sum = VxFloat80Add(math, VxFloat80Add(math, x, z), y);
                if (r == 3)
                    sum = VxFloat80Add(math, sum, VxFloat80Load(math, a[3][c]));
                out[r][c] = VxFloat80StoreFloat(math, sum);
            }
    } else {
#if defined(VX_SIMD_SSE)
        const __m128 a0 = _mm_loadu_ps(a[0]), a1 = _mm_loadu_ps(a[1]);
        const __m128 a2 = _mm_loadu_ps(a[2]), a3 = _mm_loadu_ps(a[3]);
        for (int r = 0; r < 4; ++r) {
            const __m128 x = _mm_mul_ps(a0, _mm_set1_ps(b[r][0]));
            const __m128 y = _mm_mul_ps(a1, _mm_set1_ps(b[r][1]));
            const __m128 z = _mm_mul_ps(a2, _mm_set1_ps(b[r][2]));
            const __m128 xy = _mm_add_ps(x, y);
            _mm_storeu_ps(&out[r].x, r == 3 ? _mm_add_ps(_mm_add_ps(z, a3), xy) : _mm_add_ps(xy, z));
        }
#else
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) {
                const float x = VxFloat32Multiply(a[0][c], b[r][0]);
                const float y = VxFloat32Multiply(a[1][c], b[r][1]);
                const float z = VxFloat32Multiply(a[2][c], b[r][2]);
                const float xy = VxFloat32Add(x, y);
                out[r][c] = r == 3 ? VxFloat32Add(VxFloat32Add(z, a[3][c]), xy) : VxFloat32Add(xy, z);
            }
#endif
    }
    out[0][3] = out[1][3] = out[2][3] = 0;
    out[3][3] = 1;
    result = out;
}

void Vx3DRotateVector(VxVector *out, const VxMatrix &m, const VxVector *in) {
#if defined(VX_SIMD_SSE)
    const __m128 x = _mm_mul_ps(_mm_loadu_ps(m[0]), _mm_set1_ps(in->x));
    const __m128 y = _mm_mul_ps(_mm_loadu_ps(m[1]), _mm_set1_ps(in->y));
    const __m128 z = _mm_mul_ps(_mm_loadu_ps(m[2]), _mm_set1_ps(in->z));
    float values[4];
    _mm_storeu_ps(values, _mm_add_ps(_mm_add_ps(x, y), z));
    out->Set(values[0], values[1], values[2]);
#else
    VxVector result;
    for (int c = 0; c < 3; ++c) {
        const float x = VxFloat32Multiply(m[0][c], in->x);
        const float y = VxFloat32Multiply(m[1][c], in->y);
        const float z = VxFloat32Multiply(m[2][c], in->z);
        result[c] = VxFloat32Add(VxFloat32Add(x, y), z);
    }
    *out = result;
#endif
}
void Vx3DMultiplyMatrixVector(VxVector *out, const VxMatrix &m, const VxVector *in) {
#if defined(VX_SIMD_SSE)
    const __m128 x = _mm_mul_ps(_mm_loadu_ps(m[0]), _mm_set1_ps(in->x));
    const __m128 y = _mm_mul_ps(_mm_loadu_ps(m[1]), _mm_set1_ps(in->y));
    const __m128 z = _mm_mul_ps(_mm_loadu_ps(m[2]), _mm_set1_ps(in->z));
    float values[4];
    _mm_storeu_ps(values, _mm_add_ps(_mm_add_ps(_mm_add_ps(x, y), z), _mm_loadu_ps(m[3])));
    out->Set(values[0], values[1], values[2]);
#else
    VxVector result;
    for (int c = 0; c < 3; ++c) {
        const float x = VxFloat32Multiply(m[0][c], in->x);
        const float y = VxFloat32Multiply(m[1][c], in->y);
        const float z = VxFloat32Multiply(m[2][c], in->z);
        result[c] = VxFloat32Add(VxFloat32Add(VxFloat32Add(x, y), z), m[3][c]);
    }
    *out = result;
#endif
}

void Vx3DInverseMatrix(VxMatrix &result, const VxMatrix &source) {
    VxFloatContext math;
    VxFloatInitContext(math, VxGetFPUControlWord());
    const VxMatrix m = source;
    const VxFloat80 zero = VxFloat80Load(math, 0.0f);
    const VxFloat80 firstPair = VxFloat80Product(math, m[1][1], m[2][2]);
    const VxFloat80 lastPair = VxFloat80Product(math, m[1][2], m[2][1]);
    const float firstSaved = VxFloat80StoreFloat(math, firstPair), lastSaved = VxFloat80StoreFloat(math, lastPair);
    const VxFloat80 terms[] = {
        VxFloat80Load(math,
                      VxFloat80StoreFloat(math, VxFloat80Multiply(math, firstPair, VxFloat80Load(math, m[0][0])))),
        VxFloat80Multiply(math, VxFloat80Product(math, m[1][2], m[2][0]), VxFloat80Load(math, m[0][1])),
        VxFloat80Multiply(math, VxFloat80Product(math, m[0][2], m[1][0]), VxFloat80Load(math, m[2][1])),
        VxFloat80Negate(
            math, VxFloat80Multiply(math, VxFloat80Product(math, m[0][2], m[2][0]), VxFloat80Load(math, m[1][1]))),
        VxFloat80Negate(
            math, VxFloat80Multiply(math, VxFloat80Product(math, m[1][0], m[2][2]), VxFloat80Load(math, m[0][1]))),
        VxFloat80Negate(math, VxFloat80Multiply(math, lastPair, VxFloat80Load(math, m[0][0])))};
    VxFloat80 positive = zero, negative = zero;
    for (int i = 0; i < 6; ++i) {
        if (VxFloat80Compare(math, terms[i], zero) < 0)
            negative = VxFloat80Add(math, negative, terms[i]);
        else
            positive = VxFloat80Add(math, positive, terms[i]);
    }
    const VxFloat80 determinant = VxFloat80Add(math, positive, negative);
    if (!VxFloat80Compare(math, determinant, zero))
        return;
    const float savedDeterminant = VxFloat80StoreFloat(math, determinant);
    const VxFloat80 ratio =
        VxFloat80Divide(math, VxFloat80Load(math, savedDeterminant), VxFloat80Subtract(math, positive, negative));
    const VxFloat80 absolute = VxFloat80Compare(math, ratio, zero) < 0 ? VxFloat80Negate(math, ratio) : ratio;
    if (VxFloat80Compare(math, absolute, VxFloat80Load(math, 1.0e-15)) < 0)
        return;
    const VxFloat80 reciprocal =
        VxFloat80Divide(math, VxFloat80Load(math, 1.0f), VxFloat80Load(math, savedDeterminant));
    VxMatrix out;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            const int i = (c + 1) % 3, j = (c + 2) % 3;
            const int k = (r + 1) % 3, l = (r + 2) % 3;
            VxFloat80 cofactor;
            if (r == 0 && c == 0) {
                cofactor = VxFloat80Subtract(math, VxFloat80Load(math, firstSaved), VxFloat80Load(math, lastSaved));
            } else {
                const VxFloat80 first = VxFloat80Product(math, m[i][k], m[j][l]);
                const VxFloat80 second = VxFloat80Product(math, m[i][l], m[j][k]);
                // Preserve the original subtraction followed by negation,
                // including signed zero and directed rounding.
                cofactor = (r + c) & 1 ? VxFloat80Negate(math, VxFloat80Subtract(math, second, first))
                                       : VxFloat80Subtract(math, first, second);
            }
            out[r][c] = VxFloat80StoreFloat(math, VxFloat80Multiply(math, cofactor, reciprocal));
        }
    for (int c = 0; c < 3; ++c) {
        const VxFloat80 x = VxFloat80Product(math, m[3][0], out[0][c]), y = VxFloat80Product(math, m[3][1], out[1][c]);
        const VxFloat80 z = VxFloat80Product(math, m[3][2], out[2][c]);
        const VxFloat80 translation =
            c == 2 ? VxFloat80Add(math, VxFloat80Add(math, z, y), x) : VxFloat80Add(math, VxFloat80Add(math, y, x), z);
        out[3][c] = VxFloat80StoreFloat(math, VxFloat80Negate(math, translation));
    }
    out[0][3] = out[1][3] = out[2][3] = 0;
    out[3][3] = 1;
    result = out;
}
