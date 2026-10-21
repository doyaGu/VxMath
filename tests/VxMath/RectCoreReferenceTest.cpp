#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cfenv>
#include <type_traits>
#ifndef VX_RECT_CORE_NATIVE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#else
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxRect.h"
#endif

struct RectCoreSamples {
    int samples = 0, errors = 0;
    unsigned digest = 2166136261u;
};

static void ObserveRectValue(RectCoreSamples &result, float actual, double wanted) {
    if (!std::isfinite(actual) || std::fabs(double(actual) - wanted) > 0.00001 * (1 + std::fabs(wanted))) ++result.errors;
    unsigned bits; std::memcpy(&bits,&actual,sizeof(bits));
    for (int i = 0; i < 4; ++i) { result.digest = (result.digest ^ (bits & 255u)) * 16777619u; bits >>= 8; }
}

static void ObserveRect(RectCoreSamples &result, const VxRect &r, const double *e) {
    ObserveRectValue(result,r.left,e[0]); ObserveRectValue(result,r.top,e[1]);
    ObserveRectValue(result,r.right,e[2]); ObserveRectValue(result,r.bottom,e[3]);
    const Vx2DVector size = r.GetSize(), half = r.GetHalfSize(), center = r.GetCenter();
    ObserveRectValue(result,r.GetWidth(),e[2]-e[0]); ObserveRectValue(result,r.GetHeight(),e[3]-e[1]);
    ObserveRectValue(result,size.x,e[2]-e[0]); ObserveRectValue(result,size.y,e[3]-e[1]);
    ObserveRectValue(result,half.x,(e[2]-e[0])*0.5); ObserveRectValue(result,half.y,(e[3]-e[1])*0.5);
    ObserveRectValue(result,center.x,(e[0]+e[2])*0.5); ObserveRectValue(result,center.y,(e[1]+e[3])*0.5);
    ObserveRectValue(result,r.GetHCenter(),(e[0]+e[2])*0.5); ObserveRectValue(result,r.GetVCenter(),(e[1]+e[3])*0.5);
    ObserveRectValue(result,r.GetTopLeft().x,e[0]); ObserveRectValue(result,r.GetTopLeft().y,e[1]);
    ObserveRectValue(result,r.GetBottomRight().x,e[2]); ObserveRectValue(result,r.GetBottomRight().y,e[3]);
    ++result.samples;
}

static RectCoreSamples RectGeometryDataset() {
    RectCoreSamples result;
    for (int n = 0; n < 64; ++n) for (int op = 0; op < 32; ++op) {
        const float cx = (n%7-3)*8+0.5f, cy = (n%5-2)*4-0.25f;
        const float hx = float((n%3+1)*2), hy = float(n%4+1);
        const float px = float((n%5-2)*3), py = float((n%7-3)*2);
        const float w = float(n%4+2), h = float(n%3+3);
        VxRect r(cx-hx,cy-hy,cx+hx,cy+hy);
        double e[] = {cx-hx,cy-hy,cx+hx,cy+hy};
        Vx2DVector position(px,py), size(w,h), end(px+w,py+h);
        if (op <= 5) {
            if (op==0) r=VxRect(px,py,px+w,py+h);
            if (op==1) r=VxRect(position,end);
            if (op==2) r.SetCorners(px,py,px+w,py+h);
            if (op==3) r.SetCorners(position,end);
            if (op==4) r.SetDimension(px,py,w,h);
            if (op==5) r.SetDimension(position,size);
            e[0]=px; e[1]=py; e[2]=px+w; e[3]=py+h;
        } else if (op==6) { r.SetWidth(w); e[2]=e[0]+w; }
        else if (op==7) { r.SetHeight(h); e[3]=e[1]+h; }
        else if (op==8) { r.SetSize(size); e[2]=e[0]+w; e[3]=e[1]+h; }
        else if (op==9) { r.SetHalfSize(size); e[0]=cx-w; e[1]=cy-h; e[2]=cx+w; e[3]=cy+h; }
        else if (op==10) { r.SetCenter(position); e[0]=px-hx; e[1]=py-hy; e[2]=px+hx; e[3]=py+hy; }
        else if (op==11 || op==12) {
            if (op==11) r.SetCenter(position,size); else r.SetCenter(px,py,w,h);
            e[0]=px-w; e[1]=py-h; e[2]=px+w; e[3]=py+h;
        } else if (op==13) { r.SetTopLeft(position); e[0]=px; e[1]=py; }
        else if (op==14) { r.SetBottomRight(position); e[2]=px; e[3]=py; }
        else if (op==15) { r.Move(position); e[0]=px; e[1]=py; e[2]=px+2*hx; e[3]=py+2*hy; }
        else if (op==16 || op==28 || op==29) {
            if (op==16) r.Translate(position); else if (op==28) r+=position; else r-=position;
            const int sign=op==29?-1:1;
            e[0]+=sign*px; e[2]+=sign*px; e[1]+=sign*py; e[3]+=sign*py;
        } else if (op==17) { r.HMove(px); e[0]=px; e[2]=px+2*hx; }
        else if (op==18) { r.VMove(py); e[1]=py; e[3]=py+2*hy; }
        else if (op==19) { r.HTranslate(px); e[0]+=px; e[2]+=px; }
        else if (op==20) { r.VTranslate(py); e[1]+=py; e[3]+=py; }
        else if (op==21) { r.Scale(Vx2DVector(-2.0f,0.5f)); e[2]=e[0]-4*hx; e[3]=e[1]+hy; }
        else if (op==22) { r.Inflate(Vx2DVector(3.5f,-1.25f)); e[0]-=3.5; e[2]+=3.5; e[1]+=1.25; e[3]-=1.25; }
        else if (op==23) {
            const float weight=float(n%9)*0.25f-0.5f;
            r.Interpolate(weight,VxRect(px-w,py-h,px+w,py+h));
            const double ncx=(1-weight)*cx+weight*px, ncy=(1-weight)*cy+weight*py;
            const double nhx=(1-weight)*hx+weight*w, nhy=(1-weight)*hy+weight*h;
            e[0]=ncx-nhx; e[1]=ncy-nhy; e[2]=ncx+nhx; e[3]=ncy+nhy;
        } else if (op==24) {
            r.Merge(VxRect(px-w,py-h,px+w,py+h));
            e[0]=std::fmin(e[0],double(px-w)); e[1]=std::fmin(e[1],double(py-h));
            e[2]=std::fmax(e[2],double(px+w)); e[3]=std::fmax(e[3],double(py+h));
        } else if (op==25) {
            r.Bounding(position,Vx2DVector(cx,cy)); e[0]=std::fmin(px,cx); e[1]=std::fmin(py,cy);
            e[2]=std::fmax(px,cx); e[3]=std::fmax(py,cy);
        } else if (op==26) {
            r.SetCorners(cx+hx,cy+hy,cx-hx,cy-hy); r.Normalize();
        } else if (op==27) { r.Clear(); e[0]=e[1]=e[2]=e[3]=0; }
        else if (op==30) { r*=Vx2DVector(-2.0f,0.5f); e[0]*=-2; e[2]*=-2; e[1]*=0.5; e[3]*=0.5; }
        else if (op==31) { r/=Vx2DVector(2.0f,-4.0f); e[0]*=0.5; e[2]*=0.5; e[1]*=-0.25; e[3]*=-0.25; }
        ObserveRect(result,r,e);
    }
    return result;
}

// Each expected result uses values captured before invoking the operation.
// Input references deliberately remain bound to actual mutable rectangle corners.
static RectCoreSamples RectAliasDataset(int op) {
    RectCoreSamples result;
    for (int inverted=0; inverted<2; ++inverted)
    for (int p=0; p<3; ++p) for (int q=0; q<(op<4?3:1); ++q) {
        const float l=inverted?10.0f:2.0f, t=inverted?15.0f:3.0f;
        const float right=inverted?2.0f:10.0f, bottom=inverted?3.0f:15.0f;
        const double width=right-l, height=bottom-t;
        VxRect r(l,t,right,bottom); const Vx2DVector external(4,6), external2(5,7);
        const Vx2DVector &a=p==0?external:(p==1?r.GetTopLeft():r.GetBottomRight());
        const Vx2DVector &b=q==0?external2:(q==1?r.GetTopLeft():r.GetBottomRight());
        const double x=a.x,y=a.y,u=b.x,v=b.y; double e[]={l,t,right,bottom};
        if (op==0) { r.SetCorners(a,b); e[0]=x; e[1]=y; e[2]=u; e[3]=v; }
        else if (op==1) { r.SetDimension(a,b); e[0]=x; e[1]=y; e[2]=x+u; e[3]=y+v; }
        else if (op==2) { r.SetCenter(a,b); e[0]=x-u; e[1]=y-v; e[2]=x+u; e[3]=y+v; }
        else if (op==3) { r.Bounding(a,b); e[0]=std::fmin(x,u); e[1]=std::fmin(y,v); e[2]=std::fmax(x,u); e[3]=std::fmax(y,v); }
        else if (op==4) { r.SetCenter(a); e[0]=x-width*0.5; e[1]=y-height*0.5; e[2]=x+width*0.5; e[3]=y+height*0.5; }
        else if (op==5) { r.SetHalfSize(a); e[0]=6-x; e[1]=9-y; e[2]=6+x; e[3]=9+y; }
        else if (op==6) { r.Move(a); e[0]=x; e[1]=y; e[2]=x+width; e[3]=y+height; }
        else if (op==7 || op==9 || op==10) {
            if (op==7) r.Translate(a); else if (op==9) r+=a; else r-=a;
            const int sign=op==10?-1:1; e[0]+=sign*x; e[2]+=sign*x; e[1]+=sign*y; e[3]+=sign*y;
        } else if (op==8) { r.Inflate(a); e[0]-=x; e[2]+=x; e[1]-=y; e[3]+=y; }
        else if (op==11) { r*=a; e[0]*=x; e[2]*=x; e[1]*=y; e[3]*=y; }
        else if (op==12) { r/=a; e[0]/=x; e[2]/=x; e[1]/=y; e[3]/=y; }
        else if (op==13) { r.SetSize(a); e[2]=l+x; e[3]=t+y; }
        else if (op==14) { r.Scale(a); e[2]=l+width*x; e[3]=t+height*y; }
        else if (op==15) { r.SetTopLeft(a); e[0]=x; e[1]=y; }
        else if (op==16) { r.SetBottomRight(a); e[2]=x; e[3]=y; }
        ObserveRect(result,r,e);
    }
    return result;
}

static RectCoreSamples RectIntegerDataset() {
    RectCoreSamples result;
    for (int n=0; n<64; ++n) {
        CKRECT input={n*37-1000,n*19-600,n*97+2,n*11-999}, output={};
        VxRect r; r.CopyFrom(input); const double e[]={double(input.left),double(input.top),double(input.right),double(input.bottom)};
        ObserveRect(result,r,e); r.CopyTo(&output);
        if (output.left!=input.left || output.top!=input.top || output.right!=input.right || output.bottom!=input.bottom) ++result.errors;
    }
    return result;
}

#ifndef VX_RECT_CORE_NATIVE_PROBE
static void ExpectRectCore(const VxRect &r, float l, float t, float right, float b) {
    EXPECT_FLOAT_EQ(r.left,l); EXPECT_FLOAT_EQ(r.top,t); EXPECT_FLOAT_EQ(r.right,right); EXPECT_FLOAT_EQ(r.bottom,b);
}

static void RecordRectCore(const RectCoreSamples &result) {
    char digest[16]; std::snprintf(digest,sizeof(digest),"%u",result.digest);
    EXPECT_EQ(result.errors,0); testing::Test::RecordProperty("samples",result.samples); testing::Test::RecordProperty("digest",digest);
}

TEST(RectCoreReferenceTest, LayoutIsFourOrderedFloatsWithNoPointerDependentStorage) {
    EXPECT_EQ(sizeof(VxRect),16u); EXPECT_EQ(alignof(VxRect),4u);
    EXPECT_EQ(offsetof(VxRect,left),0u); EXPECT_EQ(offsetof(VxRect,top),4u);
    EXPECT_EQ(offsetof(VxRect,right),8u); EXPECT_EQ(offsetof(VxRect,bottom),12u);
    EXPECT_TRUE(std::is_standard_layout<VxRect>::value);
    EXPECT_TRUE(std::is_trivially_copyable<VxRect>::value);
    EXPECT_TRUE(std::is_trivially_destructible<VxRect>::value);
    VxRect r(1.25f,-2.5f,3.75f,4.5f); float storage[4]; std::memcpy(storage,&r,sizeof(r));
    EXPECT_FLOAT_EQ(storage[0],1.25f); EXPECT_FLOAT_EQ(storage[1],-2.5f);
    EXPECT_FLOAT_EQ(storage[2],3.75f); EXPECT_FLOAT_EQ(storage[3],4.5f);
}

TEST(RectCoreReferenceTest, ConstructorsAndInitializationKeepSuppliedCornerOrder) {
    Vx2DVector a(8,9), b(2,3); VxRect vectors(a,b), scalar(8,9,2,3), initialized;
    initialized.SetCorners(8,9,2,3);
    ExpectRectCore(vectors,8,9,2,3); ExpectRectCore(scalar,8,9,2,3); ExpectRectCore(initialized,8,9,2,3);
    EXPECT_EQ(vectors,scalar); EXPECT_EQ(initialized,scalar);
}

TEST(RectCoreReferenceTest, MutableAndConstCornerAccessorsReferToRealStorage) {
    VxRect r(2,3,10,15); const VxRect &read=r;
    EXPECT_EQ(&r.GetTopLeft(),&read.GetTopLeft()); EXPECT_EQ(&r.GetBottomRight(),&read.GetBottomRight());
    EXPECT_EQ(reinterpret_cast<const void *>(&r.GetTopLeft()),reinterpret_cast<const void *>(&r.left));
    EXPECT_EQ(reinterpret_cast<const void *>(&r.GetBottomRight()),reinterpret_cast<const void *>(&r.right));
    r.GetTopLeft()=Vx2DVector(-1,-2); r.GetBottomRight()=Vx2DVector(7,8);
    ExpectRectCore(r,-1,-2,7,8); r.left=4; r.bottom=12;
    EXPECT_FLOAT_EQ(read.GetTopLeft().x,4); EXPECT_FLOAT_EQ(read.GetBottomRight().y,12);
}

TEST(RectCoreReferenceTest, IntegerCopyPreservesRepresentableValues) { RecordRectCore(RectIntegerDataset()); }

TEST(RectCoreReferenceTest, IntegerCopyTruncatesFractionsTowardZeroUnderEveryRoundingMode) {
    const int saved=std::fegetround(), modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    VxRect r(-3.75f,2.75f,-0.5f,0.5f);
    for (int mode:modes) {
        EXPECT_EQ(std::fesetround(mode),0); CKRECT output={}; r.CopyTo(&output);
        EXPECT_EQ(output.left,-3); EXPECT_EQ(output.top,2); EXPECT_EQ(output.right,0); EXPECT_EQ(output.bottom,0);
    }
    EXPECT_EQ(std::fesetround(saved),0);
}

TEST(RectCoreReferenceTest, NullAndEmptyHaveDistinctCoordinateDefinitions) {
    VxRect r; r.Clear(); EXPECT_TRUE(r.IsNull()); EXPECT_TRUE(r.IsEmpty());
    r.SetCorners(2,3,2,3); EXPECT_FALSE(r.IsNull()); EXPECT_TRUE(r.IsEmpty());
    r.SetCorners(2,3,2,9); EXPECT_TRUE(r.IsEmpty());
    r.SetCorners(2,3,8,3); EXPECT_TRUE(r.IsEmpty());
    r.SetCorners(8,9,2,3); EXPECT_FALSE(r.IsEmpty());
    r.SetCorners(-0.0f,0,-0.0f,0); EXPECT_TRUE(r.IsNull()); EXPECT_TRUE(r.IsEmpty());
}

TEST(RectCoreReferenceTest, CenterAndHalfSizeMutatorsPreserveTheirGeometricAnchor) {
    VxRect r(2,3,10,15); r.SetHalfSize(Vx2DVector(2,3)); ExpectRectCore(r,4,6,8,12);
    r.SetCenter(Vx2DVector(-2,-3)); ExpectRectCore(r,-4,-6,0,0);
    r.SetCenter(Vx2DVector(5,7),Vx2DVector(3,4)); ExpectRectCore(r,2,3,8,11);
    r.SetCenter(5,7,3,4); ExpectRectCore(r,2,3,8,11);
}

TEST(RectCoreReferenceTest, ScaleAnchorsTopLeftWhileCoordinateMultiplyScalesTheOrigin) {
    VxRect scaled(2,3,10,15), multiplied=scaled;
    scaled.Scale(Vx2DVector(-2.0f,0.5f)); multiplied*=Vx2DVector(-2.0f,0.5f);
    ExpectRectCore(scaled,2,3,-14,9); ExpectRectCore(multiplied,-4,1.5f,-20,7.5f);
}

TEST(RectCoreReferenceTest, InflationCanCollapseAndInvertWithoutImplicitNormalization) {
    VxRect r(2,3,10,15); r.Inflate(Vx2DVector(-4,-6)); ExpectRectCore(r,6,9,6,9); EXPECT_TRUE(r.IsEmpty());
    r.Inflate(Vx2DVector(-1,-2)); ExpectRectCore(r,7,11,5,7); EXPECT_FALSE(r.IsEmpty());
    r.Normalize(); ExpectRectCore(r,5,7,7,11);
}

TEST(RectCoreReferenceTest, InterpolationExtrapolatesAndAcceptsItsOwnRectangle) {
    VxRect r(2,3,10,15), target(6,7,18,23); r.Interpolate(0.5f,target); ExpectRectCore(r,4,5,14,19);
    r.Interpolate(2,target); ExpectRectCore(r,8,9,22,27);
    r.Interpolate(-0.5f,r); ExpectRectCore(r,8,9,22,27);
}

TEST(RectCoreReferenceTest, BoundingNormalizeAndMergeProduceTheExpectedEnvelope) {
    VxRect r; r.Bounding(Vx2DVector(10,3),Vx2DVector(2,15)); ExpectRectCore(r,2,3,10,15);
    r.Merge(VxRect(-2,5,4,20)); ExpectRectCore(r,-2,3,10,20);
    r.Merge(r); ExpectRectCore(r,-2,3,10,20);
    r.SetCorners(10,15,2,3); r.Normalize(); r.Normalize(); ExpectRectCore(r,2,3,10,15);
}

TEST(RectCoreReferenceTest, EqualityChecksEveryCoordinateAndUsesFloatEquality) {
    const VxRect original(2,3,10,15);
    const VxRect variants[]={VxRect(1,3,10,15),VxRect(2,1,10,15),VxRect(2,3,1,15),VxRect(2,3,10,1)};
    EXPECT_TRUE(original==original); EXPECT_FALSE(original!=original);
    for (const VxRect &r:variants) { EXPECT_FALSE(original==r); EXPECT_TRUE(original!=r); }
    EXPECT_TRUE(VxRect(-0.0f,0,0,0)==VxRect(0,0,0,0));
    const float nan=std::nanf(""); const VxRect unordered(nan,0,1,1);
    EXPECT_FALSE(unordered==unordered); EXPECT_TRUE(unordered!=unordered);
}

TEST(RectCoreReferenceTest, ImplicitCopyAndAssignmentCopyTheFourCoordinatesIndependently) {
    VxRect original(2,3,10,15), copy(original), assigned; assigned=original;
    copy.GetTopLeft().x=-4; assigned.GetBottomRight().y=99;
    ExpectRectCore(original,2,3,10,15); ExpectRectCore(copy,-4,3,10,15); ExpectRectCore(assigned,2,3,10,99);
    EXPECT_EQ(&(assigned+=Vx2DVector(1,2)),&assigned);
    EXPECT_EQ(&(assigned-=Vx2DVector(1,2)),&assigned);
    EXPECT_EQ(&(assigned*=Vx2DVector(2,4)),&assigned);
    EXPECT_EQ(&(assigned/=Vx2DVector(2,4)),&assigned);
}

TEST(RectCoreReferenceTest, GeometryDatasetMatchesIndependentCenterAndExtentExpectations) {
    const RectCoreSamples result=RectGeometryDataset(); EXPECT_EQ(result.samples,2048); RecordRectCore(result);
}

#define RECT_ALIAS_TEST(name,op) TEST(RectCoreReferenceTest,name) { RecordRectCore(RectAliasDataset(op)); }
RECT_ALIAS_TEST(SetCornersSnapshotsBothCornerReferences,0)
RECT_ALIAS_TEST(SetDimensionSnapshotsPositionAndSize,1)
RECT_ALIAS_TEST(SetCenterSnapshotsCenterAndHalfSize,2)
RECT_ALIAS_TEST(BoundingSnapshotsBothPointReferences,3)
RECT_ALIAS_TEST(SetCenterSnapshotsACornerUsedAsTheNewCenter,4)
RECT_ALIAS_TEST(SetHalfSizeSnapshotsACornerUsedAsTheHalfSize,5)
RECT_ALIAS_TEST(MoveSnapshotsACornerUsedAsTheDestination,6)
RECT_ALIAS_TEST(TranslateSnapshotsACornerUsedAsTheOffset,7)
RECT_ALIAS_TEST(InflateSnapshotsACornerUsedAsTheExtent,8)
RECT_ALIAS_TEST(AddAssignSnapshotsACornerUsedAsTheOffset,9)
RECT_ALIAS_TEST(SubtractAssignSnapshotsACornerUsedAsTheOffset,10)
RECT_ALIAS_TEST(MultiplyAssignSnapshotsACornerUsedAsTheFactor,11)
RECT_ALIAS_TEST(DivideAssignSnapshotsACornerUsedAsTheDivisor,12)
RECT_ALIAS_TEST(SetSizeAcceptsItsOwnCornerReference,13)
RECT_ALIAS_TEST(ScaleAcceptsItsOwnCornerReference,14)
RECT_ALIAS_TEST(SetTopLeftAcceptsEitherCornerReference,15)
RECT_ALIAS_TEST(SetBottomRightAcceptsEitherCornerReference,16)
#undef RECT_ALIAS_TEST
#endif
