#include "VxIntersect.h"

XBOOL VxIntersect::RayBox(const VxRay &ray, const VxBbox &box)
{
    return FALSE;
}

XBOOL VxIntersect::RayBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentBox(const VxRay &ray, const VxBbox &box)
{
    return FALSE;
}

XBOOL VxIntersect::SegmentBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal)
{
    return FALSE;
}

XBOOL VxIntersect::LineBox(const VxRay &ray, const VxBbox &box)
{
    return FALSE;
}

XBOOL VxIntersect::LineBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint, VxVector *innormal, VxVector *outnormal)
{
    return FALSE;
}

XBOOL VxIntersect::AABBAABB(const VxBbox &box1, const VxBbox &box2)
{
    return FALSE;
}

XBOOL VxIntersect::AABBOBB(const VxBbox &box1, const VxOBB &box2)
{
    return FALSE;
}

XBOOL VxIntersect::OBBOBB(const VxOBB &box1, const VxOBB &box2)
{
    return FALSE;
}

XBOOL VxIntersect::AABBFace(const VxBbox &box1, const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N)
{
    return FALSE;
}
