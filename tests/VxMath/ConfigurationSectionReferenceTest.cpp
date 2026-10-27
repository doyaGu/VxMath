#include "VxConfiguration.h"
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

class ConfigurationSectionLayout : public VxConfigurationSection {
public:
    static size_t ParentOffset() { return offsetof(ConfigurationSectionLayout, m_Parent); }
    static size_t NameOffset() { return offsetof(ConfigurationSectionLayout, m_Name); }
    static size_t EntriesOffset() { return offsetof(ConfigurationSectionLayout, m_Entries); }
    static size_t SectionsOffset() { return offsetof(ConfigurationSectionLayout, m_SubSections); }
};

class ConfigurationSectionReferenceTest : public ::testing::Test {
protected:
    VxConfiguration Config;
    VxConfigurationSection *Section = nullptr;
    void SetUp() override {
        Section = Config.CreateSubSection(nullptr, "audit");
        ASSERT_NE(nullptr, Section);
    }
};

TEST_F(ConfigurationSectionReferenceTest, LayoutStoresBorrowedParentNameAndTwoOwnedTables) {
    EXPECT_EQ(0u, ConfigurationSectionLayout::ParentOffset());
    EXPECT_EQ(sizeof(void *), ConfigurationSectionLayout::NameOffset());
    EXPECT_EQ(sizeof(void *) + sizeof(XString), ConfigurationSectionLayout::EntriesOffset());
    EXPECT_EQ(sizeof(void *) + sizeof(XString) + sizeof(HashOfEntry), ConfigurationSectionLayout::SectionsOffset());
    EXPECT_EQ(sizeof(void *) + sizeof(XString) + sizeof(HashOfEntry) + sizeof(HashOfSection), sizeof(VxConfigurationSection));
    EXPECT_EQ(alignof(void *), alignof(VxConfigurationSection));
    EXPECT_NE(nullptr, Section->GetParent());
    EXPECT_STREQ("audit", Section->GetName());
}

TEST_F(ConfigurationSectionReferenceTest, EmptyCountsAndIteratorsStayEmpty) {
    EXPECT_EQ(0u, Section->GetNumberOfEntries());
    EXPECT_EQ(0u, Section->GetNumberOfSubSections());
    EXPECT_EQ(0u, Section->GetNumberOfEntriesRecursive());
    EXPECT_EQ(0u, Section->GetNumberOfSubSectionsRecursive());
    ConstEntryIt entries = Section->BeginChildEntry();
    ConstSectionIt sections = Section->BeginChildSection();
    EXPECT_EQ(nullptr, Section->GetNextChildEntry(entries));
    EXPECT_EQ(nullptr, Section->GetNextChildEntry(entries));
    EXPECT_EQ(nullptr, Section->GetNextChildSection(sections));
    EXPECT_EQ(nullptr, Section->GetNextChildSection(sections));
}

TEST_F(ConfigurationSectionReferenceTest, RemoveEntryKeepsIdentityParentAndExistingStringStorage) {
    Section->AddEntry("owned", "original");
    VxConfigurationEntry *entry = Section->GetEntry("owned");
    ASSERT_NE(nullptr, entry);
    const char *name = entry->GetName(), *value = entry->GetValue();
    VxConfigurationEntry *removed = Section->RemoveEntry("owned");
    ASSERT_NE(nullptr, removed);
    EXPECT_EQ(entry, removed);
    EXPECT_EQ(Section, removed->GetParent());
    EXPECT_EQ(name, removed->GetName());
    EXPECT_EQ(value, removed->GetValue());
    EXPECT_EQ(nullptr, Section->GetEntry("owned"));
    EXPECT_EQ(0u, Section->GetNumberOfEntries());
    removed->SetValue("after removal");
    EXPECT_STREQ("after removal", removed->GetValue());
    Section->Clear();
    EXPECT_STREQ("owned", removed->GetName());
    delete removed;
}

TEST(ConfigurationSectionLifetimeTest, RemovedEntrySurvivesItsFormerOwner) {
    VxConfigurationEntry *removed;
    VxConfigurationSection *formerParent;
    {
        VxConfiguration config;
        formerParent = config.CreateSubSection(nullptr, "parent");
        ASSERT_NE(nullptr, formerParent);
        formerParent->AddEntry("key", "value");
        VxConfigurationEntry *original = formerParent->GetEntry("key");
        removed = formerParent->RemoveEntry(original->GetName());
        ASSERT_NE(nullptr, removed);
        EXPECT_EQ(original, removed);
    }
    // Parent is borrowed and may now be stale; never dereference it.
    EXPECT_EQ(formerParent, removed->GetParent());
    EXPECT_STREQ("key", removed->GetName());
    removed->SetValue(37);
    EXPECT_STREQ("37", removed->GetValue());
    delete removed;
}

TEST_F(ConfigurationSectionReferenceTest, MissingRemovalsAndDeletesLeaveExistingObjectsAlone) {
    Section->AddEntry("key", "value");
    VxConfigurationSection *child = Section->CreateSubSection("child");
    EXPECT_EQ(nullptr, Section->RemoveEntry("missing"));
    EXPECT_EQ(nullptr, Section->RemoveSection("missing"));
    EXPECT_FALSE(Section->DeleteEntry("missing"));
    EXPECT_FALSE(Section->DeleteSection("missing"));
    EXPECT_EQ(child, Section->GetSubSection("child"));
    EXPECT_EQ(1u, Section->GetNumberOfEntries());
    EXPECT_EQ(1u, Section->GetNumberOfSubSections());
}

TEST_F(ConfigurationSectionReferenceTest, RemoveSectionTransfersTheWholeOriginalObjectGraph) {
    VxConfigurationSection *child = Section->CreateSubSection("child");
    ASSERT_NE(nullptr, child);
    VxConfigurationSection *grandchild = child->CreateSubSection("grandchild");
    ASSERT_NE(nullptr, grandchild);
    child->AddEntry("direct", "first");
    grandchild->AddEntry("deep", "second");
    VxConfigurationEntry *direct = child->GetEntry("direct");
    VxConfigurationEntry *deep = grandchild->GetEntry("deep");
    const char *name = child->GetName();
    VxConfigurationSection *removed = Section->RemoveSection(name);
    ASSERT_NE(nullptr, removed);
    EXPECT_EQ(child, removed);
    EXPECT_EQ(name, removed->GetName());
    EXPECT_EQ(Section, removed->GetParent());
    EXPECT_EQ(grandchild, removed->GetSubSection("grandchild"));
    EXPECT_EQ(direct, removed->GetEntry("direct"));
    EXPECT_EQ(deep, removed->GetSubSection("grandchild")->GetEntry("deep"));
    EXPECT_EQ(nullptr, Section->GetSubSection("child"));
    EXPECT_EQ(0u, Section->GetNumberOfSubSectionsRecursive());
    Section->Clear();
    EXPECT_EQ(2u, removed->GetNumberOfEntriesRecursive());
    EXPECT_STREQ("second", removed->GetSubSection("grandchild")->GetEntry("deep")->GetValue());
    delete removed;
}

TEST(ConfigurationSectionLifetimeTest, RemovedSectionSurvivesItsFormerOwnerWithChildLinksIntact) {
    VxConfigurationSection *removed, *child, *formerParent;
    {
        VxConfiguration config;
        formerParent = config.CreateSubSection(nullptr, "parent");
        ASSERT_NE(nullptr, formerParent);
        VxConfigurationSection *original = formerParent->CreateSubSection("removed");
        ASSERT_NE(nullptr, original);
        child = original->CreateSubSection("child");
        ASSERT_NE(nullptr, child);
        child->AddEntry("key", "value");
        removed = formerParent->RemoveSection("removed");
        ASSERT_NE(nullptr, removed);
        EXPECT_EQ(original, removed);
    }
    EXPECT_EQ(formerParent, removed->GetParent());
    EXPECT_EQ(child, removed->GetSubSection("child"));
    EXPECT_EQ(removed, removed->GetSubSection("child")->GetParent());
    EXPECT_STREQ("value", removed->GetSubSection("child")->GetEntry("key")->GetValue());
    delete removed;
}

template<class T> static void CheckSectionInsertion(VxConfigurationSection *section, T first, T second, const char *text) {
    VxConfigurationEntry *existing = reinterpret_cast<VxConfigurationEntry *>(section);
    section->AddEntry("key", first, &existing);
    EXPECT_EQ(nullptr, existing);
    VxConfigurationEntry *entry = section->GetEntry("key");
    ASSERT_NE(nullptr, entry);
    EXPECT_STREQ(text, entry->GetValue());
    section->AddEntry("key", second, &existing);
    EXPECT_EQ(entry, existing);
    EXPECT_EQ(entry, section->GetEntry("key"));
    EXPECT_EQ(1u, section->GetNumberOfEntries());
    EXPECT_STREQ(text, entry->GetValue());
    section->AddEntry("key", second);
    EXPECT_STREQ(text, entry->GetValue());
}

TEST_F(ConfigurationSectionReferenceTest, StringInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, "first", "second", "first");
}
TEST_F(ConfigurationSectionReferenceTest, IntegerInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, -17, 23, "-17");
}
TEST_F(ConfigurationSectionReferenceTest, LongInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, -29L, 31L, "-29");
}
TEST_F(ConfigurationSectionReferenceTest, UnsignedInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, 37u, 41u, "37");
}
TEST_F(ConfigurationSectionReferenceTest, UnsignedLongInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, 43ul, 47ul, "43");
}
TEST_F(ConfigurationSectionReferenceTest, FloatInsertionReportsExistingEntryWithoutOverwriting) {
    CheckSectionInsertion(Section, 1.25f, 2.5f, "1.250000");
}

TEST_F(ConfigurationSectionReferenceTest, DuplicateSectionCreationReturnsNullAndPreservesTheOriginal) {
    VxConfigurationSection *child = Section->CreateSubSection("child");
    ASSERT_NE(nullptr, child);
    child->AddEntry("key", "value");
    EXPECT_EQ(nullptr, Section->CreateSubSection("child"));
    EXPECT_EQ(child, Section->GetSubSection("child"));
    EXPECT_EQ(1u, Section->GetNumberOfSubSections());
    EXPECT_STREQ("value", child->GetEntry("key")->GetValue());
}

TEST_F(ConfigurationSectionReferenceTest, NamesAreCaseSensitiveLiteralKeysAndAreCopied) {
    char name[] = "a.b";
    VxConfigurationSection *child = Section->CreateSubSection(name);
    ASSERT_NE(nullptr, child);
    name[0] = 'X';
    EXPECT_EQ(child, Section->GetSubSection("a.b"));
    EXPECT_EQ(nullptr, Section->GetSubSection("a"));
    EXPECT_EQ(nullptr, Section->GetSubSection("A.b"));
    EXPECT_STREQ("a.b", child->GetName());
    Section->AddEntry("key.with.dots", "value");
    Section->AddEntry("", "empty name");
    EXPECT_EQ(nullptr, Section->GetEntry("Key.with.dots"));
    ASSERT_NE(nullptr, Section->GetEntry("key.with.dots"));
    EXPECT_STREQ("value", Section->GetEntry("key.with.dots")->GetValue());
    ASSERT_NE(nullptr, Section->GetEntry(""));
    EXPECT_STREQ("empty name", Section->GetEntry("")->GetValue());
}

TEST_F(ConfigurationSectionReferenceTest, DeletesAcceptNamesBorrowedFromTheDeletedObjects) {
    Section->AddEntry("entry-name-to-delete", "value");
    VxConfigurationEntry *entry = Section->GetEntry("entry-name-to-delete");
    ASSERT_NE(nullptr, entry);
    EXPECT_TRUE(Section->DeleteEntry(entry->GetName()));
    EXPECT_EQ(nullptr, Section->GetEntry("entry-name-to-delete"));
    VxConfigurationSection *child = Section->CreateSubSection("section-name-to-delete");
    ASSERT_NE(nullptr, child);
    child->AddEntry("deep", "value");
    EXPECT_TRUE(Section->DeleteSection(child->GetName()));
    EXPECT_EQ(nullptr, Section->GetSubSection("section-name-to-delete"));
}

TEST_F(ConfigurationSectionReferenceTest, RecursiveCountsIncludeAllDescendantsAndExcludeTheSectionItself) {
    Section->AddEntry("root", 1);
    VxConfigurationSection *first = Section->CreateSubSection("first");
    VxConfigurationSection *second = Section->CreateSubSection("second");
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    first->AddEntry("a", 2);
    first->AddEntry("b", 3);
    VxConfigurationSection *deep = first->CreateSubSection("deep");
    ASSERT_NE(nullptr, deep);
    deep->AddEntry("c", 4);
    EXPECT_EQ(2u, Section->GetNumberOfSubSections());
    EXPECT_EQ(3u, Section->GetNumberOfSubSectionsRecursive());
    EXPECT_EQ(1u, Section->GetNumberOfEntries());
    EXPECT_EQ(4u, Section->GetNumberOfEntriesRecursive());
    EXPECT_EQ(0u, deep->GetNumberOfSubSectionsRecursive());
}

TEST_F(ConfigurationSectionReferenceTest, IteratorsVisitEveryRemainingObjectOnceAfterGrowthAndRemoval) {
    for (int i = 0; i < 100; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "key-%d", i);
        Section->AddEntry(name, i);
        ASSERT_NE(nullptr, Section->CreateSubSection(name));
    }
    for (int i = 0; i < 100; i += 3) {
        char name[32];
        snprintf(name, sizeof(name), "key-%d", i);
        delete Section->RemoveEntry(name);
        delete Section->RemoveSection(name);
    }
    bool seenEntries[100] = {}, seenSections[100] = {};
    ConstEntryIt entries = Section->BeginChildEntry();
    ConstSectionIt sections = Section->BeginChildSection();
    int entryCount = 0, sectionCount = 0;
    while (VxConfigurationEntry *entry = Section->GetNextChildEntry(entries)) {
        ASSERT_LT(entryCount++, 100);
        int index = -1;
        ASSERT_EQ(1, sscanf(entry->GetName(), "key-%d", &index));
        ASSERT_GE(index, 0); ASSERT_LT(index, 100);
        EXPECT_FALSE(seenEntries[index]); seenEntries[index] = true;
        EXPECT_NE(0, index % 3);
    }
    while (VxConfigurationSection *section = Section->GetNextChildSection(sections)) {
        ASSERT_LT(sectionCount++, 100);
        int index = -1;
        ASSERT_EQ(1, sscanf(section->GetName(), "key-%d", &index));
        ASSERT_GE(index, 0); ASSERT_LT(index, 100);
        EXPECT_FALSE(seenSections[index]); seenSections[index] = true;
        EXPECT_NE(0, index % 3);
    }
    EXPECT_EQ(66, entryCount); EXPECT_EQ(66, sectionCount);
    EXPECT_EQ(nullptr, Section->GetNextChildEntry(entries));
    EXPECT_EQ(nullptr, Section->GetNextChildSection(sections));
}

TEST_F(ConfigurationSectionReferenceTest, ClearReleasesDescendantsAndPreservesNameParentAndReusability) {
    const char *name = Section->GetName();
    VxConfigurationSection *parent = Section->GetParent();
    for (int round = 0; round < 4; ++round) {
        for (int i = 0; i < 40; ++i) {
            char key[24];
            snprintf(key, sizeof(key), "key-%d", i);
            Section->AddEntry(key, i);
            VxConfigurationSection *child = Section->CreateSubSection(key);
            ASSERT_NE(nullptr, child);
            child->AddEntry("deep", i);
        }
        Section->Clear(); Section->Clear();
        EXPECT_EQ(name, Section->GetName());
        EXPECT_EQ(parent, Section->GetParent());
        EXPECT_EQ(0u, Section->GetNumberOfEntriesRecursive());
        EXPECT_EQ(0u, Section->GetNumberOfSubSectionsRecursive());
    }
}

TEST(ConfigurationSectionConsumerTest, RegistryWritesStillReplaceExistingValuesAndTypeMetadata) {
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, "Software\\Virtools\\UserConfig\\BallanceSectionAudit");
    const char *section = sizeof(void *) == 4 ? "x86" : "x64";
    config.DeleteSection(section);
    config.OpenSection(section, VxConfig::WRITE);
    config.WriteStringEntry("value", "initial");
    config.WriteIntegerEntry("value", 37);
    config.CloseSection(section);
    config.OpenSection(section, VxConfig::READ);
    char text[32] = {};
    int value = 0;
    VxConfigValueType type = VXCONFIG_VALUE_STRING;
    EXPECT_TRUE(config.ReadIntegerEntry("value", &value));
    EXPECT_EQ(37, value);
    EXPECT_TRUE(config.ReadStringEntry("value", text, sizeof(text)));
    EXPECT_STREQ("37", text);
    EXPECT_TRUE(config.GetEntryType("value", &type));
    EXPECT_EQ(VXCONFIG_VALUE_INTEGER, type);
    config.CloseSection(section);
    EXPECT_TRUE(config.DeleteSection(section));
}
