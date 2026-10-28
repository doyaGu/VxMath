#ifndef VX_LIB
#define VX_LIB
#endif
#define VxNewAligned PoolFailureNew
#define VxDeleteAligned PoolFailureDelete
#include "VxMemoryPool.h"
#undef VxNewAligned
#undef VxDeleteAligned
#include <gtest/gtest.h>

static int PoolAllocations, PoolReleases, PoolBadReleases;
static size_t PoolBytes, PoolAlignment;
static bool PoolLive, PoolReject;
alignas(16) static XBYTE PoolStorage[256];

void *PoolFailureNew(size_t size, size_t align) {
    ++PoolAllocations;
    PoolBytes = size; PoolAlignment = align;
    if (PoolReject) return nullptr;
    PoolLive = true;
    return PoolStorage;
}
void PoolFailureDelete(void *pointer) {
    if (!pointer) return;
    ++PoolReleases;
    if (pointer != PoolStorage || !PoolLive) ++PoolBadReleases;
    PoolLive = false;
}

class MemoryPoolFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        PoolAllocations = PoolReleases = PoolBadReleases = 0;
        PoolBytes = PoolAlignment = 0;
        PoolLive = PoolReject = false;
    }
    void TearDown() override {
        EXPECT_FALSE(PoolLive);
        EXPECT_EQ(0, PoolBadReleases);
    }
};

TEST_F(MemoryPoolFailureTest, FailedInitialAllocationReportsAnEmptyPool) {
    PoolReject = true;
    VxMemoryPool pool(8);
    EXPECT_EQ(nullptr, pool.Buffer());
    EXPECT_EQ(0u, pool.AllocatedSize());
    EXPECT_EQ(1, PoolAllocations);
    EXPECT_EQ(8 * sizeof(XDWORD), PoolBytes);
    EXPECT_EQ(16u, PoolAlignment);
}

TEST_F(MemoryPoolFailureTest, FailedGrowthReleasesTheOldBufferAndClearsCapacity) {
    VxMemoryPool pool(4);
    PoolReject = true;
    pool.Allocate(16);
    EXPECT_EQ(nullptr, pool.Buffer());
    EXPECT_EQ(0u, pool.AllocatedSize());
    EXPECT_EQ(1, PoolReleases);
    EXPECT_FALSE(PoolLive);
}

TEST_F(MemoryPoolFailureTest, SameRequestCanRetryAfterAFailedAllocation) {
    VxMemoryPool pool;
    PoolReject = true;
    pool.Allocate(8);
    PoolReject = false;
    pool.Allocate(8);
    EXPECT_EQ(PoolStorage, pool.Buffer());
    EXPECT_EQ(8u, pool.AllocatedSize());
    EXPECT_EQ(2, PoolAllocations);
}

TEST_F(MemoryPoolFailureTest, RetainedRequestsDoNotCallTheAllocatorOrReleaseStorage) {
    VxMemoryPool pool(8);
    pool.Allocate(8); pool.Allocate(3); pool.Allocate(0);
    EXPECT_EQ(1, PoolAllocations);
    EXPECT_EQ(0, PoolReleases);
    EXPECT_EQ(PoolStorage, pool.Buffer());
}

TEST_F(MemoryPoolFailureTest, DestructionReleasesOnlyTheCurrentAllocationOnce) {
    {
        VxMemoryPool pool(4);
        pool.Allocate(16);
        EXPECT_EQ(1, PoolReleases);
    }
    EXPECT_EQ(2, PoolReleases);
    EXPECT_FALSE(PoolLive);
}
