#ifndef VXSPHERE_H
#define VXSPHERE_H

#include "VxVector.h"
#include "VxBbox.h"

/**
 * @brief Represents a 3D sphere, defined by a center and a radius.
 */
class VxSphere {
public:
    /// @brief Default constructor.
    VxSphere() {}

    /**
     * @brief Constructs a sphere with a given center and radius.
     * @param iCenter The center point of the sphere.
     * @param iRadius The radius of the sphere.
     */
    VxSphere(const VxVector &iCenter, const float iRadius) : m_Center(iCenter), m_Radius(iRadius) {}

    // Accessors
    /// @brief Gets a mutable reference to the sphere's center.
    VxVector &Center() { return m_Center; }
    /// @brief Gets a const reference to the sphere's center.
    const VxVector &Center() const { return m_Center; }
    /// @brief Gets a mutable reference to the sphere's radius.
    float &Radius() { return m_Radius; }
    /// @brief Gets a const reference to the sphere's radius.
    const float &Radius() const { return m_Radius; }

    /**
     * @brief Checks if a point is inside or on the surface of the sphere.
     * @param iPoint The point to check.
     * @return True if the point is inside or on the sphere, false otherwise.
     */
    bool IsPointInside(const VxVector &iPoint) {
        const double x = double(iPoint.x) - m_Center.x;
        const double y = double(iPoint.y) - m_Center.y;
        const double z = double(iPoint.z) - m_Center.z;
        return x * x + y * y + z * z <= double(m_Radius) * m_Radius;
    }

    /**
     * @brief Checks if an axis-aligned bounding box is completely contained within the sphere.
     * @param iBox The bounding box to check.
     * @return True if the box is totally inside the sphere, false otherwise.
     */
    bool IsBoxTotallyInside(const VxBbox &iBox) {
        const double minX = double(m_Center.x) - iBox.Min.x, maxX = double(m_Center.x) - iBox.Max.x;
        const double minY = double(m_Center.y) - iBox.Min.y, maxY = double(m_Center.y) - iBox.Max.y;
        const double minZ = double(m_Center.z) - iBox.Min.z, maxZ = double(m_Center.z) - iBox.Max.z;
        const double x2 = XMax(minX * minX, maxX * maxX);
        const double y2 = XMax(minY * minY, maxY * maxY);
        const double z2 = XMax(minZ * minZ, maxZ * maxZ);
        return x2 + y2 + z2 < double(m_Radius) * m_Radius;
    }

    /**
     * @brief Checks if a point lies exactly on the surface of the sphere.
     * @param iPoint The point to check.
     * @return True if the point is on the surface, false otherwise.
     */
    bool IsPointOnSurface(const VxVector &iPoint) {
        const double x = double(iPoint.x) - m_Center.x;
        const double y = double(iPoint.y) - m_Center.y;
        const double z = double(iPoint.z) - m_Center.z;
        return x * x + y * y + z * z == double(m_Radius) * m_Radius;
    }

    /// @brief Equality operator.
    bool operator==(const VxSphere &iSphere) const {
        return (m_Radius == iSphere.m_Radius) && (m_Center == iSphere.m_Center);
    }

protected:
    VxVector m_Center; ///< The center point of the sphere.
    float m_Radius;    ///< The radius of the sphere.
};

#endif // VXSPHERE_H
