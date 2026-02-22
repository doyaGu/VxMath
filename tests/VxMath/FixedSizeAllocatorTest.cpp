#include <gtest/gtest.h>

#include <vector>

#include "FixedSizeAllocator.h"

namespace {

struct CountingObject {
    static int s_ctorCount;
    static int s_dtorCount;

    int payload;

    CountingObject() : payload(0x12345678) { ++s_ctorCount; }
    ~CountingObject() { ++s_dtorCount; }
};

int CountingObject::s_ctorCount = 0;
int CountingObject::s_dtorCount = 0;

} // namespace

TEST(FixedSizeAllocatorTest, ChunkGrowthAndReuse)
{
    // Small page to make the chunk size deterministic in this test.
    // Block size 16, page size 64 -> 4 blocks per chunk.
    XFixedSizeAllocator alloc(/*iBlockSize=*/16, /*iPageSize=*/64);

    std::vector<void *> ptrs;
    ptrs.reserve(5);

    for (int i = 0; i < 4; ++i)
        ptrs.push_back(alloc.Allocate());

    ASSERT_EQ(alloc.GetChunksCount(), static_cast<size_t>(1));
    EXPECT_EQ(alloc.GetChunksOccupation(), static_cast<size_t>(4 * 16));

    ptrs.push_back(alloc.Allocate());
    ASSERT_EQ(alloc.GetChunksCount(), static_cast<size_t>(2));
    EXPECT_EQ(alloc.GetChunksOccupation(), static_cast<size_t>(5 * 16));

    for (void *p : ptrs)
        alloc.Free(p);

    EXPECT_EQ(alloc.GetChunksOccupation(), static_cast<size_t>(0));

    // Should reuse existing chunks rather than allocating new ones.
    void *p = alloc.Allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetChunksCount(), static_cast<size_t>(2));
    alloc.Free(p);
}

TEST(FixedSizeAllocatorTest, DoubleFreeDoesNotCorruptFreeList)
{
    XFixedSizeAllocator alloc(/*iBlockSize=*/32, /*iPageSize=*/128);

    void *p = alloc.Allocate();
    ASSERT_NE(p, nullptr);
    alloc.Free(p);

    // Second free should be ignored by internal validation (no crash/corruption).
    alloc.Free(p);

    void *q = alloc.Allocate();
    ASSERT_NE(q, nullptr);
    alloc.Free(q);
}

TEST(FixedSizeAllocatorTest, PartialDoubleFreeDoesNotInjectDuplicateFreeBlock)
{
    XFixedSizeAllocator alloc(/*iBlockSize=*/32, /*iPageSize=*/128); // 4 blocks per chunk

    void *a = alloc.Allocate();
    void *b = alloc.Allocate();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    alloc.Free(a);
    alloc.Free(a); // Should be ignored.

    void *c = alloc.Allocate();
    void *d = alloc.Allocate();
    void *e = alloc.Allocate();
    void *f = alloc.Allocate(); // Requires a new chunk if double-free was ignored.

    ASSERT_NE(c, nullptr);
    ASSERT_NE(d, nullptr);
    ASSERT_NE(e, nullptr);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(alloc.GetChunksCount(), static_cast<size_t>(2));

    alloc.Free(b);
    alloc.Free(c);
    alloc.Free(d);
    alloc.Free(e);
    alloc.Free(f);
}

TEST(FixedSizeAllocatorTest, ChunkTotalSizeUsesActualChunkCapacity)
{
    XFixedSizeAllocator alloc(/*iBlockSize=*/128, /*iPageSize=*/64);

    void *p = alloc.Allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetChunksCount(), static_cast<size_t>(1));
    EXPECT_EQ(alloc.GetChunksTotalSize(), static_cast<size_t>(128));
    alloc.Free(p);
}

TEST(ObjectPoolTest, ClearCallsDtorsForStillAllocatedObjects)
{
    CountingObject::s_ctorCount = 0;
    CountingObject::s_dtorCount = 0;

    XObjectPool<CountingObject> pool(TRUE);
    std::vector<CountingObject *> objs;
    objs.reserve(10);

    for (int i = 0; i < 10; ++i)
        objs.push_back(pool.Allocate());

    ASSERT_EQ(CountingObject::s_ctorCount, 10);
    ASSERT_EQ(CountingObject::s_dtorCount, 0);

    // Free half; dtors should run for those.
    for (int i = 0; i < 10; i += 2)
        pool.Free(objs[static_cast<size_t>(i)]);

    EXPECT_EQ(CountingObject::s_dtorCount, 5);

    // Clear should run dtors for the remaining live objects.
    pool.Clear();
    EXPECT_EQ(CountingObject::s_dtorCount, 10);
}

TEST(ObjectPoolTest, NoDtorModeDoesNotInvokeDestructors)
{
    CountingObject::s_ctorCount = 0;
    CountingObject::s_dtorCount = 0;

    XObjectPool<CountingObject> pool(FALSE);
    CountingObject *a = pool.Allocate();
    CountingObject *b = pool.Allocate();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(CountingObject::s_ctorCount, 2);

    pool.Free(a);
    pool.Free(b);
    EXPECT_EQ(CountingObject::s_dtorCount, 0);

    pool.Clear();
    EXPECT_EQ(CountingObject::s_dtorCount, 0);
}
