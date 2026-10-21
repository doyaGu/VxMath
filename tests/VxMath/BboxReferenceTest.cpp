#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>
#ifndef VX_BBOX_NATIVE_PROBE
#include "VxBbox.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct BboxSamples { int samples=0,errors=0; unsigned digest=2166136261u; };
typedef void (*BboxBatch)(const VxBbox &,int,XBYTE *,XDWORD,XDWORD *);
typedef void (*BboxAxis)(const VxBbox &,int,XBYTE *,XDWORD,int,XDWORD *);
static void BboxObserve(BboxSamples &r,float value,double expected) {
    if (!std::isfinite(value) || std::fabs(double(value)-expected)>2e-6*(1+std::fabs(expected))) ++r.errors;
    unsigned bits=0;if (value!=0) std::memcpy(&bits,&value,4);
    for (int b=0;b<4;++b) { r.digest=(r.digest^(bits&255u))*16777619u;bits>>=8; }
}
static void BboxObserveBox(BboxSamples &r,const VxBbox &box,const VxVector &low,const VxVector &high) {
    for (int axis=0;axis<3;++axis) { BboxObserve(r,box.Min[axis],low[axis]);BboxObserve(r,box.Max[axis],high[axis]); }
    ++r.samples;
}
static void BboxBatchMember(const VxBbox &box,int n,XBYTE *v,XDWORD stride,XDWORD *flags) {
#if defined(VX_BBOX_NATIVE_PROBE)
    alignas(16) XULONG originalFlags[128];box.ClassifyVertices(n,v,stride,originalFlags);
    for (int i=0;i<n;++i) flags[i]=XDWORD(originalFlags[i]);
#else
    box.ClassifyVertices(n,v,stride,flags);
#endif
}
static void BboxAxisMember(const VxBbox &box,int n,XBYTE *v,XDWORD stride,int axis,XDWORD *flags) {
#if defined(VX_BBOX_NATIVE_PROBE)
    alignas(16) XULONG originalFlags[128];box.ClassifyVerticesOneAxis(n,v,stride,axis,originalFlags);
    for (int i=0;i<n;++i) flags[i]=XDWORD(originalFlags[i]);
#else
    box.ClassifyVerticesOneAxis(n,v,stride,axis,flags);
#endif
}
static XDWORD BboxExpectedPoint(const VxVector &p,const VxBbox &b,bool compact) {
    const XDWORD low[]={compact?1u:16u,compact?2u:128u,compact?4u:512u};
    const XDWORD high[]={compact?8u:32u,compact?16u:64u,compact?32u:256u};
    XDWORD flags=0;for (int a=0;a<3;++a) { if (p[a]<b.Min[a]) flags|=low[a];else if (p[a]>b.Max[a]) flags|=high[a]; }
    return flags;
}
static BboxSamples BboxBatchDataset(BboxBatch batch=BboxBatchMember,BboxAxis axisBatch=BboxAxisMember,bool oneAxis=false) {
    BboxSamples result;const VxBbox box(VxVector(-1,-2,-3),VxVector(1,2,3));
    const int counts[]={8,12,16,20,124};const unsigned strides[]={16,32,48};
    for (unsigned stride:strides) for (int count:counts) for (int seed=0;seed<5;++seed) for (int axis=0;axis<(oneAxis?3:1);++axis) {
        alignas(16) XBYTE storage[48*128+16];std::memset(storage,0xa5,sizeof(storage));
        for (int n=0;n<count;++n) {
            const int index=(n+seed*25)%125;
            const float xyz[]={float(index%5-2),float((index/5)%5-2)*2,float(index/25-2)*3};
            std::memcpy(storage+n*stride,xyz,12);
        }
        XBYTE before[sizeof(storage)];std::memcpy(before,storage,sizeof(storage));alignas(16) XDWORD flags[128];
        for (XDWORD &flag:flags) flag=0xdeadbeefu;
        if (oneAxis) axisBatch(box,count,storage,stride,axis,flags);else batch(box,count,storage,stride,flags);
        for (int n=0;n<count;++n) {
            float xyz[3];std::memcpy(xyz,storage+n*stride,12);const VxVector p(xyz[0],xyz[1],xyz[2]);
            const XDWORD expected=oneAxis?(p[axis]<box.Min[axis]?1u:p[axis]>box.Max[axis]?2u:0u):BboxExpectedPoint(p,box,true);
            BboxObserve(result,float(flags[n]),expected);++result.samples;
        }
        if (std::memcmp(before,storage,sizeof(storage))) ++result.errors;
        for (int n=count;n<128;++n) if (flags[n]!=0xdeadbeefu) ++result.errors;
    }
    return result;
}
static void BboxTestMatrix(VxMatrix &m,int kind) {
    m.SetIdentity();
    if (kind==1) { m[0]=VxVector(0,1,0);m[1]=VxVector(-1,0,0);m[3]=VxVector(2,-4,1); }
    if (kind==2) { m[0]=VxVector(-2,0,0);m[1]=VxVector(0,3,0);m[2]=VxVector(0,0,0.5f); }
    if (kind==3) { m[0]=VxVector(1,0.5f,0);m[1]=VxVector(0,1,-1);m[2]=VxVector(0.25f,0,1);m[3]=VxVector(1,2,-3); }
    if (kind==4) { m[0]=VxVector(0,0,0);m[1]=VxVector(0,2,0);m[2]=VxVector(0,0,-1);m[3]=VxVector(2,-1,3); }
    if (kind==5) m[3]=VxVector(-6,4,2);
    if (kind==6) { m[0]=VxVector(0,0,-2);m[1]=VxVector(0.5f,0,0);m[2]=VxVector(0,3,0);m[3]=VxVector(-2,1,4); }
}
static BboxSamples BboxTransformDataset(bool inPlace=false,bool corners=false) {
    BboxSamples r;const VxVector extents[]={VxVector(0,0,0),VxVector(1,2,3),VxVector(2,0,1),VxVector(0.5f,1.5f,2.5f)};
    for (int x=-4;x<=4;x+=4) for (int y=-4;y<=4;y+=4) for (int z=-4;z<=4;z+=4) for (const VxVector &extent:extents) for (int kind=0;kind<7;++kind) {
        const VxVector center{float(x),float(y),float(z)};VxBbox source(center-extent,center+extent),out=source;VxMatrix m;BboxTestMatrix(m,kind);
        VxVector transformed[8];if (corners) source.TransformTo(transformed,m);else out.TransformFrom(inPlace?out:source,m);
        double low[3]={},high[3]={};
        for (int corner=0;corner<8;++corner) {
            const double local[]={(corner&4)?source.Max.x:source.Min.x,(corner&2)?source.Max.y:source.Min.y,(corner&1)?source.Max.z:source.Min.z};
            for (int axis=0;axis<3;++axis) {
                double value=m[3][axis];for (int row=0;row<3;++row) value+=local[row]*m[row][axis];
                if (!corner || value<low[axis]) low[axis]=value;if (!corner || value>high[axis]) high[axis]=value;
                if (corners) BboxObserve(r,transformed[corner][axis],value);
            }
        }
        if (!corners) for (int axis=0;axis<3;++axis) { BboxObserve(r,out.Min[axis],low[axis]);BboxObserve(r,out.Max[axis],high[axis]); }
        ++r.samples;
    }
    return r;
}
static BboxSamples BboxSetterAliases() {
    BboxSamples r;const VxVector external(2,4,6);
    for (int kind=0;kind<3;++kind) for (int first=0;first<3;++first) for (int second=0;second<3;++second) {
        VxBbox box(VxVector(-3,-2,-1),VxVector(5,7,9));
        const VxVector &a=first==0?box.Min:first==1?box.Max:external;
        const VxVector &b=second==0?box.Min:second==1?box.Max:external;
        const VxVector savedA=a,savedB=b;VxVector low,high;
        if (kind==0) { low=savedA;high=savedB;box.SetCorners(a,b); }
        if (kind==1) { low=savedA;high=savedA+savedB;box.SetDimension(a,b); }
        if (kind==2) { low=savedA-savedB;high=savedA+savedB;box.SetCenter(a,b); }
        BboxObserveBox(r,box,low,high);
    }
    return r;
}
static BboxSamples BboxViewDataset() {
    // Rows: equal, contained, enclosing, partial overlap, separated, touching,
    // interior point, boundary point. Columns: viewer inside self, between
    // self and enclosing box, outside both. These are the original -1/0/1
    // visibility relations, not the inaccurate old SDK prose's 0/1/2 labels.
    const int truth[][3]={{1,-1,-1},{0,-1,-1},{1,-1,1},{0,0,0},{-1,0,0},{0,0,0},{0,-1,-1},{0,-1,-1}};
    const VxBbox others[]={VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1)),VxBbox(VxVector(-0.5f,-0.5f,-0.5f),VxVector(0.5f,0.5f,0.5f)),VxBbox(3),VxBbox(VxVector(0,-1,-1),VxVector(2,1,1)),VxBbox(VxVector(2,-1,-1),VxVector(4,1,1)),VxBbox(VxVector(1,-1,-1),VxVector(2,1,1)),VxBbox(0),VxBbox(VxVector(1,0,0),VxVector(1,0,0))};
    const int positions[]={-4,-2,-1,0,1,2,4};BboxSamples r;
    for (int kind=0;kind<8;++kind) for (int x:positions) for (int y:positions) for (int z:positions) {
        const int ax=std::abs(x),ay=std::abs(y),az=std::abs(z);const int region=ax<=1 && ay<=1 && az<=1?0:ax<=3 && ay<=3 && az<=3?1:2;
        const VxBbox self(1);BboxObserve(r,float(self.Classify(others[kind],VxVector(float(x),float(y),float(z)))),truth[kind][region]);++r.samples;
    }
    return r;
}
static BboxSamples BboxOrdinaryDataset() {
    BboxSamples r;const VxVector extents[]={VxVector(0,0,0),VxVector(1,2,3),VxVector(2,1,0)};VxBbox boxes[81];int count=0;
    for (int x=-2;x<=2;x+=2) for (int y=-2;y<=2;y+=2) for (int z=-2;z<=2;z+=2) for (const VxVector &e:extents) { const VxVector c{float(x),float(y),float(z)};boxes[count++]=VxBbox(c-e,c+e); }
    for (const VxBbox &box:boxes) {
        for (int x=-4;x<=4;x+=2) for (int y=-4;y<=4;y+=2) for (int z=-4;z<=4;z+=2) {
            const VxVector p{float(x),float(y),float(z)};const XDWORD flags=BboxExpectedPoint(p,box,false);
            BboxObserve(r,float(box.Classify(p)),flags);BboxObserve(r,float(box.VectorIn(p)),flags==0);++r.samples;
        }
        for (const VxBbox &other:boxes) {
            VxVector mergedMin,mergedMax,intersectionMin,intersectionMax;XDWORD flags=0;bool inside=true;
            const XDWORD below[]={16,128,512},above[]={32,64,256};
            for (int a=0;a<3;++a) {
                mergedMin[a]=box.Min[a]<other.Min[a]?box.Min[a]:other.Min[a];mergedMax[a]=box.Max[a]>other.Max[a]?box.Max[a]:other.Max[a];
                intersectionMin[a]=box.Min[a]>other.Min[a]?box.Min[a]:other.Min[a];intersectionMax[a]=box.Max[a]<other.Max[a]?box.Max[a]:other.Max[a];
                if (other.Max[a]<box.Min[a]) flags|=below[a];else if (other.Min[a]>box.Max[a]) flags|=above[a];
                if (other.Min[a]<box.Min[a] || other.Max[a]>box.Max[a]) inside=false;
            }
            BboxObserve(r,float(box.Classify(other)),flags);BboxObserve(r,float(box.IsBoxInside(other)),inside);
            VxBbox merged=box,intersection=box;merged.Merge(other);intersection.Intersect(other);
            BboxObserveBox(r,merged,mergedMin,mergedMax);BboxObserveBox(r,intersection,intersectionMin,intersectionMax);
        }
    }
    return r;
}

#ifndef VX_BBOX_NATIVE_PROBE
static void RecordBbox(const BboxSamples &r) {
    EXPECT_EQ(r.errors,0);char digest[16];std::snprintf(digest,sizeof(digest),"%u",r.digest);
    testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",digest);
#if defined(VX_SIMD_SSE2)
    testing::Test::RecordProperty("compiled_sse2",1);
#else
    testing::Test::RecordProperty("compiled_sse2",0);
#endif
}
TEST(BboxReferenceTest, LayoutAndImplicitCopyRetainMaxThenMinSixFloatStorage) {
    EXPECT_EQ(sizeof(VxBbox),24u);EXPECT_EQ(alignof(VxBbox),4u);EXPECT_EQ(offsetof(VxBbox,Max),0u);EXPECT_EQ(offsetof(VxBbox,Min),12u);
    EXPECT_TRUE(std::is_standard_layout<VxBbox>::value);EXPECT_TRUE(std::is_trivially_copyable<VxBbox>::value);EXPECT_TRUE(std::is_trivially_destructible<VxBbox>::value);
    VxBbox box(VxVector(-1,-2,-3),VxVector(4,5,6)),copy(box),assigned;assigned=box;
    const float values[]={4,5,6,-1,-2,-3};EXPECT_EQ(std::memcmp(&box,values,24),0);
#if defined(_MSC_VER)
    for (int n=0;n<6;++n) EXPECT_FLOAT_EQ(box.v[n],values[n]);box.v[3]=-7;EXPECT_FLOAT_EQ(box.Min.x,-7);
#endif
    EXPECT_EQ(copy,assigned);assigned.Max.x=50;EXPECT_FLOAT_EQ(copy.Max.x,4);
}
TEST(BboxReferenceTest, DefaultResetAndRadiusConstructorsKeepOriginalSentinels) {
    VxBbox box;EXPECT_EQ(box.Min,VxVector(1e6f,1e6f,1e6f));EXPECT_EQ(box.Max,VxVector(-1e6f,-1e6f,-1e6f));EXPECT_FALSE(box.IsValid());
    box.Merge(VxVector(2,3,4));EXPECT_EQ(box.Min,VxVector(2,3,4));EXPECT_EQ(box.Max,box.Min);box.Reset();EXPECT_FALSE(box.IsValid());
    const VxBbox radius(2),negative(-2);EXPECT_EQ(radius.Min,VxVector(-2,-2,-2));EXPECT_EQ(radius.Max,VxVector(2,2,2));EXPECT_FALSE(negative.IsValid());
}
TEST(BboxReferenceTest, ValidityIncludesFlatAndPointBoxesAndRejectsEveryInvertedAxis) {
    const VxBbox box(VxVector(-1,-2,-3),VxVector(4,5,6));EXPECT_TRUE(box.IsValid());EXPECT_TRUE(VxBbox(0).IsValid());
    for (int a=0;a<3;++a) { VxBbox copy=box;copy.Min[a]=copy.Max[a]+1;EXPECT_FALSE(copy.IsValid()); }
}
TEST(BboxReferenceTest, GettersAndOrdinarySettersPreservePositionAndExtentMeaning) {
    VxBbox box(VxVector(-2,-4,-6),VxVector(4,8,12));EXPECT_EQ(box.GetSize(),VxVector(6,12,18));EXPECT_EQ(box.GetHalfSize(),VxVector(3,6,9));EXPECT_EQ(box.GetCenter(),VxVector(1,2,3));
    box.SetCorners(VxVector(1,2,3),VxVector(4,6,8));EXPECT_EQ(box.GetSize(),VxVector(3,4,5));
    box.SetDimension(VxVector(1,2,3),VxVector(3,4,5));EXPECT_EQ(box.Max,VxVector(4,6,8));
    box.SetCenter(VxVector(2,4,6),VxVector(1,2,3));EXPECT_EQ(box.Min,VxVector(1,2,3));EXPECT_EQ(box.Max,VxVector(3,6,9));
}
TEST(BboxReferenceTest, SetterArgumentsMayReferenceTheDestinationCorners) { RecordBbox(BboxSetterAliases()); }
TEST(BboxReferenceTest, CenterDoesNotOverflowWhenBothFiniteEndpointsHaveTheSameSign) {
    const VxBbox box(VxVector(FLT_MAX,-FLT_MAX,FLT_MAX),VxVector(FLT_MAX,-FLT_MAX,FLT_MAX));
    EXPECT_EQ(box.GetCenter(),VxVector(FLT_MAX,-FLT_MAX,FLT_MAX));EXPECT_EQ(box.GetHalfSize(),VxVector(0,0,0));
}
TEST(BboxReferenceTest, HalfSizeDoesNotOverflowBeforeHalvingOppositeFiniteEndpoints) {
    const VxBbox box(VxVector(-FLT_MAX,-FLT_MAX,-FLT_MAX),VxVector(FLT_MAX,FLT_MAX,FLT_MAX));
    EXPECT_EQ(box.GetHalfSize(),VxVector(FLT_MAX,FLT_MAX,FLT_MAX));EXPECT_EQ(box.GetCenter(),VxVector(0,0,0));EXPECT_TRUE(std::isinf(box.GetSize().x));
}
TEST(BboxReferenceTest, OrdinaryPointBoxMergeAndIntersectionMatchIntervalGeometry) { RecordBbox(BboxOrdinaryDataset()); }
TEST(BboxReferenceTest, EqualityTestsAllSixCoefficientsNumerically) {
    const VxBbox box(VxVector(-1,-2,-3),VxVector(4,5,6));EXPECT_TRUE(box==VxBbox(box));
    for (int n=0;n<6;++n) { VxBbox copy=box;if (n<3) copy.Max[n]+=1;else copy.Min[n-3]+=1;EXPECT_FALSE(copy==box); }
    EXPECT_TRUE(VxBbox(VxVector(-0.0f,0,0),VxVector(0,0,0))==VxBbox(0));
}
TEST(BboxReferenceTest, PointMergeAndSelfMergeKeepClosedBounds) {
    VxBbox box(VxVector(-1,-2,-3),VxVector(4,5,6));box.Merge(VxVector(8,-7,0));EXPECT_EQ(box.Max,VxVector(8,5,6));EXPECT_EQ(box.Min,VxVector(-1,-7,-3));
    const VxBbox before=box;box.Merge(box);EXPECT_EQ(box,before);box.Intersect(box);EXPECT_EQ(box,before);box.Merge(box.Min);EXPECT_EQ(box,before);box.Merge(box.Max);EXPECT_EQ(box,before);
}
TEST(BboxReferenceTest, ViewpointClassificationMatchesOriginalSignedRelationTable) { RecordBbox(BboxViewDataset()); }
TEST(BboxReferenceTest, BatchVerticesUseCompactBoxFlagsRatherThanGlobalClipFlags) { RecordBbox(BboxBatchDataset()); }
TEST(BboxReferenceTest, SingleAxisBatchUsesOneForBelowAndTwoForAbove) { RecordBbox(BboxBatchDataset(BboxBatchMember,BboxAxisMember,true)); }
TEST(BboxReferenceTest, BatchCountsZeroThroughSevenAndFourByteAlignedStridesWriteExactlyTheirOutputs) {
    const VxBbox box(1);const unsigned strides[]={0,12,16,28};
    for (int count=0;count<=7;++count) for (unsigned stride:strides) {
        alignas(16) XBYTE storage[256]={};XDWORD flags[10];for (XDWORD &v:flags) v=0xdeadbeefu;
        for (int i=0;i<(stride?count:1);++i) { const float xyz[]={-2,0,2};std::memcpy(storage+4+i*stride,xyz,12); }
        box.ClassifyVertices(count,storage+4,stride,flags+1);
        EXPECT_EQ(flags[0],0xdeadbeefu);EXPECT_EQ(flags[count+1],0xdeadbeefu);
        for (int i=0;i<count;++i) EXPECT_EQ(flags[i+1],33u);
        for (int axis=0;axis<3;++axis) { box.ClassifyVerticesOneAxis(count,storage+4,stride,axis,flags+1);for (int i=0;i<count;++i) EXPECT_EQ(flags[i+1],axis==0?1u:axis==1?0u:2u); }
    }
}
TEST(BboxReferenceTest, TransformToPreservesOriginalCornerOrderAcrossSevenAffineMaps) { RecordBbox(BboxTransformDataset(false,true)); }
TEST(BboxReferenceTest, TransformFromMatchesIndependentEightCornerBounds) { RecordBbox(BboxTransformDataset()); }
TEST(BboxReferenceTest, InPlaceTransformFromUsesAllSourceBoundsBeforeWriting) { RecordBbox(BboxTransformDataset(true)); }
#if defined(VX_SIMD_SSE)
static void BboxDirectBatch(const VxBbox &box,int n,XBYTE *v,XDWORD stride,XDWORD *flags) { VxSIMDBboxClassifyVertices(&box,n,v,stride,flags); }
TEST(BboxReferenceTest, DirectSimdBatchPreservesTheCompactMaskContract) { RecordBbox(BboxBatchDataset(BboxDirectBatch)); }
TEST(BboxReferenceTest, DirectSimdTransformsRetainCornerOrderAndSupportInPlaceBounds) {
    const VxBbox source(VxVector(-1,-2,-3),VxVector(4,5,6));VxMatrix m;BboxTestMatrix(m,3);VxVector points[8],expected[8];
    source.TransformTo(expected,m);VxSIMDBboxTransformTo(&source,points,&m);for (int n=0;n<8;++n) EXPECT_EQ(points[n],expected[n]);
    VxBbox out=source;VxSIMDBboxTransformFrom(&out,&out,&m);VxBbox separate;separate.TransformFrom(source,m);EXPECT_EQ(out,separate);
}
TEST(BboxReferenceTest, DirectSimdViewpointKernelMatchesThePublicRelation) {
    const VxBbox box(1),enclosing(3);const VxVector points[]={VxVector(0,0,0),VxVector(2,0,0),VxVector(4,0,0)};const int expected[]={1,-1,1};
    for (int i=0;i<3;++i) EXPECT_EQ(VxSIMDBboxClassify(&box,&enclosing,&points[i]),expected[i]);
}
#endif
#if defined(_WIN32)
TEST(BboxReferenceTest, ExactBoxVectorMatrixVertexAndFlagFootprintsEndAtGuardPages) {
    SYSTEM_INFO system;GetSystemInfo(&system);const SIZE_T page=system.dwPageSize;
    XBYTE *memory=static_cast<XBYTE *>(VirtualAlloc(nullptr,10*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));ASSERT_NE(memory,nullptr);
    DWORD old=0;for (int n=1;n<10;n+=2) ASSERT_TRUE(VirtualProtect(memory+n*page,page,PAGE_NOACCESS,&old));
    VxBbox *box=new(memory+page-24) VxBbox(1);VxVector *point=new(memory+3*page-12) VxVector(-2,0,2);
    VxMatrix *matrix=new(memory+5*page-64) VxMatrix;matrix->SetIdentity();matrix->operator[](3)=VxVector(2,3,4);
    VxVector *corners=reinterpret_cast<VxVector *>(memory+7*page-96);for (int n=0;n<8;++n) new(corners+n) VxVector;
    XDWORD *flags=reinterpret_cast<XDWORD *>(memory+9*page-4);box->ClassifyVertices(1,reinterpret_cast<XBYTE *>(point),12,flags);EXPECT_EQ(*flags,33u);
    for (int axis=0;axis<3;++axis) { box->ClassifyVerticesOneAxis(1,reinterpret_cast<XBYTE *>(point),12,axis,flags);EXPECT_EQ(*flags,axis==0?1u:axis==1?0u:2u); }
    box->TransformTo(corners,*matrix);EXPECT_EQ(corners[0],VxVector(1,2,3));EXPECT_EQ(corners[7],VxVector(3,4,5));
    box->TransformFrom(*box,*matrix);EXPECT_EQ(box->Min,VxVector(1,2,3));EXPECT_EQ(box->Max,VxVector(3,4,5));
    VirtualFree(memory,0,MEM_RELEASE);
}
#endif
#endif
