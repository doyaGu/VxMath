#ifndef VX_FONT_SEARCH_H
#define VX_FONT_SEARCH_H

#include "XClassArray.h"
#include "XArray.h"
#include "XString.h"

// Private shared implementation for the non-Windows bitmap and texture fonts.
// Font tables and filesystem paths use UTF-8 internally. Legacy text uses the
// current C runtime character set; callers retain ownership of their bytes.
#pragma GCC visibility push(hidden)
struct VxFontFace
{
    XString family, style, path;
    int fontIndex = 0;
    int weight = 400;
    XBOOL italic = FALSE;
    int directory = 0;
};

XBOOL VxDecodeAnsiFontText(const char *text, XArray<wchar_t> &result);
int VxGetFontCharacter(unsigned char character);
XBOOL VxDecodeAnsiFontName(const char *name, XString &result);
XBOOL VxEncodeAnsiFontName(const char *name, XString &result);
void VxGetFontDirectories(XClassArray<XString> &directories);
void VxEnumerateFontDirectories(const XClassArray<XString> &directories, XClassArray<VxFontFace> &faces);
const XClassArray<VxFontFace> &VxGetSystemFonts();
XBOOL VxFindFontFace(const XClassArray<VxFontFace> &faces, const char *family, int weight, XBOOL italic, VxFontFace &result);
void VxGetFontFallbacks(const XClassArray<VxFontFace> &faces, const char *family, int weight, XBOOL italic,
                        XClassArray<VxFontFace> &result);
#pragma GCC visibility pop
#endif
