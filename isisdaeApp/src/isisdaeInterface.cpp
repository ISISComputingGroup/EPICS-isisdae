#include <stdio.h>

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
#include <atlbase.h>
#include <atlstr.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctl.h>
#include <atlhost.h>
#include <atlconv.h>
#include <atlsafe.h>
#include <comdef.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <map>
#include <list>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "isisdaeInterface.h"
#include "variant_utils.h"

#include <macLib.h>
#include <epicsGuard.h>

#define MAX_PATH_LEN 256

#include "dae.h"

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;


/// The Microsoft ATL _com_error is not derived from std::exception hence this bit of code to throw our own COMexception() instead
static void __stdcall my_com_raise_error(HRESULT hr, IErrorInfo* perrinfo) 
{
	_com_error com_error(hr, perrinfo);
//	std::string message = "(" + com_error.Source() + ") " + com_error.Description();
    _bstr_t desc = com_error.Description();
	std::string message = (desc.GetBSTR() != NULL ? desc : "");  // for LabVIEW generated messages, Description() already includes Source()
    throw COMexception(message, hr);
}

static void initCOM(void*)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
    _set_com_error_handler(my_com_raise_error); 
}

/// \param[in] configSection @copydoc initArg1
/// \param[in] configFile @copydoc initArg2
/// \param[in] host @copydoc initArg3
/// \param[in] options @copydoc initArg4
/// \param[in] progid @copydoc initArg5
/// \param[in] username @copydoc initArg6
/// \param[in] password @copydoc initArg7
isisdaeInterface::isisdaeInterface(const char* host, int options, const char* username, const char* password) : m_dcom(false), m_options(options)
{
    if (checkOption(daeDCOM))
	{
	    m_dcom = true;
	}
	epicsThreadOnce(&onceId, initCOM, NULL);
    if (host != NULL && host[0] != '\0') 
	{
	    m_host = host;
	}
	else
	{
//		char name_buffer[MAX_COMPUTERNAME_LENGTH + 1];
//		DWORD name_size = MAX_COMPUTERNAME_LENGTH + 1;
//		if ( GetComputerNameEx(ComputerNameNetBIOS, name_buffer, &name_size) != 0 )
//		{
//			m_host = name_buffer;
//		}
//		else
//		{
//			m_host = "localhost";
//		}			
		m_host = "localhost";
	}
	epicsAtExit(epicsExitFunc, this);
	if (m_dcom)
	{
		m_clsid = isisicpLib::CLSID_dae;
		wchar_t* progid_str = NULL;
		if ( ProgIDFromCLSID(m_clsid, &progid_str) == S_OK )
		{
			m_progid = CW2CT(progid_str);
			CoTaskMemFree(progid_str);
		}
		else
		{
			m_progid = "isisicp.dae";
		}
		wchar_t* clsid_str = NULL;
		if ( StringFromCLSID(m_clsid, &clsid_str) == S_OK )
		{
			std::cerr << "Using ProgID \"" << m_progid << "\" CLSID " << CW2CT(clsid_str) << std::endl;
			CoTaskMemFree(clsid_str);
		}
		else
		{
			std::cerr << "StringFromCLSID() failed" << std::endl;
		}
	}
	else
	{	
		ISISICPINT::areYouThere();
	}
}

void isisdaeInterface::epicsExitFunc(void* arg)
{
    isisdaeInterface* daeint = static_cast<isisdaeInterface*>(arg);
	if (daeint == NULL)
	{
		return;
	}
}

long isisdaeInterface::nParams()
{
	long n = 0;
	n = 10;
	return n;
}

void isisdaeInterface::getParams(std::map<std::string,std::string>& res)
{
	res.clear();
}

template<typename T>
T isisdaeInterface::callI( boost::function<T(std::string&)> func )
{
		std::string messages;
		T res = func(messages);
		if (messages.size() > 0)
		{
			std::cerr << messages << std::endl;
		}
		return res;
}

template <typename T>
T isisdaeInterface::callD( boost::function<T(ICPDCOM*, BSTR*)> func )
{
    BSTR messages = NULL;
	checkConnection();
	T res = func(m_icp, &messages);
	if (SysStringLen(messages) > 0)
	{
		std::cerr << COLE2CT(messages) << std::endl;
	}
	SysFreeString(messages);
	return res;
}

unsigned long isisdaeInterface::getGoodFrames()
{
	return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getGoodFramesTotal, _1, _2)) : callI<long>(boost::bind(&ISISICPINT::getGoodFramesTotal, _1)));
}

unsigned long isisdaeInterface::getGoodFramesPeriod()
{
	return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getGoodFramesPeriod, _1, _2)) : callI<long>(boost::bind(&ISISICPINT::getGoodFramesPeriod, _1)));
}

unsigned long isisdaeInterface::getRawFrames()
{
	return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getRawFramesTotal, _1, _2)) : callI<long>(boost::bind(&ISISICPINT::getRawFramesTotal, _1)));
}

COAUTHIDENTITY* isisdaeInterface::createIdentity(const std::string& user, const std::string&  domain, const std::string& pass)
{
	if (user.size() == 0)
	{
		return NULL;
	}
    COAUTHIDENTITY* pidentity = new COAUTHIDENTITY;
    pidentity->Domain = (USHORT*)strdup(domain.c_str());
    pidentity->DomainLength = static_cast<ULONG>(strlen((const char*)pidentity->Domain));
    pidentity->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
    pidentity->Password = (USHORT*)strdup(pass.c_str());
    pidentity->PasswordLength = static_cast<ULONG>(strlen((const char*)pidentity->Password));
    pidentity->User = (USHORT*)strdup(user.c_str());
    pidentity->UserLength = static_cast<ULONG>(strlen((const char*)pidentity->User));
    return pidentity;
}

HRESULT isisdaeInterface::setIdentity(COAUTHIDENTITY* pidentity, IUnknown* pUnk)
{
    HRESULT hr;
    if (pidentity != NULL)
    {
       hr = CoSetProxyBlanket(pUnk, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
            RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, pidentity, EOAC_NONE);
        if (FAILED(hr))
        {
			std::cerr << "setIdentity failed" << std::endl;
            return hr;
        }
    }
    return S_OK;
}

void isisdaeInterface::checkConnection()
{
	epicsThreadOnce(&onceId, initCOM, NULL);
	HRESULT hr;
	epicsGuard<epicsMutex> _lock(m_lock);
	if ( (m_icp != NULL) && (m_icp->areYouThere() == S_OK) )
	{
		;
	}
	else if (m_host.size() > 0)
	{
		std::cerr << "(Re)Making connection to ISISICP on " << m_host << std::endl;
		CComBSTR host(m_host.c_str());
		m_pidentity = createIdentity(m_username, m_host, m_password);
		COAUTHINFO* pauth = new COAUTHINFO;
		COSERVERINFO csi = { 0, NULL, NULL, 0 };
		pauth->dwAuthnSvc = RPC_C_AUTHN_WINNT;
		pauth->dwAuthnLevel = RPC_C_AUTHN_LEVEL_DEFAULT;
		pauth->dwAuthzSvc = RPC_C_AUTHZ_NONE;
		pauth->dwCapabilities = EOAC_NONE;
		pauth->dwImpersonationLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
		pauth->pAuthIdentityData = m_pidentity;
		pauth->pwszServerPrincName = NULL;
		csi.pwszName = host;
		csi.pAuthInfo = pauth;
		MULTI_QI mq[ 1 ] = { 0 }; 
		mq[ 0 ].pIID = &isisicpLib::IID_Idae; //&IID_IDispatch;
		mq[ 0 ].pItf = NULL; 
		mq[ 0 ].hr   = S_OK; 
		hr = CoCreateInstanceEx( m_clsid, NULL, CLSCTX_REMOTE_SERVER | CLSCTX_LOCAL_SERVER, &csi, 1, mq ); 
		if( FAILED( hr ) ) 
		{ 
			hr = CoCreateInstanceEx( m_clsid, NULL, CLSCTX_ALL, &csi, 1, mq );
		}
		if( FAILED( hr ) ) 
		{
 			throw COMexception("CoCreateInstanceEx (ISISICP) ", hr);
		} 
		if( S_OK != mq[ 0 ].hr || NULL == mq[ 0 ].pItf ) 
		{ 
 			throw COMexception("CoCreateInstanceEx (ISISICP)(mq) ", mq[ 0 ].hr);
		} 
		setIdentity(m_pidentity, mq[ 0 ].pItf);
		m_icp.Release();
		m_icp.Attach( reinterpret_cast< isisicpLib::Idae* >( mq[ 0 ].pItf ) ); 
	}
	else
	{
		std::cerr << "(Re)Making local connection to ISISICP" << std::endl;
		m_pidentity = NULL;
		m_icp.Release();
		hr = m_icp.CoCreateInstance(m_clsid, NULL, CLSCTX_LOCAL_SERVER);
		if( FAILED( hr ) ) 
		{
 			throw COMexception("CoCreateInstance (ISISICP) ", hr);
		} 
	}
}

double isisdaeInterface::getGoodUAH()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getGoodUAmpH, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getGoodUAmpH, _1)));
}

double isisdaeInterface::getGoodUAHPeriod()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getGoodUAmpHPeriod, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getGoodUAmpHPeriod, _1)));
}

int isisdaeInterface::beginRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::beginRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::beginRun, _1)));
}

int isisdaeInterface::beginRunEx(long options, long period)
{
    //This method allows the dae to begin running in the paused and/or waiting states
    //Options: 0 = normal; paused = 1; waiting = 2
    if (period == -1)
	{
	    period = getPeriod();
    }
	if (m_dcom)
	{
	    return callD<int>(boost::bind(&ICPDCOM::beginRunEx, _1, options, period, _2));
	}
	else
	{
	    return callI<int>(boost::bind(&ISISICPINT::beginRunEx, options, period, _1));
	}
}

int isisdaeInterface::abortRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::abortRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::abortRun, _1)));
}

int isisdaeInterface::pauseRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::pauseRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::pauseRun, _1)));
}

int isisdaeInterface::resumeRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::resumeRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::resumeRun, _1)));
}

int isisdaeInterface::endRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::endRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::endRun, _1)));
}

int isisdaeInterface::recoverRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::setICPValueLong, _1, L"RUN_STATUS", 3, _2)) : callI<int>(boost::bind(&ISISICPINT::setICPValueLong, "RUN_STATUS", 3, _1)));
}

int isisdaeInterface::saveRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::saveRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::saveRun, _1)));
}

int isisdaeInterface::updateRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::updateCRPT, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::updateCRPT, _1)));
}

int isisdaeInterface::storeRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::storeCRPT, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::storeCRPT, _1)));
}

int isisdaeInterface::getRunState()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::getRunState, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::getRunState, _1)));
}

long isisdaeInterface::getRunNumber()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::getRunNumber, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::getRunNumber, _1)));
}

int isisdaeInterface::getPeriod()
{
    long period, daq_period, ret;
	ret = (m_dcom ? callD<int>(boost::bind(&ICPDCOM::getCurrentPeriodNumber, _1, &period, &daq_period, _2)) : callI<int>(boost::bind(&ISISICPINT::getCurrentPeriodNumber, boost::ref(period), boost::ref(daq_period), _1)));
    return period;
}

long isisdaeInterface::getNumTimeChannels(int spec)
{
    return atol(getValue("NTC1").c_str());
}

std::string isisdaeInterface::getValue(const std::string& name)
{
    if (m_dcom)
	{
        _bstr_t bs(CComBSTR(name.c_str()).Detach());
		BSTR res = callD<_bstr_t>(boost::bind(&ICPDCOM::getValue, _1, bs, _2));
		return COLE2CT(res);
	}
	else
	{
	    return callI<std::string>(boost::bind(&ISISICPINT::getValue, boost::cref(name), _1));
	}
}

int isisdaeInterface::setSampleParameter(const std::string& name, const std::string& value)
{
    ISISICPINT::string_table_t table;
    table.resize(1);
    table[0].push_back(name);   // name
    table[0].push_back("String"); // type
    table[0].push_back("");  // units
    table[0].push_back(value);  // value
    if (m_dcom)
    {
        CComVariant v;
        stringTableToVariant(table, &v);
	    return callD<int>(boost::bind(&ICPDCOM::setSampleParameters, _1, v, _2));
    }
    else
    {
	    return callI<int>(boost::bind(&ISISICPINT::setSampleParameters, boost::cref(table), _1));
    }
}

int isisdaeInterface::setRunTitle(const std::string& title)
{
    return setSampleParameter("Run Title", title);
}

int isisdaeInterface::setUserParameters(long rbno, const std::string& name, const std::string& institute, const std::string& role)
{
    ISISICPINT::string_table_t table;
    table.resize(1);
    table[0].push_back(name);
    table[0].push_back(institute);
    table[0].push_back(role);
    if (m_dcom)
    {
        CComVariant v;
        stringTableToVariant(table, &v);
	    return callD<int>(boost::bind(&ICPDCOM::setUserParameters, _1, rbno, v, _2));
    }
    else
    {
	    return callI<int>(boost::bind(&ISISICPINT::setUserParameters, rbno, boost::cref(table), _1));
    }
    return 0;
}

long isisdaeInterface::getNumPeriods()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::getNumberOfPeriods, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::getNumberOfPeriods, _1)));
}

int isisdaeInterface::setPeriod(long period)
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changePeriod, _1, period, _2)) : callI<int>(boost::bind(&ISISICPINT::changePeriod, period, _1)));
}

int isisdaeInterface::setNumPeriods(long nperiods)
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeNumberOfSoftwarePeriods, _1, nperiods, _2)) : callI<int>(boost::bind(&ISISICPINT::changeNumberOfSoftwarePeriods, nperiods, _1)));
}

double isisdaeInterface::getMEvents()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getMEvents, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getMEvents, _1)));
}

unsigned long isisdaeInterface::getTotalCounts()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::getTotalCounts, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::getTotalCounts, _1)));
}

unsigned long isisdaeInterface::getHistogramMemory()
{
    long sum = 0;
	long ret = (m_dcom ? callD<int>(boost::bind(&ICPDCOM::sumAllHistogramMemory, _1, &sum, _2)) : callI<int>(boost::bind(&ISISICPINT::sumAllHistogramMemory, boost::ref(sum), _1)));
	return sum;
}

unsigned long isisdaeInterface::getSpectraSum()
{
   long counts = 0, bin0_counts = 0;
	long ret = (m_dcom ? callD<int>(boost::bind(&ICPDCOM::sumAllSpectra, _1, &counts, &bin0_counts, _2)) : callI<int>(boost::bind(&ISISICPINT::sumAllSpectra, boost::ref(counts), boost::ref(bin0_counts), _1)));
    return counts;
}

// for ISISICPINT namespace functions
int isisdaeInterface::getXMLSettingsI(std::string& result, const std::string& template_file, int (*func)(const std::string&, std::string&, std::string&))
{
	static std::map<std::string,std::string> xml_templates;
	if (xml_templates[template_file].size() == 0)
	{
	    std::string template_file_path = macEnvExpand("$(ISISDAE)/data/xml/") + template_file;
        loadFromFile(template_file_path, xml_templates[template_file]);
	}
	return callI<int>(boost::bind(func, boost::cref(xml_templates[template_file]), boost::ref(result), _1));
}

int isisdaeInterface::getXMLSettingsD(std::string& result, const std::string& template_file, HRESULT (ICPDCOM::*func)(_bstr_t, BSTR*, BSTR*))
{
	static std::map<std::string,_bstr_t> xml_templates;
	BSTR xml_out = NULL;
	if (xml_templates[template_file].length() == 0)
	{
	    std::string template_file_path = macEnvExpand("$(ISISDAE)/data/xml/") + template_file;
		std::string tstr;
        loadFromFile(template_file_path, tstr);
		xml_templates[template_file] = CComBSTR(tstr.c_str()).Detach();
	}
	int ret = callD<int>(boost::bind(func, _1, xml_templates[template_file], &xml_out, _2));
	result = COLE2CT(xml_out);
	SysFreeString(xml_out);
	return ret;
}

int isisdaeInterface::getDAESettingsXML(std::string& result)
{
    return (m_dcom ? getXMLSettingsD(result, "dae_settings_cluster.xml", &ICPDCOM::getDAEsettings) : getXMLSettingsI(result, "dae_settings_cluster.xml", &ISISICPINT::getDAEsettings));
}

int isisdaeInterface::getTCBSettingsXML(std::string& result)
{
    return (m_dcom ? getXMLSettingsD(result, "tcb_settings_cluster.xml", &ICPDCOM::getTCB) : getXMLSettingsI(result, "tcb_settings_cluster.xml", &ISISICPINT::getTCB));
}

int isisdaeInterface::getHardwarePeriodsSettingsXML(std::string& result)
{
    return (m_dcom ? getXMLSettingsD(result, "hardware_periods_cluster.xml", &ICPDCOM::getHardwarePeriods) : getXMLSettingsI(result, "hardware_periods_cluster.xml", &ISISICPINT::getHardwarePeriods));
}

int isisdaeInterface::getUpdateSettingsXML(std::string& result)
{
    return (m_dcom ? getXMLSettingsD(result, "update_settings_cluster.xml", &ICPDCOM::getUpdateSettings) : getXMLSettingsI(result, "update_settings_cluster.xml", &ISISICPINT::getUpdateSettings));
}

int isisdaeInterface::getMonitoringSettingsXML(std::string& result)
{
    return (m_dcom ? getXMLSettingsD(result, "monitoring_settings_cluster.xml", &ICPDCOM::getMonitoringSettings) : getXMLSettingsI(result, "monitoring_settings_cluster.xml", &ISISICPINT::getMonitoringSettings));
}

int isisdaeInterface::getVetoStatus(std::string& result)
{
    int res;
	if (m_dcom)
	{
        BSTR veto_status = NULL;
		res = callD<int>(boost::bind(&ICPDCOM::getVetoStatus, _1, &veto_status, _2));
		result = COLE2CT(veto_status);
		SysFreeString(veto_status);
	}
	else
	{
	    res = callI<int>(boost::bind(&ISISICPINT::getVetoStatus, boost::ref(result), _1));		
	}
    return res;
}

int isisdaeInterface::setTCBSettingsXML(const std::string& settings)
{
    _bstr_t bs(CComBSTR(settings.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeTCB, _1, bs, _2)) : callI<int>(boost::bind(&ISISICPINT::changeTCB, boost::cref(settings), _1)));
}

int isisdaeInterface::setDAESettingsXML(const std::string& settings)
{    
    _bstr_t bs(CComBSTR(settings.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeDAEsettings, _1, bs, _2)) : callI<int>(boost::bind(&ISISICPINT::changeDAEsettings, boost::cref(settings), _1)));
}

int isisdaeInterface::setMonitoringSettingsXML(const std::string& settings)
{    
    _bstr_t bs(CComBSTR(settings.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeMonitoringSettings, _1, bs, _2)) : callI<int>(boost::bind(&ISISICPINT::changeMonitoringSettings, boost::cref(settings), _1)));
}
 
int isisdaeInterface::setHardwarePeriodsSettingsXML(const std::string& settings)
{    
    _bstr_t bs(CComBSTR(settings.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeHardwarePeriods, _1, bs, _2)) : callI<int>(boost::bind(&ISISICPINT::changeHardwarePeriods, boost::cref(settings), _1)));
}

int isisdaeInterface::setUpdateSettingsXML(const std::string& settings)
{    
    _bstr_t bs(CComBSTR(settings.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changeUpdateSettings, _1, bs, _2)) : callI<int>(boost::bind(&ISISICPINT::changeUpdateSettings, boost::cref(settings), _1)));
}

int isisdaeInterface::getRunDataFromDAE(std::map<std::string, DAEValue>& values)
{
    std::string cluster_xml;
    int res = (m_dcom ? getXMLSettingsD(cluster_xml, "run_data_cluster.xml", &ICPDCOM::updateStatusXML2) : getXMLSettingsI(cluster_xml, "run_data_cluster.xml", &ISISICPINT::updateStatusXML2)); 
    CComPtr<IXMLDOMDocument> xmldom = createXmlDom(cluster_xml);
    extractValues(values, xmldom);        
    return 0;
}

int isisdaeInterface::loadFromFile(const std::string& filename, std::string& xml)
{
   struct stat stat_struct;
   FILE *f = _fsopen(filename.c_str(), "rtN", _SH_DENYWR);
   if (f == NULL)
   {
	   return -1;
   }
   fstat(fileno(f), &stat_struct);
   char* buffer = new char[stat_struct.st_size+1];
   size_t n = fread(buffer, 1, stat_struct.st_size, f);
   fclose(f);
   // labview can have nulls embedded
   for(int i=0; i<n; i++)
   {
	   if (buffer[i] == '\0')
	   {
		   buffer[i] = ' ';
	   }
   }
   buffer[n] = '\0';
   xml = buffer;
   delete[] buffer;
   return 0;
}

CComPtr<IXMLDOMDocument> isisdaeInterface::createXmlDom(const std::string& xml)
{
    CComPtr<IXMLDOMDocument> spXMLDOM;
    VARIANT_BOOL b_status = VARIANT_FALSE;
    
    HRESULT hr = spXMLDOM.CoCreateInstance(__uuidof(DOMDocument));
    
    if ( FAILED(hr) ) 
        std::cout << "Unable to create XML parser object\n";
    if ( spXMLDOM.p == NULL ) 
        std::cout << "Unable to create XML parser object\n";
    
	spXMLDOM->put_async(VARIANT_FALSE);
	spXMLDOM->put_validateOnParse(VARIANT_FALSE);
	spXMLDOM->put_resolveExternals(VARIANT_FALSE);
    spXMLDOM->loadXML(CT2OLE(xml.c_str()), &b_status);
    if (b_status!=VARIANT_TRUE) {
        std::cout << "Some sort of error\n";
    }
    return spXMLDOM;
}

void isisdaeInterface::getNameAndValue(CComPtr<IXMLDOMNode> spXMLNode, BSTR* name, BSTR* value, DAEValue::DAEType type)
{
    //We know the node will be something like:
    //  <EW>
	//	    <Name>RunStatus</Name>
	//	    <Choice>PROCESSING</Choice>
	//  	<Choice>SETUP</Choice>
	//	    <Choice>RUNNING</Choice>
	//  	<Choice>PAUSED</Choice>
	//  	<Choice>WAITING</Choice>
	//	    <Val>2</Val>
	//  </EW>
    //  <String>
    //      <Name>UserName</Name>
    //      <Val>John Smith</Val>
    //  </String>
	HRESULT hr;
    CComPtr<IXMLDOMNode> spXMLNode2;
    CComPtr<IXMLDOMNode> spXMLNode3;    
    
    CComBSTR bstrSS(L"./Name");
    CComBSTR bstrSS2(L"./Val");
    CComBSTR bstrSS3(L"./Choice");
    
    hr = spXMLNode->selectSingleNode(bstrSS, &spXMLNode2);
    hr = spXMLNode2->get_firstChild(&spXMLNode3);
    hr = spXMLNode3->get_xml(name);
	spXMLNode3.Release(); // as we use it again later
	spXMLNode2.Release(); // as we use it again later

    hr = spXMLNode->selectSingleNode(bstrSS2, &spXMLNode2);
    hr = spXMLNode2->get_firstChild(&spXMLNode3);
	if ( FAILED(hr) || (spXMLNode3.p == NULL) ) // we may have an empty value
	{
		*value = SysAllocString(L"");
		return;
	}

    if (type == DAEValue::DAEEnum)
	{
        BSTR bstr_value = NULL;
        hr = spXMLNode3->get_xml(&bstr_value);
    	spXMLNode3.Release();
	    spXMLNode2.Release();
        unsigned long index = 0;
        VarUI4FromStr(bstr_value, 0, 0, &index);
        CComPtr<IXMLDOMNodeList> spXMLNodes;
        spXMLNode->selectNodes(bstrSS3, &spXMLNodes);
        spXMLNodes->get_item(index, &spXMLNode2);
        spXMLNode2->get_firstChild(&spXMLNode3);
        spXMLNode3->get_xml(value);
        SysFreeString(bstr_value);
	}
	else
	{
        spXMLNode3->get_xml(value);
	}
}

int isisdaeInterface::extractValues(std::map<std::string, DAEValue>& values, CComPtr<IXMLDOMDocument> spXMLDOM)
{
    values.clear();
    extractValues("Cluster/String", DAEValue::DAEString, values, spXMLDOM);
    extractValues("Cluster/EW", DAEValue::DAEEnum, values, spXMLDOM);
    extractValues("Cluster/DBL", DAEValue::DAEDouble, values, spXMLDOM);
    extractValues("Cluster/I32", DAEValue::DAEInt, values, spXMLDOM);
    extractValues("Cluster/U32", DAEValue::DAEUInt, values, spXMLDOM);
	return 0;
}

int isisdaeInterface::extractValues(const char* name, DAEValue::DAEType type, std::map<std::string, DAEValue>& values, CComPtr<IXMLDOMDocument> spXMLDOM)
{
	CComPtr<IXMLDOMNodeList> spXMLNodes;
    HRESULT hr = spXMLDOM->selectNodes(CComBSTR(name), &spXMLNodes);
    if ( FAILED(hr) ) 
       std::cout << "Unable to locate XML node";
    
    long length = 0;
    hr = spXMLNodes->get_length(&length);
    for (long i = 0; i < length; i++)
    {
        CComPtr<IXMLDOMNode> spXMLNode;
        BSTR bstrName = NULL;
        BSTR bstrValue = NULL;
        
        spXMLNodes->get_item(i, &spXMLNode);
		getNameAndValue(spXMLNode, &bstrName, &bstrValue, type);
        
        //printf("%S, %S%\n", bstrName, bstrValue);
		values[std::string(COLE2CT(bstrName))] = DAEValue(type, COLE2CT(bstrValue));
        
        SysFreeString(bstrName);
        SysFreeString(bstrValue);
    }
    
    return 0;
}

long isisdaeInterface::getSpectrum(int spec, int period, float* time_channels, float* signal, long nvals)
{
	long sum = 0, n;
	if (m_dcom)
	{
		variant_t time_channels_v, signal_v;
		callD<int>(boost::bind(&ICPDCOM::getSpectrum, _1, spec, period, &time_channels_v, &signal_v, false, true, &sum, _2));
		double *s = NULL, *t = NULL;
		accessArrayVariant(&signal_v, &s);
		accessArrayVariant(&time_channels_v, &t);
		n = arrayVariantLength(&signal_v);
		for(int i=0; i < std::min(nvals,n); ++i)
	    {
	        signal[i] = s[i];
			time_channels[i] = t[i];
	    }
		unaccessArrayVariant(&signal_v);
		unaccessArrayVariant(&time_channels_v);
	}
	else
	{
		std::vector<double> time_channels_v, signal_v;
		callI<int>(boost::bind(&ISISICPINT::getSpectrum, spec, period, boost::ref(time_channels_v), boost::ref(signal_v), false, true, boost::ref(sum), _1));
		n = signal_v.size();
		for(int i=0; i < std::min(nvals,n); ++i)
	    {
	        time_channels[i] = time_channels_v[i];
	        signal[i] = signal_v[i];
	    }
	}
    return n;
}

long isisdaeInterface::getSpectrumIntegral(long spectrum_number, long period, float time_low, float time_high, long& counts)
{
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::getSpectrumIntegral, _1, spectrum_number, period, time_low, time_high, &counts, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::getSpectrumIntegral, spectrum_number, period, time_low, time_high, boost::ref(counts), _1));
	}
    return 0;
}

