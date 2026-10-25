#include <cmath>
#include <cstring>
#include <cfloat>
#ifndef VX_AXIS_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MARFromFn)(VxMatrix &, const VxVector &, float);
typedef void (*MARPivotFn)(VxMatrix &, const VxVector &, const VxVector &, float);
struct MARResult { int calls = 0, errors = 0, faults = 0; };

static void MARTrig(float angle, double &s, double &c) {
    s = std::sin(double(angle)); c = std::cos(double(angle));
    // 190-digit Decimal/Machin-pi reference, independent of Win32 fsin/fcos.
    const float large[] = {1e10f, 1e20f, 1e30f, FLT_MAX};
    const double sine[] = {-0.4875060250875107, 0.6565766778545903, -0.7911634385219837, -0.5218765233336585};
    const double cosine[] = {0.873119622676856, 0.7542592830701055, -0.6116047854178966, 0.8530210398303042};
    for (int i = 0; i < 4; ++i) if (std::fabs(angle) == large[i]) {
        s = angle < 0 ? -sine[i] : sine[i]; c = cosine[i];
    }
}

static bool MARLinear(const VxMatrix &m, const VxVector &axis, float angle) {
    const double length = std::hypot(std::hypot(double(axis.x), double(axis.y)), double(axis.z));
    const double n[] = {axis.x/length, axis.y/length, axis.z/length};
    double s, c; MARTrig(angle, s, c);
    // Transform each basis vector by its axis projection and perpendicular
    // plane rotation. Positive angle has the original opposite Euler sign.
    bool ok = true;
    for (int basis = 0; basis < 3; ++basis) {
        const double v[] = {basis == 0 ? 1.0 : 0, basis == 1 ? 1.0 : 0, basis == 2 ? 1.0 : 0};
        const double cross[] = {v[1]*n[2]-v[2]*n[1], v[2]*n[0]-v[0]*n[2], v[0]*n[1]-v[1]*n[0]};
        for (int j = 0; j < 3; ++j) {
            const double parallel = n[basis]*n[j];
            const double expected = parallel + c*(v[j]-parallel) + s*cross[j];
            ok = ok && std::isfinite(m[basis][j]) && std::fabs(m[basis][j]-expected) <= 7e-7;
        }
    }
    return ok;
}

static bool MARAffine(const VxMatrix &m) {
    return m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0 && m[3][3] == 1;
}

static bool MARCoordinate(float actual, double expected) {
    const float rounded = float(expected);
    if (!std::isfinite(rounded)) return actual == rounded;
    const double ulp = std::fmax(std::fabs(double(std::nextafter(rounded, 0.0f))-rounded), std::ldexp(1.0, -149));
    return std::isfinite(actual) && std::fabs(double(actual)-rounded) <= 2*ulp;
}

static double MARTranslation(const VxMatrix &m, const float *origin, int column) {
    // Evaluate the affine point equation in an independent subtraction order.
    // Inputs in the unit fixture are bounded dyadic values; a separate exact
    // Fraction oracle covers wide exponent gaps and cancellation captures.
    double value = origin[column];
    for (int row = 0; row < 3; ++row) value -= double(origin[row])*m[row][column];
    return value;
}

static MARResult MARAxes(MARFromFn fn, int kind) {
    MARResult r;
    const VxVector axes[] = {VxVector(1,0,0),VxVector(0,1,0),VxVector(0,0,1),VxVector(1,2,3),VxVector(-3,1,-2)};
    const float angles[] = {-2.7f,-0.7f,0,1e-9f,0.3f,0.7f,2.4f};
    const float scales[] = {std::ldexp(1.0f,-149),std::ldexp(1.0f,-100),1,std::ldexp(1.0f,100),FLT_MAX/4};
    const float large[] = {-1e10f,-1e6f,-1000,-64,64,1000,1e6f,1e10f};
    for (const VxVector &base : axes) for (int i = 0; i < (kind == 2 ? 8 : 7); ++i)
        for (int k = 0; k < (kind == 1 ? 5 : 1); ++k) {
            const float angle = kind == 2 ? large[i] : angles[i];
            const float scale = kind == 1 ? scales[k] : 1;
            const VxVector axis(base.x*scale,base.y*scale,base.z*scale);
            VxMatrix m; std::memset(&m,0xcd,64); fn(m,axis,angle); ++r.calls;
            r.errors += !MARLinear(m,axis,angle) || !MARAffine(m) || m[3][0] != 0 || m[3][1] != 0 || m[3][2] != 0;
        }
    return r;
}

static MARResult MARPivots(MARPivotFn fn, bool wide) {
    MARResult r;
    const VxVector axes[] = {VxVector(1,0,0),VxVector(0,0,1),VxVector(1,2,3),VxVector(-3,1,-2)};
    const float angles[] = {-2.7f,-0.7f,0,0.3f,2.4f};
    const float scales[] = {std::ldexp(1.0f,-149),std::ldexp(1.0f,-120),std::ldexp(1.0f,80),std::ldexp(1.0f,120),FLT_MAX/8};
    const VxVector points[] = {VxVector(1,2,3),VxVector(7,-4,-2),VxVector(-9,3,1),VxVector(-1,0,0),VxVector(100,111,-99)};
    for (const VxVector &axis : axes) for (float angle : angles) for (int k = 0; k < 5; ++k) {
        const VxVector origin = wide ? VxVector(scales[k],2*scales[k],-3*scales[k]) : points[k];
        const float p[] = {origin.x,origin.y,origin.z};
        VxMatrix m; std::memset(&m,0xcd,64); fn(m,axis,origin,angle); ++r.calls;
        bool ok = MARLinear(m,axis,angle) && MARAffine(m);
        for (int j = 0; j < 3; ++j) ok = ok && MARCoordinate(m[3][j], MARTranslation(m,p,j));
        r.errors += !ok;
    }
    return r;
}

static MARResult MARAliases(MARPivotFn fn) {
    MARResult r;
    const float angles[] = {0,0.7f,2.4f};
    for (float angle : angles) for (int axisRow = -1; axisRow < 4; ++axisRow) for (int pivotRow = -1; pivotRow < 4; ++pivotRow) {
        VxMatrix m;
        const float values[4][4] = {{1,2,3,31},{-3,1,2,32},{2,-4,1,33},{7,-3,2,34}};
        std::memcpy(&m,values,64);
        VxVector axis(1,2,3),pivot(7,-4,-2);
        const VxVector &axisRef = axisRow < 0 ? axis : static_cast<const VxVector &>(m[axisRow]);
        const VxVector &pivotRef = pivotRow < 0 ? pivot : static_cast<const VxVector &>(m[pivotRow]);
        const VxVector capturedAxis(axisRef.x,axisRef.y,axisRef.z);
        float p[] = {pivotRef.x,pivotRef.y,pivotRef.z};
        fn(m,axisRef,pivotRef,angle); ++r.calls;
        if (pivotRow >= 0 && pivotRow < 3) for (int j = 0; j < 3; ++j) p[j] = m[pivotRow][j];
        bool ok = MARLinear(m,capturedAxis,angle) && MARAffine(m);
        for (int j = 0; j < 3; ++j) {
            const double value = MARTranslation(m,p,j); ok = ok && MARCoordinate(m[3][j],value);
            if (pivotRow == 3) p[j] = float(value);
        }
        r.errors += !ok;
    }
    return r;
}

static MARResult MARTiny(MARFromFn fn) {
    MARResult r;
    for (float angle : {-1e-9f,1e-9f}) {
        VxMatrix m; fn(m,VxVector(0,0,1),angle); ++r.calls;
        r.errors += m[0][1] != -angle || m[1][0] != angle || !MARAffine(m);
    }
    return r;
}

static MARResult MARZero(MARFromFn from, MARPivotFn pivot) {
    MARResult r;
    for (float angle : {0.0f,0.7f}) {
        VxMatrix m;
        if (from) from(m,VxVector(0,0,0),angle); else pivot(m,VxVector(0,0,0),VxVector(1,2,3),angle);
        ++r.calls; bool ok = MARAffine(m);
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) ok = ok && std::isnan(m[i][j]);
        r.errors += !ok;
    }
    return r;
}

static MARResult MAROverflow(MARPivotFn fn) {
    MARResult r;
    const float signs[] = {-1,1};
    for (float sign : signs) for (float direction : signs) {
        VxMatrix m; m.SetIdentity();
        for (int i = 0; i < 3; ++i) m[3][i] = sign*FLT_MAX;
        fn(m,VxVector(1,0,0),static_cast<const VxVector &>(m[3]),direction*2.4f); ++r.calls;
        // Y overflows first. Z must observe that stored infinity, with the
        // rotation sign deciding its sign; finite-tail compensation must not
        // turn the subsequent well-defined infinite result into NaN.
        r.errors += !MARAffine(m) || m[3][0] != 0 || !std::isinf(m[3][1]) ||
            !std::isinf(m[3][2]) || (m[3][1] < 0) != (sign < 0) || (m[3][2] < 0) != (sign*direction < 0);
    }
    return r;
}

#if defined(_WIN32)
static bool MARGuardCall(MARFromFn from, MARPivotFn pivot, VxMatrix *m, const VxVector *axis, const VxVector *origin) {
    __try { if (from) from(*m,*axis,0.7f); else pivot(*m,*axis,*origin,0.7f); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#endif

static MARResult MARFootprint(MARFromFn from, MARPivotFn pivot) {
    MARResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *blocks[3] = {};
    for (int i = 0; i < 3; ++i) {
        blocks[i] = static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!blocks[i]) { ++r.errors; for (int k = 0; k < i; ++k) VirtualFree(blocks[k],0,MEM_RELEASE); return r; }
        DWORD old; VirtualProtect(blocks[i],page,PAGE_NOACCESS,&old); VirtualProtect(blocks[i]+2*page,page,PAGE_NOACCESS,&old);
    }
    for (int edge = 0; edge < 2; ++edge) {
        VxMatrix *m = reinterpret_cast<VxMatrix *>(blocks[0]+(edge ? 2*page-64 : page));
        VxVector *axis = reinterpret_cast<VxVector *>(blocks[1]+(edge ? 2*page-12 : page));
        VxVector *origin = reinterpret_cast<VxVector *>(blocks[2]+(edge ? 2*page-12 : page));
        const float a[] = {1,2,3},p[] = {7,-4,-2}; std::memcpy(axis,a,12); std::memcpy(origin,p,12);
        DWORD old; for (int i = 1; i < 3; ++i) VirtualProtect(blocks[i]+page,page,PAGE_READONLY,&old);
        ++r.calls; const bool ok = MARGuardCall(from,pivot,m,axis,origin); r.faults += !ok;
        r.errors += !ok || !MARAffine(*m) || !MARLinear(*m,*axis,0.7f) || std::memcmp(axis,a,12) || std::memcmp(origin,p,12);
        for (int i = 1; i < 3; ++i) VirtualProtect(blocks[i]+page,page,PAGE_READWRITE,&old);
    }
    for (int i = 0; i < 3; ++i) VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}

#ifndef VX_AXIS_PROBE
TEST(MatrixAxisRotationReference, RotateBasisInOriginalDirection) { EXPECT_EQ(MARAxes(Vx3DMatrixFromRotation,0).errors,0); }
TEST(MatrixAxisRotationReference, NormalizeFiniteAxisAcrossScales) { EXPECT_EQ(MARAxes(Vx3DMatrixFromRotation,1).errors,0); }
TEST(MatrixAxisRotationReference, PreserveMultipleTurnRotations) { EXPECT_EQ(MARAxes(Vx3DMatrixFromRotation,2).errors,0); }
TEST(MatrixAxisRotationReference, RetainTinyNonzeroRotation) { EXPECT_EQ(MARTiny(Vx3DMatrixFromRotation).errors,0); }
TEST(MatrixAxisRotationReference, FixPivotUsingStoredLinearTransform) { EXPECT_EQ(MARPivots(Vx3DMatrixFromRotationAndOrigin,false).errors,0); }
TEST(MatrixAxisRotationReference, RetainTranslationRangeAndCancellation) { EXPECT_EQ(MARPivots(Vx3DMatrixFromRotationAndOrigin,true).errors,0); }
TEST(MatrixAxisRotationReference, PreserveAxisSnapshotAndSequentialOriginAliases) { EXPECT_EQ(MARAliases(Vx3DMatrixFromRotationAndOrigin).errors,0); }
TEST(MatrixAxisRotationReference, PreserveUndefinedZeroAxisPropagation) {
    EXPECT_EQ(MARZero(Vx3DMatrixFromRotation,nullptr).errors,0); EXPECT_EQ(MARZero(nullptr,Vx3DMatrixFromRotationAndOrigin).errors,0);
}
TEST(MatrixAxisRotationReference, PreserveSequentialOverflowPropagation) { EXPECT_EQ(MAROverflow(Vx3DMatrixFromRotationAndOrigin).errors,0); }
TEST(MatrixAxisRotationReference, RespectVector12Matrix64AndReadOnlyInputs) {
    const MARResult a = MARFootprint(Vx3DMatrixFromRotation,nullptr),b = MARFootprint(nullptr,Vx3DMatrixFromRotationAndOrigin);
    EXPECT_EQ(a.errors+b.errors+a.faults+b.faults,0);
}
#endif
