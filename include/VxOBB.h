#ifndef VXOBB_H
#define VXOBB_H

#include "VxVector.h"
#include "VxBbox.h"
#include "VxMatrix.h"

/**
 * @brief Represents an Oriented Bounding Box (OBB).
 *
 * @remarks
 * An OBB is a rectangular block that is not necessarily aligned with the
 * coordinate axes. It is defined by a center point, three mutually
 * perpendicular axis vectors, and the half-length extents along those axes.
 *
 * A VxOBB is defined as:
 * @code
 * class VxOBB {
 * public:
 *     VxVector m_Center;
 *     VxVector m_Axis[3];
 *     VxVector m_Extents;
 * };
 * @endcode
 *
 * @see VxMatrix, VxVector, VxBbox
 */
class VxOBB {
public:
    /// @brief Default constructor.
    VxOBB() {}

    /**
     * @brief Constructs an OBB from an axis-aligned box and a transformation matrix.
     * @param box The original AABB.
     * @param mat The transformation matrix to apply to the AABB.
     */
    VxOBB(const VxBbox &box, const VxMatrix &mat) { Create(box, mat); }

    /// @brief Gets a mutable reference to the center of the OBB.
    VxVector &GetCenter() { return m_Center; };
    /// @brief Gets a const reference to the center of the OBB.
    const VxVector &GetCenter() const { return m_Center; }

    /// @brief Gets a mutable reference to a specific axis of the OBB.
    VxVector &GetAxis(int i) { return m_Axis[i]; }
    /// @brief Gets a const reference to a specific axis of the OBB.
    const VxVector &GetAxis(int i) const { return m_Axis[i]; }
    /// @brief Gets a mutable pointer to the axes array of the OBB.
    VxVector *GetAxes() { return m_Axis; }
    /// @brief Gets a const pointer to the axes array of the OBB.
    const VxVector *GetAxes() const { return m_Axis; };

    /// @brief Gets a mutable reference to a specific extent of the OBB.
    float &GetExtent(int i) { return m_Extents[i]; }
    /// @brief Gets a const reference to a specific extent of the OBB.
    const float &GetExtent(int i) const { return m_Extents[i]; }
    /// @brief Gets a mutable pointer to the extents vector of the OBB.
    float *GetExtents() { return &m_Extents.x; }
    /// @brief Gets a const pointer to the extents vector of the OBB.
    const float *GetExtents() const { return &m_Extents.x; }

    /**
     * @brief Creates the OBB from an axis-aligned box and a transformation matrix.
     * @param box The source AABB.
     * @param mat The transformation matrix.
     * @remarks Geometric OBB queries require nonzero, mutually perpendicular
     * matrix axes. A shear is not converted into an orthogonal box.
     */
    void Create(const VxBbox &box, const VxMatrix &mat) {
        VxVector v = box.GetCenter();
        Vx3DMultiplyMatrixVector(&m_Center, mat, &v);
#if defined(VX_SIMD_SSE2)
        for (int i = 0; i < 3; ++i) {
            const __m128 axis = VxSIMDLoadFloat3(&mat[i][0]);
            const __m128d xy = _mm_cvtps_pd(axis), z = _mm_cvtps_pd(_mm_movehl_ps(axis, axis));
            const __m128d squaredXY = _mm_mul_pd(xy, xy);
            const __m128d squared = _mm_add_sd(_mm_add_sd(squaredXY, _mm_unpackhi_pd(squaredXY, squaredXY)), _mm_mul_sd(z, z));
            const __m128d length = _mm_sqrt_sd(squared, squared);
            const __m128 normalizedXY = _mm_cvtpd_ps(_mm_div_pd(xy, _mm_unpacklo_pd(length, length)));
            const __m128 normalizedZ = _mm_cvtsd_ss(_mm_setzero_ps(), _mm_div_sd(z, length));
            VxSIMDStoreFloat3(&m_Axis[i].x, _mm_movelh_ps(normalizedXY, normalizedZ));
            const double halfsize = (double(box.Max[i]) - box.Min[i]) * 0.5;
            m_Extents[i] = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_mul_sd(length, _mm_set_sd(halfsize))));
        }
#elif defined(VX_SIMD_SSE)
        m_Axis[0] = *(VxVector *) &mat[0][0];
        m_Axis[1] = *(VxVector *) &mat[1][0];
        m_Axis[2] = *(VxVector *) &mat[2][0];
        m_Extents[0] = Magnitude(m_Axis[0]);
        m_Extents[1] = Magnitude(m_Axis[1]);
        m_Extents[2] = Magnitude(m_Axis[2]);
        m_Axis[0] /= m_Extents[0];
        m_Axis[1] /= m_Extents[1];
        m_Axis[2] /= m_Extents[2];
        m_Extents[0] *= 0.5f * (box.Max[0] - box.Min[0]);
        m_Extents[1] *= 0.5f * (box.Max[1] - box.Min[1]);
        m_Extents[2] *= 0.5f * (box.Max[2] - box.Min[2]);
#else
        for (int i = 0; i < 3; ++i) {
            const double x = mat[i][0], y = mat[i][1], z = mat[i][2];
            const double length = sqrt(x * x + y * y + z * z);
            m_Axis[i].Set(float(x / length), float(y / length), float(z / length));
            m_Extents[i] = float(length * ((double(box.Max[i]) - box.Min[i]) * 0.5));
        }
#endif
    }

    /**
     * @brief Checks if a point is inside this OBB.
     * @param iV The point to check.
     * @return TRUE if the point is inside the box, FALSE otherwise.
     */
    XBOOL VectorIn(const VxVector &iV) const {
#if defined(VX_SIMD_SSE2)
        const __m128 point = VxSIMDLoadFloat3(&iV.x), center = VxSIMDLoadFloat3(&m_Center.x);
        const __m128d xy = _mm_sub_pd(_mm_cvtps_pd(point), _mm_cvtps_pd(center));
        const __m128d z = _mm_sub_sd(_mm_cvtps_pd(_mm_movehl_ps(point, point)), _mm_cvtps_pd(_mm_movehl_ps(center, center)));
        const __m128d limit = _mm_set1_pd(double(FLT_MAX) * 0.25);
        // Unit axes and displacements below this bound keep all three float
        // dot-product terms within range. Retain that path's boundary rounding.
        const bool wide = _mm_movemask_pd(_mm_cmpgt_pd(_mm_andnot_pd(_mm_set1_pd(-0.0), xy), limit)) != 0 ||
                          (_mm_movemask_pd(_mm_cmpgt_pd(_mm_andnot_pd(_mm_set1_pd(-0.0), z), limit)) & 1) != 0;
        if (wide) {
            for (int i = 0; i < 3; ++i) {
                const __m128 axis = VxSIMDLoadFloat3(&m_Axis[i].x);
                const __m128d products = _mm_mul_pd(xy, _mm_cvtps_pd(axis));
                const __m128d dot = _mm_add_sd(_mm_add_sd(products, _mm_unpackhi_pd(products, products)),
                    _mm_mul_sd(z, _mm_cvtps_pd(_mm_movehl_ps(axis, axis))));
                const float projection = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), dot));
                if (XAbs(projection) > m_Extents[i]) return FALSE;
            }
        } else {
            const __m128 d = _mm_movelh_ps(_mm_cvtpd_ps(xy), _mm_cvtsd_ss(_mm_setzero_ps(), z));
            for (int i = 0; i < 3; ++i) {
                if (XAbs(_mm_cvtss_f32(VxSIMDDotProduct3(d, VxSIMDLoadFloat3(&m_Axis[i].x)))) > m_Extents[i]) return FALSE;
            }
        }
        return TRUE;
#elif !defined(VX_SIMD_SSE)
        const double x = double(iV.x) - m_Center.x, y = double(iV.y) - m_Center.y, z = double(iV.z) - m_Center.z;
        if (fabs(x) > double(FLT_MAX) * 0.25 || fabs(y) > double(FLT_MAX) * 0.25 || fabs(z) > double(FLT_MAX) * 0.25) {
            for (int i = 0; i < 3; ++i) {
                const float projection = float(x * m_Axis[i].x + y * m_Axis[i].y + z * m_Axis[i].z);
                if (XAbs(projection) > m_Extents[i]) return FALSE;
            }
            return TRUE;
        }
        const VxVector d{float(x), float(y), float(z)};
#else
        VxVector d = (iV - m_Center);
#endif

#if !defined(VX_SIMD_SSE2)
        float xRes = DotProduct(d, m_Axis[0]);
        if (XAbs(xRes) > m_Extents[0]) return FALSE;
        float yRes = DotProduct(d, m_Axis[1]);
        if (XAbs(yRes) > m_Extents[1]) return FALSE;
        float zRes = DotProduct(d, m_Axis[2]);
        if (XAbs(zRes) > m_Extents[2]) return FALSE;
        return TRUE;
#endif
    }

    /**
     * @brief Checks if an AABB is completely contained inside this OBB.
     * @param iB The AABB to check.
     * @return TRUE if the AABB is fully inside this OBB, FALSE otherwise.
     */
    XBOOL IsBoxInside(const VxBbox &iB) const {
        if (!VectorIn(iB.Max))
            return FALSE;
        if (!VectorIn(iB.Min))
            return FALSE;
        // Quick test on extrema passed
        // need to check all other 6 points
        VxVector tmp(iB.Min.x, iB.Min.y, iB.Max.z);
        if (!VectorIn(tmp)) return FALSE;
        tmp.y = iB.Max.y;
        if (!VectorIn(tmp)) return FALSE;
        tmp.z = iB.Min.z;
        if (!VectorIn(tmp)) return FALSE;
        tmp.x = iB.Max.x;
        if (!VectorIn(tmp)) return FALSE;
        tmp.y = iB.Min.y;
        if (!VectorIn(tmp)) return FALSE;
        tmp.z = iB.Min.z;
        if (!VectorIn(tmp)) return FALSE;
        return TRUE;
    }

    /// @brief Equality operator.
    bool operator==(const VxOBB &iBox) const {
        return (m_Extents == iBox.m_Extents) &&
            (m_Center == iBox.m_Center) &&
            (m_Axis[0] == iBox.m_Axis[0]) &&
            (m_Axis[1] == iBox.m_Axis[1]) &&
            (m_Axis[2] == iBox.m_Axis[2]);
    }

    /// The center of the box.
    VxVector m_Center;
    /// The three normalized axes defining the orientation of the box.
    VxVector m_Axis[3];
    /// The half-lengths of the box along each of its axes.
    VxVector m_Extents;
};

#endif // VxOBB_H
