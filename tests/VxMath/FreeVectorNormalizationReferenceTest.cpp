#ifndef VX_FREE_NORMALIZE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#define VX_VECTOR_NORMALIZE_PROBE 1
#include "VectorNormalizationReferenceTest.cpp"

typedef void(*FNFunction)(const VxVector *, VxVector *);
static int FNInputErrors;
static void FNReference(const VxVector *source, VxVector *result) { *result = Normalize(*source); }
static void FNPointer(const VxVector *source, VxVector *result) { *result = Normalize(source); }
#if defined(VX_SIMD_SSE)
static void FNRegister(const VxVector *source, VxVector *result) {
    VxSIMDStoreFloat3(&result->x, VxSIMDNormalize3(VxSIMDLoadFloat3(&source->x)));
}
#endif
static void FNInPlace(FNFunction function, VxVector *v) {
    unsigned char before[12]; memcpy(before, v, 12);
    VxVector result; function(v, &result);
    FNInputErrors += memcmp(v, before, 12) != 0; memcpy(v, &result, 12);
}
static void FNReferenceInPlace(VxVector *v) { FNInPlace(FNReference, v); }
static void FNPointerInPlace(VxVector *v) { FNInPlace(FNPointer, v); }
#if defined(VX_SIMD_SSE)
static void FNRegisterInPlace(VxVector *v) { FNInPlace(FNRegister, v); }
#endif
static VNResult FNZero(VNFn function) {
    VNResult r;
    for (int mask = 0; mask < 8; ++mask) {
        VxVector v; for (int k = 0; k < 3; ++k) (&v.x)[k] = VNFloat(mask & (1 << k) ? 0x80000000u : 0);
        bool fault = false; VNCall(function, v, fault); ++r.calls; r.faults += fault;
        r.errors += fault || !(v.x != v.x && v.y != v.y && v.z != v.z);
    }
    return r;
}
static VNResult FNNonfinite(VNFn function) {
    VNResult r; const float values[] = {VNFloat(0x7fc12345u), HUGE_VALF, -HUGE_VALF};
    for (int field = 0; field < 3; ++field) for (float value : values) for (int mixed = 0; mixed < 2; ++mixed) {
        float p[] = {-2, 3, -0.0f}; p[field] = value; if (mixed) p[(field + 1) % 3] = HUGE_VALF;
        VxVector v; memcpy(&v, p, 12); bool fault = false; VNCall(function, v, fault);
        ++r.calls; r.faults += fault; const float actual[] = {v.x, v.y, v.z}; bool valid = !fault;
        for (int k = 0; k < 3; ++k) {
            if (value != value || fabs(p[k]) == HUGE_VALF) valid = valid && actual[k] != actual[k];
            else valid = valid && VNBits(actual[k]) == (VNBits(p[k]) & 0x80000000u);
        }
        r.errors += !valid;
    }
    return r;
}
static bool FNCall(FNFunction function, const VxVector *source, VxVector *result) {
#if defined(_WIN32)
    __try { function(source, result); return true; } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    function(source, result); return true;
#endif
}
static VNResult FNReadOnly(FNFunction function) {
    VNResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 6 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    if (!memory) { ++r.errors; return r; }
    for (int sourceEnd = 0; sourceEnd < 2; ++sourceEnd) for (int destinationEnd = 0; destinationEnd < 2; ++destinationEnd) for (int shape = 0; shape < 8; ++shape) {
        DWORD old; bool writable = VirtualProtect(memory + page, page, PAGE_READWRITE, &old) && VirtualProtect(memory + 4 * page, page, PAGE_READWRITE, &old);
        if (!writable) { ++r.errors; break; }
        memset(memory + page, 0x6d, page); memset(memory + 4 * page, 0x6d, page);
        const size_t sourceOffset = sourceEnd ? page - 12 : 0, destinationOffset = destinationEnd ? page - 12 : 0;
        VxVector *source = ::new(memory + page + sourceOffset) VxVector;
        VxVector *result = ::new(memory + 4 * page + destinationOffset) VxVector;
        float p[3]; VNCase(shape, p); memcpy(source, p, 12);
        if (!VirtualProtect(memory + page, page, PAGE_READONLY, &old)) { ++r.errors; break; }
        const bool called = FNCall(function, source, result); ++r.calls; r.faults += !called;
        bool valid = called && VNCorrect(p, *result) && memcmp(source, p, 12) == 0;
        for (size_t k = 0; k < page; ++k) {
            if (k < sourceOffset || k >= sourceOffset + 12) valid = valid && memory[page + k] == 0x6d;
            if (k < destinationOffset || k >= destinationOffset + 12) valid = valid && memory[4 * page + k] == 0x6d;
        }
        r.errors += !valid;
    }
    VirtualFree(memory, 0, MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_FREE_NORMALIZE_PROBE
class FreeVectorNormalizationReference : public testing::TestWithParam<int> {
protected:
    void SetUp() override { FNInputErrors = 0; }
    VNFn Function() {
#if defined(VX_SIMD_SSE)
        if (GetParam() == 2) return FNRegisterInPlace;
#endif
        return GetParam() ? FNPointerInPlace : FNReferenceInPlace;
    }
    FNFunction ReturnedFunction() {
#if defined(VX_SIMD_SSE)
        if (GetParam() == 2) return FNRegister;
#endif
        return GetParam() ? FNPointer : FNReference;
    }
    void Check(VNResult r, int count) { EXPECT_EQ(r.calls, count); EXPECT_EQ(r.errors, 0); EXPECT_EQ(r.faults, 0); EXPECT_EQ(FNInputErrors, 0); }
};
TEST_P(FreeVectorNormalizationReference,AnalyticDirections) { Check(VNAnalytic(Function()),5); }
TEST_P(FreeVectorNormalizationReference,EveryFiniteBinaryExponent) { Check(VNPowers(Function()),2216); }
TEST_P(FreeVectorNormalizationReference,MixedMagnitudeFiniteInputs) { Check(VNRandoms(Function()),2048); }
TEST_P(FreeVectorNormalizationReference,FormerEpsilonIsNotACutoff) { Check(VNThresholds(Function()),24); }
TEST_P(FreeVectorNormalizationReference,FreeZeroIsNaNAndCheckedRegisterZeroIsPreserved) { Check(GetParam()==2?VNZero(Function()):FNZero(Function()),8); }
TEST_P(FreeVectorNormalizationReference,NonfinitePolicy) { Check(GetParam()==2?VNNonfinite(Function()):FNNonfinite(Function()),18); }
TEST_P(FreeVectorNormalizationReference,DerivedSourceDoesNotReadOrChangeW) { Check(VNDerived(Function()),32); }
TEST_P(FreeVectorNormalizationReference,FourByteAlignedSurroundings) { Check(VNSurroundings(Function()),32); }
TEST_P(FreeVectorNormalizationReference,ExactInPlaceTwelveByteBoundary) { Check(VNFootprint(Function()),40); }
TEST_P(FreeVectorNormalizationReference,ReadOnlySourceAndSeparateDestinationExactBoundaries) { Check(FNReadOnly(ReturnedFunction()),32); }
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ReferencePointerRegister,FreeVectorNormalizationReference,testing::Values(0,1,2));
TEST(FreeNormalizeRegister,XYZLengthIgnoresWButScalesAllFourLanes) {
    const float w[] = {0,-0.0f,2,FLT_MAX,VNFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};
    for (int shape = 0; shape < 8; ++shape) for (float fourth : w) {
        float p[4]; VNCase(shape,p); p[3]=fourth; float out[4]; _mm_storeu_ps(out,VxSIMDNormalize3(_mm_loadu_ps(p)));
        VxVector v; memcpy(&v,out,12); EXPECT_TRUE(VNCorrect(p,v));
        double scale=0; for(int k=0;k<3;++k) if(fabs(p[k])>scale)scale=fabs(p[k]);
        double square=0; for(int k=0;k<3;++k){double value=p[k]/scale;square+=value*value;}
        const float expected=static_cast<float>((double(fourth)/scale)/sqrt(square));
        if(expected!=expected) EXPECT_TRUE(out[3]!=out[3]);
        else if(fabs(expected)==HUGE_VALF) EXPECT_EQ(VNBits(out[3]),VNBits(expected));
        else {const unsigned actual=VNBits(out[3]),reference=VNBits(expected); EXPECT_EQ(actual&0x80000000u,reference&0x80000000u); EXPECT_LE(actual>reference?actual-reference:reference-actual,2u);}
    }
}
TEST(FreeNormalizeRegister,ZeroXYZPreservesAllLanes) {
    const unsigned w[]={0,0x80000000u,0x3f800000u,0x7fc12345u,0x7f800000u};
    for(int mask=0;mask<8;++mask)for(unsigned fourth:w){unsigned p[4];for(int k=0;k<3;++k)p[k]=mask&(1<<k)?0x80000000u:0;p[3]=fourth;unsigned out[4];_mm_storeu_ps(reinterpret_cast<float *>(out),VxSIMDNormalize3(_mm_loadu_ps(reinterpret_cast<const float *>(p))));EXPECT_EQ(memcmp(out,p,16),0);}
}
TEST(FreeNormalizeRegister,UnorderedXYZPreservesAllLanes) {
    for(int k=0;k<3;++k){float p[]={1,-2,3,-0.0f};p[k]=VNFloat(0x7fc12345u);float out[4];_mm_storeu_ps(out,VxSIMDNormalize3(_mm_loadu_ps(p)));EXPECT_EQ(memcmp(out,p,16),0);}
}
#else
INSTANTIATE_TEST_SUITE_P(ReferencePointer,FreeVectorNormalizationReference,testing::Values(0,1));
#endif
#endif
