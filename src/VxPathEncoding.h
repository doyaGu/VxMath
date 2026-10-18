#ifndef VXPATHENCODING_H
#define VXPATHENCODING_H

#include "XArray.h"
#include "XString.h"
#include <SDL3/SDL_error.h>
#include <limits.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

// Windows ANSI paths are converted only at SDL UTF-8 boundaries.
#ifdef _WIN32
static inline XBOOL VxPathToWide(const char *path, UINT codePage, XArray<wchar_t> &wide) {
    if (!path || strlen(path) >= INT_MAX)
        return SDL_SetError("Invalid or oversized path");
    int length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!length)
        return SDL_SetError("Invalid path encoding");
    XArray<wchar_t> result;
    result.Resize(length);
    if (!MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, path, -1, result.Begin(), length))
        return SDL_SetError("Invalid path encoding");
    wide = result;
    return TRUE;
}

static inline XBOOL VxPathFromWide(const wchar_t *wide, UINT codePage, XArray<char> &bytes) {
    DWORD flags = codePage == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL substituted = FALSE;
    BOOL *used = codePage == CP_UTF8 ? NULL : &substituted;
    int length = WideCharToMultiByte(codePage, flags, wide, -1, NULL, 0, NULL, used);
    if (!length || substituted)
        return SDL_SetError("Path cannot be represented in the ANSI code page");
    XArray<char> result;
    result.Resize(length);
    if (!WideCharToMultiByte(codePage, flags, wide, -1, result.Begin(), length, NULL, used) || substituted)
        return SDL_SetError("Path cannot be represented in the ANSI code page");
    bytes = result;
    return TRUE;
}

static inline XBOOL VxPathToUtf8(const char *path, XArray<char> &bytes) {
    XArray<wchar_t> wide;
    return VxPathToWide(path, GetACP(), wide) && VxPathFromWide(wide.Begin(), CP_UTF8, bytes);
}

static inline XBOOL VxPathFromUtf8(const char *path, XArray<char> &bytes) {
    XArray<wchar_t> wide;
    return VxPathToWide(path, CP_UTF8, wide) && VxPathFromWide(wide.Begin(), GetACP(), bytes);
}

static inline XBOOL VxAssignPath(const XArray<char> &bytes, XString &path) {
    if (bytes.Size() <= 0 || bytes.Size() - 1 > XString::MAX_LENGTH)
        return SDL_SetError("Path exceeds the legacy string limit");
    path = bytes.Begin();
    return TRUE;
}

#endif

static inline const char *VxNextPathCharacter(const char *path) {
#ifdef _WIN32
    if (IsDBCSLeadByteEx(GetACP(), (BYTE)*path) && path[1]) {
        wchar_t character;
        if (MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, path, 2, &character, 1) == 1)
            return path + 2;
    }
#endif
    return path + 1;
}

static inline XBOOL VxPathEndsWithSeparator(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p = VxNextPathCharacter(p))
        last = p;
    return *last == '/' || *last == '\\';
}

#endif // VXPATHENCODING_H
