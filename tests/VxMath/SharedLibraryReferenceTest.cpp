#include <fenv.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>
#include <windows.h>
#ifndef VX_SHARED_LIBRARY_PROBE
#include "VxSharedLibrary.h"
#include <gtest/gtest.h>
#endif

struct SharedTestApi {
    void *(*Create)();
    void (*Destroy)(void *);
    void (*Attach)(void *, void *);
    void *(*Load)(void *, const char *);
    void (*Release)(void *);
    void *(*Function)(void *, const char *);
};
struct SharedChecks {
    int checks = 0, errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};

static void *SharedHandle(void *object) {
    void *handle;
    memcpy(&handle, object, sizeof(handle));
    return handle;
}
static bool SharedIsLoaded(const char *path) { return GetModuleHandleA(path) != nullptr; }
static int SharedValue(void *function) { return reinterpret_cast<int (*)()>(function)(); }

struct SharedFixture {
    fenv_t Environment;
    SharedFixture() { fegetenv(&Environment); }
    ~SharedFixture() {
        const char *paths[] = {VX_SHARED_TEST_A, VX_SHARED_TEST_B, VX_SHARED_TEST_REJECT};
        for (const char *path : paths) {
            for (int remaining = 0; remaining < 16; ++remaining) {
                HMODULE handle = GetModuleHandleA(path);
                if (!handle) break;
                FreeLibrary(handle);
            }
        }
        fesetenv(&Environment);
    }
};
struct SharedObject {
    const SharedTestApi &Api;
    void *Object;
    explicit SharedObject(const SharedTestApi &api) : Api(api), Object(api.Create()) {}
    ~SharedObject() { Api.Destroy(Object); }
};

static SharedChecks SharedBasic(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    SharedObject library(api);
    result.Expect(SharedHandle(library.Object) == nullptr);
    result.Expect(api.Function(library.Object, "SharedLibraryTestValue") == nullptr);
    result.Expect(api.Function(library.Object, nullptr) == nullptr);
    api.Release(library.Object);
    void *handle = api.Load(library.Object, VX_SHARED_TEST_A);
    result.Expect(handle && handle == SharedHandle(library.Object));
    result.Expect(handle == GetModuleHandleA(VX_SHARED_TEST_A));
    void *function = api.Function(library.Object, "SharedLibraryTestValue");
    result.Expect(function != nullptr);
    if (function) result.Expect(SharedValue(function) == 17);
    result.Expect(api.Function(library.Object, "MissingSharedLibraryTestFunction") == nullptr);
    result.Expect(api.Function(library.Object, nullptr) == nullptr);
    api.Release(library.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A));
    return result;
}

static SharedChecks SharedReload(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    SharedObject library(api);
    result.Expect(api.Load(library.Object, VX_SHARED_TEST_A) != nullptr);
    result.Expect(api.Load(library.Object, VX_SHARED_TEST_A) != nullptr);
    api.Release(library.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A));
    result.Expect(api.Load(library.Object, VX_SHARED_TEST_A) != nullptr);
    result.Expect(api.Load(library.Object, VX_SHARED_TEST_B) != nullptr);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A) && SharedIsLoaded(VX_SHARED_TEST_B));
    void *function = api.Function(library.Object, "SharedLibraryTestValue");
    result.Expect(function != nullptr);
    if (function) result.Expect(SharedValue(function) == 29);
    api.Release(library.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_B));
    return result;
}

static SharedChecks SharedFailedReload(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    SharedObject library(api);
    char missing[MAX_PATH]; sprintf(missing, "%s.missing", VX_SHARED_TEST_A);
    const char invalid[] = {char(0x81), 0};
    const char *paths[] = {missing, "", nullptr, invalid, VX_SHARED_TEST_REJECT};
    for (const char *path : paths) {
        result.Expect(api.Load(library.Object, VX_SHARED_TEST_A) != nullptr);
        result.Expect(api.Load(library.Object, path) == nullptr);
        result.Expect(SharedHandle(library.Object) == nullptr);
        result.Expect(api.Function(library.Object, "SharedLibraryTestValue") == nullptr);
        result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A));
    }
    return result;
}

static SharedChecks SharedAttach(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    HMODULE native = LoadLibraryA(VX_SHARED_TEST_A);
    result.Expect(native != nullptr); if (!native) return result;
    {
        SharedObject library(api);
        api.Attach(library.Object, native);
        result.Expect(SharedHandle(library.Object) == native);
        void *function = api.Function(library.Object, "SharedLibraryTestValue");
        result.Expect(function == reinterpret_cast<void *>(GetProcAddress(native, "SharedLibraryTestValue")));
        if (function) result.Expect(SharedValue(function) == 17);
    }
    result.Expect(SharedIsLoaded(VX_SHARED_TEST_A));
    {
        SharedObject release(api);
        api.Attach(release.Object, native);
        api.Release(release.Object);
        result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A));
    }
    return result;
}

static SharedChecks SharedAttachDoesNotRetainOrReleasePrevious(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    SharedObject first(api), second(api);
    void *a = api.Load(first.Object, VX_SHARED_TEST_A);
    void *b = api.Load(second.Object, VX_SHARED_TEST_B);
    result.Expect(a && b);
    api.Attach(first.Object, b);
    api.Attach(second.Object, nullptr);
    result.Expect(SharedHandle(first.Object) == b && SharedHandle(second.Object) == nullptr);
    result.Expect(SharedIsLoaded(VX_SHARED_TEST_A));
    api.Release(first.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_B));
    result.Expect(SharedIsLoaded(VX_SHARED_TEST_A));
    api.Attach(second.Object, a);
    api.Release(second.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_A));
    return result;
}

static SharedChecks SharedLoadSurvivesWrapperScope(const SharedTestApi &api) {
    SharedFixture fixture;
    SharedChecks result;
    void *handle = nullptr;
    { SharedObject library(api); handle = api.Load(library.Object, VX_SHARED_TEST_B); result.Expect(handle != nullptr); }
    result.Expect(SharedIsLoaded(VX_SHARED_TEST_B));
    SharedObject release(api);
    api.Attach(release.Object, handle);
    api.Release(release.Object);
    result.Expect(!SharedIsLoaded(VX_SHARED_TEST_B));
    return result;
}

static SharedChecks SharedFloatingEnvironment(const SharedTestApi &api, bool requireSSE) {
    SharedFixture fixture;
    SharedChecks result;
    SharedObject library(api);
    const char *paths[] = {VX_SHARED_TEST_A, VX_SHARED_TEST_REJECT};
    for (int k = 0; k < 2; ++k) {
#if defined(_M_IX86)
        unsigned beforeX87 = 0, afterX87 = 0;
        __control87_2(_PC_53 | _RC_UP, _MCW_PC | _MCW_RC, &beforeX87, nullptr);
#endif
        const unsigned beforeSSE = (_mm_getcsr() & ~0x6000u) | 0x2000u | 0x8000u;
        _mm_setcsr(beforeSSE);
        void *handle = api.Load(library.Object, paths[k]);
        const unsigned afterSSE = _mm_getcsr();
#if defined(_M_IX86)
        __control87_2(0, 0, &afterX87, nullptr);
        result.Expect(beforeX87 == afterX87);
#endif
        result.Expect((handle != nullptr) == (k == 0));
        if (requireSSE) result.Expect(afterSSE == beforeSSE);
        if (handle) api.Release(library.Object);
    }
    return result;
}

#ifndef VX_SHARED_LIBRARY_PROBE
static void *SharedCreate() { return new VxSharedLibrary; }
static void SharedDestroy(void *object) { delete static_cast<VxSharedLibrary *>(object); }
static void SharedAttachObject(void *object, void *handle) { static_cast<VxSharedLibrary *>(object)->Attach(handle); }
static void *SharedLoadObject(void *object, const char *path) { return static_cast<VxSharedLibrary *>(object)->Load(path); }
static void SharedReleaseObject(void *object) { static_cast<VxSharedLibrary *>(object)->ReleaseLibrary(); }
static void *SharedFunction(void *object, const char *name) { return static_cast<VxSharedLibrary *>(object)->GetFunctionPtr(name); }
static const SharedTestApi SharedCurrent = {SharedCreate, SharedDestroy, SharedAttachObject, SharedLoadObject, SharedReleaseObject, SharedFunction};

TEST(SharedLibraryReferenceTest, LoadReturnsANativeHandleAndLooksUpExactSymbols) { EXPECT_EQ(SharedBasic(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, ReloadReleasesThePreviousReferenceBeforeLoading) { EXPECT_EQ(SharedReload(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, FailedReloadAlsoReleasesThePreviousLibrary) { EXPECT_EQ(SharedFailedReload(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, AttachAcceptsNativeHandlesWithoutAnImplicitReferenceOrDestructorRelease) { EXPECT_EQ(SharedAttach(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, AttachOnlyReplacesTheStoredHandle) { EXPECT_EQ(SharedAttachDoesNotRetainOrReleasePrevious(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, LoadedLibrarySurvivesTheWrapperScope) { EXPECT_EQ(SharedLoadSurvivesWrapperScope(SharedCurrent).errors, 0); }
TEST(SharedLibraryReferenceTest, LoadPreservesPrecisionRoundingAndSIMDControlsOnSuccessAndFailure) { EXPECT_EQ(SharedFloatingEnvironment(SharedCurrent, true).errors, 0); }

TEST(SharedLibraryReferenceTest, ReleaseClearsTheHandleAndCanBeRepeated) {
    SharedFixture fixture;
    VxSharedLibrary library;
    ASSERT_NE(library.Load(VX_SHARED_TEST_B), nullptr);
    library.ReleaseLibrary();
    EXPECT_EQ(SharedHandle(&library), nullptr);
    library.ReleaseLibrary();
    EXPECT_EQ(library.GetFunctionPtr("SharedLibraryTestValue"), nullptr);
    EXPECT_EQ(library.GetFunctionPtr(nullptr), nullptr);
}
TEST(SharedLibraryReferenceTest, LayoutIsOneNativePointerAndCopiesOnlyTheHandleValue) {
    EXPECT_EQ(sizeof(VxSharedLibrary), sizeof(void *)); EXPECT_EQ(alignof(VxSharedLibrary), alignof(void *));
    VxSharedLibrary value;
    void *marker = reinterpret_cast<void *>(static_cast<uintptr_t>(0x12345));
    value.Attach(marker);
    VxSharedLibrary copy(value), assigned;
    assigned = value;
    EXPECT_EQ(SharedHandle(&copy), marker); EXPECT_EQ(SharedHandle(&assigned), marker);
    value.Attach(nullptr);
    EXPECT_EQ(SharedHandle(&copy), marker); EXPECT_EQ(SharedHandle(&assigned), marker);
    copy.Attach(nullptr); assigned.Attach(nullptr);
}
#endif
