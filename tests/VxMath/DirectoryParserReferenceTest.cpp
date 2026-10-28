#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef VX_DIRECTORY_PARSER_PROBE
#include "CKDirectoryParser.h"
#include <gtest/gtest.h>
#endif

struct DirectoryApi {
    void *(*Create)(char *, char *, int);
    const char *(*Next)(void *);
    void (*Reset)(void *, char *, char *, int);
    void (*Destroy)(void *);
};

struct DirectoryChecks {
    int checks = 0;
    int errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};

static char DirectorySeparator() {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static void DirectoryTestPath(char *out, const char *root, const char *leaf) {
    sprintf(out, "%s%c%s", root, DirectorySeparator(), leaf);
}

static bool DirectoryTestMkdir(const char *path) {
#if defined(_WIN32)
    return CreateDirectoryA(path, nullptr) != 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static void DirectoryTestRmdir(const char *path) {
#if defined(_WIN32)
    RemoveDirectoryA(path);
#else
    rmdir(path);
#endif
}

static bool DirectoryTestChdir(const char *path) {
#if defined(_WIN32)
    return SetCurrentDirectoryA(path) != 0;
#else
    return chdir(path) == 0;
#endif
}

static bool DirectoryTestCwd(char *path, unsigned size) {
#if defined(_WIN32)
    DWORD length = GetCurrentDirectoryA(size, path);
    return length > 0 && length < size;
#else
    return getcwd(path, size) != nullptr;
#endif
}

struct DirectoryFixture {
    char Root[512] = {};
    char Previous[512] = {};
    bool Ready = false;
    DirectoryFixture() {
        if (!DirectoryTestCwd(Previous, sizeof(Previous))) return;
#if defined(_WIN32)
        char temp[512];
        DWORD length = GetTempPathA(sizeof(temp), temp);
        if (!length || length >= sizeof(temp) || !GetTempFileNameA(temp, "vxd", 0, Root)) return;
        if (!DeleteFileA(Root) || !DirectoryTestMkdir(Root)) return;
#else
        strcpy(Root, "/tmp/vx-directory-XXXXXX");
        if (!mkdtemp(Root)) return;
#endif
        char path[640];
        DirectoryTestPath(path, Root, "child"); if (!DirectoryTestMkdir(path)) return;
        DirectoryTestPath(path, Root, "empty"); if (!DirectoryTestMkdir(path)) return;
        DirectoryTestPath(path, Root, "child/deep"); if (!DirectoryTestMkdir(path)) return;
        const char *files[] = {"a.txt", "b.LOG", "bare", "child/c.txt", "child/deep/d.txt"};
        for (const char *file : files) {
            DirectoryTestPath(path, Root, file);
            FILE *stream = fopen(path, "wb");
            if (!stream) return;
            fclose(stream);
        }
        Ready = true;
    }
    ~DirectoryFixture() {
        if (*Previous) DirectoryTestChdir(Previous);
        if (!*Root) return;
        char path[640];
        const char *files[] = {"a.txt", "b.LOG", "bare", "child/c.txt", "child/deep/d.txt", "root_probe.tmp"};
        for (const char *file : files) { DirectoryTestPath(path, Root, file); remove(path); }
        const char *dirs[] = {"child/deep", "child", "empty"};
        for (const char *dir : dirs) { DirectoryTestPath(path, Root, dir); DirectoryTestRmdir(path); }
        DirectoryTestRmdir(Root);
    }
};

struct DirectoryScan {
    const DirectoryApi &Api;
    void *Parser;
    DirectoryScan(const DirectoryApi &api, char *dir, char *mask, int recurse) : Api(api), Parser(api.Create(dir, mask, recurse)) {}
    ~DirectoryScan() { Api.Destroy(Parser); }
};

static bool DirectoryPathsEqual(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        char x = *a == '\\' ? '/' : *a, y = *b == '\\' ? '/' : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

static unsigned DirectoryKnownFile(const char *root, const char *file) {
    const char *names[] = {"a.txt", "b.LOG", "bare", "child/c.txt", "child/deep/d.txt"};
    char path[640];
    for (unsigned k = 0; k < 5; ++k) {
        DirectoryTestPath(path, root, names[k]);
        if (DirectoryPathsEqual(path, file)) return 1u << k;
    }
    return 0;
}

static unsigned DirectoryCollect(DirectoryChecks &result, const DirectoryApi &api, void *parser, const char *root, bool filesFirst = false) {
    unsigned found = 0;
    bool enteredChild = false;
    for (int count = 0; count < 32; ++count) {
        const char *file = api.Next(parser);
        if (!file) return found;
        unsigned bit = DirectoryKnownFile(root, file);
        result.Expect(bit != 0 && (found & bit) == 0);
        if (filesFirst && bit < 8) result.Expect(!enteredChild);
        if (bit >= 8) enteredChild = true;
        found |= bit;
        FILE *stream = fopen(file, "rb");
        result.Expect(stream != nullptr);
        if (stream) fclose(stream);
    }
    result.Expect(false);
    return found;
}

static DirectoryChecks DirectoryEnumeration(const DirectoryApi &api, int recurse) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "*.*";
    DirectoryScan scan(api, fixture.Root, mask, recurse);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root, true) == (recurse ? 31u : 7u));
    return result;
}

static DirectoryChecks DirectoryMaskAndEmptyChildren(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "c.*";
    DirectoryScan scan(api, fixture.Root, mask, 1);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 8);
    char other[] = "*.log";
    api.Reset(scan.Parser, fixture.Root, other, 1);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 2);
    char path[640]; DirectoryTestPath(path, fixture.Root, "empty");
    api.Reset(scan.Parser, path, mask, 1);
    result.Expect(api.Next(scan.Parser) == nullptr);
    DirectoryTestPath(path, fixture.Root, "missing");
    api.Reset(scan.Parser, path, mask, 0);
    result.Expect(api.Next(scan.Parser) == nullptr);
    return result;
}

static DirectoryChecks DirectoryTrailingAndRelative(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char path[640], mask[] = "*.txt";
    sprintf(path, "%s%c", fixture.Root, DirectorySeparator());
    DirectoryScan scan(api, path, mask, 0);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 1);
    result.Expect(DirectoryTestChdir(fixture.Root));
    char relative[] = "child";
    api.Reset(scan.Parser, relative, mask, 1);
    const char *file = api.Next(scan.Parser);
    result.Expect(file && DirectoryPathsEqual(file, "child/c.txt"));
    file = api.Next(scan.Parser);
    result.Expect(file && DirectoryPathsEqual(file, "child/deep/d.txt"));
    result.Expect(api.Next(scan.Parser) == nullptr);
    return result;
}

static DirectoryChecks DirectoryOwnsInputs(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char path[640], mask[] = "*.txt";
    strcpy(path, fixture.Root);
    DirectoryScan scan(api, path, mask, 0);
    result.Expect(strcmp(path, fixture.Root) == 0 && strcmp(mask, "*.txt") == 0);
    path[0] = 0; mask[0] = 0;
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 1);
    DirectoryTestPath(path, fixture.Root, "child"); strcpy(mask, "*.txt");
    api.Reset(scan.Parser, path, mask, 0);
    path[0] = 0; mask[0] = 0;
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 8);
    return result;
}

static DirectoryChecks DirectoryRepeatedEnd(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "a.txt";
    DirectoryScan scan(api, fixture.Root, mask, 0);
    for (int cycle = 0; cycle < 2; ++cycle) {
        const char *file = api.Next(scan.Parser);
        result.Expect(file && DirectoryKnownFile(fixture.Root, file) == 1);
        result.Expect(api.Next(scan.Parser) == nullptr);
    }
    return result;
}

static DirectoryChecks DirectoryRootSearch(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    result.Expect(DirectoryTestChdir(fixture.Root));
    char mask[128];
#if defined(_WIN32)
    sprintf(mask, "vx_root_%lu_%lu.tmp", GetCurrentProcessId(), GetTickCount());
#else
    sprintf(mask, "vx_root_%lu.tmp", static_cast<unsigned long>(getpid()));
#endif
    FILE *stream = fopen(mask, "wb");
    result.Expect(stream != nullptr); if (stream) fclose(stream);
    char absolute[160]; sprintf(absolute, "/%s", mask);
    FILE *rootFile = fopen(absolute, "rb");
    result.Expect(rootFile == nullptr);
    if (rootFile) fclose(rootFile);
    char root[] = "/";
    {
        DirectoryScan scan(api, root, mask, 0);
        result.Expect(api.Next(scan.Parser) == nullptr);
    }
#if defined(_WIN32)
    char backslash[] = "\\";
    {
        DirectoryScan scan(api, backslash, mask, 0);
        result.Expect(api.Next(scan.Parser) == nullptr);
    }
#endif
    result.Expect(remove(mask) == 0);
    return result;
}

static DirectoryChecks DirectoryResetAndDestroy(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "*.txt", childMask[] = "c.*", child[640];
    DirectoryTestPath(child, fixture.Root, "child");
#if defined(_WIN32)
    DWORD before = 0, after = 0;
    result.Expect(GetProcessHandleCount(GetCurrentProcess(), &before) != 0);
#endif
    for (int cycle = 0; cycle < 64; ++cycle) {
        DirectoryScan scan(api, fixture.Root, childMask, 1);
        const char *file = api.Next(scan.Parser);
        result.Expect(file && DirectoryKnownFile(fixture.Root, file) == 8);
        api.Reset(scan.Parser, fixture.Root, mask, 0);
        result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 1);
        api.Reset(scan.Parser, child, mask, 1);
        file = api.Next(scan.Parser);
        result.Expect(file && DirectoryKnownFile(fixture.Root, file) == 8);
    }
#if defined(_WIN32)
    result.Expect(GetProcessHandleCount(GetCurrentProcess(), &after) != 0);
    result.Expect(after <= before + 1);
#endif
    return result;
}

static DirectoryChecks DirectoryNullableReset(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "*.txt";
    DirectoryScan scan(api, fixture.Root, mask, 1);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 25);
    api.Reset(scan.Parser, nullptr, nullptr, 0);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 1);
    char logMask[] = "*.log";
    api.Reset(scan.Parser, nullptr, logMask, 0);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 2);
    char child[640]; DirectoryTestPath(child, fixture.Root, "child");
    api.Reset(scan.Parser, child, nullptr, 1);
    result.Expect(api.Next(scan.Parser) == nullptr);
    api.Reset(scan.Parser, nullptr, mask, 1);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 24);
    return result;
}

static DirectoryChecks DirectoryBorrowedMaskReset(const DirectoryApi &api) {
    DirectoryChecks result;
    DirectoryFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char mask[] = "c.*", child[640];
    DirectoryTestPath(child, fixture.Root, "child");
    DirectoryScan scan(api, fixture.Root, mask, 1);
    const char *file = api.Next(scan.Parser);
    result.Expect(file && DirectoryKnownFile(fixture.Root, file) == 8);
    if (!file) return result;
    const char *name = file;
    for (const char *p = file; *p; ++p) if (*p == '/' || *p == '\\') name = p + 1;
    api.Reset(scan.Parser, child, const_cast<char *>(name), 0);
    result.Expect(DirectoryCollect(result, api, scan.Parser, fixture.Root) == 8);
    return result;
}

#ifndef VX_DIRECTORY_PARSER_PROBE
static void *DirectoryCreate(char *dir, char *mask, int recurse) { return new CKDirectoryParser(dir, mask, recurse); }
static const char *DirectoryNext(void *parser) { return static_cast<CKDirectoryParser *>(parser)->GetNextFile(); }
static void DirectoryReset(void *parser, char *dir, char *mask, int recurse) { static_cast<CKDirectoryParser *>(parser)->Reset(dir, mask, recurse); }
static void DirectoryDestroy(void *parser) { delete static_cast<CKDirectoryParser *>(parser); }
static const DirectoryApi DirectoryCurrent = {DirectoryCreate, DirectoryNext, DirectoryReset, DirectoryDestroy};

TEST(DirectoryParserReferenceTest, NonRecursiveIncludesExtensionlessFilesAndSkipsDirectories) { EXPECT_EQ(DirectoryEnumeration(DirectoryCurrent, 0).errors, 0); }
TEST(DirectoryParserReferenceTest, RecursiveEnumerationReturnsCurrentFilesBeforeChildren) { EXPECT_EQ(DirectoryEnumeration(DirectoryCurrent, 1).errors, 0); }
TEST(DirectoryParserReferenceTest, MasksApplyToFilesWhileAllSubdirectoriesAreTraversed) { EXPECT_EQ(DirectoryMaskAndEmptyChildren(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, TrailingSeparatorsAndRelativePathsRemainUsable) { EXPECT_EQ(DirectoryTrailingAndRelative(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, ConstructionAndResetCopyTheirInputs) { EXPECT_EQ(DirectoryOwnsInputs(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, CallingAfterEndRestartsTheOriginalNonRecursiveScan) { EXPECT_EQ(DirectoryRepeatedEnd(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, RootDirectoryDoesNotEnumerateTheCurrentDirectory) { EXPECT_EQ(DirectoryRootSearch(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, ResetAndDestructionCloseActiveParentAndChildSearches) { EXPECT_EQ(DirectoryResetAndDestroy(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, NullResetOptionsRetainThePreviousDirectoryAndMask) { EXPECT_EQ(DirectoryNullableReset(DirectoryCurrent).errors, 0); }
TEST(DirectoryParserReferenceTest, ResetCanReuseABorrowedChildFilenameAsItsMask) { EXPECT_EQ(DirectoryBorrowedMaskReset(DirectoryCurrent).errors, 0); }

class DirectoryParserView : public CKDirectoryParser {
public:
    DirectoryParserView(const char *dir) : CKDirectoryParser(dir, "*.txt", TRUE) {}
    void CheckLayout() {
        const char *base = reinterpret_cast<const char *>(this);
        EXPECT_EQ(sizeof(CKDirectoryParser), sizeof(void *) == 8 ? 56u : 28u);
        EXPECT_EQ(alignof(CKDirectoryParser), alignof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_FindData) - base, 0);
        EXPECT_EQ(reinterpret_cast<const char *>(&m_hFile) - base, sizeof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_FullFileName) - base, 2 * sizeof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_StartDir) - base, 3 * sizeof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_FileMask) - base, 4 * sizeof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_State) - base, 5 * sizeof(void *));
        EXPECT_EQ(reinterpret_cast<const char *>(&m_SubParser) - base, 6 * sizeof(void *));
        EXPECT_EQ(sizeof(m_State), 4u);
    }
    void CheckResetState() {
        EXPECT_EQ(m_SubParser, nullptr);
#if defined(_WIN32)
        EXPECT_EQ(m_hFile, -1);
#else
        EXPECT_EQ(m_hFile, 0);
#endif
        EXPECT_NE(m_StartDir, nullptr);
        EXPECT_NE(m_FileMask, nullptr);
        EXPECT_EQ(m_State, 0u);
    }
};

TEST(DirectoryParserReferenceTest, NativeLayoutKeepsFieldOrderAndPointerWidth) {
    DirectoryFixture fixture; ASSERT_TRUE(fixture.Ready);
    DirectoryParserView parser(fixture.Root); parser.CheckLayout();
}
TEST(DirectoryParserReferenceTest, ResetReleasesActiveSearchesAndReinitializesState) {
    DirectoryFixture fixture; ASSERT_TRUE(fixture.Ready);
    DirectoryParserView parser(fixture.Root);
    ASSERT_NE(parser.GetNextFile(), nullptr);
    ASSERT_NE(parser.GetNextFile(), nullptr);
    parser.Reset(nullptr, nullptr, FALSE); parser.CheckResetState();
    ASSERT_NE(parser.GetNextFile(), nullptr);
    EXPECT_EQ(parser.GetNextFile(), nullptr);
}
#endif
