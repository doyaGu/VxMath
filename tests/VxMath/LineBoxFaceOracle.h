#ifndef LINE_BOX_FACE_ORACLE_H
#define LINE_BOX_FACE_ORACLE_H
#include <cmath>

struct LineBoxFaceResult {
    int code = 0, nearAxis = -1, farAxis = -1;
    double nearT = 0, farT = 0;
    bool originalNearNormalDefined = false, originalFarNormalDefined = false;
    VxVector entry, exit, entryNormal, exitNormal;
};

// Enumerate the six box faces, retaining a candidate only when it lies within
// every other face's half-space. Compare homogeneous parameters before doing
// any division. This does not repeat the production running slab interval or
// the original center/cross-product SAT.
static LineBoxFaceResult LineBoxFaceOracle(const VxRay &ray, const VxBbox &box, bool detailed) {
    LineBoxFaceResult r;
    r.entryNormal.Set(0, 0, 0); r.exitNormal.Set(0, 0, 0);
    bool parallel[3], allParallel = true;
    for (int i = 0; i < 3; ++i) {
        parallel[i] = detailed ? std::fabs(ray.m_Direction[i]) < 0x1p-23f : ray.m_Direction[i] == 0;
        allParallel = allParallel && parallel[i];
        if (parallel[i] && (ray.m_Origin[i] < box.Min[i] || ray.m_Origin[i] > box.Max[i])) return r;
    }
    if (allParallel) {
        r.code = 1;
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
    bool nearMax = ray.m_Direction[r.nearAxis] < 0, farMax = ray.m_Direction[r.farAxis] > 0;
    r.code = 1;
    if (!detailed) return r;
    // Return actual crossings in increasing distance from origin. On an
    // equal-distance tie across origin preserve the original axis/Min-face choice.
    // Same-side endpoints follow signed order even when double quotients collapse.
    const int nearRank = 2*r.nearAxis + (ray.m_Direction[r.nearAxis] < 0);
    const int farRank = 2*r.farAxis + (ray.m_Direction[r.farAxis] > 0);
    if (r.nearT < 0 && (r.farT <= 0 || std::fabs(r.farT) < std::fabs(r.nearT) ||
        (std::fabs(r.farT) == std::fabs(r.nearT) && farRank < nearRank))) {
        const double parameter = r.nearT; r.nearT = r.farT; r.farT = parameter;
        const int axis = r.nearAxis; r.nearAxis = r.farAxis; r.farAxis = axis;
        const bool maximum = nearMax; nearMax = farMax; farMax = maximum;
    }
    for (int i = 0; i < 3; ++i) {
        r.entry[i] = float(double(ray.m_Origin[i]) + double(ray.m_Direction[i]) * r.nearT);
        r.exit[i] = float(double(ray.m_Origin[i]) + double(ray.m_Direction[i]) * r.farT);
    }
    r.entry[r.nearAxis] = nearMax ? box.Max[r.nearAxis] : box.Min[r.nearAxis];
    r.exit[r.farAxis] = farMax ? box.Max[r.farAxis] : box.Min[r.farAxis];
    r.entryNormal[r.nearAxis] = ray.m_Direction[r.nearAxis] > 0 ? -1.0f : 1.0f;
    r.exitNormal[r.farAxis] = ray.m_Direction[r.farAxis] > 0 ? -1.0f : 1.0f;
    // The original picks unrelated slab planes and has uninitialized normal
    // indices when no parameter beats its finite sentinel. Such pointers must
    // be omitted in the native probe; retain the return/point case itself.
    for (int i=0;i<3;++i) if (!parallel[i]) {
        const double a=(double(box.Min[i])-ray.m_Origin[i])/ray.m_Direction[i];
        const double b=(double(box.Max[i])-ray.m_Origin[i])/ray.m_Direction[i];
        r.originalNearNormalDefined = r.originalNearNormalDefined || std::fmin(std::fabs(a),std::fabs(b)) < double(1.0e30f);
        r.originalFarNormalDefined = r.originalFarNormalDefined || std::fmax(std::fabs(a),std::fabs(b)) < double(1.0e30f);
    }
    return r;
}
#endif
