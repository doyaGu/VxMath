#include "VxIntersect.h"

#include <atomic>
#include <cmath>

#include "VxVector.h"
#include "VxRay.h"
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
    int first, second;
    SelectFaceProjectionAxes(norm, first, second);
    i1 = first;
    i2 = second;
    const VxVector *vertices[] = {&pt0, &pt1, &pt2};
    bool positive = false, negative = false;
    for (int edge = 0; edge < 3; ++edge) {
        const VxVector &a = *vertices[edge], &b = *vertices[(edge + 1) % 3];
        const double px = point[first], py = point[second], ax = a[first], ay = a[second], bx = b[first], by = b[second];
        // A product of two finite binary32 coordinates is exact in binary64.
        // Grow an error-free expansion of the six products, so an arbitrarily
        // small edge offset is not lost by subtracting coordinates first.
        const double terms[] = {px * by, -px * ay, -ax * by, -py * bx, py * ax, ay * bx};
        double expansion[6];
        for (int term = 0; term < 6; ++term) {
            double q = terms[term];
            for (int j = 0; j < term; ++j) {
                const double sum = q + expansion[j];
                const double virtualB = sum - q;
                expansion[j] = (q - (sum - virtualB)) + (expansion[j] - virtualB);
                q = sum;
            }
            expansion[term] = q;
        }
        for (int j = 5; j >= 0; --j) if (expansion[j] != 0.0) {
            positive = positive || expansion[j] > 0.0;
            negative = negative || expansion[j] < 0.0;
            break;
        }
        if (positive && negative) return FALSE;
    }
    if (positive) return 2;
    if (negative) return 1;
    // All edges are collinear with the query: retain the finite projected
    // segment/point hull instead of accepting its entire supporting line.
    return point[first] >= XMin(pt0[first], XMin(pt1[first], pt2[first])) &&
           point[first] <= XMax(pt0[first], XMax(pt1[first], pt2[first])) &&
           point[second] >= XMin(pt0[second], XMin(pt1[second], pt2[second])) &&
           point[second] <= XMax(pt0[second], XMax(pt1[second], pt2[second])) ? 2 : 0;
}

#if defined(VX_SIMD_SSE)
static XBOOL PointInFaceSIMDCore(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                 const VxVector &norm, int &i1, int &i2) {
    int first, second;
    SelectFaceProjectionAxes(norm, first, second);
    i1 = first;
    i2 = second;
    const VxVector *vertices[] = {&pt0, &pt1, &pt2, &pt0, &pt1};
    const __m128d px = _mm_set1_pd(point[first]), py = _mm_set1_pd(point[second]), zero = _mm_setzero_pd();
    int signs[4];
    // Two projected edge predicates in parallel. All products and error-free
    // expansion operations remain packed SSE2; no Scalar core is called.
    for (int pair = 0; pair < 3; pair += 2) {
        const __m128d ax = _mm_set_pd((*vertices[pair + 1])[first], (*vertices[pair])[first]);
        const __m128d ay = _mm_set_pd((*vertices[pair + 1])[second], (*vertices[pair])[second]);
        const __m128d bx = _mm_set_pd((*vertices[pair + 2])[first], (*vertices[pair + 1])[first]);
        const __m128d by = _mm_set_pd((*vertices[pair + 2])[second], (*vertices[pair + 1])[second]);
        const __m128d terms[] = {_mm_mul_pd(px, by), _mm_sub_pd(zero, _mm_mul_pd(px, ay)),
            _mm_sub_pd(zero, _mm_mul_pd(ax, by)), _mm_sub_pd(zero, _mm_mul_pd(py, bx)),
            _mm_mul_pd(py, ax), _mm_mul_pd(ay, bx)};
        __m128d expansion[6];
        for (int term = 0; term < 6; ++term) {
            __m128d q = terms[term];
            for (int j = 0; j < term; ++j) {
                const __m128d sum = _mm_add_pd(q, expansion[j]);
                const __m128d virtualB = _mm_sub_pd(sum, q);
                expansion[j] = _mm_add_pd(_mm_sub_pd(q, _mm_sub_pd(sum, virtualB)), _mm_sub_pd(expansion[j], virtualB));
                q = sum;
            }
            expansion[term] = q;
        }
        int classified = 0, negative = 0;
        for (int j = 5; j >= 0; --j) {
            const int n = _mm_movemask_pd(_mm_cmplt_pd(expansion[j], zero));
            const int p = _mm_movemask_pd(_mm_cmpgt_pd(expansion[j], zero));
            negative |= n & ~classified;
            classified |= n | p;
        }
        signs[pair] = classified & 1 ? (negative & 1 ? -1 : 1) : 0;
        signs[pair + 1] = classified & 2 ? (negative & 2 ? -1 : 1) : 0;
    }
    const bool positive = signs[0] > 0 || signs[1] > 0 || signs[2] > 0;
    const bool negative = signs[0] < 0 || signs[1] < 0 || signs[2] < 0;
    if (positive && negative) return FALSE;
    if (positive) return 2;
    if (negative) return 1;
    const __m128d a = _mm_set_pd(pt0[second], pt0[first]), b = _mm_set_pd(pt1[second], pt1[first]), c = _mm_set_pd(pt2[second], pt2[first]);
    const __m128d p = _mm_set_pd(point[second], point[first]);
    const __m128d outside = _mm_or_pd(_mm_cmplt_pd(p, _mm_min_pd(a, _mm_min_pd(b, c))), _mm_cmpgt_pd(p, _mm_max_pd(a, _mm_max_pd(b, c))));
    return _mm_movemask_pd(outside) ? 0 : 2;
}
#endif

} // namespace

//---------- Faces

// The face already supplies a point on its plane. Keep that anchor instead of
// rounding n.dot(pt0) into the float D of a temporary VxPlane. This also avoids
// overflowing D when the ray parameter and intersection are representable.
static XBOOL FacePlaneIntersectionCore(const VxRay &ray, const VxVector &pt0, const VxVector &norm,
                                      VxVector &res, float &dist, bool culled, double minimumT, double maximumT) {
#if defined(VX_SIMD_SSE)
    if (GetVxIntersectFaceDispatchTable()->pointInFace == PointInFaceSIMDCore) {
        const __m128 normal = VxSIMDLoadFloat3(&norm.x), direction = VxSIMDLoadFloat3(&ray.m_Direction.x);
        const __m128 origin = VxSIMDLoadFloat3(&ray.m_Origin.x), anchor = VxSIMDLoadFloat3(&pt0.x);
        const __m128d nXY = _mm_cvtps_pd(normal), nZ = _mm_cvtps_pd(_mm_movehl_ps(normal, normal));
        const __m128d dXY = _mm_cvtps_pd(direction), dZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
        const __m128d oXY = _mm_cvtps_pd(origin), oZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
        const __m128d denominatorXY = _mm_mul_pd(nXY, dXY);
        const double denominator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(denominatorXY, _mm_unpackhi_pd(denominatorXY, denominatorXY)), _mm_mul_sd(nZ, dZ)));
        if (culled ? denominator > -EPSILON : fabs(denominator) < EPSILON) return FALSE;
        const __m128d offsetXY = _mm_sub_pd(_mm_cvtps_pd(anchor), oXY);
        const __m128d offsetZ = _mm_sub_sd(_mm_cvtps_pd(_mm_movehl_ps(anchor, anchor)), oZ);
        const __m128d numeratorXY = _mm_mul_pd(nXY, offsetXY);
        const double numerator = _mm_cvtsd_f64(_mm_add_sd(_mm_add_sd(numeratorXY, _mm_unpackhi_pd(numeratorXY, numeratorXY)), _mm_mul_sd(nZ, offsetZ)));
        const double t = numerator / denominator;
        dist = float(t);
        if (t < minimumT || t > maximumT) return FALSE;
        const __m128d parameter = _mm_set1_pd(t);
        const __m128d pXY = _mm_add_pd(oXY, _mm_mul_pd(dXY, parameter));
        const __m128d pZ = _mm_add_sd(oZ, _mm_mul_sd(dZ, parameter));
        VxSIMDStoreFloat3(&res.x, _mm_movelh_ps(_mm_cvtpd_ps(pXY), _mm_cvtsd_ss(_mm_setzero_ps(), pZ)));
        return TRUE;
    }
#endif
    const double denominator = double(norm.x) * ray.m_Direction.x + double(norm.y) * ray.m_Direction.y + double(norm.z) * ray.m_Direction.z;
    if (culled ? denominator > -EPSILON : fabs(denominator) < EPSILON) return FALSE;
    const double numerator = double(norm.x) * (double(pt0.x) - ray.m_Origin.x) +
        double(norm.y) * (double(pt0.y) - ray.m_Origin.y) + double(norm.z) * (double(pt0.z) - ray.m_Origin.z);
    const double t = numerator / denominator;
    dist = float(t);
    if (t < minimumT || t > maximumT) return FALSE;
    res.Set(float(double(ray.m_Origin.x) + double(ray.m_Direction.x) * t),
            float(double(ray.m_Origin.y) + double(ray.m_Direction.y) * t),
            float(double(ray.m_Origin.z) + double(ray.m_Direction.z) * t));
    return TRUE;
}

static XBOOL FacePlaneIntersection(const VxRay &ray, const VxVector &pt0, const VxVector &norm,
                                  VxVector &res, float &dist, bool culled, bool segment) {
    return FacePlaneIntersectionCore(ray, pt0, norm, res, dist, culled, -EPSILON, segment ? 1.0f + EPSILON : HUGE_VAL);
}

static XBOOL RayFacePlaneIntersection(const VxRay &ray, const VxVector &pt0, const VxVector &norm,
                                     VxVector &res, float &dist, bool culled) {
    return FacePlaneIntersection(ray, pt0, norm, res, dist, culled, false);
}

// Is a point inside the boundary of a face
XBOOL VxIntersect::PointInFace(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, int &i1, int &i2) {
    return GetVxIntersectFaceDispatchTable()->pointInFace(point, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Ray - Face
XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                           const VxVector &norm, VxVector &res, float &dist) {
    if (!RayFacePlaneIntersection(ray, pt0, norm, res, dist, false)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded RayFace with dominant axes
XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                           const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    if (!RayFacePlaneIntersection(ray, pt0, norm, res, dist, false)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Ray - Face with culling (only from front)
XBOOL VxIntersect::RayFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                 const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    if (!RayFacePlaneIntersection(ray, pt0, norm, res, dist, true)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Segment - Face
XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, VxVector &res, float &dist) {
    if (!FacePlaneIntersection(ray, pt0, norm, res, dist, false, true)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded SegmentFace with dominant axes
XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                               const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    if (!FacePlaneIntersection(ray, pt0, norm, res, dist, false, true)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Segment - Face with culling
XBOOL VxIntersect::SegmentFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                     const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    if (!FacePlaneIntersection(ray, pt0, norm, res, dist, true, true)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Intersection Line - Face
XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                            const VxVector &norm, VxVector &res, float &dist) {
    if (!FacePlaneIntersectionCore(ray, pt0, norm, res, dist, false, -HUGE_VAL, HUGE_VAL)) {
        return FALSE;
    }

    // Check if intersection point is inside the triangle face
    int i1, i2;
    return PointInFace(res, pt0, pt1, pt2, norm, i1, i2);
}

// Overloaded LineFace with dominant axes
XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                            const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2) {
    if (!FacePlaneIntersectionCore(ray, pt0, norm, res, dist, false, -HUGE_VAL, HUGE_VAL)) {
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
    const double ax = pt0[i1], ay = pt0[i2], bx = pt1[i1], by = pt1[i2];
    const double cx = pt2[i1], cy = pt2[i2], px = pt[i1], py = pt[i2];
    const double abx = bx - ax, aby = by - ay, acx = cx - ax, acy = cy - ay;
    const double apx = px - ax, apy = py - ay;
    const double determinant = abx * acy - aby * acx;
    // Each weight is its opposite signed area. Avoid division by a particular
    // edge component, and do not recover a small w0 from rounded w1+w2.
    const double w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) / determinant;
    const double w1 = (apx * acy - apy * acx) / determinant;
    const double w2 = (abx * apy - aby * apx) / determinant;
    V2Coef = float(w2);
    V1Coef = float(w1);
    V0Coef = float(w0);
}

#if defined(VX_SIMD_SSE)
static void GetPointCoefficientsSIMDCore(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2,
                                         const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef) {
    const __m128d a = _mm_set_pd(pt0[i2], pt0[i1]), b = _mm_set_pd(pt1[i2], pt1[i1]);
    const __m128d c = _mm_set_pd(pt2[i2], pt2[i1]), p = _mm_set_pd(pt[i2], pt[i1]);
    const __m128d ab = _mm_sub_pd(b, a), ac = _mm_sub_pd(c, a), ap = _mm_sub_pd(p, a);
    const __m128d products = _mm_mul_pd(ab, _mm_shuffle_pd(ac, ac, 1));
    const __m128d determinant = _mm_sub_sd(products, _mm_unpackhi_pd(products, products));
    // Numerators for w1 and w2 occupy separate packed double lanes.
    const __m128d left = _mm_mul_pd(_mm_unpacklo_pd(ap, ab), _mm_unpackhi_pd(ac, ap));
    const __m128d right = _mm_mul_pd(_mm_unpackhi_pd(ap, ab), _mm_unpacklo_pd(ac, ap));
    const __m128d weights12 = _mm_div_pd(_mm_sub_pd(left, right), _mm_unpacklo_pd(determinant, determinant));
    const __m128d bp = _mm_sub_pd(b, p), cp = _mm_sub_pd(c, p);
    const __m128d products0 = _mm_mul_pd(bp, _mm_shuffle_pd(cp, cp, 1));
    const __m128d weight0 = _mm_div_sd(_mm_sub_sd(products0, _mm_unpackhi_pd(products0, products0)), determinant);
    // Finish every calculation before the original V2,V1,V0 write order,
    // keeping output aliases from becoming inputs to another coefficient.
    const float w0 = float(_mm_cvtsd_f64(weight0));
    const float w1 = float(_mm_cvtsd_f64(weights12));
    const float w2 = float(_mm_cvtsd_f64(_mm_unpackhi_pd(weights12, weights12)));
    V2Coef = w2;
    V1Coef = w1;
    V0Coef = w0;
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
                                   const double inputDistA[3], const double inputDistB[3], const double D[3]) {
    // Binary rescaling preserves the plane cut while keeping the original
    // no-division interval products within double's range for finite floats.
    double distA[3], distB[3];
    double scaleA = 0.0, scaleB = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (fabs(inputDistA[i]) > scaleA) scaleA = fabs(inputDistA[i]);
        if (fabs(inputDistB[i]) > scaleB) scaleB = fabs(inputDistB[i]);
    }
    if (scaleA == 0.0 || scaleB == 0.0)
        return coplanar_tri_tri(N0, A0, A1, A2, B0, B1, B2);
    int exponentA, exponentB;
    frexp(scaleA, &exponentA);
    frexp(scaleB, &exponentB);
    for (int i = 0; i < 3; ++i) {
        distA[i] = ldexp(inputDistA[i], -exponentA);
        distB[i] = ldexp(inputDistB[i], -exponentB);
    }
    const double distA01 = distA[0] * distA[1];
    const double distA02 = distA[0] * distA[2];
    if (distA01 > 0.0f && distA02 > 0.0f) return FALSE;

    const double absD[3] = {fabs(D[0]), fabs(D[1]), fabs(D[2])};

    int maxc = 0;
    double maxAbs = absD[0];
    if (absD[1] > maxAbs) {
        maxc = 1;
        maxAbs = absD[1];
    }
    if (absD[2] > maxAbs) {
        maxc = 2;
    }

    const double vA[3] = {0.0, double(A1[maxc]) - A0[maxc], double(A2[maxc]) - A0[maxc]};
    const double vB[3] = {double(B0[maxc]) - A0[maxc], double(B1[maxc]) - A0[maxc], double(B2[maxc]) - A0[maxc]};

    double aBase;
    double aNum0;
    double aNum1;
    double aDen0;
    double aDen1;
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

    const double distB01 = distB[0] * distB[1];
    const double distB02 = distB[0] * distB[2];
    double bBase;
    double bNum0;
    double bNum1;
    double bDen0;
    double bDen1;
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

    const double aDenProd = aDen0 * aDen1;
    const double bDenProd = bDen0 * bDen1;
    const double denProd = aDenProd * bDenProd;

    double isectA0 = bDenProd * (aDen1 * aNum0) + aBase * denProd;
    double isectA1 = bDenProd * (aDen0 * aNum1) + aBase * denProd;
    double isectB0 = aDenProd * (bDen1 * bNum0) + bBase * denProd;
    double isectB1 = aDenProd * (bDen0 * bNum1) + bBase * denProd;

    if (isectA0 > isectA1) {
        const double tmp = isectA0;
        isectA0 = isectA1;
        isectA1 = tmp;
    }
    if (isectB0 > isectB1) {
        const double tmp = isectB0;
        isectB0 = isectB1;
        isectB1 = tmp;
    }

    return (isectA1 >= isectB0 && isectB1 >= isectA0);
}

static XBOOL FaceFaceScalarCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                                const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1) {
    const VxVector *a[3] = {&A0, &A1, &A2};
    const VxVector *b[3] = {&B0, &B1, &B2};
    double distA[3], distB[3];
    for (int i = 0; i < 3; ++i) {
        distB[i] = (double(b[i]->x) - A0.x) * N0.x +
                   (double(b[i]->y) - A0.y) * N0.y +
                   (double(b[i]->z) - A0.z) * N0.z;
        distA[i] = (double(a[i]->x) - B0.x) * N1.x +
                   (double(a[i]->y) - B0.y) * N1.y +
                   (double(a[i]->z) - B0.z) * N1.z;
        if (fabs(distB[i]) < EPSILON) distB[i] = 0.0;
        if (fabs(distA[i]) < EPSILON) distA[i] = 0.0;
    }
    if (distB[0] * distB[1] > 0.0 && distB[0] * distB[2] > 0.0) return FALSE;
    if (distA[0] * distA[1] > 0.0 && distA[0] * distA[2] > 0.0) return FALSE;
    const double d[3] = {double(N0.y) * N1.z - double(N0.z) * N1.y,
                         double(N0.z) * N1.x - double(N0.x) * N1.z,
                         double(N0.x) * N1.y - double(N0.y) * N1.x};
    return FaceFaceFromDistances(A0, A1, A2, N0, B0, B1, B2, distA, distB, d);
}

#if defined(VX_SIMD_SSE)
static XBOOL FaceFaceSIMDCore(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0,
                              const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1) {
    const VxVector *a[3] = {&A0, &A1, &A2};
    const VxVector *b[3] = {&B0, &B1, &B2};
    const __m128d nx = _mm_setr_pd(N0.x, N1.x), ny = _mm_setr_pd(N0.y, N1.y), nz = _mm_setr_pd(N0.z, N1.z);
    const __m128d ox = _mm_setr_pd(A0.x, B0.x), oy = _mm_setr_pd(A0.y, B0.y), oz = _mm_setr_pd(A0.z, B0.z);
    double distA[3], distB[3];
    for (int i = 0; i < 3; ++i) {
        // Each lane evaluates a different triangle against the other plane.
        const __m128d dx = _mm_sub_pd(_mm_setr_pd(b[i]->x, a[i]->x), ox);
        const __m128d dy = _mm_sub_pd(_mm_setr_pd(b[i]->y, a[i]->y), oy);
        const __m128d dz = _mm_sub_pd(_mm_setr_pd(b[i]->z, a[i]->z), oz);
        const __m128d distances = _mm_add_pd(_mm_add_pd(_mm_mul_pd(dx, nx), _mm_mul_pd(dy, ny)), _mm_mul_pd(dz, nz));
        distB[i] = _mm_cvtsd_f64(distances);
        distA[i] = _mm_cvtsd_f64(_mm_unpackhi_pd(distances, distances));
        if (fabs(distB[i]) < EPSILON) distB[i] = 0.0;
        if (fabs(distA[i]) < EPSILON) distA[i] = 0.0;
    }
    if (distB[0] * distB[1] > 0.0 && distB[0] * distB[2] > 0.0) return FALSE;
    if (distA[0] * distA[1] > 0.0 && distA[0] * distA[2] > 0.0) return FALSE;
    const __m128d xy = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(N0.y, N0.z), _mm_setr_pd(N1.z, N1.x)),
                                  _mm_mul_pd(_mm_setr_pd(N0.z, N0.x), _mm_setr_pd(N1.y, N1.z)));
    double d[3];
    _mm_storeu_pd(d, xy);
    d[2] = double(N0.x) * N1.y - double(N0.y) * N1.x;
    return FaceFaceFromDistances(A0, A1, A2, N0, B0, B1, B2, distA, distB, d);
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
  double Ax,Ay,Bx,By,Cx,Cy,e,d,f;               \
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
  double a,b,c,d0,d1,d2;                     \
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

    const VxVector *vInput[3] = {&V0, &V1, &V2};
    const VxVector *uInput[3] = {&U0, &U1, &U2};
    double v[3][2], u[3][2];
    for (int i = 0; i < 3; ++i) {
        v[i][0] = double((*vInput[i])[i0]) - V0[i0];
        v[i][1] = double((*vInput[i])[i1]) - V0[i1];
        u[i][0] = double((*uInput[i])[i0]) - V0[i0];
        u[i][1] = double((*uInput[i])[i1]) - V0[i1];
    }
    i0 = 0;
    i1 = 1;
    EDGE_AGAINST_TRI_EDGES(v[0], v[1], u[0], u[1], u[2]);
    EDGE_AGAINST_TRI_EDGES(v[1], v[2], u[0], u[1], u[2]);
    EDGE_AGAINST_TRI_EDGES(v[2], v[0], u[0], u[1], u[2]);
    POINT_IN_TRI(v[0], u[0], u[1], u[2]);
    POINT_IN_TRI(u[0], v[0], v[1], v[2]);

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
