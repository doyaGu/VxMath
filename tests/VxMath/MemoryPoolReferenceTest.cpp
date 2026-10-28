#include "VxMemoryPool.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

class PoolLayoutProbe : public VxMemoryPool {
public:
    size_t PointerOffset() const {
        return reinterpret_cast<const char *>(&m_Memory) - reinterpret_cast<const char *>(this);
    }
    size_t SizeOffset() const {
        return reinterpret_cast<const char *>(&m_Allocated) - reinterpret_cast<const char *>(this);
    }
};

static void AcceptPoolValue(VxMemoryPool);
template<class T> static auto ImplicitPoolConversion(int) -> decltype(AcceptPoolValue(*static_cast<T *>(nullptr)), char());
template<class T> static long ImplicitPoolConversion(...);

TEST(MemoryPoolReferenceTest, ConstructionRequiresAnExplicitWordCount) {
    EXPECT_EQ(sizeof(long), sizeof(ImplicitPoolConversion<size_t>(0)));
    EXPECT_EQ(sizeof(long), sizeof(ImplicitPoolConversion<int>(0)));
    VxMemoryPool pool(3);
    EXPECT_EQ(3u, pool.AllocatedSize());
}

TEST(MemoryPoolReferenceTest, LayoutUsesAnOwningPointerAndNativeSizedWordCount) {
    PoolLayoutProbe pool;
    EXPECT_EQ(0u, pool.PointerOffset());
    EXPECT_EQ(sizeof(void *), pool.SizeOffset());
    EXPECT_EQ(2 * sizeof(void *), sizeof(VxMemoryPool));
    EXPECT_EQ(sizeof(void *), alignof(VxMemoryPool));
}

TEST(MemoryPoolReferenceTest, EmptyConstructionAndZeroRequestsDoNotAllocate) {
    VxMemoryPool pool;
    EXPECT_EQ(nullptr, pool.Buffer());
    EXPECT_EQ(0u, pool.AllocatedSize());
    pool.Allocate(0);
    EXPECT_EQ(nullptr, pool.Buffer());
    EXPECT_EQ(0u, pool.AllocatedSize());
}

TEST(MemoryPoolReferenceTest, CapacityIsInFourByteWordsAndTheWholeBufferIsWritable) {
    const size_t counts[] = {1, 4, 7, 31, 257};
    for (size_t count : counts) {
        VxMemoryPool pool(count);
        ASSERT_NE(nullptr, pool.Buffer());
        EXPECT_EQ(count, pool.AllocatedSize());
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(pool.Buffer()) & 15u);
        XBYTE *bytes = static_cast<XBYTE *>(pool.Buffer());
        for (size_t i = 0; i < count * sizeof(XDWORD); ++i) bytes[i] = static_cast<XBYTE>(i ^ 0x5a);
        for (size_t i = 0; i < count * sizeof(XDWORD); ++i) EXPECT_EQ(static_cast<XBYTE>(i ^ 0x5a), bytes[i]);
    }
}

TEST(MemoryPoolReferenceTest, EqualSmallerAndZeroRequestsKeepCapacityPointerAndContents) {
    VxMemoryPool pool(19);
    ASSERT_NE(nullptr, pool.Buffer());
    void *buffer = pool.Buffer();
    memset(buffer, 0x37, 19 * sizeof(XDWORD));
    const size_t counts[] = {19, 7, 0, 1, 19};
    for (size_t count : counts) {
        pool.Allocate(count);
        EXPECT_EQ(buffer, pool.Buffer());
        EXPECT_EQ(19u, pool.AllocatedSize());
        const XBYTE *bytes = static_cast<const XBYTE *>(pool.Buffer());
        for (size_t i = 0; i < 19 * sizeof(XDWORD); ++i) EXPECT_EQ(0x37, bytes[i]);
    }
}

TEST(MemoryPoolReferenceTest, GrowthProvidesTheRequestedAlignedStorage) {
    VxMemoryPool pool(3);
    pool.Allocate(257);
    ASSERT_NE(nullptr, pool.Buffer());
    EXPECT_EQ(257u, pool.AllocatedSize());
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(pool.Buffer()) & 15u);
    XDWORD *words = static_cast<XDWORD *>(pool.Buffer());
    words[0] = 0x12345678; words[256] = 0x87654321;
    EXPECT_EQ(0x12345678u, words[0]);
    EXPECT_EQ(0x87654321u, words[256]);
}

TEST(MemoryPoolReferenceTest, MultiplicationOverflowLeavesExistingStorageAlone) {
    VxMemoryPool pool(4);
    ASSERT_NE(nullptr, pool.Buffer());
    void *buffer = pool.Buffer();
    memset(buffer, 0xa5, 4 * sizeof(XDWORD));
    pool.Allocate(SIZE_MAX / sizeof(XDWORD) + 1);
    EXPECT_EQ(buffer, pool.Buffer());
    EXPECT_EQ(4u, pool.AllocatedSize());
    EXPECT_EQ(0xa5, static_cast<XBYTE *>(pool.Buffer())[15]);
}

TEST(MemoryPoolReferenceTest, ConstViewsAndEmptyImplicitCopiesDoNotAllocate) {
    const VxMemoryPool empty;
    VxMemoryPool copy(empty), assigned;
    assigned = empty;
    EXPECT_EQ(nullptr, empty.Buffer());
    EXPECT_EQ(nullptr, copy.Buffer());
    EXPECT_EQ(nullptr, assigned.Buffer());
    EXPECT_EQ(0u, copy.AllocatedSize());
    EXPECT_EQ(0u, assigned.AllocatedSize());
}
