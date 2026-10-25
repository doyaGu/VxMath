#include <cmath>
#include <cstring>
#include <cfloat>
#ifndef VX_EULER_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MEFromFn)(VxMatrix &, float, float, float);
typedef void (*METoFn)(const VxMatrix &, float *, float *, float *);
struct MEResult { int calls = 0, errors = 0, faults = 0; };

static bool MENear(double actual, double expected, double tolerance = 7e-7) {
    return std::isfinite(actual) && std::fabs(actual - expected) <= tolerance;
}

// Compose three independent row-vector axis rotations, without the runtime's
// expanded Euler formula or quaternion conversion.
static void MECompose(float x, float y, float z, double (&out)[3][3]) {
    const float angles[] = {x, y, z};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = i == j ? 1.0 : 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        const int p = (axis + 1) % 3, q = (p + 1) % 3;
        const bool snapped = std::fabs(double(angles[axis])) <= double(1e-8f);
        double s = snapped ? 0 : std::sin(double(angles[axis]));
        double c = snapped ? 1 : std::cos(double(angles[axis]));
        // Independently reduced with 190-digit Decimal/Machin pi. The Win32
        // compiler may inline fsin/fcos, whose large-argument range is limited.
        const float large[] = {1e10f, 1e20f, 1e30f, FLT_MAX};
        const double sine[] = {-0.4875060250875107, 0.6565766778545903, -0.7911634385219837, -0.5218765233336585};
        const double cosine[] = {0.873119622676856, 0.7542592830701055, -0.6116047854178966, 0.8530210398303042};
        for (int i = 0; i < 4; ++i) if (std::fabs(angles[axis]) == large[i]) {
            s = angles[axis] < 0 ? -sine[i] : sine[i]; c = cosine[i];
        }
        for (int row = 0; row < 3; ++row) {
            const double a = out[row][p], b = out[row][q];
            out[row][p] = a*c - b*s;
            out[row][q] = a*s + b*c;
        }
    }
}

static bool MEMatrix(const VxMatrix &m, const double (&expected)[3][3]) {
    bool ok = true;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) ok = ok && MENear(m[i][j], expected[i][j]);
        ok = ok && m[i][3] == 0 && m[3][i] == 0;
    }
    return ok && m[3][3] == 1;
}

static MEResult MEFromCases(MEFromFn fn, int kind) {
    MEResult r;
    const float ordinary[] = {-2.7f, -0.8f, 0, 0.3f, 1.4f};
    const float boundary[] = {-std::nextafter(1e-8f, FLT_MAX), -1e-8f,
        -std::nextafter(1e-8f, 0.0f), -0.0f, 0.0f,
        std::nextafter(1e-8f, 0.0f), 1e-8f, std::nextafter(1e-8f, FLT_MAX)};
    const float large[] = {-1e10f, -1e6f, -1000, -64, 64, 1000, 1e6f, 1e10f};
    const int count = kind == 0 ? 125 : kind == 1 ? 24 : 24;
    for (int n = 0; n < count; ++n) {
        float a[3] = {0.3f, -0.8f, 1.4f};
        if (kind == 0) { a[0] = ordinary[n/25]; a[1] = ordinary[(n/5)%5]; a[2] = ordinary[n%5]; }
        else a[n%3] = kind == 1 ? boundary[n/3] : large[n/3];
        VxMatrix out;
        std::memset(&out, 0xcd, sizeof(out));
        fn(out, a[0], a[1], a[2]);
        ++r.calls;
        double expected[3][3]; MECompose(a[0], a[1], a[2], expected);
        r.errors += !MEMatrix(out, expected);
        if (kind == 1 && std::fabs(a[n%3]) <= 1e-8f) {
            float zeroed[3] = {a[0], a[1], a[2]}; zeroed[n%3] = 0;
            VxMatrix same; fn(same, zeroed[0], zeroed[1], zeroed[2]); ++r.calls;
            r.errors += std::memcmp(&out, &same, 64) != 0;
        }
    }
    return r;
}

static void MEExpected(const VxMatrix &m, float (&v)[3]) {
    const double radial = std::hypot(double(m[0][0]), double(m[0][1]));
    const bool locked = radial <= std::ldexp(1.0, -19);
    v[0] = float(locked ? std::atan2(-double(m[2][1]), double(m[1][1]))
                        : std::atan2(double(m[1][2]), double(m[2][2])));
    v[1] = float(std::atan2(-double(m[0][2]), radial));
    v[2] = locked ? 0 : float(std::atan2(double(m[0][1]), double(m[0][0])));
}

static bool MEAngles(const float *actual, const float *expected) {
    for (int i = 0; i < 3; ++i) if (!MENear(actual[i], expected[i], 3e-7)) return false;
    return true;
}

static MEResult MEExtract(METoFn fn, int kind) {
    MEResult r;
    const float sizes[] = {1, FLT_MAX, std::ldexp(1.0f, -149),
        std::ldexp(1.0f, -100), std::ldexp(1.0f, 100)};
    const float edge = std::ldexp(1.0f, -19);
    const float boundaries[] = {std::nextafter(edge, 0.0f), edge, std::nextafter(edge, FLT_MAX)};
    const int count = kind == 0 ? 64 : kind == 1 ? 64*4 : 12;
    for (int n = 0; n < count; ++n) {
        VxMatrix m; m.SetIdentity();
        const int signs = n%64;
        const float s = sizes[kind == 1 ? n/64 + 1 : 0];
        m[0][0] = signs&1 ? -s : s; m[0][1] = signs&2 ? -s : s;
        m[0][2] = signs&4 ? -s : s;
        m[1][2] = signs&8 ? -3.0f : 3.0f; m[2][2] = signs&16 ? -4.0f : 4.0f;
        m[2][1] = signs&32 ? -2.0f : 2.0f; m[1][1] = 5;
        if (kind == 2) {
            const float value = boundaries[n%3];
            m[0][0] = n&4 ? 0 : value; m[0][1] = n&4 ? value : 0;
            if (n&8) { m[0][0] *= -1; m[0][1] *= -1; }
        }
        float expected[3], actual[3]; MEExpected(m, expected);
        unsigned before[16]; std::memcpy(before, &m, 64);
        fn(m, actual, actual+1, actual+2); ++r.calls;
        r.errors += !MEAngles(actual, expected) || std::memcmp(before, &m, 64) != 0;
    }
    return r;
}

static MEResult MEReconstruct(METoFn fn) {
    MEResult r;
    const float xs[] = {-2.4f, -0.5f, 0, 0.8f, 2.7f};
    const float ys[] = {-1.57079632679f, -1.5707954f, -1.2f, 0, 1.2f, 1.5707954f, 1.57079632679f};
    for (float x : xs) for (float y : ys) for (float z : xs) {
        double source[3][3]; MECompose(x, y, z, source);
        VxMatrix m; m.SetIdentity();
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) m[i][j] = float(source[i][j]);
        float a[3]; fn(m, a, a+1, a+2); ++r.calls;
        double rebuilt[3][3]; MECompose(a[0], a[1], a[2], rebuilt);
        bool ok = true;
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) ok = ok && MENear(rebuilt[i][j], source[i][j], 3e-6);
        r.errors += !ok;
    }
    return r;
}

static MEResult MEFields(METoFn fn) {
    MEResult r;
    const int ignored[] = {3, 4, 7, 8, 11, 12, 13, 14, 15};
    const unsigned values[] = {0, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f800001u};
    for (int locked = 0; locked < 2; ++locked) for (int field : ignored) for (unsigned value : values) {
        VxMatrix m; m.SetIdentity(); m[0][0] = locked ? 0 : 1; m[0][1] = 0;
        m[0][2] = -1; m[1][2] = 3; m[2][2] = 4; m[2][1] = -2; m[1][1] = 5;
        float expected[3]; MEExpected(m, expected);
        std::memcpy(reinterpret_cast<unsigned char *>(&m)+4*field, &value, 4);
        unsigned before[16]; std::memcpy(before, &m, 64);
        float actual[3]; fn(m, actual, actual+1, actual+2); ++r.calls;
        r.errors += !MEAngles(actual, expected) || std::memcmp(before, &m, 64) != 0;
    }
    return r;
}

static MEResult MEShared(METoFn fn, bool inputAlias, bool optional) {
    MEResult r;
    for (int locked = 0; locked < 2; ++locked) for (int layout = 0; layout < (optional ? 8 : inputAlias ? 7 : 5); ++layout) {
        VxMatrix m; m.SetIdentity(); m[0][0] = locked ? 0 : 1; m[0][1] = 0;
        m[0][2] = -1; m[1][2] = 3; m[2][2] = 4; m[2][1] = -2; m[1][1] = 5;
        float expected[3], out[3] = {71, 72, 73}, model[3] = {71, 72, 73}; MEExpected(m, expected);
        const int indices[][3] = {{0,1,2}, {0,0,1}, {0,1,0}, {0,1,1}, {0,0,0}};
        if (inputAlias) {
            const int fields[] = {0, 1, 2, 6, 10, 9, 5};
            float *dest = reinterpret_cast<float *>(&m)+fields[layout];
            // Snapshot semantics: every result describes the matrix at entry.
            fn(m, dest, out+1, out+2); ++r.calls;
            r.errors += !MENear(*dest, expected[0]) || !MENear(out[1], expected[1]) || !MENear(out[2], expected[2]);
        } else {
            const int *ix = indices[optional ? 0 : layout];
            float *p[3];
            for (int i = 0; i < 3; ++i) p[i] = optional && !(layout & (1<<i)) ? nullptr : out+ix[i];
            for (int i = 0; i < 3; ++i) if (p[i]) model[ix[i]] = expected[i];
            fn(m, p[0], p[1], p[2]); ++r.calls;
            r.errors += !MEAngles(out, model);
        }
    }
    return r;
}

#if defined(_WIN32)
static bool MEGuardCall(MEFromFn from, METoFn to, VxMatrix *m, float *out) {
    __try {
        if (from) from(*m, 0.3f, -0.8f, 1.4f);
        else to(*m, out, out+1, out+2);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#endif

static MEResult MEFootprint(MEFromFn from, METoFn to) {
    MEResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *mem = static_cast<unsigned char *>(VirtualAlloc(nullptr, page*3, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
    if (!mem) { ++r.errors; return r; }
    DWORD old; VirtualProtect(mem, page, PAGE_NOACCESS, &old); VirtualProtect(mem+2*page, page, PAGE_NOACCESS, &old);
    for (int edge = 0; edge < 2; ++edge) {
        VxMatrix *m = reinterpret_cast<VxMatrix *>(mem + (edge ? 2*page-64 : page));
        if (!from) { VxMatrix source; source.SetIdentity(); std::memcpy(m, &source, 64); VirtualProtect(mem+page, page, PAGE_READONLY, &old); }
        float out[3]; ++r.calls; const bool ok = MEGuardCall(from, to, m, out); r.faults += !ok;
        r.errors += !ok || (from ? !MENear((*m)[3][3], 1) : !MENear(out[0], 0) || !MENear(out[1], 0) || !MENear(out[2], 0));
        VirtualProtect(mem+page, page, PAGE_READWRITE, &old);
    }
    VirtualFree(mem, 0, MEM_RELEASE);
#endif
    return r;
}

#ifndef VX_EULER_PROBE
TEST(MatrixEulerReference, ComposeIndependentAxisRotations) { EXPECT_EQ(MEFromCases(Vx3DMatrixFromEulerAngles, 0).errors, 0); }
TEST(MatrixEulerReference, PreserveOriginalTinyAngleSnap) { EXPECT_EQ(MEFromCases(Vx3DMatrixFromEulerAngles, 1).errors, 0); }
TEST(MatrixEulerReference, PreserveMultipleTurnRotations) { EXPECT_EQ(MEFromCases(Vx3DMatrixFromEulerAngles, 2).errors, 0); }
TEST(MatrixEulerReference, ExtractSignedRegularAngles) { EXPECT_EQ(MEExtract(Vx3DMatrixToEulerAngles, 0).errors, 0); }
TEST(MatrixEulerReference, RetainFiniteNormAcrossFloatRange) { EXPECT_EQ(MEExtract(Vx3DMatrixToEulerAngles, 1).errors, 0); }
TEST(MatrixEulerReference, IncludeExactGimbalThresholdInLockedBranch) { EXPECT_EQ(MEExtract(Vx3DMatrixToEulerAngles, 2).errors, 0); }
TEST(MatrixEulerReference, ReconstructRegularAndGimbalRotations) { EXPECT_EQ(MEReconstruct(Vx3DMatrixToEulerAngles).errors, 0); }
TEST(MatrixEulerReference, IgnoreUnusedFieldsAndKeepInput) { EXPECT_EQ(MEFields(Vx3DMatrixToEulerAngles).errors, 0); }
TEST(MatrixEulerReference, WriteSharedOutputsInXYZOrder) { EXPECT_EQ(MEShared(Vx3DMatrixToEulerAngles, false, false).errors, 0); }
TEST(MatrixEulerReference, SnapshotInputBeforeOutputWrites) { EXPECT_EQ(MEShared(Vx3DMatrixToEulerAngles, true, false).errors, 0); }
TEST(MatrixEulerReference, PreserveOptionalOutputExtension) { EXPECT_EQ(MEShared(Vx3DMatrixToEulerAngles, false, true).errors, 0); }
TEST(MatrixEulerReference, RespectMatrixFootprintAndReadOnlyInput) {
    const MEResult a = MEFootprint(Vx3DMatrixFromEulerAngles, nullptr), b = MEFootprint(nullptr, Vx3DMatrixToEulerAngles);
    EXPECT_EQ(a.errors+b.errors+a.faults+b.faults, 0);
}
#endif
