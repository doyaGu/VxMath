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
static void ELBits(VxEigenMatrix &m,int seed) {
    const unsigned values[]={0,0x80000000u,0x7fc12345u,0x7f800000u,0xff800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0x00800000u,1,0xffffffffu,0x3eaaaaabu,0xa5a5a5a5u,0x00012345u,0x80800000u,0x41200000u};
    unsigned words[16];for(int i=0;i<16;++i)words[i]=values[(i+seed)%16];
    memcpy(&m,words,sizeof(words));
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
TEST(EigenMatrixLifetime,CopyAndMoveConstructionTransportEveryFieldBit) {
    for(int seed=0;seed<16;++seed)for(int op=ELCopy;op<=ELMove;++op) {
        VxEigenMatrix source;ELBits(source,seed);unsigned char saved[64];memcpy(saved,&source,64);
        alignas(16) unsigned char storage[64];memset(storage,0x3c,64);bool fault=false;
        VxEigenMatrix *copy=ELPerform(storage,&source,static_cast<ELAction>(op),fault);
        ASSERT_FALSE(fault);ASSERT_NE(copy,nullptr);EXPECT_EQ(memcmp(copy,saved,64),0);EXPECT_EQ(memcmp(&source,saved,64),0);
        ELPerform(copy,nullptr,ELDestroy,fault);EXPECT_FALSE(fault);
    }
}
TEST(EigenMatrixLifetime,AssignmentAndSelfAssignmentTransportEveryFieldBit) {
    for(int seed=0;seed<16;++seed)for(int op=ELAssign;op<=ELMoveAssign;++op) {
        VxEigenMatrix source,destination;ELBits(source,seed);unsigned char saved[64];memcpy(saved,&source,64);bool fault=false;
        EXPECT_EQ(ELPerform(&destination,&source,static_cast<ELAction>(op),fault),&destination);ASSERT_FALSE(fault);
        EXPECT_EQ(memcmp(&destination,saved,64),0);EXPECT_EQ(memcmp(&source,saved,64),0);
        ELPerform(&destination,&destination,static_cast<ELAction>(op),fault);EXPECT_FALSE(fault);EXPECT_EQ(memcmp(&destination,saved,64),0);
    }
}
TEST(EigenMatrixLifetime,DestructionLeavesBackingStorageUnchanged) {
    for(int seed=0;seed<16;++seed) {
        alignas(16) unsigned char storage[64];VxEigenMatrix *m=::new(storage)VxEigenMatrix;ELBits(*m,seed);unsigned char saved[64];memcpy(saved,storage,64);bool fault=false;
        EXPECT_EQ(ELPerform(m,nullptr,ELDestroy,fault),m);EXPECT_FALSE(fault);EXPECT_EQ(memcmp(storage,saved,64),0);
    }
}

#if defined(_WIN32)
TEST(EigenMatrixLifetime,GuardedConstructCopyAssignAndDestroyRespectExactStorage) {
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;
    unsigned char *source=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));ASSERT_NE(source,nullptr);
    unsigned char *destination=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));
    if(!destination){VirtualFree(source,0,MEM_RELEASE);FAIL()<<"destination allocation failed";}
    for(int edge=0;edge<4;++edge)for(int action=ELDefault;action<=ELMoveAssign;++action) {
        DWORD old;ASSERT_TRUE(VirtualProtect(source+page,page,PAGE_READWRITE,&old));ASSERT_TRUE(VirtualProtect(destination+page,page,PAGE_READWRITE,&old));
        VxEigenMatrix *src=::new(source+page+((edge&1)?page-64:0))VxEigenMatrix;ELBits(*src,edge+action);unsigned char saved[64];memcpy(saved,src,64);
        unsigned char *dst=destination+page+((edge&2)?page-64:0);memset(dst,0xa5,64);
        if(action>=ELAssign)::new(dst)VxEigenMatrix;
        ASSERT_TRUE(VirtualProtect(source+page,page,PAGE_READONLY,&old));bool fault=false;
        VxEigenMatrix *result=ELPerform(dst,src,static_cast<ELAction>(action),fault);EXPECT_FALSE(fault);ASSERT_EQ(result,reinterpret_cast<VxEigenMatrix *>(dst));
        if(action<=ELValue)EXPECT_TRUE(ELZero(dst));else EXPECT_EQ(memcmp(dst,saved,64),0);
        EXPECT_EQ(memcmp(src,saved,64),0);
        ASSERT_TRUE(VirtualProtect(destination+page,page,PAGE_NOACCESS,&old));ELPerform(result,nullptr,ELDestroy,fault);EXPECT_FALSE(fault);
        ASSERT_TRUE(VirtualProtect(source+page,page,PAGE_NOACCESS,&old));ELPerform(src,nullptr,ELDestroy,fault);EXPECT_FALSE(fault);
    }
    EXPECT_TRUE(VirtualFree(source,0,MEM_RELEASE));EXPECT_TRUE(VirtualFree(destination,0,MEM_RELEASE));
}
TEST(EigenMatrixLifetime,PrivateAdapterDoesNotClaimOriginalEigenEntryPointABI) {
    const int previous=VxGetSIMDOverride();(void)previous;
    HMODULE module=GetModuleHandleW(L"VxMath.dll");ASSERT_NE(module,nullptr);
    const char *names[]={"?Covariance@VxEigenMatrix@@QAEXPBEKH@Z","?Tridiagonal@VxEigenMatrix@@QAEXQAM0@Z","?QLAlgorithm@VxEigenMatrix@@QAE_NQAM0@Z","?EigenStuff3@VxEigenMatrix@@QAEXXZ","??0VxEigenMatrix@@QAE@XZ","??1VxEigenMatrix@@QAE@XZ"};
    for(const char *name:names)EXPECT_EQ(GetProcAddress(module,name),nullptr)<<name;
}
#endif
