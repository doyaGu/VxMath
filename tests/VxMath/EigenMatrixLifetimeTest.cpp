#include "VxMath.h"
#include "VxEigenMatrix.h"
#include <gtest/gtest.h>
#include <math.h>
#include <new>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// These are current private-layout invariants, not declarations of the original
// DLL's class layout. The latter is exercised with a separate 36-byte adapter.
static_assert(sizeof(VxEigenMatrix)==64,"private adapter retains VxMatrix storage");
static_assert(alignof(VxEigenMatrix)==4,"private adapter does not require SIMD alignment");
static_assert(__is_standard_layout(VxEigenMatrix),"base view has no extra layout state");
static_assert(!__is_polymorphic(VxEigenMatrix),"no virtual dispatch state");
static_assert(__is_trivially_copyable(VxEigenMatrix),"copy transports the object representation");

enum ELAction { ELDefault,ELValue,ELCopy,ELMove,ELAssign,ELMoveAssign,ELDestroy };
#if defined(_MSC_VER)
#define EL_NOINLINE __declspec(noinline)
#else
#define EL_NOINLINE __attribute__((noinline))
#endif
static EL_NOINLINE VxEigenMatrix *ELPerform(void *storage,VxEigenMatrix *source,ELAction action,bool &fault) {
    VxEigenMatrix *result=static_cast<VxEigenMatrix *>(storage);
#if defined(_WIN32)
    __try {
#endif
        switch(action) {
            case ELDefault:return ::new(storage)VxEigenMatrix;
            case ELValue:return ::new(storage)VxEigenMatrix();
            case ELCopy:return ::new(storage)VxEigenMatrix(*source);
            case ELMove:return ::new(storage)VxEigenMatrix(static_cast<VxEigenMatrix &&>(*source));
            case ELAssign:*result=*source;break;
            case ELMoveAssign:*result=static_cast<VxEigenMatrix &&>(*source);break;
            case ELDestroy:result->~VxEigenMatrix();break;
        }
#if defined(_WIN32)
    } __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;return nullptr;}
#else
    (void)fault;
#endif
    return result;
}
static bool ELZero(const void *storage) {
    const unsigned char *bytes=static_cast<const unsigned char *>(storage);
    for(int i=0;i<64;++i)if(bytes[i]!=0)return false;
    return true;
}

static void ELConstruct(ELAction action) {
    const unsigned char patterns[]={0,0xa5,0xff,0x3f};
    for(unsigned char pattern:patterns)for(int offset=0;offset<16;offset+=4) {
        alignas(16) unsigned char storage[128],before[128];memset(storage,0x6d,sizeof(storage));
        unsigned char *target=storage+16+offset;memset(target,pattern,64);memcpy(before,storage,sizeof(storage));
        bool fault=false;VxEigenMatrix *object=ELPerform(target,nullptr,action,fault);
        ASSERT_FALSE(fault);ASSERT_EQ(object,reinterpret_cast<VxEigenMatrix *>(target));EXPECT_TRUE(ELZero(object));
        for(int i=0;i<128;++i)if(i<16+offset||i>=80+offset)EXPECT_EQ(storage[i],before[i]);
        EXPECT_EQ(static_cast<void *>(static_cast<VxMatrix *>(object)),static_cast<void *>(target));
        ELPerform(object,nullptr,ELDestroy,fault);EXPECT_FALSE(fault);
    }
}
TEST(EigenMatrixLifetime,RetainPrivateBaseViewOffsetsAndArrayStride) {
    VxEigenMatrix m;const unsigned char *bytes=reinterpret_cast<const unsigned char *>(&m);
    VxMatrix &base=m;EXPECT_EQ(static_cast<const void *>(&base),static_cast<const void *>(&m));
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)EXPECT_EQ(reinterpret_cast<const unsigned char *>(&m[r][c])-bytes,r*16+c*4);
    VxEigenMatrix array[3];EXPECT_EQ(sizeof(array),192u);
    EXPECT_EQ(reinterpret_cast<const unsigned char *>(&array[1])-reinterpret_cast<const unsigned char *>(&array[0]),64);
    EXPECT_EQ(reinterpret_cast<const unsigned char *>(&array[2])-reinterpret_cast<const unsigned char *>(&array[1]),64);
}
TEST(EigenMatrixLifetime,DefaultConstructionZerosAllStorageAtFourByteAlignments) {ELConstruct(ELDefault);}
TEST(EigenMatrixLifetime,ValueConstructionZerosAllStorageAtFourByteAlignments) {ELConstruct(ELValue);}
TEST(EigenMatrixLifetime,DefaultArrayConstructionZerosEveryElement) {
    alignas(16) unsigned char storage[3*64+32];memset(storage,0xa5,sizeof(storage));
    // Explicit placement avoids array-allocation cookies and tests precisely
    // the per-element construction used by an ordinary default-initialized array.
    for(int i=0;i<3;++i)::new(storage+16+i*64)VxEigenMatrix;
    for(int i=0;i<3;++i)EXPECT_TRUE(ELZero(storage+16+i*64));
    VxEigenMatrix array[3];for(const VxEigenMatrix &m:array)EXPECT_TRUE(ELZero(&m));
    for(int i=0;i<16;++i){EXPECT_EQ(storage[i],0xa5);EXPECT_EQ(storage[3*64+16+i],0xa5);}
    for(int i=0;i<3;++i)reinterpret_cast<VxEigenMatrix *>(storage+16+i*64)->~VxEigenMatrix();
}

#if defined(_WIN32)

TEST(EigenMatrixLifetime,PrivateAdapterDoesNotClaimOriginalEigenEntryPointABI) {
    const int previous=VxGetSIMDOverride();(void)previous;
    HMODULE module=GetModuleHandleW(L"VxMath.dll");ASSERT_NE(module,nullptr);
    const char *names[]={"?Covariance@VxEigenMatrix@@QAEXPBEKH@Z","?Tridiagonal@VxEigenMatrix@@QAEXQAM0@Z","?QLAlgorithm@VxEigenMatrix@@QAE_NQAM0@Z","?EigenStuff3@VxEigenMatrix@@QAEXXZ","??0VxEigenMatrix@@QAE@XZ","??1VxEigenMatrix@@QAE@XZ"};
    for(const char *name:names)EXPECT_EQ(GetProcAddress(module,name),nullptr)<<name;
}
#endif
