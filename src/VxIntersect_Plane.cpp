#include "VxIntersect.h"

XBOOL VxIntersect::RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist)
{
    return FALSE;
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxPlane &plane)
{
    return FALSE;
}

XBOOL VxIntersect::BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane)
{
    return FALSE;
}

XBOOL VxIntersect::FacePlane(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxPlane &plane)
{
    return FALSE;
}

XBOOL VxIntersect::Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p)
{
    return FALSE;
}
