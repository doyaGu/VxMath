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

static float LineSegmentSquareDistanceSIMDImpl(const VxRay &line, const VxRay &segment, float *t0, float *t1) {
    const __m128d wXY = _mm_sub_pd(_mm_setr_pd(line.m_Origin.x, line.m_Origin.y),
                                  _mm_setr_pd(segment.m_Origin.x, segment.m_Origin.y));
    const double wx = _mm_cvtsd_f64(wXY);
    const double wy = _mm_cvtsd_f64(_mm_unpackhi_pd(wXY, wXY));
    const double wz = double(line.m_Origin.z) - segment.m_Origin.z;
    const double ux = line.m_Direction.x, uy = line.m_Direction.y, uz = line.m_Direction.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const __m128d nXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(vz, vx)),
                                  _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(vy, vz)));
    const double nz = ux * vy - uy * vx;
    const __m128d nSquareXY = _mm_mul_pd(nXY, nXY);
    const double denominator = _mm_cvtsd_f64(_mm_add_sd(nSquareXY, _mm_unpackhi_pd(nSquareXY, nSquareXY))) + nz * nz;
    const __m128d uCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(wz, wx)),
                                       _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(wy, wz)));
    const __m128d tXY = _mm_mul_pd(uCrossWXY, nXY);
    const double segmentNumerator = _mm_cvtsd_f64(_mm_add_sd(tXY, _mm_unpackhi_pd(tXY, tXY))) + (ux * wy - uy * wx) * nz;
    // Error-free endpoint addition retains a small component beside a
    // much larger origin; use it before comparing against the end plane.
    const __m128d originXY = _mm_setr_pd(segment.m_Origin.x, segment.m_Origin.y);
    const __m128d vXY = _mm_setr_pd(vx, vy);
    const __m128d endXY = _mm_add_pd(originXY, vXY);
    const __m128d virtualXY = _mm_sub_pd(endXY, originXY);
    const __m128d errorXY = _mm_add_pd(_mm_sub_pd(originXY, _mm_sub_pd(endXY, virtualXY)),
                                      _mm_sub_pd(vXY, virtualXY));
    const __m128d endWXY = _mm_sub_pd(_mm_sub_pd(_mm_setr_pd(line.m_Origin.x, line.m_Origin.y), endXY), errorXY);
    const double ewx = _mm_cvtsd_f64(endWXY);
    const double ewy = _mm_cvtsd_f64(_mm_unpackhi_pd(endWXY, endWXY));
    const double endZ = double(segment.m_Origin.z) + vz;
    const double virtualZ = endZ - segment.m_Origin.z;
    const double errorZ = (double(segment.m_Origin.z) - (endZ - virtualZ)) + (vz - virtualZ);
    const double ewz = (double(line.m_Origin.z) - endZ) - errorZ;
    const __m128d endCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(ewz, ewx)),
                                        _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(ewy, ewz)));
    const __m128d endDotXY = _mm_mul_pd(endCrossXY, nXY);
    const double endNumerator = _mm_cvtsd_f64(_mm_add_sd(endDotXY, _mm_unpackhi_pd(endDotXY, endDotXY))) + (ux * ewy - uy * ewx) * nz;
    double s, t, square;
    if (denominator == 0.0 || segmentNumerator < 0.0 || endNumerator > 0.0) {
        t = denominator == 0.0 || segmentNumerator < 0.0 ? 0.0 : 1.0;
        const __m128d diffXY = t == 0.0 ? wXY : endWXY;
        const double dx = t == 0.0 ? wx : ewx;
        const double dy = t == 0.0 ? wy : ewy;
        const double dz = t == 0.0 ? wz : ewz;
        const __m128d uXY = _mm_setr_pd(ux, uy);
        const __m128d lengthXY = _mm_mul_pd(uXY, uXY);
        const __m128d dotXY = _mm_mul_pd(diffXY, uXY);
        const double length = _mm_cvtsd_f64(_mm_add_sd(lengthXY, _mm_unpackhi_pd(lengthXY, lengthXY))) + uz * uz;
        s = -(_mm_cvtsd_f64(_mm_add_sd(dotXY, _mm_unpackhi_pd(dotXY, dotXY))) + dz * uz) / length;
        const __m128d cXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(dy, dz), _mm_setr_pd(uz, ux)),
                                      _mm_mul_pd(_mm_setr_pd(dz, dx), _mm_setr_pd(uy, uz)));
        const double cz = dx * uy - dy * ux;
        const __m128d cSquareXY = _mm_mul_pd(cXY, cXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(cSquareXY, _mm_unpackhi_pd(cSquareXY, cSquareXY))) + cz * cz) / length;
    } else {
        const __m128d vCrossWXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(vy, vz), _mm_setr_pd(wz, wx)),
                                           _mm_mul_pd(_mm_setr_pd(vz, vx), _mm_setr_pd(wy, wz)));
        const __m128d sXY = _mm_mul_pd(vCrossWXY, nXY);
        s = (_mm_cvtsd_f64(_mm_add_sd(sXY, _mm_unpackhi_pd(sXY, sXY))) + (vx * wy - vy * wx) * nz) / denominator;
        t = segmentNumerator / denominator;
        const __m128d heightXY = _mm_mul_pd(wXY, nXY);
        const double height = _mm_cvtsd_f64(_mm_add_sd(heightXY, _mm_unpackhi_pd(heightXY, heightXY))) + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float RayRaySquareDistanceSIMDImpl(const VxRay &ray0, const VxRay &ray1, float *t0, float *t1) {
    const __m128d wXY = _mm_sub_pd(_mm_setr_pd(ray0.m_Origin.x, ray0.m_Origin.y),
                                  _mm_setr_pd(ray1.m_Origin.x, ray1.m_Origin.y));
    const double wx = _mm_cvtsd_f64(wXY), wy = _mm_cvtsd_f64(_mm_unpackhi_pd(wXY, wXY));
    const double wz = double(ray0.m_Origin.z) - ray1.m_Origin.z;
    const double ux = ray0.m_Direction.x, uy = ray0.m_Direction.y, uz = ray0.m_Direction.z;
    const double vx = ray1.m_Direction.x, vy = ray1.m_Direction.y, vz = ray1.m_Direction.z;
    const __m128d uXY = _mm_setr_pd(ux, uy), vXY = _mm_setr_pd(vx, vy);
    const __m128d aXY = _mm_mul_pd(uXY, uXY), cXY = _mm_mul_pd(vXY, vXY);
    const __m128d uvXY = _mm_mul_pd(uXY, vXY), dXY = _mm_mul_pd(wXY, uXY), eXY = _mm_mul_pd(wXY, vXY);
    const double a = _mm_cvtsd_f64(_mm_add_sd(aXY, _mm_unpackhi_pd(aXY, aXY))) + uz * uz;
    const double c = _mm_cvtsd_f64(_mm_add_sd(cXY, _mm_unpackhi_pd(cXY, cXY))) + vz * vz;
    const double uv = _mm_cvtsd_f64(_mm_add_sd(uvXY, _mm_unpackhi_pd(uvXY, uvXY))) + uz * vz;
    const double d = _mm_cvtsd_f64(_mm_add_sd(dXY, _mm_unpackhi_pd(dXY, dXY))) + wz * uz;
    const double e = _mm_cvtsd_f64(_mm_add_sd(eXY, _mm_unpackhi_pd(eXY, eXY))) + wz * vz;
    const __m128d nXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(vz, vx)),
                                  _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(vy, vz)));
    const double nz = ux * vy - uy * vx;
    const __m128d nSquareXY = _mm_mul_pd(nXY, nXY);
    const double denominator = _mm_cvtsd_f64(_mm_add_sd(nSquareXY, _mm_unpackhi_pd(nSquareXY, nSquareXY))) + nz * nz;
    const __m128d uCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(wz, wx)),
                                      _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(wy, wz)));
    const __m128d vCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(vy, vz), _mm_setr_pd(wz, wx)),
                                      _mm_mul_pd(_mm_setr_pd(vz, vx), _mm_setr_pd(wy, wz)));
    const double uCrossZ = ux * wy - uy * wx, vCrossZ = vx * wy - vy * wx;
    const __m128d sXY = _mm_mul_pd(vCrossXY, nXY), tXY = _mm_mul_pd(uCrossXY, nXY);
    const double sNumerator = _mm_cvtsd_f64(_mm_add_sd(sXY, _mm_unpackhi_pd(sXY, sXY))) + vCrossZ * nz;
    const double tNumerator = _mm_cvtsd_f64(_mm_add_sd(tXY, _mm_unpackhi_pd(tXY, tXY))) + uCrossZ * nz;
    enum ClosestRegion { Origins, FirstRay, SecondRay, BothRays };
    ClosestRegion closest;
    if (denominator == 0.0) {
        closest = d < 0.0 ? FirstRay : uv < 0.0 ? Origins : SecondRay;
    } else if (sNumerator < 0.0) {
        if (tNumerator >= 0.0 || d >= 0.0)
            closest = e > 0.0 ? SecondRay : Origins;
        else
            closest = FirstRay;
    } else if (tNumerator < 0.0) {
        closest = d < 0.0 ? FirstRay : Origins;
    } else {
        closest = BothRays;
    }
    double first = 0.0, second = 0.0, square;
    if (closest == BothRays) {
        first = sNumerator / denominator;
        second = tNumerator / denominator;
        const __m128d heightXY = _mm_mul_pd(wXY, nXY);
        const double height = _mm_cvtsd_f64(_mm_add_sd(heightXY, _mm_unpackhi_pd(heightXY, heightXY))) + wz * nz;
        square = height * height / denominator;
    } else if (closest == FirstRay) {
        first = -d / a;
        const __m128d squareXY = _mm_mul_pd(uCrossXY, uCrossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + uCrossZ * uCrossZ) / a;
    } else if (closest == SecondRay) {
        second = e / c;
        const __m128d squareXY = _mm_mul_pd(vCrossXY, vCrossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + vCrossZ * vCrossZ) / c;
    } else {
        const __m128d squareXY = _mm_mul_pd(wXY, wXY);
        square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + wz * wz;
    }
    const float result = float(square);
    if (t0) *t0 = float(first);
    if (t1) *t1 = float(second);
    return result;
}

static float RaySegmentSquareDistanceSIMDImpl(const VxRay &ray, const VxRay &segment, float *t0, float *t1) {
    const __m128d wXY = _mm_sub_pd(_mm_setr_pd(ray.m_Origin.x, ray.m_Origin.y),
                                  _mm_setr_pd(segment.m_Origin.x, segment.m_Origin.y));
    const double wx = _mm_cvtsd_f64(wXY), wy = _mm_cvtsd_f64(_mm_unpackhi_pd(wXY, wXY));
    const double wz = double(ray.m_Origin.z) - segment.m_Origin.z;
    const double ux = ray.m_Direction.x, uy = ray.m_Direction.y, uz = ray.m_Direction.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const __m128d uXY = _mm_setr_pd(ux, uy), vXY = _mm_setr_pd(vx, vy);
    const __m128d aXY = _mm_mul_pd(uXY, uXY), cXY = _mm_mul_pd(vXY, vXY);
    const __m128d uvXY = _mm_mul_pd(uXY, vXY), dXY = _mm_mul_pd(wXY, uXY), eXY = _mm_mul_pd(wXY, vXY);
    const double a = _mm_cvtsd_f64(_mm_add_sd(aXY, _mm_unpackhi_pd(aXY, aXY))) + uz * uz;
    const double c = _mm_cvtsd_f64(_mm_add_sd(cXY, _mm_unpackhi_pd(cXY, cXY))) + vz * vz;
    const double uv = _mm_cvtsd_f64(_mm_add_sd(uvXY, _mm_unpackhi_pd(uvXY, uvXY))) + uz * vz;
    const double d = _mm_cvtsd_f64(_mm_add_sd(dXY, _mm_unpackhi_pd(dXY, dXY))) + wz * uz;
    const double e = _mm_cvtsd_f64(_mm_add_sd(eXY, _mm_unpackhi_pd(eXY, eXY))) + wz * vz;
    const __m128d nXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(vz, vx)),
                                  _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(vy, vz)));
    const double nz = ux * vy - uy * vx;
    const __m128d nSquareXY = _mm_mul_pd(nXY, nXY);
    const double denominator = _mm_cvtsd_f64(_mm_add_sd(nSquareXY, _mm_unpackhi_pd(nSquareXY, nSquareXY))) + nz * nz;
    const __m128d uCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(wz, wx)),
                                      _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(wy, wz)));
    const __m128d vCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(vy, vz), _mm_setr_pd(wz, wx)),
                                      _mm_mul_pd(_mm_setr_pd(vz, vx), _mm_setr_pd(wy, wz)));
    const double uCrossZ = ux * wy - uy * wx, vCrossZ = vx * wy - vy * wx;
    const __m128d sXY = _mm_mul_pd(vCrossXY, nXY), tXY = _mm_mul_pd(uCrossXY, nXY);
    const double sNumerator = _mm_cvtsd_f64(_mm_add_sd(sXY, _mm_unpackhi_pd(sXY, sXY))) + vCrossZ * nz;
    const double tNumerator = _mm_cvtsd_f64(_mm_add_sd(tXY, _mm_unpackhi_pd(tXY, tXY))) + uCrossZ * nz;
    const __m128d originXY = _mm_setr_pd(segment.m_Origin.x, segment.m_Origin.y);
    const __m128d endXY = _mm_add_pd(originXY, vXY);
    const __m128d virtualXY = _mm_sub_pd(endXY, originXY);
    const __m128d errorXY = _mm_add_pd(_mm_sub_pd(originXY, _mm_sub_pd(endXY, virtualXY)),
                                      _mm_sub_pd(vXY, virtualXY));
    const __m128d endWXY = _mm_sub_pd(_mm_sub_pd(_mm_setr_pd(ray.m_Origin.x, ray.m_Origin.y), endXY), errorXY);
    const double ewx = _mm_cvtsd_f64(endWXY), ewy = _mm_cvtsd_f64(_mm_unpackhi_pd(endWXY, endWXY));
    const double endZ = double(segment.m_Origin.z) + vz;
    const double virtualZ = endZ - segment.m_Origin.z;
    const double errorZ = (double(segment.m_Origin.z) - (endZ - virtualZ)) + (vz - virtualZ);
    const double ewz = (double(ray.m_Origin.z) - endZ) - errorZ;
    const __m128d endCrossXY = _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(uy, uz), _mm_setr_pd(ewz, ewx)),
                                        _mm_mul_pd(_mm_setr_pd(uz, ux), _mm_setr_pd(ewy, ewz)));
    const double endCrossZ = ux * ewy - uy * ewx;
    const __m128d endNumXY = _mm_mul_pd(endCrossXY, nXY);
    const double endNumerator = _mm_cvtsd_f64(_mm_add_sd(endNumXY, _mm_unpackhi_pd(endNumXY, endNumXY))) + endCrossZ * nz;
    const __m128d endUDotXY = _mm_mul_pd(endWXY, uXY), endVDotXY = _mm_mul_pd(endWXY, vXY);
    double endDot = _mm_cvtsd_f64(_mm_add_sd(endUDotXY, _mm_unpackhi_pd(endUDotXY, endUDotXY))) + ewz * uz;
    const double endVDot = _mm_cvtsd_f64(_mm_add_sd(endVDotXY, _mm_unpackhi_pd(endVDotXY, endVDotXY))) + ewz * vz;
    if (denominator == 0.0) {
        const __m128d rayOriginDotXY = _mm_mul_pd(_mm_setr_pd(ray.m_Origin.x, ray.m_Origin.y), uXY);
        const __m128d segmentOriginDotXY = _mm_mul_pd(originXY, uXY);
        const double products[] = { _mm_cvtsd_f64(rayOriginDotXY), _mm_cvtsd_f64(_mm_unpackhi_pd(rayOriginDotXY, rayOriginDotXY)), double(ray.m_Origin.z) * uz,
                                    -_mm_cvtsd_f64(segmentOriginDotXY), -_mm_cvtsd_f64(_mm_unpackhi_pd(segmentOriginDotXY, segmentOriginDotXY)), -double(segment.m_Origin.z) * uz,
                                    -_mm_cvtsd_f64(uvXY), -_mm_cvtsd_f64(_mm_unpackhi_pd(uvXY, uvXY)), -vz * uz };
        double sum = 0.0, error = 0.0;
        for (int i = 0; i < 9; ++i) {
            const double next = sum + products[i];
            const double part = next - sum;
            error += (sum - (next - part)) + (products[i] - part);
            sum = next;
        }
        endDot = sum + error;
    }
    enum ClosestRegion { OriginsStart, OriginsEnd, RayAtStart, RayAtEnd, OnSegment, Interior };
    ClosestRegion closest;
    if (denominator == 0.0) {
        closest = uv >= 0.0 ? (endDot < 0.0 ? RayAtEnd : OriginsEnd)
                            : (d < 0.0 ? RayAtStart : OriginsStart);
    } else if (sNumerator < 0.0) {
        // The ray-origin boundary must re-optimize the segment parameter;
        // clamping an unconstrained segment parameter first loses this region.
        if (tNumerator <= 0.0 && d < 0.0)
            closest = RayAtStart;
        else if (endNumerator > 0.0 && endDot < 0.0)
            closest = RayAtEnd;
        else
            closest = e <= 0.0 ? OriginsStart : endVDot >= 0.0 ? OriginsEnd : OnSegment;
    } else if (tNumerator < 0.0) {
        closest = d < 0.0 ? RayAtStart : OriginsStart;
    } else if (endNumerator > 0.0) {
        closest = endDot < 0.0 ? RayAtEnd : OriginsEnd;
    } else {
        closest = Interior;
    }
    double first = 0.0, second = 0.0, square;
    if (closest == Interior) {
        first = sNumerator / denominator;
        second = tNumerator / denominator;
        const __m128d heightXY = _mm_mul_pd(wXY, nXY);
        const double height = _mm_cvtsd_f64(_mm_add_sd(heightXY, _mm_unpackhi_pd(heightXY, heightXY))) + wz * nz;
        square = height * height / denominator;
    } else if (closest == RayAtStart) {
        first = -d / a;
        const __m128d squareXY = _mm_mul_pd(uCrossXY, uCrossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + uCrossZ * uCrossZ) / a;
    } else if (closest == RayAtEnd) {
        first = -endDot / a;
        second = 1.0;
        const __m128d crossXY = denominator == 0.0 ? uCrossXY : endCrossXY;
        const double crossZ = denominator == 0.0 ? uCrossZ : endCrossZ;
        const __m128d squareXY = _mm_mul_pd(crossXY, crossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + crossZ * crossZ) / a;
    } else if (closest == OnSegment) {
        second = e / c;
        const __m128d squareXY = _mm_mul_pd(vCrossXY, vCrossXY);
        square = (_mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + vCrossZ * vCrossZ) / c;
    } else if (closest == OriginsEnd) {
        second = 1.0;
        const __m128d squareXY = _mm_mul_pd(endWXY, endWXY);
        square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + ewz * ewz;
    } else {
        const __m128d squareXY = _mm_mul_pd(wXY, wXY);
        square = _mm_cvtsd_f64(_mm_add_sd(squareXY, _mm_unpackhi_pd(squareXY, squareXY))) + wz * wz;
    }
    const float result = float(square);
    if (t0) *t0 = float(first);
    if (t1) *t1 = float(second);
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

static float LineSegmentSquareDistanceScalarImpl(const VxRay &line, const VxRay &segment, float *t0, float *t1) {
    const double wx = double(line.m_Origin.x) - segment.m_Origin.x;
    const double wy = double(line.m_Origin.y) - segment.m_Origin.y;
    const double wz = double(line.m_Origin.z) - segment.m_Origin.z;
    const double ux = line.m_Direction.x, uy = line.m_Direction.y, uz = line.m_Direction.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double denominator = nx * nx + ny * ny + nz * nz;
    const double segmentNumerator = (uy * wz - uz * wy) * nx + (uz * wx - ux * wz) * ny + (ux * wy - uy * wx) * nz;
    // Keep the low part of the endpoint sum when large coordinates cancel.
    const double endX = double(segment.m_Origin.x) + vx;
    const double virtualX = endX - segment.m_Origin.x;
    const double errorX = (double(segment.m_Origin.x) - (endX - virtualX)) + (vx - virtualX);
    const double ewx = (double(line.m_Origin.x) - endX) - errorX;
    const double endY = double(segment.m_Origin.y) + vy;
    const double virtualY = endY - segment.m_Origin.y;
    const double errorY = (double(segment.m_Origin.y) - (endY - virtualY)) + (vy - virtualY);
    const double ewy = (double(line.m_Origin.y) - endY) - errorY;
    const double endZ = double(segment.m_Origin.z) + vz;
    const double virtualZ = endZ - segment.m_Origin.z;
    const double errorZ = (double(segment.m_Origin.z) - (endZ - virtualZ)) + (vz - virtualZ);
    const double ewz = (double(line.m_Origin.z) - endZ) - errorZ;
    const double endNumerator = (uy * ewz - uz * ewy) * nx + (uz * ewx - ux * ewz) * ny + (ux * ewy - uy * ewx) * nz;
    double s, t, square;
    if (denominator == 0.0 || segmentNumerator < 0.0 || endNumerator > 0.0) {
        // Test the end relative to its own position instead of subtracting
        // two large, nearly equal parameter numerators.
        t = denominator == 0.0 || segmentNumerator < 0.0 ? 0.0 : 1.0;
        const double dx = t == 0.0 ? wx : ewx;
        const double dy = t == 0.0 ? wy : ewy;
        const double dz = t == 0.0 ? wz : ewz;
        const double length = ux * ux + uy * uy + uz * uz;
        s = -(dx * ux + dy * uy + dz * uz) / length;
        const double cx = dy * uz - dz * uy;
        const double cy = dz * ux - dx * uz;
        const double cz = dx * uy - dy * ux;
        square = (cx * cx + cy * cy + cz * cz) / length;
    } else {
        // Cross products avoid the cancellation in |u|^2|v|^2-(u.v)^2
        // and in the expanded squared distance. No absolute scale threshold.
        s = ((vy * wz - vz * wy) * nx + (vz * wx - vx * wz) * ny + (vx * wy - vy * wx) * nz) / denominator;
        t = segmentNumerator / denominator;
        const double height = wx * nx + wy * ny + wz * nz;
        square = height * height / denominator;
    }
    const float result = float(square);
    if (t0) *t0 = float(s);
    if (t1) *t1 = float(t);
    return result;
}

static float RayRaySquareDistanceScalarImpl(const VxRay &ray0, const VxRay &ray1, float *t0, float *t1) {
    const double wx = double(ray0.m_Origin.x) - ray1.m_Origin.x;
    const double wy = double(ray0.m_Origin.y) - ray1.m_Origin.y;
    const double wz = double(ray0.m_Origin.z) - ray1.m_Origin.z;
    const double ux = ray0.m_Direction.x, uy = ray0.m_Direction.y, uz = ray0.m_Direction.z;
    const double vx = ray1.m_Direction.x, vy = ray1.m_Direction.y, vz = ray1.m_Direction.z;
    const double a = ux * ux + uy * uy + uz * uz;
    const double c = vx * vx + vy * vy + vz * vz;
    const double uv = ux * vx + uy * vy + uz * vz;
    const double d = wx * ux + wy * uy + wz * uz;
    const double e = wx * vx + wy * vy + wz * vz;
    const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const double denominator = nx * nx + ny * ny + nz * nz;
    const double uCrossX = uy * wz - uz * wy, uCrossY = uz * wx - ux * wz, uCrossZ = ux * wy - uy * wx;
    const double vCrossX = vy * wz - vz * wy, vCrossY = vz * wx - vx * wz, vCrossZ = vx * wy - vy * wx;
    const double sNumerator = vCrossX * nx + vCrossY * ny + vCrossZ * nz;
    const double tNumerator = uCrossX * nx + uCrossY * ny + uCrossZ * nz;
    enum ClosestRegion { Origins, FirstRay, SecondRay, BothRays };
    ClosestRegion closest;
    if (denominator == 0.0) {
        // Preserve the original representative for overlapping parallel rays,
        // the opposed-ray origin case, and legacy zero-direction arithmetic.
        closest = d < 0.0 ? FirstRay : uv < 0.0 ? Origins : SecondRay;
    } else if (sNumerator < 0.0) {
        if (tNumerator >= 0.0 || d >= 0.0)
            closest = e > 0.0 ? SecondRay : Origins;
        else
            closest = FirstRay;
    } else if (tNumerator < 0.0) {
        closest = d < 0.0 ? FirstRay : Origins;
    } else {
        closest = BothRays;
    }
    double first = 0.0, second = 0.0, square;
    if (closest == BothRays) {
        first = sNumerator / denominator;
        second = tNumerator / denominator;
        const double height = wx * nx + wy * ny + wz * nz;
        square = height * height / denominator;
    } else if (closest == FirstRay) {
        first = -d / a;
        square = (uCrossX * uCrossX + uCrossY * uCrossY + uCrossZ * uCrossZ) / a;
    } else if (closest == SecondRay) {
        second = e / c;
        square = (vCrossX * vCrossX + vCrossY * vCrossY + vCrossZ * vCrossZ) / c;
    } else {
        square = wx * wx + wy * wy + wz * wz;
    }
    const float result = float(square);
    if (t0) *t0 = float(first);
    if (t1) *t1 = float(second);
    return result;
}

static float RaySegmentSquareDistanceScalarImpl(const VxRay &ray, const VxRay &segment, float *t0, float *t1) {
    const double wx = double(ray.m_Origin.x) - segment.m_Origin.x;
    const double wy = double(ray.m_Origin.y) - segment.m_Origin.y;
    const double wz = double(ray.m_Origin.z) - segment.m_Origin.z;
    const double ux = ray.m_Direction.x, uy = ray.m_Direction.y, uz = ray.m_Direction.z;
    const double vx = segment.m_Direction.x, vy = segment.m_Direction.y, vz = segment.m_Direction.z;
    const double a = ux * ux + uy * uy + uz * uz;
    const double c = vx * vx + vy * vy + vz * vz;
    const double uv = ux * vx + uy * vy + uz * vz;
    const double d = wx * ux + wy * uy + wz * uz;
    const double e = wx * vx + wy * vy + wz * vz;
    const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const double denominator = nx * nx + ny * ny + nz * nz;
    const double uCrossX = uy * wz - uz * wy, uCrossY = uz * wx - ux * wz, uCrossZ = ux * wy - uy * wx;
    const double vCrossX = vy * wz - vz * wy, vCrossY = vz * wx - vx * wz, vCrossZ = vx * wy - vy * wx;
    const double sNumerator = vCrossX * nx + vCrossY * ny + vCrossZ * nz;
    const double tNumerator = uCrossX * nx + uCrossY * ny + uCrossZ * nz;
    // Preserve the endpoint sum's low part before testing the end plane.
    const double endX = double(segment.m_Origin.x) + vx;
    const double virtualX = endX - segment.m_Origin.x;
    const double errorX = (double(segment.m_Origin.x) - (endX - virtualX)) + (vx - virtualX);
    const double ewx = (double(ray.m_Origin.x) - endX) - errorX;
    const double endY = double(segment.m_Origin.y) + vy;
    const double virtualY = endY - segment.m_Origin.y;
    const double errorY = (double(segment.m_Origin.y) - (endY - virtualY)) + (vy - virtualY);
    const double ewy = (double(ray.m_Origin.y) - endY) - errorY;
    const double endZ = double(segment.m_Origin.z) + vz;
    const double virtualZ = endZ - segment.m_Origin.z;
    const double errorZ = (double(segment.m_Origin.z) - (endZ - virtualZ)) + (vz - virtualZ);
    const double ewz = (double(ray.m_Origin.z) - endZ) - errorZ;
    const double endCrossX = uy * ewz - uz * ewy, endCrossY = uz * ewx - ux * ewz, endCrossZ = ux * ewy - uy * ewx;
    const double endNumerator = endCrossX * nx + endCrossY * ny + endCrossZ * nz;
    double endDot = ewx * ux + ewy * uy + ewz * uz;
    const double endVDot = ewx * vx + ewy * vy + ewz * vz;
    if (denominator == 0.0) {
        // Each product of two input floats is exact in double. Accumulate
        // the endpoint projection before rounding away its small coordinate.
        const double products[] = { double(ray.m_Origin.x) * ux, double(ray.m_Origin.y) * uy, double(ray.m_Origin.z) * uz,
                                    -double(segment.m_Origin.x) * ux, -double(segment.m_Origin.y) * uy, -double(segment.m_Origin.z) * uz,
                                    -vx * ux, -vy * uy, -vz * uz };
        double sum = 0.0, error = 0.0;
        for (int i = 0; i < 9; ++i) {
            const double next = sum + products[i];
            const double part = next - sum;
            error += (sum - (next - part)) + (products[i] - part);
            sum = next;
        }
        endDot = sum + error;
    }
    enum ClosestRegion { OriginsStart, OriginsEnd, RayAtStart, RayAtEnd, OnSegment, Interior };
    ClosestRegion closest;
    if (denominator == 0.0) {
        closest = uv >= 0.0 ? (endDot < 0.0 ? RayAtEnd : OriginsEnd)
                            : (d < 0.0 ? RayAtStart : OriginsStart);
    } else if (sNumerator < 0.0) {
        // The ray-origin boundary must re-optimize the segment parameter;
        // clamping an unconstrained segment parameter first loses this region.
        if (tNumerator <= 0.0 && d < 0.0)
            closest = RayAtStart;
        else if (endNumerator > 0.0 && endDot < 0.0)
            closest = RayAtEnd;
        else
            closest = e <= 0.0 ? OriginsStart : endVDot >= 0.0 ? OriginsEnd : OnSegment;
    } else if (tNumerator < 0.0) {
        closest = d < 0.0 ? RayAtStart : OriginsStart;
    } else if (endNumerator > 0.0) {
        closest = endDot < 0.0 ? RayAtEnd : OriginsEnd;
    } else {
        closest = Interior;
    }
    double first = 0.0, second = 0.0, square;
    if (closest == Interior) {
        first = sNumerator / denominator;
        second = tNumerator / denominator;
        const double height = wx * nx + wy * ny + wz * nz;
        square = height * height / denominator;
    } else if (closest == RayAtStart) {
        first = -d / a;
        square = (uCrossX * uCrossX + uCrossY * uCrossY + uCrossZ * uCrossZ) / a;
    } else if (closest == RayAtEnd) {
        first = -endDot / a;
        second = 1.0;
        // Translating along parallel directions leaves the perpendicular
        // distance unchanged; retain the small original-origin offset.
        square = denominator == 0.0 ? (uCrossX * uCrossX + uCrossY * uCrossY + uCrossZ * uCrossZ) / a
                                     : (endCrossX * endCrossX + endCrossY * endCrossY + endCrossZ * endCrossZ) / a;
    } else if (closest == OnSegment) {
        second = e / c;
        square = (vCrossX * vCrossX + vCrossY * vCrossY + vCrossZ * vCrossZ) / c;
    } else if (closest == OriginsEnd) {
        second = 1.0;
        square = ewx * ewx + ewy * ewy + ewz * ewz;
    } else {
        square = wx * wx + wy * wy + wz * wz;
    }
    const float result = float(square);
    if (t0) *t0 = float(first);
    if (t1) *t1 = float(second);
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

// Preserve endpoint low bits without changing the process floating-point mode.
static inline double SegmentDistanceSum(double x, double y, double &error) {
    const double sum = x + y;
    const double tail = sum - x;
    error += (x - (sum - tail)) + (y - tail);
    return sum;
}

template<bool kUseSIMD>
static double SegmentDistanceDot(const double *x, const double *y) {
    double products[3];
#if defined(VX_SIMD_SSE)
    if constexpr (kUseSIMD) {
        _mm_storeu_pd(products, _mm_mul_pd(_mm_loadu_pd(x), _mm_loadu_pd(y)));
    } else
#endif
    {
        products[0] = x[0] * y[0];
        products[1] = x[1] * y[1];
    }
    products[2] = x[2] * y[2];
    double error = 0.0;
    const double sum = SegmentDistanceSum(products[0], products[1], error);
    const double result = SegmentDistanceSum(sum, products[2], error);
    return result + error;
}

template<bool kUseSIMD>
static void SegmentDistanceCross(const double *x, const double *y, double *cross) {
#if defined(VX_SIMD_SSE)
    if constexpr (kUseSIMD) {
        _mm_storeu_pd(cross, _mm_sub_pd(_mm_mul_pd(_mm_setr_pd(x[1], x[2]), _mm_setr_pd(y[2], y[0])),
                                       _mm_mul_pd(_mm_setr_pd(x[2], x[0]), _mm_setr_pd(y[1], y[2]))));
    } else
#endif
    {
        cross[0] = x[1] * y[2] - x[2] * y[1];
        cross[1] = x[2] * y[0] - x[0] * y[2];
    }
    cross[2] = x[0] * y[1] - x[1] * y[0];
}

template<bool kUseSIMD>
static float SegmentSegmentSquareDistanceCore(const VxRay &segment0, const VxRay &segment1, float *t0, float *t1) {
    const double u[] = {segment0.m_Direction.x, segment0.m_Direction.y, segment0.m_Direction.z};
    const double v[] = {segment1.m_Direction.x, segment1.m_Direction.y, segment1.m_Direction.z};
    double w[3], low[3], normal[3];
    for (int j = 0; j < 3; ++j) {
        low[j] = 0.0;
        w[j] = SegmentDistanceSum(segment0.m_Origin[j], -double(segment1.m_Origin[j]), low[j]);
    }
    SegmentDistanceCross<kUseSIMD>(u, v, normal);
    const double denominator = SegmentDistanceDot<kUseSIMD>(normal, normal);
    const double a = SegmentDistanceDot<kUseSIMD>(u, u);
    const double c = SegmentDistanceDot<kUseSIMD>(v, v);
    const double d = SegmentDistanceDot<kUseSIMD>(w, u) + SegmentDistanceDot<kUseSIMD>(low, u);
    double first = 0.0, second = 0.0, square = 0.0;
    if (a == 0.0) {
        // Original zero-first-extent representative, not a new point query.
        first = 1.0;
        square = SegmentDistanceDot<kUseSIMD>(w, w);
    } else {
        // Parallel overlap projects the second origin onto the first segment,
        // or its nearest endpoint onto the second segment, as in the original.
        int begin = 0, end = 4;
        if (denominator == 0.0) {
            begin = d > 0.0 ? 2 : -d >= a ? 3 : 0;
            end = begin + 1;
        }
        square = HUGE_VAL;
        for (int boundary = begin; boundary < end; ++boundary) {
            const double fixedS = boundary == 3 ? 1.0 : 0.0;
            const double fixedT = boundary == 1 ? 1.0 : 0.0;
            const double *direction = boundary < 2 ? u : v;
            const double *other = boundary < 2 ? v : u;
            const double length = boundary < 2 ? a : c;
            double relative[3], tail[3];
            for (int j = 0; j < 3; ++j) {
                tail[j] = low[j];
                relative[j] = SegmentDistanceSum(w[j], fixedS * u[j], tail[j]);
                relative[j] = SegmentDistanceSum(relative[j], -fixedT * v[j], tail[j]);
            }
            const double sign = boundary < 2 ? -1.0 : 1.0;
            const double projection = sign * (SegmentDistanceDot<kUseSIMD>(relative, direction)
                                             + SegmentDistanceDot<kUseSIMD>(tail, direction));
            double parameter, candidate, gradient;
            if (projection <= 0.0) {
                parameter = 0.0;
                gradient = SegmentDistanceDot<kUseSIMD>(relative, other) + SegmentDistanceDot<kUseSIMD>(tail, other);
                for (int j = 0; j < 3; ++j) relative[j] += tail[j];
                candidate = SegmentDistanceDot<kUseSIMD>(relative, relative);
            } else if (projection >= length) {
                parameter = 1.0;
                for (int j = 0; j < 3; ++j) {
                    const double endpoint = SegmentDistanceSum(relative[j], -sign * direction[j], tail[j]);
                    relative[j] = endpoint;
                }
                gradient = SegmentDistanceDot<kUseSIMD>(relative, other) + SegmentDistanceDot<kUseSIMD>(tail, other);
                for (int j = 0; j < 3; ++j) relative[j] += tail[j];
                candidate = SegmentDistanceDot<kUseSIMD>(relative, relative);
            } else {
                parameter = projection / length;
                double cross[3], correction[3], otherCross[3];
                SegmentDistanceCross<kUseSIMD>(relative, direction, cross);
                SegmentDistanceCross<kUseSIMD>(tail, direction, correction);
                for (int j = 0; j < 3; ++j) cross[j] += correction[j];
                candidate = SegmentDistanceDot<kUseSIMD>(cross, cross) / length;
                SegmentDistanceCross<kUseSIMD>(other, direction, otherCross);
                gradient = SegmentDistanceDot<kUseSIMD>(cross, otherCross) / length;
            }
            // Select by the remaining endpoint's derivative, not rounded
            // squared distances: distinct minima may round to the same value.
            const bool atMinimum = boundary == 0 || boundary == 3 ? gradient <= 0.0 : gradient >= 0.0;
            if (denominator == 0.0 || atMinimum) {
                square = candidate;
                first = boundary < 2 ? parameter : fixedS;
                second = boundary < 2 ? fixedT : parameter;
                break;
            }
        }
        if (c == 0.0 && d > 0.0) second = 1.0;
        if (denominator != 0.0) {
            double vw[3], uw[3], correction[3];
            SegmentDistanceCross<kUseSIMD>(v, w, vw);
            SegmentDistanceCross<kUseSIMD>(v, low, correction);
            for (int j = 0; j < 3; ++j) vw[j] += correction[j];
            SegmentDistanceCross<kUseSIMD>(u, w, uw);
            SegmentDistanceCross<kUseSIMD>(u, low, correction);
            for (int j = 0; j < 3; ++j) uw[j] += correction[j];
            const double s = SegmentDistanceDot<kUseSIMD>(vw, normal) / denominator;
            const double t = SegmentDistanceDot<kUseSIMD>(uw, normal) / denominator;
            double endS[3], endT[3], tailS[3], tailT[3], crossS[3], crossT[3];
            for (int j = 0; j < 3; ++j) {
                tailS[j] = tailT[j] = low[j];
                endS[j] = SegmentDistanceSum(w[j], u[j], tailS[j]);
                endT[j] = SegmentDistanceSum(w[j], -v[j], tailT[j]);
            }
            SegmentDistanceCross<kUseSIMD>(v, endS, crossS);
            SegmentDistanceCross<kUseSIMD>(v, tailS, correction);
            for (int j = 0; j < 3; ++j) crossS[j] += correction[j];
            SegmentDistanceCross<kUseSIMD>(u, endT, crossT);
            SegmentDistanceCross<kUseSIMD>(u, tailT, correction);
            for (int j = 0; j < 3; ++j) crossT[j] += correction[j];
            // Test against each endpoint independently. Subtracting the
            // denominator from a rounded numerator loses small endpoint gaps.
            if (s >= 0.0 && t >= 0.0 && SegmentDistanceDot<kUseSIMD>(crossS, normal) <= 0.0
                                           && SegmentDistanceDot<kUseSIMD>(crossT, normal) <= 0.0) {
                const double height = SegmentDistanceDot<kUseSIMD>(w, normal) + SegmentDistanceDot<kUseSIMD>(low, normal);
                const double candidate = height * height / denominator;
                square = candidate; first = s; second = t;
            }
        }
    }
    const float result = float(square);
    if (t0) *t0 = float(first);
    if (t1) *t1 = float(second);
    return result;
}

static float SegmentSegmentSquareDistanceScalarImpl(const VxRay &segment0, const VxRay &segment1, float *t0, float *t1) {
    return SegmentSegmentSquareDistanceCore<false>(segment0, segment1, t0, t1);
}
#if defined(VX_SIMD_SSE)
static float SegmentSegmentSquareDistanceSIMDImpl(const VxRay &segment0, const VxRay &segment1, float *t0, float *t1) {
    return SegmentSegmentSquareDistanceCore<true>(segment0, segment1, t0, t1);
}
#endif

typedef RayPairTerms (*MakeRayPairTermsDispatchFn)(const VxRay &, const VxRay &);
typedef float (*LineDistanceDispatchFn)(const VxRay &, const VxRay &, float *, float *);
typedef float (*PointDistanceDispatchFn)(const VxVector &, const VxRay &, float *);

struct VxDistanceDispatchTable {
    MakeRayPairTermsDispatchFn makeRayPairTerms;
    LineDistanceDispatchFn lineLineSquareDistance;
    LineDistanceDispatchFn lineRaySquareDistance;
    LineDistanceDispatchFn lineSegmentSquareDistance;
    LineDistanceDispatchFn rayRaySquareDistance;
    LineDistanceDispatchFn raySegmentSquareDistance;
    LineDistanceDispatchFn segmentSegmentSquareDistance;
    PointDistanceDispatchFn pointLineSquareDistance;
    PointDistanceDispatchFn pointRaySquareDistance;
    PointDistanceDispatchFn pointSegmentSquareDistance;
};

const VxDistanceDispatchTable kVxDistanceDispatchScalar = {
    MakeRayPairTermsScalar,
    LineLineSquareDistanceScalarImpl,
    LineRaySquareDistanceScalarImpl,
    LineSegmentSquareDistanceScalarImpl,
    RayRaySquareDistanceScalarImpl,
    RaySegmentSquareDistanceScalarImpl,
    SegmentSegmentSquareDistanceScalarImpl,
    PointLineSquareDistanceScalarImpl,
    PointRaySquareDistanceScalarImpl,
    PointSegmentSquareDistanceScalarImpl
};

#if defined(VX_SIMD_SSE)
const VxDistanceDispatchTable kVxDistanceDispatchSIMD = {
    MakeRayPairTermsSIMD,
    LineLineSquareDistanceSIMDImpl,
    LineRaySquareDistanceSIMDImpl,
    LineSegmentSquareDistanceSIMDImpl,
    RayRaySquareDistanceSIMDImpl,
    RaySegmentSquareDistanceSIMDImpl,
    SegmentSegmentSquareDistanceSIMDImpl,
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
    return GetVxDistanceDispatchTable()->lineSegmentSquareDistance(line, segment, t0, t1);
}

float VxDistance::RayRaySquareDistance(const VxRay &ray0, const VxRay &ray1, float *t0, float *t1) {
    return GetVxDistanceDispatchTable()->rayRaySquareDistance(ray0, ray1, t0, t1);
}

float VxDistance::RaySegmentSquareDistance(const VxRay &ray, const VxRay &segment, float *t0, float *t1) {
    return GetVxDistanceDispatchTable()->raySegmentSquareDistance(ray, segment, t0, t1);
}

float VxDistance::SegmentSegmentSquareDistance(const VxRay &segment0, const VxRay &segment1, float *t0, float *t1) {
    return GetVxDistanceDispatchTable()->segmentSegmentSquareDistance(segment0, segment1, t0, t1);
}

float VxDistance::PointLineSquareDistance(const VxVector &point, const VxRay &line, float *t0) {
    return GetVxDistanceDispatchTable()->pointLineSquareDistance(point, line, t0);
}

float VxDistance::PointRaySquareDistance(const VxVector &point, const VxRay &ray, float *t0) {
    return GetVxDistanceDispatchTable()->pointRaySquareDistance(point, ray, t0);
}

float VxDistance::PointSegmentSquareDistance(const VxVector &point, const VxRay &segment, float *t0) {
    return GetVxDistanceDispatchTable()->pointSegmentSquareDistance(point, segment, t0);
}



