/**
 * @file VxWindowFunctionsSDL3.cpp
 * @brief SDL3 implementation of VxWindowFunctions for portable system utilities.
 *
 * This implementation uses SDL3's APIs for file system operations, environment
 * variables, and provides portable implementations or stubs for platform-specific
 * window and bitmap functions.
 */

#include "VxWindowFunctions.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Platform-specific includes for fallback functionality
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <Windows.h>
#   include <direct.h>
#   if defined(_MSC_VER)
#       include <float.h>
#   endif
#else
#   include <unistd.h>
#   include <sys/stat.h>
#   include <sys/statvfs.h>
#   include <dirent.h>
#   include <dlfcn.h>
#   include <fnmatch.h>
#   include <limits.h>
#   ifndef PATH_MAX
#       define PATH_MAX 4096
#   endif
#endif

#include "XString.h"
#include "VxColor.h"
#include "VxImageDescEx.h"
#include "VxSharedLibrary.h"

// ============================================================================
// Keyboard Functions
// ============================================================================

static SDL_Scancode VxScanCodeToSdlScancode(XDWORD scancode) {
    switch (scancode) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x02: return SDL_SCANCODE_1;
    case 0x03: return SDL_SCANCODE_2;
    case 0x04: return SDL_SCANCODE_3;
    case 0x05: return SDL_SCANCODE_4;
    case 0x06: return SDL_SCANCODE_5;
    case 0x07: return SDL_SCANCODE_6;
    case 0x08: return SDL_SCANCODE_7;
    case 0x09: return SDL_SCANCODE_8;
    case 0x0A: return SDL_SCANCODE_9;
    case 0x0B: return SDL_SCANCODE_0;
    case 0x0C: return SDL_SCANCODE_MINUS;
    case 0x0D: return SDL_SCANCODE_EQUALS;
    case 0x0E: return SDL_SCANCODE_BACKSPACE;
    case 0x0F: return SDL_SCANCODE_TAB;
    case 0x10: return SDL_SCANCODE_Q;
    case 0x11: return SDL_SCANCODE_W;
    case 0x12: return SDL_SCANCODE_E;
    case 0x13: return SDL_SCANCODE_R;
    case 0x14: return SDL_SCANCODE_T;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x16: return SDL_SCANCODE_U;
    case 0x17: return SDL_SCANCODE_I;
    case 0x18: return SDL_SCANCODE_O;
    case 0x19: return SDL_SCANCODE_P;
    case 0x1A: return SDL_SCANCODE_LEFTBRACKET;
    case 0x1B: return SDL_SCANCODE_RIGHTBRACKET;
    case 0x1C: return SDL_SCANCODE_RETURN;
    case 0x1D: return SDL_SCANCODE_LCTRL;
    case 0x1E: return SDL_SCANCODE_A;
    case 0x1F: return SDL_SCANCODE_S;
    case 0x20: return SDL_SCANCODE_D;
    case 0x21: return SDL_SCANCODE_F;
    case 0x22: return SDL_SCANCODE_G;
    case 0x23: return SDL_SCANCODE_H;
    case 0x24: return SDL_SCANCODE_J;
    case 0x25: return SDL_SCANCODE_K;
    case 0x26: return SDL_SCANCODE_L;
    case 0x27: return SDL_SCANCODE_SEMICOLON;
    case 0x28: return SDL_SCANCODE_APOSTROPHE;
    case 0x29: return SDL_SCANCODE_GRAVE;
    case 0x2A: return SDL_SCANCODE_LSHIFT;
    case 0x2B: return SDL_SCANCODE_BACKSLASH;
    case 0x2C: return SDL_SCANCODE_Z;
    case 0x2D: return SDL_SCANCODE_X;
    case 0x2E: return SDL_SCANCODE_C;
    case 0x2F: return SDL_SCANCODE_V;
    case 0x30: return SDL_SCANCODE_B;
    case 0x31: return SDL_SCANCODE_N;
    case 0x32: return SDL_SCANCODE_M;
    case 0x33: return SDL_SCANCODE_COMMA;
    case 0x34: return SDL_SCANCODE_PERIOD;
    case 0x35: return SDL_SCANCODE_SLASH;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x37: return SDL_SCANCODE_KP_MULTIPLY;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0x3A: return SDL_SCANCODE_CAPSLOCK;
    case 0x3B: return SDL_SCANCODE_F1;
    case 0x3C: return SDL_SCANCODE_F2;
    case 0x3D: return SDL_SCANCODE_F3;
    case 0x3E: return SDL_SCANCODE_F4;
    case 0x3F: return SDL_SCANCODE_F5;
    case 0x40: return SDL_SCANCODE_F6;
    case 0x41: return SDL_SCANCODE_F7;
    case 0x42: return SDL_SCANCODE_F8;
    case 0x43: return SDL_SCANCODE_F9;
    case 0x44: return SDL_SCANCODE_F10;
    case 0x45: return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x46: return SDL_SCANCODE_SCROLLLOCK;
    case 0x47: return SDL_SCANCODE_KP_7;
    case 0x48: return SDL_SCANCODE_KP_8;
    case 0x49: return SDL_SCANCODE_KP_9;
    case 0x4A: return SDL_SCANCODE_KP_MINUS;
    case 0x4B: return SDL_SCANCODE_KP_4;
    case 0x4C: return SDL_SCANCODE_KP_5;
    case 0x4D: return SDL_SCANCODE_KP_6;
    case 0x4E: return SDL_SCANCODE_KP_PLUS;
    case 0x4F: return SDL_SCANCODE_KP_1;
    case 0x50: return SDL_SCANCODE_KP_2;
    case 0x51: return SDL_SCANCODE_KP_3;
    case 0x52: return SDL_SCANCODE_KP_0;
    case 0x53: return SDL_SCANCODE_KP_PERIOD;
    case 0x57: return SDL_SCANCODE_F11;
    case 0x58: return SDL_SCANCODE_F12;
    case 0x8D: return SDL_SCANCODE_KP_EQUALS;
    case 0x9C: return SDL_SCANCODE_KP_ENTER;
    case 0x9D: return SDL_SCANCODE_RCTRL;
    case 0xB3: return SDL_SCANCODE_KP_COMMA;
    case 0xB5: return SDL_SCANCODE_KP_DIVIDE;
    case 0xB7: return SDL_SCANCODE_PRINTSCREEN;
    case 0xB8: return SDL_SCANCODE_RALT;
    case 0xC7: return SDL_SCANCODE_HOME;
    case 0xC8: return SDL_SCANCODE_UP;
    case 0xC9: return SDL_SCANCODE_PAGEUP;
    case 0xCB: return SDL_SCANCODE_LEFT;
    case 0xCD: return SDL_SCANCODE_RIGHT;
    case 0xCF: return SDL_SCANCODE_END;
    case 0xD0: return SDL_SCANCODE_DOWN;
    case 0xD1: return SDL_SCANCODE_PAGEDOWN;
    case 0xD2: return SDL_SCANCODE_INSERT;
    case 0xD3: return SDL_SCANCODE_DELETE;
    case 0xDB: return SDL_SCANCODE_LGUI;
    case 0xDC: return SDL_SCANCODE_RGUI;
    case 0xDD: return SDL_SCANCODE_APPLICATION;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static XBOOL VxKeyStateActive(unsigned char keystate[256], XDWORD key) {
    return keystate && key < 256 && (keystate[key] & 0x81) != 0;
}

char VxScanCodeToAscii(XDWORD scancode, unsigned char keystate[256]) {
    const XBOOL shift = VxKeyStateActive(keystate, 0x2A) || VxKeyStateActive(keystate, 0x36);
    const XBOOL caps = keystate && (keystate[0x3A] & 0x01) != 0;
    const XBOOL upper = shift != caps;

    switch (scancode) {
    case 0x02: return shift ? '!' : '1';
    case 0x03: return shift ? '@' : '2';
    case 0x04: return shift ? '#' : '3';
    case 0x05: return shift ? '$' : '4';
    case 0x06: return shift ? '%' : '5';
    case 0x07: return shift ? '^' : '6';
    case 0x08: return shift ? '&' : '7';
    case 0x09: return shift ? '*' : '8';
    case 0x0A: return shift ? '(' : '9';
    case 0x0B: return shift ? ')' : '0';
    case 0x0C: return shift ? '_' : '-';
    case 0x0D: return shift ? '+' : '=';
    case 0x10: return upper ? 'Q' : 'q';
    case 0x11: return upper ? 'W' : 'w';
    case 0x12: return upper ? 'E' : 'e';
    case 0x13: return upper ? 'R' : 'r';
    case 0x14: return upper ? 'T' : 't';
    case 0x15: return upper ? 'Y' : 'y';
    case 0x16: return upper ? 'U' : 'u';
    case 0x17: return upper ? 'I' : 'i';
    case 0x18: return upper ? 'O' : 'o';
    case 0x19: return upper ? 'P' : 'p';
    case 0x1A: return shift ? '{' : '[';
    case 0x1B: return shift ? '}' : ']';
    case 0x1E: return upper ? 'A' : 'a';
    case 0x1F: return upper ? 'S' : 's';
    case 0x20: return upper ? 'D' : 'd';
    case 0x21: return upper ? 'F' : 'f';
    case 0x22: return upper ? 'G' : 'g';
    case 0x23: return upper ? 'H' : 'h';
    case 0x24: return upper ? 'J' : 'j';
    case 0x25: return upper ? 'K' : 'k';
    case 0x26: return upper ? 'L' : 'l';
    case 0x27: return shift ? ':' : ';';
    case 0x28: return shift ? '"' : '\'';
    case 0x29: return shift ? '~' : '`';
    case 0x2B: return shift ? '|' : '\\';
    case 0x2C: return upper ? 'Z' : 'z';
    case 0x2D: return upper ? 'X' : 'x';
    case 0x2E: return upper ? 'C' : 'c';
    case 0x2F: return upper ? 'V' : 'v';
    case 0x30: return upper ? 'B' : 'b';
    case 0x31: return upper ? 'N' : 'n';
    case 0x32: return upper ? 'M' : 'm';
    case 0x33: return shift ? '<' : ',';
    case 0x34: return shift ? '>' : '.';
    case 0x35: return shift ? '?' : '/';
    case 0x39: return ' ';
    default: return '\0';
    }
}

int VxScanCodeToName(XDWORD scancode, char *keyName) {
    if (!keyName)
        return 0;

    const char ascii = VxScanCodeToAscii(scancode, nullptr);
    if (ascii >= 33 && ascii <= 126) {
        keyName[0] = (char)toupper((unsigned char)ascii);
        keyName[1] = '\0';
        return 2;
    }

    switch (scancode) {
    case 0x0E:
        strcpy(keyName, "Backspace");
        return 10;
    case 0x0F:
        strcpy(keyName, "Tab");
        return 4;
    case 0x1C:
        strcpy(keyName, "Enter");
        return 6;
    case 0x2A:
        strcpy(keyName, "Left Shift");
        return 11;
    case 0x36:
        strcpy(keyName, "Right Shift");
        return 12;
    case 0x1D:
        strcpy(keyName, "Left Ctrl");
        return 10;
    case 0x9D:
        strcpy(keyName, "Right Ctrl");
        return 11;
    case 0x38:
        strcpy(keyName, "Left Alt");
        return 9;
    case 0xB8:
        strcpy(keyName, "Right Alt");
        return 10;
    case 0x39:
        strcpy(keyName, "Space");
        return 6;
    case 0x01:
        strcpy(keyName, "Escape");
        return 7;
    case 0x3B:
        strcpy(keyName, "F1");
        return 3;
    case 0x3C:
        strcpy(keyName, "F2");
        return 3;
    case 0x3D:
        strcpy(keyName, "F3");
        return 3;
    case 0x3E:
        strcpy(keyName, "F4");
        return 3;
    case 0x3F:
        strcpy(keyName, "F5");
        return 3;
    case 0x40:
        strcpy(keyName, "F6");
        return 3;
    case 0x41:
        strcpy(keyName, "F7");
        return 3;
    case 0x42:
        strcpy(keyName, "F8");
        return 3;
    case 0x43:
        strcpy(keyName, "F9");
        return 3;
    case 0x44:
        strcpy(keyName, "F10");
        return 4;
    case 0x57:
        strcpy(keyName, "F11");
        return 4;
    case 0x58:
        strcpy(keyName, "F12");
        return 4;
    case 0x37:
        strcpy(keyName, "Numpad *");
        return 9;
    case 0x47:
        strcpy(keyName, "Numpad 7");
        return 9;
    case 0x48:
        strcpy(keyName, "Numpad 8");
        return 9;
    case 0x49:
        strcpy(keyName, "Numpad 9");
        return 9;
    case 0x4A:
        strcpy(keyName, "Numpad -");
        return 9;
    case 0x4B:
        strcpy(keyName, "Numpad 4");
        return 9;
    case 0x4C:
        strcpy(keyName, "Numpad 5");
        return 9;
    case 0x4D:
        strcpy(keyName, "Numpad 6");
        return 9;
    case 0x4E:
        strcpy(keyName, "Numpad +");
        return 9;
    case 0x4F:
        strcpy(keyName, "Numpad 1");
        return 9;
    case 0x50:
        strcpy(keyName, "Numpad 2");
        return 9;
    case 0x51:
        strcpy(keyName, "Numpad 3");
        return 9;
    case 0x52:
        strcpy(keyName, "Numpad 0");
        return 9;
    case 0x53:
        strcpy(keyName, "Numpad .");
        return 9;
    case 0x8D:
        strcpy(keyName, "Numpad =");
        return 9;
    case 0x9C:
        strcpy(keyName, "Numpad Enter");
        return 13;
    case 0xB3:
        strcpy(keyName, "Numpad ,");
        return 9;
    case 0xB5:
        strcpy(keyName, "Numpad /");
        return 9;
    case 0xC7:
        strcpy(keyName, "Home");
        return 5;
    case 0xCF:
        strcpy(keyName, "End");
        return 4;
    case 0xC9:
        strcpy(keyName, "PREVIOUS");
        return 9;
    case 0xD1:
        strcpy(keyName, "NEXT");
        return 5;
    case 0xD2:
        strcpy(keyName, "Insert");
        return 7;
    case 0xD3:
        strcpy(keyName, "Delete");
        return 7;
    case 0xCB:
        strcpy(keyName, "Left Arrow");
        return 11;
    case 0xCD:
        strcpy(keyName, "Right Arrow");
        return 12;
    case 0xC8:
        strcpy(keyName, "Up Arrow");
        return 9;
    case 0xD0:
        strcpy(keyName, "Down Arrow");
        return 11;
    default:
        break;
    }

    SDL_Scancode sc = VxScanCodeToSdlScancode(scancode);
    const char *name = SDL_GetScancodeName(sc);
    if (name && *name) {
        strcpy(keyName, name);
        return (int)strlen(name) + 1;
    }

    if (keyName)
        keyName[0] = '\0';
    return 1;
}

// ============================================================================
// Cursor Functions
// ============================================================================

int VxShowCursor(XBOOL show) {
    static int s_cursorDisplayCount = 0;

    if (show)
        ++s_cursorDisplayCount;
    else
        --s_cursorDisplayCount;

    if (s_cursorDisplayCount >= 0) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }

    return s_cursorDisplayCount;
}

XBOOL VxSetCursor(VXCURSOR_POINTER cursorID) {
    SDL_SystemCursor sdlCursor;
    
    switch (cursorID) {
    case VXCURSOR_NORMALSELECT:
        sdlCursor = SDL_SYSTEM_CURSOR_DEFAULT;
        break;
    case VXCURSOR_BUSY:
        sdlCursor = SDL_SYSTEM_CURSOR_WAIT;
        break;
    case VXCURSOR_MOVE:
        sdlCursor = SDL_SYSTEM_CURSOR_MOVE;
        break;
    case VXCURSOR_LINKSELECT:
        sdlCursor = SDL_SYSTEM_CURSOR_POINTER;
        break;
    default:
        return TRUE;
    }

    // Cursor selection is harmless before the video subsystem exists. Treat it
    // as a successful no-op so headless tools and tests retain the legacy API
    // behavior without forcing video initialization.
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return TRUE;
    
    SDL_Cursor *cursor = SDL_CreateSystemCursor(sdlCursor);
    if (cursor) {
        SDL_SetCursor(cursor);
        return TRUE;
    }
    return FALSE;
}

// ============================================================================
// FPU Control Functions
// ============================================================================

XWORD VxGetFPUControlWord() {
    XWORD cw = 0;
#if defined(__GNUC__) || defined(__clang__)
#   if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__ ("fstcw %0" : "=m" (cw));
#   else
    // Non-x86: return default value
    cw = 0x027F;
#   endif
#elif defined(_MSC_VER)
#   if defined(_M_IX86) || defined(_M_X64)
    unsigned int msCw = 0;
    _controlfp_s(&msCw, 0, 0);
    cw = (XWORD)msCw;
#   endif
#endif
    return cw;
}

void VxSetFPUControlWord(XWORD Fpu) {
#if defined(__GNUC__) || defined(__clang__)
#   if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__ ("fldcw %0" : : "m" (Fpu));
#   endif
#elif defined(_MSC_VER)
#   if defined(_M_IX86) || defined(_M_X64)
    unsigned int ignored = 0;
    _controlfp_s(&ignored, (unsigned int)Fpu, 0xFFFFu);
#   endif
#endif
}

void VxSetBaseFPUControlWord() {
#if defined(_MSC_VER)
    unsigned int ignored = 0;
    _controlfp_s(&ignored, _CW_DEFAULT, _MCW_EM | _MCW_DN | _MCW_RC | _MCW_PC);
#endif
}

// ============================================================================
// Environment Variable Functions
// ============================================================================

void VxAddLibrarySearchPath(const char *path) {
    const char *envVar;
#ifdef _WIN32
    envVar = "PATH";
    char separator = ';';
#else
    envVar = "LD_LIBRARY_PATH";
    char separator = ':';
#endif
    
    const char *currentPath = SDL_getenv(envVar);
    XString newPath;
    newPath << path;
    if (currentPath && currentPath[0] != '\0') {
        newPath << separator << currentPath;
    }
    // SDL3 doesn't have SDL_setenv, use platform-specific or putenv pattern
#ifdef _WIN32
    SetEnvironmentVariableA(envVar, newPath.CStr());
#else
    setenv(envVar, newPath.CStr(), 1);
#endif
}

XBOOL VxGetEnvironmentVariable(const char *envName, XString &envValue) {
    if (!envName) {
        envValue = "";
        return FALSE;
    }
    
    const char *value = getenv(envName);
    if (!value) {
        envValue = "";
        return FALSE;
    }
    envValue = value;
    return TRUE;
}

XBOOL VxSetEnvironmentVariable(const char *envName, const char *envValue) {
#ifdef _WIN32
    return SetEnvironmentVariableA(envName, envValue);
#else
    if (envValue)
        return setenv(envName, envValue, 1) == 0;
    else
        return unsetenv(envName) == 0;
#endif
}

// ============================================================================
// Window Functions (minimal stubs - SDL windows are managed differently)
// ============================================================================

static SDL_Window *VxAsSDLWindow(WIN_HANDLE Win) {
    return (SDL_Window *)Win;
}

WIN_HANDLE VxWindowFromPoint(CKPOINT pt) {
    (void)pt;
    // SDL doesn't provide this functionality
    return NULL;
}

XBOOL VxGetClientRect(WIN_HANDLE Win, CKRECT *rect) {
    if (!rect)
        return FALSE;

    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        int w, h;
        if (SDL_GetWindowSize(window, &w, &h)) {
            rect->left = 0;
            rect->top = 0;
            rect->right = w;
            rect->bottom = h;
            return TRUE;
        }
    }
    
    memset(rect, 0, sizeof(CKRECT));
    return FALSE;
}

XBOOL VxGetWindowRect(WIN_HANDLE Win, CKRECT *rect) {
    if (!rect)
        return FALSE;

    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        int x, y, w, h;
        if (SDL_GetWindowPosition(window, &x, &y) && SDL_GetWindowSize(window, &w, &h)) {
            rect->left = x;
            rect->top = y;
            rect->right = x + w;
            rect->bottom = y + h;
            return TRUE;
        }
    }
    
    memset(rect, 0, sizeof(CKRECT));
    return FALSE;
}

XBOOL VxScreenToClient(WIN_HANDLE Win, CKPOINT *pt) {
    if (!pt)
        return FALSE;

    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        int wx, wy;
        if (SDL_GetWindowPosition(window, &wx, &wy)) {
            pt->x -= wx;
            pt->y -= wy;
            return TRUE;
        }
    }
    return FALSE;
}

XBOOL VxClientToScreen(WIN_HANDLE Win, CKPOINT *pt) {
    if (!pt)
        return FALSE;

    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        int wx, wy;
        if (SDL_GetWindowPosition(window, &wx, &wy)) {
            pt->x += wx;
            pt->y += wy;
            return TRUE;
        }
    }
    return FALSE;
}

WIN_HANDLE VxSetParent(WIN_HANDLE Child, WIN_HANDLE Parent) {
    SDL_Window *child = VxAsSDLWindow(Child);
    SDL_Window *parent = VxAsSDLWindow(Parent);
    
    if (child) {
        if (SDL_SetWindowParent(child, parent)) {
            return Parent;
        }
    }
    return NULL;
}

WIN_HANDLE VxGetParent(WIN_HANDLE Win) {
    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        return (WIN_HANDLE)SDL_GetWindowParent(window);
    }
    return NULL;
}

XBOOL VxMoveWindow(WIN_HANDLE Win, int x, int y, int Width, int Height, XBOOL Repaint) {
    (void)Repaint;
    
    SDL_Window *window = VxAsSDLWindow(Win);
    if (window) {
        SDL_SetWindowPosition(window, x, y);
        SDL_SetWindowSize(window, Width, Height);
        return TRUE;
    }
    return FALSE;
}

// ============================================================================
// Directory and Path Functions
// ============================================================================

XString VxGetTempPath() {
    // SDL3 doesn't have a direct temp path function, use platform-specific fallbacks
#ifdef _WIN32
    char buffer[512];
    DWORD len = GetTempPathA(512, buffer);
    if (len > 0 && len < 512) {
        return XString(buffer);
    }
    return XString("C:\\Temp\\");
#else
    const char *tmpdir = SDL_getenv("TMPDIR");
    if (tmpdir)
        return XString(tmpdir);
    tmpdir = SDL_getenv("TMP");
    if (tmpdir)
        return XString(tmpdir);
    tmpdir = SDL_getenv("TEMP");
    if (tmpdir)
        return XString(tmpdir);
    return XString("/tmp/");
#endif
}

XBOOL VxMakeDirectory(const char *path) {
    return SDL_CreateDirectory(path);
}

XBOOL VxRemoveDirectory(const char *path) {
    // SDL3 doesn't have a direct rmdir, use platform-specific calls
#ifdef _WIN32
    return RemoveDirectoryA(path);
#else
    return rmdir(path) == 0;
#endif
}

static XString VxJoinPathString(const char *dir, const char *file);

// Helper function to recursively delete directory
static XBOOL VxDeleteDirectoryRecursive(const char *path) {
    if (!path || !*path)
        return FALSE;

    // Use SDL_EnumerateDirectory for cross-platform directory iteration
    // For SDL3, we'll use the glob pattern approach or fall back to platform-specific
#ifdef _WIN32
    // Windows implementation using Shell API
    VxSharedLibrary lib;
    if (lib.Load("Shell32.dll")) {
        typedef int (__stdcall *LPFSHPROC)(void *);
        LPFSHPROC mySHFileOperation = (LPFSHPROC)lib.GetFunctionPtr("SHFileOperationA");
        if (mySHFileOperation) {
            // Create double-null terminated string
            const size_t pathLength = strlen(path);
            char *pathBuffer = new char[pathLength + 2];
            memcpy(pathBuffer, path, pathLength);
            pathBuffer[pathLength] = '\0';
            pathBuffer[pathLength + 1] = '\0';
            
            // SHFILEOPSTRUCT
            struct {
                void *hwnd;
                unsigned int wFunc;
                const char *pFrom;
                const char *pTo;
                unsigned short fFlags;
                int fAnyOperationsAborted;
                void *hNameMappings;
                const char *lpszProgressTitle;
            } fileOp = {0};
            
            fileOp.pFrom = pathBuffer;
            fileOp.wFunc = 0x0003; // FO_DELETE
            fileOp.fFlags = 0x0614; // FOF_NO_UI
            
            int ret = mySHFileOperation(&fileOp) == 0;
            delete[] pathBuffer;
            lib.ReleaseLibrary();
            return ret;
        }
        lib.ReleaseLibrary();
    }
    return FALSE;
#else
    // POSIX implementation
    DIR *dir = opendir(path);
    if (!dir)
        return FALSE;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        XString fullpath = VxJoinPathString(path, entry->d_name);

        struct stat st;
        if (stat(fullpath.CStr(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                VxDeleteDirectoryRecursive(fullpath.CStr());
            } else {
                unlink(fullpath.CStr());
            }
        }
    }

    closedir(dir);
    return rmdir(path) == 0;
#endif
}

XBOOL VxDeleteDirectory(const char *path) {
    return VxDeleteDirectoryRecursive(path);
}

XBOOL VxFileExists(const char *path) {
    if (!path)
        return FALSE;
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

XBOOL VxDirectoryExists(const char *path) {
    if (!path)
        return FALSE;
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

XBOOL VxCopyFile(const char *src, const char *dst, XBOOL failIfExists) {
    if (!src || !dst)
        return FALSE;
    if (failIfExists && VxFileExists(dst))
        return FALSE;

    FILE *input = fopen(src, "rb");
    if (!input)
        return FALSE;
    FILE *output = fopen(dst, "wb");
    if (!output) {
        fclose(input);
        return FALSE;
    }

    char buffer[64 * 1024];
    size_t read = 0;
    XBOOL ok = TRUE;
    while ((read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, read, output) != read) {
            ok = FALSE;
            break;
        }
    }
    if (ferror(input))
        ok = FALSE;
    fclose(output);
    fclose(input);
    return ok;
}

XBOOL VxDeleteFile(const char *path) {
    if (!path)
        return FALSE;
#ifdef _WIN32
    return DeleteFileA(path);
#else
    return unlink(path) == 0;
#endif
}

static XBOOL VxDirectoryNameMatches(const char *name, const char *mask) {
    if (!mask || !*mask)
        return TRUE;
#ifdef _WIN32
    const char *n = name;
    const char *m = mask;
    const char *star = NULL;
    const char *retry = NULL;
    while (*n) {
        if (*m == '?' || tolower((unsigned char)*m) == tolower((unsigned char)*n)) {
            ++m;
            ++n;
        } else if (*m == '*') {
            star = m++;
            retry = n;
        } else if (star) {
            m = star + 1;
            n = ++retry;
        } else {
            return FALSE;
        }
    }
    while (*m == '*')
        ++m;
    return *m == '\0';
#else
    return fnmatch(mask, name, 0) == 0;
#endif
}

static XString VxJoinPathString(const char *dir, const char *file) {
    XString result = dir ? dir : "";
    if (result.Length() > 0 && result[result.Length() - 1] != '\\' && result[result.Length() - 1] != '/') {
#ifdef _WIN32
        result << '\\';
#else
        result << '/';
#endif
    }
    result << (file ? file : "");
    return result;
}

#ifndef _WIN32
static XBOOL VxStatDirectoryEntry(const char *dir, const char *name, XBOOL &isDirectory, size_t &size) {
    XString fullpath = VxJoinPathString(dir, name);
    struct stat st;
    if (stat(fullpath.CStr(), &st) != 0)
        return FALSE;
    isDirectory = S_ISDIR(st.st_mode) ? TRUE : FALSE;
    size = (size_t)st.st_size;
    return TRUE;
}

#if defined(DT_DIR)
static XBOOL VxGetDirectoryEntryInfo(const char *dir, const char *name, unsigned char type, XBOOL &isDirectory, size_t &size) {
    if (type == DT_DIR) {
        isDirectory = TRUE;
        size = 0;
        return TRUE;
    }
    if (type == DT_REG) {
        return VxStatDirectoryEntry(dir, name, isDirectory, size);
    }

    if (type != DT_UNKNOWN
#if defined(DT_LNK)
        && type != DT_LNK
#endif
    ) {
        isDirectory = FALSE;
        size = 0;
        return TRUE;
    }

    return VxStatDirectoryEntry(dir, name, isDirectory, size);
}
#endif
#endif

XBOOL VxListDirectory(const char *dir, const char *mask, XBOOL includeDirectories, VxDirectoryEntryCallback callback, void *userData) {
    if (!dir || !callback)
        return FALSE;
#ifdef _WIN32
    XString search = VxJoinPathString(dir, (mask && *mask) ? mask : "*");
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search.CStr(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return FALSE;
    XBOOL ok = TRUE;
    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;
        XBOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory && !includeDirectories)
            continue;
        VxDirectoryEntry entry;
        entry.Name = findData.cFileName;
        entry.IsDirectory = isDirectory;
        entry.Size = (size_t)(((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow);
        if (!callback(&entry, userData)) {
            ok = FALSE;
            break;
        }
    } while (FindNextFileA(hFind, &findData));
    DWORD error = GetLastError();
    FindClose(hFind);
    return ok && error == ERROR_NO_MORE_FILES;
#else
    DIR *handle = opendir(dir);
    if (!handle)
        return FALSE;
    XBOOL ok = TRUE;
    struct dirent *entryData;
    while ((entryData = readdir(handle)) != NULL) {
        if (strcmp(entryData->d_name, ".") == 0 || strcmp(entryData->d_name, "..") == 0)
            continue;
        if (!VxDirectoryNameMatches(entryData->d_name, mask))
            continue;

        XBOOL isDirectory = FALSE;
        size_t size = 0;
#if defined(DT_DIR)
        if (!VxGetDirectoryEntryInfo(dir, entryData->d_name, entryData->d_type, isDirectory, size))
            continue;
#else
        if (!VxStatDirectoryEntry(dir, entryData->d_name, isDirectory, size))
            continue;
#endif
        if (isDirectory && !includeDirectories)
            continue;

        VxDirectoryEntry entry;
        entry.Name = entryData->d_name;
        entry.IsDirectory = isDirectory;
        entry.Size = size;
        if (!callback(&entry, userData)) {
            ok = FALSE;
            break;
        }
    }
    closedir(handle);
    return ok;
#endif
}

XBOOL VxGetCurrentDirectory(char *path, size_t pathSize) {
    if (!path || pathSize == 0)
        return FALSE;
#ifdef _WIN32
    DWORD result = GetCurrentDirectoryA((DWORD) pathSize, path);
    return result != 0 && result < pathSize;
#else
    return getcwd(path, pathSize) != NULL;
#endif
}

XString VxGetCurrentDirectory() {
#ifdef _WIN32
    DWORD required = GetCurrentDirectoryA(0, NULL);
    if (required == 0)
        return "";

    char *buffer = new char[required];
    DWORD written = GetCurrentDirectoryA(required, buffer);
    if (written == 0 || written >= required) {
        delete[] buffer;
        return "";
    }

    XString path(buffer, (int) written);
    delete[] buffer;
    return path;
#else
    char *buffer = getcwd(NULL, 0);
    if (!buffer)
        return "";
    XString path(buffer);
    free(buffer);
    return path;
#endif
}

XBOOL VxGetApplicationBasePath(char *path, size_t pathSize) {
    if (!path || pathSize == 0)
        return FALSE;
    const char *basePath = SDL_GetBasePath();
    if (!basePath)
        return FALSE;
    size_t basePathLength = strlen(basePath);
    if (basePathLength + 1 > pathSize)
        return FALSE;
    memcpy(path, basePath, basePathLength + 1);
    return TRUE;
}

XBOOL VxGetUserConfigPath(const char *appName, char *path, size_t pathSize) {
    if (!path || pathSize == 0)
        return FALSE;
    XString configPath;
    if (!VxGetUserConfigPath(appName, configPath))
        return FALSE;
    const size_t configPathLength = strlen(configPath.CStr());
    if (configPathLength + 1 > pathSize) {
        path[0] = '\0';
        return FALSE;
    }
    memcpy(path, configPath.CStr(), configPathLength + 1);
    return TRUE;
}

XBOOL VxGetUserConfigPath(const char *appName, XString &path) {
    path = "";
    const char *name = (appName && *appName) ? appName : "Ballance";
    const char *overrideBase = getenv("BALLANCE_USER_CONFIG_HOME");
    char *prefPath = NULL;
    if (overrideBase && *overrideBase) {
        path = overrideBase;
        if (path[path.Length() - 1] != '/' && path[path.Length() - 1] != '\\')
            path << '/';
        path << name << '/';
    } else if ((prefPath = SDL_GetPrefPath("", name)) != NULL) {
        path = prefPath;
        SDL_free(prefPath);
    } else {
#ifdef _WIN32
        const char *base = SDL_getenv("APPDATA");
        if (!base)
            base = SDL_getenv("USERPROFILE");
        if (!base)
            return FALSE;
        path = base;
        if (path.Length() > 0 && path[path.Length() - 1] != '\\' && path[path.Length() - 1] != '/')
            path << '\\';
        path << name << '\\';
#elif defined(__APPLE__)
        const char *home = SDL_getenv("HOME");
        if (!home)
            return FALSE;
        path = home;
        if (path.Length() > 0 && path[path.Length() - 1] != '/')
            path << '/';
        path << "Library/Application Support/" << name << '/';
#else
        const char *xdg = SDL_getenv("XDG_CONFIG_HOME");
        const char *home = SDL_getenv("HOME");
        if (xdg && *xdg) {
            path = xdg;
            if (path.Length() > 0 && path[path.Length() - 1] != '/')
                path << '/';
            path << name << '/';
        } else if (home) {
            path = home;
            if (path.Length() > 0 && path[path.Length() - 1] != '/')
                path << '/';
            path << ".config/" << name << '/';
        } else {
            return FALSE;
        }
#endif
    }
    XString marker = VxJoinPathString(path.CStr(), "_marker_");
    return VxCreateFileTree(marker.CStr());
}

XBOOL VxSetCurrentDirectory(const char *path) {
    // SDL3 doesn't provide this, use platform-specific
    if (!path)
        return FALSE;
#ifdef _WIN32
    return SetCurrentDirectoryA(path);
#else
    size_t len = strlen(path);
    char *normalized = new char[len + 1];

    for (size_t i = 0; i <= len; ++i)
        normalized[i] = (path[i] == '\\') ? '/' : path[i];

    XBOOL result = chdir(normalized) == 0;
    delete[] normalized;
    return result;
#endif
}

XBOOL VxMakePath(char *fullpath, size_t fullpathSize, const char *path, const char *file) {
    if (!path || !file || !fullpath || fullpathSize == 0)
        return FALSE;

    size_t pathLen = strlen(path);
    size_t fileLen = strlen(file);

#ifdef _WIN32
    char separator = '\\';
    char altSeparator = '/';
#else
    char separator = '/';
    char altSeparator = '\\';
#endif

    XBOOL needSeparator = (pathLen > 0 && path[pathLen - 1] != separator && path[pathLen - 1] != altSeparator);
    if (pathLen > ((size_t) -1) - (needSeparator ? 1 : 0))
        return FALSE;
    size_t totalLen = pathLen + (needSeparator ? 1 : 0);
    if (fileLen > ((size_t) -1) - totalLen)
        return FALSE;
    totalLen += fileLen;
    if (totalLen + 1 > fullpathSize)
        return FALSE;

    size_t pos = 0;
    if (pathLen > 0) {
        memcpy(fullpath, path, pathLen);
        pos = pathLen;
    }
    if (needSeparator)
        fullpath[pos++] = separator;
    if (fileLen > 0)
        memcpy(fullpath + pos, file, fileLen);

    fullpath[totalLen] = '\0';
    return TRUE;
}

XBOOL VxMakePath(XString &fullpath, const char *path, const char *file) {
    fullpath = "";
    if (!path || !file)
        return FALSE;

    size_t pathLen = strlen(path);
    size_t fileLen = strlen(file);
    if (pathLen > ((size_t) -1) - fileLen)
        return FALSE;
    size_t contentSize = pathLen + fileLen;
    if (contentSize > ((size_t) -1) - 2)
        return FALSE;
    size_t bufferSize = contentSize + 2;
    if (bufferSize > (size_t)XString::MAX_LENGTH + 1)
        return FALSE;

    char *buffer = new char[bufferSize];
    XBOOL ok = VxMakePath(buffer, bufferSize, path, file);
    if (ok)
        fullpath = buffer;
    delete[] buffer;
    return ok;
}

XBOOL VxTestDiskSpace(const char *dir, size_t size) {
    // SDL3 doesn't provide disk space info, use platform-specific
#ifdef _WIN32
    ULARGE_INTEGER freeBytesAvailable;
    if (GetDiskFreeSpaceExA(dir, &freeBytesAvailable, NULL, NULL)) {
        return size <= freeBytesAvailable.QuadPart;
    }
    return FALSE;
#else
    struct statvfs st;
    if (statvfs(dir, &st) != 0)
        return FALSE;
    unsigned long long freeBytes = (unsigned long long)st.f_bavail * st.f_frsize;
    return size <= freeBytes;
#endif
}

int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType) {
    (void)hWnd;
    (void)uType;
    
    // Use SDL's simple message box
    SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
    
    // Map Windows MB_* flags to SDL flags (basic mapping)
    if (uType & 0x10) // MB_ICONERROR
        flags = SDL_MESSAGEBOX_ERROR;
    else if (uType & 0x30) // MB_ICONWARNING
        flags = SDL_MESSAGEBOX_WARNING;
    
    SDL_ShowSimpleMessageBox(flags, lpCaption ? lpCaption : "Message", lpText ? lpText : "", NULL);
    return 0;
}

// ============================================================================
// Module Functions
// ============================================================================

XString VxGetModuleFileName(INSTANCE_HANDLE Handle) {
#ifdef _WIN32
    DWORD bufferSize = 260;
    for (;;) {
        char *buffer = new char[bufferSize];
        DWORD written = GetModuleFileNameA((HMODULE) Handle, buffer, bufferSize);
        if (written == 0) {
            delete[] buffer;
            return "";
        }
        if (written < bufferSize) {
            XString path(buffer, (int) written);
            delete[] buffer;
            return path;
        }
        delete[] buffer;
        if (bufferSize > ((DWORD) -1) / 2) {
            return "";
        }
        bufferSize = bufferSize + bufferSize;
    }
#else
    if (Handle == NULL) {
        // Get current executable path
        const char *basePath = SDL_GetBasePath();
        return basePath ? basePath : "";
    }

    Dl_info info;
    if (dladdr(Handle, &info) && info.dli_fname) {
        return info.dli_fname;
    }
    const char *basePath = SDL_GetBasePath();
    return basePath ? basePath : "";
#endif
}

size_t VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, size_t StringSize) {
    if (!string || StringSize == 0)
        return 0;

    XString path = VxGetModuleFileName(Handle);
    const size_t pathLength = strlen(path.CStr());
    const size_t copyLength = (pathLength < StringSize - 1) ? pathLength : StringSize - 1;
    if (copyLength > 0)
        memcpy(string, path.CStr(), copyLength);
    string[copyLength] = '\0';
    return copyLength;
}

INSTANCE_HANDLE VxGetModuleHandle(const char *filename) {
#ifdef _WIN32
    return GetModuleHandleA(filename);
#else
    if (filename == NULL)
        return dlopen(NULL, RTLD_NOW);
    return dlopen(filename, RTLD_NOW | RTLD_NOLOAD);
#endif
}

XBOOL VxCreateFileTree(const char *file) {
    XString filepath = file;
    if (filepath.Length() <= 1)
        return FALSE;

#ifdef _WIN32
    char separator = '\\';
    int startPos = 3; // Skip drive letter
#else
    char separator = '/';
    int startPos = 1;
#endif

    for (char *pch = &filepath[startPos]; *pch != '\0'; ++pch) {
        if (*pch != separator && *pch != '/')
            continue;
        *pch = '\0';
        
        // Check if directory exists, create if not
        SDL_PathInfo info;
        if (SDL_GetPathInfo(filepath.CStr(), &info)) {
            if (info.type != SDL_PATHTYPE_DIRECTORY) {
                *pch = separator;
                return FALSE;
            }
        } else if (!SDL_CreateDirectory(filepath.CStr())) {
            *pch = separator;
            return FALSE;
        }
        
        *pch = separator;
    }
    return TRUE;
}

XDWORD VxURLDownloadToCacheFile(const char *File, char *CachedFile, int szCachedFile) {
    // SDL3 doesn't provide URL downloading
    // This would require platform-specific code or a library like libcurl
    (void)File;
    if (CachedFile && szCachedFile > 0)
        CachedFile[0] = '\0';
    return 1; // Return error
}

// ============================================================================
// Bitmap Functions (stubs - SDL uses surfaces, not GDI bitmaps)
// ============================================================================

BITMAP_HANDLE VxCreateBitmap(const VxImageDescEx &desc) {
    // Create an SDL_Surface from the image description
    SDL_Surface *surface = SDL_CreateSurface(desc.Width, desc.Height, SDL_PIXELFORMAT_RGBA32);
    return (BITMAP_HANDLE)surface;
}

void VxDeleteBitmap(BITMAP_HANDLE Bitmap) {
    SDL_Surface *surface = (SDL_Surface *)Bitmap;
    if (surface) {
        SDL_DestroySurface(surface);
    }
}

XBYTE *VxConvertBitmap(BITMAP_HANDLE Bitmap, VxImageDescEx &desc) {
    SDL_Surface *surface = (SDL_Surface *)Bitmap;
    if (!surface)
        return NULL;
    
    desc.Width = surface->w;
    desc.Height = surface->h;
    desc.BitsPerPixel = 32;
    desc.BytesPerLine = surface->pitch;
    
    // Return pointer to surface pixels (caller should not free this)
    return (XBYTE *)surface->pixels;
}

BITMAP_HANDLE VxConvertBitmapTo24(BITMAP_HANDLE Bitmap) {
    SDL_Surface *surface = (SDL_Surface *)Bitmap;
    if (!surface)
        return NULL;
    
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
    return (BITMAP_HANDLE)converted;
}

XBOOL VxCopyBitmap(BITMAP_HANDLE Bitmap, const VxImageDescEx &desc) {
    SDL_Surface *surface = (SDL_Surface *)Bitmap;
    if (!surface || !desc.Image)
        return FALSE;
    
    // Copy image data to surface
    if (SDL_LockSurface(surface) == 0) {
        size_t rowBytes = desc.Width * (desc.BitsPerPixel / 8);
        for (int y = 0; y < desc.Height && y < surface->h; y++) {
            memcpy((XBYTE *)surface->pixels + y * surface->pitch,
                   desc.Image + y * desc.BytesPerLine,
                   rowBytes < (size_t)surface->pitch ? rowBytes : surface->pitch);
        }
        SDL_UnlockSurface(surface);
        return TRUE;
    }
    return FALSE;
}

// ============================================================================
// OS Info
// ============================================================================

VX_OSINFO VxGetOs() {
    const char *platform = SDL_GetPlatform();
    
    if (strcmp(platform, "Windows") == 0)
        return VXOS_WINSEVEN;
    else if (strcmp(platform, "Mac OS X") == 0 || strcmp(platform, "macOS") == 0)
        return VXOS_MACOSX;
    else if (strcmp(platform, "Linux") == 0)
        return VXOS_LINUXX86;
    
    return VXOS_UNKNOWN;
}

XBOOL VxGetMemoryStatus(VxMemoryStatus *status) {
    if (!status)
        return FALSE;
    memset(status, 0, sizeof(VxMemoryStatus));
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    memset(&statex, 0, sizeof(statex));
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex))
        return FALSE;
    status->MemoryLoad = statex.dwMemoryLoad;
    status->TotalPhysical = statex.ullTotalPhys;
    status->AvailablePhysical = statex.ullAvailPhys;
    status->TotalVirtual = statex.ullTotalVirtual;
    status->AvailableVirtual = statex.ullAvailVirtual;
    return TRUE;
#else
    long pageSize = sysconf(_SC_PAGESIZE);
    long physicalPages = sysconf(_SC_PHYS_PAGES);
    if (pageSize <= 0 || physicalPages <= 0)
        return FALSE;
    status->TotalPhysical = (uint64_t)pageSize * (uint64_t)physicalPages;
#if defined(_SC_AVPHYS_PAGES)
    long availablePages = sysconf(_SC_AVPHYS_PAGES);
    if (availablePages > 0)
        status->AvailablePhysical = (uint64_t)pageSize * (uint64_t)availablePages;
#endif
    status->TotalVirtual = status->TotalPhysical;
    status->AvailableVirtual = status->AvailablePhysical;
    if (status->TotalPhysical > 0 && status->AvailablePhysical > 0) {
        uint64_t used = status->TotalPhysical - status->AvailablePhysical;
        status->MemoryLoad = (XDWORD)((used * 100u) / status->TotalPhysical);
    }
    return TRUE;
#endif
}

// ============================================================================
// Font Functions (stubs - SDL uses SDL_ttf for fonts)
// ============================================================================

FONT_HANDLE VxCreateFont(const char *FontName, int FontSize, int Weight, XBOOL italic, XBOOL underline) {
    // SDL3 doesn't have built-in font support
    // Would need SDL_ttf integration
    (void)FontName;
    (void)FontSize;
    (void)Weight;
    (void)italic;
    (void)underline;
    return NULL;
}

XBOOL VxGetFontInfo(FONT_HANDLE Font, VXFONTINFO &desc) {
    (void)Font;
    desc = VXFONTINFO();
    return FALSE;
}

XBOOL VxDrawBitmapText(BITMAP_HANDLE Bitmap, FONT_HANDLE Font, const char *string, CKRECT *rect, XDWORD Align, XDWORD BkColor, XDWORD FontColor) {
    // SDL3 doesn't have built-in text rendering
    // Would need SDL_ttf integration
    (void)Bitmap;
    (void)Font;
    (void)string;
    (void)rect;
    (void)Align;
    (void)BkColor;
    (void)FontColor;
    return FALSE;
}

void VxDeleteFont(FONT_HANDLE Font) {
    (void)Font;
}
