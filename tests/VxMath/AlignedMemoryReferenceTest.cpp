#include "VxMemory.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

TEST(AlignedMemoryReferenceTest, PowerOfTwoAlignmentsProvideWritableStorage) {
    const size_t sizes[] = {1, 7, 16, 31, 127, 1024};
    const size_t alignments[] = {4, 8, 16, 32, 64, 256};
    for (size_t alignment : alignments) for (size_t size : sizes) {
        void *buffer = VxNewAligned(size, alignment);
        ASSERT_NE(nullptr, buffer);
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(buffer) % alignment);
        memset(buffer, 0x5a, size);
        EXPECT_EQ(0x5a, static_cast<unsigned char *>(buffer)[size - 1]);
        VxDeleteAligned(buffer);
    }
}

TEST(AlignedMemoryReferenceTest, ExistingAlignmentNormalizationRemainsSupported) {
    const size_t requested[] = {0, 1, 3, 6, 24, 48};
    const size_t rounded[] = {sizeof(void *), sizeof(void *), sizeof(void *), 8, 32, 64};
    for (int i = 0; i < 6; ++i) {
        void *buffer = VxNewAligned(19, requested[i]);
        ASSERT_NE(nullptr, buffer);
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(buffer) % rounded[i]);
        memset(buffer, 0xa5, 19);
        EXPECT_EQ(0xa5, static_cast<unsigned char *>(buffer)[18]);
        VxDeleteAligned(buffer);
    }
}

TEST(AlignedMemoryReferenceTest, NullReleaseAndZeroSizeKeepTheirExistingBehavior) {
    VxDeleteAligned(nullptr);
    void *buffer = VxNewAligned(0, 16);
    if (buffer) EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(buffer) & 15u);
    VxDeleteAligned(buffer);
}

TEST(AlignedMemoryReferenceTest, AllocationPaddingCannotWrapIntoASmallRequest) {
    const size_t sizes[] = {SIZE_MAX, SIZE_MAX - 1, SIZE_MAX - 15};
    for (size_t size : sizes) {
        void *buffer = VxNewAligned(size, 16);
        EXPECT_EQ(nullptr, buffer);
        VxDeleteAligned(buffer);
    }
}

TEST(AlignedMemoryReferenceTest, UnrepresentableRoundedAlignmentIsRejected) {
    EXPECT_EQ(nullptr, VxNewAligned(1, SIZE_MAX));
    EXPECT_EQ(nullptr, VxNewAligned(1, SIZE_MAX / 2 + 2));
}
