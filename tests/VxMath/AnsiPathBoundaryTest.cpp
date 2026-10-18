#include "VxMath.h"
#include "VxConfiguration.h"
#include "CKPathSplitter.h"
#include "CKDirectoryParser.h"
#include <gtest/gtest.h>
#include <SDL3/SDL.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

// Native setup and verification deliberately bypass the Vx path functions.
static XBOOL EncodeTestPath(const char *native, XString &ansi) {
#ifdef _WIN32
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, native, -1, NULL, 0);
    if (!length) return FALSE;
    XArray<wchar_t> wide;
    wide.Resize(length);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, native, -1, wide.Begin(), length)) return FALSE;
    BOOL replaced = FALSE;
    UINT codePage = GetACP();
    DWORD flags = codePage == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL *used = codePage == CP_UTF8 ? NULL : &replaced;
    int count = WideCharToMultiByte(codePage, flags, wide.Begin(), -1, NULL, 0, NULL, used);
    if (!count || replaced) return FALSE;
    XArray<char> bytes;
    bytes.Resize(count);
    if (!WideCharToMultiByte(codePage, flags, wide.Begin(), -1, bytes.Begin(), count, NULL, used) || replaced) return FALSE;
    ansi = bytes.Begin();
#else
    ansi = native;
#endif
    return TRUE;
}

class PathTestEnvironment {
public:
    PathTestEnvironment(const char *name, const char *native) : Name(name), HadValue(FALSE) {
#ifdef _WIN32
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
        WideName.Resize(length);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, WideName.Begin(), length);
        DWORD count = GetEnvironmentVariableW(WideName.Begin(), NULL, 0);
        if (count) {
            Previous.Resize((int)count);
            HadValue = GetEnvironmentVariableW(WideName.Begin(), Previous.Begin(), count) < count;
        }
        length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, native, -1, NULL, 0);
        XArray<wchar_t> value;
        value.Resize(length);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, native, -1, value.Begin(), length);
        EXPECT_TRUE(SetEnvironmentVariableW(WideName.Begin(), value.Begin()));
#else
        const char *old = getenv(name);
        if (old) { Previous = old; HadValue = TRUE; }
        EXPECT_EQ(0, setenv(name, native, 1));
#endif
    }
    ~PathTestEnvironment() {
#ifdef _WIN32
        SetEnvironmentVariableW(WideName.Begin(), HadValue ? Previous.Begin() : NULL);
#else
        if (HadValue) setenv(Name.CStr(), Previous.CStr(), 1);
        else unsetenv(Name.CStr());
#endif
    }
private:
    XString Name;
    XBOOL HadValue;
#ifdef _WIN32
    XArray<wchar_t> WideName, Previous;
#else
    XString Previous;
#endif
};

static void RemoveNativeTestDirectory(const char *directory);
static SDL_EnumerationResult SDLCALL RemoveNativeTestEntry(void *, const char *directory, const char *name) {
    XString path(directory);
    path << name;
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path.CStr(), &info) && info.type == SDL_PATHTYPE_DIRECTORY)
        RemoveNativeTestDirectory(path.CStr());
    else
        SDL_RemovePath(path.CStr());
    return SDL_ENUM_CONTINUE;
}
static void RemoveNativeTestDirectory(const char *directory) {
    SDL_EnumerateDirectory(directory, RemoveNativeTestEntry, NULL);
    SDL_RemovePath(directory);
}

class AnsiPathBoundaryTest : public ::testing::Test {
protected:
    XString Root, NativeDirectory, Directory;
    void SetUp() override {
        char suffix[80];
        snprintf(suffix, sizeof(suffix), "VxAnsiPathTest-%llu/", (unsigned long long)SDL_GetPerformanceCounter());
        Root = SDL_GetBasePath();
        Root << suffix;
        ASSERT_TRUE(SDL_CreateDirectory(Root.CStr()));
        NativeDirectory = Root;
        NativeDirectory << u8"\u4E2D\u6587";
        ASSERT_TRUE(SDL_CreateDirectory(NativeDirectory.CStr()));
    }
    void TearDown() override { RemoveNativeTestDirectory(Root.CStr()); }
    XBOOL PrepareDirectory() {
        XBOOL representable = EncodeTestPath(NativeDirectory.CStr(), Directory);
#ifdef _WIN32
        RecordProperty("AnsiCodePage", (int)GetACP());
#else
        EXPECT_TRUE(representable);
#endif
        return representable;
    }
    static void WriteNative(const char *path) {
        const unsigned char bytes[] = {0, 0x12, 0x34, 0xFF, 0x56};
        SDL_IOStream *file = SDL_IOFromFile(path, "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(sizeof(bytes), SDL_WriteIO(file, bytes, sizeof(bytes)));
        EXPECT_TRUE(SDL_CloseIO(file));
    }
    static void CheckNative(const char *path) {
        const unsigned char expected[] = {0, 0x12, 0x34, 0xFF, 0x56};
        unsigned char bytes[sizeof(expected)] = {};
        SDL_IOStream *file = SDL_IOFromFile(path, "rb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(sizeof(bytes), SDL_ReadIO(file, bytes, sizeof(bytes)));
        EXPECT_EQ(0, memcmp(expected, bytes, sizeof(bytes)));
        EXPECT_TRUE(SDL_CloseIO(file));
    }
};

TEST_F(AnsiPathBoundaryTest, ChineseDirectoryIsCreatedAtTheNativeLocation) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory;
    native << u8"/\u65B0\u5EFA";
    XString path;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), path));
    ASSERT_TRUE(VxMakeDirectory(path.CStr()));
    SDL_PathInfo info;
    ASSERT_TRUE(SDL_GetPathInfo(native.CStr(), &info));
    EXPECT_EQ(SDL_PATHTYPE_DIRECTORY, info.type);
    EXPECT_TRUE(VxDirectoryExists(path.CStr()));
    EXPECT_TRUE(VxRemoveDirectory(path.CStr()));
    EXPECT_FALSE(SDL_GetPathInfo(native.CStr(), &info));
}

TEST_F(AnsiPathBoundaryTest, ChineseFileMappingKeepsBytesAndReleasesTheSource) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory;
    native << u8"/\u6570\u636E.bin";
    WriteNative(native.CStr());
    XString path;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), path));
    const unsigned char expected[] = {0, 0x12, 0x34, 0xFF, 0x56};
    for (int repeat = 0; repeat < 3; ++repeat) {
        VxMemoryMappedFile file(path.CStr());
        ASSERT_TRUE(file.IsValid());
        ASSERT_EQ(sizeof(expected), file.GetFileSize());
        EXPECT_EQ(0, memcmp(expected, file.GetBase(), sizeof(expected)));
    }
    EXPECT_TRUE(VxDeleteFile(path.CStr()));
    SDL_PathInfo info;
    EXPECT_FALSE(SDL_GetPathInfo(native.CStr(), &info));
}

struct AnsiDirectoryEntries {
    XClassArray<XString> Names;
};
static XBOOL CollectAnsiEntry(const VxDirectoryEntry *entry, void *data) {
    ((AnsiDirectoryEntries *)data)->Names.PushBack(entry->Name);
    return TRUE;
}

TEST_F(AnsiPathBoundaryTest, CopyListAndDeleteUseTheSameAnsiNames) {
    if (!PrepareDirectory()) return;
    XString source = NativeDirectory, target = NativeDirectory;
    source << u8"/\u6E90.bin";
    target << u8"/\u526F\u672C.bin";
    WriteNative(source.CStr());
    XString from, to, name;
    ASSERT_TRUE(EncodeTestPath(source.CStr(), from));
    ASSERT_TRUE(EncodeTestPath(target.CStr(), to));
    ASSERT_TRUE(EncodeTestPath(u8"\u526F\u672C.bin", name));
    EXPECT_TRUE(VxFileExists(from.CStr()));
    ASSERT_TRUE(VxCopyFile(from.CStr(), to.CStr(), TRUE));
    CheckNative(target.CStr());
    EXPECT_FALSE(VxCopyFile(from.CStr(), to.CStr(), TRUE));
    CheckNative(target.CStr());
    AnsiDirectoryEntries entries;
    ASSERT_TRUE(VxListDirectory(Directory.CStr(), "*.bin", FALSE, CollectAnsiEntry, &entries));
    EXPECT_EQ(2, entries.Names.Size());
    EXPECT_NE(entries.Names.End(), entries.Names.Find(name));
    EXPECT_TRUE(VxDeleteDirectory(Directory.CStr()));
    SDL_PathInfo info;
    EXPECT_FALSE(SDL_GetPathInfo(NativeDirectory.CStr(), &info));
}

TEST_F(AnsiPathBoundaryTest, ChineseTempPathCanBeJoinedReadAndCleaned) {
    XBOOL representable = PrepareDirectory();
    PathTestEnvironment tmp("TMP", NativeDirectory.CStr());
    PathTestEnvironment temp("TEMP", NativeDirectory.CStr());
    PathTestEnvironment tmpdir("TMPDIR", NativeDirectory.CStr());
    XString path = VxGetTempPath();
    if (!representable) return;
    ASSERT_FALSE(path.Empty());
    ASSERT_TRUE(VxDirectoryExists(path.CStr()));
    XString name, file;
    ASSERT_TRUE(EncodeTestPath(u8"\u8D44\u6E90.bin", name));
    ASSERT_TRUE(VxMakePath(file, path.CStr(), name.CStr()));
    XString native = NativeDirectory;
    native << u8"/\u8D44\u6E90.bin";
    WriteNative(native.CStr());
    ASSERT_TRUE(VxFileExists(file.CStr()));
    { VxMemoryMappedFile mapped(file.CStr()); ASSERT_TRUE(mapped.IsValid()); EXPECT_EQ(5, mapped.GetFileSize()); }
    ASSERT_TRUE(VxDeleteFile(file.CStr()));
    SDL_PathInfo info;
    EXPECT_FALSE(SDL_GetPathInfo(native.CStr(), &info));
}

TEST_F(AnsiPathBoundaryTest, TempPathKeepsPlatformBytes) {
    XString native = Root;
    native << u8"\U0001F642";
    ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    PathTestEnvironment tmp("TMP", native.CStr());
    PathTestEnvironment temp("TEMP", native.CStr());
    PathTestEnvironment tmpdir("TMPDIR", native.CStr());
    XString path = VxGetTempPath();
#ifdef _WIN32
    char expected[2048] = {};
    DWORD length = GetTempPathA(sizeof(expected), expected);
    ASSERT_GT(length, 0u);
    ASSERT_LT(length, sizeof(expected));
    EXPECT_STREQ(expected, path.CStr());
#else
    EXPECT_EQ(native, path);
    EXPECT_TRUE(VxDirectoryExists(path.CStr()));
#endif
}

TEST_F(AnsiPathBoundaryTest, ConfigurationDirectoryKeepsAnAnsiReturnValue) {
    if (!PrepareDirectory()) return;
    PathTestEnvironment overridePath("BALLANCE_USER_CONFIG_HOME", NativeDirectory.CStr());
    XString name, path;
    ASSERT_TRUE(EncodeTestPath(u8"VxAnsiTest-\u914D\u7F6E", name));
    ASSERT_TRUE(VxGetUserConfigPath(name.CStr(), path));
    EXPECT_TRUE(VxDirectoryExists(path.CStr()));
    XString native = NativeDirectory;
    native << u8"/VxAnsiTest-\u914D\u7F6E";
    SDL_PathInfo info;
    ASSERT_TRUE(SDL_GetPathInfo(native.CStr(), &info));
    EXPECT_EQ(SDL_PATHTYPE_DIRECTORY, info.type);
    XString expected;
    native << '/';
    ASSERT_TRUE(EncodeTestPath(native.CStr(), expected));
    EXPECT_EQ(expected, path);
}

TEST_F(AnsiPathBoundaryTest, DbcsTrailByteIsNotAPathSeparator) {
    if (!PrepareDirectory()) return;
    // CP936 encodes U+4E57 as 0x81 0x5C; 0x5C is part of the character.
    XString native = NativeDirectory;
    native << u8"/\u4E57";
    XString parent, name, file;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), parent));
    ASSERT_TRUE(EncodeTestPath(u8"\u4E57.bin", name));
    ASSERT_TRUE(VxMakePath(file, parent.CStr(), name.CStr()));
    ASSERT_TRUE(VxCreateFileTree(file.CStr()));
    native << u8"/\u4E57.bin";
    WriteNative(native.CStr());
    VxMemoryMappedFile mapped(file.CStr());
    ASSERT_TRUE(mapped.IsValid());
    EXPECT_EQ(5, mapped.GetFileSize());
    CKPathSplitter split(file.CStr());
    XString stem;
    ASSERT_TRUE(EncodeTestPath(u8"\u4E57", stem));
    EXPECT_STREQ(stem.CStr(), split.GetName());
    EXPECT_STREQ(".bin", split.GetExtension());
}

TEST_F(AnsiPathBoundaryTest, ChineseTempConfigurationRoundTripPreservesValueBytes) {
    if (!PrepareDirectory()) return;
    PathTestEnvironment tmp("TMP", NativeDirectory.CStr());
    PathTestEnvironment temp("TEMP", NativeDirectory.CStr());
    PathTestEnvironment tmpdir("TMPDIR", NativeDirectory.CStr());
    XString name, file;
    ASSERT_TRUE(EncodeTestPath(u8"\u914D\u7F6E.cfg", name));
    ASSERT_TRUE(VxMakePath(file, VxGetTempPath().CStr(), name.CStr()));
    const char value[] = "\xD6\xD0\xCE\xC4";
    VxConfiguration original;
    ASSERT_TRUE(original.AddEntry(NULL, "LegacyValue", value));
    ASSERT_TRUE(original.SaveToFile(file.CStr()));
    XString native = NativeDirectory;
    native << u8"/\u914D\u7F6E.cfg";
    size_t size = 0;
    char *bytes = (char *)SDL_LoadFile(native.CStr(), &size);
    ASSERT_NE(nullptr, bytes);
    EXPECT_NE(nullptr, strstr(bytes, value));
    SDL_free(bytes);
    VxConfiguration reopened;
    XString error;
    int line = 0;
    ASSERT_TRUE(reopened.BuildFromFile(file.CStr(), line, error)) << error.CStr();
    VxConfigurationEntry *entry = reopened.GetEntry("LegacyValue", FALSE);
    ASSERT_NE(nullptr, entry);
    EXPECT_STREQ(value, entry->GetValue());
    EXPECT_TRUE(VxDeleteFile(file.CStr()));
}

class RestorePathTestDirectory {
public:
    explicit RestorePathTestDirectory(const XString &path) : Previous(path) {}
    ~RestorePathTestDirectory() { EXPECT_TRUE(VxSetCurrentDirectory(Previous.CStr())); }
private:
    XString Previous;
};

TEST_F(AnsiPathBoundaryTest, CurrentDirectoryReturnsAnsiWithoutAnExtraSeparator) {
    if (!PrepareDirectory()) return;
    XString previous = VxGetCurrentDirectory();
    ASSERT_FALSE(previous.Empty());
    RestorePathTestDirectory restore(previous);
    ASSERT_TRUE(VxSetCurrentDirectory(Directory.CStr()));
    XString native = NativeDirectory;
#ifdef _WIN32
    for (char *p = native.Str(); *p; ++p)
        if (*p == '/') *p = '\\';
#endif
    XString expected;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), expected));
    EXPECT_EQ(expected, VxGetCurrentDirectory());
    char buffer[2048] = {};
    ASSERT_TRUE(VxGetCurrentDirectory(buffer, sizeof(buffer)));
    EXPECT_STREQ(expected.CStr(), buffer);
    char small[] = "unchanged";
    EXPECT_FALSE(VxGetCurrentDirectory(small, sizeof(small)));
    EXPECT_STREQ("unchanged", small);
    XString filename = NativeDirectory;
    filename << "/relative.bin";
    WriteNative(filename.CStr());
    VxMemoryMappedFile mapped("relative.bin");
    ASSERT_TRUE(mapped.IsValid());
    EXPECT_EQ(5, mapped.GetFileSize());
}

TEST_F(AnsiPathBoundaryTest, ApplicationAndModulePathsReturnAnsiBytes) {
    XString expected;
    ASSERT_TRUE(EncodeTestPath(SDL_GetBasePath(), expected));
    char path[2048] = {};
    ASSERT_TRUE(VxGetApplicationBasePath(path, sizeof(path)));
    EXPECT_STREQ(expected.CStr(), path);
    EXPECT_TRUE(VxDirectoryExists(path));
#ifdef _WIN32
    expected << "AnsiPathBoundaryTest.exe";
#endif
    EXPECT_EQ(expected, VxGetModuleFileName(NULL));
    EXPECT_EQ(expected.Length(), VxGetModuleFileName(NULL, path, sizeof(path)));
    EXPECT_STREQ(expected.CStr(), path);
}

TEST_F(AnsiPathBoundaryTest, SharedLibraryLoadsFromTheChineseDirectory) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory;
    native << u8"/\u52A8\u6001\u5E93.bin";
    ASSERT_TRUE(SDL_CopyFile(VX_PATH_TEST_LIBRARY, native.CStr())) << SDL_GetError();
    XString path;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), path));
    VxSharedLibrary library;
    ASSERT_NE(nullptr, library.Load(path.CStr())) << SDL_GetError();
    typedef int (SDLCALL *GetVersionFunction)();
    GetVersionFunction version = (GetVersionFunction)library.GetFunctionPtr("SDL_GetVersion");
    EXPECT_NE(nullptr, version);
    if (version) EXPECT_EQ(SDL_GetVersion(), version());
    library.ReleaseLibrary();
    EXPECT_TRUE(VxDeleteFile(path.CStr()));
    SDL_PathInfo info;
    EXPECT_FALSE(SDL_GetPathInfo(native.CStr(), &info));
}

#ifdef _WIN32
TEST_F(AnsiPathBoundaryTest, InvalidAnsiCannotCreateOrOpenAPath) {
    if (!IsDBCSLeadByteEx(GetACP(), 0x81)) return;
    if (!PrepareDirectory()) return;
    XString invalid = Directory;
    invalid << "/invalid-\x81";
    EXPECT_FALSE(VxMakeDirectory(invalid.CStr()));
    EXPECT_FALSE(VxFileExists(invalid.CStr()));
    EXPECT_FALSE(VxDirectoryExists(invalid.CStr()));
    VxMemoryMappedFile mapped(invalid.CStr());
    EXPECT_FALSE(mapped.IsValid());
    VxSharedLibrary library;
    EXPECT_EQ(nullptr, library.Load(invalid.CStr()));
}
#endif

TEST_F(AnsiPathBoundaryTest, RecursiveDirectoryParserReturnsReusableAnsiPaths) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory;
    native << u8"/\u4E57";
    ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    native << u8"/\u6570\u636E.bin";
    WriteNative(native.CStr());
    XString expected, mask, start = Directory;
    ASSERT_TRUE(EncodeTestPath(native.CStr(), expected));
    ASSERT_TRUE(EncodeTestPath(u8"\u6570\u636E.*", mask));
#ifdef _WIN32
    XString normalized = native;
    for (char *p = normalized.Str(); *p; ++p)
        if (*p == '/') *p = '\\';
    ASSERT_TRUE(EncodeTestPath(normalized.CStr(), expected));
    normalized = NativeDirectory;
    for (char *p = normalized.Str(); *p; ++p)
        if (*p == '/') *p = '\\';
    ASSERT_TRUE(EncodeTestPath(normalized.CStr(), start));
#endif
    CKDirectoryParser parser(start.CStr(), mask.CStr(), TRUE);
    const char *file = parser.GetNextFile();
    ASSERT_NE(nullptr, file);
    EXPECT_STREQ(expected.CStr(), file);
    VxMemoryMappedFile mapped(file);
    ASSERT_TRUE(mapped.IsValid());
    EXPECT_EQ(5, mapped.GetFileSize());
    EXPECT_EQ(nullptr, parser.GetNextFile());
    parser.Reset(NULL, NULL, TRUE);
    file = parser.GetNextFile();
    ASSERT_NE(nullptr, file);
    EXPECT_STREQ(expected.CStr(), file);
}

TEST_F(AnsiPathBoundaryTest, AsciiConfigurationPreservesExtendedValueBytes) {
    XString native = Root; native << "legacy.cfg";
    XString filename; ASSERT_TRUE(EncodeTestPath(native.CStr(), filename));
    const char western[] = "caf\xE9-\xE7";
    const char chinese[] = "\xD6\xD0\xCE\xC4";
    VxConfiguration original;
    ASSERT_TRUE(original.AddEntry(NULL, "Western", western));
    ASSERT_TRUE(original.AddEntry(NULL, "Chinese", chinese));
    ASSERT_TRUE(original.SaveToFile(filename.CStr()));
    size_t size = 0;
    char *bytes = (char *)SDL_LoadFile(native.CStr(), &size);
    ASSERT_NE(nullptr, bytes);
    EXPECT_NE(nullptr, strstr(bytes, western));
    EXPECT_NE(nullptr, strstr(bytes, chinese));
    SDL_free(bytes);
    VxConfiguration reopened;
    XString error; int line = 0;
    ASSERT_TRUE(reopened.BuildFromFile(filename.CStr(), line, error)) << error.CStr();
    ASSERT_NE(nullptr, reopened.GetEntry("Western", FALSE));
    ASSERT_NE(nullptr, reopened.GetEntry("Chinese", FALSE));
    EXPECT_STREQ(western, reopened.GetEntry("Western", FALSE)->GetValue());
    EXPECT_STREQ(chinese, reopened.GetEntry("Chinese", FALSE)->GetValue());
}

TEST_F(AnsiPathBoundaryTest, LibrarySearchPathPreservesTheExistingNativeValue) {
#ifdef _WIN32
    const char *name = "PATH";
    const char separator = ';';
#else
    const char *name = "LD_LIBRARY_PATH";
    const char separator = ':';
#endif
    const char *previous = u8"previous-\U0001F642";
    PathTestEnvironment environment(name, previous);
    XString path; ASSERT_TRUE(EncodeTestPath(Root.CStr(), path));
    VxAddLibrarySearchPath(path.CStr());
    XString expected = Root; expected << separator << previous;
#ifdef _WIN32
    wchar_t actual[2048] = {}, wide[2048] = {};
    DWORD length = GetEnvironmentVariableW(L"PATH", actual, 2048);
    ASSERT_GT(length, 0u); ASSERT_LT(length, 2048u);
    ASSERT_GT(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, expected.CStr(), -1, wide, 2048), 0);
    EXPECT_STREQ(wide, actual);
#else
    EXPECT_STREQ(expected.CStr(), getenv(name));
#endif
}

#ifndef _WIN32
TEST_F(AnsiPathBoundaryTest, NativeFileOperationsKeepUtf8Names) {
    const char *names[] = {u8"Fran\u00E7ois", u8"caf\u00E9", u8"\u4E2D\u6587", u8"\U0001F642"};
    for (int i = 0; i < 4; ++i) {
        XString directory = Root; directory << names[i];
        ASSERT_TRUE(VxMakeDirectory(directory.CStr()));
        SDL_PathInfo info;
        ASSERT_TRUE(SDL_GetPathInfo(directory.CStr(), &info));
        EXPECT_EQ(SDL_PATHTYPE_DIRECTORY, info.type);
        XString source = directory; source << u8"/caf\u00E9.bin";
        WriteNative(source.CStr());
        EXPECT_TRUE(VxFileExists(source.CStr()));
        XString copy = directory; copy << "/copy.bin";
        ASSERT_TRUE(VxCopyFile(source.CStr(), copy.CStr(), TRUE));
        CheckNative(copy.CStr());
        { VxMemoryMappedFile mapped(source.CStr()); ASSERT_TRUE(mapped.IsValid()); EXPECT_EQ(5, mapped.GetFileSize()); }
        EXPECT_TRUE(VxTestDiskSpace(directory.CStr(), 1));
        XString nested = directory; nested << u8"/Fran\u00E7ois/sub/file.bin";
        EXPECT_TRUE(VxCreateFileTree(nested.CStr()));
        XString libraryPath = directory; libraryPath << "/library.bin";
        ASSERT_TRUE(SDL_CopyFile(VX_PATH_TEST_LIBRARY, libraryPath.CStr()));
        VxSharedLibrary library;
        ASSERT_NE(nullptr, library.Load(libraryPath.CStr())) << SDL_GetError();
        EXPECT_NE(nullptr, library.GetFunctionPtr("SDL_GetVersion"));
        library.ReleaseLibrary();
        EXPECT_TRUE(VxDeleteFile(source.CStr()));
        EXPECT_TRUE(VxDeleteDirectory(directory.CStr()));
        EXPECT_FALSE(SDL_GetPathInfo(directory.CStr(), &info));
    }
}

TEST_F(AnsiPathBoundaryTest, DirectoryEnumerationKeepsEveryNativeName) {
    const char *names[] = {"ascii.bin", u8"Fran\u00E7ois.bin", u8"caf\u00E9.bin", u8"\U0001F642.bin"};
    for (int i = 0; i < 4; ++i) {
        XString path = Root; path << names[i]; WriteNative(path.CStr());
    }
    AnsiDirectoryEntries entries;
    ASSERT_TRUE(VxListDirectory(Root.CStr(), "*.bin", FALSE, CollectAnsiEntry, &entries));
    ASSERT_EQ(4, entries.Names.Size());
    for (int i = 0; i < 4; ++i) EXPECT_NE(entries.Names.End(), entries.Names.Find(XString(names[i])));
    entries.Names.Clear();
    CKDirectoryParser parser(Root.CStr(), "*.bin", TRUE);
    while (const char *file = parser.GetNextFile()) {
        CKPathSplitter split(file);
        XString name = split.GetName(); name << split.GetExtension();
        entries.Names.PushBack(name);
        VxMemoryMappedFile mapped(file); EXPECT_TRUE(mapped.IsValid());
    }
    ASSERT_EQ(4, entries.Names.Size());
    for (int i = 0; i < 4; ++i) EXPECT_NE(entries.Names.End(), entries.Names.Find(XString(names[i])));
}

TEST_F(AnsiPathBoundaryTest, NativeTempAndConfigurationPathsKeepUtf8Bytes) {
    XString native = Root; native << u8"Fran\u00E7ois";
    ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    PathTestEnvironment tmp("TMP", native.CStr()), temp("TEMP", native.CStr()), tmpdir("TMPDIR", native.CStr());
    PathTestEnvironment config("BALLANCE_USER_CONFIG_HOME", native.CStr());
    EXPECT_EQ(native, VxGetTempPath());
    XString previous = VxGetCurrentDirectory(); ASSERT_FALSE(previous.Empty());
    RestorePathTestDirectory restore(previous);
    ASSERT_TRUE(VxSetCurrentDirectory(native.CStr()));
    EXPECT_EQ(native, VxGetCurrentDirectory());
    XString path;
    ASSERT_TRUE(VxGetUserConfigPath(u8"caf\u00E9", path));
    XString expected = native; expected << u8"/caf\u00E9/";
    EXPECT_EQ(expected, path);
    EXPECT_TRUE(VxDirectoryExists(path.CStr()));
}
#endif
