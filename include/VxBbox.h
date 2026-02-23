#ifndef VXBBOX_H
#define VXBBOX_H

#include "VxVector.h"

// Forward declarations
class VxMatrix;

/**
 * @struct VxBbox
 * @brief Represents an axis-aligned bounding box (AABB) in 3D space.
 *
 * @remarks
 * A VxBbox is defined by its minimum and maximum corner points. It provides
 * methods for merging, intersection, containment testing, and other spatial
 * operations commonly used in 3D graphics and physics.
 *
 * @see VxVector
 */
typedef struct VxBbox {
#if !defined(_MSC_VER)
    VxVector Max; ///< The maximum corner of the box (highest x, y, and z coordinates).
    VxVector Min; ///< The minimum corner of the box (lowest x, y, and z coordinates).
#else
    union {
        struct {
            VxVector Max; ///< The maximum corner of the box (highest x, y, and z coordinates).
            VxVector Min; ///< The minimum corner of the box (lowest x, y, and z coordinates).
        };
        float v[6]; ///< Array access to components {Max.x, Max.y, Max.z, Min.x, Min.y, Min.z}.
    };
#endif

    /**
     * @brief Default constructor. Creates an invalid box ready for merging.
     * @remarks The box is initialized with inverted bounds (Min > Max) so that the
     * first call to `Merge` will correctly initialize its extents.
     */
    VxBbox() : Max(-1e6f, -1e6f, -1e6f), Min(1e6f, 1e6f, 1e6f) {}

    /**
     * @brief Constructs a box from minimum and maximum corner points.
     * @param iMin The minimum corner point.
     * @param iMax The maximum corner point.
     */
    VxBbox(const VxVector &iMin, const VxVector &iMax) : Max(iMax), Min(iMin) {}

    /**
     * @brief Constructs a box centered at the origin with a given radius.
     * @param value The half-size of the box in all dimensions.
     */
    explicit VxBbox(float value) {
        Max.x = value; Max.y = value; Max.z = value;
        Min.x = -value; Min.y = -value; Min.z = -value;
    }

    /**
     * @brief Checks if the bounding box has valid extents.
     * @return TRUE if Min.x <= Max.x, Min.y <= Max.y, and Min.z <= Max.z.
     */
    XBOOL IsValid() const {
        if (Min.x > Max.x) return FALSE;
        if (Min.y > Max.y) return FALSE;
        if (Min.z > Max.z) return FALSE;
        return TRUE;
    }

    /// @brief Returns the size of the box (width, height, depth).
    VxVector GetSize() const { return Max - Min; }
    /// @brief Returns the half-size of the box (half-width, half-height, half-depth).
    VxVector GetHalfSize() const { return (Max - Min) * 0.5f; }
    /// @brief Returns the center point of the box.
    VxVector GetCenter() const { return (Max + Min) * 0.5f; }

    /// @brief Sets the box's corners from min and max points.
    void SetCorners(const VxVector &min, const VxVector &max) {
        Min = min; Max = max;
    }
    /// @brief Sets the box from a position (min corner) and size.
    void SetDimension(const VxVector &position, const VxVector &size) {
        Min = position; Max = position + size;
    }
    /// @brief Sets the box from its center and half-size.
    void SetCenter(const VxVector &center, const VxVector &halfsize) {
        Min = center - halfsize; Max = center + halfsize;
    }

    /**
     * @brief Resets the box to an invalid state, ready for merging.
     * @remarks This sets Min to a large positive value and Max to a large negative
     * value, ensuring any subsequent `Merge` operation will correctly define the box.
     */
    void Reset() {
        Max.x = -1e6f; Max.y = -1e6f; Max.z = -1e6f;
        Min.x = 1e6f; Min.y = 1e6f; Min.z = 1e6f;
    }

    /**
     * @brief Expands this box to enclose another box.
     * @param v The other VxBbox to merge with this one.
     */
    void Merge(const VxBbox &v) {
        Max.x = XMax(v.Max.x, Max.x); Max.y = XMax(v.Max.y, Max.y); Max.z = XMax(v.Max.z, Max.z);
        Min.x = XMin(v.Min.x, Min.x); Min.y = XMin(v.Min.y, Min.y); Min.z = XMin(v.Min.z, Min.z);
    }

    /**
     * @brief Expands this box to enclose a point.
     * @param v The VxVector point to merge with this one.
     */
    void Merge(const VxVector &v) {
        if (v.x > Max.x) Max.x = v.x;
        if (v.x < Min.x) Min.x = v.x;
        if (v.y > Max.y) Max.y = v.y;
        if (v.y < Min.y) Min.y = v.y;
        if (v.z > Max.z) Max.z = v.z;
        if (v.z < Min.z) Min.z = v.z;
    }

    /**
     * @brief Classifies a point's position relative to the box's planes.
     * @param iPoint The point to classify.
     * @return A bitmask of `VXCLIP_FLAGS` indicating which planes the point is outside of.
     */
    XDWORD Classify(const VxVector &iPoint) const {
        XDWORD flag = 0;
        if (iPoint.x < Min.x) flag |= VXCLIP_LEFT;
        else if (iPoint.x > Max.x) flag |= VXCLIP_RIGHT;
        if (iPoint.y < Min.y) flag |= VXCLIP_BOTTOM;
        else if (iPoint.y > Max.y) flag |= VXCLIP_TOP;
        if (iPoint.z < Min.z) flag |= VXCLIP_BACK;
        else if (iPoint.z > Max.z) flag |= VXCLIP_FRONT;
        return flag;
    }

    /**
     * @brief Classifies a box's position relative to this box.
     * @param iBox The box to classify.
     * @return A bitmask of `VXCLIP_FLAGS` indicating full exclusion on one or more axes.
     */
    XDWORD Classify(const VxBbox &iBox) const {
        XDWORD flag = 0;
        if (iBox.Max.z < Min.z) flag |= VXCLIP_BACK;
        else if (iBox.Min.z > Max.z) flag |= VXCLIP_FRONT;
        if (iBox.Max.x < Min.x) flag |= VXCLIP_LEFT;
        else if (iBox.Min.x > Max.x) flag |= VXCLIP_RIGHT;
        if (iBox.Max.y < Min.y) flag |= VXCLIP_BOTTOM;
        else if (iBox.Min.y > Max.y) flag |= VXCLIP_TOP;
        return flag;
    }

    /**
     * @brief Classifies the relative position of two boxes from a viewpoint.
     * @param box2 The other box.
     * @param pt The viewpoint.
     * @return 2 if `box2` is on the opposite side of this box from `pt`, 1 if `box2` is inside, 0 otherwise.
     */
    inline int Classify(const VxBbox &box2, const VxVector &pt) const;

    /**
     * @brief Classifies an array of vertices against the box planes.
     * @param iVcount The number of vertices to classify.
     * @param iVertices Pointer to the start of the vertex data.
     * @param iStride The byte offset between consecutive vertices.
     * @param oFlags An output array to be filled with `VXCLIP_FLAGS` for each vertex.
     */
    inline void ClassifyVertices(const int iVcount, XBYTE *iVertices, XDWORD iStride, XDWORD *oFlags) const;

    /**
     * @brief Classifies an array of vertices against a single axis of the box.
     * @param iVcount The number of vertices.
     * @param iVertices Pointer to the vertex data.
     * @param iStride The byte offset between vertices.
     * @param iAxis The axis to test against (0=x, 1=y, 2=z).
     * @param oFlags An output array to be filled with flags (0x01 if < min, 0x10 if > max).
     */
    inline void ClassifyVerticesOneAxis(const int iVcount, XBYTE *iVertices, XDWORD iStride, const int iAxis, XDWORD *oFlags) const;

    /**
     * @brief Calculates the intersection of this box with another box.
     * @param v The VxBbox to intersect with this one.
     * @remarks The result modifies this box to become the intersection volume.
     */
    void Intersect(const VxBbox &v) {
        Max.x = XMin(v.Max.x, Max.x); Max.y = XMin(v.Max.y, Max.y); Max.z = XMin(v.Max.z, Max.z);
        Min.x = XMax(v.Min.x, Min.x); Min.y = XMax(v.Min.y, Min.y); Min.z = XMax(v.Min.z, Min.z);
    }

    /**
     * @brief Tests if a point is inside the box.
     * @param v The point to test.
     * @return TRUE if the point is inside or on the boundary of this box, FALSE otherwise.
     */
    XBOOL VectorIn(const VxVector &v) const {
        if (v.x < Min.x) return FALSE;
        if (v.x > Max.x) return FALSE;
        if (v.y < Min.y) return FALSE;
        if (v.y > Max.y) return FALSE;
        if (v.z < Min.z) return FALSE;
        if (v.z > Max.z) return FALSE;
        return TRUE;
    }

    /**
     * @brief Tests if another box is totally inside this box.
     * @param b The box to test.
     * @return TRUE if box b is completely contained within this box, FALSE otherwise.
     */
    XBOOL IsBoxInside(const VxBbox &b) const {
        if (b.Min.x < Min.x) return 0;
        if (b.Min.y < Min.y) return 0;
        if (b.Min.z < Min.z) return 0;
        if (b.Max.x > Max.x) return 0;
        if (b.Max.y > Max.y) return 0;
        if (b.Max.z > Max.z) return 0;
        return 1;
    }

    /// @brief Equality operator.
    bool operator==(const VxBbox &iBox) const {
        return (Max == iBox.Max) && (Min == iBox.Min);
    }

    /**
     * @brief Transforms the eight corner points of this box by a matrix.
     * @param pts A pointer to an array to store the eight resulting points.
     * @param Mat The transformation matrix.
     */
    inline void TransformTo(VxVector *pts, const VxMatrix &Mat) const;

    /**
     * @brief Creates this box by transforming another box by a matrix.
     * @param sbox The source box to transform.
     * @param Mat The transformation matrix.
     */
    inline void TransformFrom(const VxBbox &sbox, const VxMatrix &Mat);
} VxBbox;

#endif // VXBBOX_H

// =================================================================
// VxBbox Inline Implementations
// =================================================================

#if !defined(VXBBOX_IMPL)
#define VXBBOX_IMPL

#include "VxMatrix.h"

inline int VxBbox::Classify(const VxBbox &box2, const VxVector &pt) const {
    XDWORD ptFlags = 0;
    if (pt.x < Min.x) ptFlags |= VXCLIP_LEFT; else if (pt.x > Max.x) ptFlags |= VXCLIP_RIGHT;
    if (pt.y < Min.y) ptFlags |= VXCLIP_BOTTOM; else if (pt.y > Max.y) ptFlags |= VXCLIP_TOP;
    if (pt.z < Min.z) ptFlags |= VXCLIP_BACK; else if (pt.z > Max.z) ptFlags |= VXCLIP_FRONT;
    XDWORD box2Flags = 0;
    if (box2.Max.z < Min.z) box2Flags |= VXCLIP_BACK; else if (box2.Min.z > Max.z) box2Flags |= VXCLIP_FRONT;
    if (box2.Max.x < Min.x) box2Flags |= VXCLIP_LEFT; else if (box2.Min.x > Max.x) box2Flags |= VXCLIP_RIGHT;
    if (box2.Max.y < Min.y) box2Flags |= VXCLIP_BOTTOM; else if (box2.Min.y > Max.y) box2Flags |= VXCLIP_TOP;
    if (ptFlags) {
        if (!box2Flags) {
            if (box2.Min.x>=Min.x && box2.Min.y>=Min.y && box2.Min.z>=Min.z &&
                box2.Max.x<=Max.x && box2.Max.y<=Max.y && box2.Max.z<=Max.z) return -1;
            if (Min.x>=box2.Min.x && Min.y>=box2.Min.y && Min.z>=box2.Min.z &&
                Max.x<=box2.Max.x && Max.y<=box2.Max.y && Max.z<=box2.Max.z) {
                if (pt.x<box2.Min.x || pt.x>box2.Max.x ||
                    pt.y<box2.Min.y || pt.y>box2.Max.y ||
                    pt.z<box2.Min.z || pt.z>box2.Max.z) return 1;
            }
        }
    } else {
        if (box2Flags) return -1;
        if (Min.x>=box2.Min.x && Min.y>=box2.Min.y && Min.z>=box2.Min.z &&
            Max.x<=box2.Max.x && Max.y<=box2.Max.y && Max.z<=box2.Max.z) return 1;
    }
    return 0;
}

inline void VxBbox::ClassifyVertices(const int iVcount, XBYTE *iVertices, XDWORD iStride, XDWORD *oFlags) const {
    const float maxX=Max.x, maxY=Max.y, maxZ=Max.z, minX=Min.x, minY=Min.y, minZ=Min.z;
    for (int i = 0; i < iVcount; i++) {
        const float *v = reinterpret_cast<const float *>(iVertices + i*iStride);
        XDWORD flag = 0;
        if (v[2] < minZ) flag |= VXCLIP_BACK; else if (v[2] > maxZ) flag |= VXCLIP_FRONT;
        if (v[1] < minY) flag |= VXCLIP_BOTTOM; else if (v[1] > maxY) flag |= VXCLIP_TOP;
        if (v[0] < minX) flag |= VXCLIP_LEFT; else if (v[0] > maxX) flag |= VXCLIP_RIGHT;
        oFlags[i] = flag;
    }
}

inline void VxBbox::ClassifyVerticesOneAxis(const int iVcount, XBYTE *iVertices, XDWORD iStride, const int iAxis, XDWORD *oFlags) const {
    if (iAxis < 0 || iAxis > 2) { for (int i=0;i<iVcount;i++) oFlags[i]=0; return; }
    const float maxVal = *(&Max.x + iAxis), minVal = *(&Min.x + iAxis);
    for (int i = 0; i < iVcount; i++) {
        const float *v = reinterpret_cast<const float *>(iVertices + i*iStride + iAxis*sizeof(float));
        XDWORD flag = 0;
        if (*v < minVal) flag = 1; else if (*v > maxVal) flag = 2;
        oFlags[i] = flag;
    }
}

inline void VxBbox::TransformTo(VxVector *pts, const VxMatrix &Mat) const {
    Vx3DMultiplyMatrixVector(&pts[0], Mat, &Min);
    const float sizeX=Max.x-Min.x, sizeY=Max.y-Min.y, sizeZ=Max.z-Min.z;
    const VxVector xVec(sizeX*Mat[0][0], sizeX*Mat[0][1], sizeX*Mat[0][2]);
    const VxVector yVec(sizeY*Mat[1][0], sizeY*Mat[1][1], sizeY*Mat[1][2]);
    const VxVector zVec(sizeZ*Mat[2][0], sizeZ*Mat[2][1], sizeZ*Mat[2][2]);
    pts[1] = pts[0] + zVec;
    pts[2] = pts[0] + yVec;
    pts[3] = pts[2] + zVec;
    pts[4] = pts[0] + xVec;
    pts[5] = pts[4] + zVec;
    pts[6] = pts[4] + yVec;
    pts[7] = pts[6] + zVec;
}

inline void VxBbox::TransformFrom(const VxBbox &sbox, const VxMatrix &Mat) {
    VxVector center((sbox.Min.x+sbox.Max.x)*0.5f, (sbox.Min.y+sbox.Max.y)*0.5f, (sbox.Min.z+sbox.Max.z)*0.5f);
    Vx3DMultiplyMatrixVector(&Min, Mat, &center);
    const float sizeX=sbox.Max.x-sbox.Min.x, sizeY=sbox.Max.y-sbox.Min.y, sizeZ=sbox.Max.z-sbox.Min.z;
    const VxVector xVec(sizeX*Mat[0][0], sizeX*Mat[0][1], sizeX*Mat[0][2]);
    const VxVector yVec(sizeY*Mat[1][0], sizeY*Mat[1][1], sizeY*Mat[1][2]);
    const VxVector zVec(sizeZ*Mat[2][0], sizeZ*Mat[2][1], sizeZ*Mat[2][2]);
    const float halfX = (XAbs(xVec.x)+XAbs(yVec.x)+XAbs(zVec.x))*0.5f;
    const float halfY = (XAbs(xVec.y)+XAbs(yVec.y)+XAbs(zVec.y))*0.5f;
    const float halfZ = (XAbs(xVec.z)+XAbs(yVec.z)+XAbs(zVec.z))*0.5f;
    Max.x = Min.x + halfX; Max.y = Min.y + halfY; Max.z = Min.z + halfZ;
    Min.x = Min.x - halfX; Min.y = Min.y - halfY; Min.z = Min.z - halfZ;
}

#endif // VXBBOX_IMPL
