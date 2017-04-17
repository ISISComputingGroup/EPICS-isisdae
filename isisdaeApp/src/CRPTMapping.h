#ifndef ISISDAE_CRPTMAPPING_H
#define ISISDAE_CRPTMAPPING_H

#include <windows.h>
#include <stdint.h>

#define ISISCRPT_MAX_SPEC_INTEGRALS 5000000   ///< must agree with ISISICP

class CRPTMapping
{
	public:
		CRPTMapping();
		~CRPTMapping();
        int map();
        int unmap();	
		const uint32_t* getaddr() { return m_crpt_data; }
		uint32_t getsize() { return m_crpt_data_size; }
		static void translateDeviceName(char* pszFilename);

	private:
	
        HANDLE m_crpt_data_fm;
        const uint32_t* m_crpt_data;
		uint32_t m_crpt_data_size; ///< size excluding spec integrals i.e. whole size less #ISISCRPT_MAX_SPEC_INTEGRALS
		std::string fileNameFromMappedView(void* pMem);
		int64_t fileSizeBytes(const std::string& filename);
};

#endif /* ISISDAE_CRPTMAPPING_H */
