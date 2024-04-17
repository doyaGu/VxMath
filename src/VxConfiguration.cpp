#include "VxConfiguration.h"

#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

VxConfiguration::VxConfiguration(unsigned short indent)
{
}

VxConfiguration::~VxConfiguration()
{
    Clear();
    ClearDefault();
}

void VxConfiguration::Clear()
{
    if (m_Root)
        m_Root->Clear();
}

void VxConfiguration::ClearDefault()
{
    if (m_DefaultRoot)
        m_DefaultRoot->Clear();
}

int VxConfiguration::GetNumberOfSubSections() const
{
    return m_Root->GetNumberOfSubSections();
}

int VxConfiguration::GetNumberOfEntries() const
{
    return m_Root->GetNumberOfEntries();
}

int VxConfiguration::GetNumberOfSubSectionsRecursive() const
{
    return m_Root->GetNumberOfSubSectionsRecursive();
}

int VxConfiguration::GetNumberOfEntriesRecursive() const
{
    return m_Root->GetNumberOfEntriesRecursive();
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, const char *evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, int evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, long evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, unsigned int evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, unsigned long evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(char *parent, char *ename, float evalue, VxConfigurationEntry **result)
{
    if (!parent)
    {
        m_Root->AddEntry(ename, evalue, result);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

VxConfigurationSection *VxConfiguration::CreateSubSection(char *parent, char *sname)
{
    if (!parent)
        return m_Root->CreateSubSection(sname);
    VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
    if (parentSection)
        return parentSection->CreateSubSection(sname);
    return NULL;
}

XBOOL VxConfiguration::DeleteEntry(char *parent, char *ename)
{
    if (!parent)
        return m_Root->DeleteEntry(ename);
    VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
    if (parentSection)
        return parentSection->DeleteEntry(ename);
    return FALSE;
}

XBOOL VxConfiguration::DeleteSection(char *parent, char *sname)
{
    if (!parent)
        return m_Root->DeleteSection(sname);
    VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
    if (parentSection)
        return parentSection->DeleteSection(sname);
    return FALSE;
}

VxConfigurationEntry *VxConfiguration::RemoveEntry(char *parent, char *ename)
{
    if (!parent)
        return m_Root->RemoveEntry(ename);
    VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
    if (parentSection)
        return parentSection->RemoveEntry(ename);
    return NULL;
}

VxConfigurationSection *VxConfiguration::RemoveSection(char *parent, char *sname)
{
    if (!parent)
        return m_Root->RemoveSection(sname);
    VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
    if (parentSection)
        return parentSection->RemoveSection(sname);
    return NULL;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, const char *evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, int evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, long evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, unsigned int evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, unsigned long evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(char *parent, char *ename, float evalue)
{
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent)
    {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL)
    {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

VxConfigurationSection *VxConfiguration::CreateDefaultSubSection(char *parent, char *sname)
{
    if (!parent)
    {
        if (!m_DefaultRoot)
            m_DefaultRoot = new VxConfigurationSection("default", NULL);
        return m_DefaultRoot->CreateSubSection(sname);
    }

    if (m_DefaultRoot)
    {
        VxConfigurationSection *parentSection = m_DefaultRoot->GetSubSection(parent);
        if (parentSection)
            return parentSection->CreateSubSection(sname);
    }

    return NULL;
}

ConstSectionIt VxConfiguration::BeginSections() const
{
    return m_Root->BeginChildSection();
}

ConstEntryIt VxConfiguration::BeginEntries() const
{
    return m_Root->BeginChildEntry();
}

VxConfigurationSection *VxConfiguration::GetNextSection(ConstSectionIt &it) const
{
    return m_Root->GetNextChildSection(it);
}

VxConfigurationEntry *VxConfiguration::GetNextEntry(ConstEntryIt &it) const
{
    return m_Root->GetNextChildEntry(it);
}

VxConfigurationSection *VxConfiguration::GetSubSection(char *sname, XBOOL usedot) const
{
    VxConfigurationSection *section = GetSubSection(m_Root, sname, usedot);
    if (!section)
    {
        if (m_DefaultRoot)
            return GetSubSection(m_DefaultRoot, sname, usedot);
    }
    return section;
}

VxConfigurationEntry *VxConfiguration::GetEntry(char *ename, XBOOL usedot) const
{
    if (!usedot)
    {
        VxConfigurationEntry *entry = m_Root->GetEntry(ename);
        if (!entry && m_DefaultRoot)
            entry = m_DefaultRoot->GetEntry(ename);
        return entry;
    }

    XString entryName(ename);
    if (entryName.Length() == 0)
        entryName = "";
    int dotPos = entryName.RFind('.');
    if (dotPos != XString::NOTFOUND)
    {
        entryName[dotPos] = '\0';
        VxConfigurationSection *section = GetSubSection(m_Root, entryName.Str(), TRUE);
        entryName[dotPos] = '.';
        if (section)
        {
            VxConfigurationEntry *entry = section->GetEntry(&entryName[dotPos + 1]);
            if (entry) return entry;
        }

        if (m_DefaultRoot)
        {
            entryName[dotPos] = '\0';
            section = GetSubSection(m_DefaultRoot, entryName.Str(), TRUE);
            entryName[dotPos] = '.';
            if (section)
                return section->GetEntry(&entryName[dotPos + 1]);
        }
    }

    return NULL;
}

XBOOL VxConfiguration::BuildFromDataFile(const char *name, XString &error)
{
    return FALSE;
}

XBOOL VxConfiguration::BuildFromFile(const char *name, int &cline, XString &error)
{
    return 0;
}

XBOOL VxConfiguration::BuildFromMemory(const char *buffer, int &cline, XString &error)
{
    return 0;
}

XBOOL VxConfiguration::SaveToDataFile(const char *name)
{
    return FALSE;
}

XBOOL VxConfiguration::SaveToFile(const char *name)
{
    return 0;
}

VxConfigurationSection *VxConfiguration::CreateSubSection(VxConfigurationSection *root, char *sname, XBOOL usedot) const
{
    return NULL;
}

VxConfigurationSection *VxConfiguration::GetSubSection(VxConfigurationSection *root, char *sname, XBOOL usedot) const
{
    return NULL;
}

XBOOL VxConfiguration::ManageSection(char *line, VxConfigurationSection **current, XString &error)
{
    return 0;
}

XBOOL VxConfiguration::ManageEntry(char *line, VxConfigurationSection *current, XString &error)
{
    return 0;
}

VxConfigurationSection::~VxConfigurationSection()
{
    Clear();
}

void VxConfigurationSection::Clear()
{
    m_Entries.Clear();
    m_SubSections.Clear();
}

int VxConfigurationSection::GetNumberOfSubSections() const
{
    return m_SubSections.Size();
}

int VxConfigurationSection::GetNumberOfEntries() const
{
    return m_Entries.Size();
}

int VxConfigurationSection::GetNumberOfSubSectionsRecursive() const
{
    ConstSectionIt it = BeginChildSection();
    return 0;
}

int VxConfigurationSection::GetNumberOfEntriesRecursive() const
{
    return 0;
}

void VxConfigurationSection::AddEntry(char *ename, const char *evalue, VxConfigurationEntry **result)
{
}

void VxConfigurationSection::AddEntry(char *ename, int evalue, VxConfigurationEntry **result)
{
}

void VxConfigurationSection::AddEntry(char *ename, long evalue, VxConfigurationEntry **result)
{
}

void VxConfigurationSection::AddEntry(char *ename, unsigned int evalue, VxConfigurationEntry **result)
{
}

void VxConfigurationSection::AddEntry(char *ename, unsigned long evalue, VxConfigurationEntry **result)
{
}

void VxConfigurationSection::AddEntry(char *ename, float evalue, VxConfigurationEntry **result)
{
}

VxConfigurationSection *VxConfigurationSection::CreateSubSection(char *sname)
{
    return NULL;
}

XBOOL VxConfigurationSection::DeleteEntry(char *ename)
{
    return 0;
}

XBOOL VxConfigurationSection::DeleteSection(char *sname)
{
    return 0;
}

VxConfigurationEntry *VxConfigurationSection::RemoveEntry(char *ename)
{
    return NULL;
}

VxConfigurationSection *VxConfigurationSection::RemoveSection(char *sname)
{
    return NULL;
}

ConstEntryIt VxConfigurationSection::BeginChildEntry() const
{
    return m_Entries.Begin();
}

VxConfigurationEntry *VxConfigurationSection::GetNextChildEntry(ConstEntryIt &it) const
{
    return NULL;
}

ConstSectionIt VxConfigurationSection::BeginChildSection() const
{
    return m_SubSections.Begin();
}

VxConfigurationSection *VxConfigurationSection::GetNextChildSection(ConstSectionIt &it) const
{
    return NULL;
}

VxConfigurationEntry *VxConfigurationSection::GetEntry(char *ename) const
{
    return *m_Entries.Find(ename);
}

VxConfigurationSection *VxConfigurationSection::GetSubSection(char *sname) const
{
    return *m_SubSections.Find(sname);
}

const char *VxConfigurationSection::GetName() const
{
    return m_Name.CStr();
}

VxConfigurationSection *VxConfigurationSection::GetParent() const
{
    return m_Parent;
}

VxConfigurationSection::VxConfigurationSection(char *name, VxConfigurationSection *parent) : m_Parent(parent), m_Name(name) {}

VxConfigurationEntry::~VxConfigurationEntry() {}

void VxConfigurationEntry::SetValue(const char *value)
{
    m_Value = value;
}

void VxConfigurationEntry::SetValue(int value)
{
    char buffer[32];
    sprintf(buffer, "%d", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(long value)
{
    char buffer[32];
    sprintf(buffer, "%ld", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(unsigned int value)
{
    char buffer[32];
    sprintf(buffer, "%u", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(unsigned long value)
{
    char buffer[32];
    sprintf(buffer, "%lu", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(float value)
{
    char buffer[32];
    sprintf(buffer, "%f", value);
    m_Value = buffer;
}

const char *VxConfigurationEntry::GetName() const
{
    return m_Name.CStr();
}

VxConfigurationSection *VxConfigurationEntry::GetParent() const
{
    return m_Parent;
}

const char *VxConfigurationEntry::GetValue() const
{
    return m_Value.CStr();
}

XBOOL VxConfigurationEntry::GetValueAsInteger(int &value) const
{
    return sscanf(m_Value.CStr(), "%d", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsLong(long &value) const
{
    return sscanf(m_Value.CStr(), "%ld", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsUnsignedInteger(unsigned int &value) const
{
    return sscanf(m_Value.CStr(), "%u", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsUnsignedLong(unsigned long &value) const
{
    return sscanf(m_Value.CStr(), "%lu", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsFloat(float &value) const
{
    return sscanf(m_Value.CStr(), "%f", &value) == 1;
}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, const char *value) : m_Name(name), m_Parent(parent)
{
    SetValue(value);
}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, int value) : m_Name(name), m_Parent(parent)
{
    SetValue(value);
}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, unsigned int value) : m_Name(name), m_Parent(parent)
{
    SetValue(value);
}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, float value) : m_Name(name), m_Parent(parent)
{
    SetValue(value);
}

VxConfig::VxConfig()
{
    ::RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Virtools\\UserConfig", 0, NULL, 0, KEY_WRITE, NULL, (PHKEY)&m_VirtoolsSection, NULL);
}

VxConfig::~VxConfig()
{
    ::RegCloseKey(*(PHKEY)&m_VirtoolsSection);
}

void VxConfig::OpenSection(char *iSection, VxConfig::Mode iOpeningMode)
{
    ::RegCreateKeyExA(*(PHKEY)&m_VirtoolsSection, iSection, 0, NULL, 0, iOpeningMode, NULL, (PHKEY)&m_CurrentSection, NULL);
}

void VxConfig::CloseSection(char *iSection)
{
    ::RegCloseKey(*(PHKEY)&m_CurrentSection);
}

void VxConfig::WriteStringEntry(const char *iKey, const char *iValue)
{
    ::RegSetValueExA(*(PHKEY)&m_CurrentSection, iKey, 0, REG_SZ, (LPBYTE)iValue, strlen(iValue) + 1);
}

int VxConfig::ReadStringEntry(char *iKey, char *oData)
{
    DWORD cbData = 256;
    if (::RegQueryValueExA(*(PHKEY)&m_CurrentSection, iKey, NULL, NULL, (LPBYTE)oData, &cbData) == ERROR_SUCCESS)
        return (int)cbData;
    return -1;
}
