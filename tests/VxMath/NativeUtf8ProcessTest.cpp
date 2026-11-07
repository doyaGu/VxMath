#include <gtest/gtest.h>
#include <windows.h>

TEST(NativeUtf8Process, ManifestSelectsTheUtf8ProcessCodePage) {
    ASSERT_EQ(GetACP(), CP_UTF8);
}
