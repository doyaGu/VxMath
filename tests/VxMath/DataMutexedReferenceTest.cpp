#include "VxMutex.h"
#include <gtest/gtest.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <string.h>

template<class T> static auto DataDefaultAvailable(int) -> decltype(T(), char());
template<class T> static long DataDefaultAvailable(...);
template<class T> static auto DataCopyAvailable(int) -> decltype(T(*static_cast<const T *>(nullptr)), char());
template<class T> static long DataCopyAvailable(...);
template<class T> static auto DataAssignAvailable(int) -> decltype(*static_cast<T *>(nullptr) = *static_cast<const T *>(nullptr), char());
template<class T> static long DataAssignAvailable(...);
template<class A, class B> struct DataSameType { enum { Value = 0 }; };
template<class A> struct DataSameType<A, A> { enum { Value = 1 }; };
struct alignas(16) DataAlignedValue { int Words[4]; };

TEST(DataMutexedReferenceTest, AccessTypesAndPrivateSpecialMembersKeepTheirContracts) {
    typedef VxDataMutexed<int> Data;
    typedef Data::Accessor Write;
    typedef Data::ConstAccessor Read;
    EXPECT_TRUE((DataSameType<decltype(static_cast<Write *>(nullptr)->Value()), int &>::Value));
    EXPECT_TRUE((DataSameType<decltype(static_cast<const Read *>(nullptr)->Value()), const int &>::Value));
    EXPECT_EQ(sizeof(char), sizeof(DataDefaultAvailable<Data>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataCopyAvailable<Data>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataAssignAvailable<Data>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataDefaultAvailable<Write>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataCopyAvailable<Write>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataAssignAvailable<Write>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataDefaultAvailable<Read>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataCopyAvailable<Read>(0)));
    EXPECT_EQ(sizeof(long), sizeof(DataAssignAvailable<Read>(0)));
}

template<class T> static void CheckDataLayout() {
    VxDataMutexed<T> data;
    const size_t alignment = alignof(T) > alignof(VxMutex) ? alignof(T) : alignof(VxMutex);
    const size_t offset = (sizeof(VxMutex) + alignof(T) - 1) & ~(alignof(T) - 1);
    const size_t size = (offset + sizeof(T) + alignment - 1) & ~(alignment - 1);
    EXPECT_EQ(0, reinterpret_cast<const char *>(&data.m_Mutex) - reinterpret_cast<const char *>(&data));
    EXPECT_EQ(offset, reinterpret_cast<const char *>(&data.m_Value) - reinterpret_cast<const char *>(&data));
    EXPECT_EQ(alignment, alignof(VxDataMutexed<T>));
    EXPECT_EQ(size, sizeof(VxDataMutexed<T>));
    typename VxDataMutexed<T>::Accessor write(&data);
    typename VxDataMutexed<T>::ConstAccessor read(&data);
    EXPECT_EQ(sizeof(void *), sizeof(write));
    EXPECT_EQ(sizeof(void *), sizeof(read));
    VxDataMutexed<T> *writeOwner;
    const VxDataMutexed<T> *readOwner;
    memcpy(&writeOwner, &write, sizeof(writeOwner));
    memcpy(&readOwner, &read, sizeof(readOwner));
    EXPECT_EQ(&data, writeOwner);
    EXPECT_EQ(&data, readOwner);
}

TEST(DataMutexedReferenceTest, LayoutFollowsTheValueTypeAndAccessorsStoreOneOwnerPointer) {
    CheckDataLayout<int>();
    CheckDataLayout<double>();
    CheckDataLayout<DataAlignedValue>();
}

TEST(DataMutexedReferenceTest, DefaultScalarStorageIsWrittenBeforeItIsRead) {
    VxDataMutexed<int> data;
    {
        VxDataMutexed<int>::Accessor write(&data);
        write.Value() = 23;
    }
    VxDataMutexed<int>::ConstAccessor read(&data);
    EXPECT_EQ(23, read.Value());
}

TEST(DataMutexedReferenceTest, MutableValueAliasesTheOwnedValueWithoutTransferringOwnership) {
    VxDataMutexed<int> data(17);
    int *borrowed;
    {
        VxDataMutexed<int>::Accessor write(&data);
        borrowed = &write.Value();
        EXPECT_EQ(&data.m_Value, borrowed);
        EXPECT_EQ(17, write.Value());
        write.Value() = 41;
        EXPECT_EQ(41, data.m_Value);
    }
    EXPECT_EQ(&data.m_Value, borrowed);
    VxDataMutexed<int>::ConstAccessor read(&data);
    EXPECT_EQ(41, read.Value());
}

TEST(DataMutexedReferenceTest, ConstOwnerAllowsLockingAndReturnsTheSameReadOnlyValue) {
    const VxDataMutexed<int> data(31);
    const VxDataMutexed<int>::ConstAccessor read(&data);
    EXPECT_EQ(&data.m_Value, &read.Value());
    EXPECT_EQ(31, read.Value());
}

struct DataTryProbe {
    const VxMutex &Mutex;
    int Requests;
    SDL_Semaphore *Command = SDL_CreateSemaphore(0), *Done = SDL_CreateSemaphore(0);
    SDL_Thread *Thread = nullptr;
    bool Acquired = false;
    DataTryProbe(const VxMutex &mutex, int requests) : Mutex(mutex), Requests(requests) {}
    ~DataTryProbe() {
        if (Thread) {
            int status = -1; SDL_WaitThread(Thread, &status); EXPECT_EQ(0, status);
        }
        SDL_DestroySemaphore(Command); SDL_DestroySemaphore(Done);
    }
    static int SDLCALL Run(void *data) {
        DataTryProbe &probe = *static_cast<DataTryProbe *>(data);
        SDL_Mutex *native;
        memcpy(&native, &probe.Mutex, sizeof(native));
        for (int i = 0; i < probe.Requests; ++i) {
            if (!SDL_WaitSemaphoreTimeout(probe.Command, 2000)) return 2;
            probe.Acquired = SDL_TryLockMutex(native);
            if (probe.Acquired) SDL_UnlockMutex(native);
            SDL_SignalSemaphore(probe.Done);
        }
        return 0;
    }
    bool Start() {
        if (!Command || !Done) return false;
        Thread = SDL_CreateThread(Run, "data-mutex-probe", this);
        return Thread != nullptr;
    }
    bool Try(bool &result) {
        SDL_SignalSemaphore(Command);
        if (!SDL_WaitSemaphoreTimeout(Done, 2000)) return false;
        result = Acquired;
        return true;
    }
};

TEST(DataMutexedReferenceTest, ReadAndWriteScopesEachHoldOneRecursiveExclusiveLevel) {
    VxDataMutexed<int> data(17);
    DataTryProbe probe(data.m_Mutex, 6);
    ASSERT_TRUE(probe.Start());
    bool acquired = true;
    {
        VxDataMutexed<int>::Accessor write(&data);
        EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
        {
            VxDataMutexed<int>::ConstAccessor read(&data);
            EXPECT_EQ(&write.Value(), &read.Value());
            EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
        }
        EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
    }
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
    {
        VxDataMutexed<int>::ConstAccessor read(&data);
        EXPECT_TRUE(probe.Try(acquired)); EXPECT_FALSE(acquired);
    }
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

TEST(DataMutexedReferenceTest, DifferentOwnersKeepIndependentMutexesAndValues) {
    VxDataMutexed<int> first(7), second(13);
    DataTryProbe probe(second.m_Mutex, 1);
    ASSERT_TRUE(probe.Start());
    VxDataMutexed<int>::Accessor write(&first);
    write.Value() = 29;
    bool acquired = false;
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
    EXPECT_EQ(13, second.m_Value);
}

static int DataWriteAndReturn(VxDataMutexed<int> &data) {
    VxDataMutexed<int>::Accessor write(&data);
    write.Value() = 37;
    return write.Value();
}

TEST(DataMutexedReferenceTest, EarlyReturnAndExceptionUnwindingReleaseBothAccessorKinds) {
    VxDataMutexed<int> data(17);
    DataTryProbe probe(data.m_Mutex, 2);
    ASSERT_TRUE(probe.Start());
    EXPECT_EQ(37, DataWriteAndReturn(data));
    bool acquired = false;
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
    bool caught = false;
    try {
        VxDataMutexed<int>::Accessor write(&data);
        VxDataMutexed<int>::ConstAccessor read(&data);
        throw 23;
    } catch (int value) { caught = true; EXPECT_EQ(23, value); }
    EXPECT_TRUE(caught);
    EXPECT_TRUE(probe.Try(acquired)); EXPECT_TRUE(acquired);
}

struct DataCounterValue { int Count, Mirror; };
static int SDLCALL IncrementDataCounter(void *data) {
    VxDataMutexed<DataCounterValue> &owner = *static_cast<VxDataMutexed<DataCounterValue> *>(data);
    int errors = 0;
    for (int i = 0; i < 1000; ++i) {
        VxDataMutexed<DataCounterValue>::Accessor write(&owner);
        if ((write.Value().Count ^ 0x5a5a) != write.Value().Mirror) ++errors;
        ++write.Value().Count;
        write.Value().Mirror = write.Value().Count ^ 0x5a5a;
        VxDataMutexed<DataCounterValue>::ConstAccessor read(&owner);
        if (&read.Value() != &write.Value() || read.Value().Mirror != write.Value().Mirror) ++errors;
    }
    return errors;
}

TEST(DataMutexedReferenceTest, ContendedAccessorsProtectCompleteUpdatesAndNestedReads) {
    DataCounterValue initial = {0, 0x5a5a};
    VxDataMutexed<DataCounterValue> owner(initial);
    SDL_Thread *threads[4] = {};
    for (int i = 0; i < 4; ++i) {
        threads[i] = SDL_CreateThread(IncrementDataCounter, "data-mutex-counter", &owner);
        EXPECT_NE(nullptr, threads[i]);
    }
    for (SDL_Thread *thread : threads) if (thread) {
        int status = -1; SDL_WaitThread(thread, &status); EXPECT_EQ(0, status);
    }
    VxDataMutexed<DataCounterValue>::ConstAccessor read(&owner);
    EXPECT_EQ(4000, read.Value().Count);
    EXPECT_EQ(4000 ^ 0x5a5a, read.Value().Mirror);
}
