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

TEST(ConfigurationCompatibilityTest, DataFileReadRetainsItsTextFileFallback) {
    FILE *file = fopen("readable.cfg", "wb");
    ASSERT_NE(nullptr, file);
    ASSERT_EQ(10u, fwrite("key=value\n", 1, 10, file));
    ASSERT_EQ(0, fclose(file));
    VxConfiguration config;
    XString error;
    ASSERT_TRUE(config.BuildFromDataFile("readable.cfg", error));
    ExpectEntry(config, "key", "value");
}

TEST(ConfigurationCompatibilityTest, DataFileSaveRetainsItsTextFileFallback) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "key", "value"));
    ASSERT_TRUE(config.SaveToDataFile("written.cfg"));
    FILE *file = fopen("written.cfg", "rb");
    ASSERT_NE(nullptr, file);
    char text[128] = {};
    fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    EXPECT_NE(nullptr, strstr(text, "key = value"));
}

TEST(ConfigurationCompatibilityTest, SemicolonCommentsStillLoad) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("; ordinary comment\nkey=value\n", line, error));
    ExpectEntry(config, "key", "value");
}

TEST(ConfigurationCompatibilityTest, ReopenedSectionsStillMergeTheirEntries) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A>\nx=one\n</A>\n<A>\ny=two\n</A>\n", line, error));
    ExpectEntry(config, "A.x", "one", TRUE);
    ExpectEntry(config, "A.y", "two", TRUE);
}

TEST(ConfigurationCompatibilityTest, DottedSectionTagsStillSupportHierarchicalLookup) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A.B>\nkey=value\n</A.B>\n", line, error));
    ExpectEntry(config, "A.B.key", "value", TRUE);
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

TEST(ConfigurationCompatibilityTest, RealHierarchyPrecedesLiteralDottedNames) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A.B>\nkey=literal\n</A.B>\n<A>\n<B>\nkey=hierarchy\n</B>\n</A>\n", line, error));
    ExpectEntry(config, "A.B.key", "hierarchy", TRUE);
    ASSERT_NE(nullptr, config.GetSubSection("A.B", FALSE));
    EXPECT_STREQ("literal", config.GetSubSection("A.B", FALSE)->GetEntry("key")->GetValue());
}

TEST(ConfigurationCompatibilityTest, FullDottedClosingTagClosesTheNestedHierarchy) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A>\n<B>\nkey=value\n</A.B>\nroot=next\n", line, error));
    ExpectEntry(config, "A.B.key", "value", TRUE);
    ExpectEntry(config, "root", "next");
}
