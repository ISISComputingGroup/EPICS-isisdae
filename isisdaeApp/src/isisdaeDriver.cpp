#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <iostream>
#include <map>
#include <iomanip>

#include <boost/algorithm/string.hpp>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>

#include <macLib.h>
#include <epicsGuard.h>

#include "fdManager.h"
#include "exServer.h"
#include "envDefs.h"
#include "errlog.h"

#include "utilities.h"

#include "isisdaeDriver.h"
#include "isisdaeInterface.h"
#include "convertToString.h"

#include <epicsExport.h>

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

static const char *driverName="isisdaeDriver";

void isisdaeDriver::reportErrors(const char* exc_text)
{
		std::string msgs = m_iface->getAllMessages();
        setStringParam(P_AllMsgs, msgs.c_str());
        setStringParam(P_ErrMsgs, exc_text);
		std::string mess_ts;
		isisdaeInterface::stripTimeStamp(exc_text, mess_ts);
//		std::cerr << mess_ts << std::endl;		  
		errlogSevPrintf(errlogMajor, "%s", mess_ts.c_str());
// getAsynMessages will pick these up and report to screen
//		errlogSevPrintf(errlogInfo, "%s", msgs.c_str());
		m_iface->resetMessages();
}

void isisdaeDriver::reportMessages()
{
	std::string msgs = m_iface->getAllMessages();
    setStringParam(P_AllMsgs, msgs.c_str());
    setStringParam(P_ErrMsgs, "");
// getAsynMessages will pick these up and report to screen
//		errlogSevPrintf(errlogInfo, "%s", msgs.c_str());
	m_iface->resetMessages();
}

void isisdaeDriver::beginStateTransition(int state)
{
    // signal in state transition separately and before signalling the state, might help order monitors get sent in?  
    setIntegerParam(P_StateTrans, 1);
	callParamCallbacks();
    m_RunStatus = state;
	setIntegerParam(P_RunStatus, m_RunStatus);
	callParamCallbacks();
}

void isisdaeDriver::endStateTransition()
{
	updateRunStatus();
    setIntegerParam(P_StateTrans, 0);
	callParamCallbacks();
}

template<typename T>
asynStatus isisdaeDriver::writeValue(asynUser *pasynUser, const char* functionName, T value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	getParamName(function, &paramName);
	m_iface->resetMessages();
	try
	{
		if (function == P_BeginRun)
		{
		    beginStateTransition(RS_BEGINNING);
			m_iface->beginRun();
//            zeroRunCounters();   // shouldn't be necessary as call updateRunStatus()
		}
        else if (function == P_BeginRunEx)
		{
		    beginStateTransition(RS_BEGINNING);
			m_iface->beginRunEx(value, -1);
//            zeroRunCounters();   // shouldn't be necessary as call updateRunStatus()
		}
		else if (function == P_AbortRun)
		{
		    beginStateTransition(RS_ABORTING);
			m_iface->abortRun();
		}
        else if (function == P_PauseRun)
		{
		    beginStateTransition(RS_PAUSING);
			m_iface->pauseRun();
		}
        else if (function == P_ResumeRun)
		{
		    beginStateTransition(RS_RESUMING);
			m_iface->resumeRun();
		}
        else if (function == P_EndRun)
		{
		    beginStateTransition(RS_ENDING);
			m_iface->endRun();
		}
        else if (function == P_RecoverRun)
		{
			m_iface->recoverRun();
		}
        else if (function == P_SaveRun)
		{
		    beginStateTransition(RS_SAVING);
			m_iface->saveRun();
		}
        else if (function == P_UpdateRun)
		{
		    beginStateTransition(RS_UPDATING);
			m_iface->updateRun();
		}
        else if (function == P_StoreRun)
		{
		    beginStateTransition(RS_STORING);
			m_iface->storeRun();
		}
        else if (function == P_StartSEWait)
		{
			m_iface->startSEWait();
		}
        else if (function == P_EndSEWait)
		{
			m_iface->endSEWait();
		}
        else if (function == P_Period)
		{
			m_iface->setPeriod(value);
		}
        else if (function == P_NumPeriods)
		{
			m_iface->setNumPeriods(value);
		}
		endStateTransition();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, convertToString(value).c_str());
		reportMessages();
		return asynSuccess;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s, error=%s", 
                  driverName, functionName, status, function, paramName, convertToString(value).c_str(), ex.what());
		reportErrors(ex.what());
		endStateTransition();
		return asynError;
	}
}

template<typename T>
asynStatus isisdaeDriver::readValue(asynUser *pasynUser, const char* functionName, T* value)
{
	int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	getParamName(function, &paramName);
	m_iface->resetMessages();
	try
	{
		status = asynPortDriver::readValue(pasynUser, functionName, &value);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, convertToString(*value).c_str());
		reportMessages();
		return status;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s, error=%s", 
                  driverName, functionName, status, function, paramName, convertToString(*value).c_str(), ex.what());
		reportErrors(ex.what());
		return asynError;
	}
}

template<typename T>
asynStatus isisdaeDriver::readArray(asynUser *pasynUser, const char* functionName, T *value, size_t nElements, size_t *nIn)
{
  int function = pasynUser->reason;
  asynStatus status = asynSuccess;
  const char *paramName = NULL;
	getParamName(function, &paramName);
	m_iface->resetMessages();
	try
	{
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s\n", 
              driverName, functionName, function, paramName);
		reportMessages();
		return status;
	}
	catch(const std::exception& ex)
	{
		*nIn = 0;
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, error=%s", 
                  driverName, functionName, status, function, paramName, ex.what());
		reportErrors(ex.what());
		return asynError;
	}
}

asynStatus isisdaeDriver::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
	asynStatus stat = writeValue(pasynUser, "writeFloat64", value);
    if (stat == asynSuccess)
    {
        stat = asynPortDriver::writeFloat64(pasynUser, value);
    }
	else
	{
	    callParamCallbacks(); // this flushes P_ErrMsgs
	}
    return stat;
}

asynStatus isisdaeDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    asynStatus stat = writeValue(pasynUser, "writeInt32", value);
    if (stat == asynSuccess)
    {
        stat = asynPortDriver::writeInt32(pasynUser, value);
    }
	else
	{
	    callParamCallbacks(); // this flushes P_ErrMsgs
	}
    return stat;
}

asynStatus isisdaeDriver::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn)
{
    asynStatus stat = readArray(pasynUser, "readFloat64Array", value, nElements, nIn);
	callParamCallbacks(); // this flushes P_ErrMsgs
	doCallbacksFloat64Array(value, *nIn, pasynUser->reason, 0);
    return stat;
}

asynStatus isisdaeDriver::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    asynStatus stat = readArray(pasynUser, "readInt32Array", value, nElements, nIn);
	callParamCallbacks(); // this flushes P_ErrMsgs
	doCallbacksInt32Array(value, *nIn, pasynUser->reason, 0);
    return stat;
}

//asynStatus isisdaeDriver::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
//{
//	return readValue(pasynUser, "readFloat64", value);
//}

//asynStatus isisdaeDriver::readInt32(asynUser *pasynUser, epicsInt32 *value)
//{
//	return readValue(pasynUser, "readInt32", value);
//}

asynStatus isisdaeDriver::readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason)
{
	int function = pasynUser->reason;
	int status=0;
	const char *functionName = "readOctet";
    const char *paramName = NULL;
	getParamName(function, &paramName);
	m_iface->resetMessages();
	// we don't do much yet
	return asynPortDriver::readOctet(pasynUser, value, maxChars, nActual, eomReason);

	std::string value_s;
	try
	{
		if (m_iface == NULL)
		{
			throw std::runtime_error("m_iface is NULL");
		}
//		m_iface->getLabviewValue(paramName, &value_s);
		if ( value_s.size() > maxChars ) // did we read more than we have space for?
		{
			*nActual = maxChars;
			if (eomReason) { *eomReason = ASYN_EOM_CNT | ASYN_EOM_END; }
			asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=\"%s\" (TRUNCATED from %d chars)\n", 
			  driverName, functionName, function, paramName, value_s.substr(0,*nActual).c_str(), value_s.size());
		}
		else
		{
			*nActual = value_s.size();
			if (eomReason) { *eomReason = ASYN_EOM_END; }
			asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=\"%s\"\n", 
			  driverName, functionName, function, paramName, value_s.c_str());
		}
		strncpy(value, value_s.c_str(), maxChars); // maxChars  will NULL pad if possible, change to  *nActual  if we do not want this
        setStringParam(P_AllMsgs, m_iface->getAllMessages().c_str());
        setStringParam(P_ErrMsgs, "");
		m_iface->resetMessages();
		callParamCallbacks(); // this flushes P_ErrMsgs
		return asynSuccess;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=\"%s\", error=%s", 
                  driverName, functionName, status, function, paramName, value_s.c_str(), ex.what());
		reportErrors(ex.what());
		callParamCallbacks(); // this flushes P_ErrMsgs
		*nActual = 0;
		if (eomReason) { *eomReason = ASYN_EOM_END; }
		value[0] = '\0';
		return asynError;
	}
}

// convert an EPICS waveform data type into what beamline/sample parameters would expect
void isisdaeDriver::translateBeamlineType(std::string& str)
{
	if (str == "CHAR" || str == "UCHAR" || str == "ENUM" || str == "STRING")
	{
		str = "String";
	}
	else if (str == "FLOAT" || str == "DOUBLE")
	{
		str = "Real";
	}
	else if (str == "SHORT" || str == "USHORT" || str == "LONG" || str == "ULONG")
	{
		str = "Integer";
	}
}

asynStatus isisdaeDriver::writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	getParamName(function, &paramName);
    const char* functionName = "writeOctet";
	std::string value_s;
    // we might get an embedded NULL from channel access char waveform records
    if ( (maxChars > 0) && (value[maxChars-1] == '\0') )
    {
        value_s.assign(value, maxChars-1);
    }
    else
    {
        value_s.assign(value, maxChars);
    }
	m_iface->resetMessages();
	try
	{
        if (function == P_RunTitle)
        {
			m_iface->setRunTitle(value_s);
        }
        else if (function == P_SamplePar)
        {
            std::vector<std::string> tokens;
            boost::split(tokens, value_s, boost::is_any_of("\2")); //  name, type, units, value
            if (tokens.size() == 4)
            {
				translateBeamlineType(tokens[1]);
                m_iface->setSampleParameter(tokens[0], tokens[1], tokens[2], tokens[3]);
            }
            else
            {
                throw std::runtime_error("SampleParameter: not enough tokens");
            }
        }
        else if (function == P_BeamlinePar)
        {
            std::vector<std::string> tokens;
            boost::split(tokens, value_s, boost::is_any_of("\2")); //  name, type, units, value
            if (tokens.size() == 4)
            {
				translateBeamlineType(tokens[1]);
                m_iface->setBeamlineParameter(tokens[0], tokens[1], tokens[2], tokens[3]);
            }
            else
            {
                throw std::runtime_error("BeamlineParameter: not enough tokens");
            }
        }
        else if (function == P_RBNumber)
        {
            char user[256];
            getStringParam(P_UserName, sizeof(user), user);
            m_iface->setUserParameters(atol(value_s.c_str()), user, "", "");
        }
        else if (function == P_UserName)
        {
            char rbno[16];
            getStringParam(P_RBNumber, sizeof(rbno), rbno);
            m_iface->setUserParameters(atol(rbno), value_s, "", "");
        }
        else if (function == P_DAESettings)
		{
			m_iface->setDAESettingsXML(value_s);
		}
        else if (function == P_HardwarePeriodsSettings)
		{
			m_iface->setHardwarePeriodsSettingsXML(value_s);
		}
        else if (function == P_UpdateSettings)
		{
			m_iface->setUpdateSettingsXML(value_s);
		}
        else if (function == P_TCBSettings)
		{
		    std::string tcb_xml;
		    if (uncompressString(value_s, tcb_xml) == 0)
			{
                unsigned found = tcb_xml.find_last_of(">");  // in cased junk on end
                m_iface->setTCBSettingsXML(tcb_xml.substr(0,found+1));
			}
		}
		reportMessages();
		status = asynPortDriver::writeOctet(pasynUser, value_s.c_str(), value_s.size(), nActual);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, value_s.c_str());
        if (status == asynSuccess)
        {
		    *nActual = maxChars;   // to keep result happy in case we skipped an embedded trailing NULL
        }
		return status;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s, error=%s", 
                  driverName, functionName, status, function, paramName, value_s.c_str(), ex.what());
		reportErrors(ex.what());
		callParamCallbacks(); // this flushes P_ErrMsgs
		*nActual = 0;
		return asynError;
	}
}


/// Constructor for the isisdaeDriver class.
/// Calls constructor for the asynPortDriver base class.
/// \param[in] dcomint DCOM interface pointer created by lvDCOMConfigure()
/// \param[in] portName @copydoc initArg0
isisdaeDriver::isisdaeDriver(isisdaeInterface* iface, const char *portName) 
   : asynPortDriver(portName, 
                    0, /* maxAddr */ 
                    NUM_ISISDAE_PARAMS,
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask,  /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver can block but it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0),	/* Default stack size*/
					m_iface(iface), m_RunStatus(0), m_vetopc(0.0)
{
    int i;
    const char *functionName = "isisdaeDriver";
//	epicsThreadOnce(&onceId, initCOM, NULL);

	std::map<std::string,std::string> res;
	m_iface->getParams(res);
	for(std::map<std::string,std::string>::const_iterator it=res.begin(); it != res.end(); ++it)
	{
		if (it->second == "float64")
		{
            createParam(it->first.c_str(), asynParamFloat64, &i);
		}
		else if (it->second == "int32")
		{
            createParam(it->first.c_str(), asynParamInt32, &i);
		}
		else if (it->second == "string")
		{
            createParam(it->first.c_str(), asynParamOctet, &i);
		}
		else if (it->second == "float64array")
		{
            createParam(it->first.c_str(), asynParamFloat64Array, &i);
		}
		else if (it->second == "int32array")
		{
            createParam(it->first.c_str(), asynParamInt32Array, &i);
		}
		else
		{
			std::cerr << driverName << ":" << functionName << ": unknown type " << it->second << " for parameter " << it->first << std::endl;
		}
	}
	createParam(P_GoodUAHString, asynParamFloat64, &P_GoodUAH);
    createParam(P_GoodUAHPeriodString, asynParamFloat64, &P_GoodUAHPeriod);
	createParam(P_BeginRunString, asynParamInt32, &P_BeginRun);
    createParam(P_BeginRunExString, asynParamInt32, &P_BeginRunEx);
	createParam(P_AbortRunString, asynParamInt32, &P_AbortRun);
    createParam(P_PauseRunString, asynParamInt32, &P_PauseRun);
    createParam(P_ResumeRunString, asynParamInt32, &P_ResumeRun);
    createParam(P_EndRunString, asynParamInt32, &P_EndRun);
    createParam(P_RecoverRunString, asynParamInt32, &P_RecoverRun);
    createParam(P_SaveRunString, asynParamInt32, &P_SaveRun);
    createParam(P_UpdateRunString, asynParamInt32, &P_UpdateRun);
    createParam(P_StoreRunString, asynParamInt32, &P_StoreRun);
    createParam(P_StartSEWaitString, asynParamInt32, &P_StartSEWait);
    createParam(P_EndSEWaitString, asynParamInt32, &P_EndSEWait);
	createParam(P_RunStatusString, asynParamInt32, &P_RunStatus);
    createParam(P_TotalCountsString, asynParamInt32, &P_TotalCounts);
    
    createParam(P_RunTitleString, asynParamOctet, &P_RunTitle);
    createParam(P_AllMsgsString, asynParamOctet, &P_AllMsgs);
    createParam(P_ErrMsgsString, asynParamOctet, &P_ErrMsgs);
    createParam(P_RBNumberString, asynParamOctet, &P_RBNumber);
    createParam(P_RunNumberString, asynParamOctet, &P_RunNumber);
    createParam(P_UserNameString, asynParamOctet, &P_UserName);
    createParam(P_InstNameString, asynParamOctet, &P_InstName);
    createParam(P_UserTelephoneString, asynParamOctet, &P_UserTelephone);
    createParam(P_StartTimeString, asynParamOctet, &P_StartTime);
    createParam(P_NPRatioString, asynParamFloat64, &P_NPRatio);
    createParam(P_ISISCycleString, asynParamOctet, &P_ISISCycle);
    createParam(P_DAETimingSourceString, asynParamOctet, &P_DAETimingSource);
    createParam(P_PeriodTypeString, asynParamOctet, &P_PeriodType);
    
    createParam(P_DAESettingsString, asynParamOctet, &P_DAESettings);
    createParam(P_TCBSettingsString, asynParamOctet, &P_TCBSettings);
    createParam(P_HardwarePeriodsSettingsString, asynParamOctet, &P_HardwarePeriodsSettings);
    createParam(P_UpdateSettingsString, asynParamOctet, &P_UpdateSettings);
    createParam(P_VetoStatusString, asynParamOctet, &P_VetoStatus);
    createParam(P_VetoPCString, asynParamFloat64, &P_VetoPC);
    
    createParam(P_GoodFramesTotalString, asynParamInt32, &P_GoodFramesTotal);
    createParam(P_GoodFramesPeriodString, asynParamInt32, &P_GoodFramesPeriod);
    createParam(P_RawFramesTotalString, asynParamInt32, &P_RawFramesTotal);
    createParam(P_RawFramesPeriodString, asynParamInt32, &P_RawFramesPeriod);
    
    createParam(P_RunDurationTotalString, asynParamInt32, &P_RunDurationTotal);
    createParam(P_RunDurationPeriodString, asynParamInt32, &P_RunDurationPeriod);
    createParam(P_NumTimeChannelsString, asynParamInt32, &P_NumTimeChannels);
    createParam(P_NumPeriodsString, asynParamInt32, &P_NumPeriods);
    createParam(P_DAEMemoryUsedString, asynParamInt32, &P_DAEMemoryUsed);
    
    createParam(P_PeriodString, asynParamInt32, &P_Period);
    createParam(P_NumSpectraString, asynParamInt32, &P_NumSpectra);
    createParam(P_MonitorCountsString, asynParamInt32, &P_MonitorCounts);
    createParam(P_PeriodSequenceString, asynParamInt32, &P_PeriodSequence);
    createParam(P_MonitorSpectrumString, asynParamInt32, &P_MonitorSpectrum);
    
    createParam(P_BeamCurrentString, asynParamFloat64, &P_BeamCurrent);
    createParam(P_TotalUAmpsString, asynParamFloat64, &P_TotalUAmps);
    createParam(P_MonitorFromString, asynParamFloat64, &P_MonitorFrom);
    createParam(P_MonitorToString, asynParamFloat64, &P_MonitorTo);
    createParam(P_TotalDaeCountsString, asynParamFloat64, &P_TotalDaeCounts);
    createParam(P_CountRateString, asynParamFloat64, &P_CountRate);
    createParam(P_EventModeFractionString, asynParamFloat64, &P_EventModeFraction);

    createParam(P_StateTransString, asynParamInt32, &P_StateTrans);
	
    createParam(P_SampleParString, asynParamOctet, &P_SamplePar);
    createParam(P_BeamlineParString, asynParamOctet, &P_BeamlinePar);

    setIntegerParam(P_StateTrans, 0);

    // Create the thread for background tasks (not used at present, could be used for I/O intr scanning) 
    if (epicsThreadCreate("isisdaePoller1",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)pollerThreadC1, this) == 0)
    {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }
    if (epicsThreadCreate("isisdaePoller2",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)pollerThreadC2, this) == 0)
    {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }
}

void isisdaeDriver::pollerThreadC1(void* arg)
{ 
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	driver->pollerThread1();
}

void isisdaeDriver::pollerThreadC2(void* arg)
{ 
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	driver->pollerThread2();
}

void isisdaeDriver::pollerThread1()
{
    static const char* functionName = "isisdaePoller1";
    unsigned long counter = 0;
    double delay = 0.2;    
	while(true)
	{
		lock();
        try
        {
            updateRunStatus();
        }
        catch(const std::exception& ex)
        {
            std::cerr << ex.what() << std::endl;
        }
		callParamCallbacks();        
		unlock();
        ++counter;
		epicsThreadSleep(delay);
    }
}

void isisdaeDriver::updateRunStatus()
{
        int rs = m_iface->getRunState();
        if (rs == RS_RUNNING && m_vetopc > 50.0)
        {
            m_RunStatus = RS_VETOING;
        }
        else
        {
            m_RunStatus = rs;
        }
		setIntegerParam(P_RunStatus, m_RunStatus);
		setDoubleParam(P_GoodUAH, m_iface->getGoodUAH());
        setDoubleParam(P_GoodUAHPeriod, m_iface->getGoodUAHPeriod());
        setIntegerParam(P_TotalCounts, m_iface->getTotalCounts());
        setIntegerParam(P_GoodFramesTotal, m_iface->getGoodFrames());
        setIntegerParam(P_GoodFramesPeriod, m_iface->getGoodFramesPeriod());
		setIntegerParam(P_RawFramesTotal, m_iface->getRawFrames());
        ///@todo need to update P_RawFramesPeriod, P_RunDurationTotal, P_TotalUAmps, P_RunDurationPeriod,P_TotalDaeCounts, P_MonitorCounts
}

// should not be needed as call updateRunStatus() at appropriate point
void isisdaeDriver::zeroRunCounters()
{
		setDoubleParam(P_GoodUAH, 0.0);
        setDoubleParam(P_GoodUAHPeriod, 0.0);
        setDoubleParam(P_TotalUAmps, 0.0);
        setIntegerParam(P_TotalCounts, 0);
        setIntegerParam(P_GoodFramesTotal, 0);
        setIntegerParam(P_GoodFramesPeriod, 0);
		setIntegerParam(P_RawFramesTotal, 0);
        setIntegerParam(P_RawFramesPeriod, 0);  
        setIntegerParam(P_RunDurationTotal, 0);
        setIntegerParam(P_RunDurationPeriod, 0);
        setIntegerParam(P_MonitorCounts, 0);
        setDoubleParam(P_TotalDaeCounts, 0.0);
        setDoubleParam(P_CountRate, 0.0);
}

void isisdaeDriver::pollerThread2()
{
    static const char* functionName = "isisdaePoller2";
	std::map<std::string, DAEValue> values;
    unsigned long counter = 0;
    double delay = 2.0;  
    long this_rf = 0, this_gf = 0, last_rf = 0, last_gf = 0;
    
	while(true)
	{
        bool check_settings = ( (counter == 0) || (m_RunStatus == RS_SETUP && counter % 2 == 0) || (counter % 10 == 0) );
        std::string daeSettings;
        std::string tcbSettings, tcbSettingComp;
        std::string hardwarePeriodsSettings;
        std::string updateSettings;
        std::string vetoStatus;
		epicsThreadSleep(delay);
        try
        {
            m_iface->getRunDataFromDAE(values);
            m_iface->getVetoStatus(vetoStatus);
            if (check_settings)
            {
                m_iface->getDAESettingsXML(daeSettings);
                m_iface->getTCBSettingsXML(tcbSettings);
                m_iface->getHardwarePeriodsSettingsXML(hardwarePeriodsSettings);
                m_iface->getUpdateSettingsXML(updateSettings);
            }
            this_gf = m_iface->getGoodFrames();        
		    this_rf = m_iface->getRawFrames();
        }
        catch(const std::exception& ex)
        {
            std::cerr << ex.what() << std::endl;
            continue;
        }
        if (this_rf > last_rf)
        {
            m_vetopc = 100.0 * (1.0 - static_cast<double>(this_gf - last_gf) / static_cast<double>(this_rf - last_rf));
        }
        else
        {
            m_vetopc = 0.0;
        }
        last_rf = this_rf;
        last_gf = this_gf;
		lock();
        setStringParam(P_RunTitle, values["RunTitle"]); 
        setStringParam(P_RBNumber, values["RBNumber"]); 
        setStringParam(P_RunNumber, values["RunNumber"]);
        setStringParam(P_InstName, values["InstName"]);
        setStringParam(P_UserName, values["UserName"]);
        setStringParam(P_UserTelephone, values["UserTelephone"]);
        setStringParam(P_StartTime, values["StartTime"]);
        setDoubleParam(P_NPRatio, values["N/P Ratio"]);
        setStringParam(P_ISISCycle, values["ISISCycle"]);
        setStringParam(P_DAETimingSource, values["DAETimingSource"]);
        setStringParam(P_PeriodType, values["Period Type"]);
        
        setIntegerParam(P_RawFramesPeriod, values["RawFramesPeriod"]);
        
        setIntegerParam(P_RunDurationTotal, values["RunDurationTotal"]);
        setIntegerParam(P_RunDurationPeriod, values["RunDurationPeriod"]);
        setIntegerParam(P_NumTimeChannels, values["NumberOfTimeChannels"]);
        setIntegerParam(P_NumPeriods, values["NumberOfPeriods"]);
        setIntegerParam(P_DAEMemoryUsed, values["DAEMemoryUsed"]);
        
        setIntegerParam(P_Period, values["CurrentPeriod"]);
        setIntegerParam(P_NumSpectra, values["NumberOfSpectra"]);
        setIntegerParam(P_MonitorCounts, values["MonitorCounts"]);
        setIntegerParam(P_PeriodSequence, values["PeriodSequence"]);
        setIntegerParam(P_MonitorSpectrum, values["MonitorSpectrum"]);
        
        setDoubleParam(P_BeamCurrent, values["BeamCurrent"]);
        setDoubleParam(P_TotalUAmps, values["TotalUAmps"]);
        setDoubleParam(P_MonitorFrom, values["MonitorFrom"]);
        setDoubleParam(P_MonitorTo, values["MonitorTo"]);
        setDoubleParam(P_TotalDaeCounts, values["TotalDAECounts"]);
        setDoubleParam(P_CountRate, values["CountRate"]);
        setDoubleParam(P_EventModeFraction, values["EventModeCardFraction"]);
        setDoubleParam(P_VetoPC, m_vetopc);
        
        setStringParam(P_VetoStatus, vetoStatus.c_str() );
        if (check_settings) 
        {
            setStringParam(P_DAESettings, daeSettings.c_str());
		    if (compressString(tcbSettings, tcbSettingComp) == 0)
		    {
                setStringParam(P_TCBSettings, tcbSettingComp.c_str());
		    }
            setStringParam(P_HardwarePeriodsSettings, hardwarePeriodsSettings.c_str() );
            setStringParam(P_UpdateSettings, updateSettings.c_str() );
        }          
		std::list<std::string> messages;
		std::string all_msgs;
		m_iface->getAsyncMessages(messages);
		for(std::list<std::string>::const_iterator it=messages.begin(); it != messages.end(); ++it)
		{
			std::string mess_ts;
			isisdaeInterface::stripTimeStamp(it->c_str(), mess_ts);
		    all_msgs.append(mess_ts);
			errlogSevPrintf(errlogInfo, "%s", mess_ts.c_str());
		}
		if (all_msgs.size() > 0)
		{
            setStringParam(P_AllMsgs, all_msgs.c_str());
		}
		messages.clear();
		callParamCallbacks();        
		unlock();
        ++counter;
	}
}	

static void daeCASThread(void* arg)
{
    exServer    *pCAS;
    unsigned    debugLevel = 0u;
    double      executionTime = 0.0;
    double      asyncDelay = 0.1;
    const char*        pvPrefix;
    unsigned    aliasCount = 1u;
    unsigned    scanOn = true;
    unsigned    syncScan = true;
    unsigned    maxSimultAsyncIO = 1000u;
	isisdaeInterface* iface = static_cast<isisdaeInterface*>(arg);
    printf("starting cas server\n");
	pvPrefix = macEnvExpand("$(MYPVPREFIX)DAE:");
    try {
        pCAS = new exServer ( pvPrefix, aliasCount, 
            scanOn != 0, syncScan == 0, asyncDelay,
            maxSimultAsyncIO, iface );
    }
    catch ( ... ) {
        errlogSevPrintf (errlogMajor, "Server initialization error\n" );
        errlogFlush ();
        return;
    }
    
    pCAS->setDebugLevel(debugLevel);
    try
    {
        while (true) 
        {
            fileDescriptorManager.process(1000.0);
        }
    }
    catch(const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    //pCAS->show(2u);
    delete pCAS;
    errlogFlush ();
}

//struct sockaddr_in ipa;
//ipa.sin_family = AF_INET;
//status = aToIPAddr ("127.0.0.1", 0u, &ipa);
//ipa.sin_port = 0u; // use the default CA server port
//        pPVI = new pvInfo ( pvNameStr, ipa );
//        if ( ! pPVI ) {
//            fprintf (stderr, "Unable to allocate space for a new PV in \"%s\" with PV=\"%s\" host=\"%s\"\n",
//                    pFileName, pvNameStr, hostNameStr);
//            return -1;
//        }
		
extern "C" {

/// EPICS iocsh callable function to call constructor of lvDCOMInterface().
/// \param[in] portName @copydoc initArg0
/// \param[in] configSection @copydoc initArg1
/// \param[in] configFile @copydoc initArg2
/// \param[in] host @copydoc initArg3
/// \param[in] options @copydoc initArg4
/// \param[in] progid @copydoc initArg5
/// \param[in] username @copydoc initArg6
/// \param[in] password @copydoc initArg7
int isisdaeConfigure(const char *portName, int options, const char *host, const char* username, const char* password)
{
	try
	{
		isisdaeInterface* iface = new isisdaeInterface(host, options, username, password);
		if (iface != NULL)
		{
			isisdaeDriver* driver = new isisdaeDriver(iface, portName);
			if (epicsThreadCreate("daeCAS",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)daeCASThread, iface) == 0)
			{
				printf("epicsThreadCreate failure\n");
				return(asynError);
			}	
			return(asynSuccess);
		}
		else
		{
			errlogSevPrintf(errlogMajor, "isisdaeConfigure failed (NULL)\n");
			return(asynError);
		}
			
	}
	catch(const std::exception& ex)
	{
		errlogSevPrintf(errlogMajor, "isisdaeConfigure failed: %s\n", ex.what());
		return(asynError);
	}
}

// EPICS iocsh shell commands 

static const iocshArg initArg0 = { "portName", iocshArgString};			///< The name of the asyn driver port we will create
static const iocshArg initArg1 = { "options", iocshArgInt};			    ///< options as per #lvDCOMOptions enum
static const iocshArg initArg2 = { "host", iocshArgString};				///< host name where LabVIEW is running ("" for localhost) 
static const iocshArg initArg3 = { "username", iocshArgString};			///< (optional) remote username for host #initArg3
static const iocshArg initArg4 = { "password", iocshArgString};			///< (optional) remote password for username #initArg6 on host #initArg3

static const iocshArg * const initArgs[] = { &initArg0,
                                             &initArg1,
                                             &initArg2,
                                             &initArg3,
											 &initArg4 };

static const iocshFuncDef initFuncDef = {"isisdaeConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    isisdaeConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].sval, args[4].sval);
}

static void isisdaeRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(isisdaeRegister);

}

