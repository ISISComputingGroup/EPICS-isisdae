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
#include <time.h>

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
#include "CRPTMapping.h"

#include <utilities.h>

//#include <boost/tr1/functional.hpp>

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
isisdaeInterface::isisdaeInterface(const char* host, int options, const char* username, const char* password) : m_dcom(false), m_options(options), m_data(NULL), m_data_map(NULL)
{
    if (checkOption(daeDCOM))
	{
	    m_dcom = true;
	}
	epicsThreadOnce(&onceId, initCOM, NULL);
    if ( host != NULL && host[0] != '\0' && strcmp(host, "localhost") && strcmp(host, "127.0.0.1") )
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
        // don't default to localhost, blank lets us try CoCreateInstance() rather than CoCreateInstanceEx()		
		m_host = "";
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
			std::cerr << "ProgIDFromCLSID() failed" << std::endl;
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
		std::cerr << "DAE is not using DCOM" << std::endl;
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
		std::string messages, messages_t;
		T res = func(messages);
		if (messages.size() > 0)
		{
			stripTimeStamp(messages, messages_t);
			m_allMsgs += messages;
		}
		return res;
}

template <typename T>
T isisdaeInterface::callD( boost::function<T(ICPDCOM*, BSTR*)> func )
{
    BSTR bmessages = NULL;
	std::string messages, messages_t;
	checkConnection();
	T res = func(m_icp, &bmessages);
	if (SysStringLen(bmessages) > 0)
	{
		messages = COLE2CT(bmessages);
		stripTimeStamp(messages, messages_t);
		m_allMsgs += messages;
	}
	SysFreeString(bmessages);
	return res;
}

#if 0
// may move boost::bind -> tr1 when confirm all is OK
template<typename T>
T isisdaeInterface::callItr1( std::tr1::function<T(std::string&)> func )
{
		std::string messages, messages_t;
		T res = func(messages);
		if (messages.size() > 0)
		{
			stripTimeStamp(messages, messages_t);
			m_allMsgs += messages;
		}
		return res;
}

template <typename T>
T isisdaeInterface::callDtr1( std::tr1::function<T(ICPDCOM*, BSTR*)> func )
{
    BSTR bmessages = NULL;
	std::string messages, messages_t;
	checkConnection();
	T res = func(m_icp, &bmessages);
	if (SysStringLen(bmessages) > 0)
	{
		messages = COLE2CT(bmessages);
		stripTimeStamp(messages, messages_t);
		m_allMsgs += messages;
	}
	SysFreeString(bmessages);
	return res;
}
#endif

const std::string& isisdaeInterface::getAllMessages() const
{
    return m_allMsgs;
}

// string strings of the form "2014-07-10 12:34:15 "
// assumes we can look for 2014-07- and then remove 20 chars
void isisdaeInterface::stripTimeStamp(const std::string& in, std::string& out)
{
    char time_str[64];
    struct tm tmstr;
	time_t now;
	time(&now);
	memcpy(&tmstr, localtime(&now), sizeof(tmstr));
    strftime(time_str, sizeof(time_str), "%Y-%m-", &tmstr);
    out = in;
	size_t pos = 0;
	while( (pos = out.find(time_str, pos)) != std::string::npos )
	{
	    out.erase(pos, 20);
	}
}

void isisdaeInterface::resetMessages()
{
    m_allMsgs.clear();
}

int isisdaeInterface::getAsyncMessages(std::list<std::string>& messages)
{
    int res;
    if (m_dcom)
	{
	    checkConnection();
		variant_t mess;
		res = m_icp->getStatusMessages(0, &mess);
		BSTR *s = NULL;
		accessArrayVariant(&mess, &s);
		int n = arrayVariantLength(&mess);
		messages.clear();
		for(int i=0; i < n; ++i)
	    {
		    std::string str = COLE2CT(s[i]);
	        messages.push_back(str);
	    }
		unaccessArrayVariant(&mess);
	}
	else
	{
	    res = ISISICPINT::getStatusMessages(0, messages);
	}
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

unsigned long isisdaeInterface::getRawFramesPeriod()
{
	return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getRawFramesPeriod, _1, _2)) : callI<long>(boost::bind(&ISISICPINT::getRawFramesPeriod, _1)));
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
	HRESULT hr = E_FAIL;
	if (!m_dcom) // we get called from isisDaeDriver so need to check here too
	{
		return;
	}
	epicsGuard<epicsMutex> _lock(m_lock);
	try
	{
		if (m_icp != NULL)
		{
			hr = m_icp->areYouThere();
		}
	}
	catch(...)
	{
		if (checkOption(daeSECI))
		{
			std::cerr << "Terminating as in SECI mode and ICP not present" << std::endl;
			epicsExit(0);
		}
	}
	if (hr == S_OK)
	{
		return;
	}
	if (m_icp != NULL)
	{
		try
		{
		    m_icp.Release();
		}
 	    catch(...)
	    {
		    ;
		}
		epicsThreadSleep(3.0);
	}
	epicsThreadSleep(1.0); // always sleep here to cut down on reconnection loop errors
	if (m_host.size() > 0)
	{
		delete m_data_map;
		m_data_map = NULL;
		m_data = NULL;
		m_spec_integrals = NULL;
		m_pidentity = NULL;
		maybeWaitForISISICP();
		std::cerr << "(Re)Making connection to ISISICP on " + m_host << std::endl;
		m_allMsgs.append("(Re)Making connection to ISISICP on " + m_host + "\n");
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
		m_icp.Attach( reinterpret_cast< isisicpLib::Idae* >( mq[ 0 ].pItf ) );
		m_icp->areYouThere();
		try
		{
			m_data_map = new CRPTMapping;
			m_data = m_data_map->getaddr();
			m_data_size = m_data_map->getsize();
			m_spec_integrals = m_data + m_data_size;
		}
		catch(const std::exception& ex)
		{
			m_data_map = NULL;
			std::cerr << "Error mapping CRPT: " << ex.what() << std::endl;
		}
	}
	else
	{
		delete m_data_map;
		m_data_map = NULL;
		m_data = NULL;
		m_spec_integrals = NULL;
		m_pidentity = NULL;
		maybeWaitForISISICP();
		std::cerr << "(Re)Making local connection to ISISICP" << std::endl;
		m_allMsgs.append("(Re)Making local connection to ISISICP\n");
		hr = m_icp.CoCreateInstance(m_clsid, NULL, CLSCTX_LOCAL_SERVER);
		if( FAILED( hr ) )
        {
		    std::cerr << "CoCreateInstance() failed with error " << hr << ", retrying with CLSCTX_ALL" << std::endl;
		    hr = m_icp.CoCreateInstance(m_clsid, NULL, CLSCTX_ALL);
        }
		if( FAILED( hr ) ) 
		{
 			throw COMexception("CoCreateInstance (ISISICP) ", hr);
		}
		m_icp->areYouThere();
		try
		{
			m_data_map = new CRPTMapping;
			m_data = m_data_map->getaddr();
			m_data_size = m_data_map->getsize();
			m_spec_integrals = m_data + m_data_size;
		}
		catch(const std::exception& ex)
		{
			m_data_map = NULL;
			std::cerr << "Error mapping CRPT: " << ex.what() << std::endl;
		}
	}
}

double isisdaeInterface::maybeWaitForISISICP()
{
		if (checkOption(daeSECI))
		{
			return waitForISISICP();
		}
		else
		{
			return 0.0;
		}
}

double isisdaeInterface::waitForISISICP()
{
	static double min_uptime = 20.0;
	double icpuptime = getProcessUptime("isisicp.exe");
	if (icpuptime < min_uptime)
	{
	    std::cerr << "ISISICP not currently running - waiting for ISISICP uptime of " << min_uptime << " seconds..." << std::endl;
        while ( (icpuptime = getProcessUptime("isisicp.exe")) < min_uptime )
	    {
		    epicsThreadSleep(5.0);
	    }
	}
	return icpuptime;
}

double isisdaeInterface::getGoodUAH()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getGoodUAmpH, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getGoodUAmpH, _1)));
}

double isisdaeInterface::getGoodUAHPeriod()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getGoodUAmpHPeriod, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getGoodUAmpHPeriod, _1)));
}

double isisdaeInterface::getRawUAH()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getRawUAmpH, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getRawUAmpH, _1)));
}

double isisdaeInterface::getRawUAHPeriod()
{
	return (m_dcom ? callD<double>(boost::bind(&ICPDCOM::getRawUAmpHPeriod, _1, _2)) : callI<double>(boost::bind(&ISISICPINT::getRawUAmpHPeriod, _1)));
}

int isisdaeInterface::beginRun()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::beginRun, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::beginRun, _1)));
}

int isisdaeInterface::startSEWait()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::startSEWait, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::startSEWait, _1)));
}

int isisdaeInterface::endSEWait()
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::endSEWait, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::endSEWait, _1)));
}

int isisdaeInterface::setVeto(const std::string& veto, bool enable)
{
    _bstr_t veto_(CComBSTR(veto.c_str()).Detach());
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::setVeto, _1, veto_, (enable ? 1 : 0), _2)) : callI<int>(boost::bind(&ISISICPINT::setVeto, veto, enable, _1)));	
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

int isisdaeInterface::snapshotCRPT(const std::string& filename, long do_update, long do_pause)
{
    if (m_dcom)
	{
        _bstr_t bs(CComBSTR(filename.c_str()).Detach());
		return callD<int>(boost::bind(&ICPDCOM::snapshotCRPT, _1, bs, do_update, do_pause, _2));
	}
	else
	{
	    return callI<int>(boost::bind(&ISISICPINT::snapshotCRPT, boost::cref(filename), (do_update != 0 ? true : false), (do_pause != 0 ? true : false), _1));
	}
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

//long isisdaeInterface::getNumTimeChannels(int spec)
//{
//   return atol(getValue("NTC1").c_str());
//}

long isisdaeInterface::getDAEType()
{
    return atol(getValue("DAETYPE").c_str());
}


long isisdaeInterface::getCRPTDataWords()
{
    return atol(getValue("CRPTDATASIZE").c_str());
}

std::string isisdaeInterface::getValue(const std::string& name)
{
    if (m_dcom)
	{
        _bstr_t bs(CComBSTR(name.c_str()).Detach());
		_variant_t res = callD<_variant_t>(boost::bind(&ICPDCOM::getValue, _1, bs, _2));
		_variant_t cres;
		if ( VariantChangeType(&cres, &res, 0, VT_BSTR) == S_OK )
		{
		    return std::string(COLE2CT((_bstr_t)cres));
		}
		else
		{
		    return std::string("ERROR");
		}
	}
	else
	{
	    return callI<std::string>(boost::bind(&ISISICPINT::getValue, boost::cref(name), _1));
	}
}

int isisdaeInterface::setICPValue(const std::string& name, const std::string& value)
{
    if (m_dcom)
	{
        _bstr_t name_bs(CComBSTR(name.c_str()).Detach());
        _bstr_t value_bs(CComBSTR(value.c_str()).Detach());
		return callD<int>(boost::bind(&ICPDCOM::setICPValue, _1, boost::cref(name_bs), boost::cref(value_bs), _2));
	}
	else
	{
	    return callI<int>(boost::bind(&ISISICPINT::setICPValue, boost::cref(name), boost::cref(value), _1));
	}
}

int isisdaeInterface::setICPValueLong(const std::string& name, long value)
{
    if (m_dcom)
	{
        _bstr_t name_bs(CComBSTR(name.c_str()).Detach());
		return callD<int>(boost::bind(&ICPDCOM::setICPValueLong, _1, boost::cref(name_bs), value, _2));
	}
	else
	{
	    return callI<int>(boost::bind(&ISISICPINT::setICPValueLong, boost::cref(name), value, _1));
	}
}

int isisdaeInterface::setSampleParameter(const std::string& name, const std::string& type, const std::string& units, const std::string& value)
{
    ISISICPINT::string_table_t table;
    table.resize(1);
    table[0].push_back(name);   // name
    table[0].push_back(type); // type
    table[0].push_back(units);  // units
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

int isisdaeInterface::setBeamlineParameter(const std::string& name, const std::string& type, const std::string& units, const std::string& value)
{
    ISISICPINT::string_table_t table;
    table.resize(1);
    table[0].push_back(name);   // name
    table[0].push_back(type); // type
    table[0].push_back(units);  // units
    table[0].push_back(value);  // value
    if (m_dcom)
    {
        CComVariant v;
        stringTableToVariant(table, &v);
	    return callD<int>(boost::bind(&ICPDCOM::setBeamlineParameters, _1, v, _2));
    }
    else
    {
	    return callI<int>(boost::bind(&ISISICPINT::setBeamlineParameters, boost::cref(table), _1));
    }
}

int isisdaeInterface::setRunTitle(const std::string& title)
{
    return setSampleParameter("Run Title", "String", "", title);
}

std::string isisdaeInterface::getRunTitle()
{
    return getValue("TITL");
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
	return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getNumberOfPeriods, _1, _2)) : callI<int>(boost::bind(&ISISICPINT::getNumberOfPeriods, _1)));
}

int isisdaeInterface::setPeriod(long period)
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changePeriod, _1, period, _2)) : callI<int>(boost::bind(&ISISICPINT::changePeriod, period, _1)));
}

int isisdaeInterface::changePeriodWhileRunning(long period, bool pause_first)
{
	return (m_dcom ? callD<int>(boost::bind(&ICPDCOM::changePeriodWhileRunning, _1, period, (pause_first?1:0), _2)) : callI<int>(boost::bind(&ISISICPINT::changePeriod, period, _1)));
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

// needed for VS2010 and std::tr1::bind, VS2015 was OK without
struct getSpectraSumWrapStruct
{
    long period;
	long first_spec;
	long num_spec;
	getSpectraSumWrapStruct(long period_, long first_spec_, long num_spec_) : period(period_), first_spec(first_spec_), num_spec(num_spec_) { }
};

static HRESULT getSpectraSumWrapD(isisdaeInterface::ICPDCOM* icp, const getSpectraSumWrapStruct& w, long spec_type, double time_low, double time_high, VARIANT * sums, VARIANT * max_vals, VARIANT * spec_nums, BSTR * messages)
{
    return icp->getSpectraSum(w.period, w.first_spec, w.num_spec, spec_type, time_low, time_high, sums, max_vals, spec_nums, messages);
}

static HRESULT getSpectraSumWrapI(const getSpectraSumWrapStruct& w, long spec_type, double time_low, double time_high, std::vector<long>& sums, std::vector<long>& max_vals, std::vector<long>& spec_nums, std::string& messages)
{
    return ISISICPINT::getSpectraSum(w.period, w.first_spec, w.num_spec, spec_type, time_low, time_high, sums, max_vals, spec_nums, messages);
}

int isisdaeInterface::getSpectraSum(long period, long first_spec, long num_spec, long spec_type, double time_low, double time_high, std::vector<long>& sums, std::vector<long>& max_vals, std::vector<long>& spec_nums)
{
	getSpectraSumWrapStruct w(period, first_spec, num_spec);
	if (m_dcom)
	{
		variant_t sums_v, max_vals_v, spec_nums_v;
		callD<int>(boost::bind(&getSpectraSumWrapD, _1, boost::cref(w), spec_type, time_low, time_high, &sums_v, &max_vals_v, &spec_nums_v, _2));
		makeArrayFromVariant(sums, &sums_v);
		makeArrayFromVariant(max_vals, &max_vals_v);
		makeArrayFromVariant(spec_nums, &spec_nums_v);
	}
	else
	{
		callI<int>(boost::bind(&getSpectraSumWrapI, boost::cref(w), spec_type, time_low, time_high, boost::ref(sums), boost::ref(max_vals), boost::ref(spec_nums), _1));
	}
    return 0;	
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
	cluster_xml.reserve(6000); // to avoid string reallocs
    int res = (m_dcom ? getXMLSettingsD(cluster_xml, "run_data_cluster.xml", &ICPDCOM::updateStatusXML2) : getXMLSettingsI(cluster_xml, "run_data_cluster.xml", &ISISICPINT::updateStatusXML2)); 
    CComPtr<IXMLDOMDocument> xmldom = createXmlDom(cluster_xml);
	if (xmldom)
	{
        extractValues(values, xmldom);
	}        
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
	{
        std::cout << "Unable to create XML parser object\n";
		return NULL;
	}
    if ( spXMLDOM.p == NULL ) 
	{
        std::cout << "Unable to create XML parser object\n";
		return NULL;
	}
    
	spXMLDOM->put_async(VARIANT_FALSE);
	spXMLDOM->put_validateOnParse(VARIANT_FALSE);
	spXMLDOM->put_resolveExternals(VARIANT_FALSE);
    spXMLDOM->loadXML(CT2OLE(xml.c_str()), &b_status);
    if (b_status!=VARIANT_TRUE) {
        std::cout << "Some sort of error\n";
		return NULL;
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
    hr = spXMLNode3->get_text(name);
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
        hr = spXMLNode3->get_text(&bstr_value);
    	spXMLNode3.Release();
	    spXMLNode2.Release();
        unsigned long index = 0;
        VarUI4FromStr(bstr_value, 0, 0, &index);
        CComPtr<IXMLDOMNodeList> spXMLNodes;
        spXMLNode->selectNodes(bstrSS3, &spXMLNodes);
        if ( spXMLNodes->get_item(index, &spXMLNode2) == S_OK )  // May have a missing <Choice> or index is invalid - shouldn't happen really
        {
            spXMLNode2->get_firstChild(&spXMLNode3);
            spXMLNode3->get_text(value);
        }
        else
        {
		    *value = SysAllocString(L"");
        }
        SysFreeString(bstr_value);
	}
	else
	{
        spXMLNode3->get_text(value);
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
        
//        printf("%S, %S%\n", bstrName, bstrValue);
		values[std::string(COLE2CT(bstrName))] = DAEValue(type, COLE2CT(bstrValue));
        
        SysFreeString(bstrName);
        SysFreeString(bstrValue);
    }
    
    return 0;
}

long isisdaeInterface::getSpectrumSize(long spectrum_number)
{
    return (m_dcom ? callD<long>(boost::bind(&ICPDCOM::getSpectrumSize, _1, spectrum_number, _2)) : callI<long>(boost::bind(&ISISICPINT::getSpectrumSize, spectrum_number, _1)));
}

// returns number of signal values, if as_histogram is true them there will be n+1 time bin boundary values rather than n 
long isisdaeInterface::getCRPTSpectrum(int spec, int period, float* time_channels, float* signal, long nvals, bool as_histogram, bool as_distribution)
{
    return getSpectrumHelper(spec, period, time_channels, signal, nvals, as_histogram, as_distribution, true);
}

// returns number of signal values, if as_histogram is true them there will be n+1 time bin boundary values rather than n 
long isisdaeInterface::getSpectrum(int spec, int period, float* time_channels, float* signal, long nvals, bool as_histogram, bool as_distribution)
{
    return getSpectrumHelper(spec, period, time_channels, signal, nvals, as_histogram, as_distribution, false);
}

// returns number of signal values, if as_histogram is true them there will be n+1 time bin boundary values rather than n 
long isisdaeInterface::getSpectrumHelper(int spec, int period, float* time_channels, float* signal, long nvals, bool as_histogram, bool as_distribution, bool use_crpt)
{
	long sum = 0, n;
	if (m_dcom)
	{
		variant_t time_channels_v, signal_v;
        if (use_crpt) {
		    callD<int>(boost::bind(&ICPDCOM::getCRPTSpectrum, _1, spec, period, &time_channels_v, &signal_v, as_histogram, as_distribution, &sum, _2));
        } else {
		    callD<int>(boost::bind(&ICPDCOM::getSpectrum, _1, spec, period, &time_channels_v, &signal_v, as_histogram, as_distribution, &sum, _2));
        }
		double *s = NULL, *t = NULL;
		accessArrayVariant(&signal_v, &s);
		accessArrayVariant(&time_channels_v, &t);
		n = arrayVariantLength(&time_channels_v);
		for(int i=0; i < std::min(nvals,n); ++i)
        {        
			time_channels[i] = static_cast<float>(t[i]);
	    }
		n = arrayVariantLength(&signal_v);
		for(int i=0; i < std::min(nvals,n); ++i)
	    {
	        signal[i] = static_cast<float>(s[i]);
        }
		unaccessArrayVariant(&signal_v);
		unaccessArrayVariant(&time_channels_v);
	}
	else
	{
		std::vector<double> time_channels_v, signal_v;
		callI<int>(boost::bind(&ISISICPINT::getSpectrum, spec, period, boost::ref(time_channels_v), boost::ref(signal_v), as_histogram, as_distribution, boost::ref(sum), _1));
		n = static_cast<long>(time_channels_v.size());
		for(int i=0; i < std::min(nvals,n); ++i)
	    {
	        time_channels[i] = static_cast<float>(time_channels_v[i]);
	    }
		n = static_cast<long>(signal_v.size());
		for(int i=0; i < std::min(nvals,n); ++i)
	    {
	        signal[i] = static_cast<float>(signal_v[i]);
	    }
	}
    return n;
}

long isisdaeInterface::getCRPTSpectrumIntegral(long spectrum_number, long period, float time_low, float time_high, long& counts)
{
	variant_t spectrum_numbers_v, times_low_v, times_high_v, counts_v;
    std::vector<long> spectrum_numbers = { spectrum_number };
    std::vector<float> times_low = { time_low };
    std::vector<float> times_high = { time_high };
    std::vector<long> counts_array;
	makeVariantFromArray(&spectrum_numbers_v, spectrum_numbers);
	makeVariantFromArray(&times_low_v, times_low);
	makeVariantFromArray(&times_high_v, times_high);
	if (m_dcom) {
		callD<int>(boost::bind(&ICPDCOM::getCRPTSpectraIntegral, _1, spectrum_numbers_v, period, times_low_v, times_high_v, &counts_v, _2));
		makeArrayFromVariant(counts_array, &counts_v);
        counts = (counts_array.size() > 0 ? counts_array[0] : 0);
    }
    return 0;
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

long isisdaeInterface::getSpectrumIntegral(std::vector<long>& spectrum_numbers, long period, std::vector<float>& times_low, std::vector<float>& times_high, std::vector<long>& counts)
{
	variant_t spectrum_numbers_v, times_low_v, times_high_v, counts_v;
	makeVariantFromArray(&spectrum_numbers_v, spectrum_numbers);
	makeVariantFromArray(&times_low_v, times_low);
	makeVariantFromArray(&times_high_v, times_high);
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::getSpectraIntegral, _1, spectrum_numbers_v, period, times_low_v, times_high_v, &counts_v, _2));
		makeArrayFromVariant(counts, &counts_v);
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::getSpectraIntegral, spectrum_numbers, period, times_low, times_high, boost::ref(counts), _1));
	}
    return 0;
}

long isisdaeInterface::QXReadArray(unsigned long card_id, unsigned long card_address, std::vector<long>& values, unsigned long num_values, unsigned long trans_type)
{
	variant_t values_v;
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::QXReadArray, _1, card_id, card_address, &values_v, num_values, trans_type, _2));
		makeArrayFromVariant(values, &values_v);
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::QXReadArray, card_id, card_address, boost::ref(values), num_values, trans_type, _1));
	}
    return 0;

}

long isisdaeInterface::QXWriteArray(unsigned long card_id, unsigned long card_address, const std::vector<long>& values, unsigned long trans_type)
{
	variant_t values_v;
	if (m_dcom)
	{
		makeVariantFromArray(&values_v, values);
		callD<int>(boost::bind(&ICPDCOM::QXWriteArray, _1, card_id, card_address, values_v, trans_type, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::QXWriteArray, card_id, card_address, boost::ref(values), trans_type, _1));
	}
    return 0;
}

long isisdaeInterface::VMEReadArray(unsigned long card_id, unsigned long card_address, std::vector<long>& values, unsigned long num_values)
{
	variant_t values_v;
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::VMEReadArray, _1, card_id, card_address, &values_v, num_values, _2));
		makeArrayFromVariant(values, &values_v);
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::VMEReadArray, card_id, card_address, boost::ref(values), num_values, _1));
	}
    return 0;

}

long isisdaeInterface::VMEWriteArray(unsigned long card_id, unsigned long card_address, const std::vector<long>& values)
{
	variant_t values_v;
	if (m_dcom)
	{
		makeVariantFromArray(&values_v, values);
		callD<int>(boost::bind(&ICPDCOM::VMEWriteArray, _1, card_id, card_address, values_v, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::VMEWriteArray, card_id, card_address, boost::ref(values), _1));
	}
    return 0;
}

long isisdaeInterface::VMEReadValue(unsigned long card_id, unsigned long card_address, unsigned long word_size, unsigned long& value)
{
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::VMEReadValue, _1, card_id, card_address, word_size, &value, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::VMEReadValue, card_id, card_address, word_size, &value, _1));
	}
    return 0;

}

long isisdaeInterface::VMEWriteValue(unsigned long card_id, unsigned long card_address, unsigned long word_size, unsigned long value, unsigned long mode)
{
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::VMEWriteValue, _1, card_id, card_address, word_size, value, mode, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::VMEWriteValue, card_id, card_address, word_size, value, mode, _1));
	}
    return 0;
}

int isisdaeInterface::updateCRPTSpectra(long period, long spec_start, long nspectra)
{
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::updateCRPTSpectra, _1, period, spec_start, nspectra, _2));
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::updateCRPTSpectra, period, spec_start, nspectra, _1));
	}
    return 0;	
}

long isisdaeInterface::getSpectrumIntegral2(long spec_start, long nspectra, long period, float time_low, float time_high, std::vector<long>& counts)
{
	variant_t counts_v;
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::getSpectraIntegral2, _1, spec_start, nspectra, period, time_low, time_high, &counts_v, _2));
		makeArrayFromVariant(counts, &counts_v);
	}
	else
	{
		callI<int>(boost::bind(&ISISICPINT::getSpectraIntegral2, spec_start, nspectra, period, time_low, time_high, boost::ref(counts), _1));
	}
    return 0;
}

int isisdaeInterface::getEventSpecIntegralsSize() const
{
	return ISISCRPT_MAX_SPEC_INTEGRALS;
}

void isisdaeInterface::getVetoInfo(std::vector<std::string>& names, std::vector<std::string>& alias, std::vector<long>& enabled, std::vector<long>& frames)
{
	variant_t names_v, alias_v, enabled_v, frames_v;
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::getVetoInfo, _1, &names_v, &alias_v, &enabled_v, &frames_v, _2));
		makeArrayFromVariant(names, &names_v);
		makeArrayFromVariant(alias, &alias_v);
		makeArrayFromVariant(enabled, &enabled_v);
		makeArrayFromVariant(frames, &frames_v);
	}
	else
	{
//		callI<int>(boost::bind(&ISISICPINT::getVetoInfo, boost::ref(names), boost::ref(enabled), boost::ref(frames), _1));  // need to wait for new isisicpint.lib
	}
}

void isisdaeInterface::setSpecIntgCutoff(double tmin, double tmax)
{
	if (m_dcom)
	{
		callD<int>(boost::bind(&ICPDCOM::setSpecIntgCutoff, _1, static_cast<float>(tmin), static_cast<float>(tmax), _2));
	}
	else
	{
//		callI<int>(boost::bind(&ISISICPINT::setSpecIntgCutoff, tmin, tmax, _1)); // need to wait for new isisicpint.lib
	}
}

long isisdaeInterface::getSpectrumNumberForMonitor(long mon_num)
{
	if (m_dcom)
	{
		return callD<long>(boost::bind(&ICPDCOM::getSpectrumNumberForMonitor, _1, mon_num, _2));
	}
	else
	{
        return 0;
//		callI<long>(boost::bind(&ISISICPINT::getSpectrumNumberForMonitor, mon_num, _1)); // need to wait for new isisicpint.lib
	}
}
