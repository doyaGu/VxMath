#include "VxWindowFunctions.h"

#include <Windows.h>

#include <direct.h>
#include <shellapi.h>

#include "XString.h"
#include "VxImageDescEx.h"
#include "VxSharedLibrary.h"

char VxScanCodeToAscii(XULONG scancode, unsigned char keystate[256])
{
    unsigned char state[256];

    GetKeyboardState(state);
    if (keystate[VK_PRINT])
    {
        state[VK_SHIFT] |= 0x81; // 0x81: the key is down and toggled
        state[VK_LSHIFT] |= 0x81;
    }
    if (keystate[0x36]) // VK_6
    {
        state[VK_SHIFT] |= 0x81;
        state[VK_RSHIFT] |= 0x81;
    }
    if (keystate[0x38]) // VK_8
    {
        state[VK_MENU] |= 0x81;
        state[VK_LMENU] |= 0x81;
    }
    if (keystate[0xB8]) // reserved
    {
        state[VK_CONTROL] |= 0x81;
        state[VK_MENU] |= 0x81;
        state[VK_LCONTROL] |= 0x81;
        state[VK_RMENU] |= 0x81;
    }
    if (keystate[VK_NONCONVERT])
    {
        state[VK_CONTROL] |= 0x81;
        state[VK_LCONTROL] |= 0x81;
    }
    if (keystate[0x9D]) // unassigned
    {
        state[VK_CONTROL] |= 0x81;
        state[VK_RCONTROL] |= 0x81;
    }
    UINT vkey = MapVirtualKeyA(scancode, MAPVK_VSC_TO_VK);
    WORD ch;
    int ret = ToAscii(vkey, scancode, state, &ch, 0);
    return (ret != 0) ? ch : '\0';
}

int VxScanCodeToName(XULONG scancode, char *keyName)
{
    DWORD code = (scancode & 0x7F) << 16;
    if (scancode > 0x7F)
        code |= 0x1000000;

    char name[32];
    int len = GetKeyNameTextA(code, name, 32);
    if (len > 0)
        strcpy(keyName, name);
    return len + 1;
}

int VxShowCursor(XBOOL show)
{
    return ShowCursor(show);
}

XBOOL VxSetCursor(VXCURSOR_POINTER cursorID)
{
    HCURSOR cursorNew = NULL;
    HCURSOR cursorNow = GetCursor();
    switch (cursorID)
    {
    case 1:
        cursorNew = LoadCursorA(0, (LPCSTR)IDC_ARROW);
        break;
    case 2:
        cursorNew = LoadCursorA(0, (LPCSTR)IDC_WAIT);
        break;
    case 3:
        cursorNew = LoadCursorA(0, (LPCSTR)IDC_SIZEALL);
        break;
    default:
        return TRUE;
    }
    if (cursorNew && cursorNew != cursorNow)
        SetCursor(cursorNew);
    return TRUE;
}

XWORD VxGetFPUControlWord()
{
    return 0;
}

void VxSetFPUControlWord(XWORD Fpu)
{
}

void VxSetBaseFPUControlWord()
{
}

void VxAddLibrarySearchPath(char *path)
{
    char buffer[4096];
    GetEnvironmentVariableA("PATH", buffer, sizeof(buffer));
    XString newPath;
    newPath << path << ';' << buffer;
    if (newPath[newPath.Length() - 1] != ';')
        newPath << ';';
    SetEnvironmentVariableA("PATH", newPath.CStr());
}

XBOOL VxGetEnvironmentVariable(char *envName, XString &envValue)
{
    char buffer[4096];
    if (GetEnvironmentVariableA("PATH", buffer, sizeof(buffer)) == 0)
        return FALSE;
    envValue = buffer;
    return TRUE;
}

XBOOL VxSetEnvironmentVariable(char *envName, char *envValue)
{
    return SetEnvironmentVariableA(envName, envValue);
}

//------ Window Functions
WIN_HANDLE VxWindowFromPoint(CKPOINT pt)
{
    return (WIN_HANDLE)WindowFromPoint(*(LPPOINT)&pt);
}

XBOOL VxGetClientRect(WIN_HANDLE Win, CKRECT *rect)
{
    return GetClientRect((HWND)Win, (LPRECT)rect);
}

XBOOL VxGetWindowRect(WIN_HANDLE Win, CKRECT *rect)
{
    return GetWindowRect((HWND)Win, (LPRECT)rect);
}

XBOOL VxScreenToClient(WIN_HANDLE Win, CKPOINT *pt)
{
    return ScreenToClient((HWND)Win, (LPPOINT)pt);
}

XBOOL VxClientToScreen(WIN_HANDLE Win, CKPOINT *pt)
{
    return ClientToScreen((HWND)Win, (LPPOINT)pt);
}

WIN_HANDLE VxSetParent(WIN_HANDLE Child, WIN_HANDLE Parent)
{
    return (WIN_HANDLE)SetParent((HWND)Child, (HWND)Parent);
}

WIN_HANDLE VxGetParent(WIN_HANDLE Win)
{
    return (WIN_HANDLE)GetParent((HWND)Win);
}

XBOOL VxMoveWindow(WIN_HANDLE Win, int x, int y, int Width, int Height, XBOOL Repaint)
{
    return MoveWindow((HWND)Win, x, y, Width, Height, Repaint);
}

XString VxGetTempPath()
{
    XString path(512);
    GetTempPathA(512, path.Str());
    return path;
}

XBOOL VxMakeDirectory(char *path)
{
    return mkdir(path) == 0;
}

XBOOL VxRemoveDirectory(char *path)
{
    return RemoveDirectoryA(path);
}

XBOOL VxDeleteDirectory(char *path)
{
    VxSharedLibrary lib;
    if (!lib.Load("Shell32.dll"))
        return FALSE;

    int ret = FALSE;
    typedef int(WINAPI * LPFSHPROC)(LPSHFILEOPSTRUCTA);
    LPFSHPROC mySHFileOperation = (LPFSHPROC)lib.GetFunctionPtr("SHFileOperation");
    if (mySHFileOperation || (mySHFileOperation = (LPFSHPROC)lib.GetFunctionPtr("SHFileOperationA")))
    {
        SHFILEOPSTRUCTA fileOp;
        memset(&fileOp, 0, sizeof(SHFILEOPSTRUCTA));
        fileOp.pFrom = path;
        fileOp.wFunc = FO_DELETE;
        ret = mySHFileOperation(&fileOp) == 0;
    }
    lib.ReleaseLibrary();
    return ret;
}

XBOOL VxGetCurrentDirectory(char *path)
{
    return getcwd(path, MAX_PATH) != NULL;
}

XBOOL VxSetCurrentDirectory(char *path)
{
    return chdir(path) == 0;
}

XBOOL VxMakePath(char *fullpath, char *path, char *file)
{
    strcpy(fullpath, path);
    int pathLen = strlen(path);
    if (pathLen >= MAX_PATH)
        return FALSE;
    if (fullpath[pathLen - 1] != '\\' && fullpath[pathLen - 1] != '/')
        fullpath[pathLen] = '\\';
    strcpy(&fullpath[pathLen + 1], file);
    return TRUE;
}

XBOOL VxTestDiskSpace(const char *dir, XULONG size)
{
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    typedef BOOL(__stdcall * LPFNGETDISKFREESPACEEXA)(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
    LPFNGETDISKFREESPACEEXA lpfnGetDiskFreeSpaceExA = (LPFNGETDISKFREESPACEEXA)GetProcAddress(hKernel32, "GetDiskFreeSpaceExA");

    XBOOL ret;
    if (lpfnGetDiskFreeSpaceExA)
    {
        ULARGE_INTEGER freeBytesAvailableToCaller;
        ULARGE_INTEGER totalNumberOfBytes;
        ULARGE_INTEGER totalNumberOfFreeBytes;
        ret = lpfnGetDiskFreeSpaceExA(dir, &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes);
        return ret && size <= freeBytesAvailableToCaller.QuadPart;
    }
    else
    {
        DWORD sectorsPerCluster = 0;
        DWORD bytesPerSector = 0;
        DWORD numberOfFreeClusters = 0;
        DWORD totalNumberOfClusters = 0;
        ret = GetDiskFreeSpaceA(dir, &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters);
        return ret && size <= sectorsPerCluster * bytesPerSector * numberOfFreeClusters;
    }
}

int VxMessageBox(WIN_HANDLE hWnd, char *lpText, char *lpCaption, XULONG uType)
{
    return MessageBoxA((HWND)hWnd, lpText, lpCaption, uType);
}

XULONG VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, XULONG StringSize)
{
    return GetModuleFileNameA((HMODULE)Handle, string, StringSize);
}

INSTANCE_HANDLE VxGetModuleHandle(const char *filename)
{
    return (INSTANCE_HANDLE)GetModuleHandleA(filename);
}

XBOOL VxCreateFileTree(char *file)
{
    XString filepath = file;
    if (filepath.Length() <= 2)
        return FALSE;

    for (char *pch = &filepath[3]; *pch != '\0'; ++pch)
    {
        if (*pch != '/' && *pch != '\\')
            continue;
        *pch = '\0';
        if (GetFileAttributesA(filepath.CStr()) == -1)
            if (!CreateDirectoryA(filepath.CStr(), NULL))
                break;
        *pch = '\\';
    }
    return TRUE;
}

XULONG VxURLDownloadToCacheFile(char *File, char *CachedFile, int szCachedFile)
{
    char *cachedFile = CachedFile;
    CachedFile[0] = '\0';

    VxSharedLibrary sl;
    if (!sl.Load("Urlmon.dll"))
        return E_FAIL;

    typedef HRESULT(__stdcall * LPFNURLDOWNLOADTOCACHEFILE)(LPUNKNOWN, LPCSTR, LPTSTR, DWORD, DWORD, IBindStatusCallback *);
    LPFNURLDOWNLOADTOCACHEFILE lpfnURLDownloadToCacheFileA = (LPFNURLDOWNLOADTOCACHEFILE)sl.GetFunctionPtr("URLDownloadToCacheFileA");
    XULONG ret = E_FAIL;
    if (lpfnURLDownloadToCacheFileA)
        ret = lpfnURLDownloadToCacheFileA(NULL, File, cachedFile, szCachedFile, 0, NULL);
    sl.ReleaseLibrary();
    return ret;
}

BITMAP_HANDLE VxCreateBitmap(const VxImageDescEx &desc)
{
    return NULL;
}

XBYTE *VxConvertBitmap(BITMAP_HANDLE Bitmap, VxImageDescEx &desc)
{
    return NULL;
}

XBOOL VxCopyBitmap(BITMAP_HANDLE Bitmap, const VxImageDescEx &desc)
{
    return TRUE;
}

BITMAP_HANDLE VxConvertBitmapTo24(BITMAP_HANDLE Bitmap)
{
    return NULL;
}

VX_OSINFO VxGetOs()
{
    OSVERSIONINFOA osvi;
    memset(&osvi, 0, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);
    switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32s:
        return VXOS_WIN31;
    case VER_PLATFORM_WIN32_WINDOWS:
        if (osvi.dwMinorVersion == 0)
            return VXOS_WIN95;
        else if (osvi.dwMinorVersion < 0x5A)
            return VXOS_WIN98;
        else
            return VXOS_WINME;
    case VER_PLATFORM_WIN32_NT:
        if (osvi.dwMajorVersion == 4)
            return VXOS_WINNT4;
        if (osvi.dwMajorVersion > 4)
            return VXOS_WIN2K;
    default:
        return VXOS_UNKNOWN;
    }
}