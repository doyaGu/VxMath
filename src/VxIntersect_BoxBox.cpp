#include "VxIntersect.h"

#include "VxVector.h"
#include "VxMatrix.h"
#include "VxRay.h"
#include "VxOBB.h"
#include "VxSIMD.h"

constexpr float kHugeT = 1.0e30f;
constexpr float kSegmentMaxT = 1.0f;

#if defined(VX_SIMD_SSE)
static inline __m128 VxSIMDLoadVector3(const VxVector& v) {
    return VxSIMDLoadFloat3(&v.x);
}

static inline __m128 VxSIMDAbs3(__m128 v) {
    return _mm_and_ps(v, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
}

static inline float VxSIMDDot3Scalar(__m128 a, __m128 b) {
    return _mm_cvtss_f32(VxSIMDDotProduct3(a, b));
}

static inline XBOOL PointInAABB_OutCode_SIMD(const VxBbox& box, const VxVector& p, unsigned int& outCode) {
    const __m128 pV = VxSIMDLoadVector3(p);
    const __m128 minV = VxSIMDLoadVector3(box.Min);
    const __m128 maxV = VxSIMDLoadVector3(box.Max);

    const int lessMask = _mm_movemask_ps(_mm_cmplt_ps(pV, minV)) & 0x7;
    const int geMask = _mm_movemask_ps(_mm_cmpnlt_ps(pV, maxV)) & 0x7;

    unsigned int code = 0;
    if ((lessMask & 0x1) != 0) {
        code |= 0x02;
    } else if ((geMask & 0x1) != 0) {
        code |= 0x01;
    }

    if ((lessMask & 0x2) != 0) {
        code |= 0x08;
    } else if ((geMask & 0x2) != 0) {
        code |= 0x04;
    }

    if ((lessMask & 0x4) != 0) {
        code |= 0x10;
    } else if ((geMask & 0x4) != 0) {
        code |= 0x20;
    }

    outCode = code;
    return code == 0;
}

static inline float VxSIMDAbsDotDiff3(const VxVector& a, const VxVector& b, const VxVector& n) {
    const __m128 diff = _mm_sub_ps(VxSIMDLoadVector3(a), VxSIMDLoadVector3(b));
    return _mm_cvtss_f32(VxSIMDAbs3(VxSIMDDotProduct3(diff, VxSIMDLoadVector3(n))));
}

struct VxSlabSIMDPrecompute {
    float tMin[4];
    float tMax[4];
    int parallelMask;
    int outsideParallelMask;
};

static inline VxSlabSIMDPrecompute VxSIMDPrecomputeSlabs(const VxVector& origin, const VxVector& direction, const VxBbox& box) {
    VxSlabSIMDPrecompute out{};
    const __m128 originV = VxSIMDLoadVector3(origin);
    const __m128 dirV = VxSIMDLoadVector3(direction);
    const __m128 minV = VxSIMDLoadVector3(box.Min);
    const __m128 maxV = VxSIMDLoadVector3(box.Max);
    const __m128 absDir = VxSIMDAbs3(dirV);
    const __m128 epsV = _mm_set1_ps(EPSILON);
    const __m128 parallel = _mm_cmplt_ps(absDir, epsV);
    const __m128 outside = _mm_or_ps(_mm_cmplt_ps(originV, minV), _mm_cmpgt_ps(originV, maxV));
    const __m128 parallelOutside = _mm_and_ps(parallel, outside);

    out.parallelMask = _mm_movemask_ps(parallel) & 0x7;
    out.outsideParallelMask = _mm_movemask_ps(parallelOutside) & 0x7;

    const __m128 invDir = _mm_div_ps(_mm_set1_ps(1.0f), dirV);
    const __m128 tA = _mm_mul_ps(_mm_sub_ps(minV, originV), invDir);
    const __m128 tB = _mm_mul_ps(_mm_sub_ps(maxV, originV), invDir);
    _mm_storeu_ps(out.tMin, _mm_min_ps(tA, tB));
    _mm_storeu_ps(out.tMax, _mm_max_ps(tA, tB));
    return out;
}
#endif

static inline XBOOL SlabContainsParallel(float origin, float minV, float maxV) {
    // Ground-truth treats slab bounds as inclusive when direction is ~0.
    return origin >= minV && origin <= maxV;
}

static inline float OppositeDirSign(float dir) {
    // Stores normals opposite to ray/segment direction sign
    return (dir >= 0.0f) ? -1.0f : 1.0f;
}

static inline XBOOL PointInAABB_OutCode(const VxBbox &box, const VxVector &p, unsigned int &outCode) {
    // Encodes outside-ness:
    // X: <Min -> 0x02, >=Max -> 0x01
    // Y: <Min -> 0x08, >=Max -> 0x04
    // Z: <Min -> 0x10, >=Max -> 0x20
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

//----------- Boxes

// Intersection Ray - Box (simple boolean version)
XBOOL VxIntersect::RayBox(const VxRay &ray, const VxBbox &box) {
#if defined(VX_SIMD_SSE)
    const __m128 boxMin = VxSIMDLoadVector3(box.Min);
    const __m128 boxMax = VxSIMDLoadVector3(box.Max);
    const __m128 half = _mm_mul_ps(_mm_sub_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 center = _mm_mul_ps(_mm_add_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 origin = VxSIMDLoadVector3(ray.m_Origin);
    const __m128 direction = VxSIMDLoadVector3(ray.m_Direction);
    const __m128 p = _mm_sub_ps(origin, center);

    const __m128 outside = _mm_cmpgt_ps(VxSIMDAbs3(p), half);
    const __m128 away = _mm_cmpge_ps(_mm_mul_ps(p, direction), _mm_setzero_ps());
    const __m128 reject = _mm_and_ps(outside, away);
    if ((_mm_movemask_ps(reject) & 0x7) != 0) {
        return FALSE;
    }

    alignas(16) float crossVals[4];
    alignas(16) float absDirVals[4];
    alignas(16) float halfVals[4];
    _mm_store_ps(crossVals, VxSIMDAbs3(VxSIMDCrossProduct3(p, direction)));
    _mm_store_ps(absDirVals, VxSIMDAbs3(direction));
    _mm_store_ps(halfVals, half);

    if (crossVals[0] > absDirVals[2] * halfVals[1] + absDirVals[1] * halfVals[2]) return FALSE;
    if (crossVals[1] > absDirVals[0] * halfVals[2] + absDirVals[2] * halfVals[0]) return FALSE;
    if (crossVals[2] > absDirVals[0] * halfVals[1] + absDirVals[1] * halfVals[0]) return FALSE;

    return TRUE;
#else
    // Get box center and half-extents
    float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;
    
    VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    // Vector from box center to ray origin
    float px = ray.m_Origin.x - boxCenter.x;
    float py = ray.m_Origin.y - boxCenter.y;
    float pz = ray.m_Origin.z - boxCenter.z;

    // Test if ray origin is outside box on any axis and heading away
    if (!(XAbs(px) <= boxHalfX)) {
        if (px * ray.m_Direction.x >= 0.0f) return FALSE;
    }
    if (!(XAbs(py) <= boxHalfY)) {
        if (py * ray.m_Direction.y >= 0.0f) return FALSE;
    }
    if (!(XAbs(pz) <= boxHalfZ)) {
        if (pz * ray.m_Direction.z >= 0.0f) return FALSE;
    }

    // Test separating axes from cross products (p x Direction)
    float crossX = pz * ray.m_Direction.y - py * ray.m_Direction.z;
    float crossY = px * ray.m_Direction.z - pz * ray.m_Direction.x;
    float crossZ = py * ray.m_Direction.x - px * ray.m_Direction.y;

    float absDirY = XAbs(ray.m_Direction.y);
    float absDirZ = XAbs(ray.m_Direction.z);
    
    if (XAbs(crossX) > absDirZ * boxHalfY + absDirY * boxHalfZ) return FALSE;
    
    float absDirX = XAbs(ray.m_Direction.x);
    
    if (XAbs(crossY) > absDirX * boxHalfZ + absDirZ * boxHalfX) return FALSE;
    if (XAbs(crossZ) > absDirX * boxHalfY + absDirY * boxHalfX) return FALSE;

    return TRUE;
#endif
}

// Intersection Ray - Box (detailed version with intersection points and normals)
int VxIntersect::RayBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal) {
#if defined(VX_SIMD_SSE)
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    const VxSlabSIMDPrecompute pre = VxSIMDPrecomputeSlabs(ray.m_Origin, ray.m_Direction, box);
    if (pre.outsideParallelMask != 0) {
        return 0;
    }

    for (int i = 0; i < 3; ++i) {
        if ((pre.parallelMask & (1 << i)) != 0) {
            continue;
        }

        float t1 = pre.tMin[i];
        float t2 = pre.tMax[i];

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(ray.m_Direction[i]);
        }

        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(ray.m_Direction[i]);
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
#else
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float rayDir = ray.m_Direction[i];
        const float rayOrigin = ray.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (fabsf(rayDir) < EPSILON) {
            if (!SlabContainsParallel(rayOrigin, boxMin, boxMax))
                return 0;
            continue;
        }

        const float invDir = 1.0f / rayDir;
        float t1 = (boxMin - rayOrigin) * invDir;
        float t2 = (boxMax - rayOrigin) * invDir;

        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(rayDir);
        }

        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(rayDir);
        }

        if (tNear > tFar || tFar < EPSILON)
            return 0;
    }

    inpoint = ray.m_Origin + ray.m_Direction * tNear;
    if (outpoint)
        *outpoint = ray.m_Origin + ray.m_Direction * tFar;

    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }

    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }

    return (tNear <= EPSILON) ? -1 : 1;
#endif
}

// Intersection Segment - Box (simple boolean version)
XBOOL VxIntersect::SegmentBox(const VxRay &segment, const VxBbox &box) {
#if defined(VX_SIMD_SSE)
    const __m128 boxMin = VxSIMDLoadVector3(box.Min);
    const __m128 boxMax = VxSIMDLoadVector3(box.Max);
    const __m128 half = _mm_mul_ps(_mm_sub_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 center = _mm_mul_ps(_mm_add_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 origin = VxSIMDLoadVector3(segment.m_Origin);
    const __m128 segHalf = _mm_mul_ps(VxSIMDLoadVector3(segment.m_Direction), _mm_set1_ps(0.5f));
    const __m128 segCenter = _mm_add_ps(origin, segHalf);
    const __m128 d = _mm_sub_ps(segCenter, center);

    const __m128 sepAxis = _mm_cmpgt_ps(VxSIMDAbs3(d), _mm_add_ps(VxSIMDAbs3(segHalf), half));
    if ((_mm_movemask_ps(sepAxis) & 0x7) != 0) {
        return FALSE;
    }

    alignas(16) float crossVals[4];
    alignas(16) float absHalfVals[4];
    alignas(16) float halfVals[4];
    _mm_store_ps(crossVals, VxSIMDAbs3(VxSIMDCrossProduct3(d, segHalf)));
    _mm_store_ps(absHalfVals, VxSIMDAbs3(segHalf));
    _mm_store_ps(halfVals, half);

    if (crossVals[0] > absHalfVals[2] * halfVals[1] + absHalfVals[1] * halfVals[2]) return FALSE;
    if (crossVals[1] > absHalfVals[2] * halfVals[0] + absHalfVals[0] * halfVals[2]) return FALSE;
    if (crossVals[2] > absHalfVals[1] * halfVals[0] + absHalfVals[0] * halfVals[1]) return FALSE;

    return TRUE;
#else
    // Get box center and half-extents
    float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;

    // Segment half-vector
    float segHalfX = segment.m_Direction.x * 0.5f;
    float segHalfY = segment.m_Direction.y * 0.5f;
    float segHalfZ = segment.m_Direction.z * 0.5f;
    
    // Segment center
    float segCenterX = segHalfX + segment.m_Origin.x;
    float segCenterY = segHalfY + segment.m_Origin.y;
    float segCenterZ = segHalfZ + segment.m_Origin.z;
    
    VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    // Vector from box center to segment center
    float dx = segCenterX - boxCenter.x;
    float dy = segCenterY - boxCenter.y;
    float dz = segCenterZ - boxCenter.z;

    // Test separating axes
    float absSegHalfX = XAbs(segHalfX);
    if (XAbs(dx) > absSegHalfX + boxHalfX) return FALSE;
    
    float absSegHalfY = XAbs(segHalfY);
    if (XAbs(dy) > absSegHalfY + boxHalfY) return FALSE;
    
    float absSegHalfZ = XAbs(segHalfZ);
    if (XAbs(dz) > absSegHalfZ + boxHalfZ) return FALSE;

    // Test cross product axes (d x Direction)
    float crossY = segHalfZ * dx - dz * segHalfX;
    float crossZ = dy * segHalfX - segHalfY * dx;
    
    // Cross X = dz * segHalfY - segHalfZ * dy
    if (XAbs(dz * segHalfY - segHalfZ * dy) > absSegHalfZ * boxHalfY + absSegHalfY * boxHalfZ)
        return FALSE;
    if (XAbs(crossY) > absSegHalfZ * boxHalfX + absSegHalfX * boxHalfZ)
        return FALSE;
    if (XAbs(crossZ) > absSegHalfY * boxHalfX + absSegHalfX * boxHalfY)
        return FALSE;

    return TRUE;
#endif
}

// Intersection Segment - Box (detailed version)
int VxIntersect::SegmentBox(const VxRay &segment, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal) {
#if defined(VX_SIMD_SSE)
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    const VxSlabSIMDPrecompute pre = VxSIMDPrecomputeSlabs(segment.m_Origin, segment.m_Direction, box);
    if (pre.outsideParallelMask != 0) {
        return 0;
    }

    for (int i = 0; i < 3; ++i) {
        if ((pre.parallelMask & (1 << i)) != 0) {
            continue;
        }

        float t1 = pre.tMin[i];
        float t2 = pre.tMax[i];

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(segment.m_Direction[i]);
        }

        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(segment.m_Direction[i]);
        }

        if (tNear > tFar || tFar < EPSILON) {
            return 0;
        }
    }

    if (tNear > kSegmentMaxT) {
        return 0;
    }
    if (tNear < EPSILON && tFar > kSegmentMaxT) {
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
#else
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float segDir = segment.m_Direction[i];
        const float segOrigin = segment.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (fabsf(segDir) < EPSILON) {
            if (!SlabContainsParallel(segOrigin, boxMin, boxMax))
                return 0;
            continue;
        }

        const float invDir = 1.0f / segDir;
        float t1 = (boxMin - segOrigin) * invDir;
        float t2 = (boxMax - segOrigin) * invDir;

        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (t1 > tNear) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(segDir);
        }

        if (t2 < tFar) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(segDir);
        }

        if (tNear > tFar || tFar < EPSILON)
            return 0;
    }

    // Segment parameter range is [0, 1].
    if (tNear > kSegmentMaxT)
        return 0;
    if (tNear < EPSILON && tFar > kSegmentMaxT)
        return 0;

    inpoint = segment.m_Origin + segment.m_Direction * tNear;
    if (outpoint)
        *outpoint = segment.m_Origin + segment.m_Direction * tFar;

    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }

    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }

    return (tNear <= EPSILON) ? -1 : 1;
#endif
}

// Intersection Line - Box (simple boolean version)
XBOOL VxIntersect::LineBox(const VxRay &line, const VxBbox &box) {
#if defined(VX_SIMD_SSE)
    const __m128 boxMin = VxSIMDLoadVector3(box.Min);
    const __m128 boxMax = VxSIMDLoadVector3(box.Max);
    const __m128 half = _mm_mul_ps(_mm_sub_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 center = _mm_mul_ps(_mm_add_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 origin = VxSIMDLoadVector3(line.m_Origin);
    const __m128 direction = VxSIMDLoadVector3(line.m_Direction);
    const __m128 d = _mm_sub_ps(origin, center);

    alignas(16) float crossVals[4];
    alignas(16) float absDirVals[4];
    alignas(16) float halfVals[4];
    _mm_store_ps(crossVals, VxSIMDAbs3(VxSIMDCrossProduct3(d, direction)));
    _mm_store_ps(absDirVals, VxSIMDAbs3(direction));
    _mm_store_ps(halfVals, half);

    if (crossVals[0] > absDirVals[2] * halfVals[1] + absDirVals[1] * halfVals[2]) return FALSE;
    if (crossVals[1] > absDirVals[0] * halfVals[2] + absDirVals[2] * halfVals[0]) return FALSE;
    if (crossVals[2] > absDirVals[0] * halfVals[1] + absDirVals[1] * halfVals[0]) return FALSE;

    return TRUE;
#else
    // Get box center and half-extents
    float boxHalfX = (box.Max.x - box.Min.x) * 0.5f;
    float boxHalfY = (box.Max.y - box.Min.y) * 0.5f;
    float boxHalfZ = (box.Max.z - box.Min.z) * 0.5f;
    
    VxVector boxCenter = (box.Max + box.Min) * 0.5f;

    // Vector from box center to line origin
    float dx = line.m_Origin.x - boxCenter.x;
    float dy = line.m_Origin.y - boxCenter.y;
    float dz = line.m_Origin.z - boxCenter.z;

    // Test separating axes from cross products (d x Direction)
    float crossX = dz * line.m_Direction.y - dy * line.m_Direction.z;
    float crossY = dx * line.m_Direction.z - dz * line.m_Direction.x;
    float crossZ = dy * line.m_Direction.x - dx * line.m_Direction.y;

    float absDirY = XAbs(line.m_Direction.y);
    float absDirZ = XAbs(line.m_Direction.z);
    
    if (XAbs(crossX) > absDirZ * boxHalfY + absDirY * boxHalfZ)
        return FALSE;
    
    float absDirX = XAbs(line.m_Direction.x);
    
    if (XAbs(crossY) > absDirX * boxHalfZ + absDirZ * boxHalfX)
        return FALSE;
    if (XAbs(crossZ) > absDirX * boxHalfY + absDirY * boxHalfX)
        return FALSE;

    return TRUE;
#endif
}

// Intersection Line - Box (detailed version)
int VxIntersect::LineBox(const VxRay &line, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal) {
#if defined(VX_SIMD_SSE)
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    const VxSlabSIMDPrecompute pre = VxSIMDPrecomputeSlabs(line.m_Origin, line.m_Direction, box);
    if (pre.outsideParallelMask != 0) {
        return 0;
    }

    for (int i = 0; i < 3; ++i) {
        if ((pre.parallelMask & (1 << i)) != 0) {
            continue;
        }

        float t1 = pre.tMin[i];
        float t2 = pre.tMax[i];

        // Orders the slab parameters by absolute value for lines.
        if (fabsf(t2) < fabsf(t1)) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (fabsf(t1) < fabsf(tNear)) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(line.m_Direction[i]);
        }

        if (fabsf(t2) < fabsf(tFar)) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(line.m_Direction[i]);
        }

        if (fabsf(tFar) < fabsf(tNear)) {
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
#else
    float tNear = -kHugeT;
    float tFar = kHugeT;
    int nearAxis = 0;
    float nearSign = -1.0f;
    int farAxis = 0;
    float farSign = 1.0f;

    for (int i = 0; i < 3; ++i) {
        const float dir = line.m_Direction[i];
        const float origin = line.m_Origin[i];
        const float boxMin = box.Min[i];
        const float boxMax = box.Max[i];

        if (fabsf(dir) < EPSILON) {
            if (!SlabContainsParallel(origin, boxMin, boxMax))
                return 0;
            continue;
        }

        const float invDir = 1.0f / dir;
        float t1 = (boxMin - origin) * invDir;
        float t2 = (boxMax - origin) * invDir;

        // Orders the slab parameters by absolute value for lines.
        if (fabsf(t2) < fabsf(t1)) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }

        if (fabsf(t1) < fabsf(tNear)) {
            tNear = t1;
            nearAxis = i;
            nearSign = OppositeDirSign(dir);
        }

        if (fabsf(t2) < fabsf(tFar)) {
            tFar = t2;
            farAxis = i;
            farSign = OppositeDirSign(dir);
        }

        if (fabsf(tFar) < fabsf(tNear))
            return 0;
    }

    inpoint = line.m_Origin + line.m_Direction * tNear;
    if (outpoint)
        *outpoint = line.m_Origin + line.m_Direction * tFar;

    if (innormal) {
        *innormal = VxVector::axis0();
        (*innormal)[nearAxis] = nearSign;
    }

    if (outnormal) {
        *outnormal = VxVector::axis0();
        (*outnormal)[farAxis] = farSign;
    }

    return 1;
#endif
}

// Intersection Box - Box
XBOOL VxIntersect::AABBAABB(const VxBbox &box1, const VxBbox &box2) {
#if defined(VX_SIMD_SSE)
    const __m128 min1 = VxSIMDLoadVector3(box1.Min);
    const __m128 max1 = VxSIMDLoadVector3(box1.Max);
    const __m128 min2 = VxSIMDLoadVector3(box2.Min);
    const __m128 max2 = VxSIMDLoadVector3(box2.Max);

    const __m128 minLeMax = _mm_cmple_ps(min1, max2);
    const __m128 maxGeMin = _mm_cmpge_ps(max1, min2);
    const __m128 overlap = _mm_and_ps(minLeMax, maxGeMin);
    return ((_mm_movemask_ps(overlap) & 0x7) == 0x7) ? TRUE : FALSE;
#else
    return (box1.Min.x <= box2.Max.x) && (box1.Max.x >= box2.Min.x) &&
           (box1.Min.y <= box2.Max.y) && (box1.Max.y >= box2.Min.y) &&
           (box1.Min.z <= box2.Max.z) && (box1.Max.z >= box2.Min.z);
#endif
}

// AABB - OBB intersection
XBOOL VxIntersect::AABBOBB(const VxBbox &box1, const VxOBB &box2) {
#if defined(VX_SIMD_SSE)
    const __m128 boxMin = VxSIMDLoadVector3(box1.Min);
    const __m128 boxMax = VxSIMDLoadVector3(box1.Max);
    const __m128 boxCenter = _mm_mul_ps(_mm_add_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 aabbHalf = _mm_mul_ps(_mm_sub_ps(boxMax, boxMin), _mm_set1_ps(0.5f));
    const __m128 tVec = _mm_sub_ps(VxSIMDLoadVector3(box2.m_Center), boxCenter);

    alignas(16) float halfVals[4];
    alignas(16) float tVals[4];
    _mm_store_ps(halfVals, aabbHalf);
    _mm_store_ps(tVals, tVec);

    const float aabbHalfX = halfVals[0];
    const float aabbHalfY = halfVals[1];
    const float aabbHalfZ = halfVals[2];
    const float Tx = tVals[0];
    const float Ty = tVals[1];
    const float Tz = tVals[2];

    const __m128 axis0 = VxSIMDLoadVector3(box2.m_Axis[0]);
    const __m128 axis1 = VxSIMDLoadVector3(box2.m_Axis[1]);
    const __m128 axis2 = VxSIMDLoadVector3(box2.m_Axis[2]);
    const __m128 absAxis0 = VxSIMDAbs3(axis0);
    const __m128 absAxis1 = VxSIMDAbs3(axis1);
    const __m128 absAxis2 = VxSIMDAbs3(axis2);
    const __m128 ext2 = VxSIMDLoadVector3(box2.m_Extents);

    const __m128 row0 = _mm_setr_ps(box2.m_Axis[0].x, box2.m_Axis[1].x, box2.m_Axis[2].x, 0.0f);
    const __m128 row1 = _mm_setr_ps(box2.m_Axis[0].y, box2.m_Axis[1].y, box2.m_Axis[2].y, 0.0f);
    const __m128 row2 = _mm_setr_ps(box2.m_Axis[0].z, box2.m_Axis[1].z, box2.m_Axis[2].z, 0.0f);
    const __m128 absRow0 = VxSIMDAbs3(row0);
    const __m128 absRow1 = VxSIMDAbs3(row1);
    const __m128 absRow2 = VxSIMDAbs3(row2);

    if (XAbs(Tx) > aabbHalfX + VxSIMDDot3Scalar(absRow0, ext2))
        return FALSE;
    if (XAbs(Ty) > aabbHalfY + VxSIMDDot3Scalar(absRow1, ext2))
        return FALSE;
    if (XAbs(Tz) > aabbHalfZ + VxSIMDDot3Scalar(absRow2, ext2))
        return FALSE;

    if (XAbs(VxSIMDDot3Scalar(tVec, axis0)) > box2.m_Extents.x + VxSIMDDot3Scalar(absAxis0, aabbHalf))
        return FALSE;
    if (XAbs(VxSIMDDot3Scalar(tVec, axis1)) > box2.m_Extents.y + VxSIMDDot3Scalar(absAxis1, aabbHalf))
        return FALSE;
    if (XAbs(VxSIMDDot3Scalar(tVec, axis2)) > box2.m_Extents.z + VxSIMDDot3Scalar(absAxis2, aabbHalf))
        return FALSE;

    // OBB axis components (kept as scalars for cross-axis SAT tests)
    float R00 = box2.m_Axis[0].x, R01 = box2.m_Axis[1].x, R02 = box2.m_Axis[2].x;
    float R10 = box2.m_Axis[0].y, R11 = box2.m_Axis[1].y, R12 = box2.m_Axis[2].y;
    float R20 = box2.m_Axis[0].z, R21 = box2.m_Axis[1].z, R22 = box2.m_Axis[2].z;

    float absR00 = XAbs(R00), absR01 = XAbs(R01), absR02 = XAbs(R02);
    float absR10 = XAbs(R10), absR11 = XAbs(R11), absR12 = XAbs(R12);
    float absR20 = XAbs(R20), absR21 = XAbs(R21), absR22 = XAbs(R22);
#else
    // Get AABB center and half-extents
    float aabbHalfX = (box1.Max.x - box1.Min.x) * 0.5f;
    float aabbHalfY = (box1.Max.y - box1.Min.y) * 0.5f;
    float aabbHalfZ = (box1.Max.z - box1.Min.z) * 0.5f;
    
    VxVector aabbCenter = (box1.Max + box1.Min) * 0.5f;

    // Vector from AABB center to OBB center
    float Tx = box2.m_Center.x - aabbCenter.x;
    float Ty = box2.m_Center.y - aabbCenter.y;
    float Tz = box2.m_Center.z - aabbCenter.z;

    // OBB axis components
    float R00 = box2.m_Axis[0].x, R01 = box2.m_Axis[1].x, R02 = box2.m_Axis[2].x;
    float R10 = box2.m_Axis[0].y, R11 = box2.m_Axis[1].y, R12 = box2.m_Axis[2].y;
    float R20 = box2.m_Axis[0].z, R21 = box2.m_Axis[1].z, R22 = box2.m_Axis[2].z;

    float absR00 = XAbs(R00), absR01 = XAbs(R01), absR02 = XAbs(R02);
    float absR10 = XAbs(R10), absR11 = XAbs(R11), absR12 = XAbs(R12);
    float absR20 = XAbs(R20), absR21 = XAbs(R21), absR22 = XAbs(R22);

    // Test AABB X axis
    if (XAbs(Tx) > aabbHalfX + absR00 * box2.m_Extents.x + absR01 * box2.m_Extents.y + absR02 * box2.m_Extents.z)
        return FALSE;

    // Test AABB Y axis
    if (XAbs(Ty) > aabbHalfY + absR10 * box2.m_Extents.x + absR11 * box2.m_Extents.y + absR12 * box2.m_Extents.z)
        return FALSE;

    // Test AABB Z axis
    if (XAbs(Tz) > aabbHalfZ + absR20 * box2.m_Extents.x + absR21 * box2.m_Extents.y + absR22 * box2.m_Extents.z)
        return FALSE;

    // Test OBB axis 0
    if (XAbs(Tx * R00 + Ty * R10 + Tz * R20) > box2.m_Extents.x + absR00 * aabbHalfX + absR10 * aabbHalfY + absR20 * aabbHalfZ)
        return FALSE;

    // Test OBB axis 1
    if (XAbs(Tx * R01 + Ty * R11 + Tz * R21) > box2.m_Extents.y + absR01 * aabbHalfX + absR11 * aabbHalfY + absR21 * aabbHalfZ)
        return FALSE;

    // Test OBB axis 2
    if (XAbs(Tx * R02 + Ty * R12 + Tz * R22) > box2.m_Extents.z + absR02 * aabbHalfX + absR12 * aabbHalfY + absR22 * aabbHalfZ)
        return FALSE;
#endif

    // Test cross product axes
    // AABB X axis x OBB axis 0
    if (XAbs(Tz * R10 - R20 * Ty) > absR20 * aabbHalfY + absR10 * aabbHalfZ + absR02 * box2.m_Extents.y + absR01 * box2.m_Extents.z)
        return FALSE;

    // AABB X axis x OBB axis 1
    if (XAbs(Tz * R11 - R21 * Ty) > absR21 * aabbHalfY + absR11 * aabbHalfZ + absR00 * box2.m_Extents.z + absR02 * box2.m_Extents.x)
        return FALSE;

    // AABB X axis x OBB axis 2
    if (XAbs(Tz * R12 - R22 * Ty) > absR22 * aabbHalfY + absR12 * aabbHalfZ + absR00 * box2.m_Extents.y + absR01 * box2.m_Extents.x)
        return FALSE;

    // AABB Y axis x OBB axis 0
    if (XAbs(R20 * Tx - Tz * R00) > absR20 * aabbHalfX + absR00 * aabbHalfZ + absR12 * box2.m_Extents.y + absR11 * box2.m_Extents.z)
        return FALSE;

    // AABB Y axis x OBB axis 1
    if (XAbs(R21 * Tx - Tz * R01) > absR10 * box2.m_Extents.z + absR12 * box2.m_Extents.x + absR21 * aabbHalfX + absR01 * aabbHalfZ)
        return FALSE;

    // AABB Y axis x OBB axis 2
    if (XAbs(R22 * Tx - Tz * R02) > absR10 * box2.m_Extents.y + absR11 * box2.m_Extents.x + absR22 * aabbHalfX + absR02 * aabbHalfZ)
        return FALSE;

    // AABB Z axis x OBB axis 0
    if (XAbs(Ty * R00 - R10 * Tx) > absR10 * aabbHalfX + absR00 * aabbHalfY + absR22 * box2.m_Extents.y + absR21 * box2.m_Extents.z)
        return FALSE;

    // AABB Z axis x OBB axis 1
    if (XAbs(Ty * R01 - R11 * Tx) > absR11 * aabbHalfX + absR01 * aabbHalfY + absR20 * box2.m_Extents.z + absR22 * box2.m_Extents.x)
        return FALSE;

    // AABB Z axis x OBB axis 2
    if (XAbs(Ty * R02 - R12 * Tx) > absR12 * aabbHalfX + absR02 * aabbHalfY + absR20 * box2.m_Extents.y + absR21 * box2.m_Extents.x)
        return FALSE;

    return TRUE;
}

// OBB - OBB intersection using SAT (Separating Axis Theorem)
XBOOL VxIntersect::OBBOBB(const VxOBB &box1, const VxOBB &box2) {
#if defined(VX_SIMD_SSE)
    const __m128 centerDelta = _mm_sub_ps(VxSIMDLoadVector3(box2.m_Center), VxSIMDLoadVector3(box1.m_Center));
    const __m128 axisA0 = VxSIMDLoadVector3(box1.m_Axis[0]);
    const __m128 axisA1 = VxSIMDLoadVector3(box1.m_Axis[1]);
    const __m128 axisA2 = VxSIMDLoadVector3(box1.m_Axis[2]);
    const __m128 axisB0 = VxSIMDLoadVector3(box2.m_Axis[0]);
    const __m128 axisB1 = VxSIMDLoadVector3(box2.m_Axis[1]);
    const __m128 axisB2 = VxSIMDLoadVector3(box2.m_Axis[2]);
    const __m128 ext1 = VxSIMDLoadVector3(box1.m_Extents);
    const __m128 ext2 = VxSIMDLoadVector3(box2.m_Extents);

    alignas(16) float tComp[4];
    _mm_store_ps(tComp, centerDelta);
    float Tx = tComp[0];
    float Ty = tComp[1];
    float Tz = tComp[2];

    float R00 = VxSIMDDot3Scalar(axisA0, axisB0);
    float R01 = VxSIMDDot3Scalar(axisA0, axisB1);
    float R02 = VxSIMDDot3Scalar(axisA0, axisB2);
    float R10 = VxSIMDDot3Scalar(axisA1, axisB0);
    float R11 = VxSIMDDot3Scalar(axisA1, axisB1);
    float R12 = VxSIMDDot3Scalar(axisA1, axisB2);
    float R20 = VxSIMDDot3Scalar(axisA2, axisB0);
    float R21 = VxSIMDDot3Scalar(axisA2, axisB1);
    float R22 = VxSIMDDot3Scalar(axisA2, axisB2);

    float T0 = VxSIMDDot3Scalar(centerDelta, axisA0);
    float T1 = VxSIMDDot3Scalar(centerDelta, axisA1);
    float T2 = VxSIMDDot3Scalar(centerDelta, axisA2);

    float absR00 = XAbs(R00);
    float absR01 = XAbs(R01);
    float absR02 = XAbs(R02);
    float absR10 = XAbs(R10);
    float absR11 = XAbs(R11);
    float absR12 = XAbs(R12);
    float absR20 = XAbs(R20);
    float absR21 = XAbs(R21);
    float absR22 = XAbs(R22);

    const __m128 rowA0 = _mm_setr_ps(absR00, absR01, absR02, 0.0f);
    const __m128 rowA1 = _mm_setr_ps(absR10, absR11, absR12, 0.0f);
    const __m128 rowA2 = _mm_setr_ps(absR20, absR21, absR22, 0.0f);
    if (XAbs(T0) > box1.m_Extents.x + VxSIMDDot3Scalar(rowA0, ext2))
        return FALSE;
    if (XAbs(T1) > box1.m_Extents.y + VxSIMDDot3Scalar(rowA1, ext2))
        return FALSE;
    if (XAbs(T2) > box1.m_Extents.z + VxSIMDDot3Scalar(rowA2, ext2))
        return FALSE;

    const __m128 colB0 = _mm_setr_ps(absR00, absR10, absR20, 0.0f);
    const __m128 colB1 = _mm_setr_ps(absR01, absR11, absR21, 0.0f);
    const __m128 colB2 = _mm_setr_ps(absR02, absR12, absR22, 0.0f);
    if (XAbs(VxSIMDDot3Scalar(centerDelta, axisB0)) > box2.m_Extents.x + VxSIMDDot3Scalar(colB0, ext1))
        return FALSE;
    if (XAbs(VxSIMDDot3Scalar(centerDelta, axisB1)) > box2.m_Extents.y + VxSIMDDot3Scalar(colB1, ext1))
        return FALSE;
    if (XAbs(VxSIMDDot3Scalar(centerDelta, axisB2)) > box2.m_Extents.z + VxSIMDDot3Scalar(colB2, ext1))
        return FALSE;
#else
    // Translation vector between box centers
    float Tx = box2.m_Center.x - box1.m_Center.x;
    float Ty = box2.m_Center.y - box1.m_Center.y;
    float Tz = box2.m_Center.z - box1.m_Center.z;

    // Rotation matrix from box1 to box2 coordinate system
    // R[i][j] = dot(box1.Axis[i], box2.Axis[j])
    float R00 = box1.m_Axis[0].x * box2.m_Axis[0].x + box1.m_Axis[0].y * box2.m_Axis[0].y + box1.m_Axis[0].z * box2.m_Axis[0].z;
    float R01 = box1.m_Axis[0].x * box2.m_Axis[1].x + box1.m_Axis[0].y * box2.m_Axis[1].y + box1.m_Axis[0].z * box2.m_Axis[1].z;
    float R02 = box1.m_Axis[0].x * box2.m_Axis[2].x + box1.m_Axis[0].y * box2.m_Axis[2].y + box1.m_Axis[0].z * box2.m_Axis[2].z;

    // T in box1's coordinate frame for axis A0
    float T0 = Tx * box1.m_Axis[0].x + Ty * box1.m_Axis[0].y + Tz * box1.m_Axis[0].z;
    float absR00 = XAbs(R00);
    float absR01 = XAbs(R01);
    float absR02 = XAbs(R02);

    // Test axis A0
    if (XAbs(T0) > box1.m_Extents.x + absR00 * box2.m_Extents.x + absR01 * box2.m_Extents.y + absR02 * box2.m_Extents.z)
        return FALSE;

    float R10 = box1.m_Axis[1].x * box2.m_Axis[0].x + box1.m_Axis[1].y * box2.m_Axis[0].y + box1.m_Axis[1].z * box2.m_Axis[0].z;
    float R11 = box1.m_Axis[1].x * box2.m_Axis[1].x + box1.m_Axis[1].y * box2.m_Axis[1].y + box1.m_Axis[1].z * box2.m_Axis[1].z;
    float R12 = box1.m_Axis[1].x * box2.m_Axis[2].x + box1.m_Axis[1].y * box2.m_Axis[2].y + box1.m_Axis[1].z * box2.m_Axis[2].z;

    // T in box1's coordinate frame for axis A1
    float T1 = Tx * box1.m_Axis[1].x + Ty * box1.m_Axis[1].y + Tz * box1.m_Axis[1].z;
    float absR10 = XAbs(R10);
    float absR11 = XAbs(R11);
    float absR12 = XAbs(R12);

    // Test axis A1
    if (XAbs(T1) > box1.m_Extents.y + absR10 * box2.m_Extents.x + absR11 * box2.m_Extents.y + absR12 * box2.m_Extents.z)
        return FALSE;

    float R20 = box1.m_Axis[2].x * box2.m_Axis[0].x + box1.m_Axis[2].y * box2.m_Axis[0].y + box1.m_Axis[2].z * box2.m_Axis[0].z;
    float R21 = box1.m_Axis[2].x * box2.m_Axis[1].x + box1.m_Axis[2].y * box2.m_Axis[1].y + box1.m_Axis[2].z * box2.m_Axis[1].z;
    float R22 = box1.m_Axis[2].x * box2.m_Axis[2].x + box1.m_Axis[2].y * box2.m_Axis[2].y + box1.m_Axis[2].z * box2.m_Axis[2].z;

    // T in box1's coordinate frame for axis A2
    float T2 = Tx * box1.m_Axis[2].x + Ty * box1.m_Axis[2].y + Tz * box1.m_Axis[2].z;
    float absR20 = XAbs(R20);
    float absR21 = XAbs(R21);
    float absR22 = XAbs(R22);

    // Test axis A2
    if (XAbs(T2) > box1.m_Extents.z + absR20 * box2.m_Extents.x + absR21 * box2.m_Extents.y + absR22 * box2.m_Extents.z)
        return FALSE;

    // Test axis B0
    if (XAbs(Tx * box2.m_Axis[0].x + Ty * box2.m_Axis[0].y + Tz * box2.m_Axis[0].z) >
        box2.m_Extents.x + absR00 * box1.m_Extents.x + absR10 * box1.m_Extents.y + absR20 * box1.m_Extents.z)
        return FALSE;

    // Test axis B1
    if (XAbs(Tx * box2.m_Axis[1].x + Ty * box2.m_Axis[1].y + Tz * box2.m_Axis[1].z) >
        box2.m_Extents.y + absR01 * box1.m_Extents.x + absR11 * box1.m_Extents.y + absR21 * box1.m_Extents.z)
        return FALSE;

    // Test axis B2
    if (XAbs(Tx * box2.m_Axis[2].x + Ty * box2.m_Axis[2].y + Tz * box2.m_Axis[2].z) >
        box2.m_Extents.z + absR02 * box1.m_Extents.x + absR12 * box1.m_Extents.y + absR22 * box1.m_Extents.z)
        return FALSE;
#endif

    // Test axis A0 x B0
    if (XAbs(T2 * R10 - R20 * T1) > absR20 * box1.m_Extents.y + absR10 * box1.m_Extents.z + absR02 * box2.m_Extents.y + absR01 * box2.m_Extents.z)
        return FALSE;

    // Test axis A0 x B1
    if (XAbs(T2 * R11 - R21 * T1) > absR21 * box1.m_Extents.y + absR11 * box1.m_Extents.z + absR02 * box2.m_Extents.x + absR00 * box2.m_Extents.z)
        return FALSE;

    // Test axis A0 x B2
    if (XAbs(T2 * R12 - R22 * T1) > absR22 * box1.m_Extents.y + absR12 * box1.m_Extents.z + absR01 * box2.m_Extents.x + absR00 * box2.m_Extents.y)
        return FALSE;

    // Test axis A1 x B0
    if (XAbs(R20 * T0 - T2 * R00) > absR20 * box1.m_Extents.x + absR00 * box1.m_Extents.z + absR12 * box2.m_Extents.y + absR11 * box2.m_Extents.z)
        return FALSE;

    // Test axis A1 x B1
    if (XAbs(R21 * T0 - T2 * R01) > absR21 * box1.m_Extents.x + absR01 * box1.m_Extents.z + absR12 * box2.m_Extents.x + absR10 * box2.m_Extents.z)
        return FALSE;

    // Test axis A1 x B2
    if (XAbs(R22 * T0 - T2 * R02) > absR22 * box1.m_Extents.x + absR02 * box1.m_Extents.z + absR11 * box2.m_Extents.x + absR10 * box2.m_Extents.y)
        return FALSE;

    // Test axis A2 x B0
    if (XAbs(T1 * R00 - R10 * T0) > absR10 * box1.m_Extents.x + absR00 * box1.m_Extents.y + absR22 * box2.m_Extents.y + absR21 * box2.m_Extents.z)
        return FALSE;

    // Test axis A2 x B1
    if (XAbs(T1 * R01 - R11 * T0) > absR11 * box1.m_Extents.x + absR01 * box1.m_Extents.y + absR22 * box2.m_Extents.x + absR20 * box2.m_Extents.z)
        return FALSE;

    // Test axis A2 x B2
    if (XAbs(T1 * R02 - R12 * T0) > absR12 * box1.m_Extents.x + absR02 * box1.m_Extents.y + absR21 * box2.m_Extents.x + absR20 * box2.m_Extents.y)
        return FALSE;

    // No separating axis found, boxes must intersect
    return TRUE;
}

// AABB - Face (triangle) intersection
XBOOL VxIntersect::AABBFace(const VxBbox &box, const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N) {
    // Implements a hybrid of trivial rejection + triangle-edge vs box
    // tests, and finally tests the most "planar" box diagonal vs the triangle.

    unsigned int code0 = 0, code1 = 0, code2 = 0;
#if defined(VX_SIMD_SSE)
    if (PointInAABB_OutCode_SIMD(box, A0, code0))
        return TRUE;
    if (PointInAABB_OutCode_SIMD(box, A1, code1))
        return TRUE;
    if (PointInAABB_OutCode_SIMD(box, A2, code2))
        return TRUE;
#else
    if (PointInAABB_OutCode(box, A0, code0))
        return TRUE;
    if (PointInAABB_OutCode(box, A1, code1))
        return TRUE;
    if (PointInAABB_OutCode(box, A2, code2))
        return TRUE;
#endif

    // Trivial reject: all vertices outside the same slab half-space.
    if ((code0 & code1 & code2) != 0)
        return FALSE;

    // Test triangle edges against box.
    VxRay edge;
    edge.m_Origin = A0;
    edge.m_Direction = A1 - A0;
    if (SegmentBox(edge, box))
        return TRUE;

    edge.m_Origin = A1;
    edge.m_Direction = A2 - A1;
    if (SegmentBox(edge, box))
        return TRUE;

    edge.m_Origin = A0;
    edge.m_Direction = A2 - A0;
    if (SegmentBox(edge, box))
        return TRUE;

    // Build box corners in the same order as the original.
    const VxVector corners[8] = {
        VxVector(box.Min.x, box.Min.y, box.Min.z), // 0
        VxVector(box.Min.x, box.Min.y, box.Max.z), // 1
        VxVector(box.Min.x, box.Max.y, box.Min.z), // 2
        VxVector(box.Min.x, box.Max.y, box.Max.z), // 3
        VxVector(box.Max.x, box.Min.y, box.Min.z), // 4
        VxVector(box.Max.x, box.Min.y, box.Max.z), // 5
        VxVector(box.Max.x, box.Max.y, box.Min.z), // 6
        VxVector(box.Max.x, box.Max.y, box.Max.z)  // 7
    };

    // Choose among the 4 body diagonals the one whose direction is most
    // orthogonal to the triangle normal (min |dot(diagonal, N)|).
    int bestA = 0;
    int bestB = 7;
#if defined(VX_SIMD_SSE)
    float bestScore = VxSIMDAbsDotDiff3(corners[bestA], corners[bestB], N);
#else
    float bestScore = fabsf(DotProduct(corners[bestA] - corners[bestB], N));
#endif

    {
        const int a = 1, b = 6;
#if defined(VX_SIMD_SSE)
        const float s = VxSIMDAbsDotDiff3(corners[a], corners[b], N);
#else
        const float s = fabsf(DotProduct(corners[a] - corners[b], N));
#endif
        if (s < bestScore) {
            bestScore = s;
            bestA = a;
            bestB = b;
        }
    }
    {
        const int a = 2, b = 5;
#if defined(VX_SIMD_SSE)
        const float s = VxSIMDAbsDotDiff3(corners[a], corners[b], N);
#else
        const float s = fabsf(DotProduct(corners[a] - corners[b], N));
#endif
        if (s < bestScore) {
            bestScore = s;
            bestA = a;
            bestB = b;
        }
    }
    {
        const int a = 3, b = 4;
#if defined(VX_SIMD_SSE)
        const float s = VxSIMDAbsDotDiff3(corners[a], corners[b], N);
#else
        const float s = fabsf(DotProduct(corners[a] - corners[b], N));
#endif
        if (s < bestScore) {
            bestScore = s;
            bestA = a;
            bestB = b;
        }
    }

    // Test the chosen diagonal against the triangle.
    VxRay diag;
    diag.m_Origin = corners[bestA];
    diag.m_Direction = corners[bestB] - corners[bestA];
    VxVector hit;
    hit.z = 0.0f;
    float dist = bestScore;
    return SegmentFace(diag, A0, A1, A2, N, hit, dist) ? TRUE : FALSE;
}
