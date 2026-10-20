#include <gtest/gtest.h>
#include <cmath>
#include <climits>
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

static const int IndexedModes[]={VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2,VX_SIMD_MODE_SSSE3,VX_SIMD_MODE_AVX2};

struct IndexedLifecycleImage {
    XBYTE source[4500],indices[4600],palette[1040];
    VxImageDescEx src,dst;
    int pattern;
    IndexedLifecycleImage(int width=32,int height=32,int depth=32,int entry=4,
                          bool reverseSource=false,bool reverseTarget=false,int sourceWidth=32,int sourceHeight=32)
        :pattern(1) {
        std::memset(source,0xa5,sizeof(source));std::memset(indices,0xa5,sizeof(indices));std::memset(palette,0xa5,sizeof(palette));
        src.Width=sourceWidth;src.Height=sourceHeight;src.BitsPerPixel=depth;
        int sp=sourceWidth*(depth/8)+8,dp=width+3;
        src.BytesPerLine=reverseSource?-sp:sp;src.Image=source+9+(reverseSource?(sourceHeight-1)*sp:0);
        src.RedMask=0xff0000;src.GreenMask=0xff00;src.BlueMask=0xff;src.AlphaMask=depth==32?0xff000000:0;
        dst.Width=width;dst.Height=height;dst.BitsPerPixel=8;dst.BytesPerLine=reverseTarget?-dp:dp;
        dst.Image=indices+11+(reverseTarget?(height-1)*dp:0);dst.ColorMap=palette+8;
        dst.ColorMapEntries=256;dst.BytesPerColorEntry=static_cast<short>(entry);Fill();
    }
    void Fill() {
        for(int y=0;y<src.Height;++y)for(int x=0;x<src.Width;++x) {
            XBYTE *p=src.Image+y*src.BytesPerLine+x*(src.BitsPerPixel/8);
            p[0]=static_cast<XBYTE>(pattern==2?75:pattern==0?(17*x+31*y+(x*y)%19):11*x+7*y);
            p[1]=static_cast<XBYTE>(pattern==2?25:pattern==0?p[0]:5*x+29*y);
            p[2]=static_cast<XBYTE>(pattern==2?235:pattern==0?p[0]:17*x+3*y);
            if(src.BitsPerPixel==32)p[3]=static_cast<XBYTE>(x+y);
        }
    }
    unsigned long long PaletteHash() const {
        unsigned long long result=14695981039346656037ULL;
        for(int i=0;i<256;++i)for(int c=2;c>=0;--c)result=(result^dst.ColorMap[i*dst.BytesPerColorEntry+c])*1099511628211ULL;
        return result;
    }
    static double Weight(int pixel,int output,int sourceSize,int targetSize) {
        if(sourceSize>targetSize) {
            double left=static_cast<double>(output)*sourceSize/targetSize,right=static_cast<double>(output+1)*sourceSize/targetSize;
            double overlap=std::fmin(right,pixel+1.0)-std::fmax(left,static_cast<double>(pixel));
            return overlap>0?overlap/(right-left):0;
        }
        double position=std::fmin(static_cast<double>(output)*sourceSize/targetSize,sourceSize-1.0);
        double weight=1.0-std::fabs(pixel-position);return weight>0?weight:0;
    }
    void ExpectedColor(int x,int y,bool flipped,int color[3]) const {
        double sum[3]={};
        int outputY=flipped?dst.Height-1-y:y;
        for(int sy=0;sy<src.Height;++sy) {
            double wy=Weight(sy,outputY,src.Height,dst.Height);if(wy==0)continue;
            for(int sx=0;sx<src.Width;++sx) {
                double weight=wy*Weight(sx,x,src.Width,dst.Width);if(weight==0)continue;
                const XBYTE *p=src.Image+sy*src.BytesPerLine+sx*(src.BitsPerPixel/8);
                for(int c=0;c<3;++c)sum[c]+=p[c]*weight;
            }
        }
        for(int c=0;c<3;++c)color[c]=static_cast<int>(std::floor(sum[c]+0.0000001));
    }
    static int Abs(int value){return value<0?-value:value;}
    bool PixelsCorrect(bool flipped,int maximumError=INT_MAX) const {
        for(int y=0;y<dst.Height;++y)for(int x=0;x<dst.Width;++x) {
            int color[3];ExpectedColor(x,y,flipped,color);
            const XBYTE *selected=dst.ColorMap+dst.Image[y*dst.BytesPerLine+x]*dst.BytesPerColorEntry;
            int distance=0;for(int c=0;c<3;++c)distance+=Abs(selected[c]-color[c]);
            if(distance>maximumError)return false;
            for(int i=0;i<256;++i) {
                const XBYTE *p=dst.ColorMap+i*dst.BytesPerColorEntry;int candidate=0;
                for(int c=0;c<3;++c)candidate+=Abs(p[c]-color[c]);
                if(candidate<distance)return false;
            }
        }
        return true;
    }
    void Run(bool flipped=false,int maximumError=INT_MAX) {
        XBYTE beforeSource[4500],beforeIndices[4600],beforePalette[1040];
        std::memcpy(beforeSource,source,sizeof(source));std::memcpy(beforeIndices,indices,sizeof(indices));std::memcpy(beforePalette,palette,sizeof(palette));
        VxImageDescEx beforeSrc=src,beforeDst=dst;
        if(flipped)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        ASSERT_TRUE(PixelsCorrect(flipped,maximumError))<<"target="<<dst.Width<<","<<dst.Height<<" depth="<<src.BitsPerPixel<<" flip="<<flipped;
        if(pattern==1&&src.Width==32&&src.Height==32)EXPECT_EQ(PaletteHash(),0x0a141c9e32d8f951ULL);
        for(int i=0;i<4600;++i) {
            bool active=false;for(int y=0;y<dst.Height;++y){ptrdiff_t start=dst.Image-indices+y*dst.BytesPerLine;if(i>=start&&i<start+dst.Width)active=true;}
            if(!active)EXPECT_EQ(indices[i],beforeIndices[i]);
        }
        for(int i=0;i<1040;++i) {
            ptrdiff_t offset=i-(dst.ColorMap-palette);
            if(offset<0||offset>=256*dst.BytesPerColorEntry)EXPECT_EQ(palette[i],beforePalette[i]);
        }
        if(dst.BytesPerColorEntry==4)for(int i=0;i<256;++i)EXPECT_EQ(dst.ColorMap[i*4+3],255);
        EXPECT_EQ(std::memcmp(beforeSource,source,sizeof(source)),0);
        EXPECT_EQ(std::memcmp(&src,&beforeSrc,sizeof(src)),0);EXPECT_EQ(std::memcmp(&dst,&beforeDst,sizeof(dst)),0);
    }
    void Reject(bool flipped=false) {
        XBYTE beforeIndices[4600],beforePalette[1040];std::memcpy(beforeIndices,indices,sizeof(indices));std::memcpy(beforePalette,palette,sizeof(palette));
        if(flipped)VxDoBlitUpsideDown(src,dst);else VxDoBlit(src,dst);
        EXPECT_EQ(std::memcmp(beforeIndices,indices,sizeof(indices)),0);EXPECT_EQ(std::memcmp(beforePalette,palette,sizeof(palette)),0);
    }
};

class IndexedLifecycleReferenceTest:public testing::TestWithParam<int> {
protected:
    int previous,factor;
    void SetUp() override {
        previous=VxGetSIMDOverride();factor=GetQuantizationSamplingFactor();SetQuantizationSamplingFactor(15);
        ASSERT_TRUE(VxSetSIMDOverride(IndexedModes[GetParam()]));ASSERT_EQ(VxGetSIMDEffectiveBackend(),IndexedModes[GetParam()]);
    }
    void TearDown() override {SetQuantizationSamplingFactor(factor);EXPECT_TRUE(VxSetSIMDOverride(previous));}
};

TEST_P(IndexedLifecycleReferenceTest, ResizeTrainsOriginalSourceThenMapsResampledColors) {
    const int shapes[][2]={{16,16},{64,64},{17,37},{64,16},{1,32},{32,1},{1,1}};
    for(int entry=3;entry<=4;++entry)for(int shape=0;shape<7;++shape) {
        IndexedLifecycleImage image(shapes[shape][0],shapes[shape][1],32,entry);image.Run();
    }
}

TEST_P(IndexedLifecycleReferenceTest, SignedPitchesWorkForResizeAndInversion) {
    for(int entry=3;entry<=4;++entry)for(int signs=0;signs<4;++signs) {
        IndexedLifecycleImage resized(17,37,32,entry,(signs&1)!=0,(signs&2)!=0);resized.Run();
        IndexedLifecycleImage flipped(32,32,32,entry,(signs&1)!=0,(signs&2)!=0);flipped.Run(true);
    }
}

TEST_P(IndexedLifecycleReferenceTest, InversionKeepsTrainingPaletteAndReversesOnlyIndices) {
    for(int depth=24;depth<=32;depth+=8)for(int entry=3;entry<=4;++entry)for(int reverse=0;reverse<2;++reverse) {
        IndexedLifecycleImage normal(32,32,depth,entry,false,reverse!=0),flipped(32,32,depth,entry,false,reverse!=0);
        normal.Run();flipped.Run(true);
        EXPECT_EQ(std::memcmp(normal.dst.ColorMap,flipped.dst.ColorMap,256*entry),0);
        for(int y=0;y<32;++y)EXPECT_EQ(std::memcmp(normal.dst.Image+y*normal.dst.BytesPerLine,flipped.dst.Image+(31-y)*flipped.dst.BytesPerLine,32),0);
    }
}

TEST_P(IndexedLifecycleReferenceTest, UnusedSourcePaletteCountDoesNotSuppressTruecolorTraining) {
    for(int depth=24;depth<=32;depth+=8)for(int entry=3;entry<=4;++entry)for(int flip=0;flip<2;++flip) {
        IndexedLifecycleImage image(32,32,depth,entry);image.src.ColorMapEntries=17;image.Run(flip!=0);
    }
    IndexedLifecycleImage resized(17,37);resized.src.ColorMapEntries=17;resized.Run();
}

TEST_P(IndexedLifecycleReferenceTest, RepeatedCallsAndTinySourceKeepTheirOwnNetworkAndStorage) {
    IndexedLifecycleImage image(17,37);
    for(int call=0;call<6;++call){image.pattern=call%3;image.Fill();image.Run();}
    IndexedLifecycleImage tiny(64,64,32,4,false,false,1,1);tiny.pattern=2;tiny.Fill();tiny.Run(false,0);
    EXPECT_EQ(GetQuantizationSamplingFactor(),15);
}

TEST_P(IndexedLifecycleReferenceTest, InvalidTargetsFailBeforeChangingPaletteOrIndices) {
    for(int flip=0;flip<2;++flip)for(int scenario=0;scenario<9;++scenario) {
        IndexedLifecycleImage image;
        switch(scenario) {
        case 0:image.dst.ColorMapEntries=16;break;case 1:image.dst.ColorMapEntries=255;break;
        case 2:image.dst.ColorMapEntries=257;break;case 3:image.dst.ColorMap=nullptr;break;
        case 4:image.dst.BytesPerColorEntry=2;break;case 5:image.dst.BitsPerPixel=4;break;
        case 6:image.src.BitsPerPixel=16;break;case 7:image.dst.Width=0;break;case 8:image.dst.Height=0;break;
        }
        image.Reject(flip!=0);
    }
}

TEST_P(IndexedLifecycleReferenceTest, Existing24BitConversionRemainsButUnsupportedResizesDoNotTrain) {
    IndexedLifecycleImage supported(32,32,24,3);supported.Run();
    IndexedLifecycleImage normal24Resize(17,37,24);normal24Resize.Reject();
    IndexedLifecycleImage invertedResize(17,37,32);invertedResize.Reject(true);
}

static XBYTE *IndexedProtectedStorage(size_t &page) {
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
static void IndexedProtectedLifecycle(int mode) {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    if(!VxSetSIMDOverride(mode)||VxGetSIMDEffectiveBackend()!=mode)std::_Exit(4);
    SetQuantizationSamplingFactor(15);size_t sp,dp,pp;
    XBYTE *source=IndexedProtectedStorage(sp),*target=IndexedProtectedStorage(dp),*palette=IndexedProtectedStorage(pp);
    for(int entry=3;entry<=4;++entry)for(int signs=0;signs<4;++signs)for(int flip=0;flip<2;++flip) {
        int width=flip?32:17,height=flip?32:37;IndexedLifecycleImage image(width,height,32,entry);
        XBYTE *s=source+sp-4096,*d=target+dp-width*height,*p=palette+pp-256*entry;
        for(int y=0;y<32;++y)std::memcpy(s+y*128,image.src.Image+y*image.src.BytesPerLine,128);
        if(signs&1)for(int y=0;y<16;++y)for(int x=0;x<128;++x){XBYTE t=s[y*128+x];s[y*128+x]=s[(31-y)*128+x];s[(31-y)*128+x]=t;}
        image.src.BytesPerLine=signs&1?-128:128;image.src.Image=s+(signs&1?31*128:0);
        image.dst.BytesPerLine=signs&2?-width:width;image.dst.Image=d+(signs&2?(height-1)*width:0);image.dst.ColorMap=p;
        std::memset(d-8,0xa5,width*height+8);std::memset(p-8,0xa5,256*entry+8);
        if(flip)VxDoBlitUpsideDown(image.src,image.dst);else VxDoBlit(image.src,image.dst);
        if(!image.PixelsCorrect(flip!=0)||image.PaletteHash()!=0x0a141c9e32d8f951ULL)std::_Exit(2);
        for(int i=1;i<=8;++i)if(d[-i]!=0xa5||p[-i]!=0xa5)std::_Exit(5);
    }
    VxImageDescEx src,dst;src.Width=INT_MAX;src.Height=1;src.BitsPerPixel=32;src.Image=source+sp;
    dst.Width=dst.Height=1;dst.BitsPerPixel=8;dst.ColorMapEntries=256;dst.BytesPerColorEntry=4;dst.ColorMap=palette+pp-1024;dst.Image=target+dp-1;
    *dst.Image=0xa5;std::memset(dst.ColorMap,0xa5,1024);VxDoBlit(src,dst);
    if(*dst.Image!=0xa5)std::_Exit(6);
    for(int i=0;i<1024;++i)if(dst.ColorMap[i]!=0xa5)std::_Exit(6);
    std::_Exit(0);
}
TEST_P(IndexedLifecycleReferenceTest, ExactStorageEndsAtProtectedPagesAndOverflowDoesNotReadSource) {
    EXPECT_EXIT(IndexedProtectedLifecycle(IndexedModes[GetParam()]),testing::ExitedWithCode(0),"");
}

INSTANTIATE_TEST_SUITE_P(ScalarSSE2SSSE3AVX2,IndexedLifecycleReferenceTest,testing::Range(0,4));
