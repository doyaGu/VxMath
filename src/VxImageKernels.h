#pragma once

#include "VxMath.h"

void VxGenerateMipMapKernel(const VxImageDescEx &src_desc, XBYTE *Buffer, int simdMode);
XBOOL VxConvertToNormalMapKernel(const VxImageDescEx &image, XDWORD ColorMask, int simdMode);
XBOOL VxConvertToBumpMapKernel(const VxImageDescEx &image, int simdMode);

XBOOL VxGenerateMipMap32SSE2(const VxImageDescEx &src_desc, XBYTE *Buffer);
XBOOL VxGenerateMipMap16Rgb565SSE2(const VxImageDescEx &src_desc, XBYTE *Buffer);
XBOOL VxGenerateMipMap24Rgb888SSSE3(const VxImageDescEx &src_desc, XBYTE *Buffer);
XBOOL VxFillLuminance32SSE2(const XBYTE *row, int width, int *luminance);
XBOOL VxFillLuminance24SSSE3(const XBYTE *row, int width, int *luminance);
