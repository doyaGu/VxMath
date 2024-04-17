#include "VxPlane.h"

void VxPlane::Create(const VxVector &n, const VxVector &p)
{
    m_Normal = n;
    m_D = -DotProduct(n, p);
}

void VxPlane::Create(const VxVector &a, const VxVector &b, const VxVector &c)
{
    m_Normal = Normalize(VxVector(
        (c.z - a.z) * (b.y - a.y) - (c.y - a.y) * (b.z - a.z),
        (b.z - a.z) * (c.x - a.x) - (c.z - a.z) * (b.x - a.x),
        (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)));
    m_D = -DotProduct(m_Normal, a);
}