#include "VxIntersect.h"

#include <atomic>

#include "VxVector.h"
#include "VxRay.h"
#include "VxSphere.h"
#include "VxAtomic.h"
#include "VxSIMD.h"
#include "VxIntersectDispatchStateInternal.h"

#if defined(VX_SIMD_SSE)
static inline float VxSIMDExtractX(__m128 v) {
    return _mm_cvtss_f32(v);
}

static inline float VxSIMDDot3Scalar(const VxVector& a, const VxVector& b) {
    const __m128 av = VxSIMDLoadFloat3(&a.x);
    const __m128 bv = VxSIMDLoadFloat3(&b.x);
    return VxSIMDExtractX(VxSIMDDotProduct3(av, bv));
}

static inline VxVector VxSIMDSubtractVector3(const VxVector& a, const VxVector& b) {
    VxVector out;
    const __m128 av = VxSIMDLoadFloat3(&a.x);
    const __m128 bv = VxSIMDLoadFloat3(&b.x);
    VxSIMDStoreFloat3(&out.x, VxSIMDSubtract3(av, bv));
    return out;
}

static inline VxVector VxSIMDAddScaledVector3(const VxVector& a, const VxVector& dir, float t) {
    VxVector out;
    const __m128 av = VxSIMDLoadFloat3(&a.x);
    const __m128 dv = VxSIMDLoadFloat3(&dir.x);
    const __m128 tv = _mm_set1_ps(t);
    VxSIMDStoreFloat3(&out.x, _mm_add_ps(av, _mm_mul_ps(dv, tv)));
    return out;
}
#endif

/**
 * Solves quadratic equation ax^2 + bx + c = 0
 * @param a - coefficient of x^2
 * @param b - coefficient of x  
 * @param c - constant term
 * @param t1 - receives the plus-square-root solution (not necessarily smaller)
 * @param t2 - receives the minus-square-root solution
 * @return 1 if solutions found, 0 if no real solutions
 * A negative discriminant leaves both outputs unchanged. A constant equation
 * has no selected root and writes zero to both outputs.
 */
XBOOL QuadraticFormula(float a, float b, float c, float *t1, float *t2) {
    // Treat a == 0 OR unordered(a, 0) (i.e., NaN) as a linear equation.
    if (!(a > 0.0f || a < 0.0f)) {
        if (b == 0.0f) {
            *t1 = 0.0f;
            *t2 = 0.0f;
            return 0;
        }

        const float root = -(c / b);
        *t1 = root;
        *t2 = root;
        return 1;
    }

    const double disc = (double)b * (double)b - 4.0 * (double)a * (double)c;

    // Two distinct real solutions only when disc > 0.
    if (disc > 0.0) {
        const double sqrt_disc = sqrt(disc);
        // Compute the root without subtracting nearly equal numbers, then use
        // the product of the roots for its companion. Keep the original slots
        // even when a is negative: they are plus/minus roots, not sorted roots.
        const double q = -0.5 * (b >= 0.0f ? (double)b + sqrt_disc : (double)b - sqrt_disc);
        if (b >= 0.0f) {
            *t1 = (float)((double)c / q);
            *t2 = (float)(q / (double)a);
        } else {
            *t1 = (float)(q / (double)a);
            *t2 = (float)((double)c / q);
        }
        return 1;
    }

    // One repeated root only when disc == 0.
    if (disc == 0.0) {
        const float root = (float)(-(double)b / ((double)a + (double)a));
        *t1 = root;
        *t2 = root;
        return 1;
    }

    return 0;
}

namespace {

typedef XBOOL (*VxIntersectSphereSphereDispatchFn)(const VxSphere &, const VxVector &, const VxSphere &, const VxVector &, float *, float *);
typedef int (*VxIntersectRaySphereDispatchFn)(const VxRay &, const VxSphere &, VxVector *, VxVector *);
typedef XBOOL (*VxIntersectSphereAABBDispatchFn)(const VxSphere &, const VxBbox &);

static XBOOL SphereSphereScalarCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2, float *oCollisionTime1, float *oCollisionTime2);
static int RaySphereScalarCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2);
static XBOOL SphereAABBScalarCore(const VxSphere &iSphere, const VxBbox &iBox);

#if defined(VX_SIMD_SSE)
static XBOOL SphereSphereSIMDCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2, float *oCollisionTime1, float *oCollisionTime2);
static int RaySphereSIMDCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2);
static XBOOL SphereAABBMSIMDCore(const VxSphere &iSphere, const VxBbox &iBox);
#endif

struct VxIntersectSphereDispatchTable {
    VxIntersectSphereSphereDispatchFn sphereSphere;
    VxIntersectRaySphereDispatchFn raySphere;
    VxIntersectSphereAABBDispatchFn sphereAABB;
};

const VxIntersectSphereDispatchTable kVxIntersectSphereDispatchScalar = {
    SphereSphereScalarCore,
    RaySphereScalarCore,
    SphereAABBScalarCore
};

#if defined(VX_SIMD_SSE)
const VxIntersectSphereDispatchTable kVxIntersectSphereDispatchSIMD = {
    SphereSphereSIMDCore,
    RaySphereSIMDCore,
    SphereAABBMSIMDCore
};
#endif

std::atomic<const VxIntersectSphereDispatchTable *> g_VxIntersectSphereDispatch(&kVxIntersectSphereDispatchScalar);

const VxIntersectSphereDispatchTable *GetVxIntersectSphereDispatchTable() {
    return g_VxIntersectSphereDispatch.load(std::memory_order_acquire);
}

static XBOOL SphereSphereScalarCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                    float *oCollisionTime1, float *oCollisionTime2) {
    double delta[3], speed[3], squaredDistance = 0, a = 0, projection = 0;
    for (int i = 0; i < 3; ++i) {
        delta[i] = double(iS2.Center()[i]) - iS1.Center()[i];
        speed[i] = (double(iP2[i]) - iS2.Center()[i]) - (double(iP1[i]) - iS1.Center()[i]);
        squaredDistance += delta[i] * delta[i];
        a += speed[i] * speed[i];
        projection += delta[i] * speed[i];
    }
    const double radius = double(iS1.Radius()) + iS2.Radius();
    const double radiusSquared = radius * radius;
    if (squaredDistance <= radiusSquared || a == 0) {
        if (oCollisionTime1) *oCollisionTime1 = 0;
        if (oCollisionTime2) *oCollisionTime2 = 0;
        return squaredDistance <= radiusSquared;
    }
    const double crossX = delta[1] * speed[2] - delta[2] * speed[1];
    const double crossY = delta[2] * speed[0] - delta[0] * speed[2];
    const double crossZ = delta[0] * speed[1] - delta[1] * speed[0];
    const double discriminant = radiusSquared * a - (crossX * crossX + crossY * crossY + crossZ * crossZ);
    if (discriminant < 0) return FALSE;
    const double centerT = -projection / a;
    const double offsetT = sqrt(discriminant) / a;
    const double enter = centerT - offsetT, exit = centerT + offsetT;
    if (oCollisionTime1) *oCollisionTime1 = float(enter);
    if (oCollisionTime2) *oCollisionTime2 = float(exit);
    return enter >= 0 && enter <= 1;
}

static int RaySphereScalarCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
    double delta[3], direction[3], a = 0, projection = 0;
    for (int i = 0; i < 3; ++i) {
        delta[i] = double(iRay.m_Origin[i]) - iSphere.Center()[i];
        direction[i] = iRay.m_Direction[i];
        a += direction[i] * direction[i];
        projection += delta[i] * direction[i];
    }
    if (a == 0) return 0;
    const double crossX = delta[1] * direction[2] - delta[2] * direction[1];
    const double crossY = delta[2] * direction[0] - delta[0] * direction[2];
    const double crossZ = delta[0] * direction[1] - delta[1] * direction[0];
    const double discriminant = double(iSphere.Radius()) * iSphere.Radius() * a -
        (crossX * crossX + crossY * crossY + crossZ * crossZ);
    if (discriminant < 0) return 0;

    const double closestT = -projection / a;
    const double offsetT = sqrt(discriminant) / a;
    VxVector first, second;
    for (int i = 0; i < 3; ++i) {
        // Keep the radius offset separate from the large origin/projection.
        const double closest = double(iRay.m_Origin[i]) + direction[i] * closestT;
        const double offset = direction[i] * offsetT;
        first[i] = float(closest - offset);
        second[i] = float(closest + offset);
    }
    if (discriminant == 0) {
        if (oInter1) *oInter1 = first;
        else if (oInter2) *oInter2 = first;
        return 1;
    }
    if (oInter1) *oInter1 = first;
    if (oInter2) *oInter2 = second;
    return 2;
}

static XBOOL SphereAABBScalarCore(const VxSphere &iSphere, const VxBbox &iBox) {
    double squaredDistance = 0;
    for (int i = 0; i < 3; ++i) {
        double delta = 0;
        if (iSphere.Center()[i] < iBox.Min[i]) delta = double(iBox.Min[i]) - iSphere.Center()[i];
        else if (iSphere.Center()[i] > iBox.Max[i]) delta = double(iSphere.Center()[i]) - iBox.Max[i];
        squaredDistance += delta * delta;
    }
    return squaredDistance <= double(iSphere.Radius()) * iSphere.Radius();
}

#if defined(VX_SIMD_SSE)
static XBOOL SphereSphereSIMDCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                  float *oCollisionTime1, float *oCollisionTime2) {
#if defined(VX_SIMD_SSE2)
    const __m128 c1 = VxSIMDLoadFloat3(&iS1.Center().x), c2 = VxSIMDLoadFloat3(&iS2.Center().x);
    const __m128 p1 = VxSIMDLoadFloat3(&iP1.x), p2 = VxSIMDLoadFloat3(&iP2.x);
    const __m128d deltaXY = _mm_sub_pd(_mm_cvtps_pd(c2), _mm_cvtps_pd(c1));
    const __m128d deltaZ = _mm_sub_sd(_mm_cvtps_pd(_mm_movehl_ps(c2, c2)), _mm_cvtps_pd(_mm_movehl_ps(c1, c1)));
    const __m128d speedXY = _mm_sub_pd(_mm_sub_pd(_mm_cvtps_pd(p2), _mm_cvtps_pd(c2)), _mm_sub_pd(_mm_cvtps_pd(p1), _mm_cvtps_pd(c1)));
    const __m128d speedZ = _mm_sub_sd(_mm_sub_sd(_mm_cvtps_pd(_mm_movehl_ps(p2, p2)), _mm_cvtps_pd(_mm_movehl_ps(c2, c2))),
                                    _mm_sub_sd(_mm_cvtps_pd(_mm_movehl_ps(p1, p1)), _mm_cvtps_pd(_mm_movehl_ps(c1, c1))));
    const __m128d distanceXY = _mm_mul_pd(deltaXY, deltaXY), squareXY = _mm_mul_pd(speedXY, speedXY);
    const __m128d distanceSquared = _mm_add_sd(_mm_add_sd(distanceXY, _mm_unpackhi_pd(distanceXY, distanceXY)), _mm_mul_sd(deltaZ, deltaZ));
    const __m128d a = _mm_add_sd(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY)), _mm_mul_sd(speedZ, speedZ));
    const __m128d radius = _mm_set_sd(double(iS1.Radius()) + iS2.Radius());
    const __m128d radiusSquared = _mm_mul_sd(radius, radius);
    const bool overlapping = _mm_cvtsd_f64(distanceSquared) <= _mm_cvtsd_f64(radiusSquared);
    if (overlapping || _mm_cvtsd_f64(a) == 0) {
        if (oCollisionTime1) *oCollisionTime1 = 0;
        if (oCollisionTime2) *oCollisionTime2 = 0;
        return overlapping;
    }
    double d[3], v[3];
    _mm_storeu_pd(d, deltaXY); d[2] = _mm_cvtsd_f64(deltaZ);
    _mm_storeu_pd(v, speedXY); v[2] = _mm_cvtsd_f64(speedZ);
    const __m128d crossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(d[1], d[2]), _mm_setr_pd(v[2], v[0])),
                                     _mm_mul_pd(_mm_setr_pd(d[2], d[0]), _mm_setr_pd(v[1], v[2])));
    const __m128d crossZ = _mm_sub_sd(_mm_mul_sd(_mm_set_sd(d[0]), _mm_set_sd(v[1])), _mm_mul_sd(_mm_set_sd(d[1]), _mm_set_sd(v[0])));
    const __m128d crossSquareXY = _mm_mul_pd(crossXY, crossXY);
    const __m128d crossSquared = _mm_add_sd(_mm_add_sd(crossSquareXY, _mm_unpackhi_pd(crossSquareXY, crossSquareXY)), _mm_mul_sd(crossZ, crossZ));
    const __m128d discriminant = _mm_sub_sd(_mm_mul_sd(radiusSquared, a), crossSquared);
    if (_mm_cvtsd_f64(discriminant) < 0) return FALSE;
    const __m128d projectedXY = _mm_mul_pd(deltaXY, speedXY);
    const __m128d projection = _mm_add_sd(_mm_add_sd(projectedXY, _mm_unpackhi_pd(projectedXY, projectedXY)), _mm_mul_sd(deltaZ, speedZ));
    const __m128d centerT = _mm_div_sd(_mm_sub_sd(_mm_setzero_pd(), projection), a);
    const __m128d offsetT = _mm_div_sd(_mm_sqrt_sd(discriminant, discriminant), a);
    const double enter = _mm_cvtsd_f64(_mm_sub_sd(centerT, offsetT)), exit = _mm_cvtsd_f64(_mm_add_sd(centerT, offsetT));
    if (oCollisionTime1) *oCollisionTime1 = float(enter);
    if (oCollisionTime2) *oCollisionTime2 = float(exit);
    return enter >= 0 && enter <= 1;
#else
    float fallbackCollisionTime1 = 0.0f;
    float fallbackCollisionTime2 = 0.0f;
    float *collisionTime1 = (oCollisionTime1 != nullptr) ? oCollisionTime1 : &fallbackCollisionTime1;
    float *collisionTime2 = (oCollisionTime2 != nullptr) ? oCollisionTime2 : &fallbackCollisionTime2;

    const VxVector movement1 = VxSIMDSubtractVector3(iP1, iS1.Center());
    const VxVector movement2 = VxSIMDSubtractVector3(iP2, iS2.Center());
    const VxVector centerDiff = VxSIMDSubtractVector3(iS2.Center(), iS1.Center());
    const VxVector relativeMovement = VxSIMDSubtractVector3(movement2, movement1);

    const float radiusSum = iS1.Radius() + iS2.Radius();
    const float radiusSumSquared = radiusSum * radiusSum;
    const float centerDistSquared = VxSIMDDot3Scalar(centerDiff, centerDiff);

    if (centerDistSquared <= radiusSumSquared) {
        *collisionTime1 = 0.0f;
        *collisionTime2 = 0.0f;
        return TRUE;
    }

    const float c = centerDistSquared - radiusSumSquared;
    const float b = 2.0f * VxSIMDDot3Scalar(relativeMovement, centerDiff);
    const float a = VxSIMDDot3Scalar(relativeMovement, relativeMovement);

    if (!QuadraticFormula(a, b, c, collisionTime1, collisionTime2)) {
        return FALSE;
    }

    if (*collisionTime1 > *collisionTime2) {
        const float temp = *collisionTime1;
        *collisionTime1 = *collisionTime2;
        *collisionTime2 = temp;
    }

    return (*collisionTime1 <= 1.0f && *collisionTime1 >= 0.0f);
#endif
}

static int RaySphereSIMDCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
#if defined(VX_SIMD_SSE2)
    const __m128 origin = VxSIMDLoadFloat3(&iRay.m_Origin.x);
    const __m128 direction = VxSIMDLoadFloat3(&iRay.m_Direction.x);
    const __m128 center = VxSIMDLoadFloat3(&iSphere.Center().x);
    const __m128d originXY = _mm_cvtps_pd(origin);
    const __m128d originZ = _mm_cvtps_pd(_mm_movehl_ps(origin, origin));
    const __m128d directionXY = _mm_cvtps_pd(direction);
    const __m128d directionZ = _mm_cvtps_pd(_mm_movehl_ps(direction, direction));
    const __m128d deltaXY = _mm_sub_pd(originXY, _mm_cvtps_pd(center));
    const __m128d deltaZ = _mm_sub_sd(originZ, _mm_cvtps_pd(_mm_movehl_ps(center, center)));
    const __m128d squareXY = _mm_mul_pd(directionXY, directionXY);
    const __m128d a = _mm_add_sd(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY)), _mm_mul_sd(directionZ, directionZ));
    if (_mm_cvtsd_f64(a) == 0) return 0;
    double d[3], v[3];
    _mm_storeu_pd(d, deltaXY); d[2] = _mm_cvtsd_f64(deltaZ);
    _mm_storeu_pd(v, directionXY); v[2] = _mm_cvtsd_f64(directionZ);
    const __m128d crossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(d[1], d[2]), _mm_setr_pd(v[2], v[0])),
                                     _mm_mul_pd(_mm_setr_pd(d[2], d[0]), _mm_setr_pd(v[1], v[2])));
    const __m128d crossZ = _mm_sub_sd(_mm_mul_sd(_mm_set_sd(d[0]), _mm_set_sd(v[1])), _mm_mul_sd(_mm_set_sd(d[1]), _mm_set_sd(v[0])));
    const __m128d crossSquareXY = _mm_mul_pd(crossXY, crossXY);
    const __m128d crossSquared = _mm_add_sd(_mm_add_sd(crossSquareXY, _mm_unpackhi_pd(crossSquareXY, crossSquareXY)), _mm_mul_sd(crossZ, crossZ));
    const __m128d radius = _mm_set_sd(iSphere.Radius());
    const __m128d discriminant = _mm_sub_sd(_mm_mul_sd(_mm_mul_sd(radius, radius), a), crossSquared);
    const double disc = _mm_cvtsd_f64(discriminant);
    if (disc < 0) return 0;
    const __m128d projectedXY = _mm_mul_pd(deltaXY, directionXY);
    const __m128d projection = _mm_add_sd(_mm_add_sd(projectedXY, _mm_unpackhi_pd(projectedXY, projectedXY)), _mm_mul_sd(deltaZ, directionZ));
    const __m128d closestT = _mm_div_sd(_mm_sub_sd(_mm_setzero_pd(), projection), a);
    const __m128d offsetT = _mm_div_sd(_mm_sqrt_sd(discriminant, discriminant), a);
    const __m128d closestXY = _mm_add_pd(originXY, _mm_mul_pd(directionXY, _mm_unpacklo_pd(closestT, closestT)));
    const __m128d closestZ = _mm_add_sd(originZ, _mm_mul_sd(directionZ, closestT));
    const __m128d offsetXY = _mm_mul_pd(directionXY, _mm_unpacklo_pd(offsetT, offsetT));
    const __m128d offsetZ = _mm_mul_sd(directionZ, offsetT);
    const __m128 first = _mm_movelh_ps(_mm_cvtpd_ps(_mm_sub_pd(closestXY, offsetXY)), _mm_cvtsd_ss(_mm_setzero_ps(), _mm_sub_sd(closestZ, offsetZ)));
    if (disc == 0) {
        if (oInter1) VxSIMDStoreFloat3(&oInter1->x, first);
        else if (oInter2) VxSIMDStoreFloat3(&oInter2->x, first);
        return 1;
    }
    if (oInter1) VxSIMDStoreFloat3(&oInter1->x, first);
    if (oInter2) {
        const __m128 second = _mm_movelh_ps(_mm_cvtpd_ps(_mm_add_pd(closestXY, offsetXY)), _mm_cvtsd_ss(_mm_setzero_ps(), _mm_add_sd(closestZ, offsetZ)));
        VxSIMDStoreFloat3(&oInter2->x, second);
    }
    return 2;
#else
    const float dirMagnitudeSq = VxSIMDDot3Scalar(iRay.m_Direction, iRay.m_Direction);
    if (dirMagnitudeSq <= EPSILON) {
        return 0;
    }

    const float invMagnitude = 1.0f / sqrtf(dirMagnitudeSq);
    VxVector normalizedDir;
    {
        const __m128 dir = VxSIMDLoadFloat3(&iRay.m_Direction.x);
        VxSIMDStoreFloat3(&normalizedDir.x, _mm_mul_ps(dir, _mm_set1_ps(invMagnitude)));
    }

    const VxVector center = iSphere.Center();
    const VxVector toCenter = VxSIMDSubtractVector3(center, iRay.m_Origin);
    const float projection = VxSIMDDot3Scalar(normalizedDir, toCenter);

    const float radius = iSphere.Radius();
    const float radiusSquared = radius * radius;
    const float toCenterSq = VxSIMDDot3Scalar(toCenter, toCenter);
    const float discriminant = radiusSquared - (toCenterSq - projection * projection);

    if (discriminant < 0.0f) {
        return 0;
    }

    if (discriminant == 0.0f) {
        const VxVector hit = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, projection);
        if (oInter1) {
            *oInter1 = hit;
        } else if (oInter2) {
            *oInter2 = hit;
        }
        return 1;
    }

    const float sqrtDiscriminant = sqrtf(discriminant);
    const float t1 = projection - sqrtDiscriminant;
    const float t2 = projection + sqrtDiscriminant;
    if (oInter1) {
        *oInter1 = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, t1);
    }
    if (oInter2) {
        *oInter2 = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, t2);
    }
    return 2;
#endif
}

static XBOOL SphereAABBMSIMDCore(const VxSphere &iSphere, const VxBbox &iBox) {
#if defined(VX_SIMD_SSE2)
    const __m128d center = _mm_cvtps_pd(VxSIMDLoadFloat3(&iSphere.Center().x));
    const __m128d minimum = _mm_cvtps_pd(VxSIMDLoadFloat3(&iBox.Min.x));
    const __m128d maximum = _mm_cvtps_pd(VxSIMDLoadFloat3(&iBox.Max.x));
    const __m128d delta = _mm_sub_pd(center, _mm_min_pd(_mm_max_pd(center, minimum), maximum));
    const __m128d square = _mm_mul_pd(delta, delta);
    const double z = iSphere.Center().z, lowZ = iBox.Min.z, highZ = iBox.Max.z;
    const double deltaZ = z < lowZ ? z - lowZ : z > highZ ? z - highZ : 0;
    const double squaredDistance = _mm_cvtsd_f64(_mm_add_sd(square, _mm_unpackhi_pd(square, square))) + deltaZ * deltaZ;
    return squaredDistance <= double(iSphere.Radius()) * iSphere.Radius();
#else
    const __m128 minV = VxSIMDLoadFloat3(&iBox.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&iBox.Max.x);
    const __m128 sphereCenter = VxSIMDLoadFloat3(&iSphere.Center().x);
    const __m128 clamped = _mm_min_ps(_mm_max_ps(sphereCenter, minV), maxV);
    const __m128 delta = _mm_sub_ps(sphereCenter, clamped);
    const float sqDist = _mm_cvtss_f32(VxSIMDDotProduct3(delta, delta));
    const float radius = iSphere.Radius();
    return (sqDist <= radius * radius);
#endif
}
#endif

} // namespace

void VxIntersectSphereDispatchRebuild(bool useSIMD) {
#if defined(VX_SIMD_SSE)
    const VxIntersectSphereDispatchTable *next = useSIMD ? &kVxIntersectSphereDispatchSIMD : &kVxIntersectSphereDispatchScalar;
#else
    (void) useSIMD;
    const VxIntersectSphereDispatchTable *next = &kVxIntersectSphereDispatchScalar;
#endif
    g_VxIntersectSphereDispatch.store(next, std::memory_order_release);
}

//--------- Spheres

XBOOL VxIntersect::SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                float *oCollisionTime1, float *oCollisionTime2) {
    return GetVxIntersectSphereDispatchTable()->sphereSphere(iS1, iP1, iS2, iP2, oCollisionTime1, oCollisionTime2);
}

int VxIntersect::RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
    return GetVxIntersectSphereDispatchTable()->raySphere(iRay, iSphere, oInter1, oInter2);
}

XBOOL VxIntersect::SphereAABB(const VxSphere &iSphere, const VxBbox &iBox) {
    return GetVxIntersectSphereDispatchTable()->sphereAABB(iSphere, iBox);
}



