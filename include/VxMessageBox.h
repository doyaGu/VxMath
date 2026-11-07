#ifndef VXMESSAGEBOX_H
#define VXMESSAGEBOX_H

#include "VxMathDefines.h"

/// @brief Displays an SDL-owned message box using the MB_* button/icon/default fields.
/// @return The selected ID* button value, or zero on failure/no selection.
VX_EXPORT int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType);

#endif // VXMESSAGEBOX_H
