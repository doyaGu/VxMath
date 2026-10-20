#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "VxMath.h"
#include "VxSIMDMode.h"

static const int DXTModes[]={VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_SSSE3,VX_SIMD_MODE_AVX2};

static void DescribeDXTPayload(VxImageDescEx &desc,int format,int width,int height,int size,XBYTE *image) {
    desc.Flags=static_cast<XDWORD>(format);desc.Width=width;desc.Height=height;
    desc.TotalImageSize=size;desc.Image=image;
}

struct DXTPayloadImage {
    XBYTE source[4096],target[4096];
    VxImageDescEx src,dst;
    DXTPayloadImage(int format,int width=8,int height=8,int size=0,int capacity=0,int sourceOffset=0,int targetOffset=0) {
        int bytes=size?size:((width+3)/4)*((height+3)/4)*(format==_DXT1?8:16);
        for(int i=0;i<4096;++i)source[i]=static_cast<XBYTE>(i*73+i/17+0x53);
        std::memset(target,0xa5,sizeof(target));
        DescribeDXTPayload(src,format,width,height,bytes,source+16+sourceOffset);
        DescribeDXTPayload(dst,format,width,height,capacity?capacity:bytes,target+16+targetOffset);
    }
    void Run(bool flip=false,bool copy=true) {
        XBYTE expected[4096],beforeSource[4096];
        std::memcpy(expected,target,sizeof(target));std::memcpy(beforeSource,source,sizeof(source));
        if(copy)std::memcpy(expected+(dst.Image-target),src.Image,src.TotalImageSize);
        VxImageDescEx beforeSrc=src,beforeDst=dst;
        if(flip)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        ASSERT_EQ(std::memcmp(target,expected,sizeof(target)),0)<<"format="<<src.Flags<<" height="<<src.Height<<" size="<<src.TotalImageSize;
        EXPECT_EQ(std::memcmp(source,beforeSource,sizeof(source)),0);
        EXPECT_EQ(std::memcmp(&src,&beforeSrc,sizeof(src)),0);EXPECT_EQ(std::memcmp(&dst,&beforeDst,sizeof(dst)),0);
    }
};

class DXTPayloadReferenceTest:public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(DXTModes[GetParam()]));ASSERT_EQ(VxGetSIMDEffectiveBackend(),DXTModes[GetParam()]);}
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

TEST_P(DXTPayloadReferenceTest, AllFiveFormatsCopyOnePayloadWithIndependentOffsets) {
    const int shapes[][2]={{1,1},{4,1},{4,4},{8,8},{3,5},{17,9}},offsets[]={0,1,3,7,15,16,31};
    for(int format=_DXT1;format<=_DXT5;++format)for(int shape=0;shape<6;++shape)for(int offset=0;offset<7;++offset) {
        DXTPayloadImage image(format,shapes[shape][0],shapes[shape][1],0,0,offsets[offset],offsets[(offset*3)%7]);image.Run();
        VxImageDescEx same=image.src;XBYTE before[4096];std::memcpy(before,image.source,sizeof(before));
        VxDoBlit(same,same);EXPECT_EQ(std::memcmp(before,image.source,sizeof(before)),0);
    }
}

TEST_P(DXTPayloadReferenceTest, DeclaredOpaqueLengthIncludesByteTailsAndPreservesSpareCapacity) {
    const int sizes[]={1,3,7,8,9,15,16,17,31,32,33,63,64,65,127},depths[]={0,4,8,16,32};
    for(int format=_DXT1;format<=_DXT5;++format)for(int n=0;n<15;++n)for(int b=0;b<5;++b) {
        // No compressed-data decoding: the declared payload is copied verbatim.
        DXTPayloadImage image(format,8,8,sizes[n],sizes[n]+7,1,3);
        image.src.BitsPerPixel=image.dst.BitsPerPixel=depths[b];image.Run();
    }
}

TEST_P(DXTPayloadReferenceTest, SingleRowUpsideDownIsAnIdenticalPayloadCopy) {
    const int widths[]={1,3,4,5,8,17};
    for(int format=_DXT1;format<=_DXT5;++format)for(int w=0;w<6;++w) {
        DXTPayloadImage image(format,widths[w],1);image.Run(true);
        VxImageDescEx same=image.src;XBYTE before[4096];std::memcpy(before,image.source,sizeof(before));
        VxDoBlitUpsideDown(same,same);EXPECT_EQ(std::memcmp(before,image.source,sizeof(before)),0);
    }
}

TEST_P(DXTPayloadReferenceTest, MultirowCompressedVerticalFlipRemainsUnsupported) {
    const int heights[]={2,3,4,5,8,9};
    for(int format=_DXT1;format<=_DXT5;++format)for(int h=0;h<6;++h) {
        DXTPayloadImage image(format,8,heights[h]);image.Run(true,false);
    }
}

TEST_P(DXTPayloadReferenceTest, ShortCapacityAndNonpositiveLengthsDoNotPartiallyWrite) {
    for(int format=_DXT1;format<=_DXT5;++format)for(int flip=0;flip<2;++flip) {
        DXTPayloadImage shortTarget(format,4,1,16,15);shortTarget.Run(flip!=0,false);
        DXTPayloadImage emptySource(format,4,1);emptySource.src.TotalImageSize=0;emptySource.Run(flip!=0,false);
        DXTPayloadImage negativeSource(format,4,1);negativeSource.src.TotalImageSize=-1;negativeSource.Run(flip!=0,false);
        DXTPayloadImage emptyTarget(format,4,1);emptyTarget.dst.TotalImageSize=0;emptyTarget.Run(flip!=0,false);
        DXTPayloadImage negativeTarget(format,4,1);negativeTarget.dst.TotalImageSize=-1;negativeTarget.Run(flip!=0,false);
    }
}

TEST_P(DXTPayloadReferenceTest, ConcreteColorMasksRetainPriorityOverStaleDXTFlags) {
    for(int format=_DXT1;format<=_DXT5;++format)for(int flip=0;flip<2;++flip) {
        DXTPayloadImage image(format,4,2,23,25);
        image.src.BitsPerPixel=image.dst.BitsPerPixel=32;
        image.src.RedMask=image.dst.RedMask=0xff0000;image.src.GreenMask=image.dst.GreenMask=0xff00;
        image.src.BlueMask=image.dst.BlueMask=0xff;image.src.AlphaMask=image.dst.AlphaMask=0xff000000;
        XBYTE expected[4096];std::memcpy(expected,image.target,sizeof(expected));
        for(int y=0;y<2;++y)std::memcpy(expected+(image.dst.Image-image.target)+y*25,image.src.Image+(flip?1-y:y)*23,16);
        if(flip)VxDoBlitUpsideDown(image.src,image.dst);else VxDoBlit(image.src,image.dst);
        EXPECT_EQ(std::memcmp(expected,image.target,sizeof(expected)),0);
    }
}

TEST_P(DXTPayloadReferenceTest, CrossFormatAndResizeDoNotDecodeOrReinterpretPayload) {
    for(int format=_DXT1;format<=_DXT5;++format) {
        for(int target=_DXT1;target<=_DXT5;++target)if(target!=format) {
            DXTPayloadImage image(format);image.dst.Flags=static_cast<XDWORD>(target);image.Run(false,false);image.Run(true,false);
        }
        const int depths[]={0,32};
        for(int b=0;b<2;++b) {
            DXTPayloadImage width(format);width.src.BitsPerPixel=width.dst.BitsPerPixel=depths[b];width.dst.Width=4;width.Run(false,false);width.Run(true,false);
            DXTPayloadImage height(format);height.src.BitsPerPixel=height.dst.BitsPerPixel=depths[b];height.dst.Height=4;height.Run(false,false);height.Run(true,false);
        }
    }
}

static XBYTE *DXTProtectedStorage(size_t &page) {
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;
    XBYTE *p=static_cast<XBYTE *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));DWORD old;
    if(!p||!VirtualProtect(p+page,page,PAGE_NOACCESS,&old))std::_Exit(3);
#else
    page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *p=static_cast<XBYTE *>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(p==MAP_FAILED||mprotect(p+page,page,PROT_NONE))std::_Exit(3);
#endif
    return p;
}

static void DXTProtectedPayloads(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    if(!VxSetSIMDOverride(mode)||VxGetSIMDEffectiveBackend()!=mode)std::_Exit(4);
    size_t sp,dp;XBYTE *source=DXTProtectedStorage(sp),*target=DXTProtectedStorage(dp);
    const int sizes[]={1,3,7,8,9,15,16,17,31,32,33,63,64,65,127};
    for(int format=_DXT1;format<=_DXT5;++format)for(int n=0;n<15;++n)for(int flip=0;flip<2;++flip) {
        int bytes=sizes[n];XBYTE *s=source+sp-bytes,*d=target+dp-bytes;
        for(int i=0;i<bytes;++i)s[i]=static_cast<XBYTE>(i*73+0x53);
        std::memset(d-16,0xa5,bytes+16);VxImageDescEx src,dst;
        DescribeDXTPayload(src,format,8,flip?1:8,bytes,s);DescribeDXTPayload(dst,format,8,flip?1:8,bytes,d);
        if(flip)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        if(std::memcmp(s,d,bytes))std::_Exit(2);
        for(int i=1;i<=16;++i)if(d[-i]!=0xa5)std::_Exit(5);
    }
    // Rejected multirow flips and short targets must not read the source at all.
    for(int format=_DXT1;format<=_DXT5;++format) {
        VxImageDescEx src,dst;XBYTE *d=target+dp-16;std::memset(d,0xa5,16);
        DescribeDXTPayload(src,format,4,4,16,source+sp);DescribeDXTPayload(dst,format,4,4,16,d);
        VxDoBlitUpsideDown(src,dst);src.Height=dst.Height=1;dst.TotalImageSize=15;
        VxDoBlit(src,dst);VxDoBlitUpsideDown(src,dst);
        for(int i=0;i<16;++i)if(d[i]!=0xa5)std::_Exit(6);
    }
    std::_Exit(0);
}

TEST_P(DXTPayloadReferenceTest, ExactPayloadBuffersEndAtProtectedPages) {
    EXPECT_EXIT(DXTProtectedPayloads(DXTModes[GetParam()]),testing::ExitedWithCode(0),"");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2,DXTPayloadReferenceTest,testing::Range(0,4));
