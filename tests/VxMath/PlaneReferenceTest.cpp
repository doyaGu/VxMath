#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>
#include "VxPlane.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef VX_PLANE_NATIVE_PROBE
#include <gtest/gtest.h>
#endif

struct PlaneSamples { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
typedef float (*PlaneBoxClassifier)(const VxPlane &, const VxBbox &, const VxMatrix *);

static void ObservePlaneValue(PlaneSamples &result, float value, double expected) {
    if (!std::isfinite(value) || std::fabs(double(value) - expected) > 2e-6 * (1 + std::fabs(expected))) ++result.errors;
    unsigned bits = 0;
    if (value != 0) std::memcpy(&bits, &value, 4);
    for (int byte = 0; byte < 4; ++byte) { result.digest = (result.digest ^ (bits & 255u)) * 16777619u; bits >>= 8; }
}

static float PlaneBoxMember(const VxPlane &plane, const VxBbox &box, const VxMatrix *matrix) {
    return matrix ? plane.Classify(box, *matrix) : plane.Classify(box);
}

static void PlaneTestMatrix(VxMatrix &matrix, int kind) {
    matrix.SetIdentity();
    if (kind == 1) { matrix[0] = VxVector(0,1,0); matrix[1] = VxVector(-1,0,0); matrix[3] = VxVector(2,-4,1); }
    if (kind == 2) { matrix[0] = VxVector(-2,0,0); matrix[1] = VxVector(0,3,0); matrix[2] = VxVector(0,0,0.5f); }
    if (kind == 3) { matrix[0] = VxVector(1,0.5f,0); matrix[1] = VxVector(0,1,-1); matrix[2] = VxVector(0.25f,0,1); matrix[3] = VxVector(1,2,-3); }
    if (kind == 4) { matrix[0] = VxVector(0,0,0); matrix[1] = VxVector(0,2,0); matrix[2] = VxVector(0,0,-1); matrix[3] = VxVector(2,-1,3); }
    if (kind == 5) matrix[3] = VxVector(-6,4,2);
    if (kind == 6) { matrix[0] = VxVector(0,0,-2); matrix[1] = VxVector(0.5f,0,0); matrix[2] = VxVector(0,3,0); matrix[3] = VxVector(-2,1,4); }
}

// The oracle enumerates all eight transformed vertices. It does not use the
// implementation's support radius, XClassify or matrix/vector functions.
static void PlaneCornerBounds(const VxPlane &plane, const VxBbox &box, const VxMatrix *matrix, double &low, double &high) {
    for (int corner = 0; corner < 8; ++corner) {
        const double local[] = {(corner & 1) ? box.Max.x : box.Min.x,
                                (corner & 2) ? box.Max.y : box.Min.y,
                                (corner & 4) ? box.Max.z : box.Min.z};
        double world[3];
        for (int axis = 0; axis < 3; ++axis) {
            world[axis] = matrix ? (*matrix)[3][axis] : local[axis];
            if (matrix) for (int row = 0; row < 3; ++row) world[axis] += local[row] * (*matrix)[row][axis];
        }
        const double value = plane.m_Normal.x * world[0] + plane.m_Normal.y * world[1] + plane.m_Normal.z * world[2] + plane.m_D;
        if (!corner || value < low) low = value;
        if (!corner || value > high) high = value;
    }
}

static float PlaneAxesMember(const VxPlane &plane, const VxBbox &box, const VxMatrix *matrix) {
    const VxVector half = box.GetHalfSize(), center = box.GetCenter();
    VxVector axes[4] = {(*matrix)[0] * half.x, (*matrix)[1] * half.y, (*matrix)[2] * half.z, VxVector()};
    Vx3DMultiplyMatrixVector(axes + 3, *matrix, &center);
    return plane.XClassify(axes);
}

static PlaneSamples PlaneBoxDataset(bool transformed, PlaneBoxClassifier classify = PlaneBoxMember, int resultKind = 0) {
    PlaneSamples result;
    const VxVector extents[] = {VxVector(0,0,0), VxVector(1,2,3), VxVector(2,0,1), VxVector(0.5f,1.5f,2.5f)};
    for (int nx = -1; nx <= 1; ++nx) for (int ny = -1; ny <= 1; ++ny) for (int nz = -1; nz <= 1; ++nz) {
        if (!(nx || ny || nz)) continue;
        for (int d = -6; d <= 6; d += 3) for (int x = -4; x <= 4; x += 4) for (int y = -4; y <= 4; y += 4) for (int z = -4; z <= 4; z += 4)
        for (const VxVector &extent : extents) for (int kind = 0; kind < (transformed ? 7 : 1); ++kind) {
            VxPlane plane{float(nx),float(ny),float(nz),float(d)};
            const VxVector center{float(x),float(y),float(z)};
            VxBbox box(center - extent, center + extent);
            VxMatrix matrix; PlaneTestMatrix(matrix,kind);
            const VxPlane beforePlane = plane; const VxBbox beforeBox = box; const VxMatrix beforeMatrix = matrix;
            double low = 0, high = 0; PlaneCornerBounds(plane,box,transformed ? &matrix : nullptr,low,high);
            double expected = low > 0 ? low : high < 0 ? high : 0;
            if (resultKind == 1) expected = (low > 0 || high < 0) ? (low + high) * 0.5 : 0;
            if (resultKind == 2) expected = low <= 0 && high >= 0;
            ObservePlaneValue(result,classify(plane,box,transformed ? &matrix : nullptr),expected);
            if (std::memcmp(&plane,&beforePlane,sizeof(plane)) || std::memcmp(&box,&beforeBox,sizeof(box)) || std::memcmp(&matrix,&beforeMatrix,sizeof(matrix))) ++result.errors;
            ++result.samples;
        }
    }
    return result;
}

static PlaneSamples PlanePointDataset() {
    PlaneSamples result;
    for (int nx = -2; nx <= 2; ++nx) for (int ny = -2; ny <= 2; ++ny) for (int nz = -2; nz <= 2; ++nz)
    for (int d = -4; d <= 4; d += 2) for (int x = -4; x <= 4; x += 2) for (int y = -4; y <= 4; y += 2) for (int z = -4; z <= 4; z += 2) {
        VxPlane plane{float(nx),float(ny),float(nz),float(d)}; const VxVector point{float(x),float(y),float(z)};
        const int equation = nx*x + ny*y + nz*z + d;
        ObservePlaneValue(result,plane.Classify(point),equation);
        ObservePlaneValue(result,plane.Distance(point),std::abs(equation));
        const VxVector out = plane.NearestPoint(point);
        // Original unit-normal fast projection keeps its raw coefficient
        // scaling outside the metric domain; it does not divide by |n|^2.
        ObservePlaneValue(result,out.x,x - nx*equation); ObservePlaneValue(result,out.y,y - ny*equation); ObservePlaneValue(result,out.z,z - nz*equation);
        ++result.samples;
    }
    return result;
}

static PlaneSamples PlaneFaceDataset() {
    PlaneSamples result; const VxPlane plane(0,0,1,0);
    for (int a = -3; a <= 3; ++a) for (int b = -3; b <= 3; ++b) for (int c = -3; c <= 3; ++c) {
        int low = a, high = a; if (b < low) low = b; if (c < low) low = c; if (b > high) high = b; if (c > high) high = c;
        const int expected = low > 0 ? low : high < 0 ? high : 0;
        ObservePlaneValue(result,plane.ClassifyFace(VxVector(1,0,float(a)),VxVector(0,1,float(b)),VxVector(-1,-1,float(c))),expected);
        ++result.samples;
    }
    return result;
}

#ifndef VX_PLANE_NATIVE_PROBE
static void RecordPlaneSamples(const PlaneSamples &result) {
    EXPECT_EQ(result.errors,0); char digest[16]; std::snprintf(digest,sizeof(digest),"%u",result.digest);
    testing::Test::RecordProperty("samples",result.samples); testing::Test::RecordProperty("digest",digest);
#if defined(VX_SIMD_SSE2)
    testing::Test::RecordProperty("compiled_sse2",1);
#else
    testing::Test::RecordProperty("compiled_sse2",0);
#endif
}

TEST(PlaneReferenceTest, LayoutFieldsAndImplicitCopyHaveOriginalFourFloatRepresentation) {
    EXPECT_EQ(sizeof(VxPlane),16u); EXPECT_EQ(alignof(VxPlane),4u);
    EXPECT_EQ(offsetof(VxPlane,m_Normal),0u); EXPECT_EQ(offsetof(VxPlane,m_D),12u);
    EXPECT_TRUE(std::is_standard_layout<VxPlane>::value); EXPECT_TRUE(std::is_trivially_copyable<VxPlane>::value); EXPECT_TRUE(std::is_trivially_destructible<VxPlane>::value);
    const float fields[] = {2,-3,4,-5}; VxPlane plane(2,-3,4,-5); EXPECT_EQ(std::memcmp(&plane,fields,sizeof(fields)),0);
    VxPlane copy(plane), assigned; assigned = plane; copy.m_Normal.y = 100; assigned.m_D = 101;
    EXPECT_FLOAT_EQ(plane.m_Normal.y,-3); EXPECT_FLOAT_EQ(plane.m_D,-5);
}
TEST(PlaneReferenceTest, DefaultConstructorInitializesAllFourCoefficientsToZero) {
    VxPlane plane; EXPECT_EQ(plane.m_Normal,VxVector(0,0,0)); EXPECT_FLOAT_EQ(plane.m_D,0);
}
TEST(PlaneReferenceTest, EquationConstructorsPreserveRawValuesAndSignedZeros) {
    const VxVector normal(-0.0f,2,-3); VxPlane a(normal,-0.0f), b(-0.0f,2,-3,-0.0f);
    EXPECT_EQ(std::memcmp(&a,&b,sizeof(a)),0); EXPECT_EQ(std::memcmp(&a.m_Normal,&normal,12),0); EXPECT_TRUE(std::signbit(a.m_D));
}
TEST(PlaneReferenceTest, NormalAccessorReferencesTheStoredVectorWithoutCopying) {
    VxPlane plane(2,-3,4,-5); const VxPlane &view = plane; const VxVector &normal = view.GetNormal();
    EXPECT_EQ(&normal,&plane.m_Normal); plane.m_Normal.y = 9; EXPECT_FLOAT_EQ(normal.y,9);
    EXPECT_TRUE((std::is_same<decltype(view.GetNormal()),const VxVector &>::value));
}
TEST(PlaneReferenceTest, EqualityComparesEachCoefficientNumerically) {
    const VxPlane base(2,-3,4,-5); EXPECT_TRUE(base == VxPlane(base));
    for (int axis = 0; axis < 4; ++axis) { VxPlane other = base; if (axis < 3) other.m_Normal[axis] += 1; else other.m_D += 1; EXPECT_FALSE(base == other); }
    EXPECT_TRUE(VxPlane(-0.0f,0,0,-0.0f) == VxPlane(0,0,0,0));
    const float nan = std::nanf(""); VxPlane invalid(nan,0,0,0); EXPECT_FALSE(invalid == invalid); invalid = VxPlane(0,0,0,nan); EXPECT_FALSE(invalid == invalid);
}
TEST(PlaneReferenceTest, NegationReversesTheEquationAndPreservesItsZeroSet) {
    const VxPlane plane(2,-3,4,-5), flipped = -plane;
    EXPECT_EQ(flipped.m_Normal,VxVector(-2,3,-4)); EXPECT_FLOAT_EQ(flipped.m_D,5);
    EXPECT_FLOAT_EQ(flipped.Classify(VxVector(4,1,2)),-plane.Classify(VxVector(4,1,2)));
    const VxPlane zeros(0.0f,-0.0f,0.0f,-0.0f), neg = -zeros;
    EXPECT_TRUE(std::signbit(neg.m_Normal.x)); EXPECT_FALSE(std::signbit(neg.m_Normal.y)); EXPECT_TRUE(std::signbit(neg.m_Normal.z)); EXPECT_FALSE(std::signbit(neg.m_D));
}
TEST(PlaneReferenceTest, PointGridPreservesOriginalRawEquationAndUnitNormalFastPathScaling) { RecordPlaneSamples(PlanePointDataset()); }
TEST(PlaneReferenceTest, UnitNormalDistanceAndProjectionHaveTheirGeometricMeaning) {
    const VxPlane axis(0,0,-1,3); const VxVector point(2,-4,8);
    EXPECT_FLOAT_EQ(axis.Distance(point),5); EXPECT_EQ(axis.NearestPoint(point),VxVector(2,-4,3));
    const VxPlane diagonal(0.6f,0.8f,0,0); const VxVector projected = diagonal.NearestPoint(VxVector(3,4,7));
    EXPECT_NEAR(diagonal.Distance(VxVector(3,4,7)),5,1e-6f);
    EXPECT_NEAR(projected.x,0,1e-6f); EXPECT_NEAR(projected.y,0,1e-6f); EXPECT_FLOAT_EQ(projected.z,7);
}
TEST(PlaneReferenceTest, NonUnitPlaneRetainsOriginalRawCoefficientContract) {
    const VxPlane plane(0,0,2,-10); const VxVector point(3,4,8);
    EXPECT_FLOAT_EQ(plane.Classify(point),6); EXPECT_FLOAT_EQ(plane.Distance(point),6); EXPECT_EQ(plane.NearestPoint(point),VxVector(3,4,-4));
}
TEST(PlaneReferenceTest, AabbClassificationMatchesEightCornerSupportIncludingTangency) { RecordPlaneSamples(PlaneBoxDataset(false)); }
TEST(PlaneReferenceTest, TransformedBoxReturnsNearestBoundaryAcrossSevenAffineMaps) { RecordPlaneSamples(PlaneBoxDataset(true)); }
TEST(PlaneReferenceTest, BoxBoundaryClassificationAndAxisCenterClassificationRemainDistinct) {
    const VxPlane plane(0,1,0,-1); const VxBbox box(VxVector(-1,-2,-3),VxVector(1,2,3)); VxMatrix matrix; matrix.SetIdentity(); matrix[3] = VxVector(0,6,0);
    VxVector axes[] = {VxVector(1,0,0),VxVector(0,2,0),VxVector(0,0,3),VxVector(0,6,0)};
    EXPECT_FLOAT_EQ(plane.Classify(box,matrix),3); EXPECT_FLOAT_EQ(plane.XClassify(axes),5);
    matrix[3] = VxVector(0,-6,0); axes[3] = matrix[3]; EXPECT_FLOAT_EQ(plane.Classify(box,matrix),-5); EXPECT_FLOAT_EQ(plane.XClassify(axes),-7);
}
TEST(PlaneReferenceTest, AxisBoxClassificationReturnsCenterOnlyWhenStrictlySeparated) { RecordPlaneSamples(PlaneBoxDataset(true,PlaneAxesMember,1)); }
TEST(PlaneReferenceTest, TangentBoxesAndZeroExtentPointsUseClosedIntersectionBoundaries) {
    const VxPlane plane(0,1,0,-1); VxMatrix matrix; matrix.SetIdentity();
    const VxBbox tangent(VxVector(-2,1,-2),VxVector(2,3,2)), point(VxVector(0,1,0),VxVector(0,1,0));
    EXPECT_FLOAT_EQ(plane.Classify(tangent),0); EXPECT_FLOAT_EQ(plane.Classify(tangent,matrix),0); EXPECT_FLOAT_EQ(plane.Classify(point,matrix),0);
    const VxVector axes[] = {VxVector(1,0,0),VxVector(0,2,0),VxVector(0,0,1),VxVector(0,3,0)}; EXPECT_FLOAT_EQ(plane.XClassify(axes),0);
}
TEST(PlaneReferenceTest, FaceClassificationUsesClosedTriangleSupportForEveryVertexOrder) { RecordPlaneSamples(PlaneFaceDataset()); }
TEST(PlaneReferenceTest, ZeroValuedMiddleFaceVertexCannotTurnATouchingFaceIntoASeparatedFace) {
    const VxPlane plane(0,0,1,0); const VxVector vertices[] = {VxVector(1,0,1),VxVector(0,1,0),VxVector(-1,-1,-2)};
    const int permutations[][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for (const auto &order : permutations) EXPECT_FLOAT_EQ(plane.ClassifyFace(vertices[order[0]],vertices[order[1]],vertices[order[2]]),0);
}
#if defined(VX_SIMD_SSE)
static float PlaneDirectAabb(const VxPlane &plane, const VxBbox &box, const VxMatrix *) { return VxSIMDPlaneClassifyAABB(&plane,&box); }
static float PlaneDirectAxes(const VxPlane &plane, const VxBbox &box, const VxMatrix *matrix) {
    const VxVector half = box.GetHalfSize(), center = box.GetCenter(); VxVector axes[4] = {(*matrix)[0]*half.x,(*matrix)[1]*half.y,(*matrix)[2]*half.z,VxVector()};
    Vx3DMultiplyMatrixVector(axes+3,*matrix,&center);
    const float array = VxSIMDPlaneXClassify(&plane,axes);
    const float registers = VxSIMDPlaneXClassify(&plane,VxSIMDLoadFloat3(&axes[0].x),VxSIMDLoadFloat3(&axes[1].x),VxSIMDLoadFloat3(&axes[2].x),VxSIMDLoadFloat3(&axes[3].x));
    EXPECT_FLOAT_EQ(array,registers); return registers;
}
TEST(PlaneReferenceTest, DirectSimdPointDistanceAndProjectionRetainTheEquationContract) {
    const VxPlane plane(0,0,2,-10); const VxVector point(3,4,8); VxVector projected;
    EXPECT_FLOAT_EQ(VxSIMDPlaneClassifyPoint(&plane,&point),6); EXPECT_FLOAT_EQ(VxSIMDPlaneDistance(&plane,&point),6);
    VxSIMDPlaneNearestPoint(&projected,&plane,&point); EXPECT_EQ(projected,VxVector(3,4,-4));
}
TEST(PlaneReferenceTest, DirectSimdAabbMatchesIndependentCornerBounds) { RecordPlaneSamples(PlaneBoxDataset(false,PlaneDirectAabb)); }
TEST(PlaneReferenceTest, BothDirectSimdAxisOverloadsMatchIndependentCornerBounds) { RecordPlaneSamples(PlaneBoxDataset(true,PlaneDirectAxes,1)); }
TEST(PlaneReferenceTest, DirectSimdProjectionReadsInputsBeforeAliasedOutputWrites) {
    VxPlane plane(0,0,1,-3); VxVector point(2,-4,8);
    VxSIMDPlaneNearestPoint(&point,&plane,&point); EXPECT_EQ(point,VxVector(2,-4,3));
    point = VxVector(2,-4,8); VxSIMDPlaneNearestPoint(&plane.m_Normal,&plane,&point); EXPECT_EQ(plane.m_Normal,VxVector(2,-4,3)); EXPECT_FLOAT_EQ(plane.m_D,-3);
}
#endif
#if defined(_WIN32)
TEST(PlaneReferenceTest, PlaneAndVectorOperationsRespectExactObjectFootprints) {
    SYSTEM_INFO system; GetSystemInfo(&system); const SIZE_T page = system.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,10*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)); ASSERT_NE(memory,nullptr);
    DWORD old = 0; for (int guard = 1; guard < 10; guard += 2) ASSERT_TRUE(VirtualProtect(memory + guard*page,page,PAGE_NOACCESS,&old));
    VxPlane *plane = new(memory + page - sizeof(VxPlane)) VxPlane(0,1,0,-1);
    VxVector *point = new(memory + 3*page - sizeof(VxVector)) VxVector(2,5,3);
    VxBbox *box = new(memory + 5*page - sizeof(VxBbox)) VxBbox(VxVector(-1,3,-1),VxVector(1,5,1));
    VxVector *axes = reinterpret_cast<VxVector *>(memory + 7*page - 4*sizeof(VxVector));
    new(axes) VxVector(1,0,0); new(axes+1) VxVector(0,1,0); new(axes+2) VxVector(0,0,1); new(axes+3) VxVector(0,4,0);
    VxMatrix *matrix = new(memory + 9*page - sizeof(VxMatrix)) VxMatrix; matrix->SetIdentity();
    EXPECT_FLOAT_EQ(plane->Classify(*point),4); EXPECT_FLOAT_EQ(plane->Distance(*point),4); EXPECT_EQ(plane->NearestPoint(*point),VxVector(2,1,3));
    EXPECT_FLOAT_EQ(plane->Classify(*box),2); EXPECT_FLOAT_EQ(plane->Classify(*box,*matrix),2); EXPECT_FLOAT_EQ(plane->XClassify(axes),3);
    EXPECT_FLOAT_EQ(plane->ClassifyFace(*point,*point,*point),4); EXPECT_EQ(&plane->GetNormal(),&plane->m_Normal); EXPECT_TRUE(*plane == VxPlane(0,1,0,-1)); EXPECT_FLOAT_EQ((-*plane).m_D,1);
    VirtualFree(memory,0,MEM_RELEASE);
}
#endif
#endif
