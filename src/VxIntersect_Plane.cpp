#include "VxIntersect.h"

#include "VxVector.h"
#include "VxRay.h"
#include "VxPlane.h"
#include "VxSIMD.h"
#include "VxIntersectDispatchStateInternal.h"

constexpr float kPlaneParallelEps = EPSILON;
constexpr float kSegmentMaxT = 1.0f + EPSILON;

#if defined(VX_SIMD_SSE)
static inline float VxSIMDExtractX(__m128 v) {
    return _mm_cvtss_f32(v);
}

static inline __m128 VxSIMDAbs3(__m128 v) {
    return _mm_and_ps(v, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
}

static inline float VxSIMDDot3Scalar(const VxVector& a, const VxVector& b) {
    const __m128 av = VxSIMDLoadFloat3(&a.x);
    const __m128 bv = VxSIMDLoadFloat3(&b.x);
    return VxSIMDExtractX(VxSIMDDotProduct3(av, bv));
}

static inline VxVector VxSIMDPointOnRay(const VxRay& ray, float t) {
    VxVector point;
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 pointV = _mm_add_ps(origin, _mm_mul_ps(direction, _mm_set1_ps(t)));
    VxSIMDStoreFloat3(&point.x, pointV);
    return point;
}
#endif

namespace {

typedef XBOOL (*VxIntersectRayPlaneDispatchFn)(const VxRay &, const VxPlane &, VxVector &, float &);
typedef XBOOL (*VxIntersectBoxPlaneDispatchFn)(const VxBbox &, const VxPlane &);
typedef XBOOL (*VxIntersectBoxPlaneMatrixDispatchFn)(const VxBbox &, const VxMatrix &, const VxPlane &);
typedef XBOOL (*VxIntersectPlanesDispatchFn)(const VxPlane &, const VxPlane &, const VxPlane &, VxVector &);

static XBOOL RayPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL RayPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL SegmentPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL SegmentPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL LinePlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL BoxPlaneScalarCore(const VxBbox &box, const VxPlane &plane);
static XBOOL BoxPlaneMatrixScalarCore(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane);
static XBOOL PlanesScalarCore(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p);

#if defined(VX_SIMD_SSE)
static XBOOL RayPlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL RayPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL SegmentPlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL SegmentPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL LinePlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
static XBOOL BoxPlaneSIMDCore(const VxBbox &box, const VxPlane &plane);
static XBOOL BoxPlaneMatrixSIMDCore(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane);
static XBOOL PlanesSIMDCore(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p);
#endif

struct VxIntersectPlaneDispatchTable {
    VxIntersectRayPlaneDispatchFn rayPlane;
    VxIntersectRayPlaneDispatchFn rayPlaneCulled;
    VxIntersectRayPlaneDispatchFn segmentPlane;
    VxIntersectRayPlaneDispatchFn segmentPlaneCulled;
    VxIntersectRayPlaneDispatchFn linePlane;
    VxIntersectBoxPlaneDispatchFn boxPlane;
    VxIntersectBoxPlaneMatrixDispatchFn boxPlaneMatrix;
    VxIntersectPlanesDispatchFn planes;
};

VxIntersectPlaneDispatchTable g_VxIntersectPlaneDispatch = {
    RayPlaneScalarCore,
    RayPlaneCulledScalarCore,
    SegmentPlaneScalarCore,
    SegmentPlaneCulledScalarCore,
    LinePlaneScalarCore,
    BoxPlaneScalarCore,
    BoxPlaneMatrixScalarCore,
    PlanesScalarCore
};

static XBOOL RayPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps)
        return FALSE;

    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
}

static XBOOL RayPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);
    if (denom > -kPlaneParallelEps)
        return FALSE;

    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps)
        return FALSE;

    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
}

static XBOOL SegmentPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps)
        return FALSE;
    if (dist > kSegmentMaxT)
        return FALSE;

    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
}

static XBOOL SegmentPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);
    if (denom > -kPlaneParallelEps)
        return FALSE;

    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps)
        return FALSE;
    if (dist > kSegmentMaxT)
        return FALSE;

    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
}

static XBOOL LinePlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
}

static XBOOL BoxPlaneScalarCore(const VxBbox &box, const VxPlane &plane) {
    VxVector minPt = box.Min;
    VxVector maxPt = box.Max;

    for (int i = 0; i < 3; i++) {
        if (plane.m_Normal[i] < 0.0f) {
            minPt[i] = box.Max[i];
            maxPt[i] = box.Min[i];
        }
    }

    float minDist = DotProduct(plane.m_Normal, minPt) + plane.m_D;
    float maxDist = DotProduct(plane.m_Normal, maxPt) + plane.m_D;
    return (minDist <= 0.0f && maxDist >= 0.0f) || (maxDist == 0.0f);
}

static XBOOL BoxPlaneMatrixScalarCore(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane) {
    VxVector halfSize = box.GetHalfSize();

    float radius = XAbs(DotProduct(plane.m_Normal, mat[0] * halfSize.x)) +
                   XAbs(DotProduct(plane.m_Normal, mat[1] * halfSize.y)) +
                   XAbs(DotProduct(plane.m_Normal, mat[2] * halfSize.z));

    VxVector center = box.GetCenter();
    VxVector transformedCenter;
    Vx3DMultiplyMatrixVector(&transformedCenter, mat, &center);

    float dist = DotProduct(plane.m_Normal, transformedCenter) + plane.m_D;
    return XAbs(dist) <= radius;
}

static XBOOL PlanesScalarCore(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p) {
    VxMatrix mat;
    mat[0][0] = plane1.m_Normal.x; mat[0][1] = plane1.m_Normal.y; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = plane2.m_Normal.y; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = plane3.m_Normal.y; mat[2][2] = plane3.m_Normal.z;

    float det = Vx3DMatrixDeterminant(mat);
    if (det == 0.0f)
        return FALSE;

    mat[0][0] = -plane1.m_D; mat[0][1] = plane1.m_Normal.y; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = -plane2.m_D; mat[1][1] = plane2.m_Normal.y; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = -plane3.m_D; mat[2][1] = plane3.m_Normal.y; mat[2][2] = plane3.m_Normal.z;
    float detX = Vx3DMatrixDeterminant(mat);

    mat[0][0] = plane1.m_Normal.x; mat[0][1] = -plane1.m_D; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = -plane2.m_D; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = -plane3.m_D; mat[2][2] = plane3.m_Normal.z;
    float detY = Vx3DMatrixDeterminant(mat);

    mat[0][0] = plane1.m_Normal.x; mat[0][1] = plane1.m_Normal.y; mat[0][2] = -plane1.m_D;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = plane2.m_Normal.y; mat[1][2] = -plane2.m_D;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = plane3.m_Normal.y; mat[2][2] = -plane3.m_D;
    float detZ = Vx3DMatrixDeterminant(mat);

    float invDet = 1.0f / det;
    p.x = detX * invDet;
    p.y = detY * invDet;
    p.z = detZ * invDet;
    return TRUE;
}

#if defined(VX_SIMD_SSE)
static XBOOL RayPlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps) {
        return FALSE;
    }

    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
}

static XBOOL RayPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (denom > -kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps) {
        return FALSE;
    }

    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
}

static XBOOL SegmentPlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps) {
        return FALSE;
    }
    if (dist > kSegmentMaxT) {
        return FALSE;
    }

    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
}

static XBOOL SegmentPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (denom > -kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    if (dist < -kPlaneParallelEps) {
        return FALSE;
    }
    if (dist > kSegmentMaxT) {
        return FALSE;
    }

    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
}

static XBOOL LinePlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
}

static XBOOL BoxPlaneSIMDCore(const VxBbox &box, const VxPlane &plane) {
    const __m128 n = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 minV = VxSIMDLoadFloat3(&box.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&box.Max.x);
    const __m128 signMask = _mm_cmplt_ps(n, _mm_setzero_ps());

    const __m128 minPt = _mm_or_ps(_mm_and_ps(signMask, maxV), _mm_andnot_ps(signMask, minV));
    const __m128 maxPt = _mm_or_ps(_mm_and_ps(signMask, minV), _mm_andnot_ps(signMask, maxV));

    const float minDist = VxSIMDExtractX(_mm_add_ss(VxSIMDDotProduct3(n, minPt), _mm_set_ss(plane.m_D)));
    const float maxDist = VxSIMDExtractX(_mm_add_ss(VxSIMDDotProduct3(n, maxPt), _mm_set_ss(plane.m_D)));
    return ((minDist <= 0.0f) && (maxDist >= 0.0f)) || (maxDist == 0.0f);
}

static XBOOL BoxPlaneMatrixSIMDCore(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane) {
    const VxVector halfSize = box.GetHalfSize();
    const __m128 n = VxSIMDLoadFloat3(&plane.m_Normal.x);

    const __m128 axis0 = _mm_mul_ps(VxSIMDLoadFloat3(&mat[0].x), _mm_set1_ps(halfSize.x));
    const __m128 axis1 = _mm_mul_ps(VxSIMDLoadFloat3(&mat[1].x), _mm_set1_ps(halfSize.y));
    const __m128 axis2 = _mm_mul_ps(VxSIMDLoadFloat3(&mat[2].x), _mm_set1_ps(halfSize.z));

    const float radius =
        VxSIMDExtractX(VxSIMDAbs3(VxSIMDDotProduct3(n, axis0))) +
        VxSIMDExtractX(VxSIMDAbs3(VxSIMDDotProduct3(n, axis1))) +
        VxSIMDExtractX(VxSIMDAbs3(VxSIMDDotProduct3(n, axis2)));

    const VxVector center = box.GetCenter();
    const __m128 transformedCenter = VxSIMDMatrixMultiplyVector3(&mat[0][0], VxSIMDLoadFloat3(&center.x));
    const float dist = VxSIMDExtractX(_mm_add_ss(VxSIMDDotProduct3(n, transformedCenter), _mm_set_ss(plane.m_D)));
    return XAbs(dist) <= radius;
}

static XBOOL PlanesSIMDCore(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p) {
    const __m128 n1 = VxSIMDLoadFloat3(&plane1.m_Normal.x);
    const __m128 n2 = VxSIMDLoadFloat3(&plane2.m_Normal.x);
    const __m128 n3 = VxSIMDLoadFloat3(&plane3.m_Normal.x);

    const __m128 c23 = VxSIMDCrossProduct3(n2, n3);
    const float det = VxSIMDExtractX(VxSIMDDotProduct3(n1, c23));
    if (det == 0.0f)
        return FALSE;

    const __m128 c31 = VxSIMDCrossProduct3(n3, n1);
    const __m128 c12 = VxSIMDCrossProduct3(n1, n2);

    __m128 sum = _mm_mul_ps(c23, _mm_set1_ps(-plane1.m_D));
    sum = _mm_add_ps(sum, _mm_mul_ps(c31, _mm_set1_ps(-plane2.m_D)));
    sum = _mm_add_ps(sum, _mm_mul_ps(c12, _mm_set1_ps(-plane3.m_D)));
    sum = _mm_mul_ps(sum, _mm_set1_ps(1.0f / det));

    VxSIMDStoreFloat3(&p.x, sum);
    return TRUE;
}
#endif

} // namespace

void VxIntersectPlaneDispatchRebuild(bool useSIMD) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectPlaneDispatch.rayPlane = useSIMD ? RayPlaneSIMDCore : RayPlaneScalarCore;
    g_VxIntersectPlaneDispatch.rayPlaneCulled = useSIMD ? RayPlaneCulledSIMDCore : RayPlaneCulledScalarCore;
    g_VxIntersectPlaneDispatch.segmentPlane = useSIMD ? SegmentPlaneSIMDCore : SegmentPlaneScalarCore;
    g_VxIntersectPlaneDispatch.segmentPlaneCulled = useSIMD ? SegmentPlaneCulledSIMDCore : SegmentPlaneCulledScalarCore;
    g_VxIntersectPlaneDispatch.linePlane = useSIMD ? LinePlaneSIMDCore : LinePlaneScalarCore;
    g_VxIntersectPlaneDispatch.boxPlane = useSIMD ? BoxPlaneSIMDCore : BoxPlaneScalarCore;
    g_VxIntersectPlaneDispatch.boxPlaneMatrix = useSIMD ? BoxPlaneMatrixSIMDCore : BoxPlaneMatrixScalarCore;
    g_VxIntersectPlaneDispatch.planes = useSIMD ? PlanesSIMDCore : PlanesScalarCore;
#else
    (void) useSIMD;
    g_VxIntersectPlaneDispatch.rayPlane = RayPlaneScalarCore;
    g_VxIntersectPlaneDispatch.rayPlaneCulled = RayPlaneCulledScalarCore;
    g_VxIntersectPlaneDispatch.segmentPlane = SegmentPlaneScalarCore;
    g_VxIntersectPlaneDispatch.segmentPlaneCulled = SegmentPlaneCulledScalarCore;
    g_VxIntersectPlaneDispatch.linePlane = LinePlaneScalarCore;
    g_VxIntersectPlaneDispatch.boxPlane = BoxPlaneScalarCore;
    g_VxIntersectPlaneDispatch.boxPlaneMatrix = BoxPlaneMatrixScalarCore;
    g_VxIntersectPlaneDispatch.planes = PlanesScalarCore;
#endif
}

//---------- Planes

XBOOL VxIntersect::RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return g_VxIntersectPlaneDispatch.rayPlane(ray, plane, point, dist);
}

XBOOL VxIntersect::RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return g_VxIntersectPlaneDispatch.rayPlaneCulled(ray, plane, point, dist);
}

XBOOL VxIntersect::SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return g_VxIntersectPlaneDispatch.segmentPlane(ray, plane, point, dist);
}

XBOOL VxIntersect::SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return g_VxIntersectPlaneDispatch.segmentPlaneCulled(ray, plane, point, dist);
}

XBOOL VxIntersect::LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return g_VxIntersectPlaneDispatch.linePlane(ray, plane, point, dist);
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxPlane &plane) {
    return g_VxIntersectPlaneDispatch.boxPlane(box, plane);
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane) {
    return g_VxIntersectPlaneDispatch.boxPlaneMatrix(box, mat, plane);
}

XBOOL VxIntersect::Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p) {
    return g_VxIntersectPlaneDispatch.planes(plane1, plane2, plane3, p);
}
