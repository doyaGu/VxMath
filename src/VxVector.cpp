#include "VxVector.h"

#include "VxMatrix.h"

int radToAngle(float val)
{
    return (int)(val * (180.0f / PI));
}

float Tsin(int angle)
{
    return sinf((float)angle);
}

float Tcos(int angle)
{
    return cosf((float)angle);
}

const VxVector VxVector::m_AxisX(1.0f, 0.0f, 0.0f);
const VxVector VxVector::m_AxisY(0.0f, 1.0f, 0.0f);
const VxVector VxVector::m_AxisZ(0.0f, 0.0f, 1.0f);
const VxVector VxVector::m_Axis0(0.0f, 0.0f, 0.0f);
const VxVector VxVector::m_Axis1(1.0f, 1.0f, 1.0f);

VxVector &VxVector::operator=(const VxCompressedVector &v)
{
    x = sinf(v.ya) * sinf(v.xa);
    y = -sinf(v.xa);
    z = cosf(v.ya) * cosf(v.xa);
    return *this;
}

void VxVector::Normalize()
{
    *this *= InvMagnitude(*this);
}

void VxVector::Rotate(const VxMatrix &M)
{
    x = M[2][0] * z + M[1][0] * y + M[0][0] * x;
    y = M[2][1] * z + M[0][1] * x + M[1][1] * y;
    z = M[2][2] * z + M[0][2] * x + M[1][2] * y;
}

const VxVector &VxVector::axisX()
{
    return m_AxisX;
}

const VxVector &VxVector::axisY()
{
    return m_AxisY;
}

const VxVector &VxVector::axisZ()
{
    return m_AxisZ;
}

const VxVector &VxVector::axis0()
{
    return m_Axis0;
}

const VxVector &VxVector::axis1()
{
    return m_Axis1;
}

const VxVector Rotate(const VxMatrix &mat, const VxVector &pt)
{
    VxVector v;
    v.x = mat[2][0] * pt.z + mat[1][0] * pt.y + mat[0][0] * pt.x;
    v.y = mat[2][1] * pt.z + mat[0][1] * pt.x + mat[1][1] * pt.y;
    v.z = mat[2][2] * pt.z + mat[0][2] * pt.x + mat[1][2] * pt.y;
    return v;
}

const VxVector Rotate(const VxVector &v1, const VxVector &v2, float angle)
{
    VxVector v;
    VxVector n = Normalize(v2);
    v.x = v1.x * ((1.0f - n.x * n.x) * cos(angle) + n.x * n.x) +
          v1.y * (sin(angle) * (n.z) + ((1.0f - cos(angle)) * n.y)) +
          v1.z * ((1.0f - cos(angle)) * (n.z * n.x) - (sin(angle) * n.y));
    v.y = v1.y * ((1.0f - n.y * n.y) * cos(angle) + n.y * n.y) +
          v1.z * (sin(angle) * n.x + ((1.0f - cos(angle)) * n.z * n.y)) +
          v1.x * ((1.0f - cos(angle)) * (n.y * n.x) - (sin(angle) * n.y));
    v.z = v1.z * ((1.0f - n.z * n.z) * cos(angle) + n.z * n.z) +
          v1.x * (sin(angle) * n.y + (1.0f - cos(angle)) * (n.z * n.x)) +
          v1.y * (((1.0f - cos(angle)) * n.z * n.y) - sin(angle) * (n.z));
    return v;
}

const VxVector Rotate(const VxVector *v1, const VxVector *v2, float angle)
{
    return Rotate(*v1, *v2, angle);
}