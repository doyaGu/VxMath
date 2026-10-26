#include <new>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef VX_PATH_SPLITTER_PROBE
#include "CKPathSplitter.h"
#include <gtest/gtest.h>
static const char *PSRead(CKPathSplitter &value, int field) {
    switch (field) {
    case 0: return value.GetDrive();
    case 1: return value.GetDir();
    case 2: return value.GetName();
    default: return value.GetExtension();
    }
}
static CKPathSplitter *PSAssign(CKPathSplitter &out, const CKPathSplitter &input) {
    return &(out = input);
}
#endif

struct PSChecks {
    int checks = 0, errors = 0, cases = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
    void Merge(const PSChecks &other) { checks += other.checks; errors += other.errors; cases += other.cases; }
};
struct PSCase {
    const char *path;
    const char *drive;
    const char *directory;
    const char *name;
    const char *extension;
};

static PSChecks PSVerify(CKPathSplitter &value, const PSCase &expected) {
    PSChecks result;
    const char *fields[] = {expected.drive, expected.directory, expected.name, expected.extension};
    const unsigned offsets[] = {0, 3, 259, 515};
    for (int field = 0; field < 4; ++field) {
        const char *actual = PSRead(value, field);
        result.Expect(strcmp(actual, fields[field]) == 0);
        result.Expect(actual == reinterpret_cast<const char *>(&value) + offsets[field]);
        result.Expect(PSRead(value, field) == actual);
    }
    ++result.cases;
    return result;
}

template<size_t count> static PSChecks PSCases(const PSCase (&cases)[count]) {
    PSChecks result;
    for (const PSCase &item : cases) {
        char input[1024], before[1024];
        memset(input, 0x6d, sizeof(input));
        strcpy(input, item.path);
        memcpy(before, input, sizeof(input));
        CKPathSplitter value(input);
        result.Merge(PSVerify(value, item));
        result.Expect(memcmp(input, before, sizeof(input)) == 0);
    }
    return result;
}

static PSChecks PSOrdinaryPaths() {
    const PSCase cases[] = {
        {"C:\\Users\\Test\\file.txt", "C:", "\\Users\\Test\\", "file", ".txt"},
        {"C:relative.cmo", "C:", "", "relative", ".cmo"},
        {"file.nmo", "", "", "file", ".nmo"},
        {"filename", "", "", "filename", ""},
        {"", "", "", "", ""},
        {"C:\\data\\", "C:", "\\data\\", "", ""},
        {"../data/archive.tar.gz", "", "../data/", "archive.tar", ".gz"},
        {"/a.b/c", "", "/a.b/", "c", ""}
    };
    return PSCases(cases);
}

static PSChecks PSLeadingAndTrailingDots() {
    const PSCase cases[] = {
        {".profile", "", "", "", ".profile"},
        {".", "", "", "", "."},
        {"..", "", "", ".", "."},
        {"...", "", "", "..", "."},
        {".profile.txt", "", "", ".profile", ".txt"},
        {"file.", "", "", "file", "."},
        {"C:\\dir\\.asset", "C:", "\\dir\\", "", ".asset"},
        {"C:.", "C:", "", "", "."}
    };
    return PSCases(cases);
}

static PSChecks PSDrivePrefixes() {
    const PSCase cases[] = {
        {"a:", "a:", "", "", ""},
        {"Z:abc", "Z:", "", "abc", ""},
        {"1:file.txt", "1:", "", "file", ".txt"},
        {"_:file", "_:", "", "file", ""},
        {"::file", "::", "", "file", ""},
        {":", "", "", ":", ""},
        {"name:stream.txt", "", "", "name:stream", ".txt"},
        {" C:\\file", "", " C:\\", "file", ""}
    };
    return PSCases(cases);
}

static PSChecks PSSeparatorsAndUNC() {
    const PSCase cases[] = {
        {"\\\\server\\share\\file.bin", "", "\\\\server\\share\\", "file", ".bin"},
        {"//server/share/file.bin", "", "//server/share/", "file", ".bin"},
        {"C:/a\\b/c.nmo", "C:", "/a\\b/", "c", ".nmo"},
        {"/", "", "/", "", ""},
        {"\\", "", "\\", "", ""},
        {"dir.with.dots/next/", "", "dir.with.dots/next/", "", ""},
        {"../.hidden", "", "../", "", ".hidden"},
        {"\\\\?\\C:\\long\\f.ext", "", "\\\\?\\C:\\long\\", "f", ".ext"}
    };
    return PSCases(cases);
}

static PSChecks PSLayoutAndCopy() {
    PSChecks result;
    result.Expect(sizeof(CKPathSplitter) == 771 && alignof(CKPathSplitter) == 1);
    char first[] = "D:\\one\\first.cmo", second[] = "E:/two/second.nmo";
    CKPathSplitter original(first), copied(original), assigned(second);
    const PSCase a = {first, "D:", "\\one\\", "first", ".cmo"};
    const PSCase b = {second, "E:", "/two/", "second", ".nmo"};
    result.Expect(PSAssign(assigned, original) == &assigned);
    result.Merge(PSVerify(assigned, a));
    CKPathSplitter replacement(second);
    result.Expect(PSAssign(original, replacement) == &original);
    result.Merge(PSVerify(original, b));
    result.Merge(PSVerify(copied, a));
    result.Merge(PSVerify(assigned, a));
    result.Expect(PSRead(copied, 0) != PSRead(original, 0));
    result.Expect(PSAssign(copied, copied) == &copied);
    result.Merge(PSVerify(copied, a));
    return result;
}

static PSChecks PSMaximumValidComponents() {
    PSChecks result;
    char directory[256], name[256], extension[256], path[800];
    memset(directory, 'd', 254); directory[254] = '/'; directory[255] = 0;
    memset(name, 'n', 255); name[255] = 0;
    extension[0] = '.'; memset(extension + 1, 'e', 254); extension[255] = 0;
    strcpy(path, "Q:"); strcat(path, directory); strcat(path, name); strcat(path, extension);
    CKPathSplitter value(path);
    result.Merge(PSVerify(value, {path, "Q:", directory, name, extension}));
    return result;
}

#if defined(_WIN32)
static PSChecks PSAtGuard(char *input, const PSCase &expected) {
    CKPathSplitter value(input);
    return PSVerify(value, expected);
}
static PSChecks PSProtectedAtGuard(char *input, const PSCase &expected) {
    PSChecks result;
    __try { result = PSAtGuard(input, expected); }
    __except(EXCEPTION_EXECUTE_HANDLER) { result.Expect(false); }
    return result;
}
static PSChecks PSInputGuardPages() {
    PSChecks result;
    SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
    const bool ready = pages && VirtualAlloc(pages, info.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    result.Expect(ready);
    if (ready) {
        const PSCase cases[] = {{"", "", "", "", ""}, {".", "", "", "", "."},
            {"x", "", "", "x", ""}, {"7:a.b", "7:", "", "a", ".b"}, {".file", "", "", "", ".file"}};
        for (const auto &item : cases) {
            const size_t size = strlen(item.path) + 1;
            char *input = reinterpret_cast<char *>(pages + info.dwPageSize - size);
            memcpy(input, item.path, size);
            result.Merge(PSProtectedAtGuard(input, item));
            result.Expect(memcmp(input, item.path, size) == 0);
        }
    }
    if (pages) VirtualFree(pages, 0, MEM_RELEASE);
    return result;
}
#endif

#ifndef VX_PATH_SPLITTER_PROBE
static PSChecks PSPortableInputExtensions() {
    PSChecks result;
    CKPathSplitter empty(nullptr);
    result.Merge(PSVerify(empty, {nullptr, "", "", "", ""}));
    char input[1300], directory[256], name[256], extension[256];
    memset(input, 'd', 400); input[400] = '/';
    memset(input + 401, 'n', 400); input[801] = '.';
    memset(input + 802, 'e', 400); input[1202] = 0;
    memset(directory, 'd', 255); directory[255] = 0;
    memset(name, 'n', 255); name[255] = 0;
    extension[0] = '.'; memset(extension + 1, 'e', 254); extension[255] = 0;
    alignas(CKPathSplitter) unsigned char storage[sizeof(CKPathSplitter) + 16];
    memset(storage, 0xa5, sizeof(storage));
    CKPathSplitter *value = new(storage + 8) CKPathSplitter(input);
    result.Merge(PSVerify(*value, {input, "", directory, name, extension}));
    value->~CKPathSplitter();
    for (unsigned k = 0; k < sizeof(storage); ++k) {
        if (k < 8 || k >= sizeof(CKPathSplitter) + 8) result.Expect(storage[k] == 0xa5);
    }
    return result;
}
TEST(PathSplitterReferenceTest, OrdinaryPathsPreserveComponents) { EXPECT_EQ(PSOrdinaryPaths().errors, 0); }
TEST(PathSplitterReferenceTest, LeadingAndTrailingDotsFollowOriginalSplitpath) { EXPECT_EQ(PSLeadingAndTrailingDots().errors, 0); }
TEST(PathSplitterReferenceTest, DrivePrefixIsALexicalColonPair) { EXPECT_EQ(PSDrivePrefixes().errors, 0); }
TEST(PathSplitterReferenceTest, SeparatorsAndUNCPrefixesRemainLiteral) { EXPECT_EQ(PSSeparatorsAndUNC().errors, 0); }
TEST(PathSplitterReferenceTest, FixedBuffersGettersAndImplicitCopyKeepLayout) { EXPECT_EQ(PSLayoutAndCopy().errors, 0); }
TEST(PathSplitterReferenceTest, MaximumValidComponentsFitWithoutTruncation) { EXPECT_EQ(PSMaximumValidComponents().errors, 0); }
TEST(PathSplitterReferenceTest, NullAndOversizedInputsKeepPortableBoundedBehavior) { EXPECT_EQ(PSPortableInputExtensions().errors, 0); }
#if defined(_WIN32)
TEST(PathSplitterReferenceTest, InputTerminatorCanBeAtGuardPageBoundary) { EXPECT_EQ(PSInputGuardPages().errors, 0); }
#endif
#endif
