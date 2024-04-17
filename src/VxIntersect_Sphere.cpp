#include "VxIntersect.h"

XBOOL VxIntersect::SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2, float *oCollisionTime1, float *oCollisionTime2)
{
    return FALSE;
}

int VxIntersect::RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2)
{
    return 0;
}

int VxIntersect::SphereAABB(const VxSphere &iSphere, const VxBbox &iBox)
{
    return 0;
}