#ifndef VXSIMDDISPATCHINTERNAL_H
#define VXSIMDDISPATCHINTERNAL_H

#include "VxSIMDMode.h"

int VxGetSIMDOverrideInternal();
int VxGetSIMDEffectiveBackendInternal();
void VxSIMDDispatchInitialize();
void VxSIMDDispatchRebuildAll();

// Module-level dispatch rebuild hooks.
void VxMathDispatchRebuild(int effectiveMode);
void VxDistanceDispatchRebuild(int effectiveMode);
void VxIntersectDispatchRebuild(int effectiveMode);
void VxGraphicDispatchRebuild(int effectiveMode);

#endif // VXSIMDDISPATCHINTERNAL_H
