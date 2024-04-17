#include "CKPathSplitter.h"

CKPathSplitter::CKPathSplitter(char *file)
{
    _splitpath(file, m_Drive, m_Dir, m_Fname, m_Ext);
}

CKPathSplitter::~CKPathSplitter() {}

char *CKPathSplitter::GetDrive()
{
    return m_Drive;
}

char *CKPathSplitter::GetDir()
{
    return m_Dir;
}

char *CKPathSplitter::GetName()
{
    return m_Fname;
}

char *CKPathSplitter::GetExtension()
{
    return m_Ext;
}

CKPathMaker::CKPathMaker(char *Drive, char *Directory, char *Fname, char *Extension)
{
    _makepath(m_FileName, Drive, Directory, Fname, Extension);
}

char *CKPathMaker::GetFileName()
{
    return m_FileName;
}