#include "VxIntersect.h"

#include "VxVector.h"
#include "VxRay.h"
#include "VxPlane.h"
#include "VxSIMD.h"

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

//---------- Planes

// Intersection Ray - Plane
XBOOL VxIntersect::RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE)
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
#else
    // Calculate the denominator: dot(ray.Dir, plane.Normal)
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);

    // Check if ray is parallel to plane (or nearly parallel)
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    // Calculate distance along ray
    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    // For a ray, intersection must be in positive direction (with epsilon tolerance)
    if (dist < -kPlaneParallelEps)
        return FALSE;

    // Calculate intersection point
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
#endif
}

// Intersection Ray - Plane with culling (only intersect from front)
XBOOL VxIntersect::RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE)
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
#else
    // Calculate the denominator: dot(ray.Dir, plane.Normal)
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);

    // Ensure ray is coming from the front of the plane (denom <= -epsilon)
    if (denom > -kPlaneParallelEps)
        return FALSE;

    // Calculate distance along ray
    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    // For a ray, intersection must be in positive direction (with epsilon tolerance)
    if (dist < -kPlaneParallelEps)
        return FALSE;

    // Calculate intersection point
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
#endif
}

// Intersection Segment - Plane
XBOOL VxIntersect::SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE)
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
#else
    // Calculate the denominator: dot(ray.Dir, plane.Normal)
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);

    // Check if segment is parallel to plane (or nearly parallel)
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    // Calculate distance along ray
    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    // For a segment, intersection must be between -epsilon and 1+epsilon
    if (dist < -kPlaneParallelEps)
        return FALSE;
    if (dist > kSegmentMaxT)
        return FALSE;

    // Calculate intersection point
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
#endif
}

// Intersection Segment - Plane with culling
XBOOL VxIntersect::SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE)
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
#else
    // Calculate the denominator: dot(ray.Dir, plane.Normal)
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);

    // Ensure segment is coming from the front of the plane (denom <= -epsilon)
    if (denom > -kPlaneParallelEps)
        return FALSE;

    // Calculate distance along ray
    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    // For a segment, intersection must be between -epsilon and 1+epsilon
    if (dist < -kPlaneParallelEps)
        return FALSE;
    if (dist > kSegmentMaxT)
        return FALSE;

    // Calculate intersection point
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
#endif
}

// Intersection Line - Plane
XBOOL VxIntersect::LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist) {
#if defined(VX_SIMD_SSE)
    const float denom = VxSIMDDot3Scalar(plane.m_Normal, ray.m_Direction);
    if (XAbs(denom) < kPlaneParallelEps) {
        return FALSE;
    }

    dist = -(VxSIMDDot3Scalar(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;
    point = VxSIMDPointOnRay(ray, dist);
    return TRUE;
#else
    // Calculate the denominator: dot(ray.Dir, plane.Normal)
    float denom = DotProduct(plane.m_Normal, ray.m_Direction);

    // Check if line is parallel to plane (or nearly parallel)
    if (XAbs(denom) < kPlaneParallelEps)
        return FALSE;

    // Calculate distance along ray
    dist = -(DotProduct(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    // Calculate intersection point
    point = ray.m_Origin + ray.m_Direction * dist;
    return TRUE;
#endif
}

// Intersection Box - Plane
XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxPlane &plane) {
#if defined(VX_SIMD_SSE)
    const __m128 n = VxSIMDLoadFloat3(&plane.m_Normal.x);
    const __m128 minV = VxSIMDLoadFloat3(&box.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&box.Max.x);
    const __m128 signMask = _mm_cmplt_ps(n, _mm_setzero_ps());

    const __m128 minPt = _mm_or_ps(_mm_and_ps(signMask, maxV), _mm_andnot_ps(signMask, minV));
    const __m128 maxPt = _mm_or_ps(_mm_and_ps(signMask, minV), _mm_andnot_ps(signMask, maxV));

    const float minDist = VxSIMDExtractX(_mm_add_ss(VxSIMDDotProduct3(n, minPt), _mm_set_ss(plane.m_D)));
    const float maxDist = VxSIMDExtractX(_mm_add_ss(VxSIMDDotProduct3(n, maxPt), _mm_set_ss(plane.m_D)));

    return ((minDist <= 0.0f) && (maxDist >= 0.0f)) || (maxDist == 0.0f);
#else
    VxVector minPt = box.Min;
    VxVector maxPt = box.Max;

    // For each axis, select min or max depending on normal direction
    for (int i = 0; i < 3; i++) {
        if (plane.m_Normal[i] < 0.0f) {
            minPt[i] = box.Max[i];
            maxPt[i] = box.Min[i];
        }
    }

    // Calculate signed distances from extreme points to plane
    float minDist = DotProduct(plane.m_Normal, minPt) + plane.m_D;
    float maxDist = DotProduct(plane.m_Normal, maxPt) + plane.m_D;

    // Box intersects plane if:
    // - minDist <= 0 AND maxDist >= 0 (spans the plane)
    // - OR maxDist == 0 (touches the plane)
    return (minDist <= 0.0f && maxDist >= 0.0f) || (maxDist == 0.0f);
#endif
}

// Intersection Box - Plane with transformation
XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane) {
#if defined(VX_SIMD_SSE)
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
#else
    VxVector halfSize = box.GetHalfSize();

    // Project the oriented box half-extents onto the plane normal.
    // abs(dot(n, row0*hx)) + abs(dot(n, row1*hy)) + abs(dot(n, row2*hz)))
    float radius = XAbs(DotProduct(plane.m_Normal, mat[0] * halfSize.x)) +
                   XAbs(DotProduct(plane.m_Normal, mat[1] * halfSize.y)) +
                   XAbs(DotProduct(plane.m_Normal, mat[2] * halfSize.z));

    VxVector center = box.GetCenter();
    VxVector transformedCenter;
    Vx3DMultiplyMatrixVector(&transformedCenter, mat, &center);

    float dist = DotProduct(plane.m_Normal, transformedCenter) + plane.m_D;
    return XAbs(dist) <= radius;
#endif
}

// Intersection Face - Plane
// XBOOL VxIntersect::FacePlane(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxPlane &plane) {
//     // Get signed distances from vertices to plane
//     float d0 = plane.Classify(A0);
//     float d1 = plane.Classify(A1);
//     float d2 = plane.Classify(A2);

//     // Face intersects plane if vertices are on different sides
//     // or at least one vertex is on the plane
//     if ((d0 * d1 <= 0.0f) || (d1 * d2 <= 0.0f) || (d2 * d0 <= 0.0f))
//         return TRUE;

//     return FALSE;
// }

// Intersection of 3 planes using Cramer's rule
XBOOL VxIntersect::Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p) {
#if defined(VX_SIMD_SSE)
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
#else
    // Set up the coefficient matrix
    VxMatrix mat;
    mat[0][0] = plane1.m_Normal.x; mat[0][1] = plane1.m_Normal.y; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = plane2.m_Normal.y; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = plane3.m_Normal.y; mat[2][2] = plane3.m_Normal.z;

    // Calculate determinant
    float det = Vx3DMatrixDeterminant(mat);
    if (det == 0.0f)
        return FALSE;

    // Calculate x-coordinate using Cramer's rule
    mat[0][0] = -plane1.m_D; mat[0][1] = plane1.m_Normal.y; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = -plane2.m_D; mat[1][1] = plane2.m_Normal.y; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = -plane3.m_D; mat[2][1] = plane3.m_Normal.y; mat[2][2] = plane3.m_Normal.z;
    float detX = Vx3DMatrixDeterminant(mat);

    // Calculate y-coordinate using Cramer's rule
    mat[0][0] = plane1.m_Normal.x; mat[0][1] = -plane1.m_D; mat[0][2] = plane1.m_Normal.z;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = -plane2.m_D; mat[1][2] = plane2.m_Normal.z;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = -plane3.m_D; mat[2][2] = plane3.m_Normal.z;
    float detY = Vx3DMatrixDeterminant(mat);

    // Calculate z-coordinate using Cramer's rule
    mat[0][0] = plane1.m_Normal.x; mat[0][1] = plane1.m_Normal.y; mat[0][2] = -plane1.m_D;
    mat[1][0] = plane2.m_Normal.x; mat[1][1] = plane2.m_Normal.y; mat[1][2] = -plane2.m_D;
    mat[2][0] = plane3.m_Normal.x; mat[2][1] = plane3.m_Normal.y; mat[2][2] = -plane3.m_D;
    float detZ = Vx3DMatrixDeterminant(mat);

    // Calculate final result
    float invDet = 1.0f / det;
    p.x = detX * invDet;
    p.y = detY * invDet;
    p.z = detZ * invDet;

    return TRUE;
#endif
}
