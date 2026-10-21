#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <new>
#ifndef VX_PLANE_SYSTEM_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_PLANE_SYSTEM_IMPLEMENTATION
#define VX_PLANE_SYSTEM_IMPLEMENTATION "../../src/VxIntersect_Plane.cpp"
#endif
#include VX_PLANE_SYSTEM_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef XBOOL (*PSBox)(const VxBbox &, const VxPlane &);
typedef XBOOL (*PSMatrixBox)(const VxBbox &, const VxMatrix &, const VxPlane &);
typedef XBOOL (*PSSolve)(const VxPlane &, const VxPlane &, const VxPlane &, VxVector &);
struct PSResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void PSObserve(PSResults &r, int actual, bool good) {
    if (!good) ++r.errors;
    r.digest = (r.digest ^ unsigned(actual)) * 16777619u;
    ++r.samples;
}
static void PSMatrix(VxMatrix &m, int kind) {
    m.SetIdentity();
    if (kind == 1) { m[0] = VxVector(0, 1, 0); m[1] = VxVector(-1, 0, 0); m[3] = VxVector(2, -4, 1); }
    if (kind == 2) { m[0] = VxVector(-2, 0, 0); m[1] = VxVector(0, 3, 0); m[2] = VxVector(0, 0, 0.5f); }
    if (kind == 3) { m[0] = VxVector(1, 0.5f, 0); m[1] = VxVector(0, 1, -1); m[2] = VxVector(0.25f, 0, 1); m[3] = VxVector(1, 2, -3); }
    if (kind == 4) { m[0] = VxVector(0, 0, 0); m[1] = VxVector(0, 2, 0); m[2] = VxVector(0, 0, -1); m[3] = VxVector(2, -1, 3); }
}
static bool PSCornerIntersection(const VxBbox &box, const VxPlane &p, const VxMatrix *matrix) {
    double low = 0, high = 0;
    for (int corner = 0; corner < 8; ++corner) {
        const double local[] = {corner & 1 ? box.Max.x : box.Min.x, corner & 2 ? box.Max.y : box.Min.y, corner & 4 ? box.Max.z : box.Min.z};
        double distance = p.m_D;
        for (int axis = 0; axis < 3; ++axis) {
            double world = local[axis];
            if (matrix) {
                world = (*matrix)[3][axis];
                for (int row = 0; row < 3; ++row) world += local[row] * (*matrix)[row][axis];
            }
            distance += world * p.m_Normal[axis];
        }
        if (!corner || distance < low) low = distance;
        if (!corner || distance > high) high = distance;
    }
    return low <= 0 && high >= 0;
}
static PSResults PSBoxes(PSBox plain, PSMatrixBox transformed) {
    PSResults r;
    const VxVector extents[] = {VxVector(0, 0, 0), VxVector(1, 2, 3), VxVector(2, 0, 1), VxVector(0.5f, 1.5f, 2.5f)};
    for (int nx = -1; nx <= 1; ++nx) for (int ny = -1; ny <= 1; ++ny) for (int nz = -1; nz <= 1; ++nz) {
        if (!(nx || ny || nz)) continue;
        for (int d = -6; d <= 6; d += 3) for (int x = -4; x <= 4; x += 4) for (int y = -4; y <= 4; y += 4) for (int z = -4; z <= 4; z += 4)
            for (const VxVector &extent : extents) {
                VxPlane p; p.m_Normal = VxVector(float(nx), float(ny), float(nz)); p.m_D = float(d);
                const VxVector center{float(x), float(y), float(z)};
                const VxBbox box(center - extent, center + extent);
                for (int kind = 0; kind < (transformed ? 5 : 1); ++kind) {
                    VxMatrix m; PSMatrix(m, kind);
                    const VxBbox before = box; const VxPlane beforePlane = p; const VxMatrix beforeMatrix = m;
                    const int actual = transformed ? transformed(box, m, p) : plain(box, p);
                    PSObserve(r, actual, actual == int(PSCornerIntersection(box, p, transformed ? &m : nullptr)) && !std::memcmp(&before, &box, sizeof(box)) && !std::memcmp(&beforePlane, &p, sizeof(p)) && !std::memcmp(&beforeMatrix, &m, sizeof(m)));
                }
            }
    }
    return r;
}
static PSResults PSWideBoxes(PSBox plain, PSMatrixBox transformed) {
    PSResults r;
    VxPlane p; p.m_Normal = VxVector(FLT_MAX, -FLT_MAX, 0); p.m_D = 0;
    const VxBbox point(VxVector(2, 2, 0), VxVector(2, 2, 0));
    int actual = plain(point, p); PSObserve(r, actual, actual == 1);
    VxMatrix m; m.SetIdentity();
    actual = transformed(point, m, p); PSObserve(r, actual, actual == 1);
    m[0] = VxVector(2, 2, 0); m[1] = VxVector(0, 0, 0); m[2] = VxVector(0, 0, 0);
    const VxBbox box(VxVector(-2, -1, -1), VxVector(2, 1, 1));
    actual = transformed(box, m, p); PSObserve(r, actual, actual == 1);
    p.m_Normal = VxVector(1, 0, 0);
    m.SetIdentity(); m[0] = VxVector(2, 0, 0); m[1] = VxVector(-2, 0, 0);
    const VxBbox farPoint(VxVector(FLT_MAX, FLT_MAX, 0), VxVector(FLT_MAX, FLT_MAX, 0));
    actual = transformed(farPoint, m, p); PSObserve(r, actual, actual == 1);
    // Original x87 24-bit grouping drops z before the large xy terms cancel.
    // All final values here are exactly representable, including contact at 0.
    p.m_Normal = VxVector(std::ldexp(1.0f, 30), -std::ldexp(1.0f, 30), 1); p.m_D = -1;
    const VxBbox contact(VxVector(2, 2, 1), VxVector(2, 2, 1));
    m.SetIdentity();
    actual = plain(contact, p); PSObserve(r, actual, actual == 1);
    actual = transformed(contact, m, p); PSObserve(r, actual, actual == 1);
    return r;
}

// Independent partial-pivot elimination, unlike the production cross products
// and Cramer's rule. A singular system has no unique point, even if consistent.
static bool PSEliminate(const VxPlane *planes, double *point) {
    double rows[3][4];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) rows[i][j] = planes[i].m_Normal[j];
        rows[i][3] = -double(planes[i].m_D);
    }
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        for (int i = col + 1; i < 3; ++i) if (std::fabs(rows[i][col]) > std::fabs(rows[pivot][col])) pivot = i;
        if (rows[pivot][col] == 0) return false;
        for (int j = 0; j < 4; ++j) { const double saved = rows[col][j]; rows[col][j] = rows[pivot][j]; rows[pivot][j] = saved; }
        const double divisor = rows[col][col];
        for (int j = col; j < 4; ++j) rows[col][j] /= divisor;
        for (int i = 0; i < 3; ++i) if (i != col) {
            const double factor = rows[i][col];
            for (int j = col; j < 4; ++j) rows[i][j] -= factor * rows[col][j];
        }
    }
    for (int i = 0; i < 3; ++i) point[i] = rows[i][3];
    return true;
}
static void PSSolveObserve(PSResults &r, PSSolve fn, const VxPlane *planes) {
    double expected[3];
    const bool unique = PSEliminate(planes, expected);
    VxVector point(101, -203, 307);
    const int actual = fn(planes[0], planes[1], planes[2], point);
    bool good = actual == int(unique);
    for (int i = 0; i < 3; ++i)
        good = good && (unique ? std::isfinite(point[i]) && std::fabs(double(point[i]) - expected[i]) <= 4e-6 * (1 + std::fabs(expected[i])) : point[i] == (i == 0 ? 101 : i == 1 ? -203 : 307));
    if (!good && r.errors < 2) std::fprintf(stderr, "solve hit=%d/%d point=(%.9g,%.9g,%.9g)\n", actual, int(unique), double(point.x), double(point.y), double(point.z));
    PSObserve(r, actual, good);
}
static void PSSetPlane(VxPlane &plane, const VxVector &normal, const VxVector &point, float scale = 1) {
    plane.m_Normal = normal * scale;
    plane.m_D = -float(double(normal.x) * point.x + double(normal.y) * point.y + double(normal.z) * point.z) * scale;
}
static const int psPermutations[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
static PSResults PSSystems(PSSolve fn, bool scaled) {
    PSResults r;
    const VxVector bases[][3] = {{VxVector(1, 0, 0), VxVector(0, 1, 0), VxVector(0, 0, 1)}, {VxVector(1, 2, -1), VxVector(-1, 1, 2), VxVector(2, -1, 1)}, {VxVector(1, 0, 0), VxVector(1, std::ldexp(1.0f, -20), 0), VxVector(0, 0, 1)}, {VxVector(1, 2, 3), VxVector(2, 4, 6), VxVector(0, 0, 1)}};
    const int powers[] = {-100, -80, -40, -10, 0, 10, 40, 80, 100};
    if (!scaled) {
        for (const auto &basis : bases) for (int x = -2; x <= 2; ++x) for (int y = -2; y <= 2; ++y) for (int z = -2; z <= 2; ++z) {
            VxPlane p[3];
            for (int i = 0; i < 3; ++i) PSSetPlane(p[i], basis[i], VxVector(float(x), float(y), float(z)));
            for (const auto &order : psPermutations) { VxPlane permuted[] = {p[order[0]], p[order[1]], p[order[2]]}; PSSolveObserve(r, fn, permuted); }
        }
    } else {
        for (int kind = 0; kind < 2; ++kind) for (int exponent : powers) for (int pattern = 0; pattern < 3; ++pattern) for (int signs = 0; signs < 8; ++signs) {
            VxPlane p[3];
            for (int i = 0; i < 3; ++i) {
                const int power = pattern == 0 ? exponent : pattern == 1 ? (i == 1 ? -exponent : exponent) : (i == 0 ? exponent : 0);
                PSSetPlane(p[i], bases[kind][i], VxVector(1, 2, -3), std::ldexp(signs & (1 << i) ? -1.0f : 1.0f, power));
            }
            for (const auto &order : psPermutations) { VxPlane permuted[] = {p[order[0]], p[order[1]], p[order[2]]}; PSSolveObserve(r, fn, permuted); }
        }
    }
    return r;
}
#if defined(_WIN32)
static PSResults PSFootprints(PSBox plain, PSMatrixBox transformed, PSSolve solve) {
    PSResults r;
    SYSTEM_INFO info; GetSystemInfo(&info); const std::size_t page = info.dwPageSize;
    unsigned char *regions[6] = {};
    for (int i = 0; i < 6; ++i) {
        regions[i] = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!regions[i]) { ++r.errors; break; }
        DWORD old; if (!VirtualProtect(regions[i] + page, page, PAGE_NOACCESS, &old)) ++r.errors;
    }
    if (!r.errors) {
        VxBbox *box = new(regions[0] + page - sizeof(VxBbox)) VxBbox(VxVector(-1, -1, -1), VxVector(1, 1, 1));
        VxMatrix *matrix = new(regions[1] + page - sizeof(VxMatrix)) VxMatrix; matrix->SetIdentity();
        VxPlane *planes[3];
        for (int i = 0; i < 3; ++i) {
            planes[i] = new(regions[i + 2] + page - sizeof(VxPlane)) VxPlane;
            planes[i]->m_Normal = VxVector(float(i == 0), float(i == 1), float(i == 2));
            planes[i]->m_D = float(i == 0 ? 0 : i == 1 ? -2 : -3);
        }
        VxVector *point = new(regions[5] + page - sizeof(VxVector)) VxVector(101, -203, 307);
        for (int i = 0; i < 5; ++i) { DWORD old; if (!VirtualProtect(regions[i], page, PAGE_READONLY, &old)) ++r.errors; }
        int actual = plain(*box, *planes[0]); PSObserve(r, actual, actual == 1);
        actual = transformed(*box, *matrix, *planes[0]); PSObserve(r, actual, actual == 1);
        actual = solve(*planes[0], *planes[1], *planes[2], *point); PSObserve(r, actual, actual == 1 && point->x == 0 && point->y == 2 && point->z == 3);
    }
    for (unsigned char *region : regions) if (region && !VirtualFree(region, 0, MEM_RELEASE)) ++r.errors;
    return r;
}
#endif
#ifndef VX_PLANE_SYSTEM_NATIVE_PROBE
static void PSCheck(PSResults r) { EXPECT_EQ(r.errors, 0); ::testing::Test::RecordProperty("samples", r.samples); ::testing::Test::RecordProperty("digest", std::to_string(r.digest)); }
TEST(PlaneSystemReference, ScalarAabbEightCornersAndTangency) { PSCheck(PSBoxes(BoxPlaneScalarCore, nullptr)); }
TEST(PlaneSystemReference, ScalarAffineBoxEightCornersIncludingShearAndCollapsedAxes) { PSCheck(PSBoxes(nullptr, BoxPlaneMatrixScalarCore)); }
TEST(PlaneSystemReference, ScalarFiniteWideBoxes) { PSCheck(PSWideBoxes(BoxPlaneScalarCore, BoxPlaneMatrixScalarCore)); }
TEST(PlaneSystemReference, ScalarSystemsPermutationsSingularityAndSmallDeterminant) { PSCheck(PSSystems(PlanesScalarCore, false)); }
TEST(PlaneSystemReference, ScalarEquivalentSystemsAcrossSignedScales) { PSCheck(PSSystems(PlanesScalarCore, true)); }
#if defined(VX_SIMD_SSE)
TEST(PlaneSystemReference, SimdAabbEightCornersAndTangency) { PSCheck(PSBoxes(BoxPlaneSIMDCore, nullptr)); }
TEST(PlaneSystemReference, SimdAffineBoxEightCornersIncludingShearAndCollapsedAxes) { PSCheck(PSBoxes(nullptr, BoxPlaneMatrixSIMDCore)); }
TEST(PlaneSystemReference, SimdFiniteWideBoxes) { PSCheck(PSWideBoxes(BoxPlaneSIMDCore, BoxPlaneMatrixSIMDCore)); }
TEST(PlaneSystemReference, SimdSystemsPermutationsSingularityAndSmallDeterminant) { PSCheck(PSSystems(PlanesSIMDCore, false)); }
TEST(PlaneSystemReference, SimdEquivalentSystemsAcrossSignedScales) { PSCheck(PSSystems(PlanesSIMDCore, true)); }
#endif
TEST(PlaneSystemReference, PublicTablesAndFailureOutput) {
    const PSBox box = VxIntersect::BoxPlane; const PSMatrixBox transformed = VxIntersect::BoxPlane;
    for (int i = 0; i < 4; ++i) {
        VxIntersectPlaneDispatchRebuild(i & 1);
        EXPECT_EQ(PSSystems(VxIntersect::Planes, true).errors, 0);
        EXPECT_EQ(PSWideBoxes(box, transformed).errors, 0);
    }
    VxIntersectPlaneDispatchRebuild(false);
    PSCheck(PSSystems(VxIntersect::Planes, false));
}
#if defined(_WIN32)
TEST(PlaneSystemReference, ExactReadonlyFootprintsAndTwelveByteOutput) {
    PSCheck(PSFootprints(BoxPlaneScalarCore, BoxPlaneMatrixScalarCore, PlanesScalarCore));
#if defined(VX_SIMD_SSE)
    EXPECT_EQ(PSFootprints(BoxPlaneSIMDCore, BoxPlaneMatrixSIMDCore, PlanesSIMDCore).errors, 0);
#endif
}
TEST(PlaneSystemReference, ActualCurrentDllExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *boxName = "?BoxPlane@VxIntersect@@SAHAEBUVxBbox@@AEBVVxPlane@@@Z", *matrixName = "?BoxPlane@VxIntersect@@SAHAEBUVxBbox@@AEBVVxMatrix@@AEBVVxPlane@@@Z", *solveName = "?Planes@VxIntersect@@SAHAEBVVxPlane@@00AEAUVxVector@@@Z";
#else
    const char *boxName = "?BoxPlane@VxIntersect@@SAHABUVxBbox@@ABVVxPlane@@@Z", *matrixName = "?BoxPlane@VxIntersect@@SAHABUVxBbox@@ABVVxMatrix@@ABVVxPlane@@@Z", *solveName = "?Planes@VxIntersect@@SAHABVVxPlane@@00AAUVxVector@@@Z";
#endif
    PSBox box = reinterpret_cast<PSBox>(GetProcAddress(dll, boxName)); PSMatrixBox transformed = reinterpret_cast<PSMatrixBox>(GetProcAddress(dll, matrixName)); PSSolve solve = reinterpret_cast<PSSolve>(GetProcAddress(dll, solveName));
    ASSERT_NE(box, nullptr); ASSERT_NE(transformed, nullptr); ASSERT_NE(solve, nullptr);
    PSCheck(PSBoxes(box, nullptr)); EXPECT_EQ(PSBoxes(nullptr, transformed).errors, 0); EXPECT_EQ(PSWideBoxes(box, transformed).errors, 0); EXPECT_EQ(PSSystems(solve, false).errors, 0); EXPECT_EQ(PSSystems(solve, true).errors, 0);
    EXPECT_NE(FreeLibrary(dll), 0);
}
#endif
#endif
