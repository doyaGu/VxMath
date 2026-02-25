#include "VxBlitDispatchBridge.h"

#include "VxBlitEngine.h"

void VxBlitApplySIMDMode(int effectiveMode) {
    TheBlitter.ApplySIMDMode(effectiveMode);
}
