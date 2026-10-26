#include <new>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef VX_PATH_MAKER_PROBE
#include "CKPathSplitter.h"
#include <gtest/gtest.h>
static const char *MKRead(CKPathMaker &value) { return value.GetFileName(); }
static CKPathMaker *MKAssign(CKPathMaker &out, const CKPathMaker &input) { return &(out = input); }
#endif

struct MKChecks {
    int checks = 0, errors = 0, cases = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
    void Merge(const MKChecks &other) { checks += other.checks; errors += other.errors; cases += other.cases; }
};
struct MKCase { const char *drive, *directory, *name, *extension, *expected; };

static CKPathMaker *MKConstruct(void *storage, const MKCase &item) {
    // Original 2.1 takes mutable pointers; both implementations only read them.
    return new(storage) CKPathMaker(const_cast<char *>(item.drive), const_cast<char *>(item.directory),
        const_cast<char *>(item.name), const_cast<char *>(item.extension));
}
static MKChecks MKOneCase(const MKCase &item) {
    MKChecks result;
    char data[4][512], before[4][512];
    const char *parts[] = {item.drive, item.directory, item.name, item.extension};
    for (int k = 0; k < 4; ++k) {
        memset(data[k], 0x6d, sizeof(data[k]));
        if (parts[k]) strcpy(data[k], parts[k]);
    }
    memcpy(before, data, sizeof(data));
    const MKCase input = {parts[0] ? data[0] : nullptr, parts[1] ? data[1] : nullptr,
        parts[2] ? data[2] : nullptr, parts[3] ? data[3] : nullptr, item.expected};
    alignas(CKPathMaker) unsigned char storage[sizeof(CKPathMaker) + 16];
    memset(storage, 0xa5, sizeof(storage));
    CKPathMaker *value = MKConstruct(storage + 8, input);
    const char *text = MKRead(*value);
    result.Expect(strcmp(text, item.expected) == 0);
    result.Expect(MKRead(*value) == text);
    result.Expect(memcmp(data, before, sizeof(data)) == 0);
    value->~CKPathMaker();
    for (unsigned k = 0; k < sizeof(storage); ++k) {
        if (k < 8 || k >= sizeof(CKPathMaker) + 8) result.Expect(storage[k] == 0xa5);
    }
    ++result.cases;
    return result;
}
template<size_t count> static MKChecks MKCases(const MKCase (&cases)[count]) {
    MKChecks result;
    for (const auto &item : cases) result.Merge(MKOneCase(item));
    return result;
}

static MKChecks MKOrdinaryComponents() {
    const MKCase cases[] = {
        {"C:", "\\data\\", "file", ".nmo", "C:\\data\\file.nmo"},
        {"D", "folder", "file", "cmo", "D:folder\\file.cmo"},
        {nullptr, "//server/share/", "file", "bin", "//server/share/file.bin"},
        {nullptr, "../data", "archive.tar", "gz", "../data\\archive.tar.gz"},
        {nullptr, nullptr, ".profile", nullptr, ".profile"}
    };
    return MKCases(cases);
}
static MKChecks MKDriveRules() {
    const MKCase cases[] = {
        {"C", nullptr, "file", nullptr, "C:file"},
        {"C:", nullptr, "file", nullptr, "C:file"},
        {"C:ignored", nullptr, "file", nullptr, "C:file"},
        {"AB", nullptr, "file", nullptr, "A:file"},
        {"1", nullptr, "file", nullptr, "1:file"},
        {"_:", nullptr, "file", nullptr, "_:file"},
        {":", nullptr, "file", nullptr, "::file"},
        {"", nullptr, "file", nullptr, "file"}
    };
    return MKCases(cases);
}
static MKChecks MKDirectoryRules() {
    const MKCase cases[] = {
        {nullptr, "dir", "file", nullptr, "dir\\file"},
        {nullptr, "dir/", "file", nullptr, "dir/file"},
        {nullptr, "dir\\", "file", nullptr, "dir\\file"},
        {nullptr, "/a/b", "file", nullptr, "/a/b\\file"},
        {nullptr, "\\", "file", nullptr, "\\file"},
        {nullptr, "/", nullptr, nullptr, "/"},
        {nullptr, "dir", nullptr, nullptr, "dir\\"},
        {nullptr, "", "file", nullptr, "file"}
    };
    return MKCases(cases);
}
static MKChecks MKExtensionRules() {
    const MKCase cases[] = {
        {nullptr, nullptr, "file", "txt", "file.txt"},
        {nullptr, nullptr, "file", ".txt", "file.txt"},
        {nullptr, nullptr, "file", ".", "file."},
        {nullptr, nullptr, "file", "..txt", "file..txt"},
        {nullptr, nullptr, "file.txt", "bin", "file.txt.bin"},
        {nullptr, nullptr, nullptr, "txt", ".txt"},
        {nullptr, nullptr, "file", "", "file"}
    };
    return MKCases(cases);
}
static MKChecks MKOptionalComponents() {
    const char *expected[] = {"", "D:", "folder\\", "D:folder\\", "file", "D:file", "folder\\file", "D:folder\\file",
        ".dat", "D:.dat", "folder\\.dat", "D:folder\\.dat", "file.dat", "D:file.dat", "folder\\file.dat", "D:folder\\file.dat"};
    MKChecks result;
    for (unsigned mask = 0; mask < 16; ++mask) {
        result.Merge(MKOneCase({mask&1 ? "D" : nullptr, mask&2 ? "folder" : nullptr,
            mask&4 ? "file" : nullptr, mask&8 ? "dat" : nullptr, expected[mask]}));
    }
    return result;
}
static MKChecks MKValueOwnership() {
    MKChecks result;
    char drive[] = "C:", directory[] = "dir\\", name[] = "first", extension[] = "txt";
    char second[] = "second";
    CKPathMaker original(drive, directory, name, extension), copied(original);
    CKPathMaker assigned(nullptr, nullptr, second, nullptr);
    result.Expect(MKRead(original) != MKRead(copied));
    result.Expect(MKAssign(assigned, original) == &assigned);
    result.Expect(strcmp(MKRead(assigned), "C:dir\\first.txt") == 0 && MKRead(assigned) != MKRead(original));
    CKPathMaker replacement(nullptr, nullptr, second, nullptr);
    result.Expect(MKAssign(original, replacement) == &original);
    result.Expect(strcmp(MKRead(original), "second") == 0);
    result.Expect(strcmp(MKRead(copied), "C:dir\\first.txt") == 0);
    result.Expect(strcmp(MKRead(assigned), "C:dir\\first.txt") == 0);
    const char *pointer = MKRead(copied);
    result.Expect(MKAssign(copied, copied) == &copied && MKRead(copied) == pointer);
    CKPathMaker *survivor;
    { CKPathMaker temporary(drive, directory, name, extension); survivor = new CKPathMaker(temporary); }
    result.Expect(strcmp(MKRead(*survivor), "C:dir\\first.txt") == 0);
    delete survivor;
    return result;
}
static MKChecks MKMaximumOriginalPath() {
    char directory[201], name[41], extension[16], expected[260];
    memset(directory, 'd', 200); directory[200] = 0;
    memset(name, 'n', 40); name[40] = 0;
    memset(extension, 'e', 15); extension[15] = 0;
    strcpy(expected, "Q:"); strcat(expected, directory); strcat(expected, "\\");
    strcat(expected, name); strcat(expected, "."); strcat(expected, extension);
    return MKOneCase({"Q", directory, name, extension, expected});
}

#if defined(_WIN32)
static MKChecks MKGuardCase(const MKCase &item, void *storage) {
    MKChecks result;
    CKPathMaker *value = MKConstruct(storage, item);
    result.Expect(strcmp(MKRead(*value), item.expected) == 0);
    value->~CKPathMaker();
    return result;
}
static MKChecks MKProtectedGuardCase(const MKCase &item, void *storage) {
    MKChecks result;
    __try { result = MKGuardCase(item, storage); }
    __except(EXCEPTION_EXECUTE_HANDLER) { result.Expect(false); }
    return result;
}
static MKChecks MKGuardPages() {
    MKChecks result;
    SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages[5] = {};
    bool ready = true;
    for (int k = 0; k < 5; ++k) {
        pages[k] = static_cast<unsigned char *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
        ready = ready && pages[k] != nullptr;
        if (pages[k]) ready = VirtualAlloc(pages[k], info.dwPageSize, MEM_COMMIT, PAGE_READWRITE) != nullptr && ready;
    }
    result.Expect(ready);
    if (ready) {
        const char *values[] = {"D", "dir", "file", "ext"};
        const char *parts[4];
        for (int k = 0; k < 4; ++k) {
            const size_t size = strlen(values[k]) + 1;
            char *input = reinterpret_cast<char *>(pages[k] + info.dwPageSize - size);
            memcpy(input, values[k], size); parts[k] = input;
        }
        result.Merge(MKProtectedGuardCase({parts[0], parts[1], parts[2], parts[3], "D:dir\\file.ext"},
            pages[4] + info.dwPageSize - sizeof(CKPathMaker)));
        for (int k = 0; k < 4; ++k) result.Expect(strcmp(parts[k], values[k]) == 0);
    }
    for (auto page : pages) if (page) VirtualFree(page, 0, MEM_RELEASE);
    return result;
}

static int MKDbcsDirectory(char output[8]) {
    const wchar_t character = 0x4e57;
    const UINT codePage = GetACP();
    BOOL usedDefault = FALSE;
    const int size = WideCharToMultiByte(codePage, codePage == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS,
        &character, 1, output, 7, nullptr, codePage == CP_UTF8 ? nullptr : &usedDefault);
    if (size != 2 || usedDefault || output[1] != '\\') return 0;
    output[size] = 0;
    return size;
}
static MKChecks MKDbcsTrailingSeparator() {
    MKChecks result;
    char directory[8];
    if (!MKDbcsDirectory(directory)) return result;
    char expected[32]; strcpy(expected, directory); strcat(expected, "\\file.ext");
    return MKOneCase({nullptr, directory, "file", "ext", expected});
}
#endif

#ifndef VX_PATH_MAKER_PROBE
TEST(PathMakerReferenceTest, OrdinaryComponentsFollowMakepath) { EXPECT_EQ(MKOrdinaryComponents().errors, 0); }
TEST(PathMakerReferenceTest, DriveUsesFirstCharacterAndColon) { EXPECT_EQ(MKDriveRules().errors, 0); }
TEST(PathMakerReferenceTest, DirectoryGetsATrailingSeparator) { EXPECT_EQ(MKDirectoryRules().errors, 0); }
TEST(PathMakerReferenceTest, ExtensionGetsOnlyTheRequiredDot) { EXPECT_EQ(MKExtensionRules().errors, 0); }
TEST(PathMakerReferenceTest, EveryOptionalComponentCombinationWorks) { EXPECT_EQ(MKOptionalComponents().errors, 0); }
TEST(PathMakerReferenceTest, CopyAssignmentAndDestructionKeepIndependentValues) { EXPECT_EQ(MKValueOwnership().errors, 0); }
TEST(PathMakerReferenceTest, Original259ByteBoundaryFits) { EXPECT_EQ(MKMaximumOriginalPath().errors, 0); }
#if defined(_WIN32)
TEST(PathMakerReferenceTest, InputsAndObjectCanEndAtGuardPages) { EXPECT_EQ(MKGuardPages().errors, 0); }
TEST(PathMakerReferenceTest, DbcsTrailBackslashIsNotASeparator) {
    const MKChecks result = MKDbcsTrailingSeparator();
    RecordProperty("dbcs_trailing_separator_exercised", result.cases);
    EXPECT_EQ(result.errors, 0);
}
#endif
TEST(PathMakerReferenceTest, DynamicStorageUsesNativePointerLayout) {
    char name[] = "file";
    CKPathMaker value(nullptr, nullptr, name, nullptr);
    EXPECT_EQ(sizeof(CKPathMaker), sizeof(void *));
    EXPECT_EQ(alignof(CKPathMaker), alignof(void *));
    const char *stored;
    memcpy(&stored, &value, sizeof(stored));
    EXPECT_EQ(stored, value.GetFileName());
}
TEST(PathMakerReferenceTest, LongPathsRemainUntruncatedAndOwned) {
    char directory[2049], expected[2080];
    memset(directory, 'd', 2048); directory[2048] = 0;
    strcpy(expected, "C:"); strcat(expected, directory); strcat(expected, "\\file.ext");
    CKPathMaker value("C:ignored", directory, "file", "ext"), copied(value);
    EXPECT_STREQ(value.GetFileName(), expected);
    EXPECT_STREQ(copied.GetFileName(), expected);
    EXPECT_NE(value.GetFileName(), copied.GetFileName());
}
#endif
