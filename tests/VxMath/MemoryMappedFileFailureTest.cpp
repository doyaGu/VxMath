#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "VxPathEncoding.h"
#include <SDL3/SDL_iostream.h>
#include <stdlib.h>
#include <string.h>
#ifndef VX_MMF_FAILURE_PROBE
#include <gtest/gtest.h>
#endif

struct MappedIOState {
    Sint64 Size = 32;
    size_t ReadLimit = 32;
    size_t AllocatedSize = 0;
    bool Open = true;
    bool Allocate = true;
    int Opens = 0, Closes = 0, Reads = 0, Allocations = 0, Live = 0;
};
static MappedIOState MappedIO;

static SDL_IOStream *MappedOpen(const char *, const char *) {
    ++MappedIO.Opens;
    return MappedIO.Open ? reinterpret_cast<SDL_IOStream *>(&MappedIO) : nullptr;
}
static Sint64 MappedSize(SDL_IOStream *) { return MappedIO.Size; }
static bool MappedClose(SDL_IOStream *) { ++MappedIO.Closes; return true; }
static size_t MappedRead(SDL_IOStream *, void *buffer, size_t size) {
    ++MappedIO.Reads;
    size_t count = size < MappedIO.ReadLimit ? size : MappedIO.ReadLimit;
    memset(buffer, 0x5a, count);
    return count;
}
static void *MappedAllocate(size_t size) {
    ++MappedIO.Allocations;
    MappedIO.AllocatedSize = size;
    if (!MappedIO.Allocate) return nullptr;
    void *buffer = malloc(size);
    if (buffer) ++MappedIO.Live;
    return buffer;
}
static void MappedFree(void *buffer) {
    if (buffer) { --MappedIO.Live; free(buffer); }
}

// Exercise the actual implementation with controlled IO and allocation outcomes.
#define VxMemoryMappedFile MappedFileTestSubject
#define SDL_IOFromFile MappedOpen
#define SDL_GetIOSize MappedSize
#define SDL_CloseIO MappedClose
#define SDL_ReadIO MappedRead
#define malloc MappedAllocate
#define free MappedFree
#include "../../src/VxMemoryMappedFileSDL3.cpp"
#undef free
#undef malloc
#undef SDL_ReadIO
#undef SDL_CloseIO
#undef SDL_GetIOSize
#undef SDL_IOFromFile
#undef VxMemoryMappedFile

struct MappedFailureChecks {
    int checks = 0, errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};

static MappedFailureChecks MappedOpenFailures() {
    MappedFailureChecks result;
    MappedIO = MappedIOState();
    {
        MappedFileTestSubject file(nullptr);
        result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_FileOpen);
        result.Expect(file.GetBase() == nullptr && file.GetFileSize() == 0);
        result.Expect(MappedIO.Opens == 0 && MappedIO.Closes == 0);
    }
    MappedIO.Open = false;
    {
        MappedFileTestSubject file("missing.bin");
        result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_FileOpen);
        result.Expect(file.GetBase() == nullptr && file.GetFileSize() == 0);
    }
    result.Expect(MappedIO.Opens == 1 && MappedIO.Closes == 0 && MappedIO.Allocations == 0);
    return result;
}

static MappedFailureChecks MappedSizeFailures() {
    MappedFailureChecks result;
    const Sint64 sizes[] = {0, -1};
    for (Sint64 size : sizes) {
        MappedIO = MappedIOState(); MappedIO.Size = size;
        {
            MappedFileTestSubject file("size.bin");
            result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_FileMapping);
            result.Expect(file.GetBase() == nullptr && file.GetFileSize() == 0);
        }
        result.Expect(MappedIO.Opens == 1 && MappedIO.Closes == 1 && MappedIO.Allocations == 0 && MappedIO.Reads == 0);
    }
    return result;
}

static MappedFailureChecks MappedAllocationFailure() {
    MappedFailureChecks result;
    MappedIO = MappedIOState(); MappedIO.Allocate = false;
    {
        MappedFileTestSubject file("allocation.bin");
        result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_MapView);
        result.Expect(file.GetBase() == nullptr && file.GetFileSize() == 32);
    }
    result.Expect(MappedIO.Allocations == 1 && MappedIO.AllocatedSize == 32 && MappedIO.Live == 0);
    result.Expect(MappedIO.Closes == 1 && MappedIO.Reads == 0);
    return result;
}

static MappedFailureChecks MappedShortRead() {
    MappedFailureChecks result;
    MappedIO = MappedIOState(); MappedIO.ReadLimit = 7;
    {
        MappedFileTestSubject file("short.bin");
        result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_MapView);
        result.Expect(file.GetBase() == nullptr && file.GetFileSize() == 0);
        result.Expect(MappedIO.Live == 0);
    }
    result.Expect(MappedIO.Allocations == 1 && MappedIO.Reads == 1 && MappedIO.Closes == 1 && MappedIO.Live == 0);
    return result;
}

static MappedFailureChecks MappedCompleteRead() {
    MappedFailureChecks result;
    MappedIO = MappedIOState();
    {
        MappedFileTestSubject file("complete.bin");
        result.Expect(file.IsValid() && file.GetErrorType() == VxMMF_NoError);
        result.Expect(file.GetBase() && file.GetFileSize() == 32);
        result.Expect(MappedIO.Live == 1 && MappedIO.Closes == 1);
        if (file.GetBase()) {
            const unsigned char *bytes = static_cast<const unsigned char *>(file.GetBase());
            for (unsigned k = 0; k < 32; ++k) result.Expect(bytes[k] == 0x5a);
        }
    }
    result.Expect(MappedIO.Reads == 1 && MappedIO.Live == 0 && MappedIO.Closes == 1);
    return result;
}

static MappedFailureChecks MappedWideLengths() {
    MappedFailureChecks result;
    const Sint64 sizes[] = {0xffffffffLL, 0x100000000LL, 0x100000011LL, 0x7fffffffffffffffLL};
    for (Sint64 size : sizes) {
        MappedIO = MappedIOState(); MappedIO.Size = size; MappedIO.Allocate = false;
        {
            MappedFileTestSubject file("wide.bin");
            result.Expect(!file.IsValid() && file.GetErrorType() == VxMMF_MapView && file.GetBase() == nullptr);
            if (sizeof(size_t) == 4 && size >= 0x100000000LL) {
                result.Expect(MappedIO.Allocations == 0 && file.GetFileSize() == 0);
            } else {
                result.Expect(MappedIO.Allocations == 1 && MappedIO.AllocatedSize == static_cast<size_t>(size));
                result.Expect(file.GetFileSize() == static_cast<size_t>(size));
            }
        }
        result.Expect(MappedIO.Reads == 0 && MappedIO.Closes == 1 && MappedIO.Live == 0);
    }
    return result;
}

#ifndef VX_MMF_FAILURE_PROBE
TEST(MemoryMappedFileFailureTest, FailedOpenAndNullInputOwnNoResources) { EXPECT_EQ(MappedOpenFailures().errors, 0); }
TEST(MemoryMappedFileFailureTest, EmptyOrUnknownSizeClosesTheStream) { EXPECT_EQ(MappedSizeFailures().errors, 0); }
TEST(MemoryMappedFileFailureTest, AllocationFailureClosesTheStreamAndRetainsRepresentableSize) { EXPECT_EQ(MappedAllocationFailure().errors, 0); }
TEST(MemoryMappedFileFailureTest, ShortReadDiscardsTheIncompleteSnapshot) { EXPECT_EQ(MappedShortRead().errors, 0); }
TEST(MemoryMappedFileFailureTest, CompleteReadOwnsTheBytesUntilDestruction) { EXPECT_EQ(MappedCompleteRead().errors, 0); }
TEST(MemoryMappedFileFailureTest, WideLengthsNeverNarrowIntoAValidSmallSnapshot) { EXPECT_EQ(MappedWideLengths().errors, 0); }
#endif
