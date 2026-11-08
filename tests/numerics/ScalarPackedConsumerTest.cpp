#include <gtest/gtest.h>

#pragma pack(push, 1)
#include "VxScalar.h"
struct ScalarPackedFollowing {
    char Prefix;
    uint32_t Value;
};
#pragma pack(pop)

#include "VxFPUControlWord.h"

TEST(ScalarPackedConsumerTest, PackedHostHeadersPreserveScalarSizeAndAlignment) {
    EXPECT_EQ(5u, sizeof(ScalarPackedFollowing));
#if VXMATH_ENABLE_X87_EMULATION
    ASSERT_EQ(8u, alignof(VxScalar));
    ASSERT_EQ(alignof(unsigned), alignof(VxScalarContext));
#endif
    const XWORD saved = VxGetFPUControlWord();
    VxSetFPUControlWord(static_cast<XWORD>((saved & ~0x0f00) | 0x0300 | 0x003f));
    VxScalarContext arithmetic;
    VxScalarInitContext(arithmetic);
    const VxScalar first = VxScalarLoad(arithmetic, 16777217);
    const VxScalar second = VxScalarLoad(arithmetic, 0.25);
    const VxScalar result = VxScalarAdd(arithmetic, first, second);
    EXPECT_EQ(16777217.25, VxScalarStoreDouble(arithmetic, result));
    EXPECT_EQ(16777218.0f, VxScalarStore(arithmetic, result));
    VxSetFPUControlWord(saved);
}
