#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>

static void ExpectEntry(VxConfiguration &config, const char *name, const char *expected, XBOOL dotted = FALSE) {
    VxConfigurationEntry *entry = config.GetEntry(name, dotted);
    ASSERT_NE(nullptr, entry);
    EXPECT_STREQ(expected, entry->GetValue());
}

TEST(ConfigurationCompatibilityTest, FailedFileOpenRetainsTheExistingConfiguration) {
    remove("missing.cfg");
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "keep", "value"));
    int line = 123;
    XString error;
    ASSERT_FALSE(config.BuildFromFile("missing.cfg", line, error));
    ExpectEntry(config, "keep", "value");
}

TEST(ConfigurationCompatibilityTest, NullMemoryInputRetainsTheExistingConfiguration) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "keep", "value"));
    int line = 123;
    XString error;
    ASSERT_FALSE(config.BuildFromMemory(nullptr, line, error));
    ExpectEntry(config, "keep", "value");
}

TEST(ConfigurationCompatibilityTest, MemoryInputMayBorrowAnExistingEntryValue) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "input", "loaded=value\n"));
    const char *input = config.GetEntry("input", FALSE)->GetValue();
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory(input, line, error));
    ExpectEntry(config, "loaded", "value");
}
