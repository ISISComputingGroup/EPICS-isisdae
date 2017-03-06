#include "CRPTMapping.h"

CRPTMapping::CRPTMapping() : m_crpt_data_fm(NULL), m_crpt_data(NULL)
{
	map();
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
	if ( (m_crpt_data_fm = OpenFileMapping(map_opt, FALSE, map_name)) == NULL )
	{
		unmap();
		return -1;
	}
	if ( (m_crpt_data = (uint32_t*)MapViewOfFile(m_crpt_data_fm, map_opt, 0, 0, 0)) == NULL )
	{
		unmap();
		return -1;		
	}
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
	return 0;
}
