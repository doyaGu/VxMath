#include "VxSharedLibrary.h"
#include <gtest/gtest.h>
#include <fenv.h>
#include <float.h>
#include <xmmintrin.h>
#include <windows.h>

// A failing implementation must report a test failure rather than crash the suite.
static INSTANCE_HANDLE ReloadBorrowedName(VxSharedLibrary *library, const char *name, DWORD *exception) {
    __try { return library->Load(name); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        *exception = GetExceptionCode();
        return nullptr;
    }
}

class SharedLibraryReloadTest : public ::testing::Test {
    fenv_t Saved;
protected:
    VxSharedLibrary Library;
    void SetUp() override { fegetenv(&Saved); }
    void TearDown() override {
        Library.ReleaseLibrary();
        fesetenv(&Saved);
    }
    void CheckEnvironmentAndOldReference(const char *destination, bool success) {
        ASSERT_NE(Library.Load(VX_SHARED_TEST_SWITCH), nullptr);
        fesetround(FE_UPWARD);
        feclearexcept(FE_ALL_EXCEPT);
        feraiseexcept(FE_INEXACT);
#if defined(_M_IX86)
        unsigned beforeX87, afterX87;
        __control87_2(_PC_53, _MCW_PC, &beforeX87, nullptr);
#endif
        const unsigned beforeSSE = _mm_getcsr();
        const int beforeExceptions = fetestexcept(FE_ALL_EXCEPT);
        INSTANCE_HANDLE handle = Library.Load(destination);
        const unsigned afterSSE = _mm_getcsr();
        const int afterExceptions = fetestexcept(FE_ALL_EXCEPT);
#if defined(_M_IX86)
        __control87_2(0, 0, &afterX87, nullptr);
        EXPECT_EQ(beforeX87, afterX87);
#endif
        EXPECT_EQ(success, handle != nullptr);
        EXPECT_EQ(FE_UPWARD, fegetround());
        EXPECT_EQ(beforeSSE, afterSSE);
        EXPECT_EQ(beforeExceptions, afterExceptions);
        EXPECT_EQ(nullptr, GetModuleHandleA(VX_SHARED_TEST_SWITCH));
        Library.ReleaseLibrary();
    }
};

TEST_F(SharedLibraryReloadTest, NameMayBeBorrowedFromTheModuleBeingUnloaded) {
    for (int choice = 0; choice < 3; ++choice) {
        ASSERT_NE(Library.Load(VX_SHARED_TEST_SWITCH), nullptr);
        auto next = reinterpret_cast<const char *(*)(int)>(Library.GetFunctionPtr("SharedLibraryNextName"));
        ASSERT_NE(next, nullptr);
        const char *name = next(choice);
        DWORD exception = 0;
        INSTANCE_HANDLE handle = ReloadBorrowedName(&Library, name, &exception);
        EXPECT_EQ(0u, exception) << "borrowed name choice " << choice;
        EXPECT_EQ(choice == 0, handle != nullptr);
        EXPECT_EQ(nullptr, GetModuleHandleA(VX_SHARED_TEST_SWITCH));
        Library.ReleaseLibrary();
    }
}

TEST_F(SharedLibraryReloadTest, SuccessfulSwitchPreservesTheEntireFloatingEnvironment) {
    CheckEnvironmentAndOldReference(VX_SHARED_TEST_B, true);
    EXPECT_EQ(nullptr, GetModuleHandleA(VX_SHARED_TEST_B));
}

TEST_F(SharedLibraryReloadTest, FailedSwitchPreservesTheEnvironmentAndStillReleasesTheOldReference) {
    const char invalid[] = {char(0x81), 0};
    const char *paths[] = {nullptr, "", "VxMissingReloadLibrary.dll", invalid, VX_SHARED_TEST_REJECT};
    for (const char *path : paths) CheckEnvironmentAndOldReference(path, false);
}
