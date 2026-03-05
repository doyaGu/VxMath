#include "VxIntersect.h"

#include <atomic>

#include "VxVector.h"
#include "VxMatrix.h"
#include "VxPlane.h"
#include "VxAtomic.h"
#include "VxSIMD.h"
#include "VxIntersectDispatchStateInternal.h"

#if defined(VX_SIMD_SSE)
static inline __m128 VxSIMDLoadVector3(const VxVector &v) {
    return VxSIMDLoadFloat3(&v.x);
}

static inline float VxSIMDDot3Scalar(__m128 a, __m128 b) {
    return _mm_cvtss_f32(VxSIMDDotProduct3(a, b));
}

static inline void VxSIMDComputePlaneDistances(
    const VxVector &normal,
    float planeD,
    const VxVector &p0,
    const VxVector &p1,
    const VxVector &p2,
    float outDist[3]
) {
    const __m128 n = VxSIMDLoadVector3(normal);
    outDist[0] = VxSIMDDot3Scalar(n, VxSIMDLoadVector3(p0)) + planeD;
    outDist[1] = VxSIMDDot3Scalar(n, VxSIMDLoadVector3(p1)) + planeD;
    outDist[2] = VxSIMDDot3Scalar(n, VxSIMDLoadVector3(p2)) + planeD;
}

static inline XBOOL VxSIMDPointInFace3D(
    const VxVector &point,
    const VxVector &pt0,
    const VxVector &pt1,
    const VxVector &pt2,
    const VxVector &norm
) {
    const __m128 p = VxSIMDLoadVector3(point);
    const __m128 a0 = VxSIMDLoadVector3(pt0);
    const __m128 a1 = VxSIMDLoadVector3(pt1);
    const __m128 a2 = VxSIMDLoadVector3(pt2);
    const __m128 n = VxSIMDLoadVector3(norm);

    const float c0 = VxSIMDDot3Scalar(VxSIMDCrossProduct3(_mm_sub_ps(p, a1), _mm_sub_ps(a2, a1)), n);
    const float c1 = VxSIMDDot3Scalar(VxSIMDCrossProduct3(_mm_sub_ps(p, a2), _mm_sub_ps(a0, a2)), n);
    const float c2 = VxSIMDDot3Scalar(VxSIMDCrossProduct3(_mm_sub_ps(p, a0), _mm_sub_ps(a1, a0)), n);

    const bool nonNegative = (c0 >= 0.0f) && (c1 >= 0.0f) && (c2 >= 0.0f);
    const bool negative = (c0 < 0.0f) && (c1 < 0.0f) && (c2 < 0.0f);
    return (nonNegative || negative) ? TRUE : FALSE;
}

static inline __m128 VxSIMDLoadProjected2(const VxVector &v, int i1, int i2) {
    return _mm_setr_ps(v[i1], v[i2], 0.0f, 0.0f);
}

static inline float VxSIMDCross2Scalar(__m128 a, __m128 b) {
    const __m128 ay = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1));
    const __m128 by = _mm_shuffle_ps(b, b, _MM_SHUFFLE(1, 1, 1, 1));
    return _mm_cvtss_f32(_mm_sub_ss(_mm_mul_ss(a, by), _mm_mul_ss(ay, b)));
}
#endif

int coplanar_tri_tri(const VxVector &N, const VxVector &V0, const VxVector &V1, const VxVector &V2,
                     const VxVector &U0, const VxVector &U1, const VxVector &U2);

namespace {

typedef XBOOL (*VxIntersectPointInFaceDispatchFn)(const VxVector &, const VxVector &, const VxVector &, const VxVector &,
                                                  const VxVector &, int &, int &);
typedef void (*VxIntersectGetPointCoefficientsDispatchFn)(const VxVector &, const VxVector &, const VxVector &, const VxVector &,
                                                          const int &, const int &, float &, float &, float &);
typedef XBOOL (*VxIntersectFaceFaceDispatchFn)(const VxVector &, const VxVector &, const VxVector &, const VxVector &,
                                               const VxVector &, const VxVector &, const VxVector &, const VxVector &);

static XBOOL PointInFaceScalarCore(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                   const VxVector &norm, int &i1, int &i2);
static void GetPointCoefficientsScalarCore(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                           const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef);
static XBOOL FaceFaceScalarCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                                const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1);
#if defined(VX_SIMD_SSE)
static XBOOL PointInFaceSIMDCore(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                 const VxVector &norm, int &i1, int &i2);
static void GetPointCoefficientsSIMDCore(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                         const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef);
static XBOOL FaceFaceSIMDCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                              const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1);
#endif

struct VxIntersectFaceDispatchTable {
    VxIntersectPointInFaceDispatchFn pointInFace;
    VxIntersectGetPointCoefficientsDispatchFn getPointCoefficients;
    VxIntersectFaceFaceDispatchFn faceFace;
};

const VxIntersectFaceDispatchTable kVxIntersectFaceDispatchScalar = {
    PointInFaceScalarCore,
    GetPointCoefficientsScalarCore,
    FaceFaceScalarCore
};

#if defined(VX_SIMD_SSE)
const VxIntersectFaceDispatchTable kVxIntersectFaceDispatchSIMD = {
    PointInFaceSIMDCore,
    GetPointCoefficientsSIMDCore,
    FaceFaceSIMDCore
};
#endif

std::atomic<const VxIntersectFaceDispatchTable *> g_VxIntersectFaceDispatch(&kVxIntersectFaceDispatchScalar);

const VxIntersectFaceDispatchTable *GetVxIntersectFaceDispatchTable() {
    return g_VxIntersectFaceDispatch.load(std::memory_order_acquire);
}

static inline void SelectFaceProjectionAxes(const VxVector &norm, int &i1, int &i2) {
    float maxAbs = XAbs(norm.x);
    i1 = 1;
    i2 = 2;

    if (maxAbs < XAbs(norm.y)) {
        i1 = 0;
        i2 = 2;
        maxAbs = XAbs(norm.y);
    }

    if (maxAbs < XAbs(norm.z)) {
        i1 = 0;
        i2 = 1;
    }
}

static XBOOL PointInFaceScalarCore(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                   const VxVector &norm, int &i1, int &i2) {
    SelectFaceProjectionAxes(norm, i1, i2);

    char flags = 1;
    if ((point[i1] - pt1[i1]) * (pt2[i2] - pt1[i2]) - (pt2[i1] - pt1[i1]) * (point[i2] - pt1[i2]) >= 0.0f) {
        flags = 2;
    }

    if ((point[i1] - pt2[i1]) * (pt0[i2] - pt2[i2]) - (point[i2] - pt2[i2]) * (pt0[i1] - pt2[i1]) >= 0.0f) {
        flags &= 2;
    } else {
        flags &= 1;
    }

    if (!flags) return FALSE;

    if ((point[i1] - pt0[i1]) * (pt1[i2] - pt0[i2]) - (point[i2] - pt0[i2]) * (pt1[i1] - pt0[i1]) >= 0.0f) {
        return (flags & 2) != 0;
    }

    return (flags & 1) != 0;
}

#if defined(VX_SIMD_SSE)
static XBOOL PointInFaceSIMDCore(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                 const VxVector &norm, int &i1, int &i2) {
    SelectFaceProjectionAxes(norm, i1, i2);
    return VxSIMDPointInFace3D(point, pt0, pt1, pt2, norm);
}
#endif

} // namespace

//---------- Faces

// Is a point inside the boundary of a face
XBOOL VxIntersect::PointInFace(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, int &i1, int &i2) {
    return GetVxIntersectFaceDispatchTable()->pointInFace(point, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Ray - Face
XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                           const VxVector &norm, VxVector &res, float &dist) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test ray-plane intersection
    if (!RayPlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded RayFace with dominant axes
XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                           const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test ray-plane intersection
    if (!RayPlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Ray - Face with culling (only from front)
XBOOL VxIntersect::RayFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                 const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test ray-plane intersection with culling
    if (!RayPlaneCulled(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Segment - Face
XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, VxVector &res, float &dist) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test segment-plane intersection
    if (!SegmentPlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded SegmentFace with dominant axes
XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test segment-plane intersection
    if (!SegmentPlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Segment - Face with culling
XBOOL VxIntersect::SegmentFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                     const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test segment-plane intersection with culling
    if (!SegmentPlaneCulled(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Line - Face
XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                            const VxVector &norm, VxVector &res, float &dist) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test line-plane intersection
    if (!LinePlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded LineFace with dominant axes
XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                            const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    // Create plane from normal and point
    VxPlane plane;
    plane.Create(norm, pt0);

    // Test line-plane intersection
    if (!LinePlane(ray, plane, res, dist)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Calculate barycentric coordinates for point in face
void VxIntersect::GetPointCoefficients(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                       const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef) {
    GetVxIntersectFaceDispatchTable()->getPointCoefficients(pt, pt0, pt1, pt2, i1, i2, V0Coef, V1Coef, V2Coef);
}

namespace {

static void GetPointCoefficientsScalarCore(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                           const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef) {
    float p0_i1 = pt0[i1], p0_i2 = pt0[i2];
    float p1_i1 = pt1[i1], p1_i2 = pt1[i2];
    float p2_i1 = pt2[i1], p2_i2 = pt2[i2];
    float pt_i1 = pt[i1], pt_i2 = pt[i2];

    float v1_i1 = pt_i1 - p0_i1;
    float v1_i2 = pt_i2 - p0_i2;
    float v2_i1 = p1_i1 - p0_i1;
    float v2_i2 = p1_i2 - p0_i2;
    float v3_i1 = p2_i1 - p0_i1;
    float v3_i2 = p2_i2 - p0_i2;

    if (v2_i1 == 0.0f) {
        V2Coef = v1_i1 / v3_i1;
        V1Coef = (v1_i2 - V2Coef * v3_i2) / v2_i2;
    } else {
        float denom = v3_i2 * v2_i1 - v3_i1 * v2_i2;
        V2Coef = (v2_i1 * v1_i2 - v2_i2 * v1_i1) / denom;
        V1Coef = (v1_i1 - V2Coef * v3_i1) / v2_i1;
    }

    V0Coef = 1.0f - (V1Coef + V2Coef);
}

#if defined(VX_SIMD_SSE)
static void GetPointCoefficientsSIMDCore(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                         const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef) {
    const __m128 p0 = VxSIMDLoadProjected2(pt0, i1, i2);
    const __m128 p1 = VxSIMDLoadProjected2(pt1, i1, i2);
    const __m128 p2 = VxSIMDLoadProjected2(pt2, i1, i2);
    const __m128 p = VxSIMDLoadProjected2(pt, i1, i2);

    const __m128 v1 = _mm_sub_ps(p, p0);
    const __m128 v2 = _mm_sub_ps(p1, p0);
    const __m128 v3 = _mm_sub_ps(p2, p0);

    const float v1_i1 = _mm_cvtss_f32(v1);
    const float v1_i2 = _mm_cvtss_f32(_mm_shuffle_ps(v1, v1, _MM_SHUFFLE(1, 1, 1, 1)));
    const float v2_i1 = _mm_cvtss_f32(v2);
    const float v2_i2 = _mm_cvtss_f32(_mm_shuffle_ps(v2, v2, _MM_SHUFFLE(1, 1, 1, 1)));
    const float v3_i1 = _mm_cvtss_f32(v3);
    const float v3_i2 = _mm_cvtss_f32(_mm_shuffle_ps(v3, v3, _MM_SHUFFLE(1, 1, 1, 1)));

    if (v2_i1 == 0.0f) {
        V2Coef = v1_i1 / v3_i1;
        V1Coef = (v1_i2 - V2Coef * v3_i2) / v2_i2;
    } else {
        const float denom = VxSIMDCross2Scalar(v2, v3);
        V2Coef = VxSIMDCross2Scalar(v2, v1) / denom;
        V1Coef = (v1_i1 - V2Coef * v3_i1) / v2_i1;
    }

    V0Coef = 1.0f - (V1Coef + V2Coef);
}
#endif

} // namespace

// Intersection Face - Face
XBOOL VxIntersect::FaceFace(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                            const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1) {
    return GetVxIntersectFaceDispatchTable()->faceFace(A0, A1, A2, N0, B0, B1, B2, N1);
}

namespace {

static XBOOL FaceFaceFromDistances(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                                   const VxVector &B0, const VxVector &B1, const VxVector &B2,
                                   const float distA[3], const float distB[3], const VxVector &D) {
    const float distA01 = distA[0] * distA[1];
    const float distA02 = distA[0] * distA[2];
    if (distA01 > 0.0f && distA02 > 0.0f) return FALSE;

    const VxVector absD = Absolute(D);

    int maxc = 0;
    float maxAbs = absD[0];
    if (absD[1] >= maxAbs) {
        maxc = 1;
        maxAbs = absD[1];
    }
    if (absD[2] >= maxAbs) {
        maxc = 2;
    }

    const float vA[3] = {A0[maxc], A1[maxc], A2[maxc]};
    const float vB[3] = {B0[maxc], B1[maxc], B2[maxc]};

    float aBase;
    float aNum0;
    float aNum1;
    float aDen0;
    float aDen1;
    if (distA01 > 0.0f) {
        aBase = vA[2];
        aNum0 = (vA[0] - vA[2]) * distA[2];
        aNum1 = (vA[1] - vA[2]) * distA[2];
        aDen0 = distA[2] - distA[0];
        aDen1 = distA[2] - distA[1];
    } else if (distA02 > 0.0f) {
        aBase = vA[1];
        aNum0 = (vA[0] - vA[1]) * distA[1];
        aNum1 = (vA[2] - vA[1]) * distA[1];
        aDen0 = distA[1] - distA[0];
        aDen1 = distA[1] - distA[2];
    } else if (distA[1] * distA[2] > 0.0f || distA[0] != 0.0f) {
        aBase = vA[0];
        aNum0 = (vA[1] - vA[0]) * distA[0];
        aNum1 = (vA[2] - vA[0]) * distA[0];
        aDen0 = distA[0] - distA[1];
        aDen1 = distA[0] - distA[2];
    } else if (distA[1] != 0.0f) {
        aBase = vA[1];
        aNum0 = (vA[0] - vA[1]) * distA[1];
        aNum1 = (vA[2] - vA[1]) * distA[1];
        aDen0 = distA[1] - distA[0];
        aDen1 = distA[1] - distA[2];
    } else if (distA[2] == 0.0f) {
        return coplanar_tri_tri(N0, A0, A1, A2, B0, B1, B2);
    } else {
        aBase = vA[2];
        aNum0 = (vA[0] - vA[2]) * distA[2];
        aNum1 = (vA[1] - vA[2]) * distA[2];
        aDen0 = distA[2] - distA[0];
        aDen1 = distA[2] - distA[1];
    }

    const float distB01 = distB[0] * distB[1];
    const float distB02 = distB[0] * distB[2];
    float bBase;
    float bNum0;
    float bNum1;
    float bDen0;
    float bDen1;
    if (distB01 > 0.0f) {
        bBase = vB[2];
        bNum0 = (vB[0] - vB[2]) * distB[2];
        bNum1 = (vB[1] - vB[2]) * distB[2];
        bDen0 = distB[2] - distB[0];
        bDen1 = distB[2] - distB[1];
    } else if (distB02 > 0.0f) {
        bBase = vB[1];
        bNum0 = (vB[0] - vB[1]) * distB[1];
        bNum1 = (vB[2] - vB[1]) * distB[1];
        bDen0 = distB[1] - distB[0];
        bDen1 = distB[1] - distB[2];
    } else if (distB[1] * distB[2] > 0.0f || distB[0] != 0.0f) {
        bBase = vB[0];
        bNum0 = (vB[1] - vB[0]) * distB[0];
        bNum1 = (vB[2] - vB[0]) * distB[0];
        bDen0 = distB[0] - distB[1];
        bDen1 = distB[0] - distB[2];
    } else if (distB[1] != 0.0f) {
        bBase = vB[1];
        bNum0 = (vB[0] - vB[1]) * distB[1];
        bNum1 = (vB[2] - vB[1]) * distB[1];
        bDen0 = distB[1] - distB[0];
        bDen1 = distB[1] - distB[2];
    } else if (distB[2] != 0.0f) {
        bBase = vB[2];
        bNum0 = (vB[0] - vB[2]) * distB[2];
        bNum1 = (vB[1] - vB[2]) * distB[2];
        bDen0 = distB[2] - distB[0];
        bDen1 = distB[2] - distB[1];
    } else {
        return coplanar_tri_tri(N0, A0, A1, A2, B0, B1, B2);
    }

    const float aDenProd = aDen0 * aDen1;
    const float bDenProd = bDen0 * bDen1;
    const float denProd = aDenProd * bDenProd;

    float isectA0 = bDenProd * (aDen1 * aNum0) + aBase * denProd;
    float isectA1 = bDenProd * (aDen0 * aNum1) + aBase * denProd;
    float isectB0 = aDenProd * (bDen1 * bNum0) + bBase * denProd;
    float isectB1 = aDenProd * (bDen0 * bNum1) + bBase * denProd;

    if (isectA0 > isectA1) {
        const float tmp = isectA0;
        isectA0 = isectA1;
        isectA1 = tmp;
    }
    if (isectB0 > isectB1) {
        const float tmp = isectB0;
        isectB0 = isectB1;
        isectB1 = tmp;
    }

    return (isectA1 >= isectB0 && isectB1 >= isectA0);
}

static XBOOL FaceFaceScalarCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                                const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1) {
    const float planeA_d = -DotProduct(N0, A0);
    float distB[3] = {
        DotProduct(N0, B0) + planeA_d,
        DotProduct(N0, B1) + planeA_d,
        DotProduct(N0, B2) + planeA_d,
    };
    if (XAbs(distB[0]) < EPSILON) distB[0] = 0.0f;
    if (XAbs(distB[1]) < EPSILON) distB[1] = 0.0f;
    if (XAbs(distB[2]) < EPSILON) distB[2] = 0.0f;
    if (distB[0] * distB[1] > 0.0f && distB[0] * distB[2] > 0.0f) return FALSE;

    const float planeB_d = -DotProduct(N1, B0);
    float distA[3] = {
        DotProduct(N1, A0) + planeB_d,
        DotProduct(N1, A1) + planeB_d,
        DotProduct(N1, A2) + planeB_d,
    };
    if (XAbs(distA[0]) < EPSILON) distA[0] = 0.0f;
    if (XAbs(distA[1]) < EPSILON) distA[1] = 0.0f;
    if (XAbs(distA[2]) < EPSILON) distA[2] = 0.0f;

    const VxVector D = CrossProduct(N0, N1);
    return FaceFaceFromDistances(A0, A1, A2, N0, B0, B1, B2, distA, distB, D);
}

#if defined(VX_SIMD_SSE)
static XBOOL FaceFaceSIMDCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                              const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1) {
    const __m128 n0V = VxSIMDLoadVector3(N0);
    const __m128 n1V = VxSIMDLoadVector3(N1);

    float distB[3];
    const float planeA_d = -VxSIMDDot3Scalar(n0V, VxSIMDLoadVector3(A0));
    VxSIMDComputePlaneDistances(N0, planeA_d, B0, B1, B2, distB);
    if (XAbs(distB[0]) < EPSILON) distB[0] = 0.0f;
    if (XAbs(distB[1]) < EPSILON) distB[1] = 0.0f;
    if (XAbs(distB[2]) < EPSILON) distB[2] = 0.0f;
    if (distB[0] * distB[1] > 0.0f && distB[0] * distB[2] > 0.0f) return FALSE;

    float distA[3];
    const float planeB_d = -VxSIMDDot3Scalar(n1V, VxSIMDLoadVector3(B0));
    VxSIMDComputePlaneDistances(N1, planeB_d, A0, A1, A2, distA);
    if (XAbs(distA[0]) < EPSILON) distA[0] = 0.0f;
    if (XAbs(distA[1]) < EPSILON) distA[1] = 0.0f;
    if (XAbs(distA[2]) < EPSILON) distA[2] = 0.0f;

    VxVector D = VxVector::axis0();
    VxSIMDStoreFloat3(&D.x, VxSIMDCrossProduct3(n0V, n1V));
    return FaceFaceFromDistances(A0, A1, A2, N0, B0, B1, B2, distA, distB, D);
}
#endif

} // namespace

/* this edge to edge test is based on Franlin Antonio's gem:
   "Faster Line Segment Intersection", in Graphics Gems III,
   pp. 199-202 */
#define EDGE_EDGE_TEST(V0,U0,U1)                      \
  Bx=U0[i0]-U1[i0];                                   \
  By=U0[i1]-U1[i1];                                   \
  Cx=V0[i0]-U0[i0];                                   \
  Cy=V0[i1]-U0[i1];                                   \
  f=Ay*Bx-Ax*By;                                      \
  d=By*Cx-Bx*Cy;                                      \
  if((f>0 && d>=0 && d<=f) || (f<0 && d<=0 && d>=f))  \
  {                                                   \
    e=Ax*Cy-Ay*Cx;                                    \
    if(f>0)                                           \
    {                                                 \
      if(e>=0 && e<=f) return 1;                      \
    }                                                 \
    else                                              \
    {                                                 \
      if(e<=0 && e>=f) return 1;                      \
    }                                                 \
  }

#define EDGE_AGAINST_TRI_EDGES(V0,V1,U0,U1,U2) \
{                                              \
  float Ax,Ay,Bx,By,Cx,Cy,e,d,f;               \
  Ax=V1[i0]-V0[i0];                            \
  Ay=V1[i1]-V0[i1];                            \
  /* test edge U0,U1 against V0,V1 */          \
  EDGE_EDGE_TEST(V0,U0,U1);                    \
  /* test edge U1,U2 against V0,V1 */          \
  EDGE_EDGE_TEST(V0,U1,U2);                    \
  /* test edge U2,U1 against V0,V1 */          \
  EDGE_EDGE_TEST(V0,U2,U0);                    \
}

#define POINT_IN_TRI(V0,U0,U1,U2)           \
{                                           \
  float a,b,c,d0,d1,d2;                     \
  /* is T1 completly inside T2? */          \
  /* check if V0 is inside tri(U0,U1,U2) */ \
  a=U1[i1]-U0[i1];                          \
  b=-(U1[i0]-U0[i0]);                       \
  c=-a*U0[i0]-b*U0[i1];                     \
  d0=a*V0[i0]+b*V0[i1]+c;                   \
                                            \
  a=U2[i1]-U1[i1];                          \
  b=-(U2[i0]-U1[i0]);                       \
  c=-a*U1[i0]-b*U1[i1];                     \
  d1=a*V0[i0]+b*V0[i1]+c;                   \
                                            \
  a=U0[i1]-U2[i1];                          \
  b=-(U0[i0]-U2[i0]);                       \
  c=-a*U2[i0]-b*U2[i1];                     \
  d2=a*V0[i0]+b*V0[i1]+c;                   \
  if(d0*d1>0.0)                             \
  {                                         \
    if(d0*d2>0.0) return 1;                 \
  }                                         \
}

int coplanar_tri_tri(const VxVector &N, const VxVector &V0, const VxVector &V1, const VxVector &V2,
                     const VxVector &U0, const VxVector &U1, const VxVector &U2) {
    float A[3];
    short i0, i1;
    /* first project onto an axis-aligned plane, that maximizes the area */
    /* of the triangles, compute indices: i0,i1. */
    A[0] = fabsf(N[0]);
    A[1] = fabsf(N[1]);
    A[2] = fabsf(N[2]);
    if (A[0] > A[1]) {
        if (A[0] > A[2]) {
            i0 = 1; /* A[0] is greatest */
            i1 = 2;
        } else {
            i0 = 0; /* A[2] is greatest */
            i1 = 1;
        }
    } else /* A[0]<=A[1] */
    {
        if (A[2] > A[1]) {
            i0 = 0; /* A[2] is greatest */
            i1 = 1;
        } else {
            i0 = 0; /* A[1] is greatest */
            i1 = 2;
        }
    }

    /* test all edges of triangle 1 against the edges of triangle 2 */
    EDGE_AGAINST_TRI_EDGES(V0, V1, U0, U1, U2);
    EDGE_AGAINST_TRI_EDGES(V1, V2, U0, U1, U2);
    EDGE_AGAINST_TRI_EDGES(V2, V0, U0, U1, U2);

    /* finally, test if tri1 is totally contained in tri2 or vice versa */
    POINT_IN_TRI(V0, U0, U1, U2);
    POINT_IN_TRI(U0, V0, V1, V2);

    return 0;
}

void VxIntersectFaceDispatchRebuild(bool useSIMD) {
#if defined(VX_SIMD_SSE)
    const VxIntersectFaceDispatchTable *next = useSIMD ? &kVxIntersectFaceDispatchSIMD : &kVxIntersectFaceDispatchScalar;
#else
    (void) useSIMD;
    const VxIntersectFaceDispatchTable *next = &kVxIntersectFaceDispatchScalar;
#endif
    g_VxIntersectFaceDispatch.store(next, std::memory_order_release);
}
