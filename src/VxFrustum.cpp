#include "VxFrustum.h"

VxFrustum::VxFrustum()
    : m_Origin(VxVector::axis0()),
      m_Right(VxVector::axisX()),
      m_Up(VxVector::axisY()),
      m_Dir(VxVector::axisZ()),
      m_RBound(1.0),
      m_UBound(1.0),
      m_DMin(1.0),
      m_DMax(2.0),
      m_DRatio(0),
      m_RF(0),
      m_UF(0) { Update(); }

VxFrustum::VxFrustum(const VxVector &origin, const VxVector &right, const VxVector &up, const VxVector &dir,
                     float nearplane, float farplane, float fov, float aspectratio)
    : m_Origin(origin),
      m_Right(right),
      m_Up(up),
      m_Dir(dir),
      m_RBound(tanf(fov * 0.5) * nearplane),
      m_UBound(tanf(fov * 0.5) * nearplane * aspectratio),
      m_DMin(nearplane),
      m_DMax(farplane),
      m_DRatio(0),
      m_RF(0),
      m_UF(0) { Update(); }

void VxFrustum::Transform(const VxMatrix &invworldmat) {
    m_Right *= m_RBound;
    m_Up *= m_UBound;
    m_Dir *= m_DMin;
    Vx3DMultiplyMatrixVector(&m_Origin, invworldmat, &m_Origin);

    VxVector vectors[3];
    Vx3DRotateVectorMany(vectors, invworldmat, &m_Right, 3, 12);

    m_RBound = Magnitude(vectors[0]);
    m_UBound = Magnitude(vectors[1]);
    m_DMin = Magnitude(vectors[2]) ;
    m_DMax = m_DMin * m_DRatio;
    m_Right = vectors[0] * ( 1.0f / m_RBound);
    m_Up = vectors[1] * ( 1.0f / m_UBound);
    m_Dir = vectors[2] * ( 1.0f / m_DMin);

    Update();
}

void VxFrustum::ComputeVertices(VxVector vertices[8]) const {
}

void VxFrustum::Update() {
}
