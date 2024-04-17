#include "CKDirectoryParser.h"

#include <io.h>
#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

CKDirectoryParser::CKDirectoryParser(char *dir, char *fileMask, XBOOL recurse)
{
    m_FindData = NULL;
    m_StartDir = NULL;
    m_FullFileName = NULL;
    m_FileMask = NULL;
    m_SubParser = NULL;
    m_hFile = -1;
    Reset(dir, fileMask, recurse);
}

CKDirectoryParser::~CKDirectoryParser()
{
    delete (_finddata_t *)m_FindData;
    delete[] m_StartDir;
    delete[] m_FullFileName;
    delete[] m_FileMask;

    if (m_SubParser)
        delete m_SubParser;

    if (m_hFile != -1)
        _findclose(m_hFile);
    m_hFile = -1;
}

char *CKDirectoryParser::GetNextFile()
{
    char buf[MAX_PATH];
    if ((m_State & 2) == 0)
    {
        sprintf(buf, "%s\\%s", m_StartDir, m_FileMask);
        if (m_hFile == -1)
        {
            m_hFile = _findfirst(buf, (_finddata_t *)m_FindData);
            if (m_hFile == -1)
            {
                if ((m_State & 1) != 0)
                    m_State |= 2;
                _findclose(m_hFile);
                m_hFile = -1;
                if ((m_State & 2) == 0)
                    return 0;
            }
            else
            {
                if ((((_finddata_t *)m_FindData)->attrib & 0x10) != 0)
                    return GetNextFile();
                sprintf(m_FullFileName, "%s\\%s", m_StartDir, ((_finddata_t *)m_FindData)->name);
                return m_FullFileName;
            }
        }
        if (!_findnext(m_hFile, (_finddata_t *)m_FindData))
        {
            if ((((_finddata_t *)m_FindData)->attrib & 0x10) != 0)
                return GetNextFile();
            sprintf(m_FullFileName, "%s\\%s", m_StartDir, ((_finddata_t *)m_FindData)->name);
            return m_FullFileName;
        }
        if ((m_State & 1) != 0)
            m_State |= 2;
        _findclose(m_hFile);
        m_hFile = -1;
        if ((m_State & 2) == 0)
            return NULL;
    }
    else
    {
        char *ret;
        if (m_hFile == -1)
        {
            sprintf(buf, "%s\\%s", m_StartDir, "*.*");
            m_hFile = _findfirst(buf, (_finddata_t *)m_FindData);
            if (m_hFile == -1)
                return NULL;
            if (strcmp(((_finddata_t *)m_FindData)->name, ".") != 0 &&
                strcmp(((_finddata_t *)m_FindData)->name, "..") != 0 &&
                ((((_finddata_t *)m_FindData)->attrib & 0x10) != 0))
            {
                char dir[MAX_PATH];
                sprintf(dir, "%s\\%s", m_StartDir, ((_finddata_t *)m_FindData)->name);
                m_SubParser = new CKDirectoryParser(dir, m_FileMask, TRUE);
                while (true)
                {
                    ret = m_SubParser->GetNextFile();
                    if (ret)
                        return ret;

                    if (m_SubParser)
                        delete m_SubParser;
                    m_SubParser = NULL;
                }
            }
        }
        if (!m_SubParser)
        {
            while (!_findnext(m_hFile, (_finddata_t *)m_FindData))
            {
                if (strcmp(((_finddata_t *)m_FindData)->name, ".") != 0 &&
                    strcmp(((_finddata_t *)m_FindData)->name, "..") != 0 &&
                    ((((_finddata_t *)m_FindData)->attrib & 0x10) != 0))
                {
                    char dir[MAX_PATH];
                    sprintf(dir, "%s\\%s", m_StartDir, ((_finddata_t *)m_FindData)->name);
                    m_SubParser = new CKDirectoryParser(dir, m_FileMask, TRUE);
                    ret = m_SubParser->GetNextFile();
                    if (ret)
                        return ret;

                    if (m_SubParser)
                        delete m_SubParser;
                    m_SubParser = NULL;
                }
            }
            _findclose(m_hFile);
            m_hFile = -1;
            return NULL;
        }
    }
    return NULL;
}

void CKDirectoryParser::Reset(char *dir, char *fileMask, XBOOL recurse)
{
    delete (_finddata_t *)m_FindData;
    delete[] m_FullFileName;
    if (m_SubParser)
        delete m_SubParser;
    if (dir)
        delete[] m_StartDir;
    if (fileMask)
        delete[] m_FileMask;

    m_FindData = new _finddata_t;
    m_FullFileName = new char[MAX_PATH];
    if (dir)
        m_StartDir = new char[strlen(dir) + 1];
    strcpy(m_StartDir, dir);
    if (fileMask)
        m_FileMask = new char[strlen(fileMask) + 1];
    strcpy(m_FileMask, fileMask);
    if (m_hFile != -1)
        _findclose(m_hFile);
    m_State = recurse;
    m_hFile = -1;
    m_SubParser = NULL;
    if (m_StartDir[strlen(dir) - 1] == '\\' || m_StartDir[strlen(dir) - 1] == '/')
        m_StartDir[strlen(dir) - 1] = '\0';
}

void CKDirectoryParser::Clean() {
    delete (_finddata_t *)m_FindData;

    if (m_SubParser)
        delete m_SubParser;

    if (m_hFile != -1)
        _findclose(m_hFile);
    m_hFile = -1;
}