#include "VxMutex.h"
#include <gtest/gtest.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <string.h>

template<class T> static auto MutexCopyAvailable(int) -> decltype(T(*static_cast<const T *>(nullptr)), char());
template<class T> static long MutexCopyAvailable(...);
template<class T> static auto MutexAssignAvailable(int) -> decltype(*static_cast<T *>(nullptr) = *static_cast<const T *>(nullptr), char());
template<class T> static long MutexAssignAvailable(...);

static SDL_Mutex *MutexNative(const VxMutex &mutex) {
    SDL_Mutex *native;
    memcpy(&native, &mutex, sizeof(native));
    return native;
}

// The worker probes from another thread only after each command. No scheduling
// delay is used to decide whether a recursive acquisition is still held.
struct MutexTryProbe {
    VxMutex &Mutex;
    int Requests;
    SDL_Semaphore *Command = SDL_CreateSemaphore(0);
    SDL_Semaphore *Done = SDL_CreateSemaphore(0);
    SDL_Thread *Thread = nullptr;
    bool Acquired = false;

    MutexTryProbe(VxMutex &mutex, int requests) : Mutex(mutex), Requests(requests) {}
    ~MutexTryProbe() {
        if (Thread) {
            int status = -1;
            SDL_WaitThread(Thread, &status);
            EXPECT_EQ(0, status);
        }
        SDL_DestroySemaphore(Command);
        SDL_DestroySemaphore(Done);
    }
    static int SDLCALL Run(void *data) {
        MutexTryProbe &probe = *static_cast<MutexTryProbe *>(data);
        for (int i = 0; i < probe.Requests; ++i) {
            if (!SDL_WaitSemaphoreTimeout(probe.Command, 2000)) return 2;
            SDL_Mutex *native = MutexNative(probe.Mutex);
            probe.Acquired = SDL_TryLockMutex(native);
            if (probe.Acquired) SDL_UnlockMutex(native);
            SDL_SignalSemaphore(probe.Done);
        }
        return 0;
    }
    bool Start() {
        if (!Command || !Done) return false;
        Thread = SDL_CreateThread(Run, "mutex-try-probe", this);
        return Thread != nullptr;
    }
    bool Try(bool &result) {
        SDL_SignalSemaphore(Command);
        if (!SDL_WaitSemaphoreTimeout(Done, 2000)) return false;
        result = Acquired;
        return true;
    }
};

TEST(MutexReferenceTest, LayoutIsOneOpaquePointerAndCopyingIsUnavailable) {
    VxMutex mutex;
    EXPECT_EQ(sizeof(void *), sizeof(VxMutex));
    EXPECT_EQ(sizeof(void *), alignof(VxMutex));
    EXPECT_NE(nullptr, MutexNative(mutex));
    EXPECT_EQ(sizeof(long), sizeof(MutexCopyAvailable<VxMutex>(0)));
    EXPECT_EQ(sizeof(long), sizeof(MutexAssignAvailable<VxMutex>(0)));
}

TEST(MutexReferenceTest, NewMutexIsUnlockedAndBalancedOperationsReturnTrue) {
    VxMutex mutex;
    MutexTryProbe probe(mutex, 2);
    ASSERT_TRUE(probe.Start());
    bool acquired = false;
    ASSERT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
    EXPECT_EQ(TRUE, mutex.EnterMutex());
    EXPECT_EQ(TRUE, mutex.LeaveMutex());
    ASSERT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

TEST(MutexReferenceTest, RecursiveDepthIncludesPostfixOperationsAndIgnoresDummyArguments) {
    VxMutex mutex;
    MutexTryProbe probe(mutex, 3);
    ASSERT_TRUE(probe.Start());
    EXPECT_EQ(TRUE, mutex.EnterMutex());
    EXPECT_EQ(TRUE, mutex.operator++(37));
    bool acquired = true;
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
    EXPECT_EQ(TRUE, mutex.LeaveMutex());
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
    EXPECT_EQ(TRUE, mutex.operator--(-12));
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

TEST(MutexReferenceTest, GuardsBorrowTheMutexAndReleaseExactlyTheirRecursiveLevel) {
    VxMutex mutex;
    MutexTryProbe probe(mutex, 3);
    ASSERT_TRUE(probe.Start());
    bool acquired = true;
    {
        VxMutexLock outer(mutex);
        VxMutex *borrowed;
        memcpy(&borrowed, &outer, sizeof(borrowed));
        EXPECT_EQ(&mutex, borrowed);
        EXPECT_EQ(sizeof(void *), sizeof(VxMutexLock));
        EXPECT_EQ(sizeof(void *), alignof(VxMutexLock));
        EXPECT_EQ(sizeof(char), sizeof(MutexCopyAvailable<VxMutexLock>(0)));
        EXPECT_EQ(sizeof(long), sizeof(MutexAssignAvailable<VxMutexLock>(0)));
        {
            VxMutexLock inner(mutex);
            EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
        }
        EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
    }
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

static int MutexGuardedReturn(VxMutex &mutex) {
    VxMutexLock guard(mutex);
    return 17;
}

TEST(MutexReferenceTest, GuardReleasesOnEarlyReturnAndDoesNotOwnTheMutex) {
    VxMutex mutex;
    EXPECT_EQ(17, MutexGuardedReturn(mutex));
    MutexTryProbe probe(mutex, 1);
    ASSERT_TRUE(probe.Start());
    bool acquired = false;
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
    EXPECT_EQ(TRUE, mutex.EnterMutex());
    EXPECT_EQ(TRUE, mutex.LeaveMutex());
}

TEST(MutexReferenceTest, IndependentMutexesDoNotShareExclusionState) {
    VxMutex first, second;
    EXPECT_NE(MutexNative(first), MutexNative(second));
    MutexTryProbe probe(second, 1);
    ASSERT_TRUE(probe.Start());
    VxMutexLock guard(first);
    bool acquired = false;
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

struct MutexCounterState {
    VxMutex Mutex;
    int Value = 0;
    int Mirror = 0x5a5a;
};
struct MutexCounterWorker {
    MutexCounterState *State;
    int Errors = 0;
};
static int SDLCALL IncrementMutexCounter(void *data) {
    MutexCounterWorker &worker = *static_cast<MutexCounterWorker *>(data);
    for (int i = 0; i < 1000; ++i) {
        VxMutexLock guard(worker.State->Mutex);
        if ((worker.State->Value ^ 0x5a5a) != worker.State->Mirror) ++worker.Errors;
        ++worker.State->Value;
        worker.State->Mirror = worker.State->Value ^ 0x5a5a;
    }
    return worker.Errors;
}

TEST(MutexReferenceTest, ContendedGuardsProtectACompleteReadModifyWrite) {
    MutexCounterState state;
    MutexCounterWorker workers[4] = {{&state}, {&state}, {&state}, {&state}};
    SDL_Thread *threads[4] = {};
    for (int i = 0; i < 4; ++i) {
        threads[i] = SDL_CreateThread(IncrementMutexCounter, "mutex-counter", &workers[i]);
        EXPECT_NE(nullptr, threads[i]);
    }
    for (SDL_Thread *thread : threads) if (thread) {
        int status = -1; SDL_WaitThread(thread, &status); EXPECT_EQ(0, status);
    }
    EXPECT_EQ(4000, state.Value);
    EXPECT_EQ(4000 ^ 0x5a5a, state.Mirror);
}

static int SDLCALL IncrementMutexData(void *data) {
    VxDataMutexed<int> &value = *static_cast<VxDataMutexed<int> *>(data);
    const VxDataMutexed<int> &readOnly = value;
    int errors = 0;
    for (int i = 0; i < 500; ++i) {
        VxDataMutexed<int>::Accessor write(&value);
        ++write.Value();
        VxDataMutexed<int>::ConstAccessor read(&readOnly);
        if (read.Value() != write.Value()) ++errors;
    }
    return errors;
}

TEST(MutexReferenceTest, ExistingDataAccessorsCanNestReadAndWriteLocksAcrossThreads) {
    VxDataMutexed<int> value(0);
    SDL_Thread *threads[4] = {};
    for (int i = 0; i < 4; ++i) {
        threads[i] = SDL_CreateThread(IncrementMutexData, "mutex-data", &value);
        EXPECT_NE(nullptr, threads[i]);
    }
    for (SDL_Thread *thread : threads) if (thread) {
        int status = -1; SDL_WaitThread(thread, &status); EXPECT_EQ(0, status);
    }
    VxDataMutexed<int>::ConstAccessor read(&value);
    EXPECT_EQ(2000, read.Value());
}
