#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_PROJECTION_PROBE
#ifndef VX_MATRIX_PROJECTION_HEADER
#define VX_MATRIX_PROJECTION_HEADER "VxMatrix.h"
#endif
#include VX_MATRIX_PROJECTION_HEADER
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 0=Perspective, 1=Orthographic, 2=PerspectiveRect, 3=OrthographicRect.
typedef void (*MPBuildFn)(VxMatrix &, int, const float *);
struct MPResult { int samples = 0, errors = 0; };
static void MPBuild(VxMatrix &m, int kind, const float *p) {
    switch (kind) {
    case 0: m.Perspective(p[0], p[1], p[2], p[3]); break;
    case 1: m.Orthographic(p[0], p[1], p[2], p[3]); break;
    case 2: m.PerspectiveRect(p[0], p[1], p[2], p[3], p[4], p[5]); break;
    case 3: m.OrthographicRect(p[0], p[1], p[2], p[3], p[4], p[5]); break;
    }
}
static bool MPClose(double actual, double expected) { return std::isfinite(actual) && std::fabs(actual - expected) <= 0.00002; }
static bool MPShape(const VxMatrix &m, int kind) {
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        const bool used = row == col || (row == 3 && col == 2) || (kind == 0 || kind == 2 ? row == 2 && col == 3 : row == 3 && col == 3)
            || (kind == 2 && row == 2 && col < 2) || (kind == 3 && row == 3 && col < 2);
        if (!used && m[row][col] != 0.0f) return false;
    }
    return (kind == 0 || kind == 2) ? m[2][3] == 1.0f && m[3][3] == 0.0f : m[2][3] == 0.0f && m[3][3] == 1.0f;
}
static bool MPGeometry(const VxMatrix &m, int kind, const float *p) {
    bool ok = MPShape(m, kind);
    const bool perspective = kind == 0 || kind == 2;
    const double nearPlane = p[kind < 2 ? 2 : 4], farPlane = p[kind < 2 ? 3 : 5];
    for (double depth : {0.0, 0.5, 1.0}) for (double u : {-1.0, 0.0, 1.0}) for (double v : {-1.0, 0.0, 1.0}) {
        const double z = nearPlane + depth * (farPlane - nearPlane);
        double x, y;
        if (kind == 0) { x = u * std::tan(double(p[0]) * 0.5) * z; y = v * std::tan(double(p[0]) * 0.5) * z / p[1]; }
        else if (kind == 1) { x = u / p[0]; y = v / (double(p[0]) * p[1]); }
        else {
            x = 0.5 * ((1 - u) * p[0] + (1 + u) * p[1]);
            y = kind == 2 ? 0.5 * ((1 + v) * p[2] + (1 - v) * p[3]) : 0.5 * ((1 - v) * p[2] + (1 + v) * p[3]);
            if (perspective) { x *= z / nearPlane; y *= z / nearPlane; }
        }
        const double point[4] = {x, y, z, 1}; double clip[4] = {};
        for (int col = 0; col < 4; ++col) for (int row = 0; row < 4; ++row) clip[col] += point[row] * m[row][col];
        // Projective depth is nonlinear between the planes; use the ray's reciprocal-depth interpolation.
        const double expectedDepth = perspective ? depth * farPlane / z : depth;
        ok = ok && MPClose(clip[0] / clip[3], u) && MPClose(clip[1] / clip[3], v) && MPClose(clip[2] / clip[3], expectedDepth);
        ok = ok && MPClose(clip[3] / (perspective ? z : 1), 1);
    }
    return ok;
}
static void MPObserve(MPResult &r, bool ok) { ++r.samples; r.errors += !ok; }
static MPResult MPOrdinary(MPBuildFn build, int kind) {
    MPResult result;
    for (float first : {0.5f, 1.0f, 2.0f}) for (float aspect : {0.5f, 1.0f, 1.5f, 2.0f})
    for (float nearPlane : {0.25f, 1.0f, 4.0f}) for (float gap : {0.5f, 4.0f, 64.0f}) {
        const float p[6] = {first, aspect, nearPlane, nearPlane + gap, 0, 0};
        VxMatrix m; std::memset(&m, 0xff, 64);
        if (kind < 2) { build(m, kind, p); MPObserve(result, MPGeometry(m, kind, p)); }
        else {
            // Off-centre bounds and their reversed order test the original axis conventions.
            const float bounds[6] = {-first, aspect, first + aspect, -aspect, nearPlane, nearPlane + gap};
            build(m, kind, bounds); MPObserve(result, MPGeometry(m, kind, bounds));
            const float reversed[6] = {bounds[1], bounds[0], bounds[3], bounds[2], nearPlane, nearPlane + gap};
            build(m, kind, reversed); MPObserve(result, MPGeometry(m, kind, reversed));
        }
    }
    return result;
}
static MPResult MPScale(MPBuildFn build, int kind) {
    MPResult result;
    for (int exponent : {-100, -40, 0, 40, 100, 126}) {
        const float scale = std::ldexp(1.0f, exponent);
        float p[6] = {1.0f, 1.5f, 0.25f * scale, 0.75f * scale, 0, 0};
        if (kind == 1) p[0] = std::ldexp(2.0f, -exponent);
        if (kind >= 2) { p[0] = -0.25f * scale; p[1] = 0.75f * scale; p[2] = 0.5f * scale; p[3] = -0.5f * scale; p[4] = 0.25f * scale; p[5] = 0.75f * scale; }
        VxMatrix m; build(m, kind, p); MPObserve(result, MPGeometry(m, kind, p));
    }
    return result;
}
static MPResult MPExtreme(MPBuildFn build, int kind) {
    MPResult result;
    if (kind == 0) {
        for (int exponent : {-149, -140, -130, -126}) {
            const float tiny = std::ldexp(1.0f, exponent), p[6] = {tiny, tiny, 1, 3, 0, 0};
            VxMatrix m; build(m, kind, p); MPObserve(result, MPShape(m, kind) && m[1][1] == 2.0f && m[2][2] == 1.5f && m[3][2] == -1.5f);
        }
    } else if (kind == 1 || kind == 2) {
        for (int exponent = -149; exponent <= -128; ++exponent) {
            const float tiny = std::ldexp(1.0f, exponent);
            const float p[6] = {1, 1.5f, tiny, 2 * tiny, 0, 0};
            const float rect[6] = {-tiny, tiny, tiny, -tiny, tiny, 2 * tiny};
            VxMatrix m; build(m, kind, kind == 1 ? p : rect);
            bool ok = MPShape(m, kind);
            if (kind == 1) ok = ok && m[3][2] == -1 && m[2][2] == std::ldexp(1.0f, -exponent);
            else ok = ok && m[0][0] == 1 && m[1][1] == 1 && m[2][0] == 0 && m[2][1] == 0 && m[2][2] == 2 && m[3][2] == -2 * tiny;
            MPObserve(result, ok);
        }
    }
    if (kind >= 2) {
        const float maximum = FLT_MAX;
        for (bool centred : {false, true}) {
            const float p[6] = {centred ? -maximum : maximum / 2, maximum, maximum, centred ? -maximum : maximum / 2,
                kind == 2 ? maximum / 4 : 1, kind == 2 ? maximum / 2 : 3};
            VxMatrix m; build(m, kind, p); MPObserve(result, MPGeometry(m, kind, p));
        }
    }
    return result;
}
static MPResult MPDegenerate(MPBuildFn build) {
    MPResult result;
    for (int kind = 0; kind < 4; ++kind) {
        const float p[6] = {1, 1, 1, 1, 1, 1}; VxMatrix m; build(m, kind, p);
        MPObserve(result, MPShape(m, kind) && std::isinf(m[2][2]) && std::isinf(m[3][2]) && m[3][2] < 0);
        const float reversed[6] = {1, 1.5f, 3, 1, 3, 1}; const float rect[6] = {-2, 1, 3, -1, 3, 1};
        build(m, kind, kind < 2 ? reversed : rect); MPObserve(result, MPGeometry(m, kind, kind < 2 ? reversed : rect));
    }
    const float p[6] = {1, 0, 1, 3, 0, 0}; VxMatrix m; build(m, 0, p); MPObserve(result, m[1][1] == 0 && MPShape(m, 0));
    const float zero[6] = {0, 1, 1, 3, 0, 0}; build(m, 1, zero); MPObserve(result, m[0][0] == 0 && m[1][1] == 0 && MPShape(m, 1));
    return result;
}
static MPResult MPFootprint(MPBuildFn build) {
    MPResult result;
    for (int kind = 0; kind < 4; ++kind) for (int offset = 16; offset < 32; offset += 4) {
        alignas(16) unsigned char bytes[128]; std::memset(bytes, 0xa5, sizeof(bytes));
        VxMatrix *m = new (bytes + offset) VxMatrix;
        const float p[6] = {1, 1.5f, 1, 3, 0, 0}, rect[6] = {-1, 2, 3, -2, 1, 3};
        const float *parameters = kind < 2 ? p : rect; build(*m, kind, parameters); bool ok = MPGeometry(*m, kind, parameters);
        for (int i = 0; i < 128; ++i) if (i < offset || i >= offset + 64) ok = ok && bytes[i] == 0xa5;
        MPObserve(result, ok); m->~VxMatrix();
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 3 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old;
    if (!memory || !VirtualProtect(memory, size, PAGE_NOACCESS, &old) || !VirtualProtect(memory + 2 * size, size, PAGE_NOACCESS, &old)) {
        if (memory) VirtualFree(memory, 0, MEM_RELEASE); ++result.errors; return result;
    }
    for (int kind = 0; kind < 4; ++kind) for (unsigned offset : {size, 2 * size - 64}) {
        VxMatrix *m = new (memory + offset) VxMatrix;
        const float p[6] = {1, 1.5f, 1, 3, 0, 0}, rect[6] = {-1, 2, 3, -2, 1, 3}; const float *parameters = kind < 2 ? p : rect;
        build(*m, kind, parameters); MPObserve(result, MPGeometry(*m, kind, parameters)); m->~VxMatrix();
    }
    VirtualFree(memory, 0, MEM_RELEASE);
#endif
    return result;
}
#ifndef VX_MATRIX_PROJECTION_PROBE
static void MPExpect(MPResult r, int count) { EXPECT_EQ(r.samples, count); EXPECT_EQ(r.errors, 0); }
TEST(MatrixProjectionReference, PerspectiveMapsHorizontalRaysAndReciprocalDepth) { MPExpect(MPOrdinary(MPBuild, 0), 108); }
TEST(MatrixProjectionReference, OrthographicMapsZoomAspectAndLinearDepth) { MPExpect(MPOrdinary(MPBuild, 1), 108); }
TEST(MatrixProjectionReference, PerspectiveRectMapsOffCentreAndReversedEdges) { MPExpect(MPOrdinary(MPBuild, 2), 216); }
TEST(MatrixProjectionReference, OrthographicRectPreservesScreenYConvention) { MPExpect(MPOrdinary(MPBuild, 3), 216); }
TEST(MatrixProjectionReference, PerspectiveSceneScale) { MPExpect(MPScale(MPBuild, 0), 6); }
TEST(MatrixProjectionReference, OrthographicSceneScale) { MPExpect(MPScale(MPBuild, 1), 6); }
TEST(MatrixProjectionReference, PerspectiveRectSceneScale) { MPExpect(MPScale(MPBuild, 2), 6); }
TEST(MatrixProjectionReference, OrthographicRectSceneScale) { MPExpect(MPScale(MPBuild, 3), 6); }
TEST(MatrixProjectionReference, PerspectiveRetainsFiniteVerticalScaleWhenHorizontalOverflows) { MPExpect(MPExtreme(MPBuild, 0), 4); }
TEST(MatrixProjectionReference, OrthographicRetainsFiniteDepthTranslationWhenScaleOverflows) { MPExpect(MPExtreme(MPBuild, 1), 22); }
TEST(MatrixProjectionReference, PerspectiveRectAvoidsIntermediateOverflowInSmallAndLargeFrusta) { MPExpect(MPExtreme(MPBuild, 2), 24); }
TEST(MatrixProjectionReference, OrthographicRectAvoidsIntermediateBoundsOverflow) { MPExpect(MPExtreme(MPBuild, 3), 2); }
TEST(MatrixProjectionReference, OriginalDegenerateAndReversedDepthContracts) { MPExpect(MPDegenerate(MPBuild), 10); }
TEST(MatrixProjectionReference, OutputWriteFootprint) {
#if defined(_WIN32)
    MPExpect(MPFootprint(MPBuild), 24);
#else
    MPExpect(MPFootprint(MPBuild), 16);
#endif
}
#endif
