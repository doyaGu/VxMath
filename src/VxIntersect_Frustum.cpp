#include "VxIntersect.h"

#include "VxVector.h"
#include "VxRay.h"
#include "VxPlane.h"
#include "VxFrustum.h"
#include "VxMatrix.h"
#include "VxSIMD.h"
#include "VxIntersectDispatchStateInternal.h"

#if defined(VX_SIMD_SSE)
static inline __m128 VxSIMDLoadVector3(const VxVector &v) {
    return VxSIMDLoadFloat3(&v.x);
}

static inline float VxSIMDDot3Scalar(__m128 a, __m128 b) {
    return _mm_cvtss_f32(VxSIMDDotProduct3(a, b));
}

static inline XBOOL VxSIMDFrustumOBBAxisTest(
    __m128 axisV,
    __m128 relCenterV,
    __m128 axis0V,
    __m128 axis1V,
    __m128 axis2V,
    float halfX,
    float halfY,
    float halfZ,
    __m128 frUpV,
    __m128 frRightV,
    __m128 frDirV,
    float uBound,
    float rBound,
    float dMin,
    float dRatio
) {
    const float c = VxSIMDDot3Scalar(relCenterV, axisV);
    const float r = XAbs(VxSIMDDot3Scalar(axisV, axis0V)) * halfX +
        XAbs(VxSIMDDot3Scalar(axisV, axis1V)) * halfY +
        XAbs(VxSIMDDot3Scalar(axisV, axis2V)) * halfZ;

    const float frBound = XAbs(VxSIMDDot3Scalar(axisV, frUpV)) * uBound +
        XAbs(VxSIMDDot3Scalar(axisV, frRightV)) * rBound;

    const float d = VxSIMDDot3Scalar(axisV, frDirV) * dMin;

    float frMin = d - frBound;
    if (frMin < 0.0f)
        frMin *= dRatio;

    float frMax = d + frBound;
    if (frMax > 0.0f)
        frMax *= dRatio;

    if (c + r < frMin)
        return FALSE;
    if (c - r > frMax)
        return FALSE;
    return TRUE;
}

static inline float VxSIMDPlaneClassifyFace(const VxPlane &plane, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2) {
    const __m128 n = VxSIMDLoadVector3(plane.m_Normal);
    const __m128 d = _mm_set1_ps(plane.m_D);

    const float c0 = _mm_cvtss_f32(_mm_add_ss(VxSIMDDotProduct3(n, VxSIMDLoadVector3(pt0)), d));
    const float c1 = _mm_cvtss_f32(_mm_add_ss(VxSIMDDotProduct3(n, VxSIMDLoadVector3(pt1)), d));
    const float c2 = _mm_cvtss_f32(_mm_add_ss(VxSIMDDotProduct3(n, VxSIMDLoadVector3(pt2)), d));

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
#endif

//--------- Frustum

// Intersection Frustum - Face
namespace {

typedef XBOOL (*VxIntersectFrustumFaceDispatchFn)(const VxFrustum &, const VxVector &, const VxVector &, const VxVector &);
typedef XBOOL (*VxIntersectFrustumOBBDispatchFn)(const VxFrustum &, const VxBbox &, const VxMatrix &);
typedef XBOOL (*VxIntersectFrustumBoxDispatchFn)(const VxFrustum &, const VxBbox &, const VxMatrix &);

static XBOOL FrustumFaceScalarCore(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2);
static XBOOL FrustumOBBScalarCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);
static XBOOL FrustumBoxScalarCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);

#if defined(VX_SIMD_SSE)
static XBOOL FrustumFaceSIMDCore(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2);
static XBOOL FrustumOBBSIMDCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);
static XBOOL FrustumBoxSIMDCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);
#endif

struct VxIntersectFrustumDispatchTable {
    VxIntersectFrustumFaceDispatchFn frustumFace;
    VxIntersectFrustumOBBDispatchFn frustumOBB;
    VxIntersectFrustumBoxDispatchFn frustumBox;
};

VxIntersectFrustumDispatchTable g_VxIntersectFrustumDispatch = {
    FrustumFaceScalarCore,
    FrustumOBBScalarCore,
    FrustumBoxScalarCore
};

static XBOOL FrustumFaceScalarCore(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2) {
    if (frustum.GetNearPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (frustum.GetFarPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (frustum.GetLeftPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (frustum.GetRightPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (frustum.GetBottomPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (frustum.GetUpPlane().ClassifyFace(pt0, pt1, pt2) > 0.0f)
        return FALSE;
    return TRUE;
}

static XBOOL FrustumOBBScalarCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    // It performs SAT-style interval overlap tests along:
    //  - the 3 box axes (matrix rows, normalized)
    //  - the frustum's 5 plane normals (far, bottom, up, left, right)

    // Box half-size in local space.
    VxVector halfSize = (box.Max - box.Min) * 0.5f;

    // Box center transformed to world, then expressed relative to frustum origin.
    VxVector localCenter = (box.Max + box.Min) * 0.5f;
    VxVector worldCenter;
    Vx3DMultiplyMatrixVector(&worldCenter, mat, &localCenter);
    VxVector relCenter = worldCenter - frustum.GetOrigin();

    // Box axes are the matrix rows (rotation+scale). Normalize them and fold scale into halfSize.
    VxVector axis0(mat[0].x, mat[0].y, mat[0].z);
    VxVector axis1(mat[1].x, mat[1].y, mat[1].z);
    VxVector axis2(mat[2].x, mat[2].y, mat[2].z);

    float len0 = sqrtf(SquareMagnitude(axis0));
    float len1 = sqrtf(SquareMagnitude(axis1));
    float len2 = sqrtf(SquareMagnitude(axis2));

    if (len0 != 0.0f) axis0 *= (1.0f / len0);
    if (len1 != 0.0f) axis1 *= (1.0f / len1);
    if (len2 != 0.0f) axis2 *= (1.0f / len2);

    halfSize.x *= len0;
    halfSize.y *= len1;
    halfSize.z *= len2;

    const VxVector &frDir = frustum.GetDir();
    const VxVector &frUp = frustum.GetUp();
    const VxVector &frRight = frustum.GetRight();
    const float rBound = frustum.GetRBound();
    const float uBound = frustum.GetUBound();
    const float dMin = frustum.GetDMin();
    const float dRatio = frustum.GetDRatio();

    auto axisTest = [&](const VxVector &axis) -> XBOOL {
        const float c = DotProduct(relCenter, axis);
        const float r = XAbs(DotProduct(axis, axis0)) * halfSize.x +
            XAbs(DotProduct(axis, axis1)) * halfSize.y +
            XAbs(DotProduct(axis, axis2)) * halfSize.z;

        const float frBound = XAbs(DotProduct(axis, frUp)) * uBound + XAbs(DotProduct(axis, frRight)) * rBound;

        const float d = DotProduct(axis, frDir) * dMin;

        float frMin = d - frBound;
        if (frMin < 0.0f)
            frMin *= dRatio;

        float frMax = d + frBound;
        if (frMax > 0.0f)
            frMax *= dRatio;

        if (c + r < frMin)
            return FALSE;
        if (c - r > frMax)
            return FALSE;
        return TRUE;
    };

    // Test along box axes.
    if (!axisTest(axis0)) return FALSE;
    if (!axisTest(axis1)) return FALSE;
    if (!axisTest(axis2)) return FALSE;

    // Test along frustum plane normals.
    if (!axisTest(frustum.GetFarPlane().GetNormal())) return FALSE;
    if (!axisTest(frustum.GetBottomPlane().GetNormal())) return FALSE;
    if (!axisTest(frustum.GetUpPlane().GetNormal())) return FALSE;
    if (!axisTest(frustum.GetLeftPlane().GetNormal())) return FALSE;
    if (!axisTest(frustum.GetRightPlane().GetNormal())) return FALSE;

    return TRUE;
}

static XBOOL FrustumBoxScalarCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    // Build a transformed oriented box representation (3 scaled axes + transformed center)
    // and reject if it lies strictly outside any frustum plane.
    VxVector axis[4];
    axis[0] = mat[0] * ((box.Max.x - box.Min.x) * 0.5f);
    axis[1] = mat[1] * ((box.Max.y - box.Min.y) * 0.5f);
    axis[2] = mat[2] * ((box.Max.z - box.Min.z) * 0.5f);

    VxVector center = box.GetCenter();
    Vx3DMultiplyMatrixVector(axis + 3, mat, &center);

    if (frustum.GetNearPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetFarPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetLeftPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetRightPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetBottomPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetUpPlane().XClassify(axis) > 0.0f) return FALSE;

    return TRUE;
}

#if defined(VX_SIMD_SSE)
static XBOOL FrustumFaceSIMDCore(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2) {
    if (VxSIMDPlaneClassifyFace(frustum.GetNearPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (VxSIMDPlaneClassifyFace(frustum.GetFarPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (VxSIMDPlaneClassifyFace(frustum.GetLeftPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (VxSIMDPlaneClassifyFace(frustum.GetRightPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (VxSIMDPlaneClassifyFace(frustum.GetBottomPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    if (VxSIMDPlaneClassifyFace(frustum.GetUpPlane(), pt0, pt1, pt2) > 0.0f)
        return FALSE;
    return TRUE;
}

static XBOOL FrustumOBBSIMDCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    VxVector halfSize = (box.Max - box.Min) * 0.5f;

    VxVector localCenter = (box.Max + box.Min) * 0.5f;
    VxVector worldCenter;
    Vx3DMultiplyMatrixVector(&worldCenter, mat, &localCenter);
    VxVector relCenter = worldCenter - frustum.GetOrigin();

    VxVector axis0(mat[0].x, mat[0].y, mat[0].z);
    VxVector axis1(mat[1].x, mat[1].y, mat[1].z);
    VxVector axis2(mat[2].x, mat[2].y, mat[2].z);

    const __m128 axis0V = VxSIMDLoadVector3(axis0);
    const __m128 axis1V = VxSIMDLoadVector3(axis1);
    const __m128 axis2V = VxSIMDLoadVector3(axis2);

    const float len0 = sqrtf(VxSIMDDot3Scalar(axis0V, axis0V));
    const float len1 = sqrtf(VxSIMDDot3Scalar(axis1V, axis1V));
    const float len2 = sqrtf(VxSIMDDot3Scalar(axis2V, axis2V));

    if (len0 != 0.0f) axis0 *= (1.0f / len0);
    if (len1 != 0.0f) axis1 *= (1.0f / len1);
    if (len2 != 0.0f) axis2 *= (1.0f / len2);

    halfSize.x *= len0;
    halfSize.y *= len1;
    halfSize.z *= len2;

    const float rBound = frustum.GetRBound();
    const float uBound = frustum.GetUBound();
    const float dMin = frustum.GetDMin();
    const float dRatio = frustum.GetDRatio();

    const __m128 relCenterV = VxSIMDLoadVector3(relCenter);
    const __m128 axis0N = VxSIMDLoadVector3(axis0);
    const __m128 axis1N = VxSIMDLoadVector3(axis1);
    const __m128 axis2N = VxSIMDLoadVector3(axis2);
    const __m128 frUpV = VxSIMDLoadVector3(frustum.GetUp());
    const __m128 frRightV = VxSIMDLoadVector3(frustum.GetRight());
    const __m128 frDirV = VxSIMDLoadVector3(frustum.GetDir());

    if (!VxSIMDFrustumOBBAxisTest(axis0N, relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(axis1N, relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(axis2N, relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;

    if (!VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(frustum.GetFarPlane().GetNormal()), relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(frustum.GetBottomPlane().GetNormal()), relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(frustum.GetUpPlane().GetNormal()), relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(frustum.GetLeftPlane().GetNormal()), relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;
    if (!VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(frustum.GetRightPlane().GetNormal()), relCenterV, axis0N, axis1N, axis2N, halfSize.x, halfSize.y, halfSize.z, frUpV, frRightV, frDirV, uBound, rBound, dMin, dRatio))
        return FALSE;

    return TRUE;
}

static XBOOL FrustumBoxSIMDCore(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    VxVector axis[4];

    const __m128 minV = VxSIMDLoadVector3(box.Min);
    const __m128 maxV = VxSIMDLoadVector3(box.Max);
    const __m128 half = _mm_mul_ps(_mm_sub_ps(maxV, minV), _mm_set1_ps(0.5f));

    const float halfX = _mm_cvtss_f32(half);
    const float halfY = _mm_cvtss_f32(_mm_shuffle_ps(half, half, _MM_SHUFFLE(1, 1, 1, 1)));
    const float halfZ = _mm_cvtss_f32(_mm_shuffle_ps(half, half, _MM_SHUFFLE(2, 2, 2, 2)));

    const __m128 axis0 = _mm_mul_ps(VxSIMDLoadVector3(mat[0]), _mm_set1_ps(halfX));
    const __m128 axis1 = _mm_mul_ps(VxSIMDLoadVector3(mat[1]), _mm_set1_ps(halfY));
    const __m128 axis2 = _mm_mul_ps(VxSIMDLoadVector3(mat[2]), _mm_set1_ps(halfZ));

    VxSIMDStoreFloat3(&axis[0].x, axis0);
    VxSIMDStoreFloat3(&axis[1].x, axis1);
    VxSIMDStoreFloat3(&axis[2].x, axis2);

    const __m128 center = _mm_mul_ps(_mm_add_ps(minV, maxV), _mm_set1_ps(0.5f));
    const __m128 worldCenter = VxSIMDMatrixMultiplyVector3(&mat[0][0], center);
    VxSIMDStoreFloat3(&axis[3].x, worldCenter);

    if (frustum.GetNearPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetFarPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetLeftPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetRightPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetBottomPlane().XClassify(axis) > 0.0f) return FALSE;
    if (frustum.GetUpPlane().XClassify(axis) > 0.0f) return FALSE;

    return TRUE;
}
#endif

} // namespace

void VxIntersectFrustumDispatchRebuild(bool useSIMD) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectFrustumDispatch.frustumFace = useSIMD ? FrustumFaceSIMDCore : FrustumFaceScalarCore;
    g_VxIntersectFrustumDispatch.frustumOBB = useSIMD ? FrustumOBBSIMDCore : FrustumOBBScalarCore;
    g_VxIntersectFrustumDispatch.frustumBox = useSIMD ? FrustumBoxSIMDCore : FrustumBoxScalarCore;
#else
    (void) useSIMD;
    g_VxIntersectFrustumDispatch.frustumFace = FrustumFaceScalarCore;
    g_VxIntersectFrustumDispatch.frustumOBB = FrustumOBBScalarCore;
    g_VxIntersectFrustumDispatch.frustumBox = FrustumBoxScalarCore;
#endif
}

XBOOL VxIntersect::FrustumFace(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2) {
    return g_VxIntersectFrustumDispatch.frustumFace(frustum, pt0, pt1, pt2);
}

// Intersection Frustum - AABB
XBOOL VxIntersect::FrustumAABB(const VxFrustum &frustum, const VxBbox &box) {
    VxMatrix id;
    id.SetIdentity();
    return FrustumOBB(frustum, box, id);
}

XBOOL VxIntersect::FrustumOBB(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    return g_VxIntersectFrustumDispatch.frustumOBB(frustum, box, mat);
}

// Intersection Frustum - Box (deprecated, alias for FrustumOBB)
XBOOL VxIntersect::FrustumBox(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat) {
    return g_VxIntersectFrustumDispatch.frustumBox(frustum, box, mat);
}



