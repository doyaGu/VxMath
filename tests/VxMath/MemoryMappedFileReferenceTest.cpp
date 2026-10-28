#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#endif

#ifndef VX_MMF_REFERENCE_PROBE
#include "VxMemoryMappedFile.h"
#include <gtest/gtest.h>
#endif

struct MappedFileApi {
    void *(*Create)(char *);
    void (*Destroy)(void *);
    void *(*Base)(void *);
    size_t (*Size)(void *);
    int (*Valid)(void *);
    int (*Error)(void *);
};
struct MappedFileChecks {
    int checks = 0, errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};
struct MappedFileFixture {
    char File[640] = {};
    char Directory[512] = {};
    bool Ready = false;
    MappedFileFixture() {
#if defined(_WIN32)
        DWORD length = GetTempPathA(sizeof(Directory), Directory);
        if (!length || length >= sizeof(Directory)) return;
        Ready = GetTempFileNameA(Directory, "vxm", 0, File) != 0;
#else
        strcpy(Directory, "/tmp"); strcpy(File, "/tmp/vx-mapped-XXXXXX");
        int fd = mkstemp(File);
        if (fd >= 0) { close(fd); Ready = true; }
#endif
    }
    ~MappedFileFixture() { if (*File) remove(File); }
    bool Write(size_t size, unsigned seed = 7) {
        FILE *stream = fopen(File, "wb");
        if (!stream) return false;
        unsigned char bytes[257];
        size_t offset = 0;
        bool written = true;
        while (offset < size) {
            size_t count = size - offset < sizeof(bytes) ? size - offset : sizeof(bytes);
            for (size_t k = 0; k < count; ++k) bytes[k] = static_cast<unsigned char>((offset + k) * 13 + seed);
            if (fwrite(bytes, 1, count, stream) != count) { written = false; break; }
            offset += count;
        }
        return fclose(stream) == 0 && written;
    }
};
struct MappedFileScope {
    const MappedFileApi &Api;
    void *Object;
    MappedFileScope(const MappedFileApi &api, char *file) : Api(api), Object(api.Create(file)) {}
    ~MappedFileScope() { Api.Destroy(Object); }
};

static void MappedExpectData(MappedFileChecks &result, const MappedFileApi &api, void *object, size_t size, unsigned seed = 7) {
    result.Expect(api.Valid(object) == 1 && api.Error(object) == 0);
    result.Expect(api.Size(object) == size);
    const unsigned char *base = static_cast<const unsigned char *>(api.Base(object));
    result.Expect(base != nullptr && api.Base(object) == base);
    if (base && api.Size(object) == size) {
        bool equal = true;
        for (size_t k = 0; k < size; ++k)
            if (base[k] != static_cast<unsigned char>(k * 13 + seed)) { equal = false; break; }
        result.Expect(equal);
    }
}

static MappedFileChecks MappedRealData(const MappedFileApi &api) {
    MappedFileChecks result;
    MappedFileFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    const size_t sizes[] = {1, 4095, 4096, 4097, 65537};
    for (size_t size : sizes) {
        result.Expect(fixture.Write(size));
        MappedFileScope file(api, fixture.File);
        MappedExpectData(result, api, file.Object, size);
    }
    return result;
}

static MappedFileChecks MappedRealFailures(const MappedFileApi &api) {
    MappedFileChecks result;
    MappedFileFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    char missing[672]; sprintf(missing, "%s.missing", fixture.File);
    char *paths[] = {nullptr, missing, fixture.Directory, fixture.File};
    for (int k = 0; k < 4; ++k) {
        MappedFileScope file(api, paths[k]);
        result.Expect(api.Valid(file.Object) == 0);
        result.Expect(api.Error(file.Object) == (k == 3 ? 2 : 1));
        result.Expect(api.Base(file.Object) == nullptr && api.Size(file.Object) == 0);
    }
    return result;
}

static MappedFileChecks MappedTwoReaders(const MappedFileApi &api) {
    MappedFileChecks result;
    MappedFileFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
    result.Expect(fixture.Write(257));
    MappedFileScope first(api, fixture.File), second(api, fixture.File);
    MappedExpectData(result, api, first.Object, 257);
    MappedExpectData(result, api, second.Object, 257);
    return result;
}

static MappedFileChecks MappedRepeatedLifetimes(const MappedFileApi &api) {
    MappedFileChecks result;
    MappedFileFixture fixture; result.Expect(fixture.Ready); if (!fixture.Ready) return result;
#if defined(_WIN32)
    DWORD before = 0, after = 0;
    result.Expect(GetProcessHandleCount(GetCurrentProcess(), &before) != 0);
#endif
    for (int repeat = 0; repeat < 64; ++repeat) {
        result.Expect(fixture.Write(17));
        { MappedFileScope file(api, fixture.File); MappedExpectData(result, api, file.Object, 17); }
        result.Expect(fixture.Write(0));
        { MappedFileScope empty(api, fixture.File); result.Expect(!api.Valid(empty.Object) && api.Error(empty.Object) == 2); }
    }
#if defined(_WIN32)
    result.Expect(GetProcessHandleCount(GetCurrentProcess(), &after) != 0);
    result.Expect(after <= before + 1);
#endif
    result.Expect(remove(fixture.File) == 0);
    return result;
}

#if defined(_WIN32)
static bool MappedCreateSparse(const char *path, unsigned long long size, unsigned long long &allocated) {
    HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD returned = 0, written = 0;
    LARGE_INTEGER position; position.QuadPart = static_cast<LONGLONG>(size - 1);
    unsigned char last = 0x7b;
    bool ready = DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr) != 0 &&
        SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != 0 && WriteFile(file, &last, 1, &written, nullptr) != 0 && written == 1;
    LARGE_INTEGER actual;
    ready = GetFileSizeEx(file, &actual) != 0 && static_cast<unsigned long long>(actual.QuadPart) == size && ready;
    CloseHandle(file);
    DWORD high = 0, low = GetCompressedFileSizeA(path, &high);
    allocated = (static_cast<unsigned long long>(high) << 32) | low;
    return ready;
}
#endif

#ifndef VX_MMF_REFERENCE_PROBE
static void *MappedCreate(char *path) { return new VxMemoryMappedFile(path); }
static void MappedDestroy(void *file) { delete static_cast<VxMemoryMappedFile *>(file); }
static void *MappedBase(void *file) { return static_cast<VxMemoryMappedFile *>(file)->GetBase(); }
static size_t MappedSize(void *file) { return static_cast<VxMemoryMappedFile *>(file)->GetFileSize(); }
static int MappedValid(void *file) { return static_cast<VxMemoryMappedFile *>(file)->IsValid(); }
static int MappedError(void *file) { return static_cast<VxMemoryMappedFile *>(file)->GetErrorType(); }
static const MappedFileApi MappedCurrent = {MappedCreate, MappedDestroy, MappedBase, MappedSize, MappedValid, MappedError};

TEST(MemoryMappedFileReferenceTest, BinaryBytesAndPageBoundarySizesArePreserved) { EXPECT_EQ(MappedRealData(MappedCurrent).errors, 0); }
TEST(MemoryMappedFileReferenceTest, NullMissingDirectoryAndEmptyInputsReportTheirStage) { EXPECT_EQ(MappedRealFailures(MappedCurrent).errors, 0); }
TEST(MemoryMappedFileReferenceTest, IndependentReadersCanReadTheSameFile) { EXPECT_EQ(MappedTwoReaders(MappedCurrent).errors, 0); }
TEST(MemoryMappedFileReferenceTest, DestructionReleasesSuccessfulAndFailedResources) { EXPECT_EQ(MappedRepeatedLifetimes(MappedCurrent).errors, 0); }

TEST(MemoryMappedFileReferenceTest, CurrentSizeWidthAndFieldOrderRemainConsistent) {
    struct Layout { void *File; void *Mapping; void *Base; size_t Size; VxMMF_Error Error; };
    EXPECT_EQ(sizeof(VxMemoryMappedFile), sizeof(void *) == 8 ? 40u : 20u);
    EXPECT_EQ(alignof(VxMemoryMappedFile), alignof(void *));
    EXPECT_EQ(sizeof(Layout), sizeof(VxMemoryMappedFile));
    EXPECT_EQ(VxMMF_NoError, 0); EXPECT_EQ(VxMMF_FileOpen, 1); EXPECT_EQ(VxMMF_FileMapping, 2); EXPECT_EQ(VxMMF_MapView, 3);
    MappedFileFixture fixture; ASSERT_TRUE(fixture.Ready); ASSERT_TRUE(fixture.Write(23));
    VxMemoryMappedFile file(fixture.File);
    Layout fields; memcpy(&fields, &file, sizeof(fields));
    EXPECT_EQ(fields.Base, file.GetBase()); EXPECT_EQ(fields.Size, file.GetFileSize()); EXPECT_EQ(fields.Error, file.GetErrorType());
}

TEST(MemoryMappedFileReferenceTest, SDLSnapshotSurvivesReplacementAndRemovalOfTheSource) {
    MappedFileFixture fixture; ASSERT_TRUE(fixture.Ready); ASSERT_TRUE(fixture.Write(4097));
    VxMemoryMappedFile file(fixture.File); ASSERT_TRUE(file.IsValid());
    void *base = file.GetBase();
    ASSERT_TRUE(fixture.Write(17, 91)); ASSERT_EQ(remove(fixture.File), 0);
    EXPECT_EQ(file.GetBase(), base); EXPECT_EQ(file.GetFileSize(), 4097u);
    const unsigned char *bytes = static_cast<const unsigned char *>(base);
    for (size_t k = 0; k < 4097; ++k) ASSERT_EQ(bytes[k], static_cast<unsigned char>(k * 13 + 7));
}

#if defined(_WIN32) && !defined(_WIN64)
TEST(MemoryMappedFileReferenceTest, Win32RejectsAnUnrepresentableSparseFileWithoutTruncatingIt) {
    MappedFileFixture fixture; ASSERT_TRUE(fixture.Ready);
    unsigned long long allocated = 0;
    ASSERT_TRUE(MappedCreateSparse(fixture.File, 0x100000011ULL, allocated));
    ASSERT_LT(allocated, 1024u * 1024u);
    RecordProperty("sparse_allocated_bytes", static_cast<int>(allocated));
    VxMemoryMappedFile file(fixture.File);
    EXPECT_FALSE(file.IsValid()); EXPECT_EQ(file.GetBase(), nullptr);
    EXPECT_EQ(file.GetFileSize(), 0u); EXPECT_EQ(file.GetErrorType(), VxMMF_MapView);
}
#endif
#endif
