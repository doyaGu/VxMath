#include "VxConfiguration.h"
#include <gtest/gtest.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

class ConfigurationEntryLayout : public VxConfigurationEntry {
public:
    static size_t NameOffset() { return offsetof(ConfigurationEntryLayout, m_Name); }
    static size_t ParentOffset() { return offsetof(ConfigurationEntryLayout, m_Parent); }
    static size_t ValueOffset() { return offsetof(ConfigurationEntryLayout, m_Value); }
};

class ConfigurationEntryReferenceTest : public ::testing::Test {
protected:
    VxConfiguration Config;
    VxConfigurationSection *Section = nullptr;
    VxConfigurationEntry *Entry = nullptr;

    void SetUp() override {
        Section = Config.CreateSubSection(nullptr, "audit");
        ASSERT_NE(nullptr, Section);
        Section->AddEntry("key", "initial");
        Entry = Section->GetEntry("key");
        ASSERT_NE(nullptr, Entry);
    }
};

TEST_F(ConfigurationEntryReferenceTest, LayoutContainsTwoOwnedStringsAndOneBorrowedParent) {
    EXPECT_EQ(0u, ConfigurationEntryLayout::NameOffset());
    EXPECT_EQ(sizeof(XString), ConfigurationEntryLayout::ParentOffset());
    EXPECT_EQ(sizeof(XString) + sizeof(void *), ConfigurationEntryLayout::ValueOffset());
    EXPECT_EQ(2 * sizeof(XString) + sizeof(void *), sizeof(VxConfigurationEntry));
    EXPECT_EQ(alignof(void *), alignof(VxConfigurationEntry));
    EXPECT_EQ(Section, Entry->GetParent());
    EXPECT_STREQ("key", Entry->GetName());
    EXPECT_STREQ("initial", Entry->GetValue());
}

TEST_F(ConfigurationEntryReferenceTest, StringConstructionOwnsTheNameAndValueBytes) {
    char name[] = "owned", value[] = "  unchanged = value  ";
    Section->AddEntry(name, value);
    VxConfigurationEntry *entry = Section->GetEntry("owned");
    ASSERT_NE(nullptr, entry);
    name[0] = 'X';
    value[0] = 'X';
    EXPECT_STREQ("owned", entry->GetName());
    EXPECT_STREQ("  unchanged = value  ", entry->GetValue());
    EXPECT_EQ(Section, entry->GetParent());
}

TEST_F(ConfigurationEntryReferenceTest, EachNumericConstructorUsesItsMatchingDecimalFormat) {
    Section->AddEntry("int", -17);
    Section->AddEntry("long", -29L);
    Section->AddEntry("uint", 31u);
    Section->AddEntry("ulong", 43ul);
    Section->AddEntry("float", 1.25f);
    const char *names[] = {"int", "long", "uint", "ulong", "float"};
    const char *expected[] = {"-17", "-29", "31", "43", "1.250000"};
    for (int i = 0; i < 5; ++i) {
        VxConfigurationEntry *entry = Section->GetEntry(names[i]);
        ASSERT_NE(nullptr, entry);
        EXPECT_STREQ(names[i], entry->GetName());
        EXPECT_STREQ(expected[i], entry->GetValue());
        EXPECT_EQ(Section, entry->GetParent());
    }
}

TEST_F(ConfigurationEntryReferenceTest, StringSetterCopiesInputAndAcceptsItsOwnValue) {
    char value[] = "new value";
    Entry->SetValue(value);
    value[0] = 'X';
    EXPECT_STREQ("new value", Entry->GetValue());
    Entry->SetValue(Entry->GetValue());
    EXPECT_STREQ("new value", Entry->GetValue());
    EXPECT_STREQ("key", Entry->GetName());
    EXPECT_EQ(Section, Entry->GetParent());
}

TEST_F(ConfigurationEntryReferenceTest, EmptyAndNullStringsProduceAnEmptyValue) {
    Entry->SetValue("");
    ASSERT_NE(nullptr, Entry->GetValue());
    EXPECT_STREQ("", Entry->GetValue());
    Entry->SetValue("temporary");
    Entry->SetValue(static_cast<const char *>(nullptr));
    ASSERT_NE(nullptr, Entry->GetValue());
    EXPECT_STREQ("", Entry->GetValue());
    Section->AddEntry("empty", static_cast<const char *>(nullptr));
    ASSERT_NE(nullptr, Section->GetEntry("empty"));
    EXPECT_STREQ("", Section->GetEntry("empty")->GetValue());
}

TEST_F(ConfigurationEntryReferenceTest, SignedIntegerReadsAcceptWhitespaceSignAndDecimalPrefix) {
    const char *texts[] = {"0", "  +17 suffix", "\t-29tail", "012", "0x10", "2147483647", "-2147483648"};
    const int expected[] = {0, 17, -29, 12, 0, INT_MAX, INT_MIN};
    for (int i = 0; i < 7; ++i) {
        Entry->SetValue(texts[i]);
        int value = 71;
        long wide = 71;
        ASSERT_TRUE(Entry->GetValueAsInteger(value));
        ASSERT_TRUE(Entry->GetValueAsLong(wide));
        EXPECT_EQ(expected[i], value);
        EXPECT_EQ(expected[i], wide);
    }
}

TEST_F(ConfigurationEntryReferenceTest, UnsignedReadsAcceptDecimalPrefixAndAnOptionalMinusSign) {
    const char *texts[] = {"0", "  +17 suffix", "012", "0x10", "4294967295", "-1"};
    const unsigned int expected[] = {0, 17, 12, 0, UINT_MAX, UINT_MAX};
    for (int i = 0; i < 6; ++i) {
        Entry->SetValue(texts[i]);
        unsigned int value = 71;
        unsigned long wide = 71;
        ASSERT_TRUE(Entry->GetValueAsUnsignedInteger(value));
        ASSERT_TRUE(Entry->GetValueAsUnsignedLong(wide));
        EXPECT_EQ(expected[i], value);
        EXPECT_EQ(i == 5 ? ULONG_MAX : static_cast<unsigned long>(expected[i]), wide);
    }
}

TEST_F(ConfigurationEntryReferenceTest, FloatReadsAcceptDecimalExponentAndTrailingText) {
    const char *texts[] = {"0", "  +1.25 suffix", "\t-2.5tail", "6.25e1", ".5", "-0.000000"};
    const float expected[] = {0.0f, 1.25f, -2.5f, 62.5f, 0.5f, -0.0f};
    for (int i = 0; i < 6; ++i) {
        Entry->SetValue(texts[i]);
        float value = 71.0f;
        ASSERT_TRUE(Entry->GetValueAsFloat(value));
        EXPECT_EQ(0, memcmp(&expected[i], &value, sizeof(value)));
    }
}

TEST_F(ConfigurationEntryReferenceTest, FailedNumericReadsLeaveEveryOutputUnchanged) {
    const char *texts[] = {"", " \t\r\n", "word", "+", "-", "not 23"};
    for (const char *text : texts) {
        Entry->SetValue(text);
        int integer = 71;
        long longValue = 73;
        unsigned int unsignedValue = 79;
        unsigned long unsignedLong = 83;
        float real = 89.0f;
        EXPECT_FALSE(Entry->GetValueAsInteger(integer));
        EXPECT_FALSE(Entry->GetValueAsLong(longValue));
        EXPECT_FALSE(Entry->GetValueAsUnsignedInteger(unsignedValue));
        EXPECT_FALSE(Entry->GetValueAsUnsignedLong(unsignedLong));
        EXPECT_FALSE(Entry->GetValueAsFloat(real));
        EXPECT_EQ(71, integer);
        EXPECT_EQ(73, longValue);
        EXPECT_EQ(79u, unsignedValue);
        EXPECT_EQ(83ul, unsignedLong);
        EXPECT_EQ(89.0f, real);
    }
}

TEST_F(ConfigurationEntryReferenceTest, IntegerSettersPreserveTheirFullNativeRanges) {
    const int integers[] = {INT_MIN, 0, INT_MAX};
    for (int value : integers) {
        Entry->SetValue(value);
        int result = 17;
        ASSERT_TRUE(Entry->GetValueAsInteger(result));
        EXPECT_EQ(value, result);
    }
    const long longs[] = {LONG_MIN, 0, LONG_MAX};
    for (long value : longs) {
        Entry->SetValue(value);
        long result = 17;
        ASSERT_TRUE(Entry->GetValueAsLong(result));
        EXPECT_EQ(value, result);
    }
    const unsigned int unsigneds[] = {0, UINT_MAX};
    for (unsigned int value : unsigneds) {
        Entry->SetValue(value);
        unsigned int result = 17;
        ASSERT_TRUE(Entry->GetValueAsUnsignedInteger(result));
        EXPECT_EQ(value, result);
    }
    const unsigned long unsignedLongs[] = {0, ULONG_MAX};
    for (unsigned long value : unsignedLongs) {
        Entry->SetValue(value);
        unsigned long result = 17;
        ASSERT_TRUE(Entry->GetValueAsUnsignedLong(result));
        EXPECT_EQ(value, result);
    }
}

TEST_F(ConfigurationEntryReferenceTest, FloatSetterRetainsSixFractionalPlacesAndSignedZero) {
    const float values[] = {0.0f, -0.0f, 1.25f, -2.5f, FLT_MIN};
    const char *expected[] = {"0.000000", "-0.000000", "1.250000", "-2.500000", "0.000000"};
    for (int i = 0; i < 5; ++i) {
        Entry->SetValue(values[i]);
        EXPECT_STREQ(expected[i], Entry->GetValue());
    }
}

TEST_F(ConfigurationEntryReferenceTest, LargeFiniteFloatSetterKeepsEveryDecimalDigit) {
    const float values[] = {0x1p100f, FLT_MAX, -FLT_MAX};
    const char *expected[] = {
        "1267650600228229401496703205376.000000",
        "340282346638528859811704183484516925440.000000",
        "-340282346638528859811704183484516925440.000000"
    };
    for (int i = 0; i < 3; ++i) {
        Entry->SetValue(values[i]);
        EXPECT_STREQ(expected[i], Entry->GetValue());
        float result = 0.0f;
        ASSERT_TRUE(Entry->GetValueAsFloat(result));
        EXPECT_EQ(values[i], result);
    }
}

TEST_F(ConfigurationEntryReferenceTest, FloatConstructorAlsoKeepsLargeFiniteValues) {
    Section->AddEntry("large", -FLT_MAX);
    VxConfigurationEntry *entry = Section->GetEntry("large");
    ASSERT_NE(nullptr, entry);
    EXPECT_STREQ("-340282346638528859811704183484516925440.000000", entry->GetValue());
    float result = 0.0f;
    ASSERT_TRUE(entry->GetValueAsFloat(result));
    EXPECT_EQ(-FLT_MAX, result);
}

TEST_F(ConfigurationEntryReferenceTest, EntryDestructionDoesNotDestroyItsBorrowedParent) {
    ASSERT_TRUE(Section->DeleteEntry("key"));
    EXPECT_STREQ("audit", Section->GetName());
    Section->AddEntry("next", 23);
    ASSERT_NE(nullptr, Section->GetEntry("next"));
    EXPECT_STREQ("23", Section->GetEntry("next")->GetValue());
}
