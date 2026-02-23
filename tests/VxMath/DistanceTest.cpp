#include "gtest/gtest.h"
#include "VxMath.h"
#include "VxDistance.h"
#include "VxMathTestHelpers.h"

#include <cmath>

using namespace VxMathTest;

namespace {

inline float Dot3Scalar(const VxVector& a, const VxVector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float SquareMagnitudeScalar(const VxVector& v) {
    return Dot3Scalar(v, v);
}

inline VxVector SubtractScalar(const VxVector& a, const VxVector& b) {
    return VxVector(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline VxVector SubtractScaled(const VxVector& a, const VxVector& b, float s) {
    return VxVector(a.x - b.x * s, a.y - b.y * s, a.z - b.z * s);
}

float PointLineSquareDistanceScalarRef(const VxVector& point, const VxRay& line, float* t0) {
    const VxVector diff = SubtractScalar(point, line.m_Origin);
    const float t = Dot3Scalar(diff, line.m_Direction) / SquareMagnitudeScalar(line.m_Direction);

    if (t0) *t0 = t;

    const VxVector d = SubtractScaled(diff, line.m_Direction, t);
    return SquareMagnitudeScalar(d);
}

float PointRaySquareDistanceScalarRef(const VxVector& point, const VxRay& ray, float* t0) {
    VxVector diff = SubtractScalar(point, ray.m_Origin);
    const float dotDiffDir = Dot3Scalar(diff, ray.m_Direction);

    float t;
    if (dotDiffDir > 0.0f) {
        t = dotDiffDir / SquareMagnitudeScalar(ray.m_Direction);
        diff = SubtractScaled(diff, ray.m_Direction, t);
    } else {
        t = 0.0f;
    }

    if (t0) *t0 = t;

    return SquareMagnitudeScalar(diff);
}

float PointSegmentSquareDistanceScalarRef(const VxVector& point, const VxRay& segment, float* t0) {
    VxVector diff = SubtractScalar(point, segment.m_Origin);
    const float dotDiffDir = Dot3Scalar(diff, segment.m_Direction);

    float t;
    if (dotDiffDir > 0.0f) {
        const float dotDirDir = SquareMagnitudeScalar(segment.m_Direction);
        if (dotDiffDir < dotDirDir) {
            t = dotDiffDir / dotDirDir;
            diff = SubtractScaled(diff, segment.m_Direction, t);
        } else {
            t = 1.0f;
            diff = SubtractScalar(diff, segment.m_Direction);
        }
    } else {
        t = 0.0f;
    }

    if (t0) *t0 = t;

    return SquareMagnitudeScalar(diff);
}

void ExpectDistanceOrClassifyEqual(float actual, float expected, float tol) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual));
        return;
    }

    if (std::isinf(expected)) {
        EXPECT_TRUE(std::isinf(actual));
        EXPECT_EQ(std::signbit(actual), std::signbit(expected));
        return;
    }

    EXPECT_TRUE(ScaleRelativeNear(actual, expected, tol)) << "actual=" << actual << " expected=" << expected;
}

} // namespace

// Test fixture for VxDistance tests
class VxDistanceTest : public ::testing::Test {
protected:
    // Common test vectors
    const VxVector origin{0.0f, 0.0f, 0.0f};
    const VxVector unitX{1.0f, 0.0f, 0.0f};
    const VxVector unitY{0.0f, 1.0f, 0.0f};
    const VxVector unitZ{0.0f, 0.0f, 1.0f};
};

// ============================================================================
// PointLineSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, PointLineSquareDistance_PointOnLine) {
    // Point is on the line
    VxRay line(origin, unitX, nullptr);
    VxVector point(5.0f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointLineSquareDistance(point, line, &t);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointLineSquareDistance_PointOffLine) {
    // Point is perpendicular to line at origin
    VxRay line(origin, unitX, nullptr);
    VxVector point(0.0f, 3.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointLineSquareDistance(point, line, &t);
    
    EXPECT_NEAR(dist, 9.0f, BINARY_TOL);  // Distance^2 = 3^2
    EXPECT_NEAR(t, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointLineSquareDistance_GeneralCase) {
    // Line along X axis, point at (2, 3, 4)
    VxRay line(origin, unitX, nullptr);
    VxVector point(2.0f, 3.0f, 4.0f);
    
    float t;
    float dist = VxDistance::PointLineSquareDistance(point, line, &t);
    
    // Closest point on line is (2, 0, 0), distance = sqrt(9+16) = 5
    EXPECT_NEAR(dist, 25.0f, BINARY_TOL);  // Distance^2 = 3^2 + 4^2 = 25
    EXPECT_NEAR(t, 2.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointLineSquareDistance_NullParameter) {
    VxRay line(origin, unitX, nullptr);
    VxVector point(2.0f, 3.0f, 4.0f);
    
    // Should not crash with null t parameter
    float dist = VxDistance::PointLineSquareDistance(point, line, nullptr);
    EXPECT_NEAR(dist, 25.0f, BINARY_TOL);
}

// ============================================================================
// PointRaySquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, PointRaySquareDistance_PointOnRay) {
    VxRay ray(origin, unitX, nullptr);
    VxVector point(5.0f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointRaySquareDistance(point, ray, &t);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointRaySquareDistance_PointBehindRay) {
    // Point is behind the ray origin (negative t)
    VxRay ray(origin, unitX, nullptr);
    VxVector point(-5.0f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointRaySquareDistance(point, ray, &t);
    
    // Closest point is at origin (clamped t=0)
    EXPECT_NEAR(dist, 25.0f, BINARY_TOL);  // Distance^2 from origin to point
    EXPECT_NEAR(t, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointRaySquareDistance_PointOffRay) {
    VxRay ray(origin, unitX, nullptr);
    VxVector point(2.0f, 3.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointRaySquareDistance(point, ray, &t);
    
    EXPECT_NEAR(dist, 9.0f, BINARY_TOL);  // Distance^2 = 3^2
    EXPECT_NEAR(t, 2.0f, BINARY_TOL);
}

// ============================================================================
// PointSegmentSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, PointSegmentSquareDistance_PointOnSegment) {
    VxRay segment(origin, unitX, nullptr);
    VxVector point(0.5f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointSegmentSquareDistance(point, segment, &t);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t, 0.5f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointSegmentSquareDistance_PointBeyondEnd) {
    // Point is beyond the segment end (t > 1)
    VxRay segment(origin, unitX, nullptr);
    VxVector point(5.0f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointSegmentSquareDistance(point, segment, &t);
    
    // Closest point is at segment end (1, 0, 0)
    EXPECT_NEAR(dist, 16.0f, BINARY_TOL);  // Distance^2 = 4^2
    EXPECT_NEAR(t, 1.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointSegmentSquareDistance_PointBeforeStart) {
    // Point is before the segment start (t < 0)
    VxRay segment(origin, unitX, nullptr);
    VxVector point(-5.0f, 0.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointSegmentSquareDistance(point, segment, &t);
    
    // Closest point is at origin
    EXPECT_NEAR(dist, 25.0f, BINARY_TOL);
    EXPECT_NEAR(t, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointSegmentSquareDistance_PointOffSegment) {
    VxRay segment(origin, unitX, nullptr);
    VxVector point(0.5f, 3.0f, 4.0f);
    
    float t;
    float dist = VxDistance::PointSegmentSquareDistance(point, segment, &t);
    
    EXPECT_NEAR(dist, 25.0f, BINARY_TOL);  // Distance^2 = 3^2 + 4^2
    EXPECT_NEAR(t, 0.5f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointDistanceVariants_MatchScalarReferenceAcrossEdgeInputs) {
    struct PointDistanceCase {
        VxVector point;
        VxVector origin;
        VxVector direction;
    };

    const PointDistanceCase cases[] = {
        {VxVector(2.0f, 3.0f, 4.0f), VxVector(0.0f, 0.0f, 0.0f), VxVector(1.0f, 0.0f, 0.0f)},
        {VxVector(-2.5f, 7.0f, -1.5f), VxVector(1.0f, -3.0f, 2.0f), VxVector(-2.0f, 0.5f, 1.0f)},
        {VxVector(1.0e6f + 3.0f, 1.0e6f + 4.0f, 1.0e6f - 2.0f), VxVector(1.0e6f, 1.0e6f, 1.0e6f), VxVector(4.0f, -2.0f, 1.0f)},
        {VxVector(4.0e-6f, -8.0e-6f, 9.0e-6f), VxVector(-1.0e-6f, 2.0e-6f, -3.0e-6f), VxVector(7.0e-4f, -6.0e-4f, 2.0e-4f)},
        {VxVector(3.0f, -9.0f, 6.0f), VxVector(-4.0f, 5.0f, -2.0f), VxVector(2.0f, 3.0f, -1.0f)},
    };

    for (const auto& c : cases) {
        VxRay primitive(c.origin, c.direction, nullptr);

        float tLineActual = 0.0f;
        float tLineRef = 0.0f;
        const float lineActual = VxDistance::PointLineSquareDistance(c.point, primitive, &tLineActual);
        const float lineRef = PointLineSquareDistanceScalarRef(c.point, primitive, &tLineRef);
        ExpectDistanceOrClassifyEqual(lineActual, lineRef, BINARY_TOL);
        ExpectDistanceOrClassifyEqual(tLineActual, tLineRef, BINARY_TOL);

        float tRayActual = 0.0f;
        float tRayRef = 0.0f;
        const float rayActual = VxDistance::PointRaySquareDistance(c.point, primitive, &tRayActual);
        const float rayRef = PointRaySquareDistanceScalarRef(c.point, primitive, &tRayRef);
        ExpectDistanceOrClassifyEqual(rayActual, rayRef, BINARY_TOL);
        ExpectDistanceOrClassifyEqual(tRayActual, tRayRef, BINARY_TOL);

        float tSegActual = 0.0f;
        float tSegRef = 0.0f;
        const float segActual = VxDistance::PointSegmentSquareDistance(c.point, primitive, &tSegActual);
        const float segRef = PointSegmentSquareDistanceScalarRef(c.point, primitive, &tSegRef);
        ExpectDistanceOrClassifyEqual(segActual, segRef, BINARY_TOL);
        ExpectDistanceOrClassifyEqual(tSegActual, tSegRef, BINARY_TOL);
    }
}

// ============================================================================
// LineLineSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, LineLineSquareDistance_ParallelLines) {
    // Two parallel lines along X axis, separated by distance 3 along Y
    VxRay line0(origin, unitX, nullptr);
    VxRay line1(VxVector(0.0f, 3.0f, 0.0f), unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineLineSquareDistance(line0, line1, &t0, &t1);
    
    EXPECT_NEAR(dist, 9.0f, BINARY_TOL);  // Distance^2 = 3^2
}

TEST_F(VxDistanceTest, LineLineSquareDistance_IntersectingLines) {
    // Two lines that intersect at origin
    VxRay line0(origin, unitX, nullptr);
    VxRay line1(origin, unitY, nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineLineSquareDistance(line0, line1, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, LineLineSquareDistance_SkewLines) {
    // Two skew lines
    VxRay line0(origin, unitX, nullptr);
    VxRay line1(VxVector(0.0f, 1.0f, 0.0f), unitZ, nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineLineSquareDistance(line0, line1, &t0, &t1);
    
    EXPECT_NEAR(dist, 1.0f, BINARY_TOL);  // Distance^2 = 1^2
    EXPECT_NEAR(t0, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

// ============================================================================
// LineRaySquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, LineRaySquareDistance_RayIntersectsLine) {
    VxRay line(origin, unitX, nullptr);
    VxRay ray(VxVector(0.0f, 1.0f, 0.0f), VxVector(0.0f, -1.0f, 0.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineRaySquareDistance(line, ray, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 1.0f, BINARY_TOL);  // Ray reaches line at t=1
}

TEST_F(VxDistanceTest, LineRaySquareDistance_RayPointsAway) {
    // Ray points away from line
    VxRay line(origin, unitX, nullptr);
    VxRay ray(VxVector(0.0f, 1.0f, 0.0f), unitY, nullptr);  // Ray going further away
    
    float t0, t1;
    float dist = VxDistance::LineRaySquareDistance(line, ray, &t0, &t1);
    
    // Closest point on ray is at its origin
    EXPECT_NEAR(dist, 1.0f, BINARY_TOL);  // Distance^2 = 1^2
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

// ============================================================================
// LineSegmentSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, LineSegmentSquareDistance_SegmentCrossesLine) {
    VxRay line(origin, unitX, nullptr);
    VxRay segment(VxVector(0.0f, -1.0f, 0.0f), VxVector(0.0f, 2.0f, 0.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineSegmentSquareDistance(line, segment, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.5f, BINARY_TOL);  // Segment crosses line at midpoint
}

TEST_F(VxDistanceTest, LineSegmentSquareDistance_SegmentParallelToLine) {
    VxRay line(origin, unitX, nullptr);
    VxRay segment(VxVector(0.0f, 2.0f, 0.0f), unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineSegmentSquareDistance(line, segment, &t0, &t1);
    
    EXPECT_NEAR(dist, 4.0f, BINARY_TOL);  // Distance^2 = 2^2
}

// ============================================================================
// RayRaySquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, RayRaySquareDistance_IntersectingRays) {
    VxRay ray0(origin, unitX, nullptr);
    VxRay ray1(VxVector(1.0f, 1.0f, 0.0f), VxVector(0.0f, -1.0f, 0.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::RayRaySquareDistance(ray0, ray1, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 1.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 1.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, RayRaySquareDistance_ParallelRays) {
    VxRay ray0(origin, unitX, nullptr);
    VxRay ray1(VxVector(0.0f, 2.0f, 0.0f), unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::RayRaySquareDistance(ray0, ray1, &t0, &t1);
    
    EXPECT_NEAR(dist, 4.0f, BINARY_TOL);  // Distance^2 = 2^2
}

TEST_F(VxDistanceTest, RayRaySquareDistance_DivergingRays) {
    // Two rays starting from same point going in different directions
    VxRay ray0(origin, unitX, nullptr);
    VxRay ray1(origin, unitY, nullptr);
    
    float t0, t1;
    float dist = VxDistance::RayRaySquareDistance(ray0, ray1, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, RayRaySquareDistance_BothBehind) {
    // Two rays with closest point behind both origins
    VxRay ray0(VxVector(1.0f, 0.0f, 0.0f), unitX, nullptr);
    VxRay ray1(VxVector(0.0f, 1.0f, 0.0f), unitY, nullptr);
    
    float t0, t1;
    float dist = VxDistance::RayRaySquareDistance(ray0, ray1, &t0, &t1);
    
    // Both rays should clamp to t=0
    EXPECT_NEAR(dist, 2.0f, BINARY_TOL);  // Distance^2 = 1^2 + 1^2
    EXPECT_NEAR(t0, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

// ============================================================================
// RaySegmentSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, RaySegmentSquareDistance_RayHitsSegment) {
    VxRay ray(VxVector(0.5f, 1.0f, 0.0f), VxVector(0.0f, -1.0f, 0.0f), nullptr);
    VxRay segment(origin, unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::RaySegmentSquareDistance(ray, segment, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 1.0f, BINARY_TOL);  // Ray parameter
    EXPECT_NEAR(t1, 0.5f, BINARY_TOL);  // Segment parameter
}

TEST_F(VxDistanceTest, RaySegmentSquareDistance_RayMissesSegment) {
    // Ray goes past the segment
    VxRay ray(VxVector(5.0f, 1.0f, 0.0f), VxVector(0.0f, -1.0f, 0.0f), nullptr);
    VxRay segment(origin, unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::RaySegmentSquareDistance(ray, segment, &t0, &t1);
    
    // Closest point is segment end (1, 0, 0) to ray at (5, 0, 0)
    EXPECT_NEAR(dist, 16.0f, BINARY_TOL);  // Distance^2 = 4^2
    EXPECT_NEAR(t1, 1.0f, BINARY_TOL);  // Segment clamped to end
}

// ============================================================================
// SegmentSegmentSquareDistance Tests
// ============================================================================

TEST_F(VxDistanceTest, SegmentSegmentSquareDistance_Intersecting) {
    VxRay seg0(VxVector(-1.0f, 0.0f, 0.0f), VxVector(2.0f, 0.0f, 0.0f), nullptr);
    VxRay seg1(VxVector(0.0f, -1.0f, 0.0f), VxVector(0.0f, 2.0f, 0.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::SegmentSegmentSquareDistance(seg0, seg1, &t0, &t1);
    
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 0.5f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.5f, BINARY_TOL);
}

TEST_F(VxDistanceTest, SegmentSegmentSquareDistance_Parallel) {
    VxRay seg0(origin, unitX, nullptr);
    VxRay seg1(VxVector(0.0f, 2.0f, 0.0f), unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::SegmentSegmentSquareDistance(seg0, seg1, &t0, &t1);
    
    EXPECT_NEAR(dist, 4.0f, BINARY_TOL);  // Distance^2 = 2^2
}

TEST_F(VxDistanceTest, SegmentSegmentSquareDistance_EndpointClosest) {
    // Test case with segment endpoints as closest points
    // seg0: from (0,0,0) to (1,0,0)
    // seg1: from (3,0,0) to (3,1,0) 
    // Closest points: seg0(1)=(1,0,0) to seg1(0)=(3,0,0)
    // Distance = 2, distance^2 = 4
    VxRay seg0(origin, unitX, nullptr);
    VxRay seg1(VxVector(3.0f, 0.0f, 0.0f), unitY, nullptr);
    
    float t0, t1;
    float dist = VxDistance::SegmentSegmentSquareDistance(seg0, seg1, &t0, &t1);
    
    EXPECT_NEAR(dist, 4.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 1.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, SegmentSegmentSquareDistance_CollinearOverlapping) {
    // Two collinear overlapping segments
    // seg0: from (0,0,0) to (2,0,0)
    // seg1: from (1,0,0) to (3,0,0)
    // They overlap from x=1 to x=2
    //
    // The ground-truth DLL finds the overlap and returns distance=0.
    VxRay seg0(origin, VxVector(2.0f, 0.0f, 0.0f), nullptr);
    VxRay seg1(VxVector(1.0f, 0.0f, 0.0f), VxVector(2.0f, 0.0f, 0.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::SegmentSegmentSquareDistance(seg0, seg1, &t0, &t1);
    
    // Distance should be 0 since segments overlap
    EXPECT_NEAR(dist, 0.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, SegmentSegmentSquareDistance_CollinearNonOverlapping) {
    // Two collinear segments that don't overlap
    VxRay seg0(origin, unitX, nullptr);
    VxRay seg1(VxVector(3.0f, 0.0f, 0.0f), unitX, nullptr);
    
    float t0, t1;
    float dist = VxDistance::SegmentSegmentSquareDistance(seg0, seg1, &t0, &t1);
    
    // Closest points: seg0 end at (1,0,0), seg1 start at (3,0,0)
    EXPECT_NEAR(dist, 4.0f, BINARY_TOL);  // Distance^2 = 2^2
    EXPECT_NEAR(t0, 1.0f, BINARY_TOL);
    EXPECT_NEAR(t1, 0.0f, BINARY_TOL);
}

// ============================================================================
// Distance (non-squared) wrapper tests
// ============================================================================

TEST_F(VxDistanceTest, PointLineDistance_Wrapper) {
    VxRay line(origin, unitX, nullptr);
    VxVector point(0.0f, 3.0f, 4.0f);
    
    float t;
    float dist = VxDistance::PointLineDistance(point, line, &t);
    
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);  // sqrt(3^2 + 4^2) = 5
}

TEST_F(VxDistanceTest, PointRayDistance_Wrapper) {
    VxRay ray(origin, unitX, nullptr);
    VxVector point(0.0f, 3.0f, 4.0f);
    
    float dist = VxDistance::PointRayDistance(point, ray);
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, PointSegmentDistance_Wrapper) {
    VxRay segment(origin, unitX, nullptr);
    VxVector point(0.0f, 3.0f, 4.0f);
    
    float dist = VxDistance::PointSegmentDistance(point, segment);
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, LineLineDistance_Wrapper) {
    VxRay line0(origin, unitX, nullptr);
    VxRay line1(VxVector(0.0f, 3.0f, 4.0f), unitX, nullptr);
    
    float dist = VxDistance::LineLineDistance(line0, line1);
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, RayRayDistance_Wrapper) {
    VxRay ray0(origin, unitX, nullptr);
    VxRay ray1(VxVector(0.0f, 3.0f, 4.0f), unitX, nullptr);
    
    float dist = VxDistance::RayRayDistance(ray0, ray1);
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, SegmentSegmentDistance_Wrapper) {
    VxRay seg0(origin, unitX, nullptr);
    VxRay seg1(VxVector(0.0f, 3.0f, 4.0f), unitX, nullptr);
    
    float dist = VxDistance::SegmentSegmentDistance(seg0, seg1);
    EXPECT_NEAR(dist, 5.0f, BINARY_TOL);
}

// ============================================================================
// Edge Cases and Regression Tests
// ============================================================================

TEST_F(VxDistanceTest, ZeroLengthDirection) {
    // Edge case: direction vector is zero (degenerate)
    // This tests handling of near-singular cases
    VxRay ray(origin, VxVector(1e-10f, 0.0f, 0.0f), nullptr);
    VxVector point(1.0f, 1.0f, 0.0f);
    
    // Should handle gracefully (may produce large values but not crash)
    float dist = VxDistance::PointRaySquareDistance(point, ray, nullptr);
    EXPECT_TRUE(std::isfinite(dist));
}

TEST_F(VxDistanceTest, LargeCoordinates) {
    // Test with large coordinate values
    VxRay ray(VxVector(1e6f, 1e6f, 1e6f), unitX, nullptr);
    VxVector point(1e6f + 3.0f, 1e6f + 4.0f, 1e6f);
    
    float t;
    float dist = VxDistance::PointRaySquareDistance(point, ray, &t);
    
    EXPECT_NEAR(dist, 16.0f, 0.1f);  // Distance^2 = 4^2, allow more tolerance for large values
    EXPECT_NEAR(t, 3.0f, 0.01f);
}

TEST_F(VxDistanceTest, NegativeDirectionVector) {
    // Test with negative direction
    VxRay ray(origin, VxVector(-1.0f, 0.0f, 0.0f), nullptr);
    VxVector point(-3.0f, 4.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointRaySquareDistance(point, ray, &t);
    
    EXPECT_NEAR(dist, 16.0f, BINARY_TOL);  // Distance^2 = 4^2
    EXPECT_NEAR(t, 3.0f, BINARY_TOL);
}

TEST_F(VxDistanceTest, NonUnitDirectionVector) {
    // Test with non-unit direction (length 2)
    VxRay ray(origin, VxVector(2.0f, 0.0f, 0.0f), nullptr);
    VxVector point(1.0f, 1.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointLineSquareDistance(point, ray, &t);
    
    EXPECT_NEAR(dist, 1.0f, BINARY_TOL);  // Distance^2 = 1^2 (perpendicular distance)
    EXPECT_NEAR(t, 0.5f, BINARY_TOL);     // t = 0.5 because closest point is (1,0,0), and 0.5*2 = 1
}

TEST_F(VxDistanceTest, SegmentWithNonUnitDirection) {
    // Segment from (0,0,0) to (2,0,0) represented with direction (2,0,0)
    VxRay segment(origin, VxVector(2.0f, 0.0f, 0.0f), nullptr);
    VxVector point(1.0f, 1.0f, 0.0f);
    
    float t;
    float dist = VxDistance::PointSegmentSquareDistance(point, segment, &t);
    
    EXPECT_NEAR(dist, 1.0f, BINARY_TOL);
    EXPECT_NEAR(t, 0.5f, BINARY_TOL);  // t=0.5 gives origin + 0.5*direction = (1,0,0)
}

TEST_F(VxDistanceTest, SkewLines3D) {
    // Classic skew lines in 3D
    VxRay line0(VxVector(0.0f, 0.0f, 0.0f), VxVector(1.0f, 0.0f, 0.0f), nullptr);
    VxRay line1(VxVector(0.0f, 1.0f, 1.0f), VxVector(0.0f, 0.0f, 1.0f), nullptr);
    
    float t0, t1;
    float dist = VxDistance::LineLineSquareDistance(line0, line1, &t0, &t1);
    
    // Closest approach is 1 unit in Y direction
    EXPECT_NEAR(dist, 1.0f, BINARY_TOL);
    EXPECT_NEAR(t0, 0.0f, BINARY_TOL);
    EXPECT_NEAR(t1, -1.0f, BINARY_TOL);
}
