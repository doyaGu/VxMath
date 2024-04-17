#include "VxIntersect.h"

XBOOL VxIntersect::FrustumFace(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2)
{
    return FALSE;
}

XBOOL VxIntersect::FrustumAABB(const VxFrustum &frustum, const VxBbox &box)
{
    return FALSE;
}

XBOOL VxIntersect::FrustumOBB(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat)
{
    return FALSE;
}

XBOOL VxIntersect::FrustumBox(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat)
{
    return FALSE;
}
