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
        // The symmetric input is defined by its upper triangle. A fixed
        // epsilon must not discard a nonzero coupling in a small matrix.
        const double a01 = m_Data[0][1], a02 = m_Data[0][2];
        const double a11 = m_Data[1][1], a12 = m_Data[1][2], a22 = m_Data[2][2];
        diagonal[0] = m_Data[0][0];
        offDiagonal[2] = 0;
        if (a02 == 0) {
            diagonal[1] = static_cast<float>(a11);
            diagonal[2] = static_cast<float>(a22);
            offDiagonal[0] = static_cast<float>(a01);
            offDiagonal[1] = static_cast<float>(a12);
            SetIdentity();
            return;
        }
        const double length = sqrt(a01 * a01 + a02 * a02);
        const double u = a01 / length, v = a02 / length;
        const double shift = (a22 - a11) * v + 2 * a12 * u;
        diagonal[1] = static_cast<float>(a11 + shift * v);
        diagonal[2] = static_cast<float>(a22 - shift * v);
        offDiagonal[0] = static_cast<float>(length);
        offDiagonal[1] = static_cast<float>(a12 - shift * u);
        m_Data[0][0] = 1; m_Data[0][1] = m_Data[0][2] = m_Data[0][3] = 0;
        m_Data[1][0] = 0; m_Data[1][1] = static_cast<float>(u); m_Data[1][2] = static_cast<float>(v); m_Data[1][3] = 0;
        m_Data[2][0] = 0; m_Data[2][1] = static_cast<float>(v); m_Data[2][2] = static_cast<float>(-u); m_Data[2][3] = 0;
    }

    XBOOL QLAlgorithm(float *const diagonal, float *const offDiagonal) {
        // Keep shifts, rotations and their accumulated basis in double until
        // the final float outputs. The original iteration bound is retained.
        double d[3], e[3], basis[3][3];
        for (int i = 0; i < 3; ++i) {
            d[i] = diagonal[i]; e[i] = offDiagonal[i];
            for (int j = 0; j < 3; ++j) basis[i][j] = m_Data[i][j];
        }
        XBOOL converged = TRUE;
        for (int i = 0; i < 3; ++i) {
            int iteration = 0;
            for (; iteration < 32; ++iteration) {
                int m = i;
                while (m < 2) {
                    const double sum = fabs(d[m]) + fabs(d[m + 1]);
                    if (fabs(e[m]) + sum == sum) break;
                    ++m;
                }
                if (m == i) break;
                const double shift = (d[i + 1] - d[i]) / (2 * e[i]);
                const double radius = sqrt(shift * shift + 1);
                double g = e[i] / (shift >= 0 ? shift + radius : shift - radius) + d[m] - d[i];
                double sine = 1, cosine = 1, correction = 0;
                for (int j = m - 1; j >= i; --j) {
                    const double f = sine * e[j], b = cosine * e[j];
                    if (fabs(g) > fabs(f)) {
                        const double ratio = f / g, norm = sqrt(ratio * ratio + 1);
                        e[j + 1] = norm * g;
                        cosine = 1 / norm;
                        sine = ratio * cosine;
                    } else {
                        const double ratio = g / f, norm = sqrt(ratio * ratio + 1);
                        e[j + 1] = f * norm;
                        sine = 1 / norm;
                        cosine = ratio * sine;
                    }
                    const double old = d[j + 1] - correction;
                    const double rotated = (d[j] - old) * sine + 2 * b * cosine;
                    correction = sine * rotated;
                    d[j + 1] = old + correction;
                    g = cosine * rotated - b;
                    for (int row = 0; row < 3; ++row) {
                        const double next = basis[row][j + 1], previous = basis[row][j];
                        basis[row][j + 1] = cosine * next + sine * previous;
                        basis[row][j] = cosine * previous - sine * next;
                    }
                }
                d[i] -= correction;
                e[i] = g;
                e[m] = 0;
            }
            if (iteration == 32) { converged = FALSE; break; }
        }
        for (int i = 0; i < 3; ++i) {
            diagonal[i] = static_cast<float>(d[i]);
            offDiagonal[i] = static_cast<float>(e[i]);
            for (int j = 0; j < 3; ++j) m_Data[i][j] = static_cast<float>(basis[i][j]);
        }
        return converged;
    }

    void EigenStuff3() {
        float diagonal[3];
        float offDiagonal[3];
        Tridiagonal(diagonal, offDiagonal);
        QLAlgorithm(diagonal, offDiagonal);
    }
};

#endif // VXEIGENMATRIX_H
