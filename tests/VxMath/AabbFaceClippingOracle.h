#ifndef AABB_FACE_CLIPPING_ORACLE_H
#define AABB_FACE_CLIPPING_ORACLE_H

#include <cmath>

// Independent geometric oracle: clip the convex triangle polygon against the
// six closed box half-spaces. This uses no SAT axes, box diagonals, or engine
// intersection calls. Convex clipping adds at most one vertex per plane.
// Homogeneous intersections avoid division at each cut, preserving exact
// dyadic edge contacts that repeated Cartesian interpolation can round away.
static bool AabbFaceClippingOracle(const VxBbox &box, const VxVector &a, const VxVector &b, const VxVector &c) {
    double polygon[16][4] = {{a.x,a.y,a.z,1},{b.x,b.y,b.z,1},{c.x,c.y,c.z,1}};
    int count = 3;
    for (int plane = 0; plane < 6 && count != 0; ++plane) {
        const int axis = plane / 2;
        const bool upper = (plane & 1) != 0;
        const double bound = upper ? box.Max[axis] : box.Min[axis];
        double clipped[16][4]; int output = 0;
        for (int i = 0; i < count; ++i) {
            const double *previous = polygon[(i + count - 1) % count], *current = polygon[i];
            const double previousDistance = (previous[axis] - bound * previous[3]) * (upper ? 1 : -1);
            const double currentDistance = (current[axis] - bound * current[3]) * (upper ? 1 : -1);
            const bool previousInside = previousDistance <= 0, currentInside = currentDistance <= 0;
            if (previousInside != currentInside) {
                double largest = 0;
                for (int k = 0; k < 4; ++k) {
                    clipped[output][k] = currentDistance * previous[k] - previousDistance * current[k];
                    if (std::fabs(clipped[output][k]) > largest) largest = std::fabs(clipped[output][k]);
                }
                int exponent = 0; std::frexp(largest, &exponent);
                const double sign = clipped[output][3] < 0 ? -1 : 1;
                for (int k = 0; k < 4; ++k) clipped[output][k] = sign * std::ldexp(clipped[output][k], -exponent);
                ++output;
            }
            if (currentInside) {
                for (int k = 0; k < 4; ++k) clipped[output][k] = current[k];
                ++output;
            }
        }
        count = output;
        for (int i = 0; i < count; ++i) for (int k = 0; k < 4; ++k) polygon[i][k] = clipped[i][k];
    }
    return count != 0;
}
#endif
