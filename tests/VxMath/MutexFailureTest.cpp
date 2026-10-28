#ifndef VX_LIB
#define VX_LIB
#endif
#include "VxMutex.h"
#include <SDL3/SDL_mutex.h>
#include <gtest/gtest.h>

static int MutexCreates, MutexDestroys, MutexLocks, MutexUnlocks;
static bool MutexReject;
static int MutexToken;
static SDL_Mutex *SDLCALL MutexTestCreate() {
    ++MutexCreates;
    return MutexReject ? nullptr : reinterpret_cast<SDL_Mutex *>(&MutexToken);
}
static void SDLCALL MutexTestDestroy(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&MutexToken), mutex);
    ++MutexDestroys;
}
static void SDLCALL MutexTestLock(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&MutexToken), mutex);
    ++MutexLocks;
}
static void SDLCALL MutexTestUnlock(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&MutexToken), mutex);
    ++MutexUnlocks;
}
#define SDL_CreateMutex MutexTestCreate
#define SDL_DestroyMutex MutexTestDestroy
#define SDL_LockMutex MutexTestLock
#define SDL_UnlockMutex MutexTestUnlock
#include "../../src/VxMutexSDL3.cpp"
#undef SDL_CreateMutex
#undef SDL_DestroyMutex
#undef SDL_LockMutex
#undef SDL_UnlockMutex

class MutexFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        MutexCreates = MutexDestroys = MutexLocks = MutexUnlocks = 0;
        MutexReject = false;
    }
};

TEST_F(MutexFailureTest, FailedCreationReturnsFalseWithoutCallingTheBackend) {
    MutexReject = true;
    {
        VxMutex mutex;
        EXPECT_EQ(FALSE, mutex.EnterMutex());
        EXPECT_EQ(FALSE, mutex.LeaveMutex());
        EXPECT_EQ(FALSE, mutex++);
        EXPECT_EQ(FALSE, mutex--);
    }
    EXPECT_EQ(1, MutexCreates);
    EXPECT_EQ(0, MutexDestroys);
    EXPECT_EQ(0, MutexLocks);
    EXPECT_EQ(0, MutexUnlocks);
}

TEST_F(MutexFailureTest, SuccessfulOperationsForwardTheSameOpaqueHandle) {
    {
        VxMutex mutex;
        EXPECT_EQ(TRUE, mutex.EnterMutex());
        EXPECT_EQ(TRUE, mutex++);
        EXPECT_EQ(TRUE, mutex--);
        EXPECT_EQ(TRUE, mutex.LeaveMutex());
    }
    EXPECT_EQ(1, MutexCreates);
    EXPECT_EQ(1, MutexDestroys);
    EXPECT_EQ(2, MutexLocks);
    EXPECT_EQ(2, MutexUnlocks);
}

TEST_F(MutexFailureTest, GuardOnlyAcquiresAndReleasesTheBorrowedMutex) {
    {
        VxMutex mutex;
        {
            VxMutexLock guard(mutex);
            EXPECT_EQ(1, MutexCreates);
            EXPECT_EQ(1, MutexLocks);
            EXPECT_EQ(0, MutexUnlocks);
        }
        EXPECT_EQ(1, MutexUnlocks);
        EXPECT_EQ(0, MutexDestroys);
    }
    EXPECT_EQ(1, MutexDestroys);
}
