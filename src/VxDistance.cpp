#include "VxDistance.h"

#include <atomic>
#include <float.h>

#include "VxRay.h"
#include "VxAtomic.h"
#include "VxSIMD.h"
#include "VxSIMDDispatchInternal.h"
#include "VxVector.h"

namespace {

struct RayPairTerms {
    VxVector diff;
    float a;   // |dir0|^2
    float b;   // -dot(dir0, dir1)
    float c;   // |dir1|^2
    float d;   // dot(diff, dir0)
    float e;   // -dot(diff, dir1)
    float f;   // |diff|^2
    float det; // |a*c - b*b|
};

static inline RayPairTerms MakeRayPairTermsScalar(const VxRay& r0, const VxRay& r1) {
    RayPairTerms t;
    t.diff = r0.m_Origin - r1.m_Origin;
    t.a = SquareMagnitude(r0.m_Direction);
    t.b = -DotProduct(r0.m_Direction, r1.m_Direction);
    t.c = SquareMagnitude(r1.m_Direction);
    t.d = DotProduct(t.diff, r0.m_Direction);
    t.e = -DotProduct(t.diff, r1.m_Direction);
    t.f = SquareMagnitude(t.diff);
    t.det = XAbs(t.c * t.a - t.b * t.b);
    return t;
}

#if defined(VX_SIMD_SSE)
static inline float VxSIMDExtractX(__m128 v) {
    return _mm_cvtss_f32(v);
}

static inline RayPairTerms MakeRayPairTermsSIMD(const VxRay& r0, const VxRay& r1) {
    RayPairTerms t{};

    const __m128 origin0 = VxSIMDLoadFloat3(&r0.m_Origin.x);
    const __m128 origin1 = VxSIMDLoadFloat3(&r1.m_Origin.x);
    const __m128 dir0 = VxSIMDLoadFloat3(&r0.m_Direction.x);
    const __m128 dir1 = VxSIMDLoadFloat3(&r1.m_Direction.x);
    const __m128 diff = VxSIMDSubtract3(origin0, origin1);

    const __m128 a = VxSIMDDotProduct3(dir0, dir0);
    const __m128 b = VxSIMDDotProduct3(dir0, dir1);
    const __m128 c = VxSIMDDotProduct3(dir1, dir1);
    const __m128 d = VxSIMDDotProduct3(diff, dir0);
    const __m128 e = VxSIMDDotProduct3(diff, dir1);
    const __m128 f = VxSIMDDotProduct3(diff, diff);

    VxSIMDStoreFloat3(&t.diff.x, diff);
    t.a = VxSIMDExtractX(a);
    t.b = -VxSIMDExtractX(b);
    t.c = VxSIMDExtractX(c);
    t.d = VxSIMDExtractX(d);
    t.e = -VxSIMDExtractX(e);
    t.f = VxSIMDExtractX(f);
    t.det = XAbs(t.c * t.a - t.b * t.b);
    return t;
}

static float LineLineSquareDistanceSIMDImpl(const VxRay &line0, const VxRay &line1, float *t0, float *t1) {
    const __m128d wXY = _mm_sub_pd(_mm_setr_pd(line0.m_Origin.x, line0.m_Origin.y),
                                  _mm_setr_pd(line1.m_Origin.x, line1.m_Origin.y));
    const double wx = _mm_cvtsd_f64(wXY);
    const double wy = _mm_cvtsd_f64(_mm_unpackhi_pd(wXY, wXY));
    const double wz = double(line0.m_Origin.z) - line1.m_Origin.z;
    const double ux = line0.m_Direction.x, uy = line0.m_Direction.y, uz = line0.m_Direction.z;
    const double vx = line1.m_Direction.x, vy = line1.m_Direction.y, vz = line1.m_Direction.z;
    const __m128d nXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(vz, vx)),
                                  _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(vy, vz)));
    const double nz = ux * vy - uy * vx;
    const __m128d nSquareXY = _mm_mul_pd(nXY, nXY);
    const double denominator = _mm_cvtsd_f64(_mm_add_sd(nSquareXY, _mm_unpackhi_pd(nSquareXY, nSquareXY))) + nz * nz;
    double s, t, square;
    if (denominator == 0.0) {
        const __m128d uXY = _mm_setr_pd(ux, uy);
        const __m128d lengthXY = _mm_mul_pd(uXY, uXY);
        const __m128d dotXY = _mm_mul_pd(wXY, uXY);
        const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + uz * uz;
        s = -(_mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + wz * uz) / length;
        t = 0.0;
        const __m128d cXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(wy, wz), _mm_setr_pd(uz, ux)),
                                      _mm_mul_pd(_mm_setr_pd(wz, wx), _mm_setr_pd(uy, uz)));
        const double cz = wx * uy - wy * ux;
        const __m128d cSquareXY = _mm_mul_pd(cXY, cXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(cSquareXY, _mm_unpackhi_pd(cSquareXY, cSquareXY))) + cz * cz) / length;
    } else {
        const __m128d vCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(vy, vz), _mm_setr_pd(wz, wx)),
                                           _mm_mul_pd(_mm_setr_pd(vz, vx), _mm_setr_pd(wy, wz)));
        const __m128d uCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(wz, wx)),
                                           _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(wy, wz)));
        const __m128d sXY = _mm_mul_pd(vCrossWXY, nXY);
        const __m128d tXY = _mm_mul_pd(uCrossWXY, nXY);
        s = (_mm_cvtsd_f64(_mm_add_sd(sXY, _mm_unpackhi_pd(sXY, sXY))) + (vx * wy - vy * wx) * nz) / denominator;
        t = (_mm_cvtsd_f64(_mm_add_sd(tXY, _mm_unpackhi_pd(tXY, tXY))) + (ux * wy - uy * wx) * nz) / denominator;
        const __m128d heightXY = _mm_mul_pd(wXY, nXY);
        const double height = _mm_cvtsd_f64(_mm_add_sd(heightXY, _mm_unpackhi_pd(heightXY, heightXY))) + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float LineRaySquareDistanceSIMDImpl(const VxRay &line, const VxRay &ray, float *t0, float *t1) {
    const __m128d wXY = _mm_sub_pd(_mm_setr_pd(line.m_Origin.x, line.m_Origin.y),
                                  _mm_setr_pd(ray.m_Origin.x, ray.m_Origin.y));
    const double wx = _mm_cvtsd_f64(wXY);
    const double wy = _mm_cvtsd_f64(_mm_unpackhi_pd(wXY, wXY));
    const double wz = double(line.m_Origin.z) - ray.m_Origin.z;
    const double ux = line.m_Direction.x, uy = line.m_Direction.y, uz = line.m_Direction.z;
    const double vx = ray.m_Direction.x, vy = ray.m_Direction.y, vz = ray.m_Direction.z;
    const __m128d nXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(vz, vx)),
                                  _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(vy, vz)));
    const double nz = ux * vy - uy * vx;
    const __m128d nSquareXY = _mm_mul_pd(nXY, nXY);
    const double denominator = _mm_cvtsd_f64(_mm_add_sd(nSquareXY, _mm_unpackhi_pd(nSquareXY, nSquareXY))) + nz * nz;
    const __m128d uCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(wz, wx)),
                                       _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(wy, wz)));
    const __m128d tXY = _mm_mul_pd(uCrossWXY, nXY);
    const double rayNumerator = _mm_cvtsd_f64(_mm_add_sd(tXY, _mm_unpackhi_pd(tXY, tXY))) + (ux * wy - uy * wx) * nz;
    double s, t, square;
    if (denominator == 0.0 || rayNumerator < 0.0) {
        const __m128d uXY = _mm_setr_pd(ux, uy);
        const __m128d lengthXY = _mm_mul_pd(uXY, uXY);
        const __m128d dotXY = _mm_mul_pd(wXY, uXY);
        const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + uz * uz;
        s = -(_mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + wz * uz) / length;
        t = 0.0;
        const __m128d cXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(wy, wz), _mm_setr_pd(uz, ux)),
                                      _mm_mul_pd(_mm_setr_pd(wz, wx), _mm_setr_pd(uy, uz)));
        const double cz = wx * uy - wy * ux;
        const __m128d cSquareXY = _mm_mul_pd(cXY, cXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(cSquareXY, _mm_unpackhi_pd(cSquareXY, cSquareXY))) + cz * cz) / length;
    } else {
        const __m128d vCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(vy, vz), _mm_setr_pd(wz, wx)),
                                           _mm_mul_pd(_mm_setr_pd(vz, vx), _mm_setr_pd(wy, wz)));
        const __m128d sXY = _mm_mul_pd(vCrossWXY, nXY);
        s = (_mm_cvtsd_f64(_mm_add_sd(sXY, _mm_unpackhi_pd(sXY, sXY))) + (vx * wy - vy * wx) * nz) / denominator;
        t = rayNumerator / denominator;
        const __m128d heightXY = _mm_mul_pd(wXY, nXY);
        const double height = _mm_cvtsd_f64(_mm_add_sd(heightXY, _mm_unpackhi_pd(heightXY, heightXY))) + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float PointLineSquareDistanceSIMDImpl(const VxVector &point, const VxRay &line, float *t0) {
    const __m128d diffXY = _mm_sub_pd(_mm_setr_pd(point.x, point.y),
                                     _mm_setr_pd(line.m_Origin.x, line.m_Origin.y));
    const __m128d dirXY = _mm_setr_pd(line.m_Direction.x, line.m_Direction.y);
    const double dx = _mm_cvtsd_f64(diffXY);
    const double dy = _mm_cvtsd_f64(_mm_unpackhi_pd(diffXY, diffXY));
    const double dz = double(point.z) - line.m_Origin.z;
    const double vx = line.m_Direction.x, vy = line.m_Direction.y, vz = line.m_Direction.z;
    const __m128d dotXY = _mm_mul_pd(diffXY, dirXY);
    const __m128d lengthXY = _mm_mul_pd(dirXY, dirXY);
    const double dot = _mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + dz * vz;
    const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + vz * vz;
    const double t = dot / length;

    // The cross-product form avoids subtracting a rounded closest point,
    // which can lose the entire perpendicular distance on a long line.
    const __m128d crossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(dy, dz), _mm_setr_pd(vz, vx)),
                                      _mm_mul_pd(_mm_setr_pd(dz, dx), _mm_setr_pd(vy, vz)));
    const double crossZ = dx * vy - dy * vx;
    const __m128d squareXY = _mm_mul_pd(crossXY, crossXY);
    const double square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + crossZ * crossZ;
    const float result = float(square / length);
    if (t0) *t0 = float(t);
    return result;
}

static float PointRaySquareDistanceSIMDImpl(const VxVector &point, const VxRay &ray, float *t0) {
    const __m128d diffXY = _mm_sub_pd(_mm_setr_pd(point.x, point.y),
                                     _mm_setr_pd(ray.m_Origin.x, ray.m_Origin.y));
    const __m128d dirXY = _mm_setr_pd(ray.m_Direction.x, ray.m_Direction.y);
    const double dx = _mm_cvtsd_f64(diffXY);
    const double dy = _mm_cvtsd_f64(_mm_unpackhi_pd(diffXY, diffXY));
    const double dz = double(point.z) - ray.m_Origin.z;
    const double vx = ray.m_Direction.x, vy = ray.m_Direction.y, vz = ray.m_Direction.z;
    const __m128d dotXY = _mm_mul_pd(diffXY, dirXY);
    const double dot = _mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + dz * vz;
    double t = 0.0, square;
    if (dot > 0.0) {
        const __m128d lengthXY = _mm_mul_pd(dirXY, dirXY);
        const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + vz * vz;
        t = dot / length;
        const __m128d crossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(dy, dz), _mm_setr_pd(vz, vx)),
                                          _mm_mul_pd(_mm_setr_pd(dz, dx), _mm_setr_pd(vy, vz)));
        const double crossZ = dx * vy - dy * vx;
        const __m128d squareXY = _mm_mul_pd(crossXY, crossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + crossZ * crossZ) / length;
    } else {
        // Zero direction also takes the original ray-origin branch.
        const __m128d squareXY = _mm_mul_pd(diffXY, diffXY);
        square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + dz * dz;
    }
    const float result = float(square);
    if (t0) *t0 = float(t);
    return result;
}

static float PointSegmentSquareDistanceSIMDImpl(const VxVector &point, const VxRay &segment, float *t0) {
    const __m128d diffXY = _mm_sub_pd(_mm_setr_pd(point.x, point.y),
                                     _mm_setr_pd(segment.m_Origin.x, segment.m_Origin.y));
    const __m128d dirXY = _mm_setr_pd(segment.m_Direction.x, segment.m_Direction.y);
    const double dx = _mm_cvtsd_f64(diffXY);
    const double dy = _mm_cvtsd_f64(_mm_unpackhi_pd(diffXY, diffXY));
    const double dz = double(point.z) - segment.m_Origin.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const __m128d dotXY = _mm_mul_pd(diffXY, dirXY);
    const double dot = _mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + dz * vz;
    double t = 0.0, square;
    if (dot > 0.0) {
        const __m128d lengthXY = _mm_mul_pd(dirXY, dirXY);
        const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + vz * vz;
        if (dot < length) {
            t = dot / length;
            const __m128d crossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(dy, dz), _mm_setr_pd(vz, vx)),
                                              _mm_mul_pd(_mm_setr_pd(dz, dx), _mm_setr_pd(vy, vz)));
            const double crossZ = dx * vy - dy * vx;
            const __m128d squareXY = _mm_mul_pd(crossXY, crossXY);
            square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + crossZ * crossZ) / length;
        } else {
            t = 1.0;
            const __m128d endXY = _mm_sub_pd(diffXY, dirXY);
            const double endZ = dz - vz;
            const __m128d squareXY = _mm_mul_pd(endXY, endXY);
            square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + endZ * endZ;
        }
    } else {
        // Zero direction also takes the original segment-origin branch.
        const __m128d squareXY = _mm_mul_pd(diffXY, diffXY);
        square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + dz * dz;
    }
    const float result = float(square);
    if (t0) *t0 = float(t);
    return result;
}

#endif

static float LineLineSquareDistanceScalarImpl(const VxRay &line0, const VxRay &line1, float *t0, float *t1) {
    const double wx = double(line0.m_Origin.x) - line1.m_Origin.x;
    const double wy = double(line0.m_Origin.y) - line1.m_Origin.y;
    const double wz = double(line0.m_Origin.z) - line1.m_Origin.z;
    const double ux = line0.m_Direction.x, uy = line0.m_Direction.y, uz = line0.m_Direction.z;
    const double vx = line1.m_Direction.x, vy = line1.m_Direction.y, vz = line1.m_Direction.z;
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double denominator = nx * nx + ny * ny + nz * nz;
    double s, t, square;
    if (denominator == 0.0) {
        // Preserve the original parallel convention: choose the second origin.
        const double length = ux * ux + uy * uy + uz * uz;
        s = -(wx * ux + wy * uy + wz * uz) / length;
        t = 0.0;
        const double cx = wy * uz - wz * uy;
        const double cy = wz * ux - wx * uz;
        const double cz = wx * uy - wy * ux;
        square = (cx * cx + cy * cy + cz * cz) / length;
    } else {
        // Cross products avoid the cancellation in |u|^2|v|^2-(u.v)^2
        // and in the expanded squared distance. No absolute scale threshold.
        s = ((vy * wz - vz * wy) * nx + (vz * wx - vx * wz) * ny + (vx * wy - vy * wx) * nz) / denominator;
        t = ((uy * wz - uz * wy) * nx + (uz * wx - ux * wz) * ny + (ux * wy - uy * wx) * nz) / denominator;
        const double height = wx * nx + wy * ny + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float LineRaySquareDistanceScalarImpl(const VxRay &line, const VxRay &ray, float *t0, float *t1) {
    const double wx = double(line.m_Origin.x) - ray.m_Origin.x;
    const double wy = double(line.m_Origin.y) - ray.m_Origin.y;
    const double wz = double(line.m_Origin.z) - ray.m_Origin.z;
    const double ux = line.m_Direction.x, uy = line.m_Direction.y, uz = line.m_Direction.z;
    const double vx = ray.m_Direction.x, vy = ray.m_Direction.y, vz = ray.m_Direction.z;
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double denominator = nx * nx + ny * ny + nz * nz;
    const double rayNumerator = (uy * wz - uz * wy) * nx + (uz * wx - ux * wz) * ny + (ux * wy - uy * wx) * nz;
    double s, t, square;
    if (denominator == 0.0 || rayNumerator < 0.0) {
        // Parallel lines and negative ray parameters select the ray origin.
        const double length = ux * ux + uy * uy + uz * uz;
        s = -(wx * ux + wy * uy + wz * uz) / length;
        t = 0.0;
        const double cx = wy * uz - wz * uy;
        const double cy = wz * ux - wx * uz;
        const double cz = wx * uy - wy * ux;
        square = (cx * cx + cy * cy + cz * cz) / length;
    } else {
        // Cross products avoid the cancellation in |u|^2|v|^2-(u.v)^2
        // and in the expanded squared distance. No absolute scale threshold.
        s = ((vy * wz - vz * wy) * nx + (vz * wx - vx * wz) * ny + (vx * wy - vy * wx) * nz) / denominator;
        t = rayNumerator / denominator;
        const double height = wx * nx + wy * ny + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float PointLineSquareDistanceScalarImpl(const VxVector &point, const VxRay &line, float *t0) {
    const double dx = double(point.x) - line.m_Origin.x;
    const double dy = double(point.y) - line.m_Origin.y;
    const double dz = double(point.z) - line.m_Origin.z;
    const double vx = line.m_Direction.x, vy = line.m_Direction.y, vz = line.m_Direction.z;
    const double length = vx * vx + vy * vy + vz * vz;
    const double t = (dx * vx + dy * vy + dz * vz) / length;
    const double crossX = dy * vz - dz * vy;
    const double crossY = dz * vx - dx * vz;
    const double crossZ = dx * vy - dy * vx;
    const float result = float((crossX * crossX + crossY * crossY + crossZ * crossZ) / length);
    if (t0) *t0 = float(t);
    return result;
}

static float PointRaySquareDistanceScalarImpl(const VxVector &point, const VxRay &ray, float *t0) {
    const double dx = double(point.x) - ray.m_Origin.x;
    const double dy = double(point.y) - ray.m_Origin.y;
    const double dz = double(point.z) - ray.m_Origin.z;
    const double vx = ray.m_Direction.x, vy = ray.m_Direction.y, vz = ray.m_Direction.z;
    const double dot = dx * vx + dy * vy + dz * vz;
    double t = 0.0, square;
    if (dot > 0.0) {
        const double length = vx * vx + vy * vy + vz * vz;
        t = dot / length;
        const double crossX = dy * vz - dz * vy;
        const double crossY = dz * vx - dx * vz;
        const double crossZ = dx * vy - dy * vx;
        square = (crossX * crossX + crossY * crossY + crossZ * crossZ) / length;
    } else {
        square = dx * dx + dy * dy + dz * dz;
    }
    const float result = float(square);
    if (t0) *t0 = float(t);
    return result;
}

static float PointSegmentSquareDistanceScalarImpl(const VxVector &point, const VxRay &segment, float *t0) {
    const double dx = double(point.x) - segment.m_Origin.x;
    const double dy = double(point.y) - segment.m_Origin.y;
    const double dz = double(point.z) - segment.m_Origin.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const double dot = dx * vx + dy * vy + dz * vz;
    double t = 0.0, square;
    if (dot > 0.0) {
        const double length = vx * vx + vy * vy + vz * vz;
        if (dot < length) {
            t = dot / length;
            const double crossX = dy * vz - dz * vy;
            const double crossY = dz * vx - dx * vz;
            const double crossZ = dx * vy - dy * vx;
            square = (crossX * crossX + crossY * crossY + crossZ * crossZ) / length;
        } else {
            t = 1.0;
            const double endX = dx - vx, endY = dy - vy, endZ = dz - vz;
            square = endX * endX + endY * endY + endZ * endZ;
        }
    } else {
        square = dx * dx + dy * dy + dz * dz;
    }
    const float result = float(square);
    if (t0) *t0 = float(t);
    return result;
}

typedef RayPairTerms (*MakeRayPairTermsDispatchFn)(const VxRay &, const VxRay &);
typedef float (*LineDistanceDispatchFn)(const VxRay &, const VxRay &, float *, float *);
typedef float (*PointDistanceDispatchFn)(const VxVector &, const VxRay &, float *);

struct VxDistanceDispatchTable {
    MakeRayPairTermsDispatchFn makeRayPairTerms;
    LineDistanceDispatchFn lineLineSquareDistance;
    LineDistanceDispatchFn lineRaySquareDistance;
    PointDistanceDispatchFn pointLineSquareDistance;
    PointDistanceDispatchFn pointRaySquareDistance;
    PointDistanceDispatchFn pointSegmentSquareDistance;
};

const VxDistanceDispatchTable kVxDistanceDispatchScalar = {
    MakeRayPairTermsScalar,
    LineLineSquareDistanceScalarImpl,
    LineRaySquareDistanceScalarImpl,
    PointLineSquareDistanceScalarImpl,
    PointRaySquareDistanceScalarImpl,
    PointSegmentSquareDistanceScalarImpl
};

#if defined(VX_SIMD_SSE)
const VxDistanceDispatchTable kVxDistanceDispatchSIMD = {
    MakeRayPairTermsSIMD,
    LineLineSquareDistanceSIMDImpl,
    LineRaySquareDistanceSIMDImpl,
    PointLineSquareDistanceSIMDImpl,
    PointRaySquareDistanceSIMDImpl,
    PointSegmentSquareDistanceSIMDImpl
};
#endif

std::atomic<const VxDistanceDispatchTable *> g_VxDistanceDispatch(&kVxDistanceDispatchScalar);

static inline const VxDistanceDispatchTable *GetVxDistanceDispatchTable() {
    return g_VxDistanceDispatch.load(std::memory_order_acquire);
}

static inline RayPairTerms MakeRayPairTerms(const VxRay& r0, const VxRay& r1) {
    return GetVxDistanceDispatchTable()->makeRayPairTerms(r0, r1);
}

}

void VxDistanceDispatchRebuild(int effectiveMode) {
    const bool useSIMD = (effectiveMode != VX_SIMD_MODE_NONE);
#if defined(VX_SIMD_SSE)
    const VxDistanceDispatchTable *next = useSIMD ? &kVxDistanceDispatchSIMD : &kVxDistanceDispatchScalar;
#else
    (void) useSIMD;
    const VxDistanceDispatchTable *next = &kVxDistanceDispatchScalar;
#endif
    g_VxDistanceDispatch.store(next, std::memory_order_release);
}

// Matches the exact quadratic expansion used in the original implementation.
static inline float DistanceQuadraticPart(const RayPairTerms& t, float s0, float s1) {
    return (s1 * t.c + s0 * t.b + t.e + t.e) * s1 + (s1 * t.b + s0 * t.a + t.d + t.d) * s0;
}

// Line-Line distance calculations

float VxDistance::LineLineSquareDistance(const VxRay &line0, const VxRay &line1, float *t0, float *t1) {
    return GetVxDistanceDispatchTable()->lineLineSquareDistance(line0, line1, t0, t1);
}

float VxDistance::LineRaySquareDistance(const VxRay &line, const VxRay &ray, float *t0, float *t1) {
    return GetVxDistanceDispatchTable()->lineRaySquareDistance(line, ray, t0, t1);
}

float VxDistance::LineSegmentSquareDistance(const VxRay &line, const VxRay &segment, float *t0, float *t1) {
    const RayPairTerms t = MakeRayPairTerms(line, segment);

    float s0;
    float s1;
    float quad;

    const float segParam = t.d * t.b - t.e * t.a;
    if (t.det < EPSILON || segParam < 0.0f) {
        // Parallel case or segment parameter negative - clamp to 0
        s1 = 0.0f;
        s0 = -(t.d / t.a);
        quad = t.d * s0;
        quad = quad + t.f;
    } else if (segParam > t.det) {
        // Segment parameter > 1 - clamp to 1
        const float dPlusB = t.d + t.b;
        s1 = 1.0f;
        s0 = -(dPlusB / t.a);
        quad = dPlusB * s0 + t.e + t.e + t.f + t.c;
    } else {
        const float invDet = 1.0f / t.det;
        s0 = (t.e * t.b - t.d * t.c) * invDet;
        s1 = invDet * segParam;
        quad = DistanceQuadraticPart(t, s0, s1) + t.f;
    }

    if (t0) *t0 = s0;
    if (t1) *t1 = s1;

    return XAbs(quad);
}

float VxDistance::RayRaySquareDistance(const VxRay &ray0, const VxRay &ray1, float *t0, float *t1) {
    const RayPairTerms t = MakeRayPairTerms(ray0, ray1);

    float s0 = 0.0f;
    float s1 = 0.0f;
    float f = t.f;

    if (t.det < EPSILON) {
        // Parallel case
        s1 = 0.0f;
        if (t.d < 0.0f) {
            s0 = -(t.d / t.a);
            f = f + s0 * t.d;
        } else {
            s0 = 0.0f;
            s1 = -(t.e / t.c);
            f = f + s1 * t.e;
        }
    } else {
        const float ray0Param = t.e * t.b - t.d * t.c;
        const float ray1Param = t.d * t.b - t.e * t.a;

        if (ray0Param < 0.0f) {
            // Clamp ray0 parameter to 0
            s0 = 0.0f;
            if (t.d < 0.0f) {
                // Closest point involves ray1 at t=0
                s1 = 0.0f;
                s0 = -(t.d / t.a);
                f = f + s0 * t.d;
            } else {
                // Optimize ray1 for ray0 at t=0
                if (t.e >= 0.0f) {
                    s1 = 0.0f;
                } else {
                    s1 = -(t.e / t.c);
                    f = f + s1 * t.e;
                }
            }
        } else if (ray1Param < 0.0f) {
            // Clamp ray1 parameter to 0
            s1 = 0.0f;
            if (t.d < 0.0f) {
                s0 = -(t.d / t.a);
                f = f + s0 * t.d;
            } else {
                s0 = 0.0f;
            }
        } else {
            // Both parameters in range
            const float invDet = 1.0f / t.det;
            s0 = ray0Param * invDet;
            s1 = ray1Param * invDet;
            f = f + DistanceQuadraticPart(t, s0, s1);
        }
    }

    if (t0) *t0 = s0;
    if (t1) *t1 = s1;

    return XAbs(f);
}

float VxDistance::RaySegmentSquareDistance(const VxRay &ray, const VxRay &segment, float *t0, float *t1) {
    const RayPairTerms t = MakeRayPairTerms(ray, segment);
    const float a = t.a;
    const float b = t.b;
    const float c = t.c;
    const float d = t.d;
    const float e = t.e;
    float f = t.f;
    const float det = t.det;

    float rayT = 0.0f;
    float segT = 0.0f;

    if (det < EPSILON) {
        // Parallel case
        if (b <= 0.0f) {
            // Evaluate with segment clamped at 1
            segT = 1.0f;
            const float dPlusB = d + b;
            if (dPlusB >= 0.0f) {
                rayT = 0.0f;
                f = f + e + e + c;
            } else {
                rayT = -(dPlusB / a);
                f = f + dPlusB * rayT + e + e + c;
            }
        } else {
            // Evaluate with segment clamped at 0
            segT = 0.0f;
            if (d < 0.0f) {
                rayT = -(d / a);
                f = f + rayT * d;
            } else {
                rayT = 0.0f;
            }
        }
    } else {
        const float rayParam = e * b - d * c;
        const float segParam = d * b - e * a;

        if (segParam < 0.0f) {
            // Clamp segment to 0
            segT = 0.0f;
            if (d < 0.0f) {
                rayT = -(d / a);
                f = f + rayT * d;
            } else {
                rayT = 0.0f;
            }
        } else if (segParam > det) {
            // Clamp segment to 1
            segT = 1.0f;
            if (-b <= d) {
                rayT = 0.0f;
                f = f + e + e + c;
            } else {
                const float dPlusB = d + b;
                rayT = -(dPlusB / a);
                f = f + dPlusB * rayT + e + e + c;
            }
        } else if (rayParam < 0.0f) {
            // Clamp ray to 0 and optimize segment parameter
            rayT = 0.0f;
            if (e >= 0.0f) {
                segT = 0.0f;
            } else if (-e > c) {
                segT = 1.0f;
                f = f + e + e + c;
            } else {
                segT = -(e / c);
                f = f + segT * e;
            }
        } else {
            // Both parameters in range
            const float invDet = 1.0f / det;
            rayT = rayParam * invDet;
            segT = segParam * invDet;
            f = f + DistanceQuadraticPart(t, rayT, segT);
        }
    }

    if (t0) *t0 = rayT;
    if (t1) *t1 = segT;

    return XAbs(f);
}

float VxDistance::SegmentSegmentSquareDistance(const VxRay &segment0, const VxRay &segment1, float *t0, float *t1) {
    const RayPairTerms t = MakeRayPairTerms(segment0, segment1);
    const float a = t.a;
    const float b = t.b;
    const float c = t.c;
    const float d = t.d;
    const float e = t.e;
    float f = t.f;
    const float det = t.det;

    float s0, s1;

    if (det < EPSILON) {
        // Parallel case: choose the minimum endpoint-to-segment distance.
        // This correctly handles collinear overlap (distance = 0) and disjoint parallel segments.

        const VxVector p0 = segment0.m_Origin;
        const VxVector q0 = segment0.m_Origin + segment0.m_Direction;
        const VxVector p1 = segment1.m_Origin;
        const VxVector q1 = segment1.m_Origin + segment1.m_Direction;

        float bestDist = FLT_MAX;
        float bestS0 = 0.0f;
        float bestS1 = 0.0f;

        // p0 -> segment1
        {
            float tParam = 0.0f;
            float distVal = PointSegmentSquareDistance(p0, segment1, &tParam);
            if (distVal < bestDist) {
                bestDist = distVal;
                bestS0 = 0.0f;
                bestS1 = tParam;
            }
        }

        // q0 -> segment1
        {
            float tParam = 0.0f;
            float distVal = PointSegmentSquareDistance(q0, segment1, &tParam);
            if (distVal < bestDist) {
                bestDist = distVal;
                bestS0 = 1.0f;
                bestS1 = tParam;
            }
        }

        // p1 -> segment0
        {
            float tParam = 0.0f;
            float distVal = PointSegmentSquareDistance(p1, segment0, &tParam);
            if (distVal < bestDist) {
                bestDist = distVal;
                bestS0 = tParam;
                bestS1 = 0.0f;
            }
        }

        // q1 -> segment0
        {
            float tParam = 0.0f;
            float distVal = PointSegmentSquareDistance(q1, segment0, &tParam);
            if (distVal < bestDist) {
                bestDist = distVal;
                bestS0 = tParam;
                bestS1 = 1.0f;
            }
        }

        s0 = bestS0;
        s1 = bestS1;
        f = bestDist;
    } else {
        // Non-parallel case
        float seg0Param = e * b - d * c;
        float seg1Param = d * b - e * a;

        if (seg0Param < 0.0f) {
            // seg0 parameter < 0
            if (seg1Param < 0.0f) {
                // Both < 0
                if (d < 0.0f) {
                    s1 = 0.0f;
                    if (-d >= a) {
                        s0 = 1.0f;
                        f = f + d + d + a;
                    } else {
                        s0 = -(d / a);
                        f = f + s0 * d;
                    }
                } else {
                    s0 = 0.0f;
                    if (e >= 0.0f) {
                        s1 = 0.0f;
                    } else if (-e >= c) {
                        s1 = 1.0f;
                        f = f + e + e + c;
                    } else {
                        s1 = -(e / c);
                        f = f + s1 * e;
                    }
                }
            } else if (seg1Param > det) {
                // seg0 param < 0, seg1 param > 1
                float dPlusB = d + b;
                if (dPlusB < 0.0f) {
                    s1 = 1.0f;
                    if (-dPlusB >= a) {
                        s0 = 1.0f;
                        f = f + dPlusB + e + dPlusB + e + c + a;
                    } else {
                        s0 = -(dPlusB / a);
                        f = f + dPlusB * s0 + e + e + c;
                    }
                } else {
                    s0 = 0.0f;
                    if (e >= 0.0f) {
                        s1 = 0.0f;
                    } else if (-e >= c) {
                        s1 = 1.0f;
                        f = f + e + e + c;
                    } else {
                        s1 = -(e / c);
                        f = f + s1 * e;
                    }
                }
            } else {
                // seg0 param < 0, seg1 param in [0, 1]
                if (d < 0.0f) {
                    s1 = 0.0f;
                    if (-d >= a) {
                        s0 = 1.0f;
                        f = f + d + d + a;
                    } else {
                        s0 = -(d / a);
                        f = f + s0 * d;
                    }
                } else {
                    s0 = 0.0f;
                    if (e >= 0.0f) {
                        s1 = 0.0f;
                    } else if (-e >= c) {
                        s1 = 1.0f;
                        f = f + e + e + c;
                    } else {
                        s1 = -(e / c);
                        f = f + s1 * e;
                    }
                }
            }
        } else if (seg0Param > det) {
            // seg0 parameter > 1
            if (seg1Param < 0.0f) {
                // seg0 param > 1, seg1 param < 0
                float dPlusB = d + b;
                if (dPlusB < 0.0f) {
                    s1 = 1.0f;
                    if (-dPlusB >= a) {
                        s0 = 1.0f;
                        f = f + dPlusB + e + dPlusB + e + c + a;
                    } else {
                        s0 = -(dPlusB / a);
                        f = f + dPlusB * s0 + e + e + c;
                    }
                } else {
                    s0 = 0.0f;
                    if (e >= 0.0f) {
                        s1 = 0.0f;
                    } else if (-e >= c) {
                        s1 = 1.0f;
                        f = f + e + e + c;
                    } else {
                        s1 = -(e / c);
                        f = f + s1 * e;
                    }
                }
            } else if (seg1Param > det) {
                // Both > det (both segments at end)
                float ePlusB = e + b;
                s0 = 1.0f;
                if (-ePlusB < c) {
                    s1 = -(ePlusB / c);
                    f = f + ePlusB * s1 + d + d + a;
                } else {
                    s1 = 1.0f;
                    f = f + ePlusB + d + b + ePlusB + d + b + c + a;
                }
            } else {
                // seg0 param > 1, seg1 param in [0, 1]
                // FIX: Clamp s0 to 1, compute optimal s1 (was incorrectly clamping s1)
                s0 = 1.0f;
                float ePlusB = e + b;
                if (ePlusB >= 0.0f) {
                    // s1 would be negative, clamp to 0
                    s1 = 0.0f;
                    f = f + d + d + a;
                } else if (-ePlusB >= c) {
                    // s1 would be > 1, clamp to 1
                    s1 = 1.0f;
                    f = f + ePlusB + ePlusB + c + d + d + a;
                } else {
                    // s1 is in (0, 1)
                    s1 = -(ePlusB / c);
                    f = f + ePlusB * s1 + d + d + a;
                }
            }
        } else {
            // seg0 param in [0, det]
            if (seg1Param < 0.0f) {
                // seg0 param in range, seg1 param < 0
                if (d < 0.0f) {
                    s1 = 0.0f;
                    if (-d >= a) {
                        s0 = 1.0f;
                        f = f + d + d + a;
                    } else {
                        s0 = -(d / a);
                        f = f + s0 * d;
                    }
                } else {
                    s0 = 0.0f;
                    if (e >= 0.0f) {
                        s1 = 0.0f;
                    } else if (-e >= c) {
                        s1 = 1.0f;
                        f = f + e + e + c;
                    } else {
                        s1 = -(e / c);
                        f = f + s1 * e;
                    }
                }
            } else if (seg1Param > det) {
                // seg0 param in range, seg1 param > 1
                float dPlusB = d + b;
                s1 = 1.0f;
                if (dPlusB < 0.0f) {
                    if (-dPlusB >= a) {
                        s0 = 1.0f;
                        f = f + dPlusB + e + dPlusB + e + c + a;
                    } else {
                        s0 = -(dPlusB / a);
                        f = f + dPlusB * s0 + e + e + c;
                    }
                } else {
                    s0 = 0.0f;
                    f = f + e + e + c;
                }
            } else {
                // Both in range
                float invDet = 1.0f / det;
                s0 = seg0Param * invDet;
                s1 = invDet * seg1Param;
                f = f + (s1 * c + s0 * b + e + e) * s1 + (s1 * b + s0 * a + d + d) * s0;
            }
        }
    }

    if (t0) *t0 = s0;
    if (t1) *t1 = s1;

    return XAbs(f);
}

// Point-Line distance calculations

float VxDistance::PointLineSquareDistance(const VxVector &point, const VxRay &line, float *t0) {
    return GetVxDistanceDispatchTable()->pointLineSquareDistance(point, line, t0);
}

float VxDistance::PointRaySquareDistance(const VxVector &point, const VxRay &ray, float *t0) {
    return GetVxDistanceDispatchTable()->pointRaySquareDistance(point, ray, t0);
}

float VxDistance::PointSegmentSquareDistance(const VxVector &point, const VxRay &segment, float *t0) {
    return GetVxDistanceDispatchTable()->pointSegmentSquareDistance(point, segment, t0);
}



