#ifndef VX_LIB
#define VX_LIB
#endif
#include "VxMutex.h"
#include <SDL3/SDL_mutex.h>
#include <gtest/gtest.h>
#include <string.h>

static char DataEvents[64];
static int DataEventCount, DataCreates, DataDestroys, DataLocks, DataUnlocks, DataToken;
static bool DataThrowDefault, DataThrowAssign;
static void DataEvent(char value) {
    ASSERT_LT(DataEventCount, 63);
    DataEvents[DataEventCount++] = value;
    DataEvents[DataEventCount] = 0;
}
static void DataResetEvents() { DataEventCount = 0; DataEvents[0] = 0; }
static SDL_Mutex *SDLCALL DataCreateMutex() {
    ++DataCreates; DataEvent('M');
    return reinterpret_cast<SDL_Mutex *>(&DataToken);
}
static void SDLCALL DataDestroyMutex(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&DataToken), mutex);
    ++DataDestroys; DataEvent('m');
}
static void SDLCALL DataLockMutex(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&DataToken), mutex);
    ++DataLocks; DataEvent('L');
}
static void SDLCALL DataUnlockMutex(SDL_Mutex *mutex) {
    EXPECT_EQ(reinterpret_cast<SDL_Mutex *>(&DataToken), mutex);
    ++DataUnlocks; DataEvent('U');
}
#define SDL_CreateMutex DataCreateMutex
#define SDL_DestroyMutex DataDestroyMutex
#define SDL_LockMutex DataLockMutex
#define SDL_UnlockMutex DataUnlockMutex
#include "../../src/VxMutexSDL3.cpp"
#undef SDL_CreateMutex
#undef SDL_DestroyMutex
#undef SDL_LockMutex
#undef SDL_UnlockMutex

struct DataTrackedValue {
    int Value;
    bool Record;
    DataTrackedValue() : Value(13), Record(true) {
        DataEvent('D');
        if (DataThrowDefault) throw 51;
    }
    explicit DataTrackedValue(int value) : Value(value), Record(false) {}
    DataTrackedValue(const DataTrackedValue &) = delete;
    DataTrackedValue &operator=(const DataTrackedValue &other) {
        DataEvent('A');
        if (DataThrowAssign) throw 52;
        Value = other.Value;
        return *this;
    }
    ~DataTrackedValue() { if (Record) DataEvent('d'); }
};

class DataMutexedLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        DataResetEvents();
        DataCreates = DataDestroys = DataLocks = DataUnlocks = 0;
        DataThrowDefault = DataThrowAssign = false;
    }
    void TearDown() override {
        EXPECT_EQ(DataCreates, DataDestroys);
        EXPECT_EQ(DataLocks, DataUnlocks);
    }
};

TEST_F(DataMutexedLifecycleTest, DefaultConstructionAndDestructionRespectMemberOrder) {
    {
        VxDataMutexed<DataTrackedValue> data;
        EXPECT_STREQ("MD", DataEvents);
        EXPECT_EQ(13, data.m_Value.Value);
    }
    EXPECT_STREQ("MDdm", DataEvents);
}

TEST_F(DataMutexedLifecycleTest, ValueConstructionDefaultConstructsThenAssignsWithoutCopyConstruction) {
    DataTrackedValue source(31);
    {
        VxDataMutexed<DataTrackedValue> data(source);
        EXPECT_STREQ("MDA", DataEvents);
        EXPECT_EQ(31, data.m_Value.Value);
        EXPECT_EQ(31, source.Value);
    }
    EXPECT_STREQ("MDAdm", DataEvents);
}

TEST_F(DataMutexedLifecycleTest, ValueCallsReturnReferencesWithoutAdditionalLockingOrValueCopies) {
    DataTrackedValue source(29);
    VxDataMutexed<DataTrackedValue> data(source);
    DataResetEvents();
    {
        VxDataMutexed<DataTrackedValue>::Accessor write(&data);
        VxDataMutexed<DataTrackedValue>::ConstAccessor read(&data);
        EXPECT_STREQ("LL", DataEvents);
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(&data.m_Value, &write.Value());
            EXPECT_EQ(&data.m_Value, &read.Value());
        }
        EXPECT_STREQ("LL", DataEvents);
    }
    EXPECT_STREQ("LLUU", DataEvents);
}

TEST_F(DataMutexedLifecycleTest, ExceptionUnwindingReleasesEachAcquisitionWithoutDestroyingTheOwner) {
    VxDataMutexed<int> data(17);
    DataResetEvents();
    bool caught = false;
    try {
        VxDataMutexed<int>::Accessor write(&data);
        VxDataMutexed<int>::ConstAccessor read(&data);
        throw 23;
    } catch (int value) { caught = true; EXPECT_EQ(23, value); }
    EXPECT_TRUE(caught);
    EXPECT_STREQ("LLUU", DataEvents);
    EXPECT_EQ(0, DataDestroys);
    EXPECT_EQ(17, data.m_Value);
}

TEST_F(DataMutexedLifecycleTest, ThrowingValueDefaultConstructorReleasesOnlyTheConstructedMutex) {
    DataThrowDefault = true;
    bool caught = false;
    try { VxDataMutexed<DataTrackedValue> data; }
    catch (int value) { caught = true; EXPECT_EQ(51, value); }
    EXPECT_TRUE(caught);
    EXPECT_STREQ("MDm", DataEvents);
}

TEST_F(DataMutexedLifecycleTest, ThrowingAssignmentDestroysTheValueBeforeReleasingTheMutex) {
    DataTrackedValue source(31);
    DataThrowAssign = true;
    bool caught = false;
    try { VxDataMutexed<DataTrackedValue> data(source); }
    catch (int value) { caught = true; EXPECT_EQ(52, value); }
    EXPECT_TRUE(caught);
    EXPECT_STREQ("MDAdm", DataEvents);
    EXPECT_EQ(31, source.Value);
}
