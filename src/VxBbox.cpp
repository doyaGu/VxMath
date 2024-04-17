#include "VxVector.h"

int VxBbox::Classify(const VxBbox &box2, const VxVector &pt) const
{
    return 0;
}

void VxBbox::ClassifyVertices(const int iVcount, XBYTE *iVertices, XULONG iStride, XULONG *oFlags) const
{
}

void VxBbox::ClassifyVerticesOneAxis(const int iVcount, XBYTE *iVertices, XULONG iStride, const int iAxis, XULONG *oFlags) const
{

}

void VxBbox::TransformTo(VxVector *pts, const VxMatrix &Mat) const
{
}

void VxBbox::TransformFrom(const VxBbox &sbox, const VxMatrix &Mat)
{
}