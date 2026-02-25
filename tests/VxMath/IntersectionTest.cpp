#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include "VxMath.h"

#include "VxIntersect.h"
#include "VxMatrix.h"
#include "VxOBB.h"
#include "VxPlane.h"
#include "VxRay.h"
#include "VxSphere.h"
#include "VxVector.h"

#include "VxMathTestHelpers.h"

using namespace VxMathTest;

static VxRay MakeRay(const VxVector &origin, const VxVector &direction) {
    // Use the (origin, direction, nullptr) overload.
    return VxRay(origin, direction, nullptr);
}

static VxRay MakeSegment(const VxVector &start, const VxVector &end) {
    // VxRay(start, end) stores direction = end-start, which is what VxIntersect segment APIs expect.
    return VxRay(start, end);
}

namespace {

float Dot3Ref(const VxVector& a, const VxVector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool QuadraticFormulaRef(float a, float b, float c, float* t1, float* t2) {
    if (!(a > 0.0f || a < 0.0f)) {
        if (b == 0.0f) {
            *t1 = 0.0f;
            *t2 = 0.0f;
            return false;
        }
        const float root = -(c / b);
        *t1 = root;
        *t2 = root;
        return true;
    }

    const double disc = (double)b * (double)b - 4.0 * (double)a * (double)c;
    if (disc > 0.0) {
        const double sqrtDisc = std::sqrt(disc);
        const double inv2a = 1.0 / ((double)a + (double)a);
        *t1 = (float)((sqrtDisc - (double)b) * inv2a);
        *t2 = (float)((-(double)b - sqrtDisc) * inv2a);
        return true;
    }
    if (disc == 0.0) {
        const float root = (float)(-(double)b / ((double)a + (double)a));
        *t1 = root;
        *t2 = root;
        return true;
    }

    *t1 = 0.0f;
    *t2 = 0.0f;
    return false;
}

enum class PlaneMode {
    Ray,
    RayCulled,
    Segment,
    SegmentCulled,
    Line
};

bool IntersectPlaneRef(const VxRay& ray, const VxPlane& plane, PlaneMode mode, VxVector& point, float& dist) {
    constexpr float kPlaneParallelEps = EPSILON;
    constexpr float kSegmentMaxT = 1.0f + EPSILON;

    const float denom = Dot3Ref(plane.m_Normal, ray.m_Direction);
    if (mode == PlaneMode::RayCulled || mode == PlaneMode::SegmentCulled) {
        if (denom > -kPlaneParallelEps) {
            return false;
        }
    } else {
        if (std::fabs(denom) < kPlaneParallelEps) {
            return false;
        }
    }

    dist = -(Dot3Ref(plane.m_Normal, ray.m_Origin) + plane.m_D) / denom;

    if (mode == PlaneMode::Ray || mode == PlaneMode::RayCulled || mode == PlaneMode::Segment || mode == PlaneMode::SegmentCulled) {
        if (dist < -kPlaneParallelEps) {
            return false;
        }
    }
    if (mode == PlaneMode::Segment || mode == PlaneMode::SegmentCulled) {
        if (dist > kSegmentMaxT) {
            return false;
        }
    }

    point = ray.m_Origin + ray.m_Direction * dist;
    return true;
}

int RaySphereRef(const VxRay& ray, const VxSphere& sphere, VxVector* inter1, VxVector* inter2) {
    const float invMagnitude = 1.0f / std::sqrt(Dot3Ref(ray.m_Direction, ray.m_Direction));
    const VxVector normalizedDir = ray.m_Direction * invMagnitude;

    const VxVector center = sphere.Center();
    const float toCenterX = center.x - ray.m_Origin.x;
    const float toCenterY = center.y - ray.m_Origin.y;
    const float toCenterZ = center.z - ray.m_Origin.z;
    const float projection = normalizedDir.x * toCenterX + normalizedDir.y * toCenterY + normalizedDir.z * toCenterZ;

    const float radius = sphere.Radius();
    const float radiusSq = radius * radius;
    const float toCenterSq = toCenterX * toCenterX + toCenterY * toCenterY + toCenterZ * toCenterZ;
    const float discriminant = radiusSq - (toCenterSq - projection * projection);

    if (discriminant < 0.0f) {
        return 0;
    }
    if (discriminant == 0.0f) {
        if (inter1) {
            *inter1 = ray.m_Origin + normalizedDir * projection;
        }
        return 1;
    }

    const float sqrtDiscriminant = std::sqrt(discriminant);
    const float t1 = projection - sqrtDiscriminant;
    const float t2 = projection + sqrtDiscriminant;
    if (inter1) {
        *inter1 = ray.m_Origin + normalizedDir * t1;
    }
    if (inter2) {
        *inter2 = ray.m_Origin + normalizedDir * t2;
    }
    return 2;
}

bool SphereSphereRef(const VxSphere& s1, const VxVector& p1, const VxSphere& s2, const VxVector& p2, float* t1, float* t2) {
    const VxVector movement1 = p1 - s1.Center();
    const VxVector movement2 = p2 - s2.Center();
    const VxVector centerDiff = s2.Center() - s1.Center();
    const VxVector relativeMovement = movement2 - movement1;

    const float radiusSum = s1.Radius() + s2.Radius();
    const float radiusSumSq = radiusSum * radiusSum;
    const float centerDistSq = Dot3Ref(centerDiff, centerDiff);

    if (centerDistSq <= radiusSumSq) {
        *t1 = 0.0f;
        *t2 = 0.0f;
        return true;
    }

    const float c = centerDistSq - radiusSumSq;
    const float b = 2.0f * Dot3Ref(relativeMovement, centerDiff);
    const float a = Dot3Ref(relativeMovement, relativeMovement);
    if (!QuadraticFormulaRef(a, b, c, t1, t2)) {
        return false;
    }

    if (*t1 > *t2) {
        const float tmp = *t1;
        *t1 = *t2;
        *t2 = tmp;
    }

    return (*t1 <= 1.0f && *t1 >= 0.0f);
}

bool SphereAABBRef(const VxSphere& sphere, const VxBbox& box) {
    const VxVector& center = sphere.Center();
    float sqDist = 0.0f;

    for (int i = 0; i < 3; ++i) {
        if (center[i] < box.Min[i]) {
            const float delta = box.Min[i] - center[i];
            sqDist += delta * delta;
        } else if (center[i] > box.Max[i]) {
            const float delta = center[i] - box.Max[i];
            sqDist += delta * delta;
        }
    }

    const float radius = sphere.Radius();
    return sqDist <= radius * radius;
}

float Dot3RefArray(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

VxVector MulMatVec3Ref(const VxMatrix& mat, const VxVector& v) {
    VxVector out;
    out.x = v.x * mat[0][0] + v.y * mat[1][0] + v.z * mat[2][0] + mat[3][0];
    out.y = v.x * mat[0][1] + v.y * mat[1][1] + v.z * mat[2][1] + mat[3][1];
    out.z = v.x * mat[0][2] + v.y * mat[1][2] + v.z * mat[2][2] + mat[3][2];
    return out;
}

bool BoxPlaneRef(const VxBbox& box, const VxPlane& plane) {
    VxVector minPt = box.Min;
    VxVector maxPt = box.Max;

    for (int i = 0; i < 3; ++i) {
        if (plane.m_Normal[i] < 0.0f) {
            minPt[i] = box.Max[i];
            maxPt[i] = box.Min[i];
        }
    }

    const float minDist = Dot3Ref(plane.m_Normal, minPt) + plane.m_D;
    const float maxDist = Dot3Ref(plane.m_Normal, maxPt) + plane.m_D;
    return (minDist <= 0.0f && maxDist >= 0.0f) || (maxDist == 0.0f);
}

bool BoxPlaneRef(const VxBbox& box, const VxMatrix& mat, const VxPlane& plane) {
    const VxVector halfSize = box.GetHalfSize();
    const float radius =
        std::fabs(Dot3Ref(plane.m_Normal, mat[0] * halfSize.x)) +
        std::fabs(Dot3Ref(plane.m_Normal, mat[1] * halfSize.y)) +
        std::fabs(Dot3Ref(plane.m_Normal, mat[2] * halfSize.z));

    const VxVector center = box.GetCenter();
    const VxVector transformedCenter = MulMatVec3Ref(mat, center);
    const float dist = Dot3Ref(plane.m_Normal, transformedCenter) + plane.m_D;
    return std::fabs(dist) <= radius;
}

float Determinant3Ref(const VxVector& r0, const VxVector& r1, const VxVector& r2) {
    return r0.x * (r1.y * r2.z - r1.z * r2.y) -
           r0.y * (r1.x * r2.z - r1.z * r2.x) +
           r0.z * (r1.x * r2.y - r1.y * r2.x);
}

bool PlanesRef(const VxPlane& plane1, const VxPlane& plane2, const VxPlane& plane3, VxVector& out) {
    const VxVector r0 = plane1.m_Normal;
    const VxVector r1 = plane2.m_Normal;
    const VxVector r2 = plane3.m_Normal;

    const float det = Determinant3Ref(r0, r1, r2);
    if (det == 0.0f) {
        return false;
    }

    const VxVector x0(-plane1.m_D, plane1.m_Normal.y, plane1.m_Normal.z);
    const VxVector x1(-plane2.m_D, plane2.m_Normal.y, plane2.m_Normal.z);
    const VxVector x2(-plane3.m_D, plane3.m_Normal.y, plane3.m_Normal.z);
    const float detX = Determinant3Ref(x0, x1, x2);

    const VxVector y0(plane1.m_Normal.x, -plane1.m_D, plane1.m_Normal.z);
    const VxVector y1(plane2.m_Normal.x, -plane2.m_D, plane2.m_Normal.z);
    const VxVector y2(plane3.m_Normal.x, -plane3.m_D, plane3.m_Normal.z);
    const float detY = Determinant3Ref(y0, y1, y2);

    const VxVector z0(plane1.m_Normal.x, plane1.m_Normal.y, -plane1.m_D);
    const VxVector z1(plane2.m_Normal.x, plane2.m_Normal.y, -plane2.m_D);
    const VxVector z2(plane3.m_Normal.x, plane3.m_Normal.y, -plane3.m_D);
    const float detZ = Determinant3Ref(z0, z1, z2);

    const float invDet = 1.0f / det;
    out.x = detX * invDet;
    out.y = detY * invDet;
    out.z = detZ * invDet;
    return true;
}

bool PointInAABB_OutCodeRef(const VxBbox& box, const VxVector& p, unsigned int& outCode) {
    unsigned int code = 0;
    if (p.x < box.Min.x) {
        code |= 0x02;
    } else if (!(p.x < box.Max.x)) {
        code |= 0x01;
    }

    if (p.y < box.Min.y) {
        code |= 0x08;
    } else if (!(p.y < box.Max.y)) {
        code |= 0x04;
    }

    if (p.z < box.Min.z) {
        code |= 0x10;
    } else if (!(p.z < box.Max.z)) {
        code |= 0x20;
    }

    outCode = code;
    return code == 0;
}

bool AABBFaceRef(const VxBbox& box, const VxVector& a0, const VxVector& a1, const VxVector& a2, const VxVector& n) {
    unsigned int code0 = 0;
    unsigned int code1 = 0;
    unsigned int code2 = 0;
    if (PointInAABB_OutCodeRef(box, a0, code0)) {
        return true;
    }
    if (PointInAABB_OutCodeRef(box, a1, code1)) {
        return true;
    }
    if (PointInAABB_OutCodeRef(box, a2, code2)) {
        return true;
    }

    if ((code0 & code1 & code2) != 0) {
        return false;
    }

    VxRay edge;
    edge.m_Origin = a0;
    edge.m_Direction = a1 - a0;
    if (VxIntersect::SegmentBox(edge, box)) {
        return true;
    }

    edge.m_Origin = a1;
    edge.m_Direction = a2 - a1;
    if (VxIntersect::SegmentBox(edge, box)) {
        return true;
    }

    edge.m_Origin = a0;
    edge.m_Direction = a2 - a0;
    if (VxIntersect::SegmentBox(edge, box)) {
        return true;
    }

    const VxVector corners[8] = {
        VxVector(box.Min.x, box.Min.y, box.Min.z),
        VxVector(box.Min.x, box.Min.y, box.Max.z),
        VxVector(box.Min.x, box.Max.y, box.Min.z),
        VxVector(box.Min.x, box.Max.y, box.Max.z),
        VxVector(box.Max.x, box.Min.y, box.Min.z),
        VxVector(box.Max.x, box.Min.y, box.Max.z),
        VxVector(box.Max.x, box.Max.y, box.Min.z),
        VxVector(box.Max.x, box.Max.y, box.Max.z)
    };

    int bestA = 0;
    int bestB = 7;
    float bestScore = std::fabs(Dot3Ref(corners[bestA] - corners[bestB], n));

    {
        const int bA = 1;
        const int bB = 6;
        const float score = std::fabs(Dot3Ref(corners[bA] - corners[bB], n));
        if (score < bestScore) {
            bestScore = score;
            bestA = bA;
            bestB = bB;
        }
    }
    {
        const int bA = 2;
        const int bB = 5;
        const float score = std::fabs(Dot3Ref(corners[bA] - corners[bB], n));
        if (score < bestScore) {
            bestScore = score;
            bestA = bA;
            bestB = bB;
        }
    }
    {
        const int bA = 3;
        const int bB = 4;
        const float score = std::fabs(Dot3Ref(corners[bA] - corners[bB], n));
        if (score < bestScore) {
            bestScore = score;
            bestA = bA;
            bestB = bB;
        }
    }

    VxRay diag;
    diag.m_Origin = corners[bestA];
    diag.m_Direction = corners[bestB] - corners[bestA];
    VxVector hit = VxVector::axis0();
    float dist = bestScore;
    return VxIntersect::SegmentFace(diag, a0, a1, a2, n, hit, dist) != FALSE;
}

bool PointInFaceRef(
    const VxVector& point,
    const VxVector& pt0,
    const VxVector& pt1,
    const VxVector& pt2,
    const VxVector& norm,
    int& i1,
    int& i2
) {
    float maxAbs = std::fabs(norm.x);
    i1 = 1;
    i2 = 2;
    if (maxAbs < std::fabs(norm.y)) {
        i1 = 0;
        i2 = 2;
        maxAbs = std::fabs(norm.y);
    }
    if (maxAbs < std::fabs(norm.z)) {
        i1 = 0;
        i2 = 1;
    }

    char flags = 1;
    if ((point[i1] - pt1[i1]) * (pt2[i2] - pt1[i2]) - (pt2[i1] - pt1[i1]) * (point[i2] - pt1[i2]) >= 0.0f) {
        flags = 2;
    }

    if ((point[i1] - pt2[i1]) * (pt0[i2] - pt2[i2]) - (point[i2] - pt2[i2]) * (pt0[i1] - pt2[i1]) >= 0.0f) {
        flags &= 2;
    } else {
        flags &= 1;
    }

    if (!flags) {
        return false;
    }

    if ((point[i1] - pt0[i1]) * (pt1[i2] - pt0[i2]) - (point[i2] - pt0[i2]) * (pt1[i1] - pt0[i1]) >= 0.0f) {
        return (flags & 2) != 0;
    }

    return (flags & 1) != 0;
}

void GetPointCoefficientsRef(
    const VxVector& pt,
    const VxVector& pt0,
    const VxVector& pt1,
    const VxVector& pt2,
    const int& i1,
    const int& i2,
    float& v0Coef,
    float& v1Coef,
    float& v2Coef
) {
    const float p0_i1 = pt0[i1];
    const float p0_i2 = pt0[i2];
    const float p1_i1 = pt1[i1];
    const float p1_i2 = pt1[i2];
    const float p2_i1 = pt2[i1];
    const float p2_i2 = pt2[i2];
    const float pt_i1 = pt[i1];
    const float pt_i2 = pt[i2];

    const float v1_i1 = pt_i1 - p0_i1;
    const float v1_i2 = pt_i2 - p0_i2;
    const float v2_i1 = p1_i1 - p0_i1;
    const float v2_i2 = p1_i2 - p0_i2;
    const float v3_i1 = p2_i1 - p0_i1;
    const float v3_i2 = p2_i2 - p0_i2;

    if (v2_i1 == 0.0f) {
        v2Coef = v1_i1 / v3_i1;
        v1Coef = (v1_i2 - v2Coef * v3_i2) / v2_i2;
    } else {
        const float denom = v3_i2 * v2_i1 - v3_i1 * v2_i2;
        v2Coef = (v2_i1 * v1_i2 - v2_i2 * v1_i1) / denom;
        v1Coef = (v1_i1 - v2Coef * v3_i1) / v2_i1;
    }

    v0Coef = 1.0f - (v1Coef + v2Coef);
}

bool AABBAABBRef(const VxBbox& box1, const VxBbox& box2) {
    return (box1.Min.x <= box2.Max.x) && (box1.Max.x >= box2.Min.x) &&
           (box1.Min.y <= box2.Max.y) && (box1.Max.y >= box2.Min.y) &&
           (box1.Min.z <= box2.Max.z) && (box1.Max.z >= box2.Min.z);
}

bool AABBOBBRef(const VxBbox& aabb, const VxOBB& obb) {
    const float eA[3] = {
        (aabb.Max.x - aabb.Min.x) * 0.5f,
        (aabb.Max.y - aabb.Min.y) * 0.5f,
        (aabb.Max.z - aabb.Min.z) * 0.5f
    };
    const float cA[3] = {
        (aabb.Max.x + aabb.Min.x) * 0.5f,
        (aabb.Max.y + aabb.Min.y) * 0.5f,
        (aabb.Max.z + aabb.Min.z) * 0.5f
    };
    const float cB[3] = {obb.m_Center.x, obb.m_Center.y, obb.m_Center.z};
    const float T[3] = {cB[0] - cA[0], cB[1] - cA[1], cB[2] - cA[2]};
    const float eB[3] = {obb.m_Extents.x, obb.m_Extents.y, obb.m_Extents.z};

    float R[3][3];
    float absR[3][3];
    for (int i = 0; i < 3; ++i) {
        R[0][i] = obb.m_Axis[i].x;
        R[1][i] = obb.m_Axis[i].y;
        R[2][i] = obb.m_Axis[i].z;
        absR[0][i] = std::fabs(R[0][i]);
        absR[1][i] = std::fabs(R[1][i]);
        absR[2][i] = std::fabs(R[2][i]);
    }

    for (int i = 0; i < 3; ++i) {
        const float ra = eA[i];
        const float rb = absR[i][0] * eB[0] + absR[i][1] * eB[1] + absR[i][2] * eB[2];
        if (std::fabs(T[i]) > ra + rb) {
            return false;
        }
    }

    for (int j = 0; j < 3; ++j) {
        const float axisB[3] = {obb.m_Axis[j].x, obb.m_Axis[j].y, obb.m_Axis[j].z};
        const float tProj = std::fabs(Dot3RefArray(T, axisB));
        const float ra = absR[0][j] * eA[0] + absR[1][j] * eA[1] + absR[2][j] * eA[2];
        const float rb = eB[j];
        if (tProj > ra + rb) {
            return false;
        }
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const int i1 = (i + 1) % 3;
            const int i2 = (i + 2) % 3;
            const int j1 = (j + 1) % 3;
            const int j2 = (j + 2) % 3;
            const float ra = eA[i1] * absR[i2][j] + eA[i2] * absR[i1][j];
            const float rb = eB[j1] * absR[i][j2] + eB[j2] * absR[i][j1];
            const float tProj = std::fabs(T[i2] * R[i1][j] - T[i1] * R[i2][j]);
            if (tProj > ra + rb) {
                return false;
            }
        }
    }

    return true;
}

bool OBBOBBRef(const VxOBB& box1, const VxOBB& box2) {
    const float eA[3] = {box1.m_Extents.x, box1.m_Extents.y, box1.m_Extents.z};
    const float eB[3] = {box2.m_Extents.x, box2.m_Extents.y, box2.m_Extents.z};

    const float A[3][3] = {
        {box1.m_Axis[0].x, box1.m_Axis[0].y, box1.m_Axis[0].z},
        {box1.m_Axis[1].x, box1.m_Axis[1].y, box1.m_Axis[1].z},
        {box1.m_Axis[2].x, box1.m_Axis[2].y, box1.m_Axis[2].z}
    };
    const float B[3][3] = {
        {box2.m_Axis[0].x, box2.m_Axis[0].y, box2.m_Axis[0].z},
        {box2.m_Axis[1].x, box2.m_Axis[1].y, box2.m_Axis[1].z},
        {box2.m_Axis[2].x, box2.m_Axis[2].y, box2.m_Axis[2].z}
    };

    const float worldT[3] = {
        box2.m_Center.x - box1.m_Center.x,
        box2.m_Center.y - box1.m_Center.y,
        box2.m_Center.z - box1.m_Center.z
    };

    float R[3][3];
    float absR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = A[i][0] * B[j][0] + A[i][1] * B[j][1] + A[i][2] * B[j][2];
            absR[i][j] = std::fabs(R[i][j]);
        }
    }

    float t[3];
    for (int i = 0; i < 3; ++i) {
        t[i] = A[i][0] * worldT[0] + A[i][1] * worldT[1] + A[i][2] * worldT[2];
    }

    for (int i = 0; i < 3; ++i) {
        const float ra = eA[i];
        const float rb = absR[i][0] * eB[0] + absR[i][1] * eB[1] + absR[i][2] * eB[2];
        if (std::fabs(t[i]) > ra + rb) {
            return false;
        }
    }

    for (int j = 0; j < 3; ++j) {
        const float tProj = std::fabs(worldT[0] * B[j][0] + worldT[1] * B[j][1] + worldT[2] * B[j][2]);
        const float ra = absR[0][j] * eA[0] + absR[1][j] * eA[1] + absR[2][j] * eA[2];
        const float rb = eB[j];
        if (tProj > ra + rb) {
            return false;
        }
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const int i1 = (i + 1) % 3;
            const int i2 = (i + 2) % 3;
            const int j1 = (j + 1) % 3;
            const int j2 = (j + 2) % 3;
            const float ra = eA[i1] * absR[i2][j] + eA[i2] * absR[i1][j];
            const float rb = eB[j1] * absR[i][j2] + eB[j2] * absR[i][j1];
            const float tProj = std::fabs(t[i2] * R[i1][j] - t[i1] * R[i2][j]);
            if (tProj > ra + rb) {
                return false;
            }
        }
    }

    return true;
}

bool RayBoxBoolRef(const VxRay& ray, const VxBbox& box) {
    const float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    const float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    const float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;
    const VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    const float px = ray.m_Origin.x - boxCenter.x;
    const float py = ray.m_Origin.y - boxCenter.y;
    const float pz = ray.m_Origin.z - boxCenter.z;

    if (!(std::fabs(px) <= boxHalfX) && px * ray.m_Direction.x >= 0.0f) return false;
    if (!(std::fabs(py) <= boxHalfY) && py * ray.m_Direction.y >= 0.0f) return false;
    if (!(std::fabs(pz) <= boxHalfZ) && pz * ray.m_Direction.z >= 0.0f) return false;

    const float crossX = pz * ray.m_Direction.y - py * ray.m_Direction.z;
    const float crossY = px * ray.m_Direction.z - pz * ray.m_Direction.x;
    const float crossZ = py * ray.m_Direction.x - px * ray.m_Direction.y;
    const float absDirX = std::fabs(ray.m_Direction.x);
    const float absDirY = std::fabs(ray.m_Direction.y);
    const float absDirZ = std::fabs(ray.m_Direction.z);

    if (std::fabs(crossX) > absDirZ * boxHalfY + absDirY * boxHalfZ) return false;
    if (std::fabs(crossY) > absDirX * boxHalfZ + absDirZ * boxHalfX) return false;
    if (std::fabs(crossZ) > absDirX * boxHalfY + absDirY * boxHalfX) return false;
    return true;
}

bool SegmentBoxBoolRef(const VxRay& segment, const VxBbox& box) {
    const float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    const float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    const float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;
    const VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    const float segHalfX = segment.m_Direction.x * 0.5f;
    const float segHalfY = segment.m_Direction.y * 0.5f;
    const float segHalfZ = segment.m_Direction.z * 0.5f;

    const float segCenterX = segHalfX + segment.m_Origin.x;
    const float segCenterY = segHalfY + segment.m_Origin.y;
    const float segCenterZ = segHalfZ + segment.m_Origin.z;

    const float dx = segCenterX - boxCenter.x;
    const float dy = segCenterY - boxCenter.y;
    const float dz = segCenterZ - boxCenter.z;

    const float absSegHalfX = std::fabs(segHalfX);
    const float absSegHalfY = std::fabs(segHalfY);
    const float absSegHalfZ = std::fabs(segHalfZ);
    if (std::fabs(dx) > absSegHalfX + boxHalfX) return false;
    if (std::fabs(dy) > absSegHalfY + boxHalfY) return false;
    if (std::fabs(dz) > absSegHalfZ + boxHalfZ) return false;

    const float crossX = dz * segHalfY - segHalfZ * dy;
    const float crossY = segHalfZ * dx - dz * segHalfX;
    const float crossZ = dy * segHalfX - segHalfY * dx;

    if (std::fabs(crossX) > absSegHalfZ * boxHalfY + absSegHalfY * boxHalfZ) return false;
    if (std::fabs(crossY) > absSegHalfZ * boxHalfX + absSegHalfX * boxHalfZ) return false;
    if (std::fabs(crossZ) > absSegHalfY * boxHalfX + absSegHalfX * boxHalfY) return false;
    return true;
}

bool LineBoxBoolRef(const VxRay& line, const VxBbox& box) {
    const float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    const float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    const float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;
    const VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    const float dx = line.m_Origin.x - boxCenter.x;
    const float dy = line.m_Origin.y - boxCenter.y;
    const float dz = line.m_Origin.z - boxCenter.z;

    const float crossX = dz * line.m_Direction.y - dy * line.m_Direction.z;
    const float crossY = dx * line.m_Direction.z - dz * line.m_Direction.x;
    const float crossZ = dy * line.m_Direction.x - dx * line.m_Direction.y;
    const float absDirX = std::fabs(line.m_Direction.x);
    const float absDirY = std::fabs(line.m_Direction.y);
    const float absDirZ = std::fabs(line.m_Direction.z);

    if (std::fabs(crossX) > absDirZ * boxHalfY + absDirY * boxHalfZ) return false;
    if (std::fabs(crossY) > absDirX * boxHalfZ + absDirZ * boxHalfX) return false;
    if (std::fabs(crossZ) > absDirX * boxHalfY + absDirY * boxHalfX) return false;
    return true;
}

float OppositeDirSignRef(float dir) {
    return (dir >= 0.0f) ? -1.0f : 1.0f;
}

bool SlabContainsParallelRef(float origin, float minV, float maxV) {
    return origin >= minV && origin <= maxV;
}

int RayBoxDetailedRef(const VxRay& ray, const VxBbox& box, VxVector& inpoint, VxVector* outpoint, VxVector* innormal, VxVector* outnormal) {
    constexpr float kHugeTRef = 1.0e30f;
    float tNear = -kHugeTRef;
    float tFar = kHugeTRef;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float dir = ray.m_Direction[i];
        const float origin = ray.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (std::fabs(dir) < EPSILON) {
            if (!SlabContainsParallelRef(origin, boxMin, boxMax)) {
                return 0;
            }
            continue;
        }

        const float invDir = 1.0f / dir;
        float t1 = (boxMin - origin) * invDir;
        float t2 = (boxMax - origin) * invDir;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSignRef(dir);
        }
        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSignRef(dir);
        }
        if (tNear > tFar || tFar < EPSILON) {
            return 0;
        }
    }

    inpoint = ray.m_Origin + ray.m_Direction * tNear;
    if (outpoint) {
        *outpoint = ray.m_Origin + ray.m_Direction * tFar;
    }
    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }
    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }
    return (tNear <= EPSILON) ? -1 : 1;
}

int SegmentBoxDetailedRef(const VxRay& segment, const VxBbox& box, VxVector& inpoint, VxVector* outpoint, VxVector* innormal, VxVector* outnormal) {
    constexpr float kHugeTRef = 1.0e30f;
    constexpr float kSegmentMaxTRef = 1.0f;
    float tNear = -kHugeTRef;
    float tFar = kHugeTRef;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float dir = segment.m_Direction[i];
        const float origin = segment.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (std::fabs(dir) < EPSILON) {
            if (!SlabContainsParallelRef(origin, boxMin, boxMax)) {
                return 0;
            }
            continue;
        }

        const float invDir = 1.0f / dir;
        float t1 = (boxMin - origin) * invDir;
        float t2 = (boxMax - origin) * invDir;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSignRef(dir);
        }
        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSignRef(dir);
        }
        if (tNear > tFar || tFar < EPSILON) {
            return 0;
        }
    }

    if (tNear > kSegmentMaxTRef) {
        return 0;
    }
    if (tNear < EPSILON && tFar > kSegmentMaxTRef) {
        return 0;
    }

    inpoint = segment.m_Origin + segment.m_Direction * tNear;
    if (outpoint) {
        *outpoint = segment.m_Origin + segment.m_Direction * tFar;
    }
    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }
    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }
    return (tNear <= EPSILON) ? -1 : 1;
}

int LineBoxDetailedRef(const VxRay& line, const VxBbox& box, VxVector& inpoint, VxVector* outpoint, VxVector* innormal, VxVector* outnormal) {
    constexpr float kHugeTRef = 1.0e30f;
    float tNear = -kHugeTRef;
    float tFar = kHugeTRef;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float dir = line.m_Direction[i];
        const float origin = line.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (std::fabs(dir) < EPSILON) {
            if (!SlabContainsParallelRef(origin, boxMin, boxMax)) {
                return 0;
            }
            continue;
        }

        const float invDir = 1.0f / dir;
        float t1 = (boxMin - origin) * invDir;
        float t2 = (boxMax - origin) * invDir;

        if (std::fabs(t2) < std::fabs(t1)) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (std::fabs(t1) < std::fabs(tNear)) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSignRef(dir);
        }
        if (std::fabs(t2) < std::fabs(tFar)) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSignRef(dir);
        }
        if (std::fabs(tFar) < std::fabs(tNear)) {
            return 0;
        }
    }

    inpoint = line.m_Origin + line.m_Direction * tNear;
    if (outpoint) {
        *outpoint = line.m_Origin + line.m_Direction * tFar;
    }
    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }
    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }
    return 1;
}

VxBbox MakeAABBFromCenterExtents(const VxVector& center, const VxVector& extents) {
    return VxBbox(center - extents, center + extents);
}

VxOBB MakeOBBFromCenterExtentsEuler(const VxVector& center, const VxVector& extents, float rx, float ry, float rz) {
    VxMatrix rot;
    Vx3DMatrixFromEulerAngles(rot, rx, ry, rz);

    VxOBB obb;
    obb.m_Center = center;
    obb.m_Axis[0] = VxVector(rot[0].x, rot[0].y, rot[0].z);
    obb.m_Axis[1] = VxVector(rot[1].x, rot[1].y, rot[1].z);
    obb.m_Axis[2] = VxVector(rot[2].x, rot[2].y, rot[2].z);
    obb.m_Axis[0].Normalize();
    obb.m_Axis[1].Normalize();
    obb.m_Axis[2].Normalize();
    obb.m_Extents = extents;
    return obb;
}

} // namespace

// =============================================================================
// Spheres
// =============================================================================

TEST(VxIntersect_Spheres, RaySphere_HitMissInsideTangentAndNonUnitDir) {
    VxSphere sphere(VxVector(0.0f, 0.0f, 0.0f), 2.0f);

    // Direct hit (two intersections)
    {
        VxRay ray = MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector inter1, inter2;
        int hitCount = VxIntersect::RaySphere(ray, sphere, &inter1, &inter2);
        EXPECT_EQ(hitCount, 2);
        EXPECT_TRUE(VectorNearBool(inter1, VxVector(0.0f, 0.0f, -2.0f), BINARY_TOL));
        EXPECT_TRUE(VectorNearBool(inter2, VxVector(0.0f, 0.0f, 2.0f), BINARY_TOL));
    }

    // Miss
    {
        VxRay missRay = MakeRay(VxVector(5.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector inter1 = VxVector(123.0f, 456.0f, 789.0f);
        VxVector inter2 = VxVector(321.0f, 654.0f, 987.0f);
        int hitCount = VxIntersect::RaySphere(missRay, sphere, &inter1, &inter2);
        EXPECT_EQ(hitCount, 0);
        // Ground-truth returns without writing; keep this as a weak non-regression check.
        EXPECT_EQ(inter1, VxVector(123.0f, 456.0f, 789.0f));
        EXPECT_EQ(inter2, VxVector(321.0f, 654.0f, 987.0f));
    }

    // Ray originating inside sphere: inter1 behind origin, inter2 in front.
    {
        VxRay insideRay = MakeRay(VxVector(0.0f, 0.0f, 0.0f), VxVector(1.0f, 0.0f, 0.0f));
        VxVector inter1, inter2;
        int hitCount = VxIntersect::RaySphere(insideRay, sphere, &inter1, &inter2);
        EXPECT_EQ(hitCount, 2);
        EXPECT_TRUE(VectorNearBool(inter1, VxVector(-2.0f, 0.0f, 0.0f), BINARY_TOL * 4.0f));
        EXPECT_TRUE(VectorNearBool(inter2, VxVector(2.0f, 0.0f, 0.0f), BINARY_TOL * 4.0f));
    }

    // Tangent (one intersection)
    {
        VxRay tangentRay = MakeRay(VxVector(2.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector inter1 = VxVector::axis0();
        VxVector inter2 = VxVector::axis0();
        int hitCount = VxIntersect::RaySphere(tangentRay, sphere, &inter1, &inter2);
        EXPECT_EQ(hitCount, 1);
        EXPECT_TRUE(VectorNearBool(inter1, VxVector(2.0f, 0.0f, 0.0f), BINARY_TOL));
    }

    // Non-unit direction should be normalized internally
    {
        VxRay ray = MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 10.0f));
        VxVector inter1, inter2;
        int hitCount = VxIntersect::RaySphere(ray, sphere, &inter1, &inter2);
        EXPECT_EQ(hitCount, 2);
        EXPECT_TRUE(VectorNearBool(inter1, VxVector(0.0f, 0.0f, -2.0f), BINARY_TOL));
        EXPECT_TRUE(VectorNearBool(inter2, VxVector(0.0f, 0.0f, 2.0f), BINARY_TOL));
    }
}

TEST(VxIntersect_Spheres, RaySphere_DegenerateDirectionAndNullOutputs) {
    const VxSphere sphere(VxVector(0.0f, 0.0f, 0.0f), 2.0f);

    // Degenerate direction should not divide by zero or report intersections.
    {
        const VxRay ray = MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 0.0f));
        EXPECT_EQ(VxIntersect::RaySphere(ray, sphere, nullptr, nullptr), 0);
    }

    // Optional outputs may be null.
    {
        const VxRay ray = MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector inter2 = VxVector::axis0();
        EXPECT_EQ(VxIntersect::RaySphere(ray, sphere, nullptr, &inter2), 2);
        EXPECT_TRUE(VectorNearBool(inter2, VxVector(0.0f, 0.0f, 2.0f), BINARY_TOL));
    }
}

TEST(VxIntersect_Spheres, SphereSphere_MovingCollision) {
    // Already intersecting
    {
        VxSphere s1(VxVector(0.0f, 0.0f, 0.0f), 1.0f);
        VxSphere s2(VxVector(1.0f, 0.0f, 0.0f), 1.0f);
        float t1 = -1.0f, t2 = -1.0f;
        XBOOL hit = VxIntersect::SphereSphere(s1, s1.Center(), s2, s2.Center(), &t1, &t2);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
        EXPECT_NEAR(t2, 0.0f, BINARY_TOL);
    }

    // Collision within [0,1]
    {
        VxSphere s1(VxVector(0.0f, 0.0f, 0.0f), 1.0f);
        VxSphere s2(VxVector(5.0f, 0.0f, 0.0f), 1.0f);

        // s2 moves toward s1 over the interval.
        VxVector p1 = s1.Center();
        VxVector p2 = VxVector(0.0f, 0.0f, 0.0f);

        float tEnter = -1.0f, tExit = -1.0f;
        XBOOL hit = VxIntersect::SphereSphere(s1, p1, s2, p2, &tEnter, &tExit);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(tEnter, 0.6f, BINARY_TOL);
        EXPECT_NEAR(tExit, 1.4f, BINARY_TOL);
    }

    // No collision
    {
        VxSphere s1(VxVector(0.0f, 0.0f, 0.0f), 1.0f);
        VxSphere s2(VxVector(5.0f, 0.0f, 0.0f), 1.0f);
        float tEnter = -1.0f, tExit = -1.0f;
        XBOOL hit = VxIntersect::SphereSphere(s1, s1.Center(), s2, VxVector(10.0f, 0.0f, 0.0f), &tEnter, &tExit);
        EXPECT_FALSE(hit);
    }
}

TEST(VxIntersect_Spheres, SphereAABB_BasicCases) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));

    EXPECT_TRUE(VxIntersect::SphereAABB(VxSphere(VxVector(0, 0, 0), 0.5f), box));
    EXPECT_TRUE(VxIntersect::SphereAABB(VxSphere(VxVector(1.2f, 0, 0), 0.5f), box));
    EXPECT_TRUE(VxIntersect::SphereAABB(VxSphere(VxVector(1.2f, 1.2f, 0), 0.5f), box));
    EXPECT_TRUE(VxIntersect::SphereAABB(VxSphere(VxVector(1.2f, 1.2f, 1.2f), 0.5f), box));
    EXPECT_FALSE(VxIntersect::SphereAABB(VxSphere(VxVector(2.0f, 0, 0), 0.5f), box));
}

TEST(VxIntersect_Spheres, SphereAABB_DoesNotUsePlaneApproximation) {
    // The sphere is near the box corner direction but still too far in Euclidean distance.
    const VxBbox box(
        VxVector(4.38262268f, -3.76811886f, -4.92815447f),
        VxVector(5.86545181f, -3.65976524f, -2.50481009f));
    const VxSphere sphere(
        VxVector(5.74680996f, -5.00813293f, -6.20174360f),
        1.72290361f);

    EXPECT_FALSE(VxIntersect::SphereAABB(sphere, box));
    EXPECT_FALSE(SphereAABBRef(sphere, box));
}

TEST(VxIntersect_SIMDConsistency, SphereAABB_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> boxBaseDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> boxExtDist(0.1f, 6.0f);
    std::uniform_real_distribution<float> centerDist(-14.0f, 14.0f);
    std::uniform_real_distribution<float> radiusDist(0.01f, 8.0f);

    for (int i = 0; i < 600; ++i) {
        const VxVector minPt(boxBaseDist(rng), boxBaseDist(rng), boxBaseDist(rng));
        const VxVector ext(boxExtDist(rng), boxExtDist(rng), boxExtDist(rng));
        const VxBbox box(minPt, minPt + ext);

        const VxSphere sphere(
            VxVector(centerDist(rng), centerDist(rng), centerDist(rng)),
            radiusDist(rng));

        const bool actual = VxIntersect::SphereAABB(sphere, box) != FALSE;
        const bool ref = SphereAABBRef(sphere, box);
        EXPECT_EQ(actual, ref);
    }
}

// =============================================================================
// Boxes
// =============================================================================

TEST(VxIntersect_Boxes, RayBox_BooleanAndDetailedVariants) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));

    // Hit
    {
        VxRay ray = MakeRay(VxVector(-5, 0, 0), VxVector(1, 0, 0));
        EXPECT_TRUE(VxIntersect::RayBox(ray, box));

        VxVector inPoint, outPoint, inNormal, outNormal;
        int rc = VxIntersect::RayBox(ray, box, inPoint, &outPoint, &inNormal, &outNormal);
        EXPECT_EQ(rc, 1);
        EXPECT_VEC3_NEAR(inPoint, VxVector(-1, 0, 0), BINARY_TOL);
        EXPECT_VEC3_NEAR(outPoint, VxVector(1, 0, 0), BINARY_TOL);
        EXPECT_EQ(inNormal, VxVector(-1, 0, 0));
        // Ground-truth stores the normal opposite to the direction sign for both entry/exit.
        EXPECT_EQ(outNormal, VxVector(-1, 0, 0));
    }

    // Miss
    {
        VxRay ray = MakeRay(VxVector(-5, 5, 0), VxVector(1, 0, 0));
        EXPECT_FALSE(VxIntersect::RayBox(ray, box));
        VxVector inPoint;
        EXPECT_EQ(VxIntersect::RayBox(ray, box, inPoint, nullptr, nullptr, nullptr), 0);
    }

    // Origin inside => detailed variant returns -1
    {
        VxRay ray = MakeRay(VxVector(0, 0, 0), VxVector(1, 0, 0));
        EXPECT_TRUE(VxIntersect::RayBox(ray, box));
        VxVector inPoint, outPoint;
        int rc = VxIntersect::RayBox(ray, box, inPoint, &outPoint, nullptr, nullptr);
        EXPECT_EQ(rc, -1);
        // Implementation computes inPoint at tNear (negative), outPoint at tFar (positive).
        EXPECT_VEC3_NEAR(outPoint, VxVector(1, 0, 0), BINARY_TOL);
    }

    // Parallel-to-slab behavior: direction ~0 on an axis uses [min, max) containment.
    {
        // X is parallel (0), but origin.x == Max.x should be treated as outside.
        VxRay ray = MakeRay(VxVector(1.0f, 0.0f, 0.0f), VxVector(0.0f, 1.0f, 0.0f));
        VxVector inPoint;
        // Ground-truth treats boundary points as inside for the detailed variant.
        EXPECT_EQ(VxIntersect::RayBox(ray, box, inPoint, nullptr, nullptr, nullptr), -1);
    }
}

TEST(VxIntersect_Boxes, SegmentBox_BooleanAndDetailedVariants) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));

    // Segment crosses box (t within [0,1])
    {
        VxRay seg = MakeSegment(VxVector(-2, 0, 0), VxVector(2, 0, 0));
        EXPECT_TRUE(VxIntersect::SegmentBox(seg, box));

        VxVector inPoint, outPoint, inNormal, outNormal;
        int rc = VxIntersect::SegmentBox(seg, box, inPoint, &outPoint, &inNormal, &outNormal);
        EXPECT_EQ(rc, 1);
        EXPECT_VEC3_NEAR(inPoint, VxVector(-1, 0, 0), BINARY_TOL);
        EXPECT_VEC3_NEAR(outPoint, VxVector(1, 0, 0), BINARY_TOL);
        EXPECT_EQ(inNormal, VxVector(-1, 0, 0));
        EXPECT_EQ(outNormal, VxVector(-1, 0, 0));
    }

    // Segment entirely outside
    {
        VxRay seg = MakeSegment(VxVector(2, 2, 2), VxVector(3, 2, 2));
        EXPECT_FALSE(VxIntersect::SegmentBox(seg, box));
        VxVector inPoint;
        EXPECT_EQ(VxIntersect::SegmentBox(seg, box, inPoint, nullptr, nullptr, nullptr), 0);
    }

    // Segment fully contains the box but starts inside with no valid entry within [0,1] => 0 per binary rules.
    {
        VxRay seg = MakeSegment(VxVector(0, 0, 0), VxVector(100, 0, 0));
        VxVector inPoint;
        // Ground-truth returns -1 when the segment origin is inside the box.
        EXPECT_EQ(VxIntersect::SegmentBox(seg, box, inPoint, nullptr, nullptr, nullptr), -1);
    }
}

TEST(VxIntersect_Boxes, LineBox_BooleanAndDetailedVariants) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));

    // Infinite line through box
    {
        VxRay line = MakeRay(VxVector(-5, 0, 0), VxVector(1, 0, 0));
        EXPECT_TRUE(VxIntersect::LineBox(line, box));

        VxVector inPoint, outPoint, inNormal, outNormal;
        int rc = VxIntersect::LineBox(line, box, inPoint, &outPoint, &inNormal, &outNormal);
        EXPECT_EQ(rc, 1);
        EXPECT_VEC3_NEAR(inPoint, VxVector(-1, 0, 0), BINARY_TOL);
        EXPECT_VEC3_NEAR(outPoint, VxVector(1, 0, 0), BINARY_TOL);
        EXPECT_EQ(inNormal, VxVector(-1, 0, 0));
        EXPECT_EQ(outNormal, VxVector(-1, 0, 0));
    }

    // Parallel line outside slabs
    {
        VxRay line = MakeRay(VxVector(2, 0, 0), VxVector(0, 1, 0));
        EXPECT_FALSE(VxIntersect::LineBox(line, box));
        VxVector inPoint;
        EXPECT_EQ(VxIntersect::LineBox(line, box, inPoint, nullptr, nullptr, nullptr), 0);
    }
}

TEST(VxIntersect_Boxes, AABBAABB_AABBOBB_OBBOBB) {
    VxBbox aabb(VxVector(-1, -1, -1), VxVector(1, 1, 1));

    // AABBAABB
    EXPECT_TRUE(VxIntersect::AABBAABB(aabb, VxBbox(VxVector(1, 0, 0), VxVector(3, 2, 2))));  // touching
    EXPECT_FALSE(VxIntersect::AABBAABB(aabb, VxBbox(VxVector(2.1f, 0, 0), VxVector(3, 2, 2))));

    // AABBOBB
    {
        VxMatrix id;
        Vx3DMatrixIdentity(id);
        VxOBB obb(aabb, id);
        EXPECT_TRUE(VxIntersect::AABBOBB(aabb, obb));

        VxMatrix trans;
        Vx3DMatrixIdentity(trans);
        trans[3][0] = 100.0f;
        VxOBB farObb(aabb, trans);
        EXPECT_FALSE(VxIntersect::AABBOBB(aabb, farObb));

        VxMatrix rot;
        Vx3DMatrixFromEulerAngles(rot, 0.0f, 0.0f, PI / 4.0f);
        VxOBB rotObb(aabb, rot);
        EXPECT_TRUE(VxIntersect::AABBOBB(aabb, rotObb));
    }

    // OBBOBB
    {
        VxMatrix id;
        Vx3DMatrixIdentity(id);
        VxOBB obb0(aabb, id);

        VxMatrix trans;
        Vx3DMatrixIdentity(trans);
        trans[3][0] = 1.5f;
        VxOBB obb1(aabb, trans);
        EXPECT_TRUE(VxIntersect::OBBOBB(obb0, obb1));

        trans[3][0] = 3.0f;
        VxOBB obb2(aabb, trans);
        EXPECT_FALSE(VxIntersect::OBBOBB(obb0, obb2));
    }
}

TEST(VxIntersect_SIMDConsistency, BoxSAT_MatchesScalarReference) {
    std::mt19937 rng(0xB0A5A7);
    std::uniform_real_distribution<float> centerDist(-20.0f, 20.0f);
    std::uniform_real_distribution<float> extentDist(0.1f, 6.0f);
    std::uniform_real_distribution<float> angleDist(-PI, PI);

    for (int i = 0; i < 200; ++i) {
        const VxVector cA(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector eA(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxBbox aabbA = MakeAABBFromCenterExtents(cA, eA);

        const VxVector cB(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector eB(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxBbox aabbB = MakeAABBFromCenterExtents(cB, eB);

        const VxVector cO1(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector eO1(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxOBB obb1 = MakeOBBFromCenterExtentsEuler(cO1, eO1, angleDist(rng), angleDist(rng), angleDist(rng));

        const VxVector cO2(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector eO2(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxOBB obb2 = MakeOBBFromCenterExtentsEuler(cO2, eO2, angleDist(rng), angleDist(rng), angleDist(rng));

        EXPECT_EQ(VxIntersect::AABBAABB(aabbA, aabbB), AABBAABBRef(aabbA, aabbB));
        EXPECT_EQ(VxIntersect::AABBOBB(aabbA, obb2), AABBOBBRef(aabbA, obb2));
        EXPECT_EQ(VxIntersect::OBBOBB(obb1, obb2), OBBOBBRef(obb1, obb2));
        EXPECT_EQ(VxIntersect::OBBOBB(obb1, obb2), VxIntersect::OBBOBB(obb2, obb1));
    }
}

TEST(VxIntersect_SIMDConsistency, BoxLineRaySegmentBool_MatchesScalarReference) {
    std::mt19937 rng(0x51A7B0);
    std::uniform_real_distribution<float> centerDist(-20.0f, 20.0f);
    std::uniform_real_distribution<float> extentDist(0.1f, 8.0f);
    std::uniform_real_distribution<float> valueDist(-30.0f, 30.0f);

    for (int i = 0; i < 250; ++i) {
        const VxVector boxCenter(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector boxExtents(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxBbox box = MakeAABBFromCenterExtents(boxCenter, boxExtents);

        const VxVector origin(valueDist(rng), valueDist(rng), valueDist(rng));
        const VxVector dir(valueDist(rng), valueDist(rng), valueDist(rng));
        const VxRay ray = MakeRay(origin, dir);
        const VxRay segment(origin, origin + dir);
        const VxRay line = MakeRay(origin, dir);

        EXPECT_EQ(VxIntersect::RayBox(ray, box) != FALSE, RayBoxBoolRef(ray, box));
        EXPECT_EQ(VxIntersect::SegmentBox(segment, box) != FALSE, SegmentBoxBoolRef(segment, box));
        EXPECT_EQ(VxIntersect::LineBox(line, box) != FALSE, LineBoxBoolRef(line, box));
    }
}

TEST(VxIntersect_SIMDConsistency, BoxLineRaySegmentDetailed_MatchesScalarReference) {
    std::mt19937 rng(0xD37A11);
    std::uniform_real_distribution<float> centerDist(-20.0f, 20.0f);
    std::uniform_real_distribution<float> extentDist(0.1f, 8.0f);
    std::uniform_real_distribution<float> valueDist(-30.0f, 30.0f);

    for (int i = 0; i < 220; ++i) {
        const VxVector boxCenter(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector boxExtents(extentDist(rng), extentDist(rng), extentDist(rng));
        const VxBbox box = MakeAABBFromCenterExtents(boxCenter, boxExtents);

        VxVector dir(valueDist(rng), valueDist(rng), valueDist(rng));
        if ((i % 10) == 0) dir.x = 0.0f;
        if ((i % 15) == 0) dir.y = 0.0f;
        if ((i % 21) == 0) dir.z = 0.0f;
        const VxVector origin(valueDist(rng), valueDist(rng), valueDist(rng));

        const VxRay ray = MakeRay(origin, dir);
        const VxRay segment(origin, origin + dir);
        const VxRay line = MakeRay(origin, dir);

        VxVector actIn, actOut, actInN, actOutN;
        VxVector refIn, refOut, refInN, refOutN;
        int actCode = VxIntersect::RayBox(ray, box, actIn, &actOut, &actInN, &actOutN);
        int refCode = RayBoxDetailedRef(ray, box, refIn, &refOut, &refInN, &refOutN);
        EXPECT_EQ(actCode, refCode);
        if (actCode != 0) {
            EXPECT_TRUE(VectorNearBool(actIn, refIn, BINARY_TOL));
            EXPECT_TRUE(VectorNearBool(actOut, refOut, BINARY_TOL));
            EXPECT_EQ(actInN, refInN);
            EXPECT_EQ(actOutN, refOutN);
        }

        actCode = VxIntersect::SegmentBox(segment, box, actIn, &actOut, &actInN, &actOutN);
        refCode = SegmentBoxDetailedRef(segment, box, refIn, &refOut, &refInN, &refOutN);
        EXPECT_EQ(actCode, refCode);
        if (actCode != 0) {
            EXPECT_TRUE(VectorNearBool(actIn, refIn, BINARY_TOL));
            EXPECT_TRUE(VectorNearBool(actOut, refOut, BINARY_TOL));
            EXPECT_EQ(actInN, refInN);
            EXPECT_EQ(actOutN, refOutN);
        }

        actCode = VxIntersect::LineBox(line, box, actIn, &actOut, &actInN, &actOutN);
        refCode = LineBoxDetailedRef(line, box, refIn, &refOut, &refInN, &refOutN);
        EXPECT_EQ(actCode, refCode);
        if (actCode != 0) {
            EXPECT_TRUE(VectorNearBool(actIn, refIn, BINARY_TOL));
            EXPECT_TRUE(VectorNearBool(actOut, refOut, BINARY_TOL));
            EXPECT_EQ(actInN, refInN);
            EXPECT_EQ(actOutN, refOutN);
        }
    }
}

TEST(VxIntersect_Boxes, AABBFace_TriangleIntersection) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));
    VxVector N(0, 0, 1);

    // Triangle vertex inside the box
    EXPECT_TRUE(VxIntersect::AABBFace(
        box,
        VxVector(0.0f, 0.0f, 0.0f),
        VxVector(0.5f, 0.0f, 0.0f),
        VxVector(0.0f, 0.5f, 0.0f),
        N));

    // Triangle outside but edge crosses the box
    EXPECT_TRUE(VxIntersect::AABBFace(
        box,
        VxVector(-2.0f, 0.0f, 0.0f),
        VxVector(2.0f, 0.0f, 0.0f),
        VxVector(0.0f, 2.0f, 0.0f),
        N));

    // Large triangle enclosing the box in projection (diagonal test path)
    EXPECT_TRUE(VxIntersect::AABBFace(
        box,
        VxVector(-10.0f, -10.0f, 0.0f),
        VxVector(10.0f, -10.0f, 0.0f),
        VxVector(0.0f, 10.0f, 0.0f),
        N));

    // Completely separated
    EXPECT_FALSE(VxIntersect::AABBFace(
        box,
        VxVector(10.0f, 10.0f, 10.0f),
        VxVector(12.0f, 10.0f, 10.0f),
        VxVector(10.0f, 12.0f, 10.0f),
        N));
}

TEST(VxIntersect_SIMDConsistency, AABBFace_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> boxBaseDist(-8.0f, 8.0f);
    std::uniform_real_distribution<float> boxExtDist(0.1f, 5.0f);
    std::uniform_real_distribution<float> triCenterDist(-12.0f, 12.0f);
    std::uniform_real_distribution<float> triOffsetDist(-4.0f, 4.0f);

    int generated = 0;
    int attempts = 0;
    while (generated < 500 && attempts < 6000) {
        ++attempts;

        const VxVector minPt(boxBaseDist(rng), boxBaseDist(rng), boxBaseDist(rng));
        const VxVector ext(boxExtDist(rng), boxExtDist(rng), boxExtDist(rng));
        const VxBbox box(minPt, minPt + ext);

        const VxVector c(triCenterDist(rng), triCenterDist(rng), triCenterDist(rng));
        const VxVector a0 = c + VxVector(triOffsetDist(rng), triOffsetDist(rng), triOffsetDist(rng));
        const VxVector a1 = c + VxVector(triOffsetDist(rng), triOffsetDist(rng), triOffsetDist(rng));
        const VxVector a2 = c + VxVector(triOffsetDist(rng), triOffsetDist(rng), triOffsetDist(rng));

        VxVector n = CrossProduct(a1 - a0, a2 - a0);
        const float n2 = SquareMagnitude(n);
        if (n2 <= 1.0e-6f) {
            continue;
        }
        n *= (1.0f / sqrtf(n2));

        const bool actual = VxIntersect::AABBFace(box, a0, a1, a2, n) != FALSE;
        const bool ref = AABBFaceRef(box, a0, a1, a2, n);
        EXPECT_EQ(actual, ref);

        ++generated;
    }

    EXPECT_EQ(generated, 500);
}

// =============================================================================
// Planes
// =============================================================================

TEST(VxIntersect_Planes, RayPlaneAndCulled) {
    VxPlane plane(VxVector(0.0f, 0.0f, 1.0f), VxVector(0.0f, 0.0f, 0.0f));

    // Intersects (front-facing with normal)
    {
        VxRay ray = MakeRay(VxVector(0.0f, 0.0f, 5.0f), VxVector(0.0f, 0.0f, -1.0f));
        VxVector pt;
        float dist = 0.0f;
        EXPECT_TRUE(VxIntersect::RayPlane(ray, plane, pt, dist));
        EXPECT_TRUE(VxIntersect::RayPlaneCulled(ray, plane, pt, dist));
        EXPECT_VEC3_NEAR(pt, VxVector(0.0f, 0.0f, 0.0f), BINARY_TOL);
        EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
    }

    // Back-facing (culled)
    {
        VxRay ray = MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector pt;
        float dist = 0.0f;
        EXPECT_TRUE(VxIntersect::RayPlane(ray, plane, pt, dist));
        EXPECT_FALSE(VxIntersect::RayPlaneCulled(ray, plane, pt, dist));
    }

    // Parallel
    {
        VxRay ray = MakeRay(VxVector(0.0f, 0.0f, 1.0f), VxVector(1.0f, 0.0f, 0.0f));
        VxVector pt;
        float dist = 0.0f;
        EXPECT_FALSE(VxIntersect::RayPlane(ray, plane, pt, dist));
        EXPECT_FALSE(VxIntersect::RayPlaneCulled(ray, plane, pt, dist));
    }
}

TEST(VxIntersect_Planes, SegmentPlane_LinePlane) {
    VxPlane plane(VxVector(0.0f, 0.0f, 1.0f), VxVector(0.0f, 0.0f, 0.0f));

    // Segment intersects within [0,1]
    {
        VxRay seg = MakeSegment(VxVector(0.0f, 0.0f, -1.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector pt;
        float t = 0.0f;
        EXPECT_TRUE(VxIntersect::SegmentPlane(seg, plane, pt, t));
        EXPECT_VEC3_NEAR(pt, VxVector(0.0f, 0.0f, 0.0f), BINARY_TOL);
        EXPECT_NEAR(t, 0.5f, BINARY_TOL);
    }

    // Segment intersection occurs outside [0,1]
    {
        VxRay seg = MakeSegment(VxVector(0.0f, 0.0f, 2.0f), VxVector(0.0f, 0.0f, 3.0f));
        VxVector pt;
        float t = 0.0f;
        EXPECT_FALSE(VxIntersect::SegmentPlane(seg, plane, pt, t));
    }

    // Segment culled
    {
        VxRay seg = MakeSegment(VxVector(0.0f, 0.0f, -1.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector pt;
        float t = 0.0f;
        EXPECT_FALSE(VxIntersect::SegmentPlaneCulled(seg, plane, pt, t));
    }

    // Line always intersects when not parallel
    {
        VxRay line = MakeRay(VxVector(0.0f, 0.0f, 5.0f), VxVector(0.0f, 0.0f, 1.0f));
        VxVector pt;
        float t = 0.0f;
        EXPECT_TRUE(VxIntersect::LinePlane(line, plane, pt, t));
        EXPECT_VEC3_NEAR(pt, VxVector(0.0f, 0.0f, 0.0f), BINARY_TOL);
        EXPECT_NEAR(t, -5.0f, BINARY_TOL);
    }
}

TEST(VxIntersect_Planes, BoxPlane_AndTransformedBoxPlane) {
    VxBbox box(VxVector(-1, -1, -1), VxVector(1, 1, 1));
    VxPlane planeX0(VxVector(1, 0, 0), VxVector(0, 0, 0));

    EXPECT_TRUE(VxIntersect::BoxPlane(box, planeX0));
    EXPECT_FALSE(VxIntersect::BoxPlane(VxBbox(VxVector(2, -1, -1), VxVector(3, 1, 1)), planeX0));
    EXPECT_TRUE(VxIntersect::BoxPlane(VxBbox(VxVector(0, -1, -1), VxVector(1, 1, 1)), planeX0)); // touching

    VxMatrix id;
    Vx3DMatrixIdentity(id);
    EXPECT_TRUE(VxIntersect::BoxPlane(box, id, planeX0));

    VxMatrix trans;
    Vx3DMatrixIdentity(trans);
    trans[3][0] = 10.0f;
    EXPECT_FALSE(VxIntersect::BoxPlane(box, trans, planeX0));
}

TEST(VxIntersect_Planes, Planes_ThreePlaneIntersection) {
    VxPlane px(VxVector(1, 0, 0), VxVector(1, 0, 0));
    VxPlane py(VxVector(0, 1, 0), VxVector(0, 2, 0));
    VxPlane pz(VxVector(0, 0, 1), VxVector(0, 0, 3));

    VxVector p;
    EXPECT_TRUE(VxIntersect::Planes(px, py, pz, p));
    EXPECT_VEC3_NEAR(p, VxVector(1, 2, 3), BINARY_TOL);

    // Degenerate (parallel planes) => determinant 0
    VxPlane px2(VxVector(1, 0, 0), VxVector(2, 0, 0));
    EXPECT_FALSE(VxIntersect::Planes(px, px2, pz, p));
}

TEST(VxIntersect_SIMDConsistency, BoxPlane_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> baseDist(-12.0f, 12.0f);
    std::uniform_real_distribution<float> sizeDist(0.05f, 6.0f);
    std::uniform_real_distribution<float> nDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> tDist(-8.0f, 8.0f);

    int generated = 0;
    int attempts = 0;
    while (generated < 400 && attempts < 4000) {
        ++attempts;

        const VxVector minPt(baseDist(rng), baseDist(rng), baseDist(rng));
        const VxVector ext(sizeDist(rng), sizeDist(rng), sizeDist(rng));
        const VxBbox box(minPt, minPt + ext);

        VxVector n(nDist(rng), nDist(rng), nDist(rng));
        const float n2 = SquareMagnitude(n);
        if (n2 <= 1.0e-6f) {
            continue;
        }
        n *= (1.0f / sqrtf(n2));

        const VxVector planePoint(baseDist(rng), baseDist(rng), baseDist(rng));
        const VxPlane plane(n, planePoint);

        const bool actual = VxIntersect::BoxPlane(box, plane) != FALSE;
        const bool ref = BoxPlaneRef(box, plane);
        EXPECT_EQ(actual, ref);

        VxMatrix world;
        Vx3DMatrixFromEulerAngles(world, nDist(rng), nDist(rng), nDist(rng));
        world[3][0] = tDist(rng);
        world[3][1] = tDist(rng);
        world[3][2] = tDist(rng);

        const bool actualWorld = VxIntersect::BoxPlane(box, world, plane) != FALSE;
        const bool refWorld = BoxPlaneRef(box, world, plane);
        EXPECT_EQ(actualWorld, refWorld);

        ++generated;
    }

    EXPECT_EQ(generated, 400);
}

TEST(VxIntersect_SIMDConsistency, Planes_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> nDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> pDist(-10.0f, 10.0f);

    int generated = 0;
    int attempts = 0;
    while (generated < 300 && attempts < 5000) {
        ++attempts;

        VxVector n1(nDist(rng), nDist(rng), nDist(rng));
        VxVector n2(nDist(rng), nDist(rng), nDist(rng));
        VxVector n3(nDist(rng), nDist(rng), nDist(rng));

        if (SquareMagnitude(n1) <= 1.0e-6f || SquareMagnitude(n2) <= 1.0e-6f || SquareMagnitude(n3) <= 1.0e-6f) {
            continue;
        }

        n1 *= (1.0f / sqrtf(SquareMagnitude(n1)));
        n2 *= (1.0f / sqrtf(SquareMagnitude(n2)));
        n3 *= (1.0f / sqrtf(SquareMagnitude(n3)));

        const float det = DotProduct(n1, CrossProduct(n2, n3));
        if (std::fabs(det) <= 1.0e-2f) {
            continue;
        }

        const VxVector samplePoint(pDist(rng), pDist(rng), pDist(rng));
        const VxPlane p1(n1, samplePoint);
        const VxPlane p2(n2, samplePoint);
        const VxPlane p3(n3, samplePoint);

        VxVector actualPoint = VxVector::axis0();
        VxVector refPoint = VxVector::axis0();
        const bool actual = VxIntersect::Planes(p1, p2, p3, actualPoint) != FALSE;
        const bool ref = PlanesRef(p1, p2, p3, refPoint);
        EXPECT_EQ(actual, ref);
        if (actual && ref) {
            EXPECT_TRUE(VectorNearBool(actualPoint, refPoint, BINARY_TOL * 16.0f));
        }

        ++generated;
    }

    EXPECT_EQ(generated, 300);

    // Degenerate case: first two planes are parallel.
    {
        const VxPlane p1(VxVector(1, 0, 0), VxVector(1, 0, 0));
        const VxPlane p2(VxVector(2, 0, 0), VxVector(2, 0, 0));
        const VxPlane p3(VxVector(0, 0, 1), VxVector(0, 0, 3));
        VxVector actualPoint = VxVector::axis0();
        VxVector refPoint = VxVector::axis0();
        EXPECT_EQ(VxIntersect::Planes(p1, p2, p3, actualPoint) != FALSE, PlanesRef(p1, p2, p3, refPoint));
    }
}

TEST(VxIntersect_SIMDConsistency, PlaneIntersections_MatchScalarReference) {
    struct PlaneCase {
        VxRay ray;
        VxPlane plane;
    };

    const PlaneCase cases[] = {
        {MakeRay(VxVector(0.0f, 0.0f, 5.0f), VxVector(0.0f, 0.0f, -1.0f)), VxPlane(VxVector(0.0f, 0.0f, 1.0f), VxVector(0.0f, 0.0f, 0.0f))},
        {MakeRay(VxVector(1.5f, -2.0f, 3.0f), VxVector(-0.5f, 1.25f, -2.0f)), VxPlane(VxVector(0.4f, -0.7f, 0.2f), VxVector(0.0f, 1.0f, -0.5f))},
        {MakeSegment(VxVector(0.0f, 0.0f, -2.0f), VxVector(0.0f, 0.0f, 3.0f)), VxPlane(VxVector(0.0f, 0.0f, 1.0f), VxVector(0.0f, 0.0f, 0.0f))},
        {MakeRay(VxVector(10.0f, -5.0f, 1.0e-4f), VxVector(1.0e-3f, 3.0e-3f, -2.0e-3f)), VxPlane(VxVector(0.0f, 1.0f, 0.0f), VxVector(0.0f, -1.0f, 0.0f))},
    };

    for (const auto& c : cases) {
        const struct {
            PlaneMode mode;
            XBOOL (*fn)(const VxRay&, const VxPlane&, VxVector&, float&);
        } modes[] = {
            {PlaneMode::Ray, VxIntersect::RayPlane},
            {PlaneMode::RayCulled, VxIntersect::RayPlaneCulled},
            {PlaneMode::Segment, VxIntersect::SegmentPlane},
            {PlaneMode::SegmentCulled, VxIntersect::SegmentPlaneCulled},
            {PlaneMode::Line, VxIntersect::LinePlane},
        };

        for (const auto& m : modes) {
            VxVector actualPoint;
            VxVector refPoint;
            float actualDist = 0.0f;
            float refDist = 0.0f;
            const bool actualHit = m.fn(c.ray, c.plane, actualPoint, actualDist) != FALSE;
            const bool refHit = IntersectPlaneRef(c.ray, c.plane, m.mode, refPoint, refDist);
            EXPECT_EQ(actualHit, refHit);
            if (actualHit && refHit) {
                EXPECT_TRUE(VectorNearBool(actualPoint, refPoint, BINARY_TOL));
                EXPECT_TRUE(ScaleRelativeNear(actualDist, refDist, BINARY_TOL));
            }
        }
    }
}

TEST(VxIntersect_SIMDConsistency, SphereIntersections_MatchScalarReference) {
    struct RaySphereCase {
        VxRay ray;
        VxSphere sphere;
    };

    const RaySphereCase rayCases[] = {
        {MakeRay(VxVector(0.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f)), VxSphere(VxVector(0.0f, 0.0f, 0.0f), 2.0f)},
        {MakeRay(VxVector(2.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f)), VxSphere(VxVector(0.0f, 0.0f, 0.0f), 2.0f)},
        {MakeRay(VxVector(3.0f, 0.0f, -5.0f), VxVector(0.0f, 0.0f, 1.0f)), VxSphere(VxVector(0.0f, 0.0f, 0.0f), 2.0f)},
        {MakeRay(VxVector(1.0e6f, -1.0e6f, -3.0f), VxVector(0.0f, 0.0f, 5.0f)), VxSphere(VxVector(1.0e6f, -1.0e6f, 0.0f), 1.5f)},
    };

    for (const auto& c : rayCases) {
        VxVector actual1 = VxVector::axis0();
        VxVector actual2 = VxVector::axis0();
        VxVector ref1 = VxVector::axis0();
        VxVector ref2 = VxVector::axis0();
        const int actualCount = VxIntersect::RaySphere(c.ray, c.sphere, &actual1, &actual2);
        const int refCount = RaySphereRef(c.ray, c.sphere, &ref1, &ref2);
        EXPECT_EQ(actualCount, refCount);
        if (actualCount >= 1) {
            EXPECT_TRUE(VectorNearBool(actual1, ref1, BINARY_TOL * 4.0f));
        }
        if (actualCount >= 2) {
            EXPECT_TRUE(VectorNearBool(actual2, ref2, BINARY_TOL * 4.0f));
        }
    }

    struct SweepCase {
        VxSphere s1;
        VxVector p1;
        VxSphere s2;
        VxVector p2;
    };

    const SweepCase sweepCases[] = {
        {VxSphere(VxVector(0.0f, 0.0f, 0.0f), 1.0f), VxVector(0.0f, 0.0f, 0.0f), VxSphere(VxVector(1.0f, 0.0f, 0.0f), 1.0f), VxVector(1.0f, 0.0f, 0.0f)},
        {VxSphere(VxVector(0.0f, 0.0f, 0.0f), 1.0f), VxVector(0.0f, 0.0f, 0.0f), VxSphere(VxVector(5.0f, 0.0f, 0.0f), 1.0f), VxVector(0.0f, 0.0f, 0.0f)},
        {VxSphere(VxVector(0.0f, 0.0f, 0.0f), 1.0f), VxVector(0.0f, 0.0f, 0.0f), VxSphere(VxVector(5.0f, 0.0f, 0.0f), 1.0f), VxVector(10.0f, 0.0f, 0.0f)},
        {VxSphere(VxVector(1000.0f, -2000.0f, 3000.0f), 5.0f), VxVector(1100.0f, -2000.0f, 3000.0f), VxSphere(VxVector(1120.0f, -2000.0f, 3000.0f), 7.0f), VxVector(1000.0f, -2000.0f, 3000.0f)},
    };

    for (const auto& c : sweepCases) {
        float actualT1 = 0.0f;
        float actualT2 = 0.0f;
        float refT1 = 0.0f;
        float refT2 = 0.0f;
        const bool actualHit = VxIntersect::SphereSphere(c.s1, c.p1, c.s2, c.p2, &actualT1, &actualT2) != FALSE;
        const bool refHit = SphereSphereRef(c.s1, c.p1, c.s2, c.p2, &refT1, &refT2);
        EXPECT_EQ(actualHit, refHit);
        if (actualHit && refHit) {
            EXPECT_TRUE(ScaleRelativeNear(actualT1, refT1, BINARY_TOL));
            EXPECT_TRUE(ScaleRelativeNear(actualT2, refT2, BINARY_TOL));
        }
    }
}

// =============================================================================
// Faces (triangles)
// =============================================================================

TEST(VxIntersect_Faces, PointInFace_AndDominantAxisSelection) {
    VxVector p0(0, 0, 0), p1(1, 0, 0), p2(0, 1, 0);
    VxVector n(0, 0, 1);
    int i1 = -1, i2 = -1;

    EXPECT_TRUE(VxIntersect::PointInFace(VxVector(0.25f, 0.25f, 0.0f), p0, p1, p2, n, i1, i2));
    EXPECT_FALSE(VxIntersect::PointInFace(VxVector(1.25f, 0.25f, 0.0f), p0, p1, p2, n, i1, i2));

    // For normal (0,0,1), projection uses X/Y.
    EXPECT_EQ(i1, 0);
    EXPECT_EQ(i2, 1);
}

TEST(VxIntersect_SIMDConsistency, PointInFace_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> centerDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> offsetDist(-4.0f, 4.0f);
    std::uniform_real_distribution<float> baryDist(-0.3f, 1.3f);

    int generated = 0;
    int attempts = 0;
    while (generated < 500 && attempts < 8000) {
        ++attempts;

        const VxVector center(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector p0 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        const VxVector p1 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        const VxVector p2 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));

        VxVector n = CrossProduct(p1 - p0, p2 - p0);
        const float n2 = SquareMagnitude(n);
        if (n2 <= 1.0e-6f) {
            continue;
        }
        n *= (1.0f / sqrtf(n2));

        const float u = baryDist(rng);
        const float v = baryDist(rng);
        const VxVector point = p0 + (p1 - p0) * u + (p2 - p0) * v;

        int actI1 = -1;
        int actI2 = -1;
        int refI1 = -1;
        int refI2 = -1;
        const bool actual = VxIntersect::PointInFace(point, p0, p1, p2, n, actI1, actI2) != FALSE;
        const bool ref = PointInFaceRef(point, p0, p1, p2, n, refI1, refI2);
        EXPECT_EQ(actual, ref);
        EXPECT_EQ(actI1, refI1);
        EXPECT_EQ(actI2, refI2);

        ++generated;
    }

    EXPECT_EQ(generated, 500);
}

TEST(VxIntersect_Faces, RayFace_SegmentFace_LineFace_AndCulledVariants) {
    VxVector p0(0, 0, 0), p1(5, 0, 0), p2(0, 5, 0);
    VxVector n(0, 0, 1);
    VxVector hit;
    float dist = 0.0f;
    int i1 = -1, i2 = -1;

    // Ray hit
    {
        VxRay ray = MakeRay(VxVector(1, 1, 5), VxVector(0, 0, -1));
        EXPECT_TRUE(VxIntersect::RayFace(ray, p0, p1, p2, n, hit, dist));
        EXPECT_VEC3_NEAR(hit, VxVector(1, 1, 0), BINARY_TOL);
        EXPECT_TRUE(VxIntersect::RayFace(ray, p0, p1, p2, n, hit, dist, i1, i2));
        EXPECT_EQ(i1, 0);
        EXPECT_EQ(i2, 1);
    }

    // Ray miss
    {
        VxRay ray = MakeRay(VxVector(6, 6, 5), VxVector(0, 0, -1));
        EXPECT_FALSE(VxIntersect::RayFace(ray, p0, p1, p2, n, hit, dist));
    }

    // RayFaceCulled should reject back-facing
    {
        VxRay rayBack = MakeRay(VxVector(1, 1, -5), VxVector(0, 0, 1));
        EXPECT_TRUE(VxIntersect::RayFace(rayBack, p0, p1, p2, n, hit, dist));
        EXPECT_FALSE(VxIntersect::RayFaceCulled(rayBack, p0, p1, p2, n, hit, dist, i1, i2));
    }

    // Segment hit
    {
        VxRay seg = MakeSegment(VxVector(1, 1, 1), VxVector(1, 1, -1));
        EXPECT_TRUE(VxIntersect::SegmentFace(seg, p0, p1, p2, n, hit, dist));
        EXPECT_VEC3_NEAR(hit, VxVector(1, 1, 0), BINARY_TOL);
        EXPECT_TRUE(VxIntersect::SegmentFace(seg, p0, p1, p2, n, hit, dist, i1, i2));
    }

    // Segment culled (approaching from back)
    {
        VxRay seg = MakeSegment(VxVector(1, 1, -1), VxVector(1, 1, 1));
        EXPECT_TRUE(VxIntersect::SegmentFace(seg, p0, p1, p2, n, hit, dist));
        EXPECT_FALSE(VxIntersect::SegmentFaceCulled(seg, p0, p1, p2, n, hit, dist, i1, i2));
    }

    // Line hit even with negative t
    {
        VxRay line = MakeRay(VxVector(1, 1, -5), VxVector(0, 0, 1));
        EXPECT_TRUE(VxIntersect::LineFace(line, p0, p1, p2, n, hit, dist));
        EXPECT_VEC3_NEAR(hit, VxVector(1, 1, 0), BINARY_TOL);
        EXPECT_TRUE(VxIntersect::LineFace(line, p0, p1, p2, n, hit, dist, i1, i2));
    }
}

TEST(VxIntersect_Faces, GetPointCoefficients_BarycentricBasics) {
    VxVector p0(0, 0, 0), p1(1, 0, 0), p2(0, 1, 0);
    VxVector n(0, 0, 1);

    const VxVector p(0.25f, 0.25f, 0.0f);
    int i1 = -1, i2 = -1;
    ASSERT_TRUE(VxIntersect::PointInFace(p, p0, p1, p2, n, i1, i2));

    float v0 = 0.0f, v1 = 0.0f, v2 = 0.0f;
    VxIntersect::GetPointCoefficients(p, p0, p1, p2, i1, i2, v0, v1, v2);

    EXPECT_NEAR(v0 + v1 + v2, 1.0f, BINARY_TOL);
    EXPECT_NEAR(v0, 0.5f, BINARY_TOL);
    EXPECT_NEAR(v1, 0.25f, BINARY_TOL);
    EXPECT_NEAR(v2, 0.25f, BINARY_TOL);
}

TEST(VxIntersect_SIMDConsistency, GetPointCoefficients_MatchScalarReferenceRandomized) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> centerDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> offsetDist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);

    int generated = 0;
    int attempts = 0;
    while (generated < 500 && attempts < 10000) {
        ++attempts;

        const VxVector center(centerDist(rng), centerDist(rng), centerDist(rng));
        const VxVector p0 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        const VxVector p1 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        const VxVector p2 = center + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));

        VxVector n = CrossProduct(p1 - p0, p2 - p0);
        const float n2 = SquareMagnitude(n);
        if (n2 <= 1.0e-6f) {
            continue;
        }
        n *= (1.0f / sqrtf(n2));

        float u = unitDist(rng);
        float v = unitDist(rng);
        if (u + v > 1.0f) {
            u = 1.0f - u;
            v = 1.0f - v;
        }
        const VxVector point = p0 + (p1 - p0) * u + (p2 - p0) * v;

        int i1 = -1;
        int i2 = -1;
        if (!PointInFaceRef(point, p0, p1, p2, n, i1, i2)) {
            continue;
        }

        float act0 = 0.0f;
        float act1 = 0.0f;
        float act2 = 0.0f;
        float ref0 = 0.0f;
        float ref1 = 0.0f;
        float ref2 = 0.0f;

        VxIntersect::GetPointCoefficients(point, p0, p1, p2, i1, i2, act0, act1, act2);
        GetPointCoefficientsRef(point, p0, p1, p2, i1, i2, ref0, ref1, ref2);

        EXPECT_TRUE(ScaleRelativeNear(act0, ref0, BINARY_TOL * 8.0f));
        EXPECT_TRUE(ScaleRelativeNear(act1, ref1, BINARY_TOL * 8.0f));
        EXPECT_TRUE(ScaleRelativeNear(act2, ref2, BINARY_TOL * 8.0f));
        EXPECT_TRUE(ScaleRelativeNear(act0 + act1 + act2, 1.0f, BINARY_TOL * 8.0f));

        ++generated;
    }

    EXPECT_EQ(generated, 500);
}

TEST(VxIntersect_Faces, FaceFace_BasicOverlapAndSeparation) {
    VxVector a0(0, 0, 0), a1(1, 0, 0), a2(0, 1, 0);
    VxVector n0(0, 0, 1);

    // Overlapping coplanar triangles
    {
        VxVector b0(0.5f, 0.0f, 0.0f), b1(1.5f, 0.0f, 0.0f), b2(0.5f, 1.0f, 0.0f);
        VxVector n1(0, 0, 1);
        EXPECT_TRUE(VxIntersect::FaceFace(a0, a1, a2, n0, b0, b1, b2, n1));
    }

    // Separated coplanar triangles
    {
        VxVector b0(10.0f, 10.0f, 0.0f), b1(11.0f, 10.0f, 0.0f), b2(10.0f, 11.0f, 0.0f);
        VxVector n1(0, 0, 1);
        EXPECT_FALSE(VxIntersect::FaceFace(a0, a1, a2, n0, b0, b1, b2, n1));
    }

    // Parallel planes, separated along Z
    {
        VxVector b0(0.5f, 0.0f, 1.0f), b1(1.5f, 0.0f, 1.0f), b2(0.5f, 1.0f, 1.0f);
        VxVector n1(0, 0, 1);
        EXPECT_FALSE(VxIntersect::FaceFace(a0, a1, a2, n0, b0, b1, b2, n1));
    }
}

TEST(VxIntersect_Faces, FaceFace_RandomizedSymmetry) {
    std::mt19937 rng(20260223u);
    std::uniform_real_distribution<float> centerDist(-15.0f, 15.0f);
    std::uniform_real_distribution<float> offsetDist(-2.0f, 2.0f);

    auto randomTriangle = [&](VxVector &p0, VxVector &p1, VxVector &p2, VxVector &n) -> bool {
        const VxVector c(centerDist(rng), centerDist(rng), centerDist(rng));
        p0 = c + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        p1 = c + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        p2 = c + VxVector(offsetDist(rng), offsetDist(rng), offsetDist(rng));
        n = CrossProduct(p1 - p0, p2 - p0);
        const float n2 = SquareMagnitude(n);
        if (n2 <= 1.0e-6f) {
            return false;
        }
        n *= (1.0f / sqrtf(n2));
        return true;
    };

    int generated = 0;
    int attempts = 0;
    while (generated < 250 && attempts < 2000) {
        ++attempts;

        VxVector a0, a1, a2, b0, b1, b2, n0, n1;
        if (!randomTriangle(a0, a1, a2, n0)) {
            continue;
        }
        if (!randomTriangle(b0, b1, b2, n1)) {
            continue;
        }

        const XBOOL ab = VxIntersect::FaceFace(a0, a1, a2, n0, b0, b1, b2, n1);
        const XBOOL ba = VxIntersect::FaceFace(b0, b1, b2, n1, a0, a1, a2, n0);
        EXPECT_EQ(ab, ba) << "Asymmetry detected at case " << generated;
        ++generated;
    }

    EXPECT_EQ(generated, 250);
}
