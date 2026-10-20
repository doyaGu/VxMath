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

static const int PaletteModes[] = {VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_AVX2};
static const XDWORD PaletteTargetMasks[6][4] = {
    {0xff0000,0xff00,0xff,0xff000000}, {0xff0000,0xff00,0xff,0},
    {0xff,0xff00,0xff0000,0xff000000}, {0xff000000,0xff0000,0xff00,0xff},
    {0xff00,0xff0000,0xff000000,0xff}, {0xff,0xff00,0xff0000,0}
};

static XDWORD PaletteReferencePixel(int index,int layout,int bytes) {
    XDWORD b=(index*17+3)&255,g=(index*29+5)&255,r=(index*43+7)&255,a=bytes==4 ? (index*61+128)&255 : 255;
    switch (layout) {
    case 0:return a*0x1000000u+r*0x10000u+g*0x100u+b;
    case 1:return r*0x10000u+g*0x100u+b;
    case 2:return a*0x1000000u+b*0x10000u+g*0x100u+r;
    case 3:return r*0x1000000u+g*0x10000u+b*0x100u+a;
    case 4:return b*0x1000000u+g*0x10000u+r*0x100u+a;
    default:return b*0x10000u+g*0x100u+r;
    }
}

struct PaletteReferenceImage {
    XBYTE source[600],target[2200],palette[1040];
    VxImageDescEx src,dst;
    int layout,entries,entryBytes,sourcePitch,targetPitch;

    PaletteReferenceImage(int targetLayout=0,int bytes=4,int count=256,int width=19,
                          bool reverseSource=false,bool reverseTarget=false,bool explicitSourceMasks=true)
        :layout(targetLayout),entries(count),entryBytes(bytes),sourcePitch(width+3),targetPitch(width*4+8) {
        std::memset(source,0xa5,sizeof(source));std::memset(target,0xa5,sizeof(target));std::memset(palette,0xa5,sizeof(palette));
        src.Width=dst.Width=width;src.Height=dst.Height=2;
        src.BitsPerPixel=8;src.BytesPerLine=reverseSource ? -sourcePitch : sourcePitch;
        src.Image=source+8+(reverseSource ? sourcePitch : 0);
        src.ColorMapEntries=static_cast<short>(count);src.BytesPerColorEntry=static_cast<short>(bytes);src.ColorMap=palette+8;
        if (explicitSourceMasks) {
            src.RedMask=0xff0000;src.GreenMask=0xff00;src.BlueMask=0xff;src.AlphaMask=bytes==4 ? 0xff000000 : 0;
        }
        dst.BitsPerPixel=32;dst.BytesPerLine=reverseTarget ? -targetPitch : targetPitch;
        dst.Image=target+8+(reverseTarget ? targetPitch : 0);
        dst.RedMask=PaletteTargetMasks[layout][0];dst.GreenMask=PaletteTargetMasks[layout][1];
        dst.BlueMask=PaletteTargetMasks[layout][2];dst.AlphaMask=PaletteTargetMasks[layout][3];
        for(int i=0;i<count;++i) {
            XBYTE *p=src.ColorMap+i*bytes;p[0]=static_cast<XBYTE>(i*17+3);p[1]=static_cast<XBYTE>(i*29+5);p[2]=static_cast<XBYTE>(i*43+7);
            if(bytes==4)p[3]=static_cast<XBYTE>(i*61+128);
        }
        for(int y=0;y<2;++y)for(int x=0;x<width;++x)src.Image[y*src.BytesPerLine+x]=static_cast<XBYTE>((x*37+y*11)%count);
    }

    XDWORD Pixel(int x,int y) const {
        XDWORD value;std::memcpy(&value,dst.Image+y*dst.BytesPerLine+x*4,4);return value;
    }

    void Run(bool upsideDown=false) {
        XBYTE beforeSource[sizeof(source)],beforePalette[sizeof(palette)];
        std::memcpy(beforeSource,source,sizeof(source));std::memcpy(beforePalette,palette,sizeof(palette));
        VxImageDescEx beforeSrc=src,beforeDst=dst;
        if(upsideDown)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        for(int y=0;y<2;++y)for(int x=0;x<dst.Width;++x) {
            int index=(x*37+(upsideDown ? 1-y : y)*11)%entries;
            EXPECT_EQ(Pixel(x,y),PaletteReferencePixel(index,layout,entryBytes)) << "pixel " << x << "," << y;
        }
        EXPECT_EQ(std::memcmp(source,beforeSource,sizeof(source)),0);
        EXPECT_EQ(std::memcmp(palette,beforePalette,sizeof(palette)),0);
        EXPECT_EQ(std::memcmp(&src,&beforeSrc,sizeof(src)),0);EXPECT_EQ(std::memcmp(&dst,&beforeDst,sizeof(dst)),0);
        for(int y=0;y<2;++y)for(int x=dst.Width*4;x<targetPitch;++x)EXPECT_EQ(target[8+y*targetPitch+x],0xa5);
        for(int i=0;i<8;++i){EXPECT_EQ(target[i],0xa5);EXPECT_EQ(target[8+2*targetPitch+i],0xa5);}
    }
};

class PaletteNativeLayoutsTest:public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        previous=VxGetSIMDOverride();int mode=PaletteModes[GetParam()/6];
        ASSERT_TRUE(VxSetSIMDOverride(mode));ASSERT_EQ(VxGetSIMDEffectiveBackend(),mode);
    }
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

TEST_P(PaletteNativeLayoutsTest, NativePaletteChannelsAndTargetMasks) {
    static const int counts[]={1,16,256};int layout=GetParam()%6;
    for(int bytes=3;bytes<=4;++bytes)for(int c=0;c<3;++c)for(int flip=0;flip<2;++flip) {
        SCOPED_TRACE(bytes);
        SCOPED_TRACE(counts[c]);
        SCOPED_TRACE(flip);
        PaletteReferenceImage image(layout,bytes,counts[c]);image.Run(flip!=0);
    }
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2,PaletteNativeLayoutsTest,testing::Range(0,18));

class Paletted32ReferenceTest:public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(GetParam()));ASSERT_EQ(VxGetSIMDEffectiveBackend(),GetParam());}
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

static void DecodePaletteAtProtectedBoundary(int mode,int bytes) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SYSTEM_INFO system;GetSystemInfo(&system);size_t page=system.dwPageSize;
    XBYTE *storage=static_cast<XBYTE *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!storage)std::_Exit(3);DWORD old;
    if(!VirtualProtect(storage+page,page,PAGE_NOACCESS,&old))std::_Exit(4);
#else
    size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *storage=static_cast<XBYTE *>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(storage==MAP_FAILED)std::_Exit(3);
    if(mprotect(storage+page,page,PROT_NONE))std::_Exit(4);
#endif
    XBYTE *palette=storage+page-bytes;palette[0]=3;palette[1]=5;palette[2]=7;if(bytes==4)palette[3]=128;
    if(!VxSetSIMDOverride(mode)||VxGetSIMDEffectiveBackend()!=mode)std::_Exit(5);
    PaletteReferenceImage image(0,bytes,1,9);image.src.ColorMap=palette;
    VxDoBlit(image.src,image.dst);
    bool valid=true;for(int y=0;y<2;++y)for(int x=0;x<9;++x)valid=valid&&image.Pixel(x,y)==PaletteReferencePixel(0,0,bytes);
#if defined(_WIN32)
    VirtualFree(storage,0,MEM_RELEASE);
#else
    munmap(storage,page*2);
#endif
    std::_Exit(valid?0:2);
}

TEST_P(Paletted32ReferenceTest, ReadsOnlyDeclaredPaletteEntries) {
    for(int bytes=3;bytes<=4;++bytes)
        EXPECT_EXIT(DecodePaletteAtProtectedBoundary(GetParam(),bytes),testing::ExitedWithCode(0),"");
}

TEST_P(Paletted32ReferenceTest, UndeclaredIndicesBecomeZeroWithoutReadingExtraColors) {
    PaletteReferenceImage image(0,4,16,9);
    for(int y=0;y<2;++y)for(int x=0;x<9;++x)image.src.Image[y*image.src.BytesPerLine+x]=static_cast<XBYTE>(x%2 ? 255 : 16);
    VxDoBlit(image.src,image.dst);
    for(int y=0;y<2;++y)for(int x=0;x<9;++x)EXPECT_EQ(image.Pixel(x,y),0u);
}

TEST_P(Paletted32ReferenceTest, WidthsCoverVectorChunksAndScalarTails) {
    static const int widths[]={1,3,4,7,8,9,17,257};
    for(int i=0;i<8;++i){SCOPED_TRACE(widths[i]);PaletteReferenceImage image(0,4,256,widths[i]);image.Run();}
}

TEST_P(Paletted32ReferenceTest, PaletteMutationAtSameAddressIsVisible) {
    PaletteReferenceImage image(0,4,1);image.Run();
    image.src.ColorMap[0]=99;image.src.ColorMap[1]=88;image.src.ColorMap[2]=77;image.src.ColorMap[3]=0x66;
    VxDoBlit(image.src,image.dst);
    for(int y=0;y<2;++y)for(int x=0;x<image.dst.Width;++x)EXPECT_EQ(image.Pixel(x,y),0x664d5863u);
}

TEST_P(Paletted32ReferenceTest, ZeroSourceMasksKeepExistingBgrPaletteContract) {
    for(int layout=0;layout<6;++layout){SCOPED_TRACE(layout);PaletteReferenceImage image(layout,4,16,19,false,false,false);image.Run();}
}

TEST_P(Paletted32ReferenceTest, SignedPitchesPreserveLogicalRows) {
    PaletteReferenceImage sourceReversed(0,4,16,19,true,false);sourceReversed.Run();
    PaletteReferenceImage targetReversed(0,3,16,19,false,true);targetReversed.Run();
}

TEST_P(Paletted32ReferenceTest, QuantizedSolidImageDecodesWithoutColorLoss) {
    int previousFactor=GetQuantizationSamplingFactor();SetQuantizationSamplingFactor(15);
    // A solid color has a precise reconstruction oracle; arbitrary NeuQuant error is unbounded here.
    const XBYTE colors[4][3]={{235,25,75},{235,25,75},{235,25,75},{235,25,75}};
    for(int layout=0;layout<6;++layout) {
        PaletteReferenceImage image(layout,4,256,2,false,false,false);
        XDWORD source[4];for(int i=0;i<4;++i)source[i]=0xff000000u+colors[i][0]*0x10000u+colors[i][1]*0x100u+colors[i][2];
        VxImageDescEx src;src.Width=src.Height=2;src.BitsPerPixel=32;src.BytesPerLine=8;src.Image=reinterpret_cast<XBYTE *>(source);
        src.RedMask=0xff0000;src.GreenMask=0xff00;src.BlueMask=0xff;src.AlphaMask=0xff000000;
        VxDoBlit(src,image.src);VxDoBlit(image.src,image.dst);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
            XDWORD pixel=image.Pixel(x,y);int channels[3];
            switch(layout) {
            case 0:case 1:channels[0]=(pixel>>16)&255;channels[1]=(pixel>>8)&255;channels[2]=pixel&255;break;
            case 2:case 5:channels[0]=pixel&255;channels[1]=(pixel>>8)&255;channels[2]=(pixel>>16)&255;break;
            case 3:channels[0]=(pixel>>24)&255;channels[1]=(pixel>>16)&255;channels[2]=(pixel>>8)&255;break;
            default:channels[0]=(pixel>>8)&255;channels[1]=(pixel>>16)&255;channels[2]=(pixel>>24)&255;break;
            }
            for(int c=0;c<3;++c)EXPECT_EQ(channels[c],colors[y*2+x][c]);
        }
    }
    SetQuantizationSamplingFactor(previousFactor);
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2AVX2,Paletted32ReferenceTest,
                         testing::Values(VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_AVX2));
