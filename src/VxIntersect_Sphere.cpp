#include "VxIntersect.h"

#include "VxVector.h"
#include "VxRay.h"
#include "VxSphere.h"
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
 * @param t1 - pointer to store first solution
 * @param t2 - pointer to store second solution
 * @return 1 if solutions found, 0 if no real solutions
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
        const double inv2a = 1.0 / ((double)a + (double)a);
        *t1 = (float)((sqrt_disc - (double)b) * inv2a);
        *t2 = (float)((-(double)b - sqrt_disc) * inv2a);
        return 1;
    }

    // One repeated root only when disc == 0.
    if (disc == 0.0) {
        const float root = (float)(-(double)b / ((double)a + (double)a));
        *t1 = root;
        *t2 = root;
        return 1;
    }

    *t1 = 0.0f;
    *t2 = 0.0f;
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

VxIntersectSphereDispatchTable g_VxIntersectSphereDispatch = {
    SphereSphereScalarCore,
    RaySphereScalarCore,
    SphereAABBScalarCore
};

static XBOOL SphereSphereScalarCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                    float *oCollisionTime1, float *oCollisionTime2) {
    float fallbackCollisionTime1 = 0.0f;
    float fallbackCollisionTime2 = 0.0f;
    float *collisionTime1 = (oCollisionTime1 != nullptr) ? oCollisionTime1 : &fallbackCollisionTime1;
    float *collisionTime2 = (oCollisionTime2 != nullptr) ? oCollisionTime2 : &fallbackCollisionTime2;

    VxVector movement1 = iP1 - iS1.Center();
    VxVector movement2 = iP2 - iS2.Center();
    VxVector centerDiff = iS2.Center() - iS1.Center();
    VxVector relativeMovement = movement2 - movement1;

    float radiusSum = iS1.Radius() + iS2.Radius();
    float radiusSumSquared = radiusSum * radiusSum;
    float centerDistSquared = SquareMagnitude(centerDiff);

    if (centerDistSquared <= radiusSumSquared) {
        *collisionTime1 = 0.0f;
        *collisionTime2 = 0.0f;
        return TRUE;
    }

    float c = centerDistSquared - radiusSumSquared;
    float b = 2.0f * DotProduct(relativeMovement, centerDiff);
    float a = SquareMagnitude(relativeMovement);

    if (!QuadraticFormula(a, b, c, collisionTime1, collisionTime2))
        return FALSE;

    if (*collisionTime1 > *collisionTime2) {
        float temp = *collisionTime1;
        *collisionTime1 = *collisionTime2;
        *collisionTime2 = temp;
    }

    return (*collisionTime1 <= 1.0f && *collisionTime1 >= 0.0f);
}

static int RaySphereScalarCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
    const float dirMagnitudeSq = SquareMagnitude(iRay.m_Direction);
    if (dirMagnitudeSq <= EPSILON) {
        return 0;
    }

    const float invMagnitude = 1.0f / sqrtf(dirMagnitudeSq);
    const VxVector normalizedDir = iRay.m_Direction * invMagnitude;

    const VxVector &center = iSphere.Center();
    float toCenterX = center.x - iRay.m_Origin.x;
    float toCenterY = center.y - iRay.m_Origin.y;
    float toCenterZ = center.z - iRay.m_Origin.z;

    float projection = normalizedDir.z * toCenterZ + normalizedDir.y * toCenterY + toCenterX * normalizedDir.x;
    float radius = iSphere.Radius();
    float radiusSquared = radius * radius;
    float discriminant = radiusSquared - (toCenterZ * toCenterZ + toCenterY * toCenterY + toCenterX * toCenterX - projection * projection);

    if (discriminant < 0.0f)
        return 0;

    if (discriminant == 0.0f) {
        const VxVector hit = iRay.m_Origin + normalizedDir * projection;
        if (oInter1) {
            *oInter1 = hit;
        } else if (oInter2) {
            *oInter2 = hit;
        }
        return 1;
    }

    float sqrtDiscriminant = sqrtf(discriminant);
    float t1 = projection - sqrtDiscriminant;
    float t2 = projection + sqrtDiscriminant;
    if (oInter1) {
        *oInter1 = iRay.m_Origin + normalizedDir * t1;
    }
    if (oInter2) {
        *oInter2 = iRay.m_Origin + normalizedDir * t2;
    }
    return 2;
}

static XBOOL SphereAABBScalarCore(const VxSphere &iSphere, const VxBbox &iBox) {
    const VxVector &center = iSphere.Center();
    float sqDist = 0.0f;

    for (int i = 0; i < 3; ++i) {
        if (center[i] < iBox.Min[i]) {
            const float delta = iBox.Min[i] - center[i];
            sqDist += delta * delta;
        } else if (center[i] > iBox.Max[i]) {
            const float delta = center[i] - iBox.Max[i];
            sqDist += delta * delta;
        }
    }

    const float radius = iSphere.Radius();
    return (sqDist <= radius * radius);
}

#if defined(VX_SIMD_SSE)
static XBOOL SphereSphereSIMDCore(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                  float *oCollisionTime1, float *oCollisionTime2) {
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
}

static int RaySphereSIMDCore(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
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
}

static XBOOL SphereAABBMSIMDCore(const VxSphere &iSphere, const VxBbox &iBox) {
    const __m128 minV = VxSIMDLoadFloat3(&iBox.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&iBox.Max.x);
    const __m128 sphereCenter = VxSIMDLoadFloat3(&iSphere.Center().x);
    const __m128 clamped = _mm_min_ps(_mm_max_ps(sphereCenter, minV), maxV);
    const __m128 delta = _mm_sub_ps(sphereCenter, clamped);
    const float sqDist = _mm_cvtss_f32(VxSIMDDotProduct3(delta, delta));
    const float radius = iSphere.Radius();
    return (sqDist <= radius * radius);
}
#endif

} // namespace

void VxIntersectSphereDispatchRebuild(bool useSIMD) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectSphereDispatch.sphereSphere = useSIMD ? SphereSphereSIMDCore : SphereSphereScalarCore;
    g_VxIntersectSphereDispatch.raySphere = useSIMD ? RaySphereSIMDCore : RaySphereScalarCore;
    g_VxIntersectSphereDispatch.sphereAABB = useSIMD ? SphereAABBMSIMDCore : SphereAABBScalarCore;
#else
    (void) useSIMD;
    g_VxIntersectSphereDispatch.sphereSphere = SphereSphereScalarCore;
    g_VxIntersectSphereDispatch.raySphere = RaySphereScalarCore;
    g_VxIntersectSphereDispatch.sphereAABB = SphereAABBScalarCore;
#endif
}

//--------- Spheres

XBOOL VxIntersect::SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                float *oCollisionTime1, float *oCollisionTime2) {
    return g_VxIntersectSphereDispatch.sphereSphere(iS1, iP1, iS2, iP2, oCollisionTime1, oCollisionTime2);
}

int VxIntersect::RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
    return g_VxIntersectSphereDispatch.raySphere(iRay, iSphere, oInter1, oInter2);
}

XBOOL VxIntersect::SphereAABB(const VxSphere &iSphere, const VxBbox &iBox) {
    return g_VxIntersectSphereDispatch.sphereAABB(iSphere, iBox);
}



