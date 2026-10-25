#pragma once
#include <math.h>

struct TBFlags { int visible; unsigned combined, common; };

// Independent direct corner enumeration. Modes: exact current, original SSE,
// original x87. The estimated reciprocal belongs only to the native oracle.
static TBFlags TBModel(const VxMatrix &m,const VxBbox &box,const VxRect *screen,VxRect *out,int mode) {
    static const unsigned corners[]={0,4,2,6,1,5,3,7},flat[]={0,1,2,3};
    const bool isFlat=box.Min.z==box.Max.z;
    const int count=isFlat ? 4 : 8;
    double points[8][4]; unsigned combined=0,common=0xffffffffu;
    for(int i=0;i<count;++i) {
        const unsigned bits=isFlat ? flat[i] : corners[i];
        const double p[]={bits&1 ? box.Max.x : box.Min.x,bits&2 ? box.Max.y : box.Min.y,bits&4 ? box.Max.z : box.Min.z,1};
        for(int col=0;col<4;++col) { points[i][col]=0; for(int row=0;row<4;++row) points[i][col]+=p[row]*m[row][col]; }
        const double x=points[i][0],y=points[i][1],z=points[i][2],w=points[i][3];
        unsigned flags=0;
        if(x < -w) flags|=0x10; if(x > w) flags|=0x20;
        if(y < -w) flags|=0x80; if(y > w) flags|=0x40;
        if(z < 0) flags|=0x100; if(z > w) flags|=0x200;
        combined|=flags; common&=flags;
    }
    if(out && screen) {
        const double halfX=mode ? float((screen->right-screen->left)*0.5f) : (double(screen->right)-screen->left)*0.5;
        const double halfY=mode ? float((screen->bottom-screen->top)*0.5f) : (double(screen->bottom)-screen->top)*0.5;
        const double centerX=mode ? float(screen->left+float(halfX)) : screen->left+halfX;
        const double centerY=mode ? float(screen->top+float(halfY)) : screen->top+halfY;
        float minX=mode==2 ? 1.0e6f : 1.0e10f,minY=minX,maxX=-minX,maxY=maxX;
        if(mode!=2 || !common) {
            if(!common) for(int i=0;i<count;++i) {
                const double w=points[i][3];
                if(mode==2 && !(w>0)) continue;
                float x,y;
                if(mode==1) {
#if defined(VX_SIMD_SSE) || defined(VX_TRANSFORM_BOX_PROBE)
                    const float inv=fabsf(_mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(float(w)))));
                    x=float(float(float(points[i][0])*float(halfX))*inv)+float(centerX);
                    y=float(float(float(points[i][1])*float(-halfY))*inv)+float(centerY);
#else
                    x=y=0;
#endif
                } else {
                    const double denominator=mode==2 ? w : fabs(w);
                    x=float(points[i][0]/denominator*halfX+centerX);
                    y=float(points[i][1]/denominator*(-halfY)+centerY);
                }
                if(mode==2) {
                    if(x<minX)minX=x; if(x>maxX)maxX=x;
                    if(y<minY)minY=y; if(y>maxY)maxY=y;
                } else {
                    minX=minX<x ? minX : x; maxX=maxX>x ? maxX : x;
                    minY=minY<y ? minY : y; maxY=maxY>y ? maxY : y;
                }
            }
            out->left=minX; out->right=maxX; out->top=minY; out->bottom=maxY;
        }
        if(combined&0x100) {
            if(combined&0x10)out->left=screen->left;
            if(combined&0x20)out->right=screen->right;
            if(combined&0x40)out->top=screen->top;
            if(combined&0x80)out->bottom=screen->bottom;
        }
    }
    return TBFlags{common==0,combined,common};
}
