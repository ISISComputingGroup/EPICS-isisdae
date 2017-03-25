#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <iostream>
#include <map>
#include <iomanip>
#include <sys/timeb.h>
#include <numeric>
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
#include "pugixml.hpp"

#include "isisdaeDriver.h"
#include "isisdaeInterface.h"
#include "convertToString.h"
#include "ADCore/ADApp/pluginSrc/colorMaps.h"
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
    m_inStateTrans = true;
    setIntegerParam(P_StateTrans, 1);
	callParamCallbacks();
    m_RunStatus = state;
	setIntegerParam(P_RunStatus, m_RunStatus);
	callParamCallbacks();
}

struct frame_uamp_state
{
    struct timeb tb;
	long frames;
	double uah;
	bool error; // error reported
	frame_uamp_state() : frames(0), uah(0.0), error(false) { memset(&tb, 0, sizeof(struct timeb)); }
};


static void check_frame_uamp(const char* type, long& frames, double& uah, frame_uamp_state& state)
{
    static const double uah_per_sec = 400.0 / 3600.0;  // use 400 microamp beam, to allow for timing errors etc
	static const long frames_per_sec = 100; // double 50Hz to allow for rounding erros etc
	static const long MAXFRAMES = (1 << 30); // 240 days at 50Hz
	static const double MAXUAH = 2e6; // 280 days 
    bool update_state = true;
    struct timeb now;
    ftime(&now);
    if (state.tb.time == 0) // first call
	{
	    memcpy(&(state.tb), &now, sizeof(struct timeb));
		state.error = false;
		if (uah >= 0.0 && uah < MAXUAH)
		{
		    state.uah = uah;
		}
		else
		{
		    state.uah = MAXUAH; 
		}
		if (frames >= 0 && frames < MAXFRAMES)
		{
		    state.frames = frames;
		}
		else
		{
		    state.frames = MAXFRAMES;  
		}
		return;
	}
	double tdiff = (now.time - state.tb.time) + (now.millitm - state.tb.millitm) / 1000.0; 
	if (tdiff < 0.1)
	{
		tdiff = 0.1; // we get called from poller and elsewhere, so can get a very small tdiff that elads to errors
	}
	// isis is 50Hz max, use 55 to just allow a bit of leeway 
    if ( (frames < 0) || (frames > MAXFRAMES) || ((frames - state.frames) >  frames_per_sec * tdiff) )
	{
		if (!state.error)
		{
			errlogSevPrintf(errlogInfo, "Ignoring invalid %s frames %ld", type, frames);
			state.error = true;
		}
	    frames = state.frames;
		update_state = false;
	}
	if ( (uah < 0.0) ||  (uah > MAXUAH) || ((uah - state.uah) >  uah_per_sec * tdiff) )
	{
		if (!state.error)
		{
			errlogSevPrintf(errlogInfo, "Ignoring invalid %s uah %f", type, uah);
			state.error = true;
		}
	    uah = state.uah;
		update_state = false;
	}
	if (update_state)
	{
	    memcpy(&(state.tb), &now, sizeof(struct timeb));
		state.uah = uah;
		state.frames = frames;
		if (state.error)
		{
			errlogSevPrintf(errlogInfo, "%s frames OK %ld uah OK %f", type, frames, uah);
		    state.error = false;
		}
	}
}

void isisdaeDriver::endStateTransition()
{
    m_inStateTrans = false;
	updateRunStatus();
    setIntegerParam(P_StateTrans, 0);
	callParamCallbacks();
}

void isisdaeDriver::setADAcquire(int acquire)
{
    int adstatus;
    int acquiring;
    int imageMode;
    asynStatus status = asynSuccess;

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
    getIntegerParam(ADStatus, &adstatus);
    getIntegerParam(ADAcquire, &acquiring);
    getIntegerParam(ADImageMode, &imageMode);
      if (acquire && !acquiring) {
        setStringParam(ADStatusMessage, "Acquiring data");
        setIntegerParam(ADStatus, ADStatusAcquire); 
        setIntegerParam(ADAcquire, 1); 
      }
      if (!acquire && acquiring) {
        setIntegerParam(ADAcquire, 0); 
        setStringParam(ADStatusMessage, "Acquisition stopped");
        if (imageMode == ADImageContinuous) {
          setIntegerParam(ADStatus, ADStatusIdle);
        } else {
          setIntegerParam(ADStatus, ADStatusAborted);
        }
      }
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
            zeroRunCounters();
			m_iface->beginRun();
			setADAcquire(1);
		}
        else if (function == P_BeginRunEx)
		{
		    beginStateTransition(RS_BEGINNING);
            zeroRunCounters();
			m_iface->beginRunEx(static_cast<long>(value), -1);
			setADAcquire(1);
		}
		else if (function == P_AbortRun)
		{
		    beginStateTransition(RS_ABORTING);
			m_iface->abortRun();
			setADAcquire(0);
		}
        else if (function == P_PauseRun)
		{
		    beginStateTransition(RS_PAUSING);
			m_iface->pauseRun();
			setADAcquire(0);
		}
        else if (function == P_ResumeRun)
		{
		    beginStateTransition(RS_RESUMING);
			m_iface->resumeRun();
			setADAcquire(1);
		}
        else if (function == P_EndRun)
		{
		    beginStateTransition(RS_ENDING);
			m_iface->endRun();
			setADAcquire(0);
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
			setADAcquire(0);
		}
        else if (function == P_EndSEWait)
		{
			m_iface->endSEWait();
			setADAcquire(1);
		}
        else if (function == P_Period)
		{
			m_iface->setPeriod(static_cast<long>(value));
		}
        else if (function == P_NumPeriods)
		{
			m_iface->setNumPeriods(static_cast<long>(value));
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
		status = ADDriver::readValue(pasynUser, functionName, &value);
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
        stat = ADDriver::writeFloat64(pasynUser, value);
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
        stat = ADDriver::writeInt32(pasynUser, value);
    }
	else
	{
	    callParamCallbacks(); // this flushes P_ErrMsgs
	}
    return stat;
}

asynStatus isisdaeDriver::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn)
{
	int function = pasynUser->reason;
	if (function < FIRST_ISISDAE_PARAM)
	{
		return ADDriver::readFloat64Array(pasynUser, value, nElements, nIn);
	}
    asynStatus stat = readArray(pasynUser, "readFloat64Array", value, nElements, nIn);
	callParamCallbacks(); // this flushes P_ErrMsgs
	doCallbacksFloat64Array(value, *nIn, function, 0);
    return stat;
}

asynStatus isisdaeDriver::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
	int function = pasynUser->reason;
	if (function < FIRST_ISISDAE_PARAM)
	{
		return ADDriver::readInt32Array(pasynUser, value, nElements, nIn);
	}
    asynStatus stat = readArray(pasynUser, "readInt32Array", value, nElements, nIn);
	callParamCallbacks(); // this flushes P_ErrMsgs
	doCallbacksInt32Array(value, *nIn, function, 0);
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
	return ADDriver::readOctet(pasynUser, value, maxChars, nActual, eomReason);

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
                size_t found = tcb_xml.find_last_of(">");  // in cased junk on end
                m_iface->setTCBSettingsXML(tcb_xml.substr(0,found+1));
			}
		}        
        else if (function == P_SnapshotCRPT)
		{
		    beginStateTransition(RS_STORING);
			m_iface->snapshotCRPT(value_s, 1, 1);
            endStateTransition();
		}        
        
		reportMessages();
		status = ADDriver::writeOctet(pasynUser, value_s.c_str(), value_s.size(), nActual);
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
        endStateTransition();
		*nActual = 0;
		return asynError;
	}
}


/// Constructor for the isisdaeDriver class.
/// Calls constructor for the asynPortDriver base class.
/// \param[in] dcomint DCOM interface pointer created by lvDCOMConfigure()
/// \param[in] portName @copydoc initArg0
isisdaeDriver::isisdaeDriver(isisdaeInterface* iface, const char *portName) 
   : ADDriver(portName, 
                    1, /* maxAddr */ 
                    NUM_ISISDAE_PARAMS,
					0, // maxBuffers
					0, // maxMemory
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask,  /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver can block but it is not multi-device */
					// need to think about ASYN_MULTIDEVICE for future multiple AD views
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0),	/* Default stack size*/
					m_iface(iface), m_RunStatus(0), m_vetopc(0.0), m_inStateTrans(false), m_pRaw(NULL)
{					
	int i;
	int status;
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
    createParam(P_SnapshotCRPTString, asynParamOctet, &P_SnapshotCRPT);
    createParam(P_StartSEWaitString, asynParamInt32, &P_StartSEWait);
    createParam(P_EndSEWaitString, asynParamInt32, &P_EndSEWait);
	createParam(P_RunStatusString, asynParamInt32, &P_RunStatus);
    createParam(P_TotalCountsString, asynParamInt32, &P_TotalCounts);
    
    createParam(P_RunTitleString, asynParamOctet, &P_RunTitle);
    createParam(P_AllMsgsString, asynParamOctet, &P_AllMsgs);
    createParam(P_ErrMsgsString, asynParamOctet, &P_ErrMsgs);
    createParam(P_RBNumberString, asynParamOctet, &P_RBNumber);
    createParam(P_RunNumberString, asynParamOctet, &P_RunNumber);
    createParam(P_IRunNumberString, asynParamInt32, &P_IRunNumber);
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

    createParam(P_wiringTableFileString, asynParamOctet, &P_wiringTableFile);
    createParam(P_detectorTableFileString, asynParamOctet, &P_detectorTableFile);
    createParam(P_spectraTableFileString, asynParamOctet, &P_spectraTableFile);
    createParam(P_tcbFileString, asynParamOctet, &P_tcbFile);
    createParam(P_periodsFileString, asynParamOctet, &P_periodsFile);
	
    createParam(P_diagTableSumString, asynParamInt32Array, &P_diagTableSum);
    createParam(P_diagTableSpecString, asynParamInt32Array, &P_diagTableSpec);
    createParam(P_diagTableMaxString, asynParamInt32Array, &P_diagTableMax);
    createParam(P_diagTableCntRateString, asynParamFloat64Array, &P_diagTableCntRate);
    createParam(P_diagFramesString, asynParamInt32, &P_diagFrames);
    createParam(P_diagMinFramesString, asynParamInt32, &P_diagMinFrames);
    createParam(P_diagEnableString, asynParamInt32, &P_diagEnable);
    createParam(P_diagPeriodString, asynParamInt32, &P_diagPeriod);
    createParam(P_diagSpecStartString, asynParamInt32, &P_diagSpecStart);
    createParam(P_diagSpecNumString, asynParamInt32, &P_diagSpecNum);
    createParam(P_diagSpecShowString, asynParamInt32, &P_diagSpecShow);
    createParam(P_diagSumString, asynParamInt32, &P_diagSum);
    createParam(P_diagSpecMatchString, asynParamInt32, &P_diagSpecMatch);
    createParam(P_diagSpecIntLowString, asynParamFloat64, &P_diagSpecIntLow);
    createParam(P_diagSpecIntHighString, asynParamFloat64, &P_diagSpecIntHigh);
	
    setIntegerParam(P_StateTrans, 0);

    // area detector defaults
//	int maxSizeX = 128, maxSizeY = 128;
	int maxSizeX = 8, maxSizeY = 8;
	NDDataType_t dataType = NDUInt16;
    status =  setStringParam (ADManufacturer, "STFC ISIS");
    status |= setStringParam (ADModel, "ISISDAE");
    status |= setIntegerParam(ADMaxSizeX, maxSizeX);
    status |= setIntegerParam(ADMaxSizeY, maxSizeY);
    status |= setIntegerParam(ADMinX, 0);
    status |= setIntegerParam(ADMinY, 0);
    status |= setIntegerParam(ADBinX, 1);
    status |= setIntegerParam(ADBinY, 1);
    status |= setIntegerParam(ADReverseX, 0);
    status |= setIntegerParam(ADReverseY, 0);
    status |= setIntegerParam(ADSizeX, maxSizeX);
    status |= setIntegerParam(ADSizeY, maxSizeY);
    status |= setIntegerParam(NDArraySizeX, maxSizeX);
    status |= setIntegerParam(NDArraySizeY, maxSizeY);
    status |= setIntegerParam(NDArraySize, 0);
    status |= setIntegerParam(NDDataType, dataType);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setDoubleParam (ADAcquireTime, .001);
    status |= setDoubleParam (ADAcquirePeriod, .005);
    status |= setIntegerParam(ADNumImages, 100);

    if (status) {
        printf("%s: unable to set DAE parameters\n", functionName);
        return;
    }

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
    if (epicsThreadCreate("isisdaePoller3",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)pollerThreadC3, this) == 0)
    {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }
    if (epicsThreadCreate("isisdaePoller4",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)pollerThreadC4, this) == 0)
    {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }
}

void isisdaeDriver::pollerThreadC1(void* arg)
{
	epicsThreadSleep(1.0);	// let constructor complete
    isisdaeDriver* driver = (isisdaeDriver*)arg;
	if (driver != NULL)
	{
	    driver->pollerThread1();
	}
}

void isisdaeDriver::pollerThreadC2(void* arg)
{ 
	epicsThreadSleep(1.0);	// let constructor complete
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	if (driver != NULL)
	{
	    driver->pollerThread2();
	}
}

void isisdaeDriver::pollerThreadC3(void* arg)
{ 
	epicsThreadSleep(1.0);	// let constructor complete
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	if (driver != NULL)
	{
	    driver->pollerThread3();
	}
}

void isisdaeDriver::pollerThreadC4(void* arg)
{ 
	epicsThreadSleep(1.0);	// let constructor complete
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	if (driver != NULL)
	{
	    driver->pollerThread4();
	}
}

void isisdaeDriver::pollerThread1()
{
    static const char* functionName = "isisdaePoller1";
    unsigned long counter = 0;
    double delay = 0.2;
    lock();
	while(true)
	{
        unlock();
		epicsThreadSleep(delay);
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
        ++counter;
    }
}

void isisdaeDriver::updateRunStatus()
{
        static frame_uamp_state fu_state, r_fu_state, p_fu_state;
		static const char* no_check_frame_uamp = macEnvExpand("$(NOCHECKFUAMP=)");

        if (m_inStateTrans)
        {
            return;
        }
        int rs = m_iface->getRunState();
		if (rs == RS_RUNNING)
		{
			setADAcquire(1);
		}
		else
		{
			setADAcquire(0);			
		}
        if (rs == RS_RUNNING && m_vetopc > 50.0)
        {
            m_RunStatus = RS_VETOING;
        }
        else
        {
            m_RunStatus = rs;
        }
		long frames = m_iface->getGoodFrames();
		double uah = m_iface->getGoodUAH();
//		long p_frames = m_iface->getGoodFramesPeriod();   // this currently only works in period card mode
//		double p_uah = m_iface->getGoodUAHPeriod();
		long r_frames = m_iface->getRawFrames();
		double r_uah = 0.0;
		if (no_check_frame_uamp == NULL || *no_check_frame_uamp == '\0')
		{
		    check_frame_uamp("good", frames, uah, fu_state);
		    check_frame_uamp("raw", r_frames, r_uah, r_fu_state);
//		    check_frame_uamp("period good", p_frames, p_uah, p_fu_state);
		}
		setDoubleParam(P_GoodUAH, uah);
//        setDoubleParam(P_GoodUAHPeriod, p_uah);
        setIntegerParam(P_TotalCounts, m_iface->getTotalCounts());
        setIntegerParam(P_GoodFramesTotal, frames);
//        setIntegerParam(P_GoodFramesPeriod, p_frames);
		setIntegerParam(P_RawFramesTotal, r_frames);
		setIntegerParam(P_RunStatus, m_RunStatus);
        ///@todo need to update P_RawFramesPeriod, P_RunDurationTotal, P_TotalUAmps, P_RunDurationPeriod,P_TotalDaeCounts, P_MonitorCounts
}

// zero counters st start of run, done early before actual readbacks
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
	    callParamCallbacks();
}

void isisdaeDriver::pollerThread2()
{
    static const char* functionName = "isisdaePoller2";
	std::map<std::string, DAEValue> values;
    unsigned long counter = 0;
    double delay = 2.0;  
    long this_rf = 0, this_gf = 0, last_rf = 0, last_gf = 0;
    bool check_settings;
    std::string daeSettings;
    std::string tcbSettings, tcbSettingComp;
    std::string hardwarePeriodsSettings;
    std::string updateSettings;
    std::string vetoStatus;

    lock();
	while(true)
	{
        unlock();
		epicsThreadSleep(delay);
		lock();
        if (m_inStateTrans)   // do nothing if in state transition
        {
            continue;
        }
        check_settings = ( (counter == 0) || (m_RunStatus == RS_SETUP && counter % 2 == 0) || (counter % 10 == 0) );
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
        setStringParam(P_RunTitle, values["RunTitle"]); 
        setStringParam(P_RBNumber, values["RBNumber"]); 
		const char* rn = values["RunNumber"];
        setStringParam(P_RunNumber, rn);
        setIntegerParam(P_IRunNumber, atol(rn));
        setStringParam(P_InstName, values["InstName"]);
        setStringParam(P_UserName, values["UserName"]);
        setStringParam(P_UserTelephone, values["UserTelephone"]);
        setStringParam(P_StartTime, values["StartTime"]);
        setDoubleParam(P_NPRatio, values["N/P Ratio"]);
        setStringParam(P_ISISCycle, values["ISISCycle"]);
        setStringParam(P_DAETimingSource, values["DAETimingSource"]);
        setStringParam(P_PeriodType, values["Period Type"]);
        
        setIntegerParam(P_RawFramesPeriod, values["RawFramesPeriod"]);
        setIntegerParam(P_GoodFramesPeriod, values["GoodFramesPeriod"]);
        
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
			
			std::string val;
			getDAEXML(daeSettings, "/Cluster/String[Name='Wiring Table']/Val", val);
			setStringParam(P_wiringTableFile, val.c_str());
			getDAEXML(daeSettings, "/Cluster/String[Name='Detector Table']/Val", val);
			setStringParam(P_detectorTableFile, val.c_str());
			getDAEXML(daeSettings, "/Cluster/String[Name='Spectra Table']/Val", val);
			setStringParam(P_spectraTableFile, val.c_str());
			getDAEXML(tcbSettings, "/Cluster/String[Name='Time Channel File']/Val", val);
			setStringParam(P_tcbFile, val.c_str());
			getDAEXML(hardwarePeriodsSettings, "/Cluster/String[Name='Period File']/Val", val);
			setStringParam(P_periodsFile, val.c_str());
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
        ++counter;
	}
}	
			
void isisdaeDriver::pollerThread3()
{
    static const char* functionName = "isisdaePoller3";
    double delay = 2.0;
	std::vector<long> sums[2], max_vals, spec_nums;
	std::vector<double> rate;
	int frames[2] = {0, 0}, period = 1, first_spec = 1, num_spec = 10, spec_type = 0;
	double time_low = 0.0, time_high = -1.0;
	bool b = true;
	int i1, i2, n1, sum, fdiff, fdiff_min = 0, diag_enable = 0;
    lock();
	// read sums alternately into sums[0] and sums[1] by toggling b so a count rate can be calculated
	while(true)
	{
        unlock();
		epicsThreadSleep(delay);
		i1 = (b == true ? 0 : 1);
		i2 = (b == true ? 1 : 0);
		frames[i1] = m_iface->getGoodFrames(); // read prior to lock in case ICP busy
        lock();
		getIntegerParam(P_diagEnable, &diag_enable);
		if (diag_enable != 1)
			continue;
		fdiff = frames[i1] - frames[i2];
		getIntegerParam(P_diagMinFrames, &fdiff_min);
		if (fdiff < fdiff_min)
			continue;
		getIntegerParam(P_diagSpecShow, &spec_type);
		getIntegerParam(P_diagSpecStart, &first_spec);
		getIntegerParam(P_diagSpecNum, &num_spec);
		getIntegerParam(P_diagPeriod, &period);
		getDoubleParam(P_diagSpecIntLow, &time_low);
		getDoubleParam(P_diagSpecIntHigh, &time_high);
		
        unlock(); // getSepctraSum may take a while so release asyn lock
		m_iface->getSpectraSum(period, first_spec, num_spec, spec_type, 
		     time_low, time_high, sums[i1], max_vals, spec_nums);
        lock();
		n1 = sums[i1].size();
		sum = std::accumulate(sums[i1].begin(), sums[i1].end(), 0);
		rate.resize(n1);
		if ( n1 == sums[i2].size() && fdiff > 0 )
		{
			for(int i=0; i<n1; ++i)
			{
				rate[i] = static_cast<double>(sums[i1][i] - sums[i2][i]) / static_cast<double>(fdiff);
			}
		}
		else
		{
			std::fill(rate.begin(), rate.end(), 0.0);
		}
	    doCallbacksInt32Array(reinterpret_cast<epicsInt32*>(&(sums[i1][0])), n1, P_diagTableSum, 0);
	    doCallbacksInt32Array(reinterpret_cast<epicsInt32*>(&(max_vals[0])), n1, P_diagTableMax, 0);
	    doCallbacksInt32Array(reinterpret_cast<epicsInt32*>(&(spec_nums[0])), n1, P_diagTableSpec, 0);
		doCallbacksFloat64Array(reinterpret_cast<epicsFloat64*>(&(rate[0])), n1, P_diagTableCntRate, 0);
		setIntegerParam(P_diagFrames, fdiff);
		setIntegerParam(P_diagSum, sum);
		setIntegerParam(P_diagSpecMatch, n1);
        callParamCallbacks();
		b = !b;
    }
}

void isisdaeDriver::pollerThread4()
{
    static const char* functionName = "isisdaePoller4";
    double loop_delay = 2.0;
	int acquiring;
    int status = asynSuccess;
    int imageCounter;
    int numImages, numImagesCounter;
    int imageMode;
    int arrayCallbacks;
    int old_acquiring = 0;
    NDArray *pImage;
    double acquireTime, acquirePeriod, delay;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;

	while(true)
	{
		lock();
		getIntegerParam(ADAcquire, &acquiring);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
		if (acquiring == 0)
		{
			old_acquiring = acquiring;
			unlock();
			epicsThreadSleep(acquirePeriod);
			continue;
		}
		if (old_acquiring == 0)
		{
            setIntegerParam(ADNumImagesCounter, 0);
			old_acquiring = acquiring;
        }
        setIntegerParam(ADStatus, ADStatusAcquire); 
		epicsTimeGetCurrent(&startTime);
        getIntegerParam(ADImageMode, &imageMode);

        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);  // not really used

        setShutter(ADShutterOpen);
        callParamCallbacks();
            
        /* Update the image */
        status = computeImage();
//        if (status) continue;

		// could sleep to make up to acquireTime
		
        /* Close the shutter */
        setShutter(ADShutterClosed);
        
        setIntegerParam(ADStatus, ADStatusReadout);
        /* Call the callbacks to update any changes */
        callParamCallbacks();

        pImage = this->pArrays[0];

        /* Get the current parameters */
        getIntegerParam(NDArrayCounter, &imageCounter);
        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &numImagesCounter);
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        imageCounter++;
        numImagesCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);
        setIntegerParam(ADNumImagesCounter, numImagesCounter);

        /* Put the frame number and time stamp into the buffer */
        pImage->uniqueId = imageCounter;
        pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;
        updateTimeStamp(&pImage->epicsTS);

        /* Get any attributes that have been defined for this driver */
        this->getAttributes(pImage->pAttributeList);

        if (arrayCallbacks) {
          /* Call the NDArray callback */
          /* Must release the lock here, or we can get into a deadlock, because we can
           * block on the plugin lock, and the plugin can be calling us */
          this->unlock();
          asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s:%s: calling imageData callback\n", driverName, functionName);
          doCallbacksGenericPointer(pImage, NDArrayData, 0);
          this->lock();
        }

        /* Call the callbacks to update any changes */
        callParamCallbacks();
        /* sleep for the acquire period minus elapsed time. */
        epicsTimeGetCurrent(&endTime);
        elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
        delay = acquirePeriod - elapsedTime;
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s:%s: delay=%f\n",
                    driverName, functionName, delay);
        if (delay >= 0.0) {
            /* We set the status to waiting to indicate we are in the period delay */
            setIntegerParam(ADStatus, ADStatusWaiting);
            callParamCallbacks();
            this->unlock();
            epicsThreadSleep(delay);
            this->lock();
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();  
        }
        this->unlock();
    }
}

/** Computes the new image data */
int isisdaeDriver::computeImage()
{
    int status = asynSuccess;
    NDDataType_t dataType;
    int itemp;
    int binX, binY, minX, minY, sizeX, sizeY, reverseX, reverseY;
    int xDim=0, yDim=1, colorDim=-1;
    int resetImage;
    int maxSizeX, maxSizeY;
    int colorMode;
    int ndims=0;
    NDDimension_t dimsOut[3];
    size_t dims[3];
    NDArrayInfo_t arrayInfo;
    NDArray *pImage;
    const char* functionName = "computeImage";

    /* NOTE: The caller of this function must have taken the mutex */

    status |= getIntegerParam(ADBinX,         &binX);
    status |= getIntegerParam(ADBinY,         &binY);
    status |= getIntegerParam(ADMinX,         &minX);
    status |= getIntegerParam(ADMinY,         &minY);
    status |= getIntegerParam(ADSizeX,        &sizeX);
    status |= getIntegerParam(ADSizeY,        &sizeY);
    status |= getIntegerParam(ADReverseX,     &reverseX);
    status |= getIntegerParam(ADReverseY,     &reverseY);
    status |= getIntegerParam(ADMaxSizeX,     &maxSizeX);
    status |= getIntegerParam(ADMaxSizeY,     &maxSizeY);
    status |= getIntegerParam(NDColorMode,    &colorMode);
    status |= getIntegerParam(NDDataType,     &itemp); 
	dataType = (NDDataType_t)itemp;
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error getting parameters\n",
                    driverName, functionName);

    /* Make sure parameters are consistent, fix them if they are not */
    if (binX < 1) {
        binX = 1;
        status |= setIntegerParam(ADBinX, binX);
    }
    if (binY < 1) {
        binY = 1;
        status |= setIntegerParam(ADBinY, binY);
    }
    if (minX < 0) {
        minX = 0;
        status |= setIntegerParam(ADMinX, minX);
    }
    if (minY < 0) {
        minY = 0;
        status |= setIntegerParam(ADMinY, minY);
    }
    if (minX > maxSizeX-1) {
        minX = maxSizeX-1;
        status |= setIntegerParam(ADMinX, minX);
    }
    if (minY > maxSizeY-1) {
        minY = maxSizeY-1;
        status |= setIntegerParam(ADMinY, minY);
    }
    if (minX+sizeX > maxSizeX) {
        sizeX = maxSizeX-minX;
        status |= setIntegerParam(ADSizeX, sizeX);
    }
    if (minY+sizeY > maxSizeY) {
        sizeY = maxSizeY-minY;
        status |= setIntegerParam(ADSizeY, sizeY);
    }

    switch (colorMode) {
        case NDColorModeMono:
            ndims = 2;
            xDim = 0;
            yDim = 1;
            break;
        case NDColorModeRGB1:
            ndims = 3;
            colorDim = 0;
            xDim     = 1;
            yDim     = 2;
            break;
        case NDColorModeRGB2:
            ndims = 3;
            colorDim = 1;
            xDim     = 0;
            yDim     = 2;
            break;
        case NDColorModeRGB3:
            ndims = 3;
            colorDim = 2;
            xDim     = 0;
            yDim     = 1;
            break;
    }

// we could be more efficient
//    if (resetImage) {
    /* Free the previous raw buffer */
        if (m_pRaw) m_pRaw->release();
        /* Allocate the raw buffer we use to compute images. */
        dims[xDim] = maxSizeX;
        dims[yDim] = maxSizeY;
        if (ndims > 2) dims[colorDim] = 3;
        m_pRaw = this->pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);

        if (!m_pRaw) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:%s: error allocating raw buffer\n",
                      driverName, functionName);
            return(status);
        }
//    }

    switch (dataType) {
        case NDInt8:
            status |= computeArray<epicsInt8>(maxSizeX, maxSizeY);
            break;
        case NDUInt8:
            status |= computeArray<epicsUInt8>(maxSizeX, maxSizeY);
            break;
        case NDInt16:
            status |= computeArray<epicsInt16>(maxSizeX, maxSizeY);
            break;
        case NDUInt16:
            status |= computeArray<epicsUInt16>(maxSizeX, maxSizeY);
            break;
        case NDInt32:
            status |= computeArray<epicsInt32>(maxSizeX, maxSizeY);
            break;
        case NDUInt32:
            status |= computeArray<epicsUInt32>(maxSizeX, maxSizeY);
            break;
        case NDFloat32:
            status |= computeArray<epicsFloat32>(maxSizeX, maxSizeY);
            break;
        case NDFloat64:
            status |= computeArray<epicsFloat64>(maxSizeX, maxSizeY);
            break;
    }

    /* Extract the region of interest with binning.
     * If the entire image is being used (no ROI or binning) that's OK because
     * convertImage detects that case and is very efficient */
    m_pRaw->initDimension(&dimsOut[xDim], sizeX);
    m_pRaw->initDimension(&dimsOut[yDim], sizeY);
    if (ndims > 2) m_pRaw->initDimension(&dimsOut[colorDim], 3);
    dimsOut[xDim].binning = binX;
    dimsOut[xDim].offset  = minX;
    dimsOut[xDim].reverse = reverseX;
    dimsOut[yDim].binning = binY;
    dimsOut[yDim].offset  = minY;
    dimsOut[yDim].reverse = reverseY;
    /* We save the most recent image buffer so it can be used in the read() function.
     * Now release it before getting a new version. */
    if (this->pArrays[0]) this->pArrays[0]->release();
    status = this->pNDArrayPool->convert(m_pRaw,
                                         &this->pArrays[0],
                                         dataType,
                                         dimsOut);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating buffer in convert()\n",
                    driverName, functionName);
        return(status);
    }
    pImage = this->pArrays[0];
    pImage->getInfo(&arrayInfo);
    status = asynSuccess;
    status |= setIntegerParam(NDArraySize,  (int)arrayInfo.totalBytes);
    status |= setIntegerParam(NDArraySizeX, (int)pImage->dims[xDim].size);
    status |= setIntegerParam(NDArraySizeY, (int)pImage->dims[yDim].size);
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error setting parameters\n",
                    driverName, functionName);
    return(status);
}

template <typename epicsType> 
void isisdaeDriver::computeColour(uint32_t value, uint32_t maxval, epicsType& mono)
{
	epicsType limit = std::numeric_limits<epicsType>::max();
	if (maxval > 0)
	{
		mono = static_cast<epicsType>((double)value / (double)maxval * (double)limit);
	}
	else
	{
		mono = static_cast<epicsType>(0);
	}
}

template <typename epicsType> 
void isisdaeDriver::computeColour(uint32_t value, uint32_t maxval, epicsType& red, epicsType& green, epicsType& blue)
{
	int i;
	epicsType limit = std::numeric_limits<epicsType>::max();
	if (maxval > 0)
	{
		i = (int)round(255.0 * (double)value / (double)maxval);
	}
	else
	{
		i = 0;
	}
	red = static_cast<epicsType>((double)RainbowColorR[i] / 255.0 * (double)limit);
	green = static_cast<epicsType>((double)RainbowColorG[i] / 255.0 * (double)limit);
	blue = static_cast<epicsType>((double)RainbowColorB[i] / 255.0 * (double)limit);
}

template <typename epicsType> 
int isisdaeDriver::computeArray(int sizeX, int sizeY)
{
    epicsType *pMono=NULL, *pRed=NULL, *pGreen=NULL, *pBlue=NULL;
    int columnStep=0, rowStep=0, colorMode;
    int status = asynSuccess;
    double exposureTime, gain;
    int i, j, k;
	
    status = getDoubleParam (ADGain,        &gain);
    status = getIntegerParam(NDColorMode,   &colorMode);
    status = getDoubleParam (ADAcquireTime, &exposureTime);

    switch (colorMode) {
        case NDColorModeMono:
            pMono = (epicsType *)m_pRaw->pData;
            break;
        case NDColorModeRGB1:
            columnStep = 3;
            rowStep = 0;
            pRed   = (epicsType *)m_pRaw->pData;
            pGreen = (epicsType *)m_pRaw->pData+1;
            pBlue  = (epicsType *)m_pRaw->pData+2;
            break;
        case NDColorModeRGB2:
            columnStep = 1;
            rowStep = 2 * sizeX;
            pRed   = (epicsType *)m_pRaw->pData;
            pGreen = (epicsType *)m_pRaw->pData + sizeX;
            pBlue  = (epicsType *)m_pRaw->pData + 2*sizeX;
            break;
        case NDColorModeRGB3:
            columnStep = 1;
            rowStep = 0;
            pRed   = (epicsType *)m_pRaw->pData;
            pGreen = (epicsType *)m_pRaw->pData + sizeX*sizeY;
            pBlue  = (epicsType *)m_pRaw->pData + 2*sizeX*sizeY;
            break;
    }
    m_pRaw->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

	const uint32_t* integrals = m_iface->getEventSpecIntegrals();
	
    k = 0;
	uint32_t maxval = 0;
	for (i=0; i<sizeY; i++) {
			for (j=0; j<sizeX; j++) {
				if (integrals[k] > maxval)
				{
					maxval = integrals[k];
				}
				++k;
			}
	}
	
	k = 0;
	for (i=0; i<sizeY; i++) {
		switch (colorMode) {
			case NDColorModeMono:
				for (j=0; j<sizeX; j++) {
					computeColour(integrals[k], maxval, *pMono);
					++pMono;
					++k;
				}
				break;
			case NDColorModeRGB1:
			case NDColorModeRGB2:
			case NDColorModeRGB3:
				for (j=0; j<sizeX; j++) {
					computeColour(integrals[k], maxval, *pRed, *pGreen, *pBlue);
					pRed   += columnStep;
					pGreen += columnStep;
					pBlue  += columnStep;
					++k;
				}
				pRed   += rowStep;
				pGreen += rowStep;
				pBlue  += rowStep;
				break;
		}
	}
    return(status);
}

void isisdaeDriver::getDAEXML(const std::string& xmlstr, const std::string& path, std::string& value)
{
	value = "";
	pugi::xml_document dae_doc;
	pugi::xml_parse_result result = dae_doc.load_buffer(xmlstr.c_str(), xmlstr.size());
 	if (!result)
	{
	    std::cerr << "Error in XML: " << result.description() << " at offset " << result.offset << std::endl;
		return;
	}
	pugi::xpath_node_set nodes = dae_doc.select_nodes(path.c_str());
	if (nodes.size() > 0)
	{
	    value = nodes[0].node().child_value();
	}
}

/** Controls the shutter */
void isisdaeDriver::setShutter(int open)
{
    int shutterMode;

    getIntegerParam(ADShutterMode, &shutterMode);
    if (shutterMode == ADShutterModeDetector) {
        /* Simulate a shutter by just changing the status readback */
        setIntegerParam(ADShutterStatus, open);
    } else {
        /* For no shutter or EPICS shutter call the base class method */
        ADDriver::setShutter(open);
    }
}

/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void isisdaeDriver::report(FILE *fp, int details)
{
    fprintf(fp, "ISIS DAE driver %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
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
			if (driver != NULL)
			{
				if (epicsThreadCreate("daeCAS",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackMedium),
                          (EPICSTHREADFUNC)daeCASThread, iface) == 0)
				{
					printf("epicsThreadCreate failure\n");
					return(asynError);
				}
			}
			else
			{
			    errlogSevPrintf(errlogMajor, "isisdaeConfigure failed (NULL)\n");
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

