#include "VxIntersect.h"

#include "VxVector.h"
#include "VxRay.h"
#include "VxSphere.h"
#include "VxPlane.h"
#include "VxSIMD.h"

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

static inline float VxSIMDClassifyAABBWithPlane(__m128 normal, float planeD, const VxBbox& box) {
    const __m128 minV = VxSIMDLoadFloat3(&box.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&box.Max.x);
    const __m128 signMask = _mm_cmpge_ps(normal, _mm_setzero_ps());
    const __m128 nearPoint = _mm_or_ps(_mm_and_ps(signMask, minV), _mm_andnot_ps(signMask, maxV));
    const __m128 farPoint = _mm_or_ps(_mm_and_ps(signMask, maxV), _mm_andnot_ps(signMask, minV));

    const float nearDist = _mm_cvtss_f32(_mm_add_ss(VxSIMDDotProduct3(normal, nearPoint), _mm_set_ss(planeD)));
    if (nearDist > 0.0f) {
        return nearDist;
    }

    const float farDist = _mm_cvtss_f32(_mm_add_ss(VxSIMDDotProduct3(normal, farPoint), _mm_set_ss(planeD)));
    if (farDist < 0.0f) {
        return farDist;
    }
    return 0.0f;
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

//--------- Spheres

// Sphere-Sphere intersection with movement
XBOOL VxIntersect::SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2,
                                float *oCollisionTime1, float *oCollisionTime2) {
#if defined(VX_SIMD_SSE)
    const VxVector movement1 = VxSIMDSubtractVector3(iP1, iS1.Center());
    const VxVector movement2 = VxSIMDSubtractVector3(iP2, iS2.Center());
    const VxVector centerDiff = VxSIMDSubtractVector3(iS2.Center(), iS1.Center());
    const VxVector relativeMovement = VxSIMDSubtractVector3(movement2, movement1);

    const float radiusSum = iS1.Radius() + iS2.Radius();
    const float radiusSumSquared = radiusSum * radiusSum;
    const float centerDistSquared = VxSIMDDot3Scalar(centerDiff, centerDiff);

    if (centerDistSquared <= radiusSumSquared) {
        *oCollisionTime1 = 0.0f;
        *oCollisionTime2 = 0.0f;
        return TRUE;
    }

    const float c = centerDistSquared - radiusSumSquared;
    const float b = 2.0f * VxSIMDDot3Scalar(relativeMovement, centerDiff);
    const float a = VxSIMDDot3Scalar(relativeMovement, relativeMovement);

    if (!QuadraticFormula(a, b, c, oCollisionTime1, oCollisionTime2)) {
        return FALSE;
    }

    if (*oCollisionTime1 > *oCollisionTime2) {
        const float temp = *oCollisionTime1;
        *oCollisionTime1 = *oCollisionTime2;
        *oCollisionTime2 = temp;
    }

    return (*oCollisionTime1 <= 1.0f && *oCollisionTime1 >= 0.0f);
#else
    // Calculate movement vectors from sphere centers to target positions
    VxVector movement1 = iP1 - iS1.Center();
    VxVector movement2 = iP2 - iS2.Center();

    // Vector from sphere1 center to sphere2 center
    VxVector centerDiff = iS2.Center() - iS1.Center();

    // Relative movement vector (difference of movements)
    VxVector relativeMovement = movement2 - movement1;

    // Sum of radii and its square
    float radiusSum = iS1.Radius() + iS2.Radius();
    float radiusSumSquared = radiusSum * radiusSum;

    // Square distance between sphere centers
    float centerDistSquared = SquareMagnitude(centerDiff);

    // Check if spheres are already intersecting
    if (centerDistSquared <= radiusSumSquared) {
        *oCollisionTime1 = 0.0f;
        *oCollisionTime2 = 0.0f;
        return TRUE;
    }

    // Set up quadratic equation coefficients for collision detection
    float c = centerDistSquared - radiusSumSquared;
    float b = 2.0f * DotProduct(relativeMovement, centerDiff);
    float a = SquareMagnitude(relativeMovement);

    // Solve the quadratic equation
    if (!QuadraticFormula(a, b, c, oCollisionTime1, oCollisionTime2))
        return FALSE;

    // Ensure t1 <= t2 (swap if necessary)
    if (*oCollisionTime1 > *oCollisionTime2) {
        float temp = *oCollisionTime1;
        *oCollisionTime1 = *oCollisionTime2;
        *oCollisionTime2 = temp;
    }

    // Check if collision occurs within valid time range [0, 1]
    return (*oCollisionTime1 <= 1.0f && *oCollisionTime1 >= 0.0f);
#endif
}

// Intersection Ray - Sphere
int VxIntersect::RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2) {
#if defined(VX_SIMD_SSE)
    const float invMagnitude = 1.0f / sqrtf(VxSIMDDot3Scalar(iRay.m_Direction, iRay.m_Direction));
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
        *oInter1 = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, projection);
        return 1;
    }

    const float sqrtDiscriminant = sqrtf(discriminant);
    const float t1 = projection - sqrtDiscriminant;
    const float t2 = projection + sqrtDiscriminant;
    *oInter1 = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, t1);
    *oInter2 = VxSIMDAddScaledVector3(iRay.m_Origin, normalizedDir, t2);
    return 2;
#else
    // Normalize the ray direction vector.
    const float invMagnitude = 1.0f / sqrtf(SquareMagnitude(iRay.m_Direction));
    const VxVector normalizedDir = iRay.m_Direction * invMagnitude;

    // Vector from ray origin to sphere center
    const VxVector &center = iSphere.Center();
    float toCenterX = center.x - iRay.m_Origin.x;
    float toCenterY = center.y - iRay.m_Origin.y;
    float toCenterZ = center.z - iRay.m_Origin.z;

    // Project the toCenter vector onto the normalized ray direction
    float projection = normalizedDir.z * toCenterZ + normalizedDir.y * toCenterY + toCenterX * normalizedDir.x;

    // Calculate the discriminant using geometric approach
    float radius = iSphere.Radius();
    float radiusSquared = radius * radius;
    float discriminant = radiusSquared - (toCenterZ * toCenterZ + toCenterY * toCenterY + toCenterX * toCenterX - projection * projection);

    if (discriminant < 0.0f)
        return 0; // No intersection

    if (discriminant == 0.0f) {
        // Single intersection point (ray is tangent to sphere)
        *oInter1 = iRay.m_Origin + normalizedDir * projection;
        return 1;
    }

    // Two intersection points
    float sqrtDiscriminant = sqrtf(discriminant);
    float t1 = projection - sqrtDiscriminant;
    float t2 = projection + sqrtDiscriminant;

    // Compute first intersection point
    *oInter1 = iRay.m_Origin + normalizedDir * t1;

    // Compute second intersection point
    *oInter2 = iRay.m_Origin + normalizedDir * t2;

    return 2;
#endif
}

// Intersection Sphere - AABB
XBOOL VxIntersect::SphereAABB(const VxSphere &iSphere, const VxBbox &iBox) {
#if defined(VX_SIMD_SSE)
    const __m128 minV = VxSIMDLoadFloat3(&iBox.Min.x);
    const __m128 maxV = VxSIMDLoadFloat3(&iBox.Max.x);
    const __m128 boxCenter = _mm_mul_ps(_mm_add_ps(minV, maxV), _mm_set1_ps(0.5f));
    const __m128 sphereCenter = VxSIMDLoadFloat3(&iSphere.Center().x);
    __m128 normal = _mm_sub_ps(boxCenter, sphereCenter);

    const float magSq = _mm_cvtss_f32(VxSIMDDotProduct3(normal, normal));
    if (magSq > EPSILON) {
        normal = _mm_mul_ps(normal, _mm_set1_ps(1.0f / sqrtf(magSq)));
    }

    const float planeD = -_mm_cvtss_f32(VxSIMDDotProduct3(normal, sphereCenter));
    const float signedDist = VxSIMDClassifyAABBWithPlane(normal, planeD, iBox);
    return (fabsf(signedDist) <= iSphere.Radius());
#else
    const VxVector boxCenter = (iBox.Min + iBox.Max) * 0.5f;

    VxPlane plane;
    plane.Create(boxCenter - iSphere.Center(), iSphere.Center());

    const float signedDist = plane.Classify(iBox);
    return (fabsf(signedDist) <= iSphere.Radius());
#endif
}
