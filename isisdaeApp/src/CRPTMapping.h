#ifndef ISISDAE_CRPTMAPPING_H
#define ISISDAE_CRPTMAPPING_H

#include <windows.h>
#include <stdint.h>

class CRPTMapping
{
	public:
		CRPTMapping();
		~CRPTMapping();
        int map();
        int unmap();	
		uint32_t* getaddr() { return m_crpt_data; }

	private:
        HANDLE m_crpt_data_fm;
        uint32_t* m_crpt_data;
};

#endif /* ISISDAE_CRPTMAPPING_H */
