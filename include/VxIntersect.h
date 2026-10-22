#ifndef VXINTERSECT_H
#define VXINTERSECT_H

#include "VxMathDefines.h"

struct VxVector;
struct VxBbox;
class VxMatrix;
class VxRay;
class VxOBB;
class VxPlane;
class VxSphere;
class VxFrustum;

/**
 * @brief Provides static methods for various intersection tests.
 *
 * @remarks
 * The VxIntersect class is a collection of static methods for performing
 * intersection tests between various geometric primitives, such as rays,
 * segments, lines, boxes (AABB and OBB), planes, triangles (faces), spheres, and frustums.
 *
 * @see VxPlane, VxRay, VxBbox, VxVector
 */
class VxIntersect {
public:
    //----------- Boxes

    /// @brief Tests a closed forward ray against an AABB; only exactly zero direction components are parallel.
    VX_EXPORT static XBOOL RayBox(const VxRay &ray, const VxBbox &box);
    /**
     * @brief Finds the intersection points of a ray with an AABB.
     * @return 0 on failure, -1 when the entry parameter is <= 2^-23, otherwise 1.
     * Uses the original absolute 2^-23 parallel/forward thresholds; failure preserves outputs.
     * Writes line entry/exit points, including an entry behind the origin. Both normals
     * oppose their direction component; tied faces prefer X, then Y, then Z.
     * If all components are parallel and the origin is inside, both points are the origin and normals are zero.
     */
    VX_EXPORT static XBOOL RayBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);

    /// @brief Tests closed segment overlap for origin+t*direction, 0 <= t <= 1.
    VX_EXPORT static XBOOL SegmentBox(const VxRay &ray, const VxBbox &box);
    /**
     * @brief Reports segment surface crossings using the original absolute 2^-23 thresholds.
     * Rejects entry > 1+2^-23, and entry < 2^-23 with exit > 1+2^-23 (contained segment).
     * @return 0 on failure, -1 when entry <= 2^-23, otherwise 1; failure preserves outputs.
     * Writes full line entry/exit points without clipping to [0,1]. Both normals oppose
     * their direction component; tied faces prefer X, then Y, then Z.
     */
    VX_EXPORT static XBOOL SegmentBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);

    /// @brief Tests a closed infinite line against an AABB; exactly zero direction tests its origin.
    VX_EXPORT static XBOOL LineBox(const VxRay &ray, const VxBbox &box);
    /**
     * @brief Finds both actual infinite-line crossings, nearest to origin first.
     * Uses the original absolute 2^-23 parallel threshold and opposite-direction normals.
     * Equal-distance crossings on opposite sides prefer X/Y/Z, then Min. A contained all-parallel line writes
     * origin to both points and zero normals. Returns 1 on success, 0 without output writes on failure.
     */
    VX_EXPORT static XBOOL LineBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);

    /// @brief Tests closed AABBs; face/edge/corner contact and zero extents count as intersection.
    VX_EXPORT static XBOOL AABBAABB(const VxBbox &box1, const VxBbox &box2);
    /// @brief Tests closed AABB/OBB overlap with unit orthogonal OBB axes and nonnegative half-extents.
    VX_EXPORT static XBOOL AABBOBB(const VxBbox &box1, const VxOBB &box2);
    /// @brief Tests closed OBB overlap with unit orthogonal axes and nonnegative half-extents.
    VX_EXPORT static XBOOL OBBOBB(const VxOBB &box1, const VxOBB &box2);
    /// @brief Tests closed AABB/triangle overlap; N is the face normal (need not be unit length).
    VX_EXPORT static XBOOL AABBFace(const VxBbox &box1, const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N);

    //---------- Planes

    // The five ray/segment/line tests return the parameter t in origin + t*direction,
    // not a metric distance. They retain the original absolute 2^-23 parallel
    // and endpoint tolerances. Parallel/backface rejection leaves both outputs
    // unchanged; a parameter-range rejection writes t but preserves point.
    /// @brief Intersects a ray with a plane.
    VX_EXPORT static XBOOL RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    /// @brief Intersects a ray with a plane, culled by the plane's normal.
    VX_EXPORT static XBOOL RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    /// @brief Intersects a line segment with a plane.
    VX_EXPORT static XBOOL SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    /// @brief Intersects a line segment with a plane, culled by the plane's normal.
    VX_EXPORT static XBOOL SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    /// @brief Intersects an infinite line with a plane.
    VX_EXPORT static XBOOL LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    /// @brief Checks for intersection between an AABB and a plane.
    VX_EXPORT static XBOOL BoxPlane(const VxBbox &box, const VxPlane &plane);
    /// @brief Checks for intersection between a transformed AABB (OBB) and a plane.
    VX_EXPORT static XBOOL BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane);
    /// @brief Checks for intersection between a triangle face and a plane.
    // VX_EXPORT static XBOOL FacePlane(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxPlane &plane);
    /// @brief Finds the intersection point of three planes.
    /// @return FALSE when there is no unique solution; p is then unchanged.
    VX_EXPORT static XBOOL Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p);

    //---------- Faces

    /**
     * @brief Tests the closed triangle in the projection selected by the largest absolute normal component.
     * Ties drop X, then Y, then Z. Both axes are written even on failure; no coplanarity test is performed.
     * Returns 0 outside, 1 for negative edge signs, 2 for positive/all-zero signs. Degenerate triangles
     * use their finite projected point/segment hull. Aliased axis outputs retain the second axis.
     */
    VX_EXPORT static XBOOL PointInFace(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, int &i1, int &i2);
    /// @brief Intersects a ray with a triangle face.
    VX_EXPORT static XBOOL RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    /// @brief Intersects a ray with a triangle face, providing barycentric coordinate information.
    VX_EXPORT static XBOOL RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    /// @brief Intersects a ray with a front-facing triangle face.
    VX_EXPORT static XBOOL RayFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    /// @brief Intersects a line segment with a triangle face.
    VX_EXPORT static XBOOL SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    /// @brief Intersects a line segment with a triangle face, providing barycentric coordinate information.
    VX_EXPORT static XBOOL SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    /// @brief Intersects a line segment with a front-facing triangle face.
    VX_EXPORT static XBOOL SegmentFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    /// @brief Intersects an infinite line with a triangle face.
    VX_EXPORT static XBOOL LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    /// @brief Intersects an infinite line with a triangle face, providing barycentric coordinate information.
    VX_EXPORT static XBOOL LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);

    /// @brief Calculates barycentric coordinates for a point within a triangle face.
    VX_EXPORT static void GetPointCoefficients(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef);

    /// @brief Checks for intersection between two triangle faces.
    VX_EXPORT static XBOOL FaceFace(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0, const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1);

    //--------- Frustum

    /// @brief Conservative visibility test: rejects a face only when all three
    /// vertices lie strictly outside the same frustum plane. TRUE can include
    /// disjoint triangles near a frustum edge; coplanar vertices are retained.
    VX_EXPORT static XBOOL FrustumFace(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2);
    /// @brief Checks for intersection between a frustum and an AABB.
    VX_EXPORT static XBOOL FrustumAABB(const VxFrustum &frustum, const VxBbox &box);
    /// @brief Checks for intersection between a frustum and an OBB.
    VX_EXPORT static XBOOL FrustumOBB(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);
    /**
     * @brief Legacy ordered-plane visibility test. Accepts on the first
     * crossing/touching plane, rejects on the first strictly outside plane.
     * @deprecated Prefer FrustumOBB for its distinct eight-axis overlap test.
     */
    VX_EXPORT static XBOOL FrustumBox(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);

    //--------- Spheres
    /// @brief Checks for collision between two moving spheres.
    VX_EXPORT static XBOOL SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2, float *oCollisionTime1, float *oCollisionTime2);
    /// @brief Original line/sphere test: returns both intersections along the
    /// direction, including points behind the origin. Returns 0, 1, or 2;
    /// a miss preserves outputs, and a tangent writes the first available output.
    VX_EXPORT static int RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2);
    /// @brief Checks for intersection between a sphere and an AABB.
    VX_EXPORT static int SphereAABB(const VxSphere &iSphere, const VxBbox &iBox);
};

#endif // VXINTERSECT_H
