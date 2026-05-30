#include "VxWindowFunctions.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#if defined(__linux__) && defined(RTLD_DI_LINKMAP)
#include <link.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
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

static XBOOL CopyDirectoryPartToBuffer(const char *filePath, char *output, size_t outputSize) {
    if (!filePath || !output || outputSize == 0) {
        return FALSE;
    }

    const char *lastSlash = strrchr(filePath, '/');
    if (!lastSlash) {
        return FALSE;
    }

    const size_t directoryLength = (size_t) (lastSlash - filePath) + 1;
    if (directoryLength + 1 > outputSize) {
        return FALSE;
    }

    memcpy(output, filePath, directoryLength);
    output[directoryLength] = '\0';
    return TRUE;
}

static XBOOL VxDirectoryNameMatches(const char *name, const char *mask) {
    if (!name) {
        return FALSE;
    }
    if (!mask || !*mask || strcmp(mask, "*") == 0) {
        return TRUE;
    }
    return fnmatch(mask, name, 0) == 0 ? TRUE : FALSE;
}

static XBOOL VxStatDirectoryEntry(const char *dir, const char *name, XBOOL &isDirectory, size_t &size) {
    XString fullpath(dir);
    AppendPathComponent(fullpath, name);

    struct stat st;
    if (stat(fullpath.CStr(), &st) != 0) {
        return FALSE;
    }

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

static char *VxReadCurrentExecutablePath() {
#if defined(__linux__)
    char *resolvedPath = realpath("/proc/self/exe", NULL);
    if (!resolvedPath) {
        return NULL;
    }

    const size_t length = strlen(resolvedPath);
    char *path = new char[length + 1];
    memcpy(path, resolvedPath, length + 1);
    free(resolvedPath);
    return path;
#elif defined(__APPLE__)
    uint32_t size = 1;
    char probe[1];
    _NSGetExecutablePath(probe, &size);
    if (size == 0) {
        return NULL;
    }

    char *path = new char[size + 1];
    if (_NSGetExecutablePath(path, &size) == 0) {
        path[size] = '\0';
        return path;
    }

    delete[] path;
    return NULL;
#else
    return NULL;
#endif
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

static XBOOL HasUrlScheme(const char *file) {
    if (!file) {
        return FALSE;
    }
    return strstr(file, "://") != NULL ? TRUE : FALSE;
}

static XBOOL IsFileUrl(const char *file) {
    if (!file) {
        return FALSE;
    }
    return strncasecmp(file, "file://", 7) == 0 ? TRUE : FALSE;
}

static XDWORD CopyCachedPath(const char *path, char *cachedFile, int cachedFileSize) {
    const size_t pathLen = strlen(path);
    if (pathLen >= static_cast<size_t>(cachedFileSize)) {
        cachedFile[0] = '\0';
        return 0x8007007Au;
    }
    memcpy(cachedFile, path, pathLen + 1);
    return 0;
}

static XBOOL CreateDownloadCacheFile(char *cachedFile, int cachedFileSize, XString &path, int *fd) {
    if (!cachedFile || cachedFileSize <= 0 || !fd) {
        return FALSE;
    }

    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) {
        tmp = "/tmp";
    }

    XString pattern(tmp);
    AppendPathComponent(pattern, "vxurlcache-XXXXXX");

    const size_t patternLength = strlen(pattern.CStr());
    char *localPath = new char[patternLength + 1];
    memcpy(localPath, pattern.CStr(), patternLength + 1);

    const int localFd = mkstemp(localPath);
    if (localFd < 0) {
        delete[] localPath;
        return FALSE;
    }

    if (CopyCachedPath(localPath, cachedFile, cachedFileSize) != 0) {
        close(localFd);
        unlink(localPath);
        delete[] localPath;
        return FALSE;
    }

    path = localPath;
    delete[] localPath;
    *fd = localFd;
    return TRUE;
}

static XDWORD CopyFileToCache(const char *sourcePath, char *cachedFile, int cachedFileSize) {
    if (!sourcePath || !*sourcePath) {
        return 0x80070057u;
    }

    int inFd = open(sourcePath, O_RDONLY);
    if (inFd < 0) {
        return 0x80004005u;
    }

    XString cachePath;
    int outFd = -1;
    if (!CreateDownloadCacheFile(cachedFile, cachedFileSize, cachePath, &outFd)) {
        close(inFd);
        return 0x80004005u;
    }

    char buffer[16384];
    XDWORD result = 0;
    for (;;) {
        const ssize_t readCount = read(inFd, buffer, sizeof(buffer));
        if (readCount == 0) {
            break;
        }
        if (readCount < 0) {
            result = 0x80004005u;
            break;
        }

        ssize_t written = 0;
        while (written < readCount) {
            const ssize_t writeCount = write(outFd, buffer + written, static_cast<size_t>(readCount - written));
            if (writeCount <= 0) {
                result = 0x80004005u;
                break;
            }
            written += writeCount;
        }
        if (result != 0) {
            break;
        }
    }

    close(inFd);
    close(outFd);

    if (result != 0) {
        unlink(cachePath.CStr());
        cachedFile[0] = '\0';
    }
    return result;
}

static XDWORD RunDownloader(const char *tool, char *const argv[], const char *outputPath) {
    const pid_t pid = fork();
    if (pid < 0) {
        return 0x80004005u;
    }

    if (pid == 0) {
        execvp(tool, argv);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return 0x80004005u;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        unlink(outputPath);
        return 0x80004005u;
    }

    return 0;
}

static XDWORD DownloadUrlWithExternalTool(const char *url, char *cachedFile, int cachedFileSize) {
    XString cachePath;
    int outFd = -1;
    if (!CreateDownloadCacheFile(cachedFile, cachedFileSize, cachePath, &outFd)) {
        return 0x80004005u;
    }
    close(outFd);

    char *curlArgv[] = {
        const_cast<char *>("curl"),
        const_cast<char *>("-fsSL"),
        const_cast<char *>("-o"),
        const_cast<char *>(cachePath.CStr()),
        const_cast<char *>(url),
        NULL
    };
    XDWORD result = RunDownloader("curl", curlArgv, cachePath.CStr());
    if (result == 0) {
        return 0;
    }

    char *wgetArgv[] = {
        const_cast<char *>("wget"),
        const_cast<char *>("-q"),
        const_cast<char *>("-O"),
        const_cast<char *>(cachePath.CStr()),
        const_cast<char *>(url),
        NULL
    };
    result = RunDownloader("wget", wgetArgv, cachePath.CStr());
    if (result != 0) {
        cachedFile[0] = '\0';
    }
    return result;
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

XBOOL VxFileExists(const char *path) {
    if (!path) {
        return FALSE;
    }

    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) ? TRUE : FALSE;
}

XBOOL VxDirectoryExists(const char *path) {
    if (!path) {
        return FALSE;
    }

    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode) ? TRUE : FALSE;
}

XBOOL VxListDirectory(const char *dir, const char *mask, XBOOL includeDirectories, VxDirectoryEntryCallback callback, void *userData) {
    if (!dir || !callback) {
        return FALSE;
    }

    DIR *handle = opendir(dir);
    if (!handle) {
        return FALSE;
    }

    XBOOL ok = TRUE;
    struct dirent *entryData;
    while ((entryData = readdir(handle)) != NULL) {
        if (strcmp(entryData->d_name, ".") == 0 || strcmp(entryData->d_name, "..") == 0) {
            continue;
        }
        if (!VxDirectoryNameMatches(entryData->d_name, mask)) {
            continue;
        }

        XBOOL isDirectory = FALSE;
        size_t size = 0;
#if defined(DT_DIR)
        if (!VxGetDirectoryEntryInfo(dir, entryData->d_name, entryData->d_type, isDirectory, size)) {
            continue;
        }
#else
        if (!VxStatDirectoryEntry(dir, entryData->d_name, isDirectory, size)) {
            continue;
        }
#endif

        if (isDirectory && !includeDirectories) {
            continue;
        }

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
}

XBOOL VxGetCurrentDirectory(char *path, size_t pathSize) {
    if (!path || pathSize == 0) {
        return FALSE;
    }
    return getcwd(path, pathSize) != NULL;
}

XString VxGetCurrentDirectory() {
    char *buffer = getcwd(NULL, 0);
    if (!buffer) {
        return "";
    }

    XString path(buffer);
    free(buffer);
    return path;
}

XBOOL VxGetApplicationBasePath(char *path, size_t pathSize) {
    if (!path || pathSize == 0) {
        return FALSE;
    }

    char *modulePath = VxReadCurrentExecutablePath();
    XBOOL result = CopyDirectoryPartToBuffer(modulePath, path, pathSize);
    delete[] modulePath;
    return result;
}

XBOOL VxGetUserConfigPath(const char *appName, char *path, size_t pathSize) {
    if (!path || pathSize == 0) {
        return FALSE;
    }

    XString configPath;
    if (!VxGetUserConfigPath(appName, configPath)) {
        return FALSE;
    }

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

#if defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home || !*home) {
        return FALSE;
    }
    path = home;
    AppendPathComponent(path, "Library");
    AppendPathComponent(path, "Application Support");
    AppendPathComponent(path, name);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) {
        path = xdg;
        AppendPathComponent(path, name);
    } else if (home && *home) {
        path = home;
        AppendPathComponent(path, ".config");
        AppendPathComponent(path, name);
    } else {
        return FALSE;
    }
#endif

    if (path.Length() > 0 && path[path.Length() - 1] != '/') {
        path << '/';
    }

    XString marker(path);
    AppendPathComponent(marker, "_marker_");
    VxCreateFileTree(marker.CStr());
    return TRUE;
}

XBOOL VxSetCurrentDirectory(const char *path) {
    if (!path) {
        return FALSE;
    }

    size_t len = strlen(path);
    char *normalized = new char[len + 1];

    for (size_t i = 0; i <= len; ++i) {
        normalized[i] = (path[i] == '\\') ? '/' : path[i];
    }

    XBOOL result = chdir(normalized) == 0;
    delete[] normalized;
    return result;
}

XBOOL VxMakePath(char *fullpath, size_t fullpathSize, const char *path, const char *file) {
    if (!fullpath || fullpathSize == 0 || !path || !file) {
        return FALSE;
    }

    const size_t pathLen = strlen(path);
    const size_t fileLen = strlen(file);
    const bool needSep = (pathLen > 0 && path[pathLen - 1] != '/' && path[pathLen - 1] != '\\');
    if (pathLen > ((size_t) -1) - (needSep ? 1 : 0)) {
        return FALSE;
    }
    size_t totalLen = pathLen + (needSep ? 1 : 0);
    if (fileLen > ((size_t) -1) - totalLen) {
        return FALSE;
    }
    totalLen += fileLen;
    if (totalLen + 1 > fullpathSize) {
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

XBOOL VxMakePath(XString &fullpath, const char *path, const char *file) {
    fullpath = "";
    if (!path || !file) {
        return FALSE;
    }

    const size_t pathLen = strlen(path);
    const size_t fileLen = strlen(file);
    if (pathLen > ((size_t) -1) - fileLen) {
        return FALSE;
    }
    const size_t contentSize = pathLen + fileLen;
    if (contentSize > ((size_t) -1) - 2) {
        return FALSE;
    }
    const size_t bufferSize = contentSize + 2;
    if (bufferSize > (size_t)XString::MAX_LENGTH + 1) {
        return FALSE;
    }

    char *buffer = new char[bufferSize];
    XBOOL ok = VxMakePath(buffer, bufferSize, path, file);
    if (ok) {
        fullpath = buffer;
    }
    delete[] buffer;
    return ok;
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

XString VxGetModuleFileName(INSTANCE_HANDLE Handle) {
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

    char *localPath = NULL;
    if (!resolvedPath) {
        localPath = VxReadCurrentExecutablePath();
        resolvedPath = localPath;
    }

    if (!resolvedPath) {
        delete[] localPath;
        return "";
    }

    XString result(resolvedPath);
    delete[] localPath;
    return result;
}

size_t VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, size_t StringSize) {
    if (!string || StringSize == 0) {
        return 0;
    }

    XString path = VxGetModuleFileName(Handle);
    return CopyModulePathToBuffer(path.CStr(), string, StringSize);
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
    if (!CachedFile || szCachedFile <= 0) {
        return 0x80070057u;
    }
    CachedFile[0] = '\0';

    if (!File || !*File) {
        return 0x80070057u;
    }

    if (IsFileUrl(File)) {
        XString path(File + 7);
        if (strncmp(path.CStr(), "localhost/", 10) == 0) {
            path = path.CStr() + 9;
        }
        VxUnEscapeUrl(path);
        return CopyFileToCache(path.CStr(), CachedFile, szCachedFile);
    }

    if (!HasUrlScheme(File)) {
        return CopyFileToCache(File, CachedFile, szCachedFile);
    }

    return DownloadUrlWithExternalTool(File, CachedFile, szCachedFile);
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
