#include "VxWindowFunctions.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#if defined(__linux__) && defined(RTLD_DI_LINKMAP)
#include <link.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "VxImageDescEx.h"
#include "XString.h"

static size_t CopyModulePathToBuffer(const char *path, char *output, size_t outputSize) {
    if (!path || !output || outputSize == 0) {
        return 0;
    }

    const size_t srcLen = strlen(path);
    const size_t maxCopy = outputSize - 1;
    const size_t copyLen = (srcLen < maxCopy) ? srcLen : maxCopy;
    if (copyLen > 0) {
        memcpy(output, path, copyLen);
    }
    output[copyLen] = '\0';
    return copyLen;
}

static XBOOL AppendPathComponent(XString &path, const char *name) {
    if (!name || !*name) {
        return FALSE;
    }

    if (path.Length() > 0 && path[path.Length() - 1] != '/') {
        path << '/';
    }
    path << name;
    return TRUE;
}

static XBOOL RemoveDirectoryTree(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        return errno == ENOENT ? TRUE : FALSE;
    }

    if (!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) {
        return unlink(path) == 0;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        return FALSE;
    }

    XBOOL success = TRUE;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        XString child(path);
        AppendPathComponent(child, entry->d_name);
        if (!RemoveDirectoryTree(child.CStr())) {
            success = FALSE;
            break;
        }
    }

    closedir(dir);
    if (!success) {
        return FALSE;
    }

    return rmdir(path) == 0;
}

static XBOOL CreateDirectoryTree(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }

    XString current(path);
    for (XWORD i = 1; i < current.Length(); ++i) {
        if (current[i] != '/') {
            continue;
        }

        current[i] = '\0';
        if (current.Length() > 0 && current.CStr()[0] != '\0') {
            if (mkdir(current.CStr(), 0777) != 0 && errno != EEXIST) {
                current[i] = '/';
                return FALSE;
            }
        }
        current[i] = '/';
    }

    if (mkdir(current.CStr(), 0777) != 0 && errno != EEXIST) {
        return FALSE;
    }

    struct stat st;
    return stat(current.CStr(), &st) == 0 && S_ISDIR(st.st_mode);
}

static XBOOL CopyParentPath(const char *file, XString &parent) {
    if (!file || !*file) {
        return FALSE;
    }

    const char *lastSlash = strrchr(file, '/');
    if (!lastSlash) {
        parent = "";
        return TRUE;
    }

    const int parentLen = static_cast<int>(lastSlash - file);
    if (parentLen <= 0) {
        parent = "/";
        return TRUE;
    }

    parent.Create(file, parentLen);
    return TRUE;
}

char VxScanCodeToAscii(XDWORD scancode, unsigned char keystate[256]) {
    (void) keystate;

    switch (scancode) {
    case 0x1E: return 'a';
    case 0x30: return 'b';
    case 0x2E: return 'c';
    case 0x20: return 'd';
    case 0x12: return 'e';
    case 0x21: return 'f';
    case 0x22: return 'g';
    case 0x23: return 'h';
    case 0x17: return 'i';
    case 0x24: return 'j';
    case 0x25: return 'k';
    case 0x26: return 'l';
    case 0x32: return 'm';
    case 0x31: return 'n';
    case 0x18: return 'o';
    case 0x19: return 'p';
    case 0x10: return 'q';
    case 0x13: return 'r';
    case 0x1F: return 's';
    case 0x14: return 't';
    case 0x16: return 'u';
    case 0x2F: return 'v';
    case 0x11: return 'w';
    case 0x2D: return 'x';
    case 0x15: return 'y';
    case 0x2C: return 'z';
    default: return '\0';
    }
}

int VxScanCodeToName(XDWORD scancode, char *keyName) {
    if (!keyName) {
        return 0;
    }

    switch (scancode) {
    case 0xCB:
        strcpy(keyName, "Left");
        break;
    case 0xCD:
        strcpy(keyName, "Right");
        break;
    case 0xC8:
        strcpy(keyName, "Up");
        break;
    case 0xD0:
        strcpy(keyName, "Down");
        break;
    case 0x3B:
        strcpy(keyName, "F1");
        break;
    default:
        snprintf(keyName, 32, "ScanCode_%u", static_cast<unsigned int>(scancode));
        break;
    }

    return static_cast<int>(strlen(keyName) + 1);
}

int VxShowCursor(XBOOL show) {
    static int cursorCount = 0;
    if (show) {
        ++cursorCount;
    } else {
        --cursorCount;
    }
    return cursorCount;
}

XBOOL VxSetCursor(VXCURSOR_POINTER cursorID) {
    (void) cursorID;
    return TRUE;
}

XWORD VxGetFPUControlWord() {
    XWORD cw = 0;
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("fstcw %0" : "=m"(cw));
#endif
    return cw;
}

void VxSetFPUControlWord(XWORD Fpu) {
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("fldcw %0" : : "m"(Fpu));
#else
    (void) Fpu;
#endif
}

void VxSetBaseFPUControlWord() {
#if defined(__GNUC__) || defined(__clang__)
    const XWORD defaultFpu = 0x037F;
    VxSetFPUControlWord(defaultFpu);
#endif
}

void VxAddLibrarySearchPath(const char *path) {
    if (!path || !*path) {
        return;
    }

    const char *existing = getenv("LD_LIBRARY_PATH");
    XString value(path);
    if (existing && *existing) {
        value << ':' << existing;
    }
    setenv("LD_LIBRARY_PATH", value.CStr(), 1);
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
    if (!envName || !*envName) {
        return FALSE;
    }

    if (!envValue) {
        return unsetenv(envName) == 0;
    }

    return setenv(envName, envValue, 1) == 0;
}

WIN_HANDLE VxWindowFromPoint(CKPOINT pt) {
    (void) pt;
    return NULL;
}

XBOOL VxGetClientRect(WIN_HANDLE Win, CKRECT *rect) {
    (void) Win;
    if (!rect) {
        return FALSE;
    }
    rect->left = rect->top = rect->right = rect->bottom = 0;
    return FALSE;
}

XBOOL VxGetWindowRect(WIN_HANDLE Win, CKRECT *rect) {
    return VxGetClientRect(Win, rect);
}

XBOOL VxScreenToClient(WIN_HANDLE Win, CKPOINT *pt) {
    (void) Win;
    (void) pt;
    return FALSE;
}

XBOOL VxClientToScreen(WIN_HANDLE Win, CKPOINT *pt) {
    (void) Win;
    (void) pt;
    return FALSE;
}

WIN_HANDLE VxSetParent(WIN_HANDLE Child, WIN_HANDLE Parent) {
    (void) Parent;
    return Child;
}

WIN_HANDLE VxGetParent(WIN_HANDLE Win) {
    (void) Win;
    return NULL;
}

XBOOL VxMoveWindow(WIN_HANDLE Win, int x, int y, int Width, int Height, XBOOL Repaint) {
    (void) Win;
    (void) x;
    (void) y;
    (void) Width;
    (void) Height;
    (void) Repaint;
    return FALSE;
}

XString VxGetTempPath() {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) {
        tmp = "/tmp";
    }
    return XString(tmp);
}

XBOOL VxMakeDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }

    if (mkdir(path, 0777) == 0) {
        return TRUE;
    }

    if (errno != EEXIST) {
        return FALSE;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return FALSE;
    }
    return S_ISDIR(st.st_mode) ? TRUE : FALSE;
}

XBOOL VxRemoveDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }
    return rmdir(path) == 0;
}

XBOOL VxDeleteDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }

    return RemoveDirectoryTree(path);
}

XBOOL VxGetCurrentDirectory(char *path) {
    if (!path) {
        return FALSE;
    }
    return getcwd(path, _MAX_PATH) != NULL;
}

XBOOL VxSetCurrentDirectory(const char *path) {
    if (!path) {
        return FALSE;
    }
    return chdir(path) == 0;
}

XBOOL VxMakePath(char *fullpath, const char *path, const char *file) {
    if (!fullpath || !path || !file) {
        return FALSE;
    }

    const size_t pathLen = strlen(path);
    const size_t fileLen = strlen(file);
    const bool needSep = (pathLen > 0 && path[pathLen - 1] != '/');
    const size_t totalLen = pathLen + (needSep ? 1 : 0) + fileLen;
    if (totalLen >= _MAX_PATH) {
        return FALSE;
    }

    size_t pos = 0;
    if (pathLen) {
        memcpy(fullpath, path, pathLen);
        pos = pathLen;
    }
    if (needSep) {
        fullpath[pos++] = '/';
    }
    if (fileLen) {
        memcpy(fullpath + pos, file, fileLen);
    }
    fullpath[pos + fileLen] = '\0';
    return TRUE;
}

XBOOL VxTestDiskSpace(const char *dir, size_t size) {
    if (!dir) {
        return FALSE;
    }

    struct statvfs fs;
    if (statvfs(dir, &fs) != 0) {
        return FALSE;
    }

    unsigned long long available = static_cast<unsigned long long>(fs.f_bavail) * static_cast<unsigned long long>(fs.f_frsize);
    return available >= size;
}

int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType) {
    (void) hWnd;
    (void) uType;
    fprintf(stderr, "%s: %s\n", lpCaption ? lpCaption : "VxMessage", lpText ? lpText : "");
    return 0;
}

size_t VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, size_t StringSize) {
    if (!string || StringSize == 0) {
        return 0;
    }

    const char *resolvedPath = NULL;

#if defined(__linux__) && defined(RTLD_DI_LINKMAP)
    if (Handle) {
        struct link_map *linkMap = NULL;
        if (dlinfo(Handle, RTLD_DI_LINKMAP, &linkMap) == 0
            && linkMap
            && linkMap->l_name
            && linkMap->l_name[0] != '\0') {
            resolvedPath = linkMap->l_name;
        }
    }
#else
    (void) Handle;
#endif

    char localPath[PATH_MAX] = {};
    if (!resolvedPath) {
#if defined(__linux__)
        const ssize_t len = readlink("/proc/self/exe", localPath, sizeof(localPath) - 1);
        if (len > 0) {
            localPath[len] = '\0';
            resolvedPath = localPath;
        }
#elif defined(__APPLE__)
        uint32_t size = static_cast<uint32_t>(sizeof(localPath));
        if (_NSGetExecutablePath(localPath, &size) == 0) {
            resolvedPath = localPath;
        }
#endif
    }

    if (!resolvedPath) {
        string[0] = '\0';
        return 0;
    }

    return CopyModulePathToBuffer(resolvedPath, string, StringSize);
}

INSTANCE_HANDLE VxGetModuleHandle(const char *filename) {
    if (!filename) {
        return dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    }
    return dlopen(filename, RTLD_NOW | RTLD_LOCAL);
}

XBOOL VxCreateFileTree(const char *file) {
    if (!file || !*file) {
        return FALSE;
    }

    XString parent;
    if (!CopyParentPath(file, parent)) {
        return FALSE;
    }
    if (parent.Length() == 0) {
        return TRUE;
    }

    return CreateDirectoryTree(parent.CStr());
}

XDWORD VxURLDownloadToCacheFile(const char *File, char *CachedFile, int szCachedFile) {
    (void) File;
    if (CachedFile && szCachedFile > 0) {
        CachedFile[0] = '\0';
    }
    // URL download is not implemented on non-Windows.
    return 0x80004005u;
}

BITMAP_HANDLE VxCreateBitmap(const VxImageDescEx &desc) {
    (void) desc;
    return NULL;
}

void VxDeleteBitmap(BITMAP_HANDLE Bitmap) {
    (void) Bitmap;
}

XBYTE *VxConvertBitmap(BITMAP_HANDLE Bitmap, VxImageDescEx &desc) {
    (void) Bitmap;
    (void) desc;
    return NULL;
}

BITMAP_HANDLE VxConvertBitmapTo24(BITMAP_HANDLE Bitmap) {
    (void) Bitmap;
    return NULL;
}

XBOOL VxCopyBitmap(BITMAP_HANDLE Bitmap, const VxImageDescEx &desc) {
    (void) Bitmap;
    (void) desc;
    return FALSE;
}

VX_OSINFO VxGetOs() {
#if defined(__APPLE__)
    return VXOS_MACOSX;
#elif defined(__ANDROID__)
    return VXOS_LINUXX86;
#elif defined(__FreeBSD__)
    return VXOS_LINUXX86;
#elif defined(__linux__)
    return VXOS_LINUXX86;
#else
    return VXOS_UNKNOWN;
#endif
}

FONT_HANDLE VxCreateFont(const char *FontName, int FontSize, int Weight, XBOOL italic, XBOOL underline) {
    (void) FontName;
    (void) FontSize;
    (void) Weight;
    (void) italic;
    (void) underline;
    return NULL;
}

XBOOL VxGetFontInfo(FONT_HANDLE Font, VXFONTINFO &desc) {
    (void) Font;
    desc.FaceName = "";
    desc.Height = 0;
    desc.Weight = 0;
    desc.Italic = FALSE;
    desc.Underline = FALSE;
    return FALSE;
}

XBOOL VxDrawBitmapText(BITMAP_HANDLE Bitmap, FONT_HANDLE Font, const char *string, CKRECT *rect, XDWORD Align, XDWORD BkColor, XDWORD FontColor) {
    (void) Bitmap;
    (void) Font;
    (void) string;
    (void) rect;
    (void) Align;
    (void) BkColor;
    (void) FontColor;
    return FALSE;
}

void VxDeleteFont(FONT_HANDLE Font) {
    (void) Font;
}
