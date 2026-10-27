#include "VxConfiguration.h"
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

class ConfigurationView : public VxConfiguration {
public:
    using VxConfiguration::VxConfiguration;
    VxConfigurationSection *Root() const { return m_Root; }
    VxConfigurationSection *Defaults() const { return m_DefaultRoot; }
    static size_t RootOffset() { return offsetof(ConfigurationView, m_Root); }
    static size_t DefaultOffset() { return offsetof(ConfigurationView, m_DefaultRoot); }
    static size_t IndentOffset() { return offsetof(ConfigurationView, m_Indent); }
    using VxConfiguration::CreateSubSection;
    using VxConfiguration::GetSubSection;
    using VxConfiguration::ManageEntry;
    using VxConfiguration::ManageSection;
};

static void AcceptConfiguration(VxConfiguration);
template<class T> static auto ImplicitConfiguration(int) -> decltype(AcceptConfiguration(*static_cast<T *>(nullptr)), char());
template<class T> static long ImplicitConfiguration(...);

class ConfigurationReferenceTest : public ::testing::Test {
protected:
    ConfigurationView Config;
    const char *Input = "configuration-reference-input.txt";
    const char *Output = "configuration-reference-output.txt";
    void TearDown() override { remove(Input); remove(Output); }
    void WriteInput(const char *text) {
        FILE *file = fopen(Input, "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(strlen(text), fwrite(text, 1, strlen(text), file));
        EXPECT_EQ(0, fclose(file));
    }
    XString ReadOutput() {
        FILE *file = fopen(Output, "rt");
        if (!file) { ADD_FAILURE() << "Cannot read saved output"; return ""; }
        char buffer[4096];
        size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
        buffer[length] = 0;
        EXPECT_EQ(0, ferror(file));
        EXPECT_EQ(0, fclose(file));
        return buffer;
    }
};

TEST_F(ConfigurationReferenceTest, LayoutContainsTwoOwnedRootsAndTheIndentationSetting) {
    EXPECT_EQ(0u, ConfigurationView::RootOffset());
    EXPECT_EQ(sizeof(void *), ConfigurationView::DefaultOffset());
    EXPECT_EQ(2 * sizeof(void *), ConfigurationView::IndentOffset());
    EXPECT_EQ(sizeof(void *) == 4 ? 12u : 24u, sizeof(VxConfiguration));
    EXPECT_EQ(alignof(void *), alignof(VxConfiguration));
    EXPECT_EQ(sizeof(long), sizeof(ImplicitConfiguration<unsigned short>(0))); // Keep the current explicit constructor.
    ASSERT_NE(nullptr, Config.Root());
    EXPECT_STREQ("root", Config.Root()->GetName());
    EXPECT_EQ(nullptr, Config.Root()->GetParent());
    EXPECT_EQ(nullptr, Config.Defaults());
}

TEST_F(ConfigurationReferenceTest, DottedLookupWithoutADotStillSearchesMainAndDefaultRootEntries) {
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "fallback", "default"));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "shared", "default"));
    ASSERT_TRUE(Config.AddEntry(nullptr, "shared", "main"));
    ASSERT_NE(nullptr, Config.GetEntry("fallback", TRUE));
    EXPECT_EQ(Config.GetEntry("fallback", FALSE), Config.GetEntry("fallback", TRUE));
    ASSERT_NE(nullptr, Config.GetEntry("shared", TRUE));
    EXPECT_STREQ("main", Config.GetEntry("shared", TRUE)->GetValue());
}

TEST_F(ConfigurationReferenceTest, MissingMainChildEntryFallsBackToTheCorrespondingDefaultSection) {
    ASSERT_TRUE(Config.AddDefaultEntry("A.B", "fallback", "default"));
    ASSERT_TRUE(Config.AddDefaultEntry("A.B", "shared", "default"));
    ASSERT_NE(nullptr, Config.CreateSubSection(nullptr, "A"));
    ASSERT_NE(nullptr, Config.CreateSubSection("A", "B"));
    ASSERT_TRUE(Config.AddEntry("A.B", "shared", "main"));
    ASSERT_NE(nullptr, Config.GetEntry("A.B.shared", TRUE));
    ASSERT_NE(nullptr, Config.GetEntry("A.B.fallback", TRUE));
    EXPECT_STREQ("main", Config.GetEntry("A.B.shared", TRUE)->GetValue());
    EXPECT_STREQ("default", Config.GetEntry("A.B.fallback", TRUE)->GetValue());
}

TEST_F(ConfigurationReferenceTest, ClearDefaultRetainsTheAllocatedDefaultRootObject) {
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "key", "default"));
    VxConfigurationSection *root = Config.Defaults();
    ASSERT_NE(nullptr, root);
    Config.ClearDefault();
    EXPECT_EQ(root, Config.Defaults());
    EXPECT_EQ(nullptr, Config.GetEntry("key", FALSE));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "next", "value"));
    EXPECT_EQ(root, Config.Defaults());
    EXPECT_STREQ("default", Config.Defaults()->GetName());
}

TEST_F(ConfigurationReferenceTest, ClearingOneTreeLeavesTheOtherTreeAndRootIdentityIntact) {
    ASSERT_TRUE(Config.AddEntry(nullptr, "main", "value"));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "default", "fallback"));
    VxConfigurationSection *root = Config.Root();
    Config.Clear();
    EXPECT_EQ(root, Config.Root());
    EXPECT_EQ(nullptr, Config.GetEntry("main", FALSE));
    ASSERT_NE(nullptr, Config.GetEntry("default", FALSE));
    ASSERT_TRUE(Config.AddEntry(nullptr, "main", "new"));
    Config.ClearDefault();
    ASSERT_NE(nullptr, Config.GetEntry("main", FALSE));
    EXPECT_STREQ("new", Config.GetEntry("main", FALSE)->GetValue());
    EXPECT_EQ(nullptr, Config.GetEntry("default", FALSE));
}

TEST_F(ConfigurationReferenceTest, AllSixDefaultOverloadsPopulateOnlyTheDefaultTree) {
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "text", "value"));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "int", -17));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "long", -29L));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "uint", 31u));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "ulong", 43ul));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "float", 1.25f));
    EXPECT_EQ(0u, Config.GetNumberOfEntriesRecursive());
    const char *names[] = {"text", "int", "long", "uint", "ulong", "float"};
    const char *values[] = {"value", "-17", "-29", "31", "43", "1.250000"};
    for (int i = 0; i < 6; ++i) {
        VxConfigurationEntry *entry = Config.GetEntry(names[i], FALSE);
        ASSERT_NE(nullptr, entry);
        EXPECT_STREQ(values[i], entry->GetValue());
        EXPECT_EQ(Config.Defaults(), entry->GetParent());
    }
}

TEST_F(ConfigurationReferenceTest, AddEntryUsesAnExistingDefaultSectionWhenNoMainSectionExists) {
    ASSERT_TRUE(Config.AddDefaultEntry("defaults", "key", "original"));
    VxConfigurationSection *section = Config.GetSubSection("defaults", TRUE);
    ASSERT_NE(nullptr, section);
    VxConfigurationEntry *previous = nullptr;
    ASSERT_TRUE(Config.AddEntry("defaults", "key", "ignored", &previous));
    EXPECT_EQ(section->GetEntry("key"), previous);
    EXPECT_STREQ("original", previous->GetValue());
    ASSERT_TRUE(Config.AddEntry("defaults", "new", "value"));
    EXPECT_EQ(nullptr, Config.Root()->GetSubSection("defaults"));
    EXPECT_EQ(0u, Config.GetNumberOfEntriesRecursive());
    EXPECT_STREQ("value", section->GetEntry("new")->GetValue());
}

TEST_F(ConfigurationReferenceTest, HierarchicalInsertionReusesExistingPathPrefixes) {
    ASSERT_TRUE(Config.AddEntry("A.B", "first", 1));
    ASSERT_TRUE(Config.AddEntry("A.C", "second", 2));
    ASSERT_TRUE(Config.AddDefaultEntry("D.B", "first", 3));
    ASSERT_TRUE(Config.AddDefaultEntry("D.C", "second", 4));
    EXPECT_EQ(3u, Config.GetNumberOfSubSectionsRecursive());
    ASSERT_NE(nullptr, Config.GetEntry("A.C.second", TRUE));
    ASSERT_NE(nullptr, Config.GetEntry("D.C.second", TRUE));
    EXPECT_STREQ("2", Config.GetEntry("A.C.second", TRUE)->GetValue());
    EXPECT_STREQ("4", Config.GetEntry("D.C.second", TRUE)->GetValue());
}
