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

static const int CrossByteModes[]={VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_SSSE3,VX_SIMD_MODE_AVX2};
static const XDWORD CrossByteMasks[][4]={
    {0xff0000,0xff00,0xff,0xff000000},{0xff0000,0xff00,0xff,0},
    {0xff,0xff00,0xff0000,0xff000000},{0xff000000,0xff0000,0xff00,0xff},
    {0xff00,0xff0000,0xff000000,0xff},{0xff,0xff00,0xff0000,0},
    {0xffff,0xffff0000,0,0},{0xffffffff,0,0,0},{0,0,0,0xffffffff}
};

static XDWORD CrossByteField(unsigned value,XDWORD mask) {
    if(!mask)return 0;
    int shift=0;while(!(mask&(1u<<shift)))++shift;
    int bits=0;XDWORD run=mask>>shift;while(run&1u){++bits;run>>=1;}
    XDWORD result=0;
    for(int bit=0;bit<bits;++bit)result=(result<<1)|((value>>(7-bit%8))&1u);
    return result<<shift;
}

struct CrossBytePaletteImage {
    XBYTE source[600],target[2400],palette[1040];
    VxImageDescEx src,dst;
    int bits,bytes,entries;
    CrossBytePaletteImage(int depth=3,int entryBytes=4,int count=0,int width=17,
                         bool reverseSource=false,bool reverseTarget=false,int sourceOffset=0,int targetOffset=0,int indexOffset=0)
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
            for(int x=0;x<width;++x)for(int bit=0;bit<bits;++bit) {
                const int position=x*bits+bit;
                src.Image[y*src.BytesPerLine+position/8]|=static_cast<XBYTE>((((x*3+y+indexOffset)%entries)>>(bits-1-bit)&1)<<(7-position%8));
            }
        }
    }
    void SetMasks(int layout) {
        dst.RedMask=CrossByteMasks[layout][0];dst.GreenMask=CrossByteMasks[layout][1];dst.BlueMask=CrossByteMasks[layout][2];dst.AlphaMask=CrossByteMasks[layout][3];
    }
    XDWORD Expected(int x,int y,bool flipped) const {
        int row=flipped?1-y:y;
        int index=0;
        for(int bit=0;bit<bits;++bit) {
            int position=x*bits+bit;index=index*2+((src.Image[row*src.BytesPerLine+position/8]>>(7-position%8))&1);
        }
        if(index>=entries)return 0;
        const XBYTE *p=src.ColorMap+index*bytes;
        return CrossByteField(p[2],dst.RedMask)|CrossByteField(p[1],dst.GreenMask)|CrossByteField(p[0],dst.BlueMask)|CrossByteField(bytes==4?p[3]:255,dst.AlphaMask);
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

class PackedPaletteCrossByteReferenceTest:public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(CrossByteModes[GetParam()]));ASSERT_EQ(VxGetSIMDEffectiveBackend(),CrossByteModes[GetParam()]);}
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

static XBYTE *CrossByteProtectedStorage(size_t &page) {
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

TEST_P(PackedPaletteCrossByteReferenceTest, LiteralPackedVectorsCrossEveryByteBoundary) {
    const int depths[]={3,5,6,7};
    const XBYTE packed[][7]={{0x05,0x39,0x77},{0x07,0xc3,0xe1,0x74,0x7c},
                            {0x03,0xf0,0x7e,0x0b,0xd0,0xfc},{0x01,0xfc,0x0f,0xe0,0x5f,0x41,0xfc}};
    const int indices[][8]={{0,1,2,3,4,5,6,7},{0,31,1,30,2,29,3,28},
                           {0,63,1,62,2,61,3,60},{0,127,1,126,2,125,3,124}};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int layout=0;layout<9;++layout) {
        CrossBytePaletteImage image(depths[b],bytes,0,8);image.SetMasks(layout);
        for(int y=0;y<2;++y)std::memcpy(image.src.Image+y*image.src.BytesPerLine,packed[b],depths[b]);
        VxDoBlit(image.src,image.dst);
        for(int y=0;y<2;++y)for(int x=0;x<8;++x) {
            const XBYTE *p=image.src.ColorMap+indices[b][x]*bytes;
            const XDWORD expected=CrossByteField(p[2],image.dst.RedMask)|CrossByteField(p[1],image.dst.GreenMask)|
                CrossByteField(p[0],image.dst.BlueMask)|CrossByteField(bytes==4?p[3]:255,image.dst.AlphaMask);
            XDWORD actual;std::memcpy(&actual,image.dst.Image+y*image.dst.BytesPerLine+x*4,4);
            ASSERT_EQ(actual,expected)<<"depth="<<depths[b]<<" entry="<<bytes<<" layout="<<layout<<" x="<<x;
        }
    }
}

TEST_P(PackedPaletteCrossByteReferenceTest, EveryIndexAtEveryBitPhaseAndOriginalTargetLayout) {
    const int depths[]={3,5,6,7};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int layout=0;layout<9;++layout)for(int seed=0;seed<8;++seed) {
        CrossBytePaletteImage image(depths[b],bytes,0,257,false,false,0,0,seed);image.SetMasks(layout);
        // Odd multiplier traverses all indices; the long rows revisit each bit phase.
        image.Run();image.Run(true);
    }
}

TEST_P(PackedPaletteCrossByteReferenceTest, AllTwoBytePatternsInOneContiguousBitstream) {
    const int sourceBytes=65536*2,depths[]={3,5,6,7};
    XBYTE *source=new XBYTE[sourceBytes];
    for(int pair=0;pair<65536;++pair){source[pair*2]=static_cast<XBYTE>(pair>>8);source[pair*2+1]=static_cast<XBYTE>(pair);}
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes) {
        CrossBytePaletteImage image(depths[b],bytes);VxImageDescEx src=image.src,dst=image.dst;
        int width=sourceBytes*8/depths[b];XBYTE *target=new XBYTE[width*4];
        src.Image=source;src.BytesPerLine=sourceBytes;dst.Image=target;dst.BytesPerLine=width*4;
        src.Width=dst.Width=width;src.Height=dst.Height=1;VxDoBlit(src,dst);
        for(int x=0;x<width;++x) {
            int index=0;
            for(int bit=0;bit<depths[b];++bit){int position=x*depths[b]+bit;index=index*2+((source[position/8]>>(7-position%8))&1);}
            const XBYTE *p=src.ColorMap+index*bytes;XDWORD actual;
            const XDWORD expected=(static_cast<XDWORD>(bytes==4?p[3]:255)<<24)|(p[2]<<16)|(p[1]<<8)|p[0];
            std::memcpy(&actual,target+x*4,4);ASSERT_EQ(actual,expected)<<"depth="<<depths[b]<<" x="<<x;
        }
        delete[] target;
    }
    delete[] source;
}

TEST_P(PackedPaletteCrossByteReferenceTest, RowsResetBitCursorAcrossSignedPitchesOffsetsAndFlips) {
    const int depths[]={3,5,6,7},widths[]={1,2,3,4,5,7,8,9,15,16,17,31,33,65,257};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int w=0;w<15;++w)
    for(int signs=0;signs<4;++signs)for(int flip=0;flip<2;++flip) {
        CrossBytePaletteImage image(depths[b],bytes,0,widths[w],(signs&1)!=0,(signs&2)!=0,w%4,(w*3)%4);
        image.Run(flip!=0);
    }
}

TEST_P(PackedPaletteCrossByteReferenceTest, UnusedTailBitsDoNotBecomePixels) {
    const int depths[]={3,5,6,7};
    for(int b=0;b<4;++b)for(int width=1;width<=33;++width) {
        CrossBytePaletteImage image(depths[b],4,0,width);image.Run();
        int used=width*depths[b],tail=used%8;
        if(tail)for(int y=0;y<2;++y)image.src.Image[y*image.src.BytesPerLine+used/8]^=static_cast<XBYTE>((1<<(8-tail))-1);
        image.Run();image.Run(true);
    }
}

TEST_P(PackedPaletteCrossByteReferenceTest, ShortPalettesAndSameAddressPaletteOrIndexChanges) {
    const int depths[]={3,5,6,7};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes) {
        CrossBytePaletteImage image(depths[b],bytes,3);image.Run();
        image.src.ColorMap[0]=99;image.src.ColorMap[1]=88;image.src.ColorMap[2]=77;image.Run();
        std::memset(image.src.Image,0xff,(image.src.Width*depths[b]+7)/8);image.Run();
    }
}

TEST_P(PackedPaletteCrossByteReferenceTest, ChannelWidthsStillQuantizeAndRepeatCorrectly) {
    const int depths[]={3,5,6,7},widths[]={1,4,5,6,8,16,24,32};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int w=0;w<8;++w)for(int channel=0;channel<4;++channel) {
        CrossBytePaletteImage image(depths[b],bytes);image.dst.RedMask=image.dst.GreenMask=image.dst.BlueMask=image.dst.AlphaMask=0;
        XDWORD mask=widths[w]==32?0xffffffffu:((1u<<widths[w])-1)<<(32-widths[w]);
        if(channel==0)image.dst.RedMask=mask;if(channel==1)image.dst.GreenMask=mask;
        if(channel==2)image.dst.BlueMask=mask;if(channel==3)image.dst.AlphaMask=mask;
        image.Run();
    }
}

static void CrossByteProtectedRows(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    if(!VxSetSIMDOverride(mode)||VxGetSIMDEffectiveBackend()!=mode)std::_Exit(4);
    size_t sp,dp,pp;XBYTE *source=CrossByteProtectedStorage(sp),*target=CrossByteProtectedStorage(dp),*palette=CrossByteProtectedStorage(pp);
    const int depths[]={3,5,6,7};
    for(int b=0;b<4;++b)for(int bytes=3;bytes<=4;++bytes)for(int shortMap=0;shortMap<2;++shortMap)
    for(int width=1;width<=33;++width)for(int flip=0;flip<2;++flip) {
        int count=shortMap?3:1<<depths[b];CrossBytePaletteImage image(depths[b],bytes,count,width);
        int input=(width*depths[b]+7)/8;
        XBYTE *s=source+sp-input,*d=target+dp-width*4,*p=palette+pp-count*bytes;
        std::memcpy(s,image.src.Image,input);std::memcpy(p,image.src.ColorMap,count*bytes);std::memset(d-16,0xa5,width*4+16);
        image.src.Image=s;image.src.ColorMap=p;image.dst.Image=d;image.src.Height=image.dst.Height=1;
        if(flip)VxDoBlitUpsideDown(image.src,image.dst);else VxDoBlit(image.src,image.dst);
        for(int x=0;x<width;++x){XDWORD color;std::memcpy(&color,d+x*4,4);if(color!=image.Expected(x,0,false))std::_Exit(2);}
        for(int i=1;i<=16;++i)if(d[-i]!=0xa5)std::_Exit(5);
    }
    std::_Exit(0);
}

TEST_P(PackedPaletteCrossByteReferenceTest, ExactSourcePaletteAndTargetEndAtProtectedPages) {
    EXPECT_EXIT(CrossByteProtectedRows(CrossByteModes[GetParam()]),testing::ExitedWithCode(0),"");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2,PackedPaletteCrossByteReferenceTest,testing::Range(0,4));
