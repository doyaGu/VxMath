#ifndef VXINTERSECTDISPATCHSTATEINTERNAL_H
#define VXINTERSECTDISPATCHSTATEINTERNAL_H

void VxIntersectPlaneDispatchRebuild(bool useSIMD);
void VxIntersectSphereDispatchRebuild(bool useSIMD);
void VxIntersectFrustumDispatchRebuild(bool useSIMD);
void VxIntersectFaceDispatchRebuild(bool useSIMD);

#endif // VXINTERSECTDISPATCHSTATEINTERNAL_H
