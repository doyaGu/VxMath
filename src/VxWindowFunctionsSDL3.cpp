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

char VxScanCodeToAscii(XDWORD scancode, unsigned char keystate[256]) {
    // SDL3 scancode to ASCII conversion
    // This is a simplified implementation - SDL3's key handling is event-based
    (void)keystate;
    
    SDL_Scancode sc = (SDL_Scancode)scancode;
    SDL_Keycode keycode = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
    
    // Only return printable ASCII characters
    if (keycode >= 32 && keycode <= 126) {
        return (char)keycode;
    }
    return '\0';
}

int VxScanCodeToName(XDWORD scancode, char *keyName) {
    SDL_Scancode sc = (SDL_Scancode)scancode;
    const char *name = SDL_GetScancodeName(sc);
    
    if (name && keyName) {
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
    
    const char *value = SDL_getenv(envName);
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

// Helper function to recursively delete directory
static XBOOL VxDeleteDirectoryRecursive(const char *path) {
    // Use SDL_EnumerateDirectory for cross-platform directory iteration
    int count = 0;
    char **entries = NULL;
    
    // For SDL3, we'll use the glob pattern approach or fall back to platform-specific
#ifdef _WIN32
    // Windows implementation using Shell API
    VxSharedLibrary lib;
    if (lib.Load("Shell32.dll")) {
        typedef int (__stdcall *LPFSHPROC)(void *);
        LPFSHPROC mySHFileOperation = (LPFSHPROC)lib.GetFunctionPtr("SHFileOperationA");
        if (mySHFileOperation) {
            // Create double-null terminated string
            char pathBuffer[MAX_PATH + 2];
            strncpy(pathBuffer, path, MAX_PATH);
            pathBuffer[strlen(path)] = '\0';
            pathBuffer[strlen(path) + 1] = '\0';
            
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
    char fullpath[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                VxDeleteDirectoryRecursive(fullpath);
            } else {
                unlink(fullpath);
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

XBOOL VxGetCurrentDirectory(char *path) {
    const char *basePath = SDL_GetBasePath();
    if (basePath) {
        strncpy(path, basePath, MAX_PATH - 1);
        path[MAX_PATH - 1] = '\0';
        return TRUE;
    }
    
    // Fallback to platform-specific
#ifdef _WIN32
    return GetCurrentDirectoryA(MAX_PATH, path) != 0;
#else
    return getcwd(path, PATH_MAX) != NULL;
#endif
}

XBOOL VxSetCurrentDirectory(const char *path) {
    // SDL3 doesn't provide this, use platform-specific
#ifdef _WIN32
    return SetCurrentDirectoryA(path);
#else
    return chdir(path) == 0;
#endif
}

XBOOL VxMakePath(char *fullpath, const char *path, const char *file) {
    if (!path || !file || !fullpath)
        return FALSE;

    strcpy(fullpath, path);
    size_t pathLen = strlen(path);

#ifdef _WIN32
    char separator = '\\';
    char altSeparator = '/';
#else
    char separator = '/';
    char altSeparator = '\\';
#endif

    if (pathLen >= MAX_PATH - strlen(file) - 1)
        return FALSE;

    // Check if we need to add a path separator
    if (pathLen > 0 && fullpath[pathLen - 1] != separator && fullpath[pathLen - 1] != altSeparator) {
        fullpath[pathLen] = separator;
        ++pathLen;
    }

    strcpy(&fullpath[pathLen], file);
    return TRUE;
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

size_t VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, size_t StringSize) {
    if (Handle == NULL) {
        // Get current executable path
        const char *basePath = SDL_GetBasePath();
        if (basePath) {
            strncpy(string, basePath, StringSize - 1);
            string[StringSize - 1] = '\0';
            XDWORD len = (XDWORD)strlen(string);
            return len;
        }
    }
    
    // For specific module handles, fall back to platform-specific
#ifdef _WIN32
    return GetModuleFileNameA((HMODULE)Handle, string, (DWORD)StringSize);
#else
    Dl_info info;
    if (dladdr(Handle, &info) && info.dli_fname) {
        strncpy(string, info.dli_fname, StringSize - 1);
        string[StringSize - 1] = '\0';
        return strlen(string);
    }
    return 0;
#endif
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
        if (!SDL_GetPathInfo(filepath.CStr(), &info)) {
            SDL_CreateDirectory(filepath.CStr());
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
    memset(&desc, 0, sizeof(VXFONTINFO));
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
