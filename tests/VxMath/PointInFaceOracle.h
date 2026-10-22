#ifndef POINT_IN_FACE_ORACLE_H
#define POINT_IN_FACE_ORACLE_H
#include <cmath>

struct PointInFaceResult { int code, first, second; };

// Projected ray parity is independent of the production three-edge sign
// intersection. No coplanarity requirement.
static PointInFaceResult PointInFaceOracle(const VxVector &p, const VxVector &a,
    const VxVector &b, const VxVector &c, const VxVector &normal) {
    PointInFaceResult r={0,0,1};
    const double nx=std::fabs(normal.x),ny=std::fabs(normal.y),nz=std::fabs(normal.z);
    if(nx>=ny&&nx>=nz){r.first=1;r.second=2;}
    else if(ny>=nz){r.first=0;r.second=2;}
    const int x=r.first,y=r.second;
    const double px=p[x],py=p[y];
    const double area=(double(b[x])-a[x])*(double(c[y])-a[y])-(double(b[y])-a[y])*(double(c[x])-a[x]);
    // A horizontal ray parity test, plus closed edge membership, differs from
    // the production intersection of three oriented half-spaces. Anchor each
    // edge at its closer endpoint to retain tiny offsets near a vertex.
    const VxVector *vertices[]={&a,&b,&c};bool inside=false,onBoundary=false;
    for(int i=0;i<3;++i){
        const VxVector &start=*vertices[i],&end=*vertices[(i+1)%3];
        const double ax=start[x],ay=start[y],bx=end[x],by=end[y],dx=bx-ax,dy=by-ay;
        const double sax=px-ax,say=py-ay,sbx=px-bx,sby=py-by;
        const bool atEnd=sbx*sbx+sby*sby<sax*sax+say*say;
        const double side=atEnd?sbx*dy-sby*dx:sax*dy-say*dx;
        if(side==0&&px>=(ax<bx?ax:bx)&&px<=(ax>bx?ax:bx)&&py>=(ay<by?ay:by)&&py<=(ay>by?ay:by))onBoundary=true;
        if((ay>py)!=(by>py))if(dy>0?side<0:side>0)inside=!inside;
    }
    r.code=onBoundary||inside?(area>0?1:2):0;return r;
}
#endif
