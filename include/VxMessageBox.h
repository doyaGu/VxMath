#ifndef VXMESSAGEBOX_H
#define VXMESSAGEBOX_H

#include "VxMathDefines.h"

/// @brief Displays a message box using MB_* fields and ID* button values.
/// Windows forwards MessageBoxA text and flags unchanged and accepts native or SDL owner windows.
/// @return The selected ID* button value, or zero on failure/no selection.
VX_EXPORT int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType);

#endif // VXMESSAGEBOX_H
