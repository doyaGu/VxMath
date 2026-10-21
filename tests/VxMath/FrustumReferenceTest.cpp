#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>
#ifndef VX_FRUSTUM_NATIVE_PROBE
#include "VxFrustum.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct FrustumProbe : VxFrustum {
    using VxFrustum::m_Origin; using VxFrustum::m_Right; using VxFrustum::m_Up; using VxFrustum::m_Dir;
    using VxFrustum::m_RBound; using VxFrustum::m_UBound; using VxFrustum::m_DMin; using VxFrustum::m_DMax;
    using VxFrustum::m_DRatio; using VxFrustum::m_RF; using VxFrustum::m_UF;
    using VxFrustum::m_LeftPlane; using VxFrustum::m_RightPlane; using VxFrustum::m_UpPlane;
    using VxFrustum::m_BottomPlane; using VxFrustum::m_NearPlane; using VxFrustum::m_FarPlane;
    float SinglePlane(const VxVector a[4], const VxPlane &p) const { return XClassify(a, p); }
};
struct FrustumSamples { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void FrustumObserve(FrustumSamples &s, float value, double expected) {
    if (!std::isfinite(value) || std::fabs(value - expected) > 3e-5 * (1 + std::fabs(expected))) ++s.errors;
    // Quantized observations permit VC6 x87 and current float arithmetic to differ by a few ulps.
    const int code = std::isfinite(value) && std::fabs(value) < 1e6 ? int(std::round(value * 1024.0)) : 0x7fffffff;
    unsigned bits = unsigned(code);
    for (int i = 0; i < 4; ++i) { s.digest = (s.digest ^ (bits & 255u)) * 16777619u; bits >>= 8; }
}
static void FrustumFrame(VxVector axes[3], int frame) {
    axes[0] = VxVector(1, 0, 0); axes[1] = VxVector(0, 1, 0); axes[2] = VxVector(0, 0, 1);
    if (frame == 1) { axes[0] = VxVector(0, 1, 0); axes[1] = VxVector(-1, 0, 0); }
    if (frame == 2) { axes[0] = VxVector(0, 0, 1); axes[1] = VxVector(1, 0, 0); axes[2] = VxVector(0, 1, 0); }
}
static VxFrustum FrustumMake(int frame = 0, float shift = 0, float scale = 1) {
    VxVector axes[3]; FrustumFrame(axes, frame); VxFrustum f;
    f.GetOrigin() = VxVector(shift, -shift * 2, shift * 3);
    f.GetRight() = axes[0]; f.GetUp() = axes[1]; f.GetDir() = axes[2];
    f.GetRBound() = 2 * scale; f.GetUBound() = scale; f.GetDMin() = 2 * scale; f.GetDMax() = 10 * scale;
    f.Update(); return f;
}
static VxVector FrustumWorld(const VxFrustum &f, double x, double y, double z) {
    VxVector out;
    for (int c = 0; c < 3; ++c) out[c] = float(f.GetOrigin()[c] + x * f.GetRight()[c] + y * f.GetUp()[c] + z * f.GetDir()[c]);
    return out;
}
static void FrustumLocal(const VxFrustum &f, const VxVector &p, double out[3]) {
    const VxVector axes[] = {f.GetRight(), f.GetUp(), f.GetDir()};
    for (int row = 0; row < 3; ++row) { out[row] = 0; for (int c = 0; c < 3; ++c) out[row] += (double(p[c]) - f.GetOrigin()[c]) * axes[row][c]; }
}
static double FrustumDistance(int plane, const double p[3]) {
    switch (plane) {
    case 0: return 2 - p[2]; case 1: return p[2] - 10;
    case 2: return (-p[0] - p[2]) / std::sqrt(2.0);
    case 3: return (p[0] - p[2]) / std::sqrt(2.0);
    case 4: return (2 * p[1] - p[2]) / std::sqrt(5.0);
    default: return (-2 * p[1] - p[2]) / std::sqrt(5.0);
    }
}
static FrustumSamples FrustumGeometryDataset() {
    FrustumSamples s;
    for (int frame = 0; frame < 3; ++frame) for (int shift = -2; shift <= 2; shift += 2) {
        const VxFrustum f = FrustumMake(frame, float(shift)); VxVector corners[8]; f.ComputeVertices(corners);
        const int signs[4][2] = {{-1,-1},{-1,1},{1,1},{1,-1}};
        for (int i = 0; i < 8; ++i) {
            const double z = i < 4 ? 2 : 10;
            const VxVector expected = FrustumWorld(f, signs[i % 4][0] * z, signs[i % 4][1] * z * 0.5, z);
            for (int c = 0; c < 3; ++c) FrustumObserve(s, corners[i][c], expected[c]);
        }
        const VxPlane *planes[] = {&f.GetNearPlane(), &f.GetFarPlane(), &f.GetLeftPlane(), &f.GetRightPlane(), &f.GetUpPlane(), &f.GetBottomPlane()};
        const double n[6][3] = {{0,0,-1},{0,0,1},{-1,0,-1},{1,0,-1},{0,2,-1},{0,-2,-1}};
        for (int p = 0; p < 6; ++p) {
            const double length = p < 2 ? 1 : p < 4 ? std::sqrt(2.0) : std::sqrt(5.0);
            double d = p == 0 ? 2 : p == 1 ? -10 : 0;
            for (int c = 0; c < 3; ++c) {
                const double expected = (n[p][0]*f.GetRight()[c] + n[p][1]*f.GetUp()[c] + n[p][2]*f.GetDir()[c]) / length;
                FrustumObserve(s, planes[p]->m_Normal[c], expected); d -= expected * f.GetOrigin()[c];
            }
            FrustumObserve(s, planes[p]->m_D, d);
        }
        ++s.samples;
    }
    return s;
}
static FrustumSamples FrustumPointDataset() {
    FrustumSamples s;
    for (int frame = 0; frame < 3; ++frame) for (int shift = -2; shift <= 2; shift += 2) {
        const VxFrustum f = FrustumMake(frame, float(shift));
        for (int x = -12; x <= 12; x += 6) for (int y = -6; y <= 6; y += 3) for (int z = 1; z <= 11; z += 2) {
            unsigned flags = 0;
            if (z < 2) flags |= VXCLIP_FRONT; else if (z > 10) flags |= VXCLIP_BACK;
            if (x < -z) flags |= VXCLIP_LEFT; else if (x > z) flags |= VXCLIP_RIGHT;
            if (2*y < -z) flags |= VXCLIP_BOTTOM; else if (2*y > z) flags |= VXCLIP_TOP;
            const VxVector point = FrustumWorld(f, x, y, z);
            FrustumObserve(s, float(f.Classify(point)), flags); FrustumObserve(s, float(f.IsInside(point)), flags == 0);
            ++s.samples;
        }
    }
    return s;
}
static FrustumSamples FrustumBoxDataset(bool transformed) {
    FrustumSamples s;
    for (int frame = 0; frame < 3; ++frame) for (int shift = -2; shift <= 2; shift += 2)
    for (int x = -12; x <= 12; x += 4) for (int y = -6; y <= 6; y += 3) for (int z = -2; z <= 14; z += 4) {
        const VxFrustum f = FrustumMake(frame, float(shift));
        const VxVector center = FrustumWorld(f, x, y, z);
        const VxBbox box = transformed ? VxBbox(VxVector(-0.5f,-1,-1.5f), VxVector(0.5f,1,1.5f)) : VxBbox(center - VxVector(0.375f,0.875f,1.25f), center + VxVector(0.375f,0.875f,1.25f));
        VxMatrix matrix; matrix.SetIdentity(); matrix[0] = VxVector(0,2,0); matrix[1] = VxVector(-1,0,0); matrix[2] = VxVector(0.25f,0,1); matrix[3] = center;
        double points[8][3];
        for (int v = 0; v < 8; ++v) {
            VxVector p((v&1)?box.Max.x:box.Min.x, (v&2)?box.Max.y:box.Min.y, (v&4)?box.Max.z:box.Min.z);
            if (transformed) {
                VxVector q; for (int c = 0; c < 3; ++c) q[c] = float(matrix[3][c] + double(p.x)*matrix[0][c] + double(p.y)*matrix[1][c] + double(p.z)*matrix[2][c]); p = q;
            }
            FrustumLocal(f, p, points[v]);
        }
        double product = 1, expected = 0;
        for (int p = 0; p < 6; ++p) {
            double low = 1e20, high = -1e20;
            for (int v = 0; v < 8; ++v) { const double d = FrustumDistance(p, points[v]); if (d < low) low = d; if (d > high) high = d; }
            double value = transformed ? ((low > 0 || high < 0) ? (low+high)*0.5 : 0) : low > 0 ? low : high < 0 ? high : 0;
            if (value > 0) { expected = value; break; } product *= value; expected = -product;
        }
        FrustumObserve(s, transformed ? f.Classify(box, matrix) : f.Classify(box), expected); ++s.samples;
    }
    return s;
}
static FrustumSamples FrustumTransformDataset() {
    FrustumSamples s;
    for (int frame = 0; frame < 3; ++frame) for (int shift = -2; shift <= 2; shift += 2) for (int map = 0; map < 3; ++map) {
        VxFrustum f = FrustumMake(frame, float(shift)); VxVector before[8], after[8]; f.ComputeVertices(before);
        VxMatrix m; m.SetIdentity();
        if (map == 1) { m[0] = VxVector(0,2,0); m[1] = VxVector(-0.5f,0,0); m[2] = VxVector(0,0,4); }
        if (map == 2) { m[0] = VxVector(0,0,0.5f); m[1] = VxVector(2,0,0); m[2] = VxVector(0,4,0); }
        m[3] = VxVector(4,-2,8); f.Transform(m); f.ComputeVertices(after);
        for (int v = 0; v < 8; ++v) for (int c = 0; c < 3; ++c) {
            const double expected = m[3][c] + double(before[v].x)*m[0][c] + double(before[v].y)*m[1][c] + double(before[v].z)*m[2][c];
            FrustumObserve(s, after[v][c], expected);
        }
        ++s.samples;
    }
    return s;
}
static FrustumSamples FrustumScaleDataset(bool transform) {
    FrustumSamples s;
    const int powers[] = {-90,-60,-30,-10,0,10,30,60,90};
    for (int exponent : powers) {
        const float scale = std::ldexp(1.0f, exponent); VxFrustum f = FrustumMake(0, 0, transform ? 1 : scale);
        if (transform) { VxMatrix m; m.SetIdentity(); for (int i = 0; i < 3; ++i) m[i] *= scale; f.Transform(m); }
        const VxPlane *planes[] = {&f.GetLeftPlane(), &f.GetRightPlane(), &f.GetUpPlane(), &f.GetBottomPlane()};
        const double normal[4][3] = {{-1,0,-1},{1,0,-1},{0,2,-1},{0,-2,-1}};
        for (int p = 0; p < 4; ++p) for (int c = 0; c < 3; ++c) FrustumObserve(s, planes[p]->m_Normal[c], normal[p][c]/std::sqrt(p < 2 ? 2.0 : 5.0));
        FrustumObserve(s, float(f.Classify(VxVector(20*scale,0,5*scale))), VXCLIP_RIGHT);
        if (transform) {
            const VxVector *axes[] = {&f.GetRight(), &f.GetUp(), &f.GetDir()};
            for (int i = 0; i < 3; ++i) for (int c = 0; c < 3; ++c) FrustumObserve(s, (*axes[i])[c], i == c ? 1 : 0);
            const float values[] = {f.GetRBound(),f.GetUBound(),f.GetDMin(),f.GetDMax()};
            const double expected[] = {2,1,2,10};
            for (int i = 0; i < 4; ++i) if (!std::isfinite(values[i]) || std::fabs(double(values[i])/scale - expected[i]) > 3e-5) ++s.errors;
        }
        ++s.samples;
    }
    return s;
}

#ifndef VX_FRUSTUM_NATIVE_PROBE
static void FrustumCheck(FrustumSamples s) {
    EXPECT_EQ(s.errors, 0); ::testing::Test::RecordProperty("samples", s.samples); ::testing::Test::RecordProperty("digest", std::to_string(s.digest));
#if defined(VX_SIMD_SSE2)
    ::testing::Test::RecordProperty("compiled_sse2", 1);
#else
    ::testing::Test::RecordProperty("compiled_sse2", 0);
#endif
}
TEST(FrustumReference, PlanesAndOrderedCornersMatchIndependentCameraGeometry) { FrustumCheck(FrustumGeometryDataset()); }
TEST(FrustumReference, PointFlagsAndInsideMatchLocalInequalities) { FrustumCheck(FrustumPointDataset()); }
TEST(FrustumReference, AabbClassifyMatchesEightCornerPlaneIntervals) { FrustumCheck(FrustumBoxDataset(false)); }
TEST(FrustumReference, AffineBoxClassifyUsesSignedCenterWhenSeparated) { FrustumCheck(FrustumBoxDataset(true)); }
TEST(FrustumReference, TransformMapsAllEightCornersThroughKnownAffineMaps) { FrustumCheck(FrustumTransformDataset()); }
TEST(FrustumReference, UpdateKeepsUnitSidePlanesAcrossBinaryScales) { FrustumCheck(FrustumScaleDataset(false)); }
TEST(FrustumReference, TransformKeepsNonzeroAxesAndBoundsAcrossBinaryScales) { FrustumCheck(FrustumScaleDataset(true)); }
TEST(FrustumReference, DefaultConstructorAndHorizontalFovContract) {
    const VxFrustum f; EXPECT_EQ(f.GetOrigin(), VxVector(0,0,0)); EXPECT_EQ(f.GetRight(), VxVector(1,0,0));
    EXPECT_EQ(f.GetUp(), VxVector(0,1,0)); EXPECT_EQ(f.GetDir(), VxVector(0,0,1));
    EXPECT_FLOAT_EQ(f.GetRBound(),1); EXPECT_FLOAT_EQ(f.GetUBound(),1); EXPECT_FLOAT_EQ(f.GetDMin(),1); EXPECT_FLOAT_EQ(f.GetDMax(),2);
    EXPECT_FLOAT_EQ(f.GetDRatio(),2); EXPECT_FLOAT_EQ(f.GetRF(),-4); EXPECT_FLOAT_EQ(f.GetUF(),-4);
    const VxFrustum wide(VxVector(4,2,1), f.GetRight(), f.GetUp(), f.GetDir(), 2, 20, 1.5707963267948966f, 0.5f);
    EXPECT_NEAR(wide.GetRBound(),2,1e-5); EXPECT_NEAR(wide.GetUBound(),1,1e-5); EXPECT_FLOAT_EQ(wide.GetDRatio(),10);
    EXPECT_TRUE(wide.IsInside(VxVector(9,4,7))); EXPECT_FALSE(wide.IsInside(VxVector(9,6,7)));
}
TEST(FrustumReference, AccessorsExposeOriginalFieldAddressesAndExplicitCacheLifecycle) {
    FrustumProbe f; const FrustumProbe &v = f;
    EXPECT_EQ(&f.GetOrigin(), &f.m_Origin); EXPECT_EQ(&v.GetOrigin(), &f.m_Origin);
    EXPECT_EQ(&f.GetRight(), &f.m_Right); EXPECT_EQ(&v.GetRight(), &f.m_Right);
    EXPECT_EQ(&f.GetUp(), &f.m_Up); EXPECT_EQ(&v.GetUp(), &f.m_Up); EXPECT_EQ(&f.GetDir(), &f.m_Dir); EXPECT_EQ(&v.GetDir(), &f.m_Dir);
    EXPECT_EQ(&f.GetRBound(), &f.m_RBound); EXPECT_EQ(&v.GetRBound(), &f.m_RBound);
    EXPECT_EQ(&f.GetUBound(), &f.m_UBound); EXPECT_EQ(&v.GetUBound(), &f.m_UBound);
    EXPECT_EQ(&f.GetDMin(), &f.m_DMin); EXPECT_EQ(&v.GetDMin(), &f.m_DMin);
    EXPECT_EQ(&f.GetDMax(), &f.m_DMax); EXPECT_EQ(&v.GetDMax(), &f.m_DMax);
    EXPECT_EQ(&v.GetLeftPlane(), &f.m_LeftPlane); EXPECT_EQ(&v.GetRightPlane(), &f.m_RightPlane);
    EXPECT_EQ(&v.GetUpPlane(), &f.m_UpPlane); EXPECT_EQ(&v.GetBottomPlane(), &f.m_BottomPlane);
    EXPECT_EQ(&v.GetNearPlane(), &f.m_NearPlane); EXPECT_EQ(&v.GetFarPlane(), &f.m_FarPlane);
    f.GetOrigin() = VxVector(2,4,8); f.GetDMin() = 2; f.GetDMax() = 12; f.GetRBound() = 4; f.GetUBound() = 3;
    EXPECT_FLOAT_EQ(f.GetDRatio(),2); EXPECT_FLOAT_EQ(f.GetNearPlane().m_D,1); f.Update();
    EXPECT_FLOAT_EQ(f.GetDRatio(),6); EXPECT_FLOAT_EQ(f.GetRF(),-96); EXPECT_FLOAT_EQ(f.GetUF(),-72); EXPECT_FLOAT_EQ(f.GetNearPlane().m_D,10);
}
TEST(FrustumReference, LayoutAndImplicitCopyHaveNoHiddenPaddingOrVtable) {
    EXPECT_EQ(sizeof(VxFrustum),172u); EXPECT_EQ(alignof(VxFrustum),4u);
    const std::size_t offsets[] = {offsetof(FrustumProbe,m_Origin),offsetof(FrustumProbe,m_Right),offsetof(FrustumProbe,m_Up),offsetof(FrustumProbe,m_Dir),offsetof(FrustumProbe,m_RBound),offsetof(FrustumProbe,m_UBound),offsetof(FrustumProbe,m_DMin),offsetof(FrustumProbe,m_DMax),offsetof(FrustumProbe,m_DRatio),offsetof(FrustumProbe,m_RF),offsetof(FrustumProbe,m_UF),offsetof(FrustumProbe,m_LeftPlane),offsetof(FrustumProbe,m_RightPlane),offsetof(FrustumProbe,m_UpPlane),offsetof(FrustumProbe,m_BottomPlane),offsetof(FrustumProbe,m_NearPlane),offsetof(FrustumProbe,m_FarPlane)};
    const std::size_t expected[] = {0,12,24,36,48,52,56,60,64,68,72,76,92,108,124,140,156};
    for (int i=0;i<17;++i) EXPECT_EQ(offsets[i],expected[i]);
    EXPECT_TRUE(std::is_standard_layout<VxFrustum>::value); EXPECT_TRUE(std::is_trivially_copyable<VxFrustum>::value); EXPECT_TRUE(std::is_trivially_destructible<VxFrustum>::value);
    VxFrustum f = FrustumMake(), copy(f), assigned; assigned = f; EXPECT_TRUE(copy == assigned); copy.GetOrigin().x = 100; EXPECT_EQ(f.GetOrigin().x,0);
}
TEST(FrustumReference, EqualityComparesOnlyAllSixteenBaseFloats) {
    FrustumProbe f, same; float *fields[] = {&same.m_Origin.x,&same.m_Origin.y,&same.m_Origin.z,&same.m_Right.x,&same.m_Right.y,&same.m_Right.z,&same.m_Up.x,&same.m_Up.y,&same.m_Up.z,&same.m_Dir.x,&same.m_Dir.y,&same.m_Dir.z,&same.m_RBound,&same.m_UBound,&same.m_DMin,&same.m_DMax};
    for (float *field : fields) { const float old = *field; *field = old + 1; EXPECT_FALSE(f == same); *field = old; }
    same.m_DRatio = 17; same.m_RF = 19; same.m_UF = 23; same.m_LeftPlane.m_D = 31; EXPECT_TRUE(f == same);
    same.m_Origin.x = -0.0f; EXPECT_TRUE(f == same); same.m_Origin.x = std::nanf(""); EXPECT_FALSE(same == same);
}
TEST(FrustumReference, SinglePlaneClassifyReturnsCenterRatherThanNearestBoundary) {
    const FrustumProbe f; VxVector axes[] = {VxVector(1,0,0),VxVector(0,2,0),VxVector(0,0,3),VxVector(8,0,0)}; const VxPlane p(VxVector(1,0,0),-2);
    EXPECT_FLOAT_EQ(f.SinglePlane(axes,p),6); axes[3].x = 0; EXPECT_FLOAT_EQ(f.SinglePlane(axes,p),-2);
    axes[3].x = 1; EXPECT_FLOAT_EQ(f.SinglePlane(axes,p),0); axes[3].x = 2; EXPECT_FLOAT_EQ(f.SinglePlane(axes,p),0); axes[3].x = 3; EXPECT_FLOAT_EQ(f.SinglePlane(axes,p),0);
}
TEST(FrustumReference, ClosedPointBoundariesAndLaterPlaneRejection) {
    const VxFrustum f = FrustumMake(); const VxVector boundary[] = {VxVector(0,0,2),VxVector(0,0,10),VxVector(-4,0,4),VxVector(4,0,4),VxVector(0,-2,4),VxVector(0,2,4)};
    for (const VxVector &p : boundary) { EXPECT_TRUE(f.IsInside(p)); EXPECT_EQ(f.Classify(p),0u); }
    EXPECT_EQ(f.Classify(VxBbox(VxVector(-0.5f,-0.5f,1),VxVector(0.5f,0.5f,3))),0);
    EXPECT_GT(f.Classify(VxBbox(VxVector(20,-0.5f,1),VxVector(21,0.5f,3))),0);
    EXPECT_LT(f.Classify(VxBbox(VxVector(-0.5f,-0.5f,4),VxVector(0.5f,0.5f,5))),0);
}
#if defined(VX_SIMD_SSE)
TEST(FrustumReference, DirectSimdEntriesUseTheSameOriginalCameraContract) {
    VxFrustum f = FrustumMake(); f.GetDMax() = 12; VxSIMDFrustumUpdate(&f); EXPECT_FLOAT_EQ(f.GetDRatio(),6);
    VxVector vertices[8]; VxSIMDFrustumComputeVertices(&f,vertices); EXPECT_EQ(vertices[7],VxVector(12,-6,12));
    VxMatrix m; m.SetIdentity(); m[3] = VxVector(4,8,16); VxSIMDFrustumTransform(&f,&m); EXPECT_EQ(f.GetOrigin(),VxVector(4,8,16));
    VxSIMDFrustumComputeVertices(&f,vertices); EXPECT_EQ(vertices[7],VxVector(16,2,28));
}
#endif
#if defined(_WIN32)
TEST(FrustumReference, ExactObjectAndEightVertexFootprintsAtGuardPages) {
    SYSTEM_INFO info; GetSystemInfo(&info); const std::size_t page = info.dwPageSize;
    unsigned char *regions[3] = {};
    for (int i = 0; i < 3; ++i) { regions[i] = static_cast<unsigned char *>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE)); ASSERT_NE(regions[i],nullptr); DWORD old; ASSERT_NE(VirtualProtect(regions[i]+page,page,PAGE_NOACCESS,&old),0); }
    VxFrustum *f = new(regions[0]+page-sizeof(VxFrustum)) VxFrustum; *f = FrustumMake();
    VxVector *vertices = reinterpret_cast<VxVector *>(regions[1]+page-sizeof(VxVector)*8);
    VxMatrix *m = new(regions[2]+page-sizeof(VxMatrix)) VxMatrix; m->SetIdentity(); f->Transform(*m); f->ComputeVertices(vertices);
    EXPECT_EQ(vertices[7],VxVector(10,-5,10)); EXPECT_EQ(f->Classify(VxVector(0,0,4)),0u);
    f->~VxFrustum(); m->~VxMatrix(); for (unsigned char *region : regions) EXPECT_NE(VirtualFree(region,0,MEM_RELEASE),0);
}
#endif
#endif
