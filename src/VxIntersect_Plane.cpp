#include "VxIntersect.h"

#include <atomic>

#include "VxVector.h"
#include "VxRay.h"
#include "VxPlane.h"
#include "VxAtomic.h"
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

const VxIntersectPlaneDispatchTable kVxIntersectPlaneDispatchScalar = {
    RayPlaneScalarCore,
    RayPlaneCulledScalarCore,
    SegmentPlaneScalarCore,
    SegmentPlaneCulledScalarCore,
    LinePlaneScalarCore,
    BoxPlaneScalarCore,
    BoxPlaneMatrixScalarCore,
    PlanesScalarCore
};

#if defined(VX_SIMD_SSE)
const VxIntersectPlaneDispatchTable kVxIntersectPlaneDispatchSIMD = {
    RayPlaneSIMDCore,
    RayPlaneCulledSIMDCore,
    SegmentPlaneSIMDCore,
    SegmentPlaneCulledSIMDCore,
    LinePlaneSIMDCore,
    BoxPlaneSIMDCore,
    BoxPlaneMatrixSIMDCore,
    PlanesSIMDCore
};
#endif

std::atomic<const VxIntersectPlaneDispatchTable *> g_VxIntersectPlaneDispatch(&kVxIntersectPlaneDispatchScalar);

const VxIntersectPlaneDispatchTable *GetVxIntersectPlaneDispatchTable() {
    return g_VxIntersectPlaneDispatch.load(std::memory_order_acquire);
}

static XBOOL RayPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const double denom = double(plane.m_Normal.x) * ray.m_Direction.x +
        double(plane.m_Normal.y) * ray.m_Direction.y + double(plane.m_Normal.z) * ray.m_Direction.z;
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

    const double numerator = double(plane.m_Normal.x) * ray.m_Origin.x +
        double(plane.m_Normal.y) * ray.m_Origin.y + double(plane.m_Normal.z) * ray.m_Origin.z + plane.m_D;
    const double t = -numerator / denom;
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;

    point.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
              float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
              float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
    return TRUE;
}

static XBOOL RayPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const double denom = double(plane.m_Normal.x) * ray.m_Direction.x +
        double(plane.m_Normal.y) * ray.m_Direction.y + double(plane.m_Normal.z) * ray.m_Direction.z;
    if (denom > -kPlaneParallelEps)
        return FALSE;

    const double numerator = double(plane.m_Normal.x) * ray.m_Origin.x +
        double(plane.m_Normal.y) * ray.m_Origin.y + double(plane.m_Normal.z) * ray.m_Origin.z + plane.m_D;
    const double t = -numerator / denom;
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;

    point.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
              float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
              float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
    return TRUE;
}

static XBOOL SegmentPlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const double denom = double(plane.m_Normal.x) * ray.m_Direction.x +
        double(plane.m_Normal.y) * ray.m_Direction.y + double(plane.m_Normal.z) * ray.m_Direction.z;
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

    const double numerator = double(plane.m_Normal.x) * ray.m_Origin.x +
        double(plane.m_Normal.y) * ray.m_Origin.y + double(plane.m_Normal.z) * ray.m_Origin.z + plane.m_D;
    const double t = -numerator / denom;
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;
    if (t > kSegmentMaxT)
        return FALSE;

    point.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
              float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
              float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
    return TRUE;
}

static XBOOL SegmentPlaneCulledScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const double denom = double(plane.m_Normal.x) * ray.m_Direction.x +
        double(plane.m_Normal.y) * ray.m_Direction.y + double(plane.m_Normal.z) * ray.m_Direction.z;
    if (denom > -kPlaneParallelEps)
        return FALSE;

    const double numerator = double(plane.m_Normal.x) * ray.m_Origin.x +
        double(plane.m_Normal.y) * ray.m_Origin.y + double(plane.m_Normal.z) * ray.m_Origin.z + plane.m_D;
    const double t = -numerator / denom;
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;
    if (t > kSegmentMaxT)
        return FALSE;

    point.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
              float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
              float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
    return TRUE;
}

static XBOOL LinePlaneScalarCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    const double denom = double(plane.m_Normal.x) * ray.m_Direction.x +
        double(plane.m_Normal.y) * ray.m_Direction.y + double(plane.m_Normal.z) * ray.m_Direction.z;
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

    const double numerator = double(plane.m_Normal.x) * ray.m_Origin.x +
        double(plane.m_Normal.y) * ray.m_Origin.y + double(plane.m_Normal.z) * ray.m_Origin.z + plane.m_D;
    const double t = -numerator / denom;
    dist = float(t);

    point.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
              float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
              float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
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
#if defined(VX_SIMD_SSE2)
    const __m128 normal = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128d normalXY = _mm_cvtps_pd(normal);
    const __m128d normalZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d denominatorXY = _mm_mul_pd(normalXY, directionXY);
    const double denom = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(normalZ, directionZ)));
#else
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
#endif
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d numeratorXY = _mm_mul_pd(normalXY, originXY);
    const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(normalZ, originZ))) + plane.m_D;
    const double t = -numerator / denom;
#else
    const float t = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
#endif
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d parameter = _mm_set1_pd(t);
    const __m128d pointXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, parameter));
    const __m128d pointZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, parameter));
    VxSIMDStoreFloat3(&point.x, _mm_movelh_ps(_mm_cvtpd_ps(pointXY), _mm_cvtsd_ss(_mm_setzero_ps(), pointZ)));
#else
    point = VxSIMDPointOnRay(ray, t);
#endif
    return TRUE;
}

static XBOOL RayPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE2)
    const __m128 normal = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128d normalXY = _mm_cvtps_pd(normal);
    const __m128d normalZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d denominatorXY = _mm_mul_pd(normalXY, directionXY);
    const double denom = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(normalZ, directionZ)));
#else
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
#endif
    if (denom > -kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d numeratorXY = _mm_mul_pd(normalXY, originXY);
    const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(normalZ, originZ))) + plane.m_D;
    const double t = -numerator / denom;
#else
    const float t = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
#endif
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d parameter = _mm_set1_pd(t);
    const __m128d pointXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, parameter));
    const __m128d pointZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, parameter));
    VxSIMDStoreFloat3(&point.x, _mm_movelh_ps(_mm_cvtpd_ps(pointXY), _mm_cvtsd_ss(_mm_setzero_ps(), pointZ)));
#else
    point = VxSIMDPointOnRay(ray, t);
#endif
    return TRUE;
}

static XBOOL SegmentPlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE2)
    const __m128 normal = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128d normalXY = _mm_cvtps_pd(normal);
    const __m128d normalZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d denominatorXY = _mm_mul_pd(normalXY, directionXY);
    const double denom = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(normalZ, directionZ)));
#else
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
#endif
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d numeratorXY = _mm_mul_pd(normalXY, originXY);
    const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(normalZ, originZ))) + plane.m_D;
    const double t = -numerator / denom;
#else
    const float t = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
#endif
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;
    if (t > kSegmentMaxT)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d parameter = _mm_set1_pd(t);
    const __m128d pointXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, parameter));
    const __m128d pointZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, parameter));
    VxSIMDStoreFloat3(&point.x, _mm_movelh_ps(_mm_cvtpd_ps(pointXY), _mm_cvtsd_ss(_mm_setzero_ps(), pointZ)));
#else
    point = VxSIMDPointOnRay(ray, t);
#endif
    return TRUE;
}

static XBOOL SegmentPlaneCulledSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE2)
    const __m128 normal = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128d normalXY = _mm_cvtps_pd(normal);
    const __m128d normalZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d denominatorXY = _mm_mul_pd(normalXY, directionXY);
    const double denom = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(normalZ, directionZ)));
#else
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
#endif
    if (denom > -kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d numeratorXY = _mm_mul_pd(normalXY, originXY);
    const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(normalZ, originZ))) + plane.m_D;
    const double t = -numerator / denom;
#else
    const float t = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
#endif
    dist = float(t);
    if (t < -kPlaneParallelEps)
        return FALSE;
    if (t > kSegmentMaxT)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d parameter = _mm_set1_pd(t);
    const __m128d pointXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, parameter));
    const __m128d pointZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, parameter));
    VxSIMDStoreFloat3(&point.x, _mm_movelh_ps(_mm_cvtpd_ps(pointXY), _mm_cvtsd_ss(_mm_setzero_ps(), pointZ)));
#else
    point = VxSIMDPointOnRay(ray, t);
#endif
    return TRUE;
}

static XBOOL LinePlaneSIMDCore(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE2)
    const __m128 normal = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
    const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x);
    const __m128d normalXY = _mm_cvtps_pd(normal);
    const __m128d normalZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d denominatorXY = _mm_mul_pd(normalXY, directionXY);
    const double denom = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(normalZ, directionZ)));
#else
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
#endif
    if (fabs(denom) < kPlaneParallelEps)
        return FALSE;

#if defined(VX_SIMD_SSE2)
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d numeratorXY = _mm_mul_pd(normalXY, originXY);
    const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(normalZ, originZ))) + plane.m_D;
    const double t = -numerator / denom;
#else
    const float t = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
#endif
    dist = float(t);

#if defined(VX_SIMD_SSE2)
    const __m128d parameter = _mm_set1_pd(t);
    const __m128d pointXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, parameter));
    const __m128d pointZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, parameter));
    VxSIMDStoreFloat3(&point.x, _mm_movelh_ps(_mm_cvtpd_ps(pointXY), _mm_cvtsd_ss(_mm_setzero_ps(), pointZ)));
#else
    point = VxSIMDPointOnRay(ray, t);
#endif
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
    const VxIntersectPlaneDispatchTable *next = useSIMD ? &kVxIntersectPlaneDispatchSIMD : &kVxIntersectPlaneDispatchScalar;
#else
    (void) useSIMD;
    const VxIntersectPlaneDispatchTable *next = &kVxIntersectPlaneDispatchScalar;
#endif
    g_VxIntersectPlaneDispatch.store(next, std::memory_order_release);
}

//---------- Planes

XBOOL VxIntersect::RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return GetVxIntersectPlaneDispatchTable()->rayPlane(ray, plane, point, dist);
}

XBOOL VxIntersect::RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return GetVxIntersectPlaneDispatchTable()->rayPlaneCulled(ray, plane, point, dist);
}

XBOOL VxIntersect::SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return GetVxIntersectPlaneDispatchTable()->segmentPlane(ray, plane, point, dist);
}

XBOOL VxIntersect::SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return GetVxIntersectPlaneDispatchTable()->segmentPlaneCulled(ray, plane, point, dist);
}

XBOOL VxIntersect::LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
    return GetVxIntersectPlaneDispatchTable()->linePlane(ray, plane, point, dist);
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxPlane &plane) {
    return GetVxIntersectPlaneDispatchTable()->boxPlane(box, plane);
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane) {
    return GetVxIntersectPlaneDispatchTable()->boxPlaneMatrix(box, mat, plane);
}

XBOOL VxIntersect::Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p) {
    return GetVxIntersectPlaneDispatchTable()->planes(plane1, plane2, plane3, p);
}
