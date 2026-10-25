#ifndef VXEIGENMATRIX_H
#define VXEIGENMATRIX_H

#include "VxMatrix.h"
#include <math.h>
#include <string.h>

// VxEigenMatrix class for computing eigenvalues/eigenvectors
class VxEigenMatrix : public VxMatrix {
public:
    VxEigenMatrix() = default;

    void Covariance(const float *points, XDWORD stride, XDWORD count) {
        // Population covariance. Center first instead of subtracting two large
        // raw moments; retain the actual variance rather than normalizing it.
        float origin[3] = {};
        if (count) memcpy(origin, points, sizeof(origin));
        const unsigned char *cursor = reinterpret_cast<const unsigned char *>(points);
        double moments[6]; // xx, yy, xy, zz, xz, yz
#if defined(VX_SIMD_SSE2)
        const __m128d originXY = _mm_set_pd(origin[1], origin[0]);
        const __m128d originZ = _mm_set_sd(origin[2]);
        __m128d meanXY = _mm_setzero_pd(), meanZ = _mm_setzero_pd();
        const __m128d divisor = _mm_set1_pd(count);
        for (XDWORD i = 0; i < count; ++i) {
            float point[3];
            memcpy(point, cursor, sizeof(point));
            const __m128d xy = _mm_cvtps_pd(_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64 *>(point)));
            meanXY = _mm_add_pd(meanXY, _mm_sub_pd(xy, originXY));
            meanZ = _mm_add_sd(meanZ, _mm_sub_sd(_mm_set_sd(point[2]), originZ));
            if (i + 1 < count) cursor += stride;
        }
        meanXY = _mm_div_pd(meanXY, divisor);
        meanZ = _mm_div_sd(meanZ, divisor);
        __m128d xxYY = _mm_setzero_pd(), xyZZ = _mm_setzero_pd(), xzYZ = _mm_setzero_pd();
        cursor = reinterpret_cast<const unsigned char *>(points);
        for (XDWORD i = 0; i < count; ++i) {
            float point[3];
            memcpy(point, cursor, sizeof(point));
            const __m128d xy = _mm_sub_pd(_mm_sub_pd(_mm_cvtps_pd(_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64 *>(point))), originXY), meanXY);
            const __m128d z = _mm_sub_sd(_mm_sub_sd(_mm_set_sd(point[2]), originZ), meanZ);
            xxYY = _mm_add_pd(xxYY, _mm_mul_pd(xy, xy));
            xyZZ = _mm_add_pd(xyZZ, _mm_mul_pd(_mm_unpacklo_pd(xy, z), _mm_shuffle_pd(xy, z, 1)));
            xzYZ = _mm_add_pd(xzYZ, _mm_mul_pd(xy, _mm_unpacklo_pd(z, z)));
            if (i + 1 < count) cursor += stride;
        }
        _mm_storeu_pd(moments, _mm_div_pd(xxYY, divisor));
        _mm_storeu_pd(moments + 2, _mm_div_pd(xyZZ, divisor));
        _mm_storeu_pd(moments + 4, _mm_div_pd(xzYZ, divisor));
#else
        double mean[3] = {};
        for (XDWORD i = 0; i < count; ++i) {
            float point[3];
            memcpy(point, cursor, sizeof(point));
            for (int k = 0; k < 3; ++k) mean[k] += double(point[k]) - origin[k];
            if (i + 1 < count) cursor += stride;
        }
        for (int k = 0; k < 3; ++k) mean[k] /= count;
        for (int k = 0; k < 6; ++k) moments[k] = 0;
        cursor = reinterpret_cast<const unsigned char *>(points);
        for (XDWORD i = 0; i < count; ++i) {
            float point[3];
            memcpy(point, cursor, sizeof(point));
            const double x = (double(point[0]) - origin[0]) - mean[0];
            const double y = (double(point[1]) - origin[1]) - mean[1];
            const double z = (double(point[2]) - origin[2]) - mean[2];
            moments[0] += x * x; moments[1] += y * y;
            moments[2] += x * y; moments[3] += z * z;
            moments[4] += x * z; moments[5] += y * z;
            if (i + 1 < count) cursor += stride;
        }
        for (int k = 0; k < 6; ++k) moments[k] /= count;
#endif
        m_Data[0][0] = static_cast<float>(moments[0]);
        m_Data[1][1] = static_cast<float>(moments[1]);
        m_Data[2][2] = static_cast<float>(moments[3]);
        m_Data[0][1] = m_Data[1][0] = static_cast<float>(moments[2]);
        m_Data[0][2] = m_Data[2][0] = static_cast<float>(moments[4]);
        m_Data[1][2] = m_Data[2][1] = static_cast<float>(moments[5]);
        m_Data[0][3] = m_Data[1][3] = m_Data[2][3] = 0;
    }

    void Tridiagonal(float *const diagonal, float *const offDiagonal) {
        const VxVector4 &r0 = (*this)[0];
        const VxVector4 &r1 = (*this)[1];
        const VxVector4 &r2 = (*this)[2];

        const float a01 = r0[1];
        const float a02 = r0[2];
        const float a11 = r1[1];
        const float a12 = r1[2];
        const float a22 = r2[2];

        diagonal[0] = m_Data[0][0];
        offDiagonal[2] = 0.0f;

        if (fabs(a02) < EPSILON) {
            diagonal[1] = a11;
            diagonal[2] = a22;
            offDiagonal[0] = a01;
            offDiagonal[1] = a12;

            // Identity
            SetIdentity();
            return;
        }

        const float length = sqrtf(a02 * a02 + a01 * a01);
        const float invLength = 1.0f / length;

        const float u1 = a01 * invLength;
        const float u2 = a02 * invLength;

        // v14 in the binary
        const float tmp = (a22 - a11) * u2 + 2.0f * a12 * u1;

        diagonal[1] = a11 + tmp * u2;
        diagonal[2] = a22 - tmp * u2;
        offDiagonal[0] = length;
        offDiagonal[1] = a12 - tmp * u1;

        // Householder eigenvector matrix as built by the binary
        VxVector4 &row0 = (*this)[0];
        VxVector4 &row1 = (*this)[1];
        VxVector4 &row2 = (*this)[2];

        row0.Set(1.0f, 0.0f, 0.0f, 0.0f);
        row1.Set(0.0f, u1, u2, 0.0f);
        row2.Set(0.0f, u2, -u1, 0.0f);
    }

    XBOOL QLAlgorithm(float *const diagonal, float *const offDiagonal) {
        // Only i=0..1 are processed for 3x3
        for (int i = 0; i <= 1; ++i) {
            int iter = 0;
            for (; iter < 32; ++iter) {
                int m = i;
                while (m <= 1) {
                    const double s = fabs(diagonal[m]) + fabs(diagonal[m + 1]);
                    if (fabs(offDiagonal[m]) + s == s) {
                        break;
                    }
                    ++m;
                }

                if (m == i) {
                    break;
                }

                // Implicit shift
                const float g0 = (diagonal[i + 1] - diagonal[i]) / (offDiagonal[i] + offDiagonal[i]);
                const float r0 = sqrtf(g0 * g0 + 1.0f);
                float g;
                if (g0 >= 0.0f) {
                    g = offDiagonal[i] / (g0 + r0) + diagonal[m] - diagonal[i];
                } else {
                    g = offDiagonal[i] / (g0 - r0) + diagonal[m] - diagonal[i];
                }

                float c = 1.0f;
                float s = 1.0f;
                float p = 0.0f;

                for (int j = m - 1; j >= i; --j) {
                    const float f = c * offDiagonal[j];
                    const float b = s * offDiagonal[j];

                    if (fabs(g) > fabs(f)) {
                        const float t = f / g;
                        const float r = sqrtf(t * t + 1.0f);
                        offDiagonal[j + 1] = r * g;
                        s = 1.0f / r;
                        c = t * s;
                    } else {
                        const float t = g / f;
                        const float r = sqrtf(t * t + 1.0f);
                        offDiagonal[j + 1] = f * r;
                        c = 1.0f / r;
                        s = t * c;
                    }

                    const float g2 = diagonal[j + 1] - p;
                    const float r2 = (diagonal[j] - g2) * c + 2.0f * b * s;
                    p = c * r2;
                    diagonal[j + 1] = g2 + p;
                    g = s * r2 - b;

                    // Update eigenvectors (exact order from the binary)
                    for (int row = 0; row < 3; ++row) {
                        VxVector4 &rowVec = (*this)[row];
                        const float t = rowVec[j + 1];
                        const float u = rowVec[j];
                        rowVec[j + 1] = s * t + c * u;
                        rowVec[j] = s * u - c * t;
                    }
                }

                diagonal[i] -= p;
                offDiagonal[i] = g;
                offDiagonal[m] = 0.0f;
            }

            if (iter == 32) {
                return FALSE;
            }
        }

        return TRUE;
    }

    void EigenStuff3() {
        float diagonal[3];
        float offDiagonal[3];
        Tridiagonal(diagonal, offDiagonal);
        QLAlgorithm(diagonal, offDiagonal);
    }
};

#endif // VXEIGENMATRIX_H
