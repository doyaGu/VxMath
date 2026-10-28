#include <new>
#include <stddef.h>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef VX_FILE_EXTENSION_PROBE
#include "CKPathSplitter.h"
#include <gtest/gtest.h>
#endif

struct ExtensionChecks {
    int checks = 0;
    int errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
    void Text(const CKFileExtension &value, const char *expected) {
        Expect(memcmp(value.m_Data, expected, strlen(expected) + 1) == 0);
    }
};

static ExtensionChecks ExtensionLayoutAndDefault() {
    ExtensionChecks result;
    result.Expect(sizeof(CKFileExtension) == 4 && alignof(CKFileExtension) == 1);
    result.Expect(offsetof(CKFileExtension, m_Data) == 0);
    unsigned char storage[4];
    memset(storage, 0xa5, sizeof(storage));
    CKFileExtension *value = new(storage) CKFileExtension;
    for (int k = 0; k < 4; ++k) result.Expect(value->m_Data[k] == 0);
    return result;
}

static ExtensionChecks ExtensionConstruction() {
    ExtensionChecks result;
    const char *inputs[] = {"", ".", "..", "...", ".bmp", "TGA", "jpeg", ".jpeg", "..bmp",
        "a.b", " abc", "b m", "a/b", "a\\b", "image.bmp", ".BMP ", "\xc3\xa9XY"};
    const char *expected[] = {"", "", ".", "..", "bmp", "TGA", "jpe", "jpe", ".bm",
        "a.b", " ab", "b m", "a/b", "a\\b", "ima", "BMP", "\xc3\xa9X"};
    for (unsigned k = 0; k < sizeof(inputs) / sizeof(inputs[0]); ++k) {
        char input[32];
        strcpy(input, inputs[k]);
        CKFileExtension value(input);
        result.Text(value, expected[k]);
        result.Expect(strcmp(input, inputs[k]) == 0);
    }
    CKFileExtension absent(static_cast<char *>(nullptr));
    result.Text(absent, "");
    char embedded[] = {'b', 'm', 0, 'p', 0};
    result.Text(CKFileExtension(embedded), "bm");
    return result;
}

static ExtensionChecks ExtensionLengthBoundaries() {
    ExtensionChecks result;
    for (unsigned length = 0; length <= 16; ++length) {
        char input[17], expected[4] = {};
        memset(input, 'x', length);
        input[length] = 0;
        memset(expected, 'x', length < 3 ? length : 3);
        result.Text(CKFileExtension(input), expected);
    }
    return result;
}

static ExtensionChecks ExtensionCaseComparison() {
    ExtensionChecks result;
    for (char letter = 'a'; letter <= 'z'; ++letter) {
        char lower[] = {letter, 'm', 'p', 0};
        char upper[] = {char(letter - 'a' + 'A'), 'M', 'P', 0};
        CKFileExtension a(lower), b(upper);
        result.Expect((a == b) == 1);
        result.Expect((b == a) == 1);
        result.Expect((a == a) == 1);
    }
    char first[] = ".jpeg", second[] = "JPE-more", other[] = "jpg", shortName[] = "jp";
    CKFileExtension a(first), b(second), c(other), d(shortName), empty;
    result.Expect((a == b) == 1);
    result.Expect((a == c) == 0);
    result.Expect((c == d) == 0);
    result.Expect((a == empty) == 0);
    result.Expect((empty == empty) == 1);
    return result;
}

static ExtensionChecks ExtensionPointerViews() {
    ExtensionChecks result;
    char input[] = ".bmp";
    CKFileExtension value(input);
    char *writable = value;
    const char *readable = value;
    result.Expect(writable == value.m_Data && readable == value.m_Data);
    writable[0] = 'T'; writable[1] = 'G'; writable[2] = 'A';
    result.Text(value, "TGA");
    result.Expect(strcmp(readable, "TGA") == 0);
    result.Expect(strcmp(input, ".bmp") == 0);
    writable[1] = 0;
    result.Text(value, "T");
    return result;
}

static ExtensionChecks ExtensionCopyValues() {
    ExtensionChecks result;
    CKFileExtension original;
    memcpy(original.m_Data, "a\0xy", 4);
    CKFileExtension copied(original), assigned;
    result.Expect(&(assigned = original) == &assigned);
    result.Expect(memcmp(copied.m_Data, original.m_Data, 4) == 0);
    result.Expect(memcmp(assigned.m_Data, original.m_Data, 4) == 0);
    result.Expect(&(assigned = assigned) == &assigned);
    result.Expect(memcmp(assigned.m_Data, "a\0xy", 4) == 0);
    copied.m_Data[0] = 'b'; assigned.m_Data[0] = 'c';
    result.Text(original, "a"); result.Text(copied, "b"); result.Text(assigned, "c");
    return result;
}

static ExtensionChecks ExtensionObjectFootprint() {
    ExtensionChecks result;
    unsigned char storage[12];
    memset(storage, 0xa5, sizeof(storage));
    char input[] = ".jpeg";
    CKFileExtension *value = new(storage + 4) CKFileExtension(input);
    result.Text(*value, "jpe");
    CKFileExtension other;
    *value = other;
    result.Text(*value, "");
    for (unsigned k = 0; k < sizeof(storage); ++k) {
        if (k < 4 || k >= 8) result.Expect(storage[k] == 0xa5);
    }
    return result;
}

#if defined(_WIN32)
static ExtensionChecks ExtensionGuardPages() {
    ExtensionChecks result;
    SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages[2] = {};
    bool ready = true;
    for (int k = 0; k < 2; ++k) {
        pages[k] = static_cast<unsigned char *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
        ready = ready && pages[k] != nullptr;
        if (pages[k]) ready = VirtualAlloc(pages[k], info.dwPageSize, MEM_COMMIT, PAGE_READWRITE) != nullptr && ready;
    }
    result.Expect(ready);
    if (ready) {
        char *input = reinterpret_cast<char *>(pages[0] + info.dwPageSize - 5);
        memcpy(input, ".tga", 5);
        CKFileExtension *value = new(pages[1] + info.dwPageSize - 4) CKFileExtension(input);
        result.Text(*value, "tga");
        result.Expect(value->operator char *() == value->m_Data);
        result.Expect(value->operator const char *() == value->m_Data);
        CKFileExtension copied(*value);
        result.Expect((*value == copied) == 1);
        *value = copied;
        result.Text(*value, "tga");
        result.Expect(memcmp(input, ".tga", 5) == 0);
        new(value) CKFileExtension;
        for (int k = 0; k < 4; ++k) result.Expect(value->m_Data[k] == 0);
    }
    for (unsigned char *page : pages) if (page) VirtualFree(page, 0, MEM_RELEASE);
    return result;
}
#endif

#ifndef VX_FILE_EXTENSION_PROBE
TEST(FileExtensionReferenceTest, LayoutAndDefaultConstructionAreFourBytes) { EXPECT_EQ(ExtensionLayoutAndDefault().errors, 0); }
TEST(FileExtensionReferenceTest, ConstructorStripsOneDotAndTruncatesBytes) { EXPECT_EQ(ExtensionConstruction().errors, 0); }
TEST(FileExtensionReferenceTest, LengthBoundariesAlwaysTerminate) { EXPECT_EQ(ExtensionLengthBoundaries().errors, 0); }
TEST(FileExtensionReferenceTest, ComparisonIsCaseInsensitiveAndReturnsBooleanInt) { EXPECT_EQ(ExtensionCaseComparison().errors, 0); }
TEST(FileExtensionReferenceTest, PointerConversionsExposeTheSameWritableStorage) { EXPECT_EQ(ExtensionPointerViews().errors, 0); }
TEST(FileExtensionReferenceTest, ImplicitCopiesPreserveAllFourBytesAndIndependence) { EXPECT_EQ(ExtensionCopyValues().errors, 0); }
TEST(FileExtensionReferenceTest, ConstructionAndAssignmentStayInsideObject) { EXPECT_EQ(ExtensionObjectFootprint().errors, 0); }
#if defined(_WIN32)
TEST(FileExtensionReferenceTest, InputAndObjectFitAtGuardPageBoundaries) { EXPECT_EQ(ExtensionGuardPages().errors, 0); }
#endif
TEST(FileExtensionReferenceTest, CurrentConstInputRemainsAccepted) {
    const char *input = ".BMP";
    CKFileExtension value(input);
    EXPECT_STREQ(value.m_Data, "BMP");
    EXPECT_STREQ(input, ".BMP");
}
#endif
