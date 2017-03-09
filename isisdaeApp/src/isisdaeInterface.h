#ifndef ISISDAE_INTERFACE_H
#define ISISDAE_INTERFACE_H

/// \file lvDCOMInterface.h header for #lvDCOMInterface class.

#include <stdio.h>

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>
//#include <sys/stat.h>
//#include <process.h>
//#include <fcntl.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
#include <atlbase.h>
#include <atlstr.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctl.h>
#include <atlhost.h>

#include <string>
#include <vector>
#include <map>
#include <list>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsExit.h>

//#import "isisicp.tlb" named_guids
// The above statement would generate isisicp.tlh and isisicp.tli but we include pre-built versions here
#include "isisicp.tlh"

/// Options that can be passed from EPICS iocsh via #lvDCOMConfigure command.
/// In the iocBoot st.cmd file you will need to add the relevant integer enum values together and pass this single integer value.
enum isisdaeOptions
{
    daeDCOM = 1 				///< If the LabVIEW VI is idle when we connect to it, issue a warning message  
};	

class DAEValue
{
public:
    enum DAEType { DAEUnknown = 0, DAEDouble = 1, DAEInt = 2, DAEUInt = 3, DAEString = 4, DAEEnum = 5 };
private:
	DAEType m_type;
    std::string m_value;
public:
	DAEValue(DAEType type, const char* value) : m_type(type), m_value(value) { }
	DAEValue() : m_type(DAEUnknown), m_value("") { }
	// todo: check m_type is compatible for cast
	operator const char*() const { return m_value.c_str(); }
	operator int() const { return atoi(m_value.c_str()); }
	operator unsigned() const { return atoi(m_value.c_str()); }
	operator double() const { return atof(m_value.c_str()); }
};

class isisdaeInterface
{
public:
	isisdaeInterface(const char* host, int options, const char* username, const char* password);
	long nParams();
	void getParams(std::map<std::string,std::string>& res);
	virtual ~isisdaeInterface() { }
	unsigned long getGoodFrames();
    unsigned long getGoodFramesPeriod();
	unsigned long getRawFrames();
	double getGoodUAH();
    double getGoodUAHPeriod();
	int beginRun();
    int beginRunEx(long options, long period);
	int abortRun();
    int pauseRun();
    int resumeRun();
    int endRun();
    int recoverRun();
    int saveRun();
    int updateRun();
    int storeRun();
    int snapshotCRPT(const std::string& filename, long do_update, long do_pause);
    int startSEWait();
    int endSEWait();
	int getRunState();
    long getRunNumber();
    int setPeriod(long period);
    int setNumPeriods(long nperiods);
    int setRunTitle(const std::string& title);
    int setUserParameters(long rbno, const std::string& name, const std::string& institute, const std::string& role);
    int setSampleParameter(const std::string& name, const std::string& type, const std::string& units, const std::string& value);
    int setBeamlineParameter(const std::string& name, const std::string& type, const std::string& units, const std::string& value);
    int getPeriod();
    long getNumPeriods();
	long getNumTimeChannels(int spec);
	long getSpectrum(int spec, int period, float* time_channels, float* signal, long nvals);
    long getSpectrumIntegral(long spectrum_number, long period, float time_low, float time_high, long& counts);
    double getMEvents();
    unsigned long getTotalCounts();
    unsigned long getHistogramMemory();
    int getSpectraSum(long period, long first_spec, long num_spec, long spec_type, double time_low, double time_high,                       std::vector<long>& sums, std::vector<long>& max_vals, std::vector<long>& spec_nums);
    int getRunDataFromDAE(std::map<std::string, DAEValue>& values);
    int getDAESettingsXML(std::string& result);
    int setDAESettingsXML(const std::string& settings);
    int getTCBSettingsXML(std::string& result);
    int setTCBSettingsXML(const std::string& settings);
    int getHardwarePeriodsSettingsXML(std::string& result);
    int setHardwarePeriodsSettingsXML(const std::string& settings);
    int getUpdateSettingsXML(std::string& result);
    int setUpdateSettingsXML(const std::string& settings);
    int getMonitoringSettingsXML(std::string& result);
    int setMonitoringSettingsXML(const std::string& settings);
    int getVetoStatus(std::string& result);
	const std::string& getAllMessages() const;
	int getAsyncMessages(std::list<std::string>& messages);
	void resetMessages(); 
	static void stripTimeStamp(const std::string& in, std::string& out);
	
private:
	std::string m_host;
	std::string m_progid;
	std::string m_allMsgs;
	CLSID m_clsid;
	std::string m_username;
	std::string m_password;
	int m_options; ///< the various #lvDCOMOptions currently in use
	epicsMutex m_lock;
	bool m_dcom;
	typedef isisicpLib::Idae ICPDCOM;
	CComPtr<ICPDCOM> m_icp;
	COAUTHIDENTITY* m_pidentity;

	COAUTHIDENTITY* createIdentity(const std::string& user, const std::string& domain, const std::string& pass);
	HRESULT setIdentity(COAUTHIDENTITY* pidentity, IUnknown* pUnk);
	static void epicsExitFunc(void* arg);
	bool checkOption(isisdaeOptions option) { return ( m_options & static_cast<int>(option) ) != 0; }
	void checkConnection();
    int extractValues(std::map<std::string, DAEValue>& values, CComPtr<IXMLDOMDocument> spXMLDOM);
    int extractValues(const char* name, DAEValue::DAEType type, std::map<std::string, DAEValue>& values, CComPtr<IXMLDOMDocument> spXMLDOM);
    int loadFromFile(const std::string& filename, std::string& xml);
    CComPtr<IXMLDOMDocument> createXmlDom(const std::string& xml);
    void getNameAndValue(CComPtr<IXMLDOMNode> spXMLNode, BSTR* name, BSTR* value, DAEValue::DAEType);
	std::string getValue(const std::string& name);
	// call ISISICPINT functions
	template <typename T> T callI( boost::function<T(std::string&)> func );
	template <typename T> T callItr1( std::tr1::function<T(std::string&)> func );

	// call ICPDCOM functions
	template <typename T> T callD( boost::function<T(ICPDCOM*, BSTR*)> func );
	template <typename T> T callDtr1( std::tr1::function<T(ICPDCOM*, BSTR*)> func );
  
	int getXMLSettingsI(std::string& result, const std::string& template_file, int (*func)(const std::string&, std::string&, std::string&));
    int getXMLSettingsD(std::string& result, const std::string& template_file, HRESULT (ICPDCOM::*func)(_bstr_t, BSTR*, BSTR*));
};

#endif /* ISISDAE_INTERFACE_H */
