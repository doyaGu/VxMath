#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include "VxPathEncoding.h"
#include "XP.h"
#include <SDL3/SDL_stdinc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

VxConfiguration::VxConfiguration(unsigned short indent) : m_Root(NULL), m_DefaultRoot(NULL), m_Indent(indent) {
    m_Root = new VxConfigurationSection("root", NULL);
}

VxConfiguration::~VxConfiguration() {
    delete m_Root;
    delete m_DefaultRoot;
}

void VxConfiguration::Clear() { if (m_Root) m_Root->Clear(); }

void VxConfiguration::ClearDefault() {
    if (m_DefaultRoot) m_DefaultRoot->Clear();
}

size_t VxConfiguration::GetNumberOfSubSections() const {
    return m_Root ? m_Root->GetNumberOfSubSections() : 0;
}

size_t VxConfiguration::GetNumberOfEntries() const {
    return m_Root ? m_Root->GetNumberOfEntries() : 0;
}

size_t VxConfiguration::GetNumberOfSubSectionsRecursive() const {
    return m_Root ? m_Root->GetNumberOfSubSectionsRecursive() : 0;
}

size_t VxConfiguration::GetNumberOfEntriesRecursive() const {
    return m_Root ? m_Root->GetNumberOfEntriesRecursive() : 0;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, const char *evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, int evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, long evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, unsigned int evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, unsigned long evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *section = GetSubSection(parent, TRUE);
    if (section != NULL || (section = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        section->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

XBOOL VxConfiguration::AddEntry(const char *parent, const char *ename, float evalue, VxConfigurationEntry **result) {
    if (!ename) return FALSE;
    
    if (!parent) {
        if (m_Root) {
            m_Root->AddEntry(ename, evalue, result);
            return TRUE;
        }
        return FALSE;
    }

    VxConfigurationSection *parentSection = GetSubSection(parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_Root, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, result);
        return TRUE;
    }
    return FALSE;
}

VxConfigurationSection *VxConfiguration::CreateSubSection(const char *parent, const char *sname) {
    if (!sname) return NULL;
    
    if (!parent && m_Root)
        return m_Root->CreateSubSection(sname);

    if (m_Root) {
        VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
        if (parentSection)
            return parentSection->CreateSubSection(sname);
    }
    return NULL;
}

XBOOL VxConfiguration::DeleteEntry(const char *parent, const char *ename) {
    if (!parent && m_Root)
        return m_Root->DeleteEntry(ename);

    if (m_Root) {
        VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
        if (parentSection)
            return parentSection->DeleteEntry(ename);
    }
    return FALSE;
}

XBOOL VxConfiguration::DeleteSection(const char *parent, const char *sname) {
    if (!parent && m_Root)
        return m_Root->DeleteSection(sname);

    if (m_Root) {
        VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
        if (parentSection)
            return parentSection->DeleteSection(sname);
    }
    return FALSE;
}

VxConfigurationEntry *VxConfiguration::RemoveEntry(const char *parent, const char *ename) {
    if (!parent && m_Root)
        return m_Root->RemoveEntry(ename);

    if (m_Root) {
        VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
        if (parentSection)
            return parentSection->RemoveEntry(ename);
    }
    return NULL;
}

VxConfigurationSection *VxConfiguration::RemoveSection(const char *parent, const char *sname) {
    if (!parent && m_Root)
        return m_Root->RemoveSection(sname);

    if (m_Root) {
        VxConfigurationSection *parentSection = m_Root->GetSubSection(parent);
        if (parentSection)
            return parentSection->RemoveSection(sname);
    }
    return NULL;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, const char *evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, int evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, long evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, unsigned int evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, unsigned long evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

XBOOL VxConfiguration::AddDefaultEntry(const char *parent, const char *ename, float evalue) {
    if (!m_DefaultRoot)
        m_DefaultRoot = new VxConfigurationSection("default", NULL);

    if (!parent) {
        m_DefaultRoot->AddEntry(ename, evalue);
        return TRUE;
    }

    VxConfigurationSection *parentSection = GetSubSection(m_DefaultRoot, parent, TRUE);
    if (parentSection || (parentSection = CreateSubSection(m_DefaultRoot, parent, TRUE)) != NULL) {
        parentSection->AddEntry(ename, evalue, NULL);
        return TRUE;
    }

    return FALSE;
}

VxConfigurationSection *VxConfiguration::CreateDefaultSubSection(const char *parent, const char *sname) {
    if (!parent) {
        if (!m_DefaultRoot)
            m_DefaultRoot = new VxConfigurationSection("default", NULL);
        return m_DefaultRoot->CreateSubSection(sname);
    }

    if (m_DefaultRoot) {
        VxConfigurationSection *parentSection = m_DefaultRoot->GetSubSection(parent);
        if (parentSection)
            return parentSection->CreateSubSection(sname);
    }

    return NULL;
}

ConstSectionIt VxConfiguration::BeginSections() const {
    return m_Root ? m_Root->BeginChildSection() : ConstSectionIt();
}

ConstEntryIt VxConfiguration::BeginEntries() const {
    return m_Root ? m_Root->BeginChildEntry() : ConstEntryIt();
}

VxConfigurationSection *VxConfiguration::GetNextSection(ConstSectionIt &it) const {
    return m_Root ? m_Root->GetNextChildSection(it) : NULL;
}

VxConfigurationEntry *VxConfiguration::GetNextEntry(ConstEntryIt &it) const {
    return m_Root ? m_Root->GetNextChildEntry(it) : NULL;
}

VxConfigurationSection *VxConfiguration::GetSubSection(const char *sname, XBOOL usedot) const {
    if (!sname) return NULL;
    
    VxConfigurationSection *section = NULL;

    if (m_Root)
        section = GetSubSection(m_Root, sname, usedot);

    if (!section && m_DefaultRoot)
        return GetSubSection(m_DefaultRoot, sname, usedot);

    return section;
}

VxConfigurationEntry *VxConfiguration::GetEntry(const char *ename, XBOOL usedot) const {
    if (!ename) return NULL;
    
    if (!usedot || !strchr(ename, '.')) {
        VxConfigurationEntry *entry = m_Root ? m_Root->GetEntry(ename) : NULL;
        if (!entry && m_DefaultRoot)
            entry = m_DefaultRoot->GetEntry(ename);
        return entry;
    }

    XString entryName(ename);
    if (entryName.Length() == 0)
        entryName = "";
    int dotPos = entryName.RFind('.');
    if (dotPos != XString::NOTFOUND) {
        entryName[dotPos] = '\0';
        VxConfigurationSection *section = m_Root ? GetSubSection(m_Root, entryName.Str(), TRUE) : NULL;
        entryName[dotPos] = '.';
        if (section) {
            VxConfigurationEntry *entry = section->GetEntry(&entryName[dotPos + 1]);
            if (entry) return entry;
        }

        if (m_DefaultRoot) {
            entryName[dotPos] = '\0';
            section = GetSubSection(m_DefaultRoot, entryName.Str(), TRUE);
            entryName[dotPos] = '.';
            if (section)
                return section->GetEntry(&entryName[dotPos + 1]);
        }
    }

    return NULL;
}

XBOOL VxConfiguration::BuildFromDataFile(const char *name, XString &error) {
    return FALSE;
}

XBOOL VxConfiguration::BuildFromFile(const char *name, int &cline, XString &error) {
    if (!name) {
        error = "Null filename";
        return FALSE;
    }
    FILE *file = fopen(name, "rb");
    if (!file) {
        error = "Could not open file: ";
        error += name;
        return FALSE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        error = "Could not seek configuration file";
        return FALSE;
    }
    long fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        error = "Could not read configuration file size";
        return FALSE;
    }
    XAP<char> buffer;
    try {
        buffer.Set(new char[static_cast<size_t>(fileSize) + 1]);
    } catch (...) {
        fclose(file);
        throw;
    }
    size_t read = fread(buffer, 1, static_cast<size_t>(fileSize), file);
    bool failed = ferror(file) != 0;
    fclose(file);
    if (failed || read != static_cast<size_t>(fileSize)) {
        error = "Failed to read entire file";
        return FALSE;
    }

    buffer[read] = '\0';

    return BuildFromMemory(buffer, cline, error);
}

XBOOL VxConfiguration::BuildFromMemory(const char *buffer, int &cline, XString &error) {
    cline = 0;
    if (!buffer || !m_Root) {
        error = "Null configuration buffer";
        return FALSE;
    }
    XAP<char> bufferCopy(new char[strlen(buffer) + 1]);
    strcpy(bufferCopy, buffer);
    Clear();
    VxConfigurationSection *currentSection = m_Root;
    char *next = bufferCopy;
    while (*next) {
        char *line = next;
        next += strcspn(next, "\r\n");
        if (*next) {
            char separator = *next;
            *next++ = '\0';
            if (separator == '\r' && *next == '\n') ++next;
        }
        char *trimmed = Shrink(line);
        if (*trimmed && *trimmed != '#') {
            XBOOL result = *trimmed == '<'
                ? ManageSection(trimmed + 1, &currentSection, error)
                : ManageEntry(trimmed, currentSection, error);
            if (!result) return FALSE;
        }
        ++cline;
    }

    if (currentSection != m_Root) {
        error = "Missing closing tag for section: ";
        error += currentSection->GetName();
        return FALSE;
    }
    error = "OK";
    return TRUE;
}

XBOOL VxConfiguration::SaveToDataFile(const char *name) {
    return FALSE;
}

XBOOL VxConfiguration::SaveToFile(const char *name) {
    if (!m_Root || !name) return FALSE;
    FILE *file = fopen(name, "wt");
    if (!file) return FALSE;
    struct WriteSection {
        static bool DoWrite(FILE *f, VxConfigurationSection *section, int level, unsigned short indentSize) {
            int padding = level * indentSize;
            if (fprintf(f, "%*s<%s>\n", padding, "", section->GetName()) < 0) return false;
            ConstEntryIt entIt = section->BeginChildEntry();
            VxConfigurationEntry *entry;
            while ((entry = section->GetNextChildEntry(entIt)) != NULL) {
                if (fprintf(f, "%*s%s = %s\n", padding + indentSize, "", entry->GetName(), entry->GetValue()) < 0)
                    return false;
            }
            ConstSectionIt secIt = section->BeginChildSection();
            VxConfigurationSection *subSection;
            while ((subSection = section->GetNextChildSection(secIt)) != NULL) {
                if (!DoWrite(f, subSection, level + 1, indentSize)) return false;
            }
            return fprintf(f, "%*s</%s>\n", padding, "", section->GetName()) >= 0;
        }
    };
    bool success = true;
    ConstEntryIt entIt = m_Root->BeginChildEntry();
    VxConfigurationEntry *entry;
    while (success && (entry = m_Root->GetNextChildEntry(entIt)) != NULL)
        success = fprintf(file, "%s = %s\n", entry->GetName(), entry->GetValue()) >= 0;
    ConstSectionIt secIt = m_Root->BeginChildSection();
    VxConfigurationSection *section;
    while (success && (section = m_Root->GetNextChildSection(secIt)) != NULL)
        success = WriteSection::DoWrite(file, section, 0, m_Indent);
    if (fclose(file) != 0) success = false;
    return success ? TRUE : FALSE;
}

VxConfigurationSection *VxConfiguration::CreateSubSection(VxConfigurationSection *root, const char *sname, XBOOL usedot) const {
    if (!root || !sname) return NULL;

    if (!usedot) return root->CreateSubSection(sname);

    // Handle dot notation for hierarchical sections
    XString sectionPath(sname);
    int dotPos = sectionPath.Find('.');

    if (dotPos == XString::NOTFOUND) return root->CreateSubSection(sname);

    // Extract the first section name
    sectionPath[dotPos] = '\0';
    char *firstSection = sectionPath.Str();

    // Get or create the first section
    VxConfigurationSection *section = root->GetSubSection(firstSection);
    if (!section) section = root->CreateSubSection(firstSection);

    if (!section) return NULL;

    // Process remaining path recursively
    return CreateSubSection(section, &sectionPath[dotPos + 1], TRUE);
}

VxConfigurationSection *VxConfiguration::GetSubSection(VxConfigurationSection *root, const char *sname, XBOOL usedot) const {
    if (!root || !sname) return NULL;

    if (!usedot) return root->GetSubSection(sname);

    // Handle dot notation for hierarchical sections
    XString sectionPath(sname);
    int dotPos = sectionPath.Find('.');
    if (dotPos == XString::NOTFOUND) return root->GetSubSection(sname);

    // Extract the first section name
    sectionPath[dotPos] = '\0';
    const char *firstSection = sectionPath.CStr();

    // Get the first section
    VxConfigurationSection *section = root->GetSubSection(firstSection);
    if (!section) return NULL;

    // Process remaining path recursively
    return GetSubSection(section, &sectionPath[dotPos + 1], TRUE);
}

XBOOL VxConfiguration::ManageSection(const char *line, VxConfigurationSection **current, XString &error) {
    if (!line || !current || !m_Root) return FALSE;
    XString tag(line);
    int end = tag.RFind('>');
    if (end == XString::NOTFOUND) {
        error = "can't find end of section tag";
        return FALSE;
    }
    tag[end] = '\0';
    const char *name = tag.CStr();
    if (*name == '/') {
        ++name;
        if (!*current || *current == m_Root) {
            error = "Unexpected closing tag: ";
            error += name;
            return FALSE;
        }
        if (strcmp(name, (*current)->GetName()) != 0) {
            error = "Mismatched closing tag: ";
            error += name;
            error += " (expected ";
            error += (*current)->GetName();
            error += ")";
            return FALSE;
        }
        *current = (*current)->GetParent();
        return TRUE;
    }
    VxConfigurationSection *section = (*current)->CreateSubSection(name);
    if (!section) {
        error = "can't create subsection: ";
        error += name;
        return FALSE;
    }
    *current = section;
    return TRUE;
}

XBOOL VxConfiguration::ManageEntry(const char *line, VxConfigurationSection *current, XString &error) {
    if (!line || !current) return FALSE;
    const char *separator = strchr(line, '=');
    if (!separator) {
        error = "can't find equal ('=') separator";
        return FALSE;
    }
    XString key;
    if (separator != line) key = XString(line, static_cast<int>(separator - line));
    const char *text = separator + 1;
    text += strspn(text, " \t\n");
    XString value(text);
    current->AddEntry(key.Empty() ? "" : Shrink(key.Str()), value.Empty() ? "" : Shrink(value.Str()));
    return TRUE;
}

// VxConfigurationSection implementation

VxConfigurationSection::~VxConfigurationSection() { Clear(); }

void VxConfigurationSection::Clear() {
    for (EntryIt it = m_Entries.Begin(); it != m_Entries.End(); ++it)
        delete *it;
    m_Entries.Clear();
    for (SectionIt it = m_SubSections.Begin(); it != m_SubSections.End(); ++it)
        delete *it;
    m_SubSections.Clear();
}

size_t VxConfigurationSection::GetNumberOfSubSections() const { return m_SubSections.Size(); }

size_t VxConfigurationSection::GetNumberOfEntries() const { return m_Entries.Size();}

size_t VxConfigurationSection::GetNumberOfSubSectionsRecursive() const {
    size_t count = m_SubSections.Size();

    ConstSectionIt it = BeginChildSection();
    VxConfigurationSection *section;
    while ((section = GetNextChildSection(it)) != NULL) {
        count += section->GetNumberOfSubSectionsRecursive();
    }

    return count;
}

size_t VxConfigurationSection::GetNumberOfEntriesRecursive() const {
    size_t count = m_Entries.Size();

    ConstSectionIt it = BeginChildSection();
    VxConfigurationSection *section;
    while ((section = GetNextChildSection(it)) != NULL) {
        count += section->GetNumberOfEntriesRecursive();
    }

    return count;
}

void VxConfigurationSection::AddEntry(const char *ename, const char *evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue ? evalue : "");
    m_Entries.Insert(ename, entry);
}

void VxConfigurationSection::AddEntry(const char *ename, int evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue);
    m_Entries.Insert(ename, entry);
}

void VxConfigurationSection::AddEntry(const char *ename, long evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue);
    m_Entries.Insert(ename, entry);
}

void VxConfigurationSection::AddEntry(const char *ename, unsigned int evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue);
    m_Entries.Insert(ename, entry);
}

void VxConfigurationSection::AddEntry(const char *ename, unsigned long evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue);
    m_Entries.Insert(ename, entry);
}

void VxConfigurationSection::AddEntry(const char *ename, float evalue, VxConfigurationEntry **result) {
    if (!ename) return;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (result) *result = entry;
    if (entry) return;

    entry = new VxConfigurationEntry(this, ename, evalue);
    m_Entries.Insert(ename, entry);
}

VxConfigurationSection *VxConfigurationSection::CreateSubSection(const char *sname) {
    if (!sname) return NULL;

    VxConfigurationSection *section = GetSubSection(sname);
    if (section) return NULL;

    section = new VxConfigurationSection(sname, this);
    m_SubSections.Insert(sname, section);

    return section;
}

XBOOL VxConfigurationSection::DeleteEntry(const char *ename) {
    VxConfigurationEntry *entry = RemoveEntry(ename);
    if (!entry) return FALSE;
    delete entry;
    return TRUE;
}

XBOOL VxConfigurationSection::DeleteSection(const char *sname) {
    VxConfigurationSection *section = RemoveSection(sname);
    if (!section) return FALSE;
    delete section;
    return TRUE;
}

VxConfigurationEntry *VxConfigurationSection::RemoveEntry(const char *ename) {
    if (!ename) return NULL;

    VxConfigurationEntry *entry = GetEntry(ename);
    if (!entry) return NULL;

    m_Entries.Remove(ename);
    return entry;
}

VxConfigurationSection *VxConfigurationSection::RemoveSection(const char *sname) {
    if (!sname) return NULL;

    VxConfigurationSection *section = GetSubSection(sname);
    if (!section) return NULL;

    m_SubSections.Remove(sname);
    return section;
}

ConstEntryIt VxConfigurationSection::BeginChildEntry() const { return m_Entries.Begin(); }

VxConfigurationEntry *VxConfigurationSection::GetNextChildEntry(ConstEntryIt &it) const {
    if (it == m_Entries.End()) return NULL;

    VxConfigurationEntry *entry = *it;
    ++it;
    return entry;
}

ConstSectionIt VxConfigurationSection::BeginChildSection() const { return m_SubSections.Begin(); }

VxConfigurationSection *VxConfigurationSection::GetNextChildSection(ConstSectionIt &it) const {
    if (it == m_SubSections.End()) return NULL;

    VxConfigurationSection *section = *it;
    ++it;
    return section;
}

VxConfigurationEntry *VxConfigurationSection::GetEntry(const char *ename) const {
    if (!ename) return NULL;

    ConstEntryIt it = m_Entries.Find(ename);
    if (it == m_Entries.End()) return NULL;
    return *it;
}

VxConfigurationSection *VxConfigurationSection::GetSubSection(const char *sname) const {
    if (!sname) return NULL;

    ConstSectionIt it = m_SubSections.Find(sname);
    if (it == m_SubSections.End()) return NULL;
    return *it;
}

const char *VxConfigurationSection::GetName() const { return m_Name.CStr(); }

VxConfigurationSection *VxConfigurationSection::GetParent() const { return m_Parent; }

VxConfigurationSection::VxConfigurationSection(const char *name, VxConfigurationSection *parent)
    : m_Parent(parent), m_Name(name ? name : "") {}

// VxConfigurationEntry implementation
VxConfigurationEntry::~VxConfigurationEntry() {}

void VxConfigurationEntry::SetValue(const char *value) { m_Value = value ? value : ""; }

void VxConfigurationEntry::SetValue(int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(long value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%ld", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(unsigned int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(unsigned long value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    m_Value = buffer;
}

void VxConfigurationEntry::SetValue(float value) {
    char buffer[64]; // Sign, 39 integer digits, decimal point and six fractional digits.
    snprintf(buffer, sizeof(buffer), "%f", value);
    m_Value = buffer;
}

const char *VxConfigurationEntry::GetName() const { return m_Name.CStr(); }

VxConfigurationSection *VxConfigurationEntry::GetParent() const { return m_Parent; }

const char *VxConfigurationEntry::GetValue() const { return m_Value.CStr(); }

XBOOL VxConfigurationEntry::GetValueAsInteger(int &value) const {
    return sscanf(m_Value.CStr(), "%d", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsLong(long &value) const {
    return sscanf(m_Value.CStr(), "%ld", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsUnsignedInteger(unsigned int &value) const {
    return sscanf(m_Value.CStr(), "%u", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsUnsignedLong(unsigned long &value) const {
    return sscanf(m_Value.CStr(), "%lu", &value) == 1;
}

XBOOL VxConfigurationEntry::GetValueAsFloat(float &value) const {
    return sscanf(m_Value.CStr(), "%f", &value) == 1;
}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, const char *value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value); }

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, int value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value); }

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, long value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value); }

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, unsigned int value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value); }

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, unsigned long value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value);}

VxConfigurationEntry::VxConfigurationEntry(VxConfigurationSection *parent, const char *name, float value)
    : m_Name(name ? name : ""), m_Parent(parent) { SetValue(value); }

char *Shrink(char *str) {
    if (!str) return NULL;

    // Skip leading whitespace
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n')) str++;

    // Empty string
    if (!*str) return str;

    // Find end of string
    char *end = str + (int)strlen(str) - 1;

    // Remove trailing whitespace
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    return str;
}

// VxConfig implementation
static const char *VxDefaultRegistryBase() {
    return "Software\\Virtools\\UserConfig";
}

static const char *VxRegistryRootName(VxConfigRoot root) {
    return root == VXCONFIG_ROOT_LOCAL_MACHINE ? "LocalMachine" : "CurrentUser";
}

static void VxAppendRegistryName(XString &result, const char *begin, const char *end) {
    const char *digits = "0123456789ABCDEF";
    for (const char *p = begin; p != end;) {
        const char *next = VxNextPathCharacter(p);
        unsigned char c = static_cast<unsigned char>(*p);
        if (next == p + 1 && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '_' || c == '-' || c >= 128)) {
            result << '%' << digits[c >> 4] << digits[c & 15];
            p = next;
        } else {
            while (p != next) result << *p++;
        }
    }
}

static XString VxNormalizeRegistryPath(VxConfigRoot root, const char *base, const char *section) {
    XString path(VxRegistryRootName(root));
    const char *parts[2] = {base, section};
    for (int p = 0; p < 2; ++p) {
        const char *part = parts[p];
        while (part && *part) {
            const char *end = part;
            while (*end && *end != '\\') end = VxNextPathCharacter(end);
            if (end != part) {
                path << '.';
                VxAppendRegistryName(path, part, end);
            }
            part = *end ? end + 1 : end;
        }
    }
    return path;
}

static XString VxNormalizeRegistryValueName(const char *key) {
    XString value;
    if (key) VxAppendRegistryName(value, key, key + strlen(key));
    return value;
}

static XBOOL VxRegistryNamesEqual(const char *left, const char *right) {
    if (strcmp(left, right) == 0) return TRUE;
#ifdef _WIN32
    XArray<wchar_t> a, b;
    return VxPathToWide(left, GetACP(), a) && VxPathToWide(right, GetACP(), b) &&
        CompareStringOrdinal(a.Begin(), -1, b.Begin(), -1, TRUE) == CSTR_EQUAL;
#else
    return SDL_strcasecmp(left, right) == 0;
#endif
}

static VxConfigurationSection *VxGetRegistrySection(VxConfiguration &config, const char *path, XBOOL create) {
    VxConfigurationSection *parent = NULL;
    while (*path) {
        const char *end = strchr(path, '.');
        if (!end) end = path + strlen(path);
        XString name(path, static_cast<int>(end - path));
        VxConfigurationSection *child = parent ? parent->GetSubSection(name.CStr()) : config.GetSubSection(name.CStr(), FALSE);
        if (!child) {
            ConstSectionIt it = parent ? parent->BeginChildSection() : config.BeginSections();
            VxConfigurationSection *candidate;
            while ((candidate = parent ? parent->GetNextChildSection(it) : config.GetNextSection(it)) != NULL) {
                if (VxRegistryNamesEqual(candidate->GetName(), name.CStr())) { child = candidate; break; }
            }
        }
        if (!child && create)
            child = parent ? parent->CreateSubSection(name.CStr()) : config.CreateSubSection(NULL, name.CStr());
        if (!child) return NULL;
        parent = child;
        path = *end ? end + 1 : end;
    }
    return parent;
}

static VxConfigurationEntry *VxGetRegistryEntry(VxConfigurationSection *section, const char *name) {
    VxConfigurationEntry *entry = section->GetEntry(name);
    if (entry) return entry;
    ConstEntryIt it = section->BeginChildEntry();
    while ((entry = section->GetNextChildEntry(it)) != NULL)
        if (VxRegistryNamesEqual(entry->GetName(), name)) return entry;
    return NULL;
}

// NULL removes private metadata; an empty string remains a stored value.
static VxConfigurationEntry *VxUpdateRegistryEntry(VxConfigurationSection *section, const char *name, const char *value) {
    VxConfigurationEntry *entry = VxGetRegistryEntry(section, name);
    if (!value) {
        if (entry) section->DeleteEntry(entry->GetName());
        return NULL;
    }
    if (entry) entry->SetValue(value);
    else {
        section->AddEntry(name, value);
        entry = section->GetEntry(name);
    }
    return entry;
}

static XString VxEncodeRegistryValue(const char *value, size_t length) {
    const char *digits = "0123456789ABCDEF";
    XString result;
    result.Reserve(static_cast<XWORD>(length * 2));
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        result << digits[c >> 4] << digits[c & 15];
    }
    return result;
}

static int VxRegistryHexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static XBOOL VxDecodeRegistryValue(const char *text, XString &result) {
    size_t length = strlen(text);
    if (length % 2 || length / 2 > XString::MAX_LENGTH - result.Length()) return FALSE;
    result.Reserve(static_cast<XWORD>(result.Length() + length / 2));
    for (size_t i = 0; i < length; i += 2) {
        int high = VxRegistryHexDigit(text[i]), low = VxRegistryHexDigit(text[i + 1]);
        if (high < 0 || low < 0 || (high == 0 && low == 0)) return FALSE;
        result << static_cast<char>((high << 4) | low);
    }
    return TRUE;
}

static XBOOL VxLoadRegistryFile(VxConfiguration &config, const char *file) {
    XString error;
    int line = 0;
    return config.BuildFromFile(file, line, error) || !VxFileExists(file);
}

static const char *VxConfigTypeName(VxConfigValueType type) {
    switch (type) {
    case VXCONFIG_VALUE_INTEGER: return "integer";
    case VXCONFIG_VALUE_FLOAT: return "float";
    case VXCONFIG_VALUE_BOOLEAN: return "boolean";
    case VXCONFIG_VALUE_STRING:
    default: return "string";
    }
}

static VxConfigValueType VxConfigTypeFromName(const char *name) {
    if (!name)
        return VXCONFIG_VALUE_STRING;
    if (strcmp(name, "integer") == 0)
        return VXCONFIG_VALUE_INTEGER;
    if (strcmp(name, "float") == 0)
        return VXCONFIG_VALUE_FLOAT;
    if (strcmp(name, "boolean") == 0)
        return VXCONFIG_VALUE_BOOLEAN;
    return VXCONFIG_VALUE_STRING;
}

static XString VxJoinConfigPath(const char *dir, const char *file) {
    XString result = dir ? dir : "";
    if (result.Length() > 0 && result[result.Length() - 1] != '\\' && result[result.Length() - 1] != '/') {
#ifdef _WIN32
        result << '\\';
#else
        result << '/';
#endif
    }
    result << (file ? file : "");
    return result;
}

static XBOOL VxGetRegistryFilePathString(XString &path) {
    path = "";
    XString dir;
    if (!VxGetUserConfigPath("Ballance", dir))
        return FALSE;
    path = VxJoinConfigPath(dir.CStr(), "registry.ini");
    return TRUE;
}

VxConfig::VxConfig()
    : m_VirtoolsSection(NULL),
      m_CurrentSection(NULL),
      m_Root(VXCONFIG_ROOT_CURRENT_USER),
      m_BaseSection(VxDefaultRegistryBase()),
      m_CurrentSectionName(""),
      m_CurrentMode(READ) {
    OpenRoot(VXCONFIG_ROOT_CURRENT_USER, VxDefaultRegistryBase());
}

VxConfig::VxConfig(VxConfigRoot root, const char *baseSection)
    : m_VirtoolsSection(NULL),
      m_CurrentSection(NULL),
      m_Root(root),
      m_BaseSection(baseSection ? baseSection : VxDefaultRegistryBase()),
      m_CurrentSectionName(""),
      m_CurrentMode(READ) {
    OpenRoot(root, baseSection ? baseSection : VxDefaultRegistryBase());
}

VxConfig::~VxConfig() {
    m_CurrentSection = NULL;
    m_VirtoolsSection = NULL;
}

XBOOL VxConfig::OpenRoot(VxConfigRoot root, const char *baseSection) {
    m_Root = root;
    m_BaseSection = baseSection ? baseSection : VxDefaultRegistryBase();
    m_CurrentSectionName = "";
    m_CurrentSection = NULL;
    m_VirtoolsSection = NULL;
    m_CurrentMode = static_cast<Mode>(0);
    return TRUE;
}

void VxConfig::OpenSection(const char *iSection, VxConfig::Mode iOpeningMode) {
    CloseSection(NULL);
    if (!iSection || *iSection == '\\') return;
    XString file;
    if (!VxGetRegistryFilePathString(file)) return;
    VxConfiguration config;
    if (!VxLoadRegistryFile(config, file.CStr())) return;
    XString path = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), iSection);
    if (!VxGetRegistrySection(config, path.CStr(), FALSE)) {
        if (!VxGetRegistrySection(config, path.CStr(), TRUE) || !config.SaveToFile(file.CStr())) return;
    }
    m_CurrentSectionName = iSection;
    m_CurrentMode = iOpeningMode;
}

void VxConfig::CloseSection(const char *iSection) {
    (void)iSection;
    m_CurrentSection = NULL;
    m_CurrentSectionName = "";
    m_CurrentMode = static_cast<Mode>(0);
}

void VxConfig::WriteStringEntry(const char *iKey, const char *iValue) {
    WriteRawEntry(iKey ? iKey : "", iValue ? iValue : "", VXCONFIG_VALUE_STRING);
}

void VxConfig::WriteIntegerEntry(const char *iKey, int iValue) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d", iValue);
    WriteRawEntry(iKey, buffer, VXCONFIG_VALUE_INTEGER);
}

void VxConfig::WriteFloatEntry(const char *iKey, float iValue) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.9g", iValue);
    WriteRawEntry(iKey, buffer, VXCONFIG_VALUE_FLOAT);
}

void VxConfig::WriteBooleanEntry(const char *iKey, XBOOL iValue) {
    WriteRawEntry(iKey, iValue ? "1" : "0", VXCONFIG_VALUE_BOOLEAN);
}

XDWORD VxConfig::ReadStringEntry(const char *iKey, char *oData) {
    size_t length = 0;
    if (!ReadRawEntry(iKey ? iKey : "", oData, 256, NULL, &length))
        return static_cast<XDWORD>(-1);
    return static_cast<XDWORD>(length);
}

XBOOL VxConfig::ReadStringEntry(const char *iKey, char *oData, size_t iSize) {
    return ReadRawEntry(iKey, oData, iSize, NULL);
}

XBOOL VxConfig::ReadIntegerEntry(const char *iKey, int *oData) {
    XString text;
    if (!oData || !ReadEntryValue(iKey, text, NULL))
        return FALSE;
    char *end;
    errno = 0;
    long value = strtol(text.CStr(), &end, 10);
    if (end == text.CStr() || errno == ERANGE || value < INT_MIN || value > INT_MAX)
        return FALSE;
    *oData = static_cast<int>(value);
    return TRUE;
}

XBOOL VxConfig::ReadFloatEntry(const char *iKey, float *oData) {
    XString text;
    if (!oData || !ReadEntryValue(iKey, text, NULL))
        return FALSE;
    return sscanf(text.CStr(), "%f", oData) == 1;
}

XBOOL VxConfig::ReadBooleanEntry(const char *iKey, XBOOL *oData) {
    int value = 0;
    if (!oData || !ReadIntegerEntry(iKey, &value))
        return FALSE;
    *oData = value ? TRUE : FALSE;
    return TRUE;
}

XBOOL VxConfig::DeleteEntry(const char *iKey) {
    if (!(m_CurrentMode & WRITE))
        return FALSE;
    XString file;
    if (!VxGetRegistryFilePathString(file))
        return FALSE;
    VxConfiguration config;
    if (!VxLoadRegistryFile(config, file.CStr())) return FALSE;
    XString section = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), m_CurrentSectionName.CStr());
    XString key = VxNormalizeRegistryValueName(iKey);
    VxConfigurationSection *sectionObject = VxGetRegistrySection(config, section.CStr(), FALSE);
    if (!sectionObject)
        return FALSE;
    VxConfigurationEntry *entry = VxGetRegistryEntry(sectionObject, key.CStr());
    if (!entry) return FALSE;
    XBOOL removed = sectionObject->DeleteEntry(entry->GetName());
    VxUpdateRegistryEntry(sectionObject, (key + ".type").CStr(), NULL);
    VxUpdateRegistryEntry(sectionObject, (key + ".tail").CStr(), NULL);
    if (removed)
        return config.SaveToFile(file.CStr());
    return FALSE;
}

XBOOL VxConfig::DeleteSection(const char *iSection) {
    if (!iSection && !(m_CurrentMode & WRITE)) return FALSE;
    XString sectionName = iSection ? iSection : m_CurrentSectionName.CStr();
    XString file;
    if (!VxGetRegistryFilePathString(file))
        return FALSE;
    VxConfiguration config;
    if (!VxLoadRegistryFile(config, file.CStr())) return FALSE;
    XString section = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), sectionName.CStr());
    VxConfigurationSection *target = VxGetRegistrySection(config, section.CStr(), FALSE);
    if (!target) return FALSE;
    XBOOL closesCurrent = FALSE;
    if (m_CurrentMode) {
        XString currentPath = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), m_CurrentSectionName.CStr());
        VxConfigurationSection *current = VxGetRegistrySection(config, currentPath.CStr(), FALSE);
        for (; current; current = current->GetParent()) {
            if (current == target) { closesCurrent = TRUE; break; }
        }
    }
    if (!target->GetParent()->DeleteSection(target->GetName()) || !config.SaveToFile(file.CStr())) return FALSE;
    if (closesCurrent) CloseSection(NULL);
    return TRUE;
}

XBOOL VxConfig::EntryExists(const char *iKey) {
    char buffer[4];
    return ReadRawEntry(iKey, buffer, sizeof(buffer), NULL);
}

XBOOL VxConfig::GetEntryType(const char *iKey, VxConfigValueType *oType) {
    char buffer[4];
    VxConfigValueType type = VXCONFIG_VALUE_STRING;
    if (!ReadRawEntry(iKey, buffer, sizeof(buffer), &type))
        return FALSE;
    if (oType)
        *oType = type;
    return TRUE;
}

XBOOL VxConfig::GetRegistryFilePath(char *oPath, size_t iSize) {
    if (!oPath || iSize == 0)
        return FALSE;
    XString file;
    if (!VxGetRegistryFilePathString(file))
        return FALSE;
    if ((size_t)file.Length() + 1 > iSize)
        return FALSE;
    memcpy(oPath, file.CStr(), (size_t)file.Length() + 1);
    return TRUE;
}

XBOOL VxConfig::WriteRawEntry(const char *iKey, const char *iValue, VxConfigValueType iType) {
    if (!(m_CurrentMode & WRITE) || !iValue)
        return FALSE;
    size_t length = strlen(iValue);
    if (length > XString::MAX_LENGTH) return FALSE;
    XBOOL encoded = FALSE;
    if (iType == VXCONFIG_VALUE_STRING) {
        encoded = length && (iValue[0] == ' ' || iValue[length - 1] == ' ');
        for (size_t i = 0; i < length && !encoded; ++i) {
            unsigned char c = static_cast<unsigned char>(iValue[i]);
            encoded = c < 32 || c == 127;
        }
    }
    // Hex doubles the length, so at most two entries cover the full XString range.
    size_t first = encoded && length > XString::MAX_LENGTH / 2 ? XString::MAX_LENGTH / 2 : length;
    XString value = encoded ? VxEncodeRegistryValue(iValue, first) : XString(iValue);
    XString tail;
    if (encoded && length > first) tail = VxEncodeRegistryValue(iValue + first, length - first);
    XString file;
    if (!VxGetRegistryFilePathString(file))
        return FALSE;
    VxCreateFileTree(file.CStr());
    VxConfiguration config;
    if (!VxLoadRegistryFile(config, file.CStr())) return FALSE;
    XString section = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), m_CurrentSectionName.CStr());
    XString key = VxNormalizeRegistryValueName(iKey);
    VxConfigurationSection *sectionObject = VxGetRegistrySection(config, section.CStr(), TRUE);
    if (!sectionObject) return FALSE;
    VxConfigurationEntry *entry = VxUpdateRegistryEntry(sectionObject, key.CStr(), value.CStr());
    XString storedKey(entry->GetName());
    VxUpdateRegistryEntry(sectionObject, (storedKey + ".type").CStr(), encoded ? "string.hex" : VxConfigTypeName(iType));
    VxUpdateRegistryEntry(sectionObject, (storedKey + ".tail").CStr(), tail.Empty() ? NULL : tail.CStr());
    return config.SaveToFile(file.CStr());
}

XBOOL VxConfig::ReadRawEntry(const char *iKey, char *oData, size_t iSize, VxConfigValueType *oType, size_t *oLength) {
    if ((!oData && !oLength) || (oData && iSize == 0)) return FALSE;
    XString value;
    VxConfigValueType type;
    if (!ReadEntryValue(iKey, value, &type)) return FALSE;
    if (oLength) {
        *oLength = value.Length() + 1;
        if (oData && *oLength > iSize) return FALSE;
    }
    if (oData) {
        strncpy(oData, value.CStr(), iSize - 1);
        oData[iSize - 1] = '\0';
    }
    if (oType) *oType = type;
    return TRUE;
}

XBOOL VxConfig::ReadEntryValue(const char *iKey, XString &result, VxConfigValueType *oType) {
    if (!(m_CurrentMode & READ))
        return FALSE;
    XString file;
    if (!VxGetRegistryFilePathString(file))
        return FALSE;
    VxConfiguration config;
    XString error;
    int line = 0;
    if (!config.BuildFromFile(file.CStr(), line, error))
        return FALSE;
    XString section = VxNormalizeRegistryPath(m_Root, m_BaseSection.CStr(), m_CurrentSectionName.CStr());
    XString key = VxNormalizeRegistryValueName(iKey);
    VxConfigurationSection *sectionObject = VxGetRegistrySection(config, section.CStr(), FALSE);
    if (!sectionObject)
        return FALSE;
    VxConfigurationEntry *entry = VxGetRegistryEntry(sectionObject, key.CStr());
    if (!entry)
        return FALSE;
    XString storedKey(entry->GetName());
    VxConfigurationEntry *typeEntry = VxGetRegistryEntry(sectionObject, (storedKey + ".type").CStr());
    const char *value = entry->GetValue();
    XString decoded;
    if (typeEntry && strcmp(typeEntry->GetValue(), "string.hex") == 0) {
        if (!VxDecodeRegistryValue(value, decoded)) return FALSE;
        VxConfigurationEntry *tail = VxGetRegistryEntry(sectionObject, (storedKey + ".tail").CStr());
        if (tail && !VxDecodeRegistryValue(tail->GetValue(), decoded)) return FALSE;
        value = decoded.CStr();
    }
    result = value;
    if (oType) {
        *oType = typeEntry ? VxConfigTypeFromName(typeEntry->GetValue()) : VXCONFIG_VALUE_STRING;
    }
    return TRUE;
}
