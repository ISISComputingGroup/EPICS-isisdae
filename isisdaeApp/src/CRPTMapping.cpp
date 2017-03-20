#include <string>
#include <iostream>
#include <epicsThread.h>
#include "CRPTMapping.h"
#include <psapi.h>

CRPTMapping::CRPTMapping() : m_crpt_data_fm(NULL), m_crpt_data(NULL), m_crpt_data_size(0)
{
	if (map() == -1)
	{
		throw std::runtime_error("CRPTMapping error");
	}
}

CRPTMapping::~CRPTMapping()
{
	unmap();
}

int CRPTMapping::map()
{
	int map_opt = FILE_MAP_READ;
	const char* map_name = "isis_data";
	unmap();
	for(int i = 0; ((m_crpt_data_fm = OpenFileMapping(map_opt, FALSE, map_name)) == NULL) && i < 20; ++i)
	{
		std::cerr << "Waiting for global section" << std::endl;
		epicsThreadSleep(5.0);
	}
	if (m_crpt_data_fm == NULL)
	{
		return -1;		
	}
	if ( (m_crpt_data = (uint32_t*)MapViewOfFile(m_crpt_data_fm, map_opt, 0, 0, 0)) == NULL )
	{
		unmap();
		return -1;		
	}
	std::string fname = fileNameFromMappedView((void*)m_crpt_data);
	int64_t fsize = fileSizeBytes(fname);
	if (fsize == 0)
	{
		fsize = fileSizeBytes("c:\\data\\data.run");
	}
	m_crpt_data_size = fsize / sizeof(uint32_t) - ISISCRPT_MAX_SPEC_INTEGRALS;
	std::cerr << "CRPT data size (words) = " << m_crpt_data_size << std::endl;
	std::cerr << "Spec integrals size (words) = " << ISISCRPT_MAX_SPEC_INTEGRALS << std::endl;
	return 0;
}

int CRPTMapping::unmap()
{
    if (m_crpt_data != NULL)
	{
		UnmapViewOfFile(m_crpt_data);
		m_crpt_data = NULL;
	}
    if (m_crpt_data_fm != NULL)
	{
		CloseHandle(m_crpt_data_fm);
		m_crpt_data_fm = NULL;
	}
	m_crpt_data_size = 0;
	return 0;
}

std::string CRPTMapping::fileNameFromMappedView(void* pMem)
{
	TCHAR pszFilename[MAX_PATH+1];
	int n = GetMappedFileName(GetCurrentProcess(), pMem, pszFilename, MAX_PATH);
	pszFilename[n] = '\0';
	if (n > 0)
	{
		translateDeviceName(pszFilename);
		return pszFilename;
	}
	else
	{
		return "";
	}
}

int64_t CRPTMapping::fileSizeBytes(const std::string& filename)
{
	int64_t the_size = 0;
	WIN32_FILE_ATTRIBUTE_DATA file_attr;
	if ( GetFileAttributesEx(filename.c_str(), GetFileExInfoStandard, &file_attr) != 0 )
	{
		the_size = (static_cast<int64_t>(file_attr.nFileSizeHigh) << 32) | file_attr.nFileSizeLow;
	}
	return the_size;
}

void CRPTMapping::translateDeviceName(char* pszFilename)
{
	// Translate path with device name to drive letters.
	const int buffer_size = 512;
	char szTemp[buffer_size];
	szTemp[0] = '\0';

	if (GetLogicalDriveStrings(buffer_size-1, szTemp)) 
	{
		char szName[MAX_PATH];
		char szDrive[3] = " :";
		BOOL bFound = FALSE;
		char* p = szTemp;

		do 
		{
			// Copy the drive letter to the template string
			*szDrive = *p;

			// Look up each device name
			if (QueryDosDevice(szDrive, szName, MAX_PATH))
			{
				size_t uNameLen = strlen(szName);

				if (uNameLen < MAX_PATH) 
				{
					bFound = (strnicmp(pszFilename, szName, uNameLen) == 0);

					if (bFound && *(pszFilename + uNameLen) == '\\') 
					{
						// Reconstruct pszFilename using szTempFile
						// Replace device path with DOS path
						char szTempFile[MAX_PATH];
						_snprintf(szTempFile, MAX_PATH, "%s%s", szDrive, pszFilename + uNameLen);
						strncpy(pszFilename, szTempFile, MAX_PATH);
					}
				}
			}

			// Go to the next NULL character.
			while (*p++);
		} while (!bFound && *p); // end of string
	}
}
