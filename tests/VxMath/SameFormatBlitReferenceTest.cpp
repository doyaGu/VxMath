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

struct SameFormatLayout {
    int format, bytes, detected;
    XDWORD red, green, blue, alpha;
};

// Literal original descriptor fields; format27 aliases ordinary BGR32 (format13).
static const SameFormatLayout SameLayouts[] = {
    {1,4,1,0xff0000,0xff00,0xff,0xff000000},
    {2,4,2,0xff0000,0xff00,0xff,0},
    {3,3,3,0xff0000,0xff00,0xff,0},
    {4,2,4,0xf800,0x7e0,0x1f,0},
    {5,2,5,0x7c00,0x3e0,0x1f,0},
    {6,2,6,0x7c00,0x3e0,0x1f,0x8000},
    {7,2,7,0xf00,0xf0,0xf,0xf000},
    {8,1,8,0xe0,0x1c,0x3,0},
    {9,1,9,0x30,0xc,0x3,0xc0},
    {10,4,10,0xff,0xff00,0xff0000,0xff000000},
    {11,4,11,0xff000000,0xff0000,0xff00,0xff},
    {12,4,12,0xff00,0xff0000,0xff000000,0xff},
    {13,4,13,0xff,0xff00,0xff0000,0},
    {14,3,14,0xff,0xff00,0xff0000,0},
    {15,2,15,0x1f,0x7e0,0xf800,0},
    {16,2,16,0x1f,0x3e0,0x7c00,0},
    {17,2,17,0x1f,0x3e0,0x7c00,0x8000},
    {18,2,18,0xf,0xf0,0xf00,0xf000},
    {24,2,24,0xff,0xff00,0,0},
    {25,4,25,0xffff,0xffff0000,0,0},
    {26,2,26,0x1f,0x3e0,0xfc00,0},
    {27,4,13,0xff,0xff00,0xff0000,0}
};
static const int SameWidths[] = {1,2,3,4,7,8,9,15,16,17,31,32,33,63,64,65,257};
static const int SameOffsets[] = {0,1,2,3,7,15,16,31};
static const int SameModes[] = {VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_SSSE3,VX_SIMD_MODE_AVX2};

static void DescribeSameFormat(VxImageDescEx &d,int layout) {
    const SameFormatLayout &f=SameLayouts[layout];
    d.BitsPerPixel=f.bytes*8;
    d.RedMask=f.red;d.GreenMask=f.green;d.BlueMask=f.blue;d.AlphaMask=f.alpha;
    d.Flags=f.format>=24 ? static_cast<XDWORD>(f.format) : 0;
}

struct SameFormatImage {
    XBYTE source[2400],target[2400];
    VxImageDescEx src,dst;
    int bytes;
    SameFormatImage(int layout,int width=257,int sourceOffset=0,int targetOffset=0,
                    bool reverseSource=false,bool reverseTarget=false) : bytes(SameLayouts[layout].bytes) {
        std::memset(source,0x63,sizeof(source));std::memset(target,0xa5,sizeof(target));
        DescribeSameFormat(src,layout);DescribeSameFormat(dst,layout);
        src.Width=dst.Width=width;src.Height=dst.Height=2;
        const int sourcePitch=width*bytes+5,targetPitch=width*bytes+11;
        src.BytesPerLine=reverseSource ? -sourcePitch : sourcePitch;
        dst.BytesPerLine=reverseTarget ? -targetPitch : targetPitch;
        src.Image=source+32+sourceOffset+(reverseSource ? sourcePitch : 0);
        dst.Image=target+32+targetOffset+(reverseTarget ? targetPitch : 0);
        for(int y=0;y<2;++y)for(int x=0;x<width*bytes;++x)
            src.Image[y*src.BytesPerLine+x]=static_cast<XBYTE>(x+y*91);
    }
    void Run(bool flipped=false,bool unknown=false) {
        XBYTE before[2400],expected[2400];
        std::memcpy(before,source,sizeof(source));std::memcpy(expected,target,sizeof(target));
        VxImageDescEx srcBefore=src,dstBefore=dst;
        if(!unknown) {
            ASSERT_NE(VxImageDesc2PixelFormat(src),UNKNOWN_PF);
            ASSERT_EQ(VxImageDesc2PixelFormat(src),VxImageDesc2PixelFormat(dst));
        } else {
            ASSERT_EQ(VxImageDesc2PixelFormat(src),UNKNOWN_PF);
            ASSERT_EQ(VxImageDesc2PixelFormat(dst),UNKNOWN_PF);
        }
        for(int y=0;y<2;++y)for(int x=0;x<src.Width*bytes;++x) {
            XBYTE input=src.Image[(flipped ? 1-y : y)*src.BytesPerLine+x];
            expected[(dst.Image-target)+y*dst.BytesPerLine+x]=unknown ? (x%bytes==0 ? input&0x55 : 0) : input;
        }
        if(flipped)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        EXPECT_EQ(std::memcmp(target,expected,sizeof(target)),0);
        EXPECT_EQ(std::memcmp(source,before,sizeof(source)),0);
        EXPECT_EQ(std::memcmp(&src,&srcBefore,sizeof(src)),0);
        EXPECT_EQ(std::memcmp(&dst,&dstBefore,sizeof(dst)),0);
    }
};

class SameFormatBlitReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        previous=VxGetSIMDOverride();
        ASSERT_TRUE(VxSetSIMDOverride(SameModes[GetParam()]));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(),SameModes[GetParam()]);
    }
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

TEST_P(SameFormatBlitReferenceTest, AllKnownLayoutsPreserveEveryStorageBit) {
    for(int f=0;f<22;++f) {
        SCOPED_TRACE(SameLayouts[f].format);
        SameFormatImage image(f);
        ASSERT_EQ(VxImageDesc2PixelFormat(image.src),SameLayouts[f].detected);
        image.Run();
    }
}

TEST_P(SameFormatBlitReferenceTest, WidthsAndIndependentOffsetsPreservePadding) {
    for(int f=0;f<22;++f)for(int w=0;w<17;++w)for(int offset=0;offset<8;++offset) {
        SCOPED_TRACE(SameLayouts[f].format);
        SCOPED_TRACE(SameWidths[w]);
        SCOPED_TRACE(offset);
        SameFormatImage image(f,SameWidths[w],SameOffsets[offset],SameOffsets[(offset*3)%8]);
        image.Run();
    }
}

TEST_P(SameFormatBlitReferenceTest, SignedIndependentPitchesAndUpsideDown) {
    for(int f=0;f<22;++f)for(int signs=0;signs<4;++signs)for(int flip=0;flip<2;++flip) {
        SCOPED_TRACE(SameLayouts[f].format);
        SCOPED_TRACE(signs);
        SCOPED_TRACE(flip);
        SameFormatImage image(f,33,1,3,(signs&1)!=0,(signs&2)!=0);
        image.Run(flip!=0);
    }
}

TEST_P(SameFormatBlitReferenceTest, ConcreteLayoutsTakePriorityOverSpecialFlags) {
    static const XDWORD flags[]={0,19,20,21,22,23,24,25,26,27,0xffffffff};
    for(int f=0;f<22;++f)for(int flag=0;flag<11;++flag) {
        SCOPED_TRACE(SameLayouts[f].format);
        SCOPED_TRACE(flags[flag]);
        SameFormatImage image(f,17);
        image.src.Flags=flags[flag];image.dst.Flags=flags[(flag+3)%11];
        image.Run();
        if(SameLayouts[f].format>=24 && SameLayouts[f].format<=26) {
            // Native bump recognition ignores alpha; raw copy must retain all storage bits.
            image.src.AlphaMask=0x80000000;image.dst.AlphaMask=0x40000000;
            image.Run(true);
        }
    }
}

TEST_P(SameFormatBlitReferenceTest, EqualUnknownLayoutsUseMaskedConversion) {
    static const int layouts[]={7,3,2,0}; // 8/16/24/32-bit storage.
    for(int b=0;b<4;++b) {
        SameFormatImage image(layouts[b],17);
        image.src.Flags=image.dst.Flags=0;
        image.src.RedMask=image.dst.RedMask=1;
        image.src.GreenMask=image.dst.GreenMask=4;
        image.src.BlueMask=image.dst.BlueMask=16;
        image.src.AlphaMask=image.dst.AlphaMask=64;
        image.Run(false,true);image.Run(true,true);
    }
}

TEST_P(SameFormatBlitReferenceTest, DifferentDXTFormatsAndLinearConversionsRemainUnsupported) {
    for(int src=19;src<=23;++src)for(int dst=19;dst<=24;++dst) {
        if(src==dst)continue;
        XBYTE source[128],target[128],before[128];
        std::memset(source,0x63,sizeof(source));std::memset(target,0xa5,sizeof(target));
        std::memcpy(before,target,sizeof(target));
        VxImageDescEx s,d;
        s.Width=d.Width=4;s.Height=d.Height=4;s.Image=source;d.Image=target;
        s.Flags=static_cast<XDWORD>(src);s.TotalImageSize=8;
        d.Flags=static_cast<XDWORD>(dst);d.TotalImageSize=16;
        if(dst==24)DescribeSameFormat(d,0);
        VxDoBlit(s,d);VxDoBlitUpsideDown(s,d);
        EXPECT_EQ(std::memcmp(target,before,sizeof(target)),0);
        VxImageDescEx reverseSource=d,reverseTarget=s;
        reverseSource.Image=source;reverseTarget.Image=target;
        VxDoBlit(reverseSource,reverseTarget);VxDoBlitUpsideDown(reverseSource,reverseTarget);
        EXPECT_EQ(std::memcmp(target,before,sizeof(target)),0);
        for(int i=0;i<128;++i)EXPECT_EQ(source[i],0x63);
    }
}

static XBYTE *SameProtectedStorage(size_t &page) {
#if defined(_WIN32)
    SYSTEM_INFO system;GetSystemInfo(&system);page=system.dwPageSize;
    XBYTE *storage=static_cast<XBYTE *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    DWORD old;
    if(!storage || !VirtualProtect(storage+page,page,PAGE_NOACCESS,&old))std::_Exit(3);
#else
    page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *storage=static_cast<XBYTE *>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(storage==MAP_FAILED || mprotect(storage+page,page,PROT_NONE))std::_Exit(3);
#endif
    return storage;
}

static void SameProtectedRows(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    if(!VxSetSIMDOverride(mode) || VxGetSIMDEffectiveBackend()!=mode)std::_Exit(4);
    size_t sourcePage,targetPage;
    XBYTE *source=SameProtectedStorage(sourcePage),*target=SameProtectedStorage(targetPage);
    for(int f=0;f<22;++f)for(int w=0;w<17;++w) {
        SameFormatImage image(f,SameWidths[w]);int bytes=SameWidths[w]*image.bytes;
        XBYTE *src=source+sourcePage-bytes,*dst=target+targetPage-bytes;
        std::memcpy(src,image.src.Image,bytes);std::memset(dst-16,0xa5,bytes+16);
        image.src.Image=src;image.dst.Image=dst;image.src.Height=image.dst.Height=1;
        // The final logical row need not allocate padding described by the stride.
        VxDoBlit(image.src,image.dst);
        if(std::memcmp(src,dst,bytes))std::_Exit(2);
        for(int i=1;i<=16;++i)if(dst[-i]!=0xa5)std::_Exit(5);
        std::memset(dst,0xa5,bytes);VxDoBlitUpsideDown(image.src,image.dst);
        if(std::memcmp(src,dst,bytes))std::_Exit(6);
    }
    std::_Exit(0);
}

TEST_P(SameFormatBlitReferenceTest, ExactVisibleRowEndsAtProtectedPages) {
    EXPECT_EXIT(SameProtectedRows(SameModes[GetParam()]),testing::ExitedWithCode(0),"");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2,SameFormatBlitReferenceTest,testing::Range(0,4));
