#include "VxIntersect.h"

XBOOL VxIntersect::PointInFace(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, int &i1, int &i2)
{
    return FALSE;
}

XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2)
{
    return FALSE;
}

XBOOL VxIntersect::RayFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2)
{
    return FALSE;
}

XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2)
{
    return FALSE;
}

void VxIntersect::GetPointCoefficients(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef)
{
}

XBOOL VxIntersect::FaceFace(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0, const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1)
{
    return FALSE;
}
