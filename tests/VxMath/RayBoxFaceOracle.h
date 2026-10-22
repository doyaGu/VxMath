#ifndef RAY_BOX_FACE_ORACLE_H
#define RAY_BOX_FACE_ORACLE_H
#include <cmath>

struct RayBoxFaceResult {
    int code = 0, nearAxis = -1, farAxis = -1;
    double nearT = 0, farT = 0;
    bool originalNearNormalDefined = false, originalFarNormalDefined = false;
    VxVector entry, exit, entryNormal, exitNormal;
};

// Enumerate the six box faces, retaining a candidate only when it lies within
// every other face's half-space. Compare homogeneous parameters before doing
// any division. This does not repeat the production running slab interval or
// the original center/cross-product SAT.
static RayBoxFaceResult RayBoxFaceOracle(const VxRay &ray, const VxBbox &box, bool detailed) {
    RayBoxFaceResult r;
    r.entryNormal.Set(0, 0, 0); r.exitNormal.Set(0, 0, 0);
    bool parallel[3], allParallel = true;
    for (int i = 0; i < 3; ++i) {
        parallel[i] = detailed ? std::fabs(ray.m_Direction[i]) < 0x1p-23f : ray.m_Direction[i] == 0;
        allParallel = allParallel && parallel[i];
        if (parallel[i] && (ray.m_Origin[i] < box.Min[i] || ray.m_Origin[i] > box.Max[i])) return r;
    }
    if (allParallel) {
        r.code = detailed ? -1 : 1;
        r.entry = r.exit = ray.m_Origin;
        return r;
    }
    double firstN = 0, firstD = 1, lastN = 0, lastD = 1;
    for (int face = 0; face < 6; ++face) {
        const int axis = face / 2;
        if (parallel[axis]) continue;
        const double direction = ray.m_Direction[axis], denominator = std::fabs(direction);
        const double bound = (face & 1) ? box.Max[axis] : box.Min[axis];
        const double numerator = (bound - ray.m_Origin[axis]) * (direction < 0 ? -1 : 1);
        bool inside = true;
        for (int j = 0; j < 3; ++j) {
            if (j == axis || parallel[j]) continue;
            const double movement = double(ray.m_Direction[j]) * numerator;
            const double aboveMin = (double(ray.m_Origin[j]) - box.Min[j]) * denominator + movement;
            const double belowMax = (double(box.Max[j]) - ray.m_Origin[j]) * denominator - movement;
            if (aboveMin < 0 || belowMax < 0) inside = false;
        }
        if (!inside) continue;
        const bool entering = direction > 0 ? !(face & 1) : (face & 1) != 0;
        if (entering && (r.nearAxis < 0 || numerator * firstD < firstN * denominator)) {
            firstN = numerator; firstD = denominator; r.nearAxis = axis;
        }
        if (!entering && (r.farAxis < 0 || numerator * lastD > lastN * denominator)) {
            lastN = numerator; lastD = denominator; r.farAxis = axis;
        }
    }
    if (r.nearAxis < 0) return r;
    r.nearT = firstN / firstD; r.farT = lastN / lastD;
    if (!detailed) { r.code = lastN >= 0; return r; }
    if (r.farT < 0x1p-23) return r;
    r.code = r.nearT <= 0x1p-23 ? -1 : 1;
    for (int i = 0; i < 3; ++i) {
        r.entry[i] = float(double(ray.m_Origin[i]) + double(ray.m_Direction[i]) * r.nearT);
        r.exit[i] = float(double(ray.m_Origin[i]) + double(ray.m_Direction[i]) * r.farT);
    }
    r.entry[r.nearAxis] = ray.m_Direction[r.nearAxis] > 0 ? box.Min[r.nearAxis] : box.Max[r.nearAxis];
    r.exit[r.farAxis] = ray.m_Direction[r.farAxis] > 0 ? box.Max[r.farAxis] : box.Min[r.farAxis];
    r.entryNormal[r.nearAxis] = ray.m_Direction[r.nearAxis] > 0 ? -1.0f : 1.0f;
    r.exitNormal[r.farAxis] = ray.m_Direction[r.farAxis] > 0 ? -1.0f : 1.0f;
    // Original normals are uninitialized if its finite sentinel never moves.
    r.originalNearNormalDefined = r.nearT > -double(1.0e30f);
    r.originalFarNormalDefined = r.farT < double(1.0e30f);
    return r;
}
#endif
