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

static const int DispatchModes[]={VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_SSSE3,VX_SIMD_MODE_AVX2};
static const XDWORD DispatchMasks[][4]={
    {0xff0000,0xff00,0xff,0xff000000},{0xff0000,0xff00,0xff,0},
    {0xff,0xff00,0xff0000,0xff000000},{0xff000000,0xff0000,0xff00,0xff},
    {0xff00,0xff0000,0xff000000,0xff},{0xff,0xff00,0xff0000,0},
    {0xffff,0xffff0000,0,0},{0xffffffff,0,0,0},{0,0,0,0xffffffff}
};

static XDWORD DispatchField(unsigned value,XDWORD mask) {
    if(!mask)return 0;
    int shift=0;while(!(mask&(1u<<shift)))++shift;
    int bits=0;XDWORD run=mask>>shift;while(run&1u){++bits;run>>=1;}
    XDWORD result=0;
    for(int bit=0;bit<bits;++bit)result=(result<<1)|((value>>(7-bit%8))&1u);
    return result<<shift;
}

struct DispatchPaletteImage {
    XBYTE source[600],target[2400],palette[1040];
    VxImageDescEx src,dst;
    int bits,bytes,entries;
    DispatchPaletteImage(int depth=2,int entryBytes=4,int count=0,int width=17,
                         bool reverseSource=false,bool reverseTarget=false,int sourceOffset=0,int targetOffset=0)
        :bits(depth),bytes(entryBytes),entries(count?count:1<<depth) {
        std::memset(source,0x63,sizeof(source));std::memset(target,0xa5,sizeof(target));std::memset(palette,0x73,sizeof(palette));
        src.Width=dst.Width=width;src.Height=dst.Height=2;
        int sp=(width*bits+7)/8+3,dp=width*4+7;
        src.BitsPerPixel=bits;src.BytesPerLine=reverseSource?-sp:sp;dst.BytesPerLine=reverseTarget?-dp:dp;
        src.Image=source+8+sourceOffset+(reverseSource?sp:0);dst.Image=target+8+targetOffset+(reverseTarget?dp:0);
        src.RedMask=0xff0000;src.GreenMask=0xff00;src.BlueMask=0xff;src.AlphaMask=bytes==4?0xff000000:0;
        src.BytesPerColorEntry=static_cast<short>(bytes);src.ColorMapEntries=static_cast<short>(entries);src.ColorMap=palette+8;
        dst.BitsPerPixel=32;SetMasks(0);
        for(int i=0;i<entries;++i) {
            XBYTE *p=src.ColorMap+i*bytes;p[0]=static_cast<XBYTE>(3+i*17);p[1]=static_cast<XBYTE>(5+i*29);p[2]=static_cast<XBYTE>(7+i*43);
            if(bytes==4)p[3]=static_cast<XBYTE>(128+i*31);
        }
        for(int y=0;y<2;++y) {
            std::memset(src.Image+y*src.BytesPerLine,0,(width*bits+7)/8);
            for(int x=0;x<width;++x)src.Image[y*src.BytesPerLine+x*bits/8]|=static_cast<XBYTE>(((x*3+y)%entries)<<(8-bits-(x*bits)%8));
        }
    }
    void SetMasks(int layout) {
        dst.RedMask=DispatchMasks[layout][0];dst.GreenMask=DispatchMasks[layout][1];dst.BlueMask=DispatchMasks[layout][2];dst.AlphaMask=DispatchMasks[layout][3];
    }
    XDWORD Expected(int x,int y,bool flipped) const {
        int row=flipped?1-y:y;
        int index=(src.Image[row*src.BytesPerLine+x*bits/8]>>(8-bits-(x*bits)%8))&((1<<bits)-1);
        if(index>=entries)return 0;
        const XBYTE *p=src.ColorMap+index*bytes;
        return DispatchField(p[2],dst.RedMask)|DispatchField(p[1],dst.GreenMask)|DispatchField(p[0],dst.BlueMask)|DispatchField(bytes==4?p[3]:255,dst.AlphaMask);
    }
    void Run(bool flipped=false) {
        XBYTE beforeSource[600],beforePalette[1040],expected[2400];
        std::memcpy(beforeSource,source,sizeof(source));std::memcpy(beforePalette,palette,sizeof(palette));std::memcpy(expected,target,sizeof(target));
        VxImageDescEx beforeSrc=src,beforeDst=dst;
        for(int y=0;y<2;++y)for(int x=0;x<src.Width;++x) {
            XDWORD color=Expected(x,y,flipped);std::memcpy(expected+(dst.Image-target)+y*dst.BytesPerLine+x*4,&color,4);
        }
        if(flipped)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        ASSERT_EQ(std::memcmp(target,expected,sizeof(target)),0);
        EXPECT_EQ(std::memcmp(source,beforeSource,sizeof(source)),0);EXPECT_EQ(std::memcmp(palette,beforePalette,sizeof(palette)),0);
        EXPECT_EQ(std::memcmp(&src,&beforeSrc,sizeof(src)),0);EXPECT_EQ(std::memcmp(&dst,&beforeDst,sizeof(dst)),0);
    }
    void Reject() {
        XBYTE before[2400];std::memcpy(before,target,sizeof(target));
        VxDoBlit(src,dst);VxDoBlitUpsideDown(src,dst);
        EXPECT_EQ(std::memcmp(target,before,sizeof(target)),0);
    }
};

class PaletteDispatchReferenceTest:public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(DispatchModes[GetParam()]));ASSERT_EQ(VxGetSIMDEffectiveBackend(),DispatchModes[GetParam()]);}
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

TEST_P(PaletteDispatchReferenceTest, TwoBitAllPackedBytesAndOriginal32BitLayouts) {
    for(int bytes=3;bytes<=4;++bytes)for(int layout=0;layout<6;++layout)for(int packed=0;packed<256;++packed) {
        DispatchPaletteImage image(2,bytes,4,4);image.SetMasks(layout);
        image.src.Image[0]=static_cast<XBYTE>(packed);image.src.Image[image.src.BytesPerLine]=static_cast<XBYTE>(packed^255);
        image.Run();
    }
}

TEST_P(PaletteDispatchReferenceTest, TwoBitWidthsPitchesAndIndependentOffsets) {
    const int widths[]={1,2,3,4,5,7,8,9,17,65,257},offsets[]={0,1,2,3,7,15,16,31};
    for(int bytes=3;bytes<=4;++bytes)for(int w=0;w<11;++w)for(int offset=0;offset<8;++offset) {
        DispatchPaletteImage image(2,bytes,4,widths[w],false,false,offsets[offset],offsets[(offset*3)%8]);image.Run();
    }
    for(int signs=0;signs<4;++signs)for(int flip=0;flip<2;++flip) {
        DispatchPaletteImage image(2,4,4,33,(signs&1)!=0,(signs&2)!=0,1,3);image.Run(flip!=0);
    }
}

TEST_P(PaletteDispatchReferenceTest, Existing32BitTargetsIncludeWideColorAndAlphaFields) {
    const int depths[]={1,2,4,8};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int layout=6;layout<9;++layout) {
        DispatchPaletteImage image(depths[b],bytes,0,257);image.SetMasks(layout);image.Run();image.Run(true);
    }
}

TEST_P(PaletteDispatchReferenceTest, TargetMasksQuantizeOrRepeatEveryChannelWidth) {
    const int widths[]={1,4,5,6,8,16,24,32},depths[]={1,2,4,8};
    for(int depth=0;depth<4;++depth)for(int bytes=3;bytes<=4;++bytes)for(int w=0;w<8;++w)for(int channel=0;channel<4;++channel) {
        DispatchPaletteImage image(depths[depth],bytes,0,17);image.dst.RedMask=image.dst.GreenMask=image.dst.BlueMask=image.dst.AlphaMask=0;
        XDWORD mask=widths[w]==32?0xffffffffu:((1u<<widths[w])-1)<<(32-widths[w]);
        if(channel==0)image.dst.RedMask=mask;if(channel==1)image.dst.GreenMask=mask;if(channel==2)image.dst.BlueMask=mask;if(channel==3)image.dst.AlphaMask=mask;
        image.Run();
    }
}

TEST_P(PaletteDispatchReferenceTest, SameConcreteFormatPrecedesPaletteEntryValidation) {
    DispatchPaletteImage image(8,4,4,17);
    image.src.RedMask=image.dst.RedMask=0xe0;image.src.GreenMask=image.dst.GreenMask=0x1c;image.src.BlueMask=image.dst.BlueMask=3;
    image.src.AlphaMask=image.dst.AlphaMask=0;image.dst.BitsPerPixel=8;image.src.BytesPerColorEntry=5;
    XBYTE expected[2400];std::memcpy(expected,image.target,sizeof(expected));
    for(int y=0;y<2;++y)std::memcpy(expected+(image.dst.Image-image.target)+y*image.dst.BytesPerLine,image.src.Image+y*image.src.BytesPerLine,17);
    VxDoBlit(image.src,image.dst);EXPECT_EQ(std::memcmp(expected,image.target,sizeof(expected)),0);
}

TEST_P(PaletteDispatchReferenceTest, ZeroEntryStrideKeepsOrdinaryMaskedColorConversion) {
    DispatchPaletteImage image(8,4,256,256);image.src.BytesPerColorEntry=0;
    image.src.RedMask=0xe0;image.src.GreenMask=0x1c;image.src.BlueMask=3;image.src.AlphaMask=0;
    XBYTE expected[2400];std::memcpy(expected,image.target,sizeof(expected));
    for(int y=0;y<2;++y)for(int x=0;x<256;++x) {
        unsigned v=image.src.Image[y*image.src.BytesPerLine+x],r=v>>5,g=(v>>2)&7,b=v&3;
        r=(r<<5)|(r<<2)|(r>>1);g=(g<<5)|(g<<2)|(g>>1);b*=85;
        XDWORD color=0xff000000u|(r<<16)|(g<<8)|b;std::memcpy(expected+(image.dst.Image-image.target)+y*image.dst.BytesPerLine+x*4,&color,4);
    }
    VxDoBlit(image.src,image.dst);EXPECT_EQ(std::memcmp(expected,image.target,sizeof(expected)),0);
}

TEST_P(PaletteDispatchReferenceTest, UnsupportedPaletteEntryWidthsDoNotDecode) {
    const int strides[]={-1,1,2,5,8};
    for(int i=0;i<5;++i) {DispatchPaletteImage image(8,4,4);image.src.BytesPerColorEntry=static_cast<short>(strides[i]);image.Reject();}
}

TEST_P(PaletteDispatchReferenceTest, PaletteCannotBypassCompressedFormatRejection) {
    for(int bits=1;bits<=8;bits*=2) {
        DispatchPaletteImage source(bits);source.src.Flags=_DXT1;source.Reject();
        DispatchPaletteImage target(bits);target.dst.RedMask=target.dst.GreenMask=target.dst.BlueMask=target.dst.AlphaMask=0;target.dst.Flags=_DXT5;target.Reject();
    }
}

TEST_P(PaletteDispatchReferenceTest, ConcreteDestinationMasksOverrideStaleSpecialFlags) {
    const int depths[]={1,2,4,8};
    for(int b=0;b<4;++b)for(int flag=19;flag<=27;++flag) {
        DispatchPaletteImage image(depths[b]);image.dst.Flags=static_cast<XDWORD>(flag);image.Run();
    }
}

TEST_P(PaletteDispatchReferenceTest, TwoBitShortPaletteAndSameAddressChanges) {
    DispatchPaletteImage image(2,4,1);image.Run();
    image.src.ColorMap[0]=99;image.src.ColorMap[1]=88;image.src.ColorMap[2]=77;image.src.ColorMap[3]=66;image.Run();
    image.src.Image[0]=0xff;image.Run();
}

static XBYTE *DispatchProtectedStorage(size_t &page) {
#if defined(_WIN32)
    SYSTEM_INFO system;GetSystemInfo(&system);page=system.dwPageSize;
    XBYTE *p=static_cast<XBYTE *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));DWORD old;
    if(!p||!VirtualProtect(p+page,page,PAGE_NOACCESS,&old))std::_Exit(3);
#else
    page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    XBYTE *p=static_cast<XBYTE *>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(p==MAP_FAILED||mprotect(p+page,page,PROT_NONE))std::_Exit(3);
#endif
    return p;
}
static void DispatchProtectedRows(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    if(!VxSetSIMDOverride(mode)||VxGetSIMDEffectiveBackend()!=mode)std::_Exit(4);
    size_t sp,dp,pp;XBYTE *source=DispatchProtectedStorage(sp),*target=DispatchProtectedStorage(dp),*palette=DispatchProtectedStorage(pp);
    const int widths[]={1,3,4,5,17,257};
    for(int bytes=3;bytes<=4;++bytes)for(int count=1;count<=4;count*=4)for(int w=0;w<6;++w) {
        DispatchPaletteImage image(2,bytes,count,widths[w]);int input=(widths[w]*2+7)/8;
        XBYTE *s=source+sp-input,*d=target+dp-widths[w]*4,*p=palette+pp-count*bytes;
        std::memcpy(s,image.src.Image,input);std::memcpy(p,image.src.ColorMap,count*bytes);std::memset(d-16,0xa5,widths[w]*4+16);
        image.src.Image=s;image.src.ColorMap=p;image.dst.Image=d;image.src.Height=image.dst.Height=1;
        VxDoBlit(image.src,image.dst);
        for(int x=0;x<widths[w];++x) {XDWORD color=0;std::memcpy(&color,d+x*4,4);if(color!=image.Expected(x,0,false))std::_Exit(2);}
        for(int i=1;i<=16;++i)if(d[-i]!=0xa5)std::_Exit(5);
    }
    std::_Exit(0);
}
TEST_P(PaletteDispatchReferenceTest, ExactPackedRowsAndShortPalettesEndAtProtectedPages) {
    EXPECT_EXIT(DispatchProtectedRows(DispatchModes[GetParam()]),testing::ExitedWithCode(0),"");
}
INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2,PaletteDispatchReferenceTest,testing::Range(0,4));
