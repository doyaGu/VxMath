#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cfenv>
#include <cfloat>
#include <type_traits>
#ifndef VX_VECTOR2D_NATIVE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#else
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/Vx2dVector.h"
#endif

struct Vector2DSamples { int samples=0, errors=0; unsigned digest=2166136261u; };

static void Observe2D(Vector2DSamples &r,float actual,double expected) {
    if (!std::isfinite(actual) || std::fabs(double(actual)-expected)>1e-6*(std::fabs(expected)+1e-30)) ++r.errors;
    unsigned bits; std::memcpy(&bits,&actual,sizeof(bits));
    for (int n=0;n<4;++n) { r.digest=(r.digest^(bits&255u))*16777619u; bits>>=8; }
}

static Vector2DSamples Vector2DArithmeticDataset() {
    Vector2DSamples r;
    for (int n=0;n<64;++n) for (int op=0;op<21;++op) {
        const float x=float(n%13+1)*0.25f*(n%2?-1:1),y=float(n%7+1)*0.5f*(n%3?-1:1);
        const Vx2DVector a(x,y),b(2.0f,-4.0f); Vx2DVector out;
        double ex=x,ey=y;
        if (op==0) out=+a;
        else if (op==1) { out=-a;ex=-x;ey=-y; }
        else if (op==2 || op==10) { if (op==2) out=a+b;else { out=a;out+=b; } ex=x+2;ey=y-4; }
        else if (op==3 || op==11) { if (op==3) out=a-b;else { out=a;out-=b; } ex=x-2;ey=y+4; }
        else if (op==4 || op==12) { if (op==4) out=a*b;else { out=a;out*=b; } ex=x*2;ey=y*-4; }
        else if (op==5 || op==13) { if (op==5) out=a/b;else { out=a;out/=b; } ex=x*0.5;ey=y*-0.25; }
        else if (op==6 || op==8 || op==14) {
            if (op==6) out=a*2;else if (op==8) out=2*a;else { out=a;out*=2; } ex=x*2;ey=y*2;
        } else if (op==7 || op==15) { if (op==7) out=a/2;else { out=a;out/=2; } ex=x*0.5;ey=y*0.5; }
        else if (op==9) { out=8/a;ex=8.0/x;ey=8.0/y; }
        else if (op==16) { out=a.Cross();ex=-y;ey=x; }
        else if (op==17) { out=a;out.Set(y,x);ex=y;ey=x; }
        else if (op==18) { out.Set(n-30,50-n);ex=n-30;ey=50-n; }
        else if (op==19) { const float data[]={y,x};out=Vx2DVector(data);ex=y;ey=x; }
        else if (op==20) { out=Vx2DVector(x);ex=x;ey=x; }
        Observe2D(r,out.x,ex);Observe2D(r,out.y,ey);
        Observe2D(r,a.Dot(b),double(x)*2-double(y)*4);
        Observe2D(r,a.SquareMagnitude(),double(x)*x+double(y)*y);
        Observe2D(r,a.Min(),x<y?x:y);Observe2D(r,a.Max(),x>y?x:y);
        ++r.samples;
    }
    return r;
}

static Vector2DSamples Vector2DLengthDataset() {
    Vector2DSamples r;
    const int triples[][3]={{3,4,5},{5,12,13},{8,15,17},{7,24,25}};
    const int exponents[]={-120,-100,-75,-50,-10,0,10,50,75,100,120,123};
    for (const auto &triple:triples) for (int exponent:exponents) for (int signs=0;signs<4;++signs) {
        const float scale=std::ldexp(1.0f,exponent);
        Vx2DVector v(triple[0]*scale*(signs&1?-1:1),triple[1]*scale*(signs&2?-1:1));
        const double x=v.x,y=v.y,m=std::sqrt(x*x+y*y);
        Observe2D(r,v.Magnitude(),m);v.Normalize();Observe2D(r,v.x,x/m);Observe2D(r,v.y,y/m);
        Observe2D(r,v.SquareMagnitude(),1);++r.samples;
    }
    return r;
}

#ifndef VX_VECTOR2D_NATIVE_PROBE
static void ExpectVector2D(const Vx2DVector &v,float x,float y) { EXPECT_FLOAT_EQ(v.x,x);EXPECT_FLOAT_EQ(v.y,y); }
static void RecordVector2D(const Vector2DSamples &r) {
    char digest[16];std::snprintf(digest,sizeof(digest),"%u",r.digest);
    EXPECT_EQ(r.errors,0);testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",digest);
}

TEST(Vector2DReferenceTest, LayoutAndIndexedReferencesShareTwoFloatStorage) {
    EXPECT_EQ(sizeof(Vx2DVector),8u);EXPECT_EQ(alignof(Vx2DVector),4u);
    EXPECT_EQ(offsetof(Vx2DVector,x),0u);EXPECT_EQ(offsetof(Vx2DVector,y),4u);EXPECT_EQ(offsetof(Vx2DVector,v),0u);
    EXPECT_TRUE(std::is_standard_layout<Vx2DVector>::value);EXPECT_TRUE(std::is_trivially_copyable<Vx2DVector>::value);
    EXPECT_TRUE(std::is_trivially_destructible<Vx2DVector>::value);
    Vx2DVector v(3,4);const Vx2DVector &read=v;
    EXPECT_EQ(&v[0],&v.x);EXPECT_EQ(&v[1],&v.y);EXPECT_EQ(&read[0],&v.x);EXPECT_EQ(&read[1],&v.y);
    v[0]=-2;v.v[1]=8;ExpectVector2D(read,-2,8);v.y=9;EXPECT_FLOAT_EQ(read.v[1],9);
}

TEST(Vector2DReferenceTest, ConstructorsInitializeZeroSplatFloatIntegerAndArrayInputs) {
    Vx2DVector zero,splat(1.25f),floats(-2.5f,3.75f),integers(-17,25);
    const float data[]={4.25f,-9.5f};Vx2DVector array(data);
    ExpectVector2D(zero,0,0);ExpectVector2D(splat,1.25f,1.25f);ExpectVector2D(floats,-2.5f,3.75f);
    ExpectVector2D(integers,-17,25);ExpectVector2D(array,4.25f,-9.5f);
}

TEST(Vector2DReferenceTest, ExistingImplicitScalarConstructionRemainsSourceCompatible) {
    EXPECT_TRUE((std::is_convertible<float,Vx2DVector>::value));
    Vx2DVector v=2.5f;ExpectVector2D(v,2.5f,2.5f);
    RecordProperty("implicit_scalar_conversion",int(std::is_convertible<float,Vx2DVector>::value));
}

TEST(Vector2DReferenceTest, SetOverloadsConvertIntegersAndAcceptSwappedOwnComponents) {
    Vx2DVector v;v.Set(-5,7);ExpectVector2D(v,-5,7);
    v.Set(1.25f,-2.5f);v.Set(v.y,v.x);ExpectVector2D(v,-2.5f,1.25f);
    v.Set(16777217,-16777217);ExpectVector2D(v,16777216.0f,-16777216.0f);
}

TEST(Vector2DReferenceTest, MagnitudeAndSquareMagnitudeFollowTheEuclideanDefinition) {
    const Vx2DVector v(-3,4),zero;
    EXPECT_FLOAT_EQ(v.SquareMagnitude(),25);EXPECT_FLOAT_EQ(v.Magnitude(),5);
    EXPECT_FLOAT_EQ(zero.Magnitude(),0);EXPECT_FLOAT_EQ(zero.SquareMagnitude(),0);
    EXPECT_FLOAT_EQ(Vx2DVector(-7,0).Magnitude(),7);
}

TEST(Vector2DReferenceTest, CrossRotatesCounterclockwiseAndDotIsAnInnerProduct) {
    const Vx2DVector v(3,4),p=v.Cross();ExpectVector2D(p,-4,3);ExpectVector2D(p.Cross(),-3,-4);
    EXPECT_FLOAT_EQ(v.Dot(p),0);EXPECT_FLOAT_EQ(v.Dot(Vx2DVector(2,-1)),2);
    EXPECT_FLOAT_EQ(p.Magnitude(),v.Magnitude());
}

TEST(Vector2DReferenceTest, NormalizeReturnsThisAndPreservesAnOrdinaryDirection) {
    Vx2DVector v(-3,4);EXPECT_EQ(&v.Normalize(),&v);ExpectVector2D(v,-0.6f,0.8f);
    EXPECT_NEAR(v.Magnitude(),1,1e-6);v.Normalize();ExpectVector2D(v,-0.6f,0.8f);
}

TEST(Vector2DReferenceTest, ZeroNormalizationPreservesSignedZeroWithoutInvalidOrDivision) {
    std::fenv_t saved;std::fegetenv(&saved);Vx2DVector zero(-0.0f,0.0f);
    std::feclearexcept(FE_ALL_EXCEPT);zero.Normalize();
    const int severe=std::fetestexcept(FE_INVALID|FE_DIVBYZERO);
    EXPECT_EQ(severe,0);EXPECT_TRUE(std::signbit(zero.x));EXPECT_FALSE(std::signbit(zero.y));
    ExpectVector2D(zero,0,0);std::fesetenv(&saved);
}

TEST(Vector2DReferenceTest, ComponentwiseMutationsSupportTheirOwnVectorAndReturnThis) {
    Vx2DVector v(3,4);EXPECT_EQ(&(v+=v),&v);ExpectVector2D(v,6,8);
    EXPECT_EQ(&(v*=v),&v);ExpectVector2D(v,36,64);
    EXPECT_EQ(&(v/=v),&v);ExpectVector2D(v,1,1);
    EXPECT_EQ(&(v-=v),&v);ExpectVector2D(v,0,0);
}

TEST(Vector2DReferenceTest, ScalarDivisionAndLeftDivisionHaveDifferentNumerators) {
    const Vx2DVector original(4,-8);ExpectVector2D(original/2,2,-4);ExpectVector2D(8/original,2,-1);
    ExpectVector2D(2*original,8,-16);ExpectVector2D(original*2,8,-16);
    Vx2DVector v=original;EXPECT_EQ(&(v*=2),&v);EXPECT_EQ(&(v/=2),&v);ExpectVector2D(v,4,-8);
}

TEST(Vector2DReferenceTest, UnaryCopyAndImplicitCopiesDoNotShareStorage) {
    Vx2DVector v(3,4),plus=+v,copy(v),assigned;assigned=v;plus.x=-7;copy.y=-8;assigned[0]=99;
    ExpectVector2D(v,3,4);ExpectVector2D(plus,-7,4);ExpectVector2D(copy,3,-8);ExpectVector2D(assigned,99,4);
    ExpectVector2D(-v,-3,-4);
}

TEST(Vector2DReferenceTest, RelationalOperatorsAreComponentwiseAndNotLexicographic) {
    const Vx2DVector a(1,9),b(2,8),equal(1,9),higher(2,10);
    EXPECT_FALSE(a<b);EXPECT_FALSE(b<a);EXPECT_FALSE(a<=b);EXPECT_FALSE(b<=a);
    EXPECT_TRUE(a<higher);EXPECT_TRUE(a<=equal);EXPECT_FALSE(a<equal);
}

TEST(Vector2DReferenceTest, EqualityIsNumericIncludingSignedZeroAndUnorderedValues) {
    EXPECT_TRUE(Vx2DVector(-0.0f,0.0f)==Vx2DVector(0.0f,-0.0f));
    EXPECT_FALSE(Vx2DVector(1,2)==Vx2DVector(1,3));EXPECT_TRUE(Vx2DVector(1,2)!=Vx2DVector(1,3));
    const Vx2DVector v(std::nanf(""),1.0f);
    EXPECT_FALSE(v==v);EXPECT_TRUE(v!=v);EXPECT_FALSE(v<v);EXPECT_FALSE(v<=v);
}

TEST(Vector2DReferenceTest, MinAndMaxSelectTheSecondComponentOnEqualOrUnorderedComparisons) {
    EXPECT_FLOAT_EQ(Vx2DVector(-3,4).Min(),-3);EXPECT_FLOAT_EQ(Vx2DVector(-3,4).Max(),4);
    const Vx2DVector zeros(0.0f,-0.0f);EXPECT_TRUE(std::signbit(zeros.Min()));EXPECT_TRUE(std::signbit(zeros.Max()));
    const float nan=std::nanf("");EXPECT_FLOAT_EQ(Vx2DVector(nan,7.0f).Min(),7);EXPECT_FLOAT_EQ(Vx2DVector(nan,7.0f).Max(),7);
    EXPECT_TRUE(std::isnan(Vx2DVector(7.0f,nan).Min()));EXPECT_TRUE(std::isnan(Vx2DVector(7.0f,nan).Max()));
}

TEST(Vector2DReferenceTest, ArithmeticDatasetMatchesTheOriginalSdkAndIndependentExpectations) {
    const Vector2DSamples r=Vector2DArithmeticDataset();EXPECT_EQ(r.samples,1344);RecordVector2D(r);
}

TEST(Vector2DReferenceTest, LargeFiniteMagnitudeDoesNotOverflowAnIntermediateSquare) {
    const Vx2DVector v(3e20f,4e20f);const double expected=std::sqrt(double(v.x)*v.x+double(v.y)*v.y);
    EXPECT_TRUE(std::isfinite(v.Magnitude()));EXPECT_NEAR(v.Magnitude(),expected,expected*1e-6);
    EXPECT_TRUE(std::isinf(v.SquareMagnitude()));
}

TEST(Vector2DReferenceTest, TinyFiniteMagnitudeDoesNotUnderflowAnIntermediateSquare) {
    const Vx2DVector v(3e-30f,4e-30f);const double expected=std::sqrt(double(v.x)*v.x+double(v.y)*v.y);
    EXPECT_GT(v.Magnitude(),0);EXPECT_NEAR(v.Magnitude(),expected,expected*1e-6);
    EXPECT_FLOAT_EQ(v.SquareMagnitude(),0);
}

TEST(Vector2DReferenceTest, LargeFiniteVectorNormalizesWithoutLosingItsDirection) {
    Vx2DVector v(3e20f,4e20f);v.Normalize();ExpectVector2D(v,0.6f,0.8f);
}

TEST(Vector2DReferenceTest, TinyFiniteVectorNormalizesInsteadOfRemainingUnchanged) {
    Vx2DVector v(3e-30f,4e-30f);v.Normalize();ExpectVector2D(v,0.6f,0.8f);
}

TEST(Vector2DReferenceTest, DirectionRemainsRepresentableWhenMagnitudeExceedsFloatRange) {
    Vx2DVector v(FLT_MAX,FLT_MAX);EXPECT_TRUE(std::isinf(v.Magnitude()));v.Normalize();
    const float component=float(std::sqrt(0.5));ExpectVector2D(v,component,component);
}

TEST(Vector2DReferenceTest, LengthDatasetKeepsPythagoreanDirectionsAcrossFloatScales) {
    const Vector2DSamples r=Vector2DLengthDataset();EXPECT_EQ(r.samples,192);RecordVector2D(r);
}
#endif
