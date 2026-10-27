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

TEST_F(ConfigurationReferenceTest, MemoryParsingCountsPhysicalLinesIncludingEmptyOnes) {
    int line = 71;
    XString error("sentinel");
    ASSERT_TRUE(Config.BuildFromMemory("\n\n#comment\nkey=value\n\n", line, error));
    EXPECT_EQ(5, line);
    EXPECT_STREQ("OK", error.CStr());
    ASSERT_NE(nullptr, Config.GetEntry("key", FALSE));
}

TEST_F(ConfigurationReferenceTest, MemoryParsingProcessesTheFinalUnterminatedLine) {
    int line = 71;
    XString error("sentinel");
    ASSERT_TRUE(Config.BuildFromMemory("key=value", line, error));
    EXPECT_EQ(1, line);
    EXPECT_STREQ("OK", error.CStr());
    ASSERT_NE(nullptr, Config.GetEntry("key", FALSE));
    EXPECT_STREQ("value", Config.GetEntry("key", FALSE)->GetValue());
}

TEST_F(ConfigurationReferenceTest, FileAndMemoryAgreeForLFAndCRLFTextAndPreserveInputBytes) {
    const char *texts[] = {"#comment\n<A>\nkey=value\n</A>\n", "#comment\r\n<A>\r\nkey=value\r\n</A>\r\n"};
    for (const char *text : texts) {
        char bytes[128]; strcpy(bytes, text);
        int line = 71;
        XString error("sentinel");
        ASSERT_TRUE(Config.BuildFromMemory(bytes, line, error));
        EXPECT_EQ(4, line); EXPECT_STREQ("OK", error.CStr());
        EXPECT_STREQ(text, bytes);
        WriteInput(text);
        line = 71; error = "sentinel";
        ASSERT_TRUE(Config.BuildFromFile(Input, line, error));
        EXPECT_EQ(4, line); EXPECT_STREQ("OK", error.CStr());
        ASSERT_NE(nullptr, Config.GetSubSection("A", FALSE));
        EXPECT_STREQ("value", Config.GetSubSection("A", FALSE)->GetEntry("key")->GetValue());
    }
}

TEST_F(ConfigurationReferenceTest, DotsInParsedSectionTagsAreLiteralNameBytes) {
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromMemory("<A.B>\nkey=value\n</A.B>\n", line, error));
    VxConfigurationSection *literal = Config.GetSubSection("A.B", FALSE);
    ASSERT_NE(nullptr, literal);
    EXPECT_EQ(nullptr, Config.GetSubSection("A", FALSE));
    EXPECT_EQ(1u, Config.GetNumberOfSubSectionsRecursive());
    EXPECT_STREQ("value", literal->GetEntry("key")->GetValue());
}

TEST_F(ConfigurationReferenceTest, TagNamesRetainInnerSpacesAndIgnoreTextAfterTheFinalDelimiter) {
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromMemory("< space > trailing\nkey=value\n</ space > tail\n", line, error));
    VxConfigurationSection *section = Config.GetSubSection(" space ", FALSE);
    ASSERT_NE(nullptr, section);
    EXPECT_EQ(nullptr, Config.GetSubSection("space", FALSE));
    EXPECT_STREQ("value", section->GetEntry("key")->GetValue());
}

TEST_F(ConfigurationReferenceTest, EmptySectionNamesAndEmptyEntryKeysRemainValid) {
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromMemory("<>\n = value\n</>\n", line, error));
    VxConfigurationSection *section = Config.GetSubSection("", FALSE);
    ASSERT_NE(nullptr, section);
    ASSERT_NE(nullptr, section->GetEntry(""));
    EXPECT_STREQ("value", section->GetEntry("")->GetValue());
}

TEST_F(ConfigurationReferenceTest, OnlyHashStartsACommentAndValuesKeepInlineMarkers) {
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromMemory("# comment\n;key=value # literal\n", line, error));
    ASSERT_NE(nullptr, Config.GetEntry(";key", FALSE));
    EXPECT_STREQ("value # literal", Config.GetEntry(";key", FALSE)->GetValue());
    EXPECT_FALSE(Config.BuildFromMemory(";not a comment\n", line, error));
    EXPECT_EQ(0, line);
}

TEST_F(ConfigurationReferenceTest, DuplicateEntriesKeepTheFirstValueButRepeatedSectionTagsFail) {
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromMemory("key=first\nkey=second\n", line, error));
    EXPECT_STREQ("first", Config.GetEntry("key", FALSE)->GetValue());
    EXPECT_FALSE(Config.BuildFromMemory("<same>\n</same>\n<same>\n", line, error));
    EXPECT_EQ(2, line);
    EXPECT_EQ(1u, Config.GetNumberOfSubSections());
}

TEST_F(ConfigurationReferenceTest, ParseFailureReportsTheZeroBasedPhysicalLineAndRetainsTheParsedPrefix) {
    int line = 71; XString error;
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "fallback", "default"));
    ASSERT_TRUE(Config.AddEntry(nullptr, "stale", "old"));
    EXPECT_FALSE(Config.BuildFromMemory("# comment\n\nvalid=prefix\ninvalid\n", line, error));
    EXPECT_EQ(3, line);
    EXPECT_EQ(nullptr, Config.GetEntry("stale", FALSE));
    ASSERT_NE(nullptr, Config.GetEntry("valid", FALSE));
    EXPECT_STREQ("prefix", Config.GetEntry("valid", FALSE)->GetValue());
    ASSERT_NE(nullptr, Config.GetEntry("fallback", FALSE));
    EXPECT_FALSE(error.Empty());
}

TEST_F(ConfigurationReferenceTest, AnUnclosedSectionRemainsAnErrorInsteadOfSilentlyAcceptingTruncation) {
    int line = 71; XString error;
    EXPECT_FALSE(Config.BuildFromMemory("<open>\nkey=value\n", line, error));
    EXPECT_EQ(2, line);
    EXPECT_NE(XString::NOTFOUND, error.Find("Missing closing tag"));
}

TEST_F(ConfigurationReferenceTest, FileOpenFailureClearsMainEntriesAndLeavesDefaultsAndLineOutputIntact) {
    ASSERT_TRUE(Config.AddEntry(nullptr, "stale", "old"));
    ASSERT_TRUE(Config.AddDefaultEntry(nullptr, "fallback", "default"));
    int line = 71; XString error;
    remove(Input);
    EXPECT_FALSE(Config.BuildFromFile(Input, line, error));
    EXPECT_EQ(71, line);
    EXPECT_EQ(nullptr, Config.GetEntry("stale", FALSE));
    EXPECT_NE(nullptr, Config.GetEntry("fallback", FALSE));
    EXPECT_FALSE(error.Empty());
}

TEST_F(ConfigurationReferenceTest, FileParsingErrorsUseTheSamePhysicalLineAndAlwaysReleaseTheFile) {
    WriteInput("valid=prefix\n\ninvalid\n");
    for (int i = 0; i < 8; ++i) {
        int line = 71; XString error;
        EXPECT_FALSE(Config.BuildFromFile(Input, line, error));
        EXPECT_EQ(2, line);
        ASSERT_NE(nullptr, Config.GetEntry("valid", FALSE));
    }
    EXPECT_EQ(0, remove(Input));
}

TEST_F(ConfigurationReferenceTest, DataFileEntrypointsAreUnsupportedAndHaveNoTextFileSideEffects) {
    WriteInput("loaded=wrong\n");
    ASSERT_TRUE(Config.AddEntry(nullptr, "existing", "value"));
    XString error("sentinel");
    EXPECT_FALSE(Config.BuildFromDataFile(Input, error));
    EXPECT_STREQ("sentinel", error.CStr());
    EXPECT_NE(nullptr, Config.GetEntry("existing", FALSE));
    EXPECT_EQ(nullptr, Config.GetEntry("loaded", FALSE));
    FILE *file = fopen(Output, "wb"); ASSERT_NE(nullptr, file);
    EXPECT_EQ(8u, fwrite("sentinel", 1, 8, file)); EXPECT_EQ(0, fclose(file));
    EXPECT_FALSE(Config.SaveToDataFile(Output));
    EXPECT_STREQ("sentinel", ReadOutput().CStr());
}

TEST_F(ConfigurationReferenceTest, SavingUsesAccumulatedIndentationWithoutExtraBlankLinesOrDefaults) {
    VxConfiguration config(3);
    ASSERT_TRUE(config.AddEntry(nullptr, "root", "value"));
    ASSERT_TRUE(config.AddEntry("A", "a", "one"));
    ASSERT_TRUE(config.AddEntry("A.B", "b", "two"));
    ASSERT_TRUE(config.AddDefaultEntry(nullptr, "fallback", "omitted"));
    ASSERT_TRUE(config.SaveToFile(Output));
    EXPECT_STREQ("root = value\n<A>\n   a = one\n   <B>\n      b = two\n   </B>\n</A>\n", ReadOutput().CStr());
}

TEST_F(ConfigurationReferenceTest, ZeroIndentAndEmptyConfigurationsSaveAsExpected) {
    VxConfiguration config(0);
    ASSERT_TRUE(config.SaveToFile(Output));
    EXPECT_STREQ("", ReadOutput().CStr());
    ASSERT_TRUE(config.AddEntry("A.B", "key", "value"));
    ASSERT_TRUE(config.SaveToFile(Output));
    EXPECT_STREQ("<A>\n<B>\nkey = value\n</B>\n</A>\n", ReadOutput().CStr());
}

TEST_F(ConfigurationReferenceTest, SavingALiteralDottedSectionRoundTripsItsIdentityAndBytes) {
    VxConfigurationSection *section = Config.CreateSubSection(nullptr, "A.B");
    ASSERT_NE(nullptr, section);
    section->AddEntry("key", "value");
    ASSERT_TRUE(Config.SaveToFile(Output));
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromFile(Output, line, error));
    ASSERT_NE(nullptr, Config.GetSubSection("A.B", FALSE));
    EXPECT_EQ(nullptr, Config.GetSubSection("A", FALSE));
    EXPECT_STREQ("value", Config.GetSubSection("A.B", FALSE)->GetEntry("key")->GetValue());
}

TEST_F(ConfigurationReferenceTest, SaveFailureDoesNotReportSuccessForADirectory) {
    ASSERT_TRUE(Config.AddEntry(nullptr, "key", "value"));
    EXPECT_FALSE(Config.SaveToFile("."));
}

TEST_F(ConfigurationReferenceTest, LongConfigurationLinesAreProcessedAsCompleteLines) {
    char text[4096];
    strcpy(text, "key=");
    memset(text + 4, 'x', sizeof(text) - 6);
    text[sizeof(text) - 2] = '\n'; text[sizeof(text) - 1] = 0;
    WriteInput(text);
    int line = 0; XString error;
    ASSERT_TRUE(Config.BuildFromFile(Input, line, error));
    EXPECT_EQ(1, line);
    ASSERT_NE(nullptr, Config.GetEntry("key", FALSE));
    EXPECT_EQ(sizeof(text) - 6, strlen(Config.GetEntry("key", FALSE)->GetValue()));
}

