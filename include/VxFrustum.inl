/**
 * @file VxFrustum.inl
 * @brief Inline implementations for VxFrustum plus SIMD-accelerated frustum and
 *        box-projection operations.
 *
 * Part 1: Scalar inline bodies (moved from VxFrustum.h).
 * Part 2: SIMD-accelerated frustum update, compute vertices, transform,
 *         and box projection utilities (TransformBox2D, ProjectBoxZExtents).
 *
 * Included automatically at the bottom of VxFrustum.h - do not include directly.
 */

#pragma once

#include "VxSIMD.h"
#include "VxRect.h"

#if defined(VX_SIMD_SSE)
VX_SIMD_INLINE void VxSIMDFrustumUpdate(VxFrustum *frustum) noexcept;
VX_SIMD_INLINE void VxSIMDFrustumComputeVertices(const VxFrustum *frustum, VxVector *vertices) noexcept;
VX_SIMD_INLINE void VxSIMDFrustumTransform(VxFrustum *frustum, const VxMatrix *invWorldMat) noexcept;
#endif

// =============================================================================
// Part 1 - Scalar Inline Implementations
// =============================================================================

inline VxVector &VxFrustum::GetOrigin() {
    return m_Origin;
}

inline const VxVector &VxFrustum::GetOrigin() const {
    return m_Origin;
}

inline VxVector &VxFrustum::GetRight() {
    return m_Right;
}

inline const VxVector &VxFrustum::GetRight() const {
    return m_Right;
}

inline VxVector &VxFrustum::GetUp() {
    return m_Up;
}

inline const VxVector &VxFrustum::GetUp() const {
    return m_Up;
}

inline VxVector &VxFrustum::GetDir() {
    return m_Dir;
}

inline const VxVector &VxFrustum::GetDir() const {
    return m_Dir;
}

inline float &VxFrustum::GetRBound() {
    return m_RBound;
}

inline const float &VxFrustum::GetRBound() const {
    return m_RBound;
}

inline float &VxFrustum::GetUBound() {
    return m_UBound;
}

inline const float &VxFrustum::GetUBound() const {
    return m_UBound;
}

inline float &VxFrustum::GetDMin() {
    return m_DMin;
}

inline const float &VxFrustum::GetDMin() const {
    return m_DMin;
}

inline float &VxFrustum::GetDMax() {
    return m_DMax;
}

inline const float &VxFrustum::GetDMax() const {
    return m_DMax;
}

inline float VxFrustum::GetDRatio() const {
    return m_DRatio;
}

inline float VxFrustum::GetRF() const {
    return m_RF;
}

inline float VxFrustum::GetUF() const {
    return m_UF;
}

inline const VxPlane &VxFrustum::GetNearPlane() const {
    return m_NearPlane;
}

inline const VxPlane &VxFrustum::GetFarPlane() const {
    return m_FarPlane;
}

inline const VxPlane &VxFrustum::GetLeftPlane() const {
    return m_LeftPlane;
}

inline const VxPlane &VxFrustum::GetRightPlane() const {
    return m_RightPlane;
}

inline const VxPlane &VxFrustum::GetUpPlane() const {
    return m_UpPlane;
}

inline const VxPlane &VxFrustum::GetBottomPlane() const {
    return m_BottomPlane;
}

inline VxFrustum::VxFrustum()
    : m_Origin(VxVector::axis0()),
      m_Right(VxVector::axisX()),
      m_Up(VxVector::axisY()),
      m_Dir(VxVector::axisZ()),
      m_RBound(1.0f),
      m_UBound(1.0f),
      m_DMin(1.0f),
      m_DMax(2.0f),
      m_DRatio(0.0f),
      m_RF(0.0f),
      m_UF(0.0f) { Update(); }

inline VxFrustum::VxFrustum(const VxVector &origin, const VxVector &right, const VxVector &up, const VxVector &dir,
                             float nearplane, float farplane, float fov, float aspectratio)
    : m_Origin(origin),
      m_Right(right),
      m_Up(up),
      m_Dir(dir),
      m_RBound(tanf(fov * 0.5f) * nearplane),
      m_UBound(tanf(fov * 0.5f) * nearplane * aspectratio),
      m_DMin(nearplane),
      m_DMax(farplane),
      m_DRatio(0.0f),
      m_RF(0.0f),
      m_UF(0.0f) { Update(); }

inline XDWORD VxFrustum::Classify(const VxVector &v) const {
    XDWORD flags = 0;
    if (GetNearPlane().Classify(v) > 0.0f) flags |= VXCLIP_FRONT;
    else if (GetFarPlane().Classify(v) > 0.0f) flags |= VXCLIP_BACK;
    if (GetLeftPlane().Classify(v) > 0.0f) flags |= VXCLIP_LEFT;
    else if (GetRightPlane().Classify(v) > 0.0f) flags |= VXCLIP_RIGHT;
    if (GetBottomPlane().Classify(v) > 0.0f) flags |= VXCLIP_BOTTOM;
    else if (GetUpPlane().Classify(v) > 0.0f) flags |= VXCLIP_TOP;
    return flags;
}

inline float VxFrustum::Classify(const VxBbox &v) const {
    float cumul = 1.0f;
    float f = m_NearPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    f = m_FarPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    f = m_LeftPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    f = m_RightPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    f = m_UpPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    f = m_BottomPlane.Classify(v);
    if (f > 0.0f) return f;
    cumul *= f;
    return -cumul;
}

inline float VxFrustum::Classify(const VxBbox &b, const VxMatrix &mat) const {
    float cumul = 1.0f;

    VxVector axis[4];
    axis[0] = mat[0] * ((b.Max.x - b.Min.x) * 0.5f);
    axis[1] = mat[1] * ((b.Max.y - b.Min.y) * 0.5f);
    axis[2] = mat[2] * ((b.Max.z - b.Min.z) * 0.5f);
    VxVector v = b.GetCenter();
    Vx3DMultiplyMatrixVector(axis + 3, mat, &v);

    float f = XClassify(axis, m_NearPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    f = XClassify(axis, m_FarPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    f = XClassify(axis, m_LeftPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    f = XClassify(axis, m_RightPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    f = XClassify(axis, m_UpPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    f = XClassify(axis, m_BottomPlane);
    if (f > 0.0f) return f;
    cumul *= f;
    return -cumul;
}

inline XBOOL VxFrustum::IsInside(const VxVector &v) const {
    if (GetNearPlane().Classify(v) > 0.0f) return FALSE;
    if (GetFarPlane().Classify(v) > 0.0f) return FALSE;
    if (GetLeftPlane().Classify(v) > 0.0f) return FALSE;
    if (GetRightPlane().Classify(v) > 0.0f) return FALSE;
    if (GetBottomPlane().Classify(v) > 0.0f) return FALSE;
    if (GetUpPlane().Classify(v) > 0.0f) return FALSE;
    return TRUE;
}

inline void VxFrustum::Update() {
    m_DRatio = m_DMax / m_DMin;
    m_RF = m_RBound * m_DMax * -2.0f;
    m_UF = m_UBound * m_DMax * -2.0f;

    VxVector nearDirVec = m_Dir * m_DMin;
    VxVector upVec = m_Up * m_UBound;
    VxVector rightVec = m_Right * m_RBound;

    VxVector nbl_rel = nearDirVec - rightVec - upVec;
    VxVector ntl_rel = nearDirVec - rightVec + upVec;
    VxVector nbr_rel = nearDirVec + rightVec - upVec;
    VxVector ntr_rel = nearDirVec + rightVec + upVec;

    VxVector nbl = m_Origin + nbl_rel;
    VxVector farVertex = m_Origin + ntr_rel * m_DRatio;

    VxVector nearNormal = -m_Dir;
    m_NearPlane.Create(nearNormal, nbl);
    m_FarPlane.Create(m_Dir, farVertex);

    // Construct in relative coordinates: translation must not erase small edges.
    // The triangle kernel normalizes in double (packed double on SSE2), without
    // the vector Normalize epsilon or a float squared-cross overflow/underflow.
    const VxVector zero(0.0f, 0.0f, 0.0f);
    m_LeftPlane.Create(zero, nbl_rel, ntl_rel);
    m_LeftPlane.Create(m_LeftPlane.m_Normal, m_Origin);
    m_RightPlane.Create(zero, ntr_rel, nbr_rel);
    m_RightPlane.Create(m_RightPlane.m_Normal, m_Origin);
    m_BottomPlane.Create(zero, nbr_rel, nbl_rel);
    m_BottomPlane.Create(m_BottomPlane.m_Normal, m_Origin);
    m_UpPlane.Create(zero, ntl_rel, ntr_rel);
    m_UpPlane.Create(m_UpPlane.m_Normal, m_Origin);
}

inline void VxFrustum::ComputeVertices(VxVector vertices[8]) const {
#if defined(VX_SIMD_SSE)
    VxSIMDFrustumComputeVertices(this, vertices);
#else
    VxVector nearDirVec = m_Dir * m_DMin;
    VxVector rightVec = m_Right * m_RBound;
    VxVector upVec = m_Up * m_UBound;

    VxVector leftVec = nearDirVec - rightVec;
    VxVector rightVec2 = nearDirVec + rightVec;

    vertices[0] = leftVec - upVec;
    vertices[1] = leftVec + upVec;
    vertices[2] = rightVec2 + upVec;
    vertices[3] = rightVec2 - upVec;

    for (int i = 0; i < 4; i++) {
        VxVector nearVec = vertices[i];
        vertices[i + 4] = m_Origin + nearVec * m_DRatio;
        vertices[i] += m_Origin;
    }
#endif
}

inline void VxFrustum::Transform(const VxMatrix &invworldmat) {
#if defined(VX_SIMD_SSE)
    VxSIMDFrustumTransform(this, &invworldmat);
#else
    m_Right *= m_RBound;
    m_Up *= m_UBound;
    m_Dir *= m_DMin;

    VxVector newOrigin;
    Vx3DMultiplyMatrixVector(&newOrigin, invworldmat, &m_Origin);
    m_Origin = newOrigin;

    VxVector resultVectors[3];
    Vx3DRotateVectorMany(resultVectors, invworldmat, &m_Right, 3, sizeof(VxVector));

    double lengths[3];
    for (int i = 0; i < 3; ++i) {
        const double x = resultVectors[i].x, y = resultVectors[i].y, z = resultVectors[i].z;
        lengths[i] = sqrt(x * x + y * y + z * z);
        if (lengths[i] > 0.0) {
            resultVectors[i].Set(float(x / lengths[i]), float(y / lengths[i]), float(z / lengths[i]));
        } else {
            resultVectors[i] = VxVector::axis0();
        }
    }
    const float newRBound = float(lengths[0]);
    const float newUBound = float(lengths[1]);
    const float newDMin   = float(lengths[2]);

    m_RBound = newRBound;
    m_UBound = newUBound;
    m_DMax   = newDMin * m_DRatio;
    m_DMin   = newDMin;

    m_Right = resultVectors[0];
    m_Up    = resultVectors[1];
    m_Dir   = resultVectors[2];

    Update();
#endif
}

inline bool VxFrustum::operator==(const VxFrustum &iFrustum) const {
    return (m_Origin == iFrustum.m_Origin) &&
           (m_Right == iFrustum.m_Right) &&
           (m_Up == iFrustum.m_Up) &&
           (m_Dir == iFrustum.m_Dir) &&
           (m_RBound == iFrustum.m_RBound) &&
           (m_UBound == iFrustum.m_UBound) &&
           (m_DMin == iFrustum.m_DMin) &&
           (m_DMax == iFrustum.m_DMax);
}

inline float VxFrustum::XClassify(const VxVector axis[4], const VxPlane &plane) {
    return plane.XClassify(axis);
}

// =============================================================================
// Part 2 - SIMD-Accelerated Frustum & Box Projection Operations
// =============================================================================

#if defined(VX_SIMD_SSE)

#include "VxSIMD.h"

VX_SIMD_INLINE void VxSIMDFrustumUpdate(VxFrustum *frustum) noexcept {
    frustum->Update();
}

VX_SIMD_INLINE void VxSIMDFrustumComputeVertices(const VxFrustum *frustum, VxVector *vertices) noexcept {
    const __m128 dir = VxSIMDLoadFloat3(&frustum->GetDir().x);
    const __m128 right = VxSIMDLoadFloat3(&frustum->GetRight().x);
    const __m128 up = VxSIMDLoadFloat3(&frustum->GetUp().x);
    const __m128 origin = VxSIMDLoadFloat3(&frustum->GetOrigin().x);
    const __m128 dMin = _mm_set1_ps(frustum->GetDMin());
    const __m128 rBound = _mm_set1_ps(frustum->GetRBound());
    const __m128 uBound = _mm_set1_ps(frustum->GetUBound());
    const __m128 dRatio = _mm_set1_ps(frustum->GetDRatio());

    const __m128 nearDirVec = _mm_mul_ps(dir, dMin);
    const __m128 rightVec = _mm_mul_ps(right, rBound);
    const __m128 upVec = _mm_mul_ps(up, uBound);

    const __m128 leftVec = _mm_sub_ps(nearDirVec, rightVec);
    const __m128 rightVec2 = _mm_add_ps(nearDirVec, rightVec);

    __m128 nearCorners[4];
    nearCorners[0] = _mm_sub_ps(leftVec, upVec);
    nearCorners[1] = _mm_add_ps(leftVec, upVec);
    nearCorners[2] = _mm_add_ps(rightVec2, upVec);
    nearCorners[3] = _mm_sub_ps(rightVec2, upVec);

    for (int i = 0; i < 4; ++i) {
        const __m128 farVec = _mm_add_ps(origin, _mm_mul_ps(nearCorners[i], dRatio));
        const __m128 nearVec = _mm_add_ps(origin, nearCorners[i]);
        VxSIMDStoreFloat3(&vertices[i].x, nearVec);
        VxSIMDStoreFloat3(&vertices[i + 4].x, farVec);
    }
}

VX_SIMD_INLINE void VxSIMDFrustumTransform(VxFrustum *frustum, const VxMatrix *invWorldMat) noexcept {
    VxVector &right = frustum->GetRight();
    VxVector &up = frustum->GetUp();
    VxVector &dir = frustum->GetDir();
    VxVector &origin = frustum->GetOrigin();
    float &rBound = frustum->GetRBound();
    float &uBound = frustum->GetUBound();
    float &dMin = frustum->GetDMin();
    float &dMax = frustum->GetDMax();
    const float dRatio = frustum->GetDRatio();

    const float *matData = reinterpret_cast<const float *>(&(*invWorldMat)[0][0]);
    const __m128 rightScaled = _mm_mul_ps(VxSIMDLoadFloat3(&right.x), _mm_set1_ps(rBound));
    const __m128 upScaled = _mm_mul_ps(VxSIMDLoadFloat3(&up.x), _mm_set1_ps(uBound));
    const __m128 dirScaled = _mm_mul_ps(VxSIMDLoadFloat3(&dir.x), _mm_set1_ps(dMin));

    const __m128 newOrigin = VxSIMDMatrixMultiplyVector3(matData, VxSIMDLoadFloat3(&origin.x));
    VxSIMDStoreFloat3(&origin.x, newOrigin);

    const __m128 rotatedRight = VxSIMDMatrixRotateVector3(matData, rightScaled);
    const __m128 rotatedUp = VxSIMDMatrixRotateVector3(matData, upScaled);
    const __m128 rotatedDir = VxSIMDMatrixRotateVector3(matData, dirScaled);

#if defined(VX_SIMD_SSE2)
    const __m128 rotated[3] = {rotatedRight, rotatedUp, rotatedDir};
    VxVector *axes[3] = {&right, &up, &dir};
    float lengths[3];
    for (int i = 0; i < 3; ++i) {
        const __m128d xy = _mm_cvtps_pd(rotated[i]);
        const __m128d z = _mm_cvtps_pd(_mm_movehl_ps(rotated[i], rotated[i]));
        const __m128d squareXY = _mm_mul_pd(xy, xy);
        const __m128d sum = _mm_add_sd(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY)), _mm_mul_sd(z, z));
        const __m128d length = _mm_sqrt_sd(sum, sum);
        lengths[i] = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), length));
        if (_mm_comigt_sd(length, _mm_setzero_pd())) {
            const __m128 normalizedXY = _mm_cvtpd_ps(_mm_div_pd(xy, _mm_unpacklo_pd(length, length)));
            const __m128 normalizedZ = _mm_cvtsd_ss(_mm_setzero_ps(), _mm_div_sd(z, length));
            VxSIMDStoreFloat3(&axes[i]->x, _mm_movelh_ps(normalizedXY, normalizedZ));
        } else {
            *axes[i] = VxVector::axis0();
        }
    }
    rBound = lengths[0];
    uBound = lengths[1];
    dMin = lengths[2];
    dMax = lengths[2] * dRatio;
#else
    const float newRBound = VxSIMDLength3(rotatedRight);
    const float newUBound = VxSIMDLength3(rotatedUp);
    const float newDMin = VxSIMDLength3(rotatedDir);

    rBound = newRBound;
    uBound = newUBound;
    dMin = newDMin;
    dMax = newDMin * dRatio;

    if (newRBound > 0.0f) {
        VxSIMDStoreFloat3(&right.x, _mm_mul_ps(rotatedRight, _mm_set1_ps(1.0f / newRBound)));
    } else {
        right = VxVector::axis0();
    }
    if (newUBound > 0.0f) {
        VxSIMDStoreFloat3(&up.x, _mm_mul_ps(rotatedUp, _mm_set1_ps(1.0f / newUBound)));
    } else {
        up = VxVector::axis0();
    }
    if (newDMin > 0.0f) {
        VxSIMDStoreFloat3(&dir.x, _mm_mul_ps(rotatedDir, _mm_set1_ps(1.0f / newDMin)));
    } else {
        dir = VxVector::axis0();
    }
#endif

    frustum->Update();
}

//-------------------------------------------------------------------------------------
// Box Projection Operations
//-------------------------------------------------------------------------------------

VX_SIMD_INLINE XBOOL VxSIMDTransformBox2D(const VxMatrix *worldProjection, const VxBbox *box, VxRect *extents, const VxRect *screenSize, VXCLIP_FLAGS *orClipFlags, VXCLIP_FLAGS *andClipFlags) noexcept {
    if (!box || !box->IsValid()) {
        if (orClipFlags) *orClipFlags = static_cast<VXCLIP_FLAGS>(VXCLIP_ALL);
        if (andClipFlags) *andClipFlags = static_cast<VXCLIP_FLAGS>(VXCLIP_ALL);
        return FALSE;
    }
    const double low[3] = {box->Min.x, box->Min.y, box->Min.z};
    const double high[3] = {box->Max.x, box->Max.y, box->Max.z};
    const bool flat = static_cast<float>(high[2] - low[2]) == 0;
    const int count = flat ? 4 : 8;
    const int xMask = flat ? 1 : 4, yMask = 2, zMask = flat ? 0 : 1;
    __m128d terms[2][3][2], translation[2];
    for (int pair = 0; pair < 2; ++pair) {
        translation[pair] = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&(*worldProjection)[3][pair*2]))));
        for (int row = 0; row < 3; ++row) {
            const __m128d coefficients = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&(*worldProjection)[row][pair*2]))));
            terms[pair][row][0] = _mm_mul_pd(coefficients,_mm_set1_pd(low[row]));
            terms[pair][row][1] = _mm_mul_pd(coefficients,_mm_set1_pd(high[row]));
        }
    }
    const __m128d zero = _mm_setzero_pd(), sign = _mm_set1_pd(-0.0);
    __m128d xy[8]; double ws[8];
    unsigned combined = 0, common = 0xffffffffu;
    for (int i = 0; i < count; ++i) {
        __m128d point[2];
        for (int pair = 0; pair < 2; ++pair) {
            const __m128d x = terms[pair][0][(i & xMask) != 0];
            const __m128d y = terms[pair][1][(i & yMask) != 0];
            const __m128d z = terms[pair][2][(i & zMask) != 0];
            point[pair] = _mm_add_pd(_mm_add_pd(_mm_add_pd(x, y), z), translation[pair]);
        }
        const __m128d w = _mm_unpackhi_pd(point[1],point[1]);
        const __m128d negative = _mm_cmplt_pd(point[0],_mm_sub_pd(zero,w));
        const __m128d positive = _mm_cmpgt_pd(point[0],w);
        // Gather four side predicates and two depth predicates with two
        // vector-to-integer transfers instead of one transfer per comparison.
        const unsigned sides = _mm_movemask_ps(_mm_shuffle_ps(_mm_castpd_ps(negative),_mm_castpd_ps(positive),_MM_SHUFFLE(2,0,2,0)));
        const __m128d depth = _mm_unpacklo_pd(_mm_cmplt_sd(point[1],zero),_mm_cmpgt_sd(point[1],w));
        unsigned flags = ((sides & 1) << 4) | ((sides & 2) << 6) | ((sides & 12) << 3) | (_mm_movemask_pd(depth) << 8);
        combined |= flags; common &= flags;
        xy[i] = point[0]; ws[i] = _mm_cvtsd_f64(w);
    }
    if (extents && screenSize) {
        const double halfX = (double(screenSize->right) - screenSize->left) * 0.5;
        const double halfY = (double(screenSize->bottom) - screenSize->top) * 0.5;
        const __m128d scale = _mm_set_pd(-halfY,halfX);
        const __m128d center = _mm_set_pd(screenSize->top+halfY,screenSize->left+halfX);
        __m128 minimum = _mm_set1_ps(1.0e10f), maximum = _mm_set1_ps(-1.0e10f);
        if (!common) for (int i = 0; i < count; ++i) {
            const __m128d denominator = _mm_andnot_pd(sign,_mm_set1_pd(ws[i]));
            const __m128 projected = _mm_cvtpd_ps(_mm_add_pd(_mm_mul_pd(_mm_div_pd(xy[i],denominator),scale),center));
            minimum = _mm_min_ps(minimum,projected);
            maximum = _mm_max_ps(maximum,projected);
        }
        float lo[2],hi[2];
        _mm_storel_epi64(reinterpret_cast<__m128i *>(lo),_mm_castps_si128(minimum));
        _mm_storel_epi64(reinterpret_cast<__m128i *>(hi),_mm_castps_si128(maximum));
        extents->left = lo[0]; extents->right = hi[0]; extents->top = lo[1]; extents->bottom = hi[1];
        // Preserve the original live viewport reads after the extent writes.
        if (combined & VXCLIP_FRONT) {
            if (combined & VXCLIP_LEFT) extents->left = screenSize->left;
            if (combined & VXCLIP_RIGHT) extents->right = screenSize->right;
            if (combined & VXCLIP_TOP) extents->top = screenSize->top;
            if (combined & VXCLIP_BOTTOM) extents->bottom = screenSize->bottom;
        }
    }
    if (orClipFlags) *orClipFlags = static_cast<VXCLIP_FLAGS>(combined);
    if (andClipFlags) *andClipFlags = static_cast<VXCLIP_FLAGS>(common);
    return common == 0;
}

VX_SIMD_INLINE void VxSIMDProjectBoxZExtents(const VxMatrix *worldProjection, const VxBbox *box, float *zhMin, float *zhMax) noexcept {
    if (!zhMin || !zhMax) return;
    if (!box || !box->IsValid()) {
        *zhMin = 1.0e10f;
        *zhMax = -1.0e10f;
        return;
    }
    const double low[3] = {box->Min.x, box->Min.y, box->Min.z};
    const double high[3] = {box->Max.x, box->Max.y, box->Max.z};
    int xMask = 4, yMask = 2, zMask = 1, count = 8;
    if (static_cast<float>(high[2] - low[2]) <= EPSILON) {
        xMask = 1; zMask = 0; count = 4;
    } else if (static_cast<float>(high[0] - low[0]) <= EPSILON) {
        xMask = 0; count = 4;
    } else if (static_cast<float>(high[1] - low[1]) <= EPSILON) {
        xMask = 2; yMask = 0; count = 4;
    }
    // Only Z/W columns are needed. Packed double preserves finite ratios even
    // when float products, box dimensions, or intermediate coordinates overflow.
    __m128d terms[3][2];
    for (int row = 0; row < 3; ++row) {
        const __m128d zw = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&(*worldProjection)[row][2]))));
        terms[row][0] = _mm_mul_pd(zw, _mm_set1_pd(low[row]));
        terms[row][1] = _mm_mul_pd(zw, _mm_set1_pd(high[row]));
    }
    const __m128d translation = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&(*worldProjection)[3][2]))));
    const __m128d zero = _mm_setzero_pd(), one = _mm_set1_pd(1);
    float projected[8];
    for (int i = 0; i < count; i += 2) {
        __m128d points[2];
        for (int point = 0; point < 2; ++point) {
            const int corner = i + point;
            const __m128d components[4] = {terms[0][(corner & xMask) != 0], terms[1][(corner & yMask) != 0], terms[2][(corner & zMask) != 0], translation};
            __m128d sum = _mm_add_pd(_mm_add_pd(_mm_add_pd(components[0],components[1]),components[2]),components[3]);
            const __m128d sign = _mm_set1_pd(-0.0);
            const __m128d magnitude = _mm_add_pd(_mm_add_pd(_mm_andnot_pd(sign,components[0]),_mm_andnot_pd(sign,components[1])),_mm_add_pd(_mm_andnot_pd(sign,components[2]),_mm_andnot_pd(sign,components[3])));
            if (_mm_movemask_pd(_mm_cmplt_pd(_mm_andnot_pd(sign,sum),_mm_mul_pd(magnitude,_mm_set1_pd(1.0 / 16777216.0))))) {
                __m128d correction = zero;
                sum = components[0];
                for (int row = 1; row < 4; ++row) {
                    const __m128d next = _mm_add_pd(sum,components[row]);
                    const __m128d larger = _mm_cmpge_pd(_mm_andnot_pd(sign,sum),_mm_andnot_pd(sign,components[row]));
                    const __m128d errorA = _mm_add_pd(_mm_sub_pd(sum,next),components[row]);
                    const __m128d errorB = _mm_add_pd(_mm_sub_pd(components[row],next),sum);
                    correction = _mm_add_pd(correction,_mm_or_pd(_mm_and_pd(larger,errorA),_mm_andnot_pd(larger,errorB)));
                    sum = next;
                }
                sum = _mm_add_pd(sum,correction);
            }
            points[point] = sum;
        }
        const __m128d a = points[0], b = points[1];
        const __m128d z = _mm_unpacklo_pd(a,b), w = _mm_unpackhi_pd(a,b);
        const __m128d front = _mm_cmpnlt_pd(w,zero);
        const __m128d denominator = _mm_or_pd(_mm_and_pd(front,w),_mm_andnot_pd(front,one));
        const __m128 value = _mm_cvtpd_ps(_mm_div_pd(_mm_and_pd(front,z),denominator));
        _mm_storel_epi64(reinterpret_cast<__m128i *>(projected+i),_mm_castps_si128(value));
    }
    if (zhMin == zhMax) {
        *zhMin = 1.0e10f;
        *zhMax = -1.0e10f;
        for (int i = 0; i < count; ++i) {
            if (projected[i] < *zhMin) *zhMin = projected[i];
            if (projected[i] > *zhMax) *zhMax = projected[i];
        }
        return;
    }
    float minimum = 1.0e10f, maximum = -1.0e10f;
    for (int i = 0; i < count; ++i) {
        if (projected[i] < minimum) minimum = projected[i];
        if (projected[i] > maximum) maximum = projected[i];
    }
    *zhMin = minimum;
    *zhMax = maximum;
}

#endif // VX_SIMD_SSE
