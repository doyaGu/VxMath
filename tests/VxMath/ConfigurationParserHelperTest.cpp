#ifndef VX_LIB
#define VX_LIB
#endif
#ifndef VX_CONFIG_TEST_SOURCE
#define VX_CONFIG_TEST_SOURCE "../../src/VxConfiguration.cpp"
#endif
#include VX_CONFIG_TEST_SOURCE
#include <gtest/gtest.h>

class ConfigurationParserView : public VxConfiguration {
public:
    VxConfigurationSection *Root() const { return m_Root; }
    using VxConfiguration::CreateSubSection;
    using VxConfiguration::GetSubSection;
    using VxConfiguration::ManageEntry;
    using VxConfiguration::ManageSection;
};

TEST(ConfigurationParserHelperTest, PathAndEntryHelpersUseTheirSpecifiedRoot) {
    ConfigurationParserView config;
    VxConfigurationSection *child = config.CreateSubSection(config.Root(), "A.B", TRUE);
    ASSERT_NE(nullptr, child);
    EXPECT_EQ(child, config.GetSubSection(config.Root(), "A.B", TRUE));
    EXPECT_EQ(nullptr, config.GetSubSection(config.Root(), "A.B", FALSE));
    XString error;
    ASSERT_TRUE(config.ManageEntry(" key = value=tail ", child, error));
    ASSERT_NE(nullptr, child->GetEntry("key"));
    EXPECT_STREQ("value=tail", child->GetEntry("key")->GetValue());
}

TEST(ConfigurationParserHelperTest, SectionHelperReceivesTheTextAfterTheOpeningAngleBracket) {
    ConfigurationParserView config;
    VxConfigurationSection *current = config.Root();
    XString error;
    ASSERT_TRUE(config.ManageSection("A.B>", &current, error));
    ASSERT_NE(nullptr, current);
    EXPECT_STREQ("A.B", current->GetName());
    EXPECT_EQ(config.Root(), current->GetParent());
    ASSERT_TRUE(config.ManageSection("/A.B>", &current, error));
    EXPECT_EQ(config.Root(), current);
}

TEST(ConfigurationParserHelperTest, ShrinkTrimsSpacesTabsAndLineFeedsButNotCarriageReturns) {
    char text[] = " \t\nvalue\n\t ";
    EXPECT_STREQ("value", Shrink(text));
    char carriage[] = " value\r ";
    EXPECT_STREQ("value\r", Shrink(carriage));
}

TEST(ConfigurationParserHelperTest, EntryKeyAndSeparatorDoNotReduceTheValueCapacity) {
    ConfigurationParserView config;
    const int length = XString::MAX_LENGTH;
    XAP<char> line(new char[length + 9]);
    memcpy(line, "value = ", 8);
    memset(static_cast<char *>(line) + 8, 'x', length);
    line[length + 8] = 0;
    XString error;
    ASSERT_TRUE(config.ManageEntry(line, config.Root(), error));
    VxConfigurationEntry *entry = config.Root()->GetEntry("value");
    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(static_cast<size_t>(length), strlen(entry->GetValue()));
    EXPECT_EQ(0, memcmp(entry->GetValue(), static_cast<char *>(line) + 8, length + 1));
    EXPECT_EQ(0, memcmp(static_cast<char *>(line), "value = ", 8));
}
