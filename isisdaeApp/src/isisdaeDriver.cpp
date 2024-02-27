#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <iostream>
#include <map>
#include <vector>
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
#include <initHooks.h>
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

const char* isisdaeDriver::RunStateNames[] = { "PROCESSING", "SETUP", "RUNNING", "PAUSED", "WAITING", "VETOING", "ENDING", "SAVING", "RESUMING", "PAUSING", "BEGINNING", "ABORTING", "UPDATING", "STORING", "CHANGING" };

/// An STL exception describing a Win32 Structured Exception. 
/// Code needs to be compiled with /EHa if you wish to use this via _set_se_translator().
/// Note that _set_se_translator() needs to be called on a per thread basis
class Win32StructuredException : public std::runtime_error
{
public:
	explicit Win32StructuredException(const std::string& message) : std::runtime_error(message) { }
	explicit Win32StructuredException(unsigned int code, EXCEPTION_POINTERS *pExp) : std::runtime_error(win32_message(code, pExp)) { }
private:
	static std::string win32_message(unsigned int code, EXCEPTION_POINTERS * pExp);
};

std::string Win32StructuredException::win32_message(unsigned int code, EXCEPTION_POINTERS * pExp)
{
	char buffer[256];
	_snprintf(buffer, sizeof(buffer), "Win32StructuredException code 0x%x pExpCode 0x%x pExpAddress %p", code, 
                  (unsigned)pExp->ExceptionRecord->ExceptionCode, pExp->ExceptionRecord->ExceptionAddress);
	buffer[sizeof(buffer)-1] = '\0';
	return std::string(buffer);
}

/// Function to translate a Win32 structured exception into a standard C++ exception. 
/// This is registered via registerStructuredExceptionHandler()
static void seTransFunction(unsigned int u, EXCEPTION_POINTERS* pExp)
{
	throw Win32StructuredException(u, pExp);
}

/// Register a handler for Win32 structured exceptions. This needs to be done on a per thread basis.
static void registerStructuredExceptionHandler()
{
	_set_se_translator(seTransFunction);
}

void isisdaeDriver::reportErrors(const char* exc_text)
{
	try
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
	catch(const std::exception& ex)
	{
		errlogSevPrintf(errlogMajor, "Exception %s in reportErrors()", ex.what());
	}
	catch(...)
	{
		errlogSevPrintf(errlogMajor, "Unknown exception in reportErrors()");
	}
}

void isisdaeDriver::reportMessages()
{
	try
	{
	    std::string msgs = m_iface->getAllMessages();
        setStringParam(P_AllMsgs, msgs.c_str());
        setStringParam(P_ErrMsgs, "");
// getAsynMessages will pick these up and report to screen
//		errlogSevPrintf(errlogInfo, "%s", msgs.c_str());
	    m_iface->resetMessages();
	}
	catch(const std::exception& ex)
	{
		errlogSevPrintf(errlogMajor, "Exception %s in reportMessages()", ex.what());
	}
	catch(...)
	{
		errlogSevPrintf(errlogMajor, "Unknown exception in reportMessages()");
	}
}

void isisdaeDriver::beginStateTransition(int state)
{
    // signal items separately, might help order monitors get sent in?  
    m_inStateTrans = true;
	if (state == RS_CHANGING)
	{
        m_RunStatus = RS_SETUP;
        setIntegerParam(P_inChangingState, 1);
	}
	else
	{
        m_RunStatus = state;		
        setIntegerParam(P_inChangingState, 0);
	}
	setIntegerParam(P_RunStatus, m_RunStatus);
	callParamCallbacks();
    setIntegerParam(P_StateTrans, 1);
	callParamCallbacks();
}

struct frame_uamp_state
{
    struct timeb tb;
	long frames;
	double uah;
	bool frames_error; // error reported
	bool uamps_error; // error reported
	frame_uamp_state() : frames(0), uah(0.0), frames_error(false), uamps_error(false) { memset(&tb, 0, sizeof(struct timeb)); }
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
		state.frames_error = state.uamps_error = false;
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
    double tdiff = difftime(now.time, state.tb.time) + ((int)now.millitm - (int)state.tb.millitm) / 1000.0;

	if (tdiff < 0.1)
	{
		tdiff = 0.1; // we get called from poller and elsewhere, so can get a very small tdiff that leads to errors
	}
	// isis is 50Hz max, use 55 to just allow a bit of leeway 
    if ( (frames < 0) || (frames > MAXFRAMES) || ((frames - state.frames) >  frames_per_sec * tdiff) )
	{
		if (!state.frames_error)
		{
			errlogSevPrintf(errlogInfo, "Ignoring invalid %s frames %ld (old = %ld, tdiff = %f)", type, frames, state.frames, tdiff);
			state.frames_error = true;
		}
	    frames = state.frames;
		update_state = false;
	}
	if ( (uah < 0.0) ||  (uah > MAXUAH) || ((uah - state.uah) >  uah_per_sec * tdiff) )
	{
		if (!state.uamps_error)
		{
			errlogSevPrintf(errlogInfo, "Ignoring invalid %s uah %f (old = %f, tdiff = %f)", type, uah, state.uah, tdiff);
			state.uamps_error = true;
		}
	    uah = state.uah;
		update_state = false;
	}
	if (update_state)
	{
	    memcpy(&(state.tb), &now, sizeof(struct timeb));
		state.uah = uah;
		state.frames = frames;
		if (state.frames_error)
		{
			errlogSevPrintf(errlogInfo, "%s frames OK %ld", type, frames);
		    state.frames_error = false;
		}
		if (state.uamps_error)
		{
			errlogSevPrintf(errlogInfo, "%s uah OK %f", type, uah);
		    state.uamps_error = false;
		}
	}
}

void isisdaeDriver::endStateTransition()
{
    m_inStateTrans = false;
    setIntegerParam(P_StateTrans, 0);
    setIntegerParam(P_inChangingState, 0);
	callParamCallbacks();
	try
	{
		updateRunStatus();
	}
	catch (const std::exception& ex)
	{
		std::cerr << "endStateTransition exception: " << ex.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "endStateTransition unknown exception" << std::endl;
	}
	callParamCallbacks();
}

void isisdaeDriver::setADAcquire(int acquire)
{
    int adstatus;
    int acquiring;
    int imageMode;
    asynStatus status = asynSuccess;

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
	for(int i=0; i<maxAddr; ++i)
	{
		getIntegerParam(i, ADStatus, &adstatus);
		getIntegerParam(i, ADAcquire, &acquiring);
		getIntegerParam(i, ADImageMode, &imageMode);
		  if (acquire && !acquiring) {
			setStringParam(i, ADStatusMessage, "Acquiring data");
			setIntegerParam(i, ADStatus, ADStatusAcquire); 
			setIntegerParam(i, ADAcquire, 1); 
		  }
		  if (!acquire && acquiring) {
			setIntegerParam(i, ADAcquire, 0); 
			setStringParam(i, ADStatusMessage, "Acquisition stopped");
			if (imageMode == ADImageContinuous) {
			  setIntegerParam(i, ADStatus, ADStatusIdle);
			} else {
			  setIntegerParam(i, ADStatus, ADStatusAborted);
			}
		  }
	}
}

template<typename T>
asynStatus isisdaeDriver::writeValue(asynUser *pasynUser, const char* functionName, T value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	int addr = 0;
	getAddress(pasynUser, &addr);
	try
	{
	    m_iface->resetMessages();
		const std::vector<int>& disallowed = m_disallowedStateCommand[m_RunStatus];
		if ( std::find(disallowed.begin(), disallowed.end(), function) != disallowed.end() )
		{
			std::ostringstream mess;
			mess << "Cannot execute command \"" << paramName << "\" when in state \"" << RunStateNames[m_RunStatus] << "\"";
			throw std::runtime_error(mess.str());
		}			
		if (function == P_BeginRun)
		{
            int blockSpecZero = 0;
            getIntegerParam(P_blockSpecZero, &blockSpecZero);
		    beginStateTransition(RS_BEGINNING);
            zeroRunCounters();
			m_iface->setICPValueLong("BLOCK_SPEC_ZERO", blockSpecZero);
			m_iface->beginRun();
			setADAcquire(1);
		}
        else if (function == P_BeginRunEx)
		{
            int blockSpecZero = 0;
            getIntegerParam(P_blockSpecZero, &blockSpecZero);
		    beginStateTransition(RS_BEGINNING);
            zeroRunCounters();
			m_iface->setICPValueLong("BLOCK_SPEC_ZERO", blockSpecZero);
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
        else if (function == P_SEWait)
		{
            if (value != 0)
            {
			    m_iface->startSEWait();
			    setADAcquire(0);
            }
            else
            {
			    m_iface->endSEWait();
			    setADAcquire(1);
            }
		}
        else if (function == P_Period)
		{
			m_iface->setPeriod(static_cast<long>(value));
		}
        else if (function == P_NumPeriods)
		{
			m_iface->setNumPeriods(static_cast<long>(value));
		}
		else if (function == P_integralsTMin)
		{
			double intgTMax(0.0);
			getDoubleParam(addr, P_integralsTMax, &intgTMax);
			m_iface->setSpecIntgCutoff(value, intgTMax);
		}
		else if (function == P_integralsTMax)
		{
			double intgTMin(0.0);
			getDoubleParam(addr, P_integralsTMin, &intgTMin);
			m_iface->setSpecIntgCutoff(intgTMin, value);
		}
		endStateTransition();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, convertToString(value).c_str());
		reportMessages();
		status = asynSuccess;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s, error=%s", 
                  driverName, functionName, status, function, paramName, convertToString(value).c_str(), ex.what());
		reportErrors(ex.what());
		endStateTransition();
		status = asynError;
	}
	return status;
}

#if 0
// need to consider function < FIRST_ISISDAE_PARAM here or elsewhere if used. 
template<typename T>
asynStatus isisdaeDriver::readValue(asynUser *pasynUser, const char* functionName, T* value)
{
	int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	try
	{
	    m_iface->resetMessages();
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
#endif

template<typename T>
asynStatus isisdaeDriver::readArray(asynUser *pasynUser, const char* functionName, T *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	try
	{
	    m_iface->resetMessages();
		if (function == P_VMEReadValueProps || function == P_VMEReadArrayProps || function == P_QXReadArrayProps)
		{
			std::vector<epicsInt32>& props = m_directRWProps[function];
			if (nElements >= props.size())
			{
				std::copy(props.begin(), props.end(), value);
				*nIn = props.size();
			}
			else
			{
				throw std::runtime_error("props not large enough");
			}
		}
		else if (function == P_VMEReadArrayData)
		{
			std::vector<epicsInt32>& props = m_directRWProps[P_VMEReadArrayProps];
			int n = props[2];
			if (n <= nElements)
			{
				std::vector<long> value_v;
				m_iface->VMEReadArray(props[0], props[1], value_v, n);
				std::copy(value_v.begin(), value_v.end(), value);
				*nIn = n;
			}
			else
			{
			    throw std::runtime_error("array not large enough");
			}
		}
		else if (function == P_QXReadArrayData)
		{
			std::vector<epicsInt32>& props = m_directRWProps[P_QXReadArrayProps];
			int n = props[2];
			if (n <= nElements)
			{
				std::vector<long> value_v;
				m_iface->QXReadArray(props[0], props[1], value_v, n, props[3]);
				std::copy(value_v.begin(), value_v.end(), value);
				*nIn = n;
			}
			else
			{
			    throw std::runtime_error("array not large enough");
			}
	    }
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, nElements=%llu, nIn=%llu\n", 
              driverName, functionName, function, paramName, (unsigned long long)nElements, (unsigned long long)(*nIn));
		reportMessages();
		return status;
	}
	catch(const std::exception& ex)
	{
		*nIn = 0;
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, nElements=%llu, error=%s", 
                  driverName, functionName, status, function, paramName, (unsigned long long)nElements, ex.what());
		reportErrors(ex.what());
		return asynError;
	}
}

asynStatus isisdaeDriver::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
	int function = pasynUser->reason;
	if (function < FIRST_ISISDAE_PARAM)
	{
	    registerStructuredExceptionHandler();
        return ADDriver::writeFloat64(pasynUser, value);
	}
	asynStatus stat = writeValue(pasynUser, "writeFloat64", value);
	if (stat == asynSuccess)
	{
		asynPortDriver::writeFloat64(pasynUser, value); // to update parameter and do callbacks
	}
	else
	{
		callParamCallbacks(); // this flushes P_ErrMsgs
	}
	return stat;
}

asynStatus isisdaeDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
	int function = pasynUser->reason;
	if (function < FIRST_ISISDAE_PARAM)
	{
	    registerStructuredExceptionHandler();
        return ADDriver::writeInt32(pasynUser, value);
	}
	asynStatus stat = writeValue(pasynUser, "writeInt32", value);
	if (stat == asynSuccess)
	{
		asynPortDriver::writeInt32(pasynUser, value); // to update parameter and do callbacks
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
	registerStructuredExceptionHandler();
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
	registerStructuredExceptionHandler();
	if (function < FIRST_ISISDAE_PARAM)
	{
		return ADDriver::readInt32Array(pasynUser, value, nElements, nIn);
	}
    asynStatus stat = readArray(pasynUser, "readInt32Array", value, nElements, nIn);
	callParamCallbacks(); // this flushes P_ErrMsgs
	doCallbacksInt32Array(value, *nIn, function, 0);
    return stat;
}

asynStatus isisdaeDriver::writeInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements)
{
	int function = pasynUser->reason;
	const char *functionName = "writeInt32Array";
    asynStatus status = asynSuccess;
	registerStructuredExceptionHandler();
    const char *paramName = NULL;
	getParamName(function, &paramName);
	if (function < FIRST_ISISDAE_PARAM)
	{
		return ADDriver::writeInt32Array(pasynUser, value, nElements);
	}
	try
	{
	    m_iface->resetMessages();
		if (function == P_VMEReadValueProps || function == P_VMEReadArrayProps ||  function == P_QXReadArrayProps)
		{
				std::vector<epicsInt32>& props = m_directRWProps[function];
				if (nElements == props.size())
				{
					std::copy(value, value + nElements, props.begin());
				}
				else
				{
				    throw std::runtime_error("props size wrong");
				}
		}
		else if (function == P_VMEWriteValue && nElements == 5)
		{
			m_iface->VMEWriteValue(value[0], value[1], value[2], value[4], value[3]);
		}		
		else if (function == P_VMEWriteArray && nElements >= 3) // card_id, address, num_values, value1, value2, ...
		{
		    int n = value[2];
			if ( nElements >= (n + 3) )
			{
			    std::vector<long> value_v(value + 3, value + n + 3);
			    m_iface->VMEWriteArray(value[0], value[1], value_v);
			}
			else
			{
				throw std::runtime_error("array size wrong");
			}
		}
		else if (function == P_QXWriteArray && nElements >= 4) // card_id, card_address, num_values, transfer_type, value1, value2, ...
		{
		    int n = value[2];
			if ( nElements >= (n + 4) )
			{
			    std::vector<long> value_v(value + 4, value + n + 4);
			    m_iface->QXWriteArray(value[0], value[1], value_v, value[3]);
			}
			else
			{
				throw std::runtime_error("array size wrong");
			}
		}
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, nElements=%llu\n", 
              driverName, functionName, function, paramName, (unsigned long long)nElements);
		reportMessages();
		return status;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, nElements=%llu, error=%s", 
                  driverName, functionName, status, function, paramName, (unsigned long long)nElements, ex.what());
		reportErrors(ex.what());
		return asynError;
	}
}

asynStatus isisdaeDriver::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
	int function = pasynUser->reason;
	const char *functionName = "readFloat64";
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	try
	{
	    if (function < FIRST_ISISDAE_PARAM)
	    {
	        return ADDriver::readFloat64(pasynUser, value);
	    }
	    else
	    {
	        m_iface->resetMessages();
	        return asynPortDriver::readFloat64(pasynUser, value);
        }
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: function=%d, name=%s, error=%s", 
                  driverName, functionName, function, paramName, ex.what());
		reportErrors(ex.what());
		callParamCallbacks(); // this flushes P_ErrMsgs
		return asynError;
	}
}

asynStatus isisdaeDriver::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
	int function = pasynUser->reason;
	const char *functionName = "readInt32";
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	try
	{
	    if (function < FIRST_ISISDAE_PARAM)
	    {
	        return ADDriver::readInt32(pasynUser, value);
	    }
	    m_iface->resetMessages();
		if (function == P_VMEReadValueData)
		{
			std::vector<epicsInt32>& props = m_directRWProps[P_VMEReadValueProps];
			unsigned long valtemp;
			m_iface->VMEReadValue(props[0], props[1], props[2], valtemp);
			*value = valtemp;
		}
		else
		{
	        return asynPortDriver::readInt32(pasynUser, value);
		}
		asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, convertToString(*value).c_str());
		reportMessages();
		return asynSuccess;
	}
	catch(const std::exception& ex)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: function=%d, name=%s, error=%s", 
                  driverName, functionName, function, paramName, ex.what());
		reportErrors(ex.what());
		callParamCallbacks(); // this flushes P_ErrMsgs
		return asynError;
	}
}

asynStatus isisdaeDriver::readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason)
{
	int function = pasynUser->reason;
	int status=0;
	const char *functionName = "readOctet";
    const char *paramName = NULL;
	std::string value_s;
	registerStructuredExceptionHandler();
	getParamName(function, &paramName);
	try
	{
	    if (function < FIRST_ISISDAE_PARAM)
	    {
	        return ADDriver::readOctet(pasynUser, value, maxChars, nActual, eomReason);
	    }
	    else
	    {
	        m_iface->resetMessages();
	        // we don't do much yet
	        return asynPortDriver::readOctet(pasynUser, value, maxChars, nActual, eomReason);
        }
	}
#if 0
	try
	{
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
#endif
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

void isisdaeDriver::settingsOP(int (isisdaeInterface::*func)(const std::string&), const std::string& value, const char* err_msg)
{
	if (m_RunStatus == RS_SETUP)
	{
	    beginStateTransition(RS_CHANGING);
		(m_iface->*func)(value);
		setRunState(RS_SETUP);
        endStateTransition();
	}
	else
	{
		throw std::runtime_error(err_msg);
	}
}

asynStatus isisdaeDriver::writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName = NULL;
	registerStructuredExceptionHandler();
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
	try
	{
	    m_iface->resetMessages();
        if (function == P_RunTitleSP)
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
			settingsOP(&isisdaeInterface::setDAESettingsXML, value_s, "Can only change DAE settings in SETUP");
		}
        else if (function == P_HardwarePeriodsSettings)
		{
			settingsOP(&isisdaeInterface::setHardwarePeriodsSettingsXML, value_s, "Can only change Hardware period settings in SETUP");			
		}
        else if (function == P_UpdateSettings)
		{
			settingsOP(&isisdaeInterface::setUpdateSettingsXML, value_s, "Can only change Update settings in SETUP");			
		}
        else if (function == P_TCBSettings)
		{
			if (m_RunStatus == RS_SETUP)
			{
		        beginStateTransition(RS_CHANGING);
		        std::string tcb_xml;
		        if (uncompressString(value_s, tcb_xml) == 0)
			    {
                    size_t found = tcb_xml.find_last_of(">");  // in cased junk on end
                    m_iface->setTCBSettingsXML(tcb_xml.substr(0,found+1));
			    }
                endStateTransition();
			}
			else
			{
				throw std::runtime_error("Can only change TCB settings in SETUP");			
			}
		}        
        else if (function == P_SnapshotCRPT)
		{
		    beginStateTransition(RS_STORING);
			m_iface->snapshotCRPT(value_s, 1, 1);
            endStateTransition();
		}        
        else if (function == P_vetoEnable)
		{
			m_iface->setVeto(value_s, true);
		}        
        else if (function == P_vetoDisable)
		{
			m_iface->setVeto(value_s, false);
		}        
    
		reportMessages();
	    if (function < FIRST_ISISDAE_PARAM)
		{
		    status = ADDriver::writeOctet(pasynUser, value_s.c_str(), value_s.size(), nActual);
		}
		else
		{
		    status = asynPortDriver::writeOctet(pasynUser, value_s.c_str(), value_s.size(), nActual); // update parameters and do callbacks
		}
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, value_s.c_str());
        if (status == asynSuccess && *nActual == value_s.size())
        {
		    *nActual = maxChars;   // to keep result happy in case we skipped an embedded trailing NULL when creating value_s
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
	catch(...)
	{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s, error=unknow exception", 
                  driverName, functionName, status, function, paramName, value_s.c_str());
		reportErrors("unknown exception");
		callParamCallbacks(); // this flushes P_ErrMsgs
        endStateTransition();
		*nActual = 0;
		return asynError;
	}
}

/// vector insert helper for use with m_disallowedStateCommand
template <typename T>
std::vector<T>& operator<<(std::vector<T>& vec, const T& val)
{
	vec.push_back(val);
	return vec;
}

/// Constructor for the isisdaeDriver class.
/// Calls constructor for the asynPortDriver base class.
/// \param[in] dcomint DCOM interface pointer created by lvDCOMConfigure()
/// \param[in] portName @copydoc initArg0
isisdaeDriver::isisdaeDriver(isisdaeInterface* iface, const char *portName, int ndet)
   : ADDriver(portName, 
                    (ndet < 1 ? 1 : ndet), /* maxAddr */
                    NUM_ISISDAE_PARAMS,
					0, // maxBuffers
					0, // maxMemory
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynFloat64ArrayMask | asynOctetMask,  /* Interrupt mask */
                    ASYN_CANBLOCK | ASYN_MULTIDEVICE, /* asynFlags.  This driver can block and is multi-device from the point of view of area detector live views */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0),	/* Default stack size*/
					m_iface(iface), m_RunStatus(0), m_vetopc(0.0), m_inStateTrans(false), m_pRaw(NULL)
{					
	int i;
	int status = 0;
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
    createParam(P_SEWaitString, asynParamInt32, &P_SEWait);
	createParam(P_RunStatusString, asynParamInt32, &P_RunStatus);
    createParam(P_TotalCountsString, asynParamInt32, &P_TotalCounts);
    
	createParam(P_DAETypeString, asynParamInt32, &P_DAEType);
	createParam(P_IsMuonDAEString, asynParamInt32, &P_IsMuonDAE);
    
    createParam(P_RunTitleString, asynParamOctet, &P_RunTitle);
    createParam(P_RunTitleSPString, asynParamOctet, &P_RunTitleSP);
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
    createParam(P_MEventsString, asynParamFloat64, &P_MEvents);
    createParam(P_CountRateString, asynParamFloat64, &P_CountRate);
    createParam(P_CountRateFrameString, asynParamFloat64, &P_CountRateFrame);
    createParam(P_EventModeFractionString, asynParamFloat64, &P_EventModeFraction);
    createParam(P_EventModeBufferUsedFractionString, asynParamFloat64, &P_EventModeBufferUsedFraction);
	createParam(P_EventModeFileMBString, asynParamFloat64, &P_EventModeFileMB);
	createParam(P_EventModeDataRateString, asynParamFloat64, &P_EventModeDataRate);

    createParam(P_StateTransString, asynParamInt32, &P_StateTrans);
    createParam(P_inChangingStateString, asynParamInt32, &P_inChangingState);
	
    createParam(P_SampleParString, asynParamOctet, &P_SamplePar);
    createParam(P_BeamlineParString, asynParamOctet, &P_BeamlinePar);

    createParam(P_wiringTableFileString, asynParamOctet, &P_wiringTableFile);
    createParam(P_detectorTableFileString, asynParamOctet, &P_detectorTableFile);
    createParam(P_spectraTableFileString, asynParamOctet, &P_spectraTableFile);
    createParam(P_tcbFileString, asynParamOctet, &P_tcbFile);
    createParam(P_periodsFileString, asynParamOctet, &P_periodsFile);

    createParam(P_VMEReadValuePropsString, asynParamInt32Array, &P_VMEReadValueProps);
    createParam(P_VMEReadValueDataString, asynParamInt32, &P_VMEReadValueData);
    createParam(P_VMEWriteValueString, asynParamInt32Array, &P_VMEWriteValue);
    createParam(P_VMEReadArrayPropsString, asynParamInt32Array, &P_VMEReadArrayProps);
    createParam(P_VMEReadArrayDataString, asynParamInt32Array, &P_VMEReadArrayData);
    createParam(P_VMEWriteArrayString, asynParamInt32Array, &P_VMEWriteArray);
    createParam(P_QXReadArrayPropsString, asynParamInt32Array, &P_QXReadArrayProps);
    createParam(P_QXReadArrayDataString, asynParamInt32Array, &P_QXReadArrayData);
    createParam(P_QXWriteArrayString, asynParamInt32Array, &P_QXWriteArray);
	
    m_directRWProps[P_VMEReadValueProps].resize(3, 0);  // card, address, word_size
    m_directRWProps[P_VMEReadArrayProps].resize(3, 0); // card, address, num_values
    m_directRWProps[P_QXReadArrayProps].resize(4, 0); // card, address, num_values, trans_type
	
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
	
	createParam(P_integralsEnableString, asynParamInt32, &P_integralsEnable); 
	createParam(P_integralsSpecStartString, asynParamInt32, &P_integralsSpecStart); 
	createParam(P_integralsSpecModeString, asynParamInt32, &P_integralsSpecMode);
	createParam(P_integralsTransformModeString, asynParamInt32, &P_integralsTransformMode); 
	createParam(P_integralsModeString, asynParamInt32, &P_integralsMode); 
	createParam(P_integralsUpdateRateString, asynParamFloat64, &P_integralsUpdateRate); 
	createParam(P_integralsCountRateString, asynParamFloat64, &P_integralsCountRate); 
	createParam(P_integralsSpecCountRateString, asynParamFloat64, &P_integralsSpecCountRate); 
	createParam(P_integralsSpecMaxString, asynParamFloat64, &P_integralsSpecMax); 
	createParam(P_integralsDataModeString, asynParamInt32, &P_integralsDataMode); 
	createParam(P_integralsTMinString, asynParamFloat64, &P_integralsTMin); 
	createParam(P_integralsTMaxString, asynParamFloat64, &P_integralsTMax); 
	createParam(P_integralsPeriodString, asynParamInt32, &P_integralsPeriod); 
	createParam(P_integralsSpecMapString, asynParamOctet, &P_integralsSpecMap); 
	
	createParam(P_simulationModeString, asynParamInt32, &P_simulationMode); 
	
	createParam(P_vetoEnableString, asynParamOctet, &P_vetoEnable);
	createParam(P_vetoDisableString, asynParamOctet, &P_vetoDisable);
	createParam(P_vetoFramesExt0String, asynParamInt32, &P_vetoFramesExt0); 
	createParam(P_vetoFramesExt1String, asynParamInt32, &P_vetoFramesExt1); 
	createParam(P_vetoFramesExt2String, asynParamInt32, &P_vetoFramesExt2); 
	createParam(P_vetoFramesExt3String, asynParamInt32, &P_vetoFramesExt3); 
	createParam(P_vetoNameExt0String, asynParamOctet, &P_vetoNameExt0); 
	createParam(P_vetoNameExt1String, asynParamOctet, &P_vetoNameExt1); 
	createParam(P_vetoNameExt2String, asynParamOctet, &P_vetoNameExt2); 
	createParam(P_vetoNameExt3String, asynParamOctet, &P_vetoNameExt3); 
	createParam(P_vetoPCExt0String, asynParamFloat64, &P_vetoPCExt0); 
	createParam(P_vetoPCExt1String, asynParamFloat64, &P_vetoPCExt1); 
	createParam(P_vetoPCExt2String, asynParamFloat64, &P_vetoPCExt2); 
	createParam(P_vetoPCExt3String, asynParamFloat64, &P_vetoPCExt3); 
    
    createParam(P_blockSpecZeroString, asynParamInt32, &P_blockSpecZero);

    // list commands that are not allowed when you are in the given run state
	m_disallowedStateCommand[RS_SETUP] << P_AbortRun << P_EndRun << P_PauseRun << P_ResumeRun;
	m_disallowedStateCommand[RS_RUNNING] << P_BeginRun << P_BeginRunEx << P_ResumeRun;
	m_disallowedStateCommand[RS_PAUSED] << P_BeginRun << P_BeginRunEx << P_PauseRun;
	m_disallowedStateCommand[RS_WAITING] = m_disallowedStateCommand[RS_RUNNING];
	m_disallowedStateCommand[RS_VETOING] = m_disallowedStateCommand[RS_RUNNING];

    setIntegerParam(P_StateTrans, 0);
    setIntegerParam(P_simulationMode, 0);
	setIntegerParam(P_diagEnable, 0);
	setIntegerParam(P_DAEType, DAEType::UnknownDAE);
    setIntegerParam(P_IsMuonDAE, 0);
    setIntegerParam(P_blockSpecZero, 0);
    zeroRunCounters(false); // may avoid some PVS coming up in undefined alarm?

    // area detector defaults
//	NDDataType_t dataType = NDUInt16;
	NDDataType_t dataType = NDFloat32;
//	NDDataType_t dataType = NDUInt8;
	for(int i=0; i<ndet; ++i)
	{
		status =  setStringParam (i, ADManufacturer, "STFC ISIS");
		status |= setStringParam (i, ADModel, "ISISDAE");
		status |= setIntegerParam(i, ADMaxSizeX, 1);
		status |= setIntegerParam(i, ADMaxSizeY, 1);
		status |= setIntegerParam(i, ADMinX, 0);
		status |= setIntegerParam(i, ADMinY, 0);
		status |= setIntegerParam(i, ADBinX, 1);
		status |= setIntegerParam(i, ADBinY, 1);
		status |= setIntegerParam(i, ADReverseX, 0);
		status |= setIntegerParam(i, ADReverseY, 0);
		status |= setIntegerParam(i, ADSizeX, 1);
		status |= setIntegerParam(i, ADSizeY, 1);
		status |= setIntegerParam(i, NDArraySizeX, 1);
		status |= setIntegerParam(i, NDArraySizeY, 1);
		status |= setIntegerParam(i, NDArraySize, 1);
		status |= setIntegerParam(i, NDDataType, dataType);
		status |= setIntegerParam(i, ADImageMode, ADImageContinuous);
		status |= setIntegerParam(i, ADStatus, ADStatusIdle);
		status |= setIntegerParam(i, ADAcquire, 0);
		status |= setDoubleParam (i, ADAcquireTime, .001);
		status |= setDoubleParam (i, ADAcquirePeriod, .005);
		status |= setIntegerParam(i, ADNumImages, 100);
		status |= setIntegerParam(i, P_integralsSpecStart, 1);
		status |= setIntegerParam(i, P_integralsSpecMode, 0);
		status |= setIntegerParam(i, P_integralsTransformMode, 0);
		status |= setIntegerParam(i, P_integralsEnable, 0);
		status |= setIntegerParam(i, P_integralsMode, 0);
		status |= setIntegerParam(i, P_integralsDataMode, 0);
		status |= setDoubleParam(i, P_integralsUpdateRate, 0.0);
		status |= setDoubleParam(i, P_integralsCountRate, 0.0);
		status |= setDoubleParam(i, P_integralsSpecCountRate, 0.0);
		status |= setDoubleParam(i, P_integralsSpecMax, 0.0);
		status |= setDoubleParam(i, P_integralsTMin, 0.0);
		status |= setDoubleParam(i, P_integralsTMax, 0.0);
		status |= setIntegerParam(i, P_integralsPeriod, 0);
		status |= setStringParam (i, P_integralsSpecMap, "");
	}
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
	waitForIOCRunning();
    isisdaeDriver* driver = (isisdaeDriver*)arg;
	if (driver != NULL)
	{
	    driver->pollerThread1();
	}
}

void isisdaeDriver::pollerThreadC2(void* arg)
{ 
	waitForIOCRunning();
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	if (driver != NULL)
	{
	    driver->pollerThread2();
	}
}

void isisdaeDriver::pollerThreadC3(void* arg)
{ 
	waitForIOCRunning();
    isisdaeDriver* driver = (isisdaeDriver*)arg; 
	if (driver != NULL)
	{
	    driver->pollerThread3();
	}
}

void isisdaeDriver::pollerThreadC4(void* arg)
{ 
	waitForIOCRunning();
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
    double delay = (m_iface->checkOption(daeSECI) ? 3.0 : 0.2);

	registerStructuredExceptionHandler();
	while(true)
	{
		epicsThreadSleep(delay);
 		epicsGuard<isisdaeDriver> _lock(*this);
        try
        {
			m_iface->checkConnection();
			updateRunStatus();
        }
        catch(const std::exception& ex)
        {
            std::cerr << "updateRunStatus exception: " << ex.what() << std::endl;
        }
        catch(...)
        {
            std::cerr << "updateRunStatus exception" << std::endl;
        }
        callParamCallbacks();        
        ++counter;
    }
}

void isisdaeDriver::updateRunStatus()
{
        static frame_uamp_state fu_state, r_fu_state, p_fu_state;
		static const char* no_check_frame_uamp = macEnvExpand("$(NOCHECKFUAMP=)");
		static unsigned long old_tc, old_frames;
		static epicsTime old_tc_ts(epicsTime::getCurrent());
		
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
        unsigned long tc = m_iface->getTotalCounts();
		epicsTime tc_ts(epicsTime::getCurrent());
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
		setIntegerParam(P_TotalCounts, tc);
        setDoubleParam(P_MEvents, static_cast<double>(tc) / 1.0e6);
		double tdiff = static_cast<double>(tc_ts - old_tc_ts);
		if (tdiff > 0.5)
		{
		    if (frames <= old_frames || tc <= old_tc)
		    {
                setDoubleParam(P_CountRate, 0.0);
                setDoubleParam(P_CountRateFrame, 0.0);
		    }
			else
			{
                setDoubleParam(P_CountRate, static_cast<double>(tc - old_tc) / tdiff * 3600.0 / 1e6); // to make million events per hour			
                setDoubleParam(P_CountRateFrame, static_cast<double>(tc - old_tc) / static_cast<double>(frames - old_frames));
			}
		    old_tc_ts = tc_ts;
		    old_tc = tc;
		    old_frames = frames;
		}
        setIntegerParam(P_GoodFramesTotal, frames);
//        setIntegerParam(P_GoodFramesPeriod, p_frames);
		setIntegerParam(P_RawFramesTotal, r_frames);
		setIntegerParam(P_RunStatus, m_RunStatus);
        ///@todo need to update P_RawFramesPeriod, P_RunDurationTotal, P_TotalUAmps, P_RunDurationPeriod, P_MonitorCounts
}

// zero counters st start of run, done early before actual readbacks
void isisdaeDriver::zeroRunCounters(bool do_callbacks)
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
        setDoubleParam(P_MEvents, 0.0);
        setDoubleParam(P_CountRate, 0.0);
        setDoubleParam(P_CountRateFrame, 0.0);
        setIntegerParam(P_vetoFramesExt0, 0);
        setIntegerParam(P_vetoFramesExt1, 0);
        setIntegerParam(P_vetoFramesExt2, 0);
        setIntegerParam(P_vetoFramesExt3, 0);
        setDoubleParam(P_vetoPCExt0, 0.0);
        setDoubleParam(P_vetoPCExt1, 0.0);
        setDoubleParam(P_vetoPCExt2, 0.0);
        setDoubleParam(P_vetoPCExt3, 0.0);
        if (do_callbacks) {
            callParamCallbacks();
        }
}

void isisdaeDriver::pollerThread2()
{
    static const char* functionName = "isisdaePoller2";
	std::map<std::string, DAEValue> values;
    unsigned long counter = 0;
	double delay = (m_iface->checkOption(daeSECI) ? 5.0 : 2.0);
	double beam_current;

    long this_rf = 0, this_gf = 0, last_rf = 0, last_gf = 0;
	double frames_diff;
	long last_ext_veto[4] = { 0, 0, 0, 0 };
    bool check_settings;
    long dae_type = DAEType::UnknownDAE;
	static const std::string sim_mode_title("(DAE SIMULATION MODE)"); // prefix added by ICP if simulation mode enabled in icp_config.xml
    std::string daeSettings;
    std::string tcbSettings, tcbSettingComp;
    std::string hardwarePeriodsSettings;
    std::string updateSettings;
    std::string vetoStatus;
	std::vector<std::string> veto_names, veto_aliases;
	std::vector<long> veto_enabled, veto_frames;

	registerStructuredExceptionHandler();
	while(true)
	{
		epicsThreadSleep(delay);
 		epicsGuard<isisdaeDriver> _lock(*this);
        if (m_inStateTrans)   // do nothing if in state transition
        {
            continue;
        }
        check_settings = ( (counter == 0) || (m_RunStatus == RS_SETUP && counter % 2 == 0) || (counter % 10 == 0) );
        try
        {
			m_iface->checkConnection();
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
		    m_iface->getVetoInfo(veto_names, veto_aliases, veto_enabled, veto_frames);
		    this_rf = m_iface->getRawFrames();
            if (dae_type == DAEType::UnknownDAE)
            {
                dae_type = m_iface->getDAEType();
                setIntegerParam(P_DAEType, dae_type);
                if (dae_type == DAEType::MuonDAE2 || dae_type == DAEType::MuonDAE3)
                {
                    setIntegerParam(P_IsMuonDAE, 1);
                }
                else
                {
                    setIntegerParam(P_IsMuonDAE, 0);
                }
            }
        }
        catch(const std::exception& ex)
        {
            std::cerr << "pollerThread2 exception: " << ex.what() << std::endl;
            continue;
        }
        catch(...)
        {
            std::cerr << "pollerThread2 exception" << std::endl;
            continue;
        }
        if (this_rf > last_rf)
        {
			frames_diff = static_cast<double>(this_rf - last_rf); 
            m_vetopc = 100.0 * (1.0 - static_cast<double>(this_gf - last_gf) / frames_diff);
        }
        else
        {
            m_vetopc = 0.0;
			frames_diff = -1e10;  // make -ve so (now - last / frames) is small positive
         }
        last_rf = this_rf;
        last_gf = this_gf;
		
		for(int i=0; i<veto_names.size(); ++i)
		{
			if (veto_names[i] == "EXT0")
			{
				setIntegerParam(P_vetoFramesExt0, veto_frames[i]);
				setDoubleParam(P_vetoPCExt0, 100.0 * static_cast<double>(veto_frames[i] - last_ext_veto[0]) / frames_diff);
				setStringParam(P_vetoNameExt0, veto_aliases[i]);
				last_ext_veto[0] = veto_frames[i];
			}
			if (veto_names[i] == "EXT1")
			{
				setIntegerParam(P_vetoFramesExt1, veto_frames[i]);
				setDoubleParam(P_vetoPCExt1, 100.0 * static_cast<double>(veto_frames[i] - last_ext_veto[1]) / frames_diff);
				last_ext_veto[1] = veto_frames[i];
				setStringParam(P_vetoNameExt1, veto_aliases[i]);
			}
			if (veto_names[i] == "EXT2")
			{
				setIntegerParam(P_vetoFramesExt2, veto_frames[i]);
				setDoubleParam(P_vetoPCExt2, 100.0 * static_cast<double>(veto_frames[i] - last_ext_veto[2]) / frames_diff);
				setStringParam(P_vetoNameExt2, veto_aliases[i]);
				last_ext_veto[2] = veto_frames[i];
			}
			if (veto_names[i] == "EXT3")
			{
				setIntegerParam(P_vetoFramesExt3, veto_frames[i]);
				setDoubleParam(P_vetoPCExt3, 100.0 * static_cast<double>(veto_frames[i] - last_ext_veto[3]) / frames_diff);
				setStringParam(P_vetoNameExt3, veto_aliases[i]);
				last_ext_veto[3] = veto_frames[i];
			}
		}
		
		// strip simulation mode prefix from title and instead set simulation PV
		std::string title(values["RunTitle"]);
		if ( !title.compare(0, sim_mode_title.size(), sim_mode_title) )
		{
			title.erase(0, sim_mode_title.size());
			// ICP adds an extra space after prefix if title non-zero size
			if (title.size() > 0 && title[0] == ' ')
			{
				title.erase(0, 1);
			}
            setIntegerParam(P_simulationMode, 1);
		}
		else
		{
            setIntegerParam(P_simulationMode, 0);			
		}
        setStringParam(P_RunTitle, title.c_str());
		
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
        
        beam_current = values["BeamCurrent"];
		if (beam_current > 0.0)
		{
            setDoubleParam(P_BeamCurrent, beam_current);
		}
		else
		{
            setDoubleParam(P_BeamCurrent, 0.0);
		}

        setDoubleParam(P_TotalUAmps, values["TotalUAmps"]);
        setDoubleParam(P_MonitorFrom, values["MonitorFrom"]);
        setDoubleParam(P_MonitorTo, values["MonitorTo"]);
//        setDoubleParam(P_MEvents, values["TotalDAECounts"]);  // now updated in separate loop
//        setDoubleParam(P_CountRate, values["CountRate"]); // now updated in separate loop
        setDoubleParam(P_EventModeFraction, values["EventModeCardFraction"]);
		setDoubleParam(P_EventModeBufferUsedFraction, values["EventModeBufferUsedFraction"]);
		setDoubleParam(P_EventModeFileMB, values["EventModeFileMB"]);
		setDoubleParam(P_EventModeDataRate, values["EventModeDataRate"]);
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
		try {
		    m_iface->getAsyncMessages(messages);
		}
        catch(const std::exception& ex)
        {
            std::cerr << "getAsyncMessages exception: " << ex.what() << std::endl;
        }
        catch(...)
        {
            std::cerr << "getAsyncMessages exception" << std::endl;
        }
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
	double delay = (m_iface->checkOption(daeSECI) ? 5.0 : 2.0);
	std::vector<long> sums[2], max_vals, spec_nums;
	std::vector<double> rate;
	int frames[2] = {0, 0}, period = 1, first_spec = 1, num_spec = 10, spec_type = 0, nmatch;
	double time_low = 0.0, time_high = -1.0;
	bool b = true;
	int i1, i2, n1, sum, fdiff, fdiff_min = 0, diag_enable = 0;

 	registerStructuredExceptionHandler();
    this->lock();
	setIntegerParam(P_diagFrames, 0);
	setIntegerParam(P_diagSum, 0);
	setIntegerParam(P_diagSpecMatch, 0);
    callParamCallbacks();
    this->unlock();
	// read sums alternately into sums[0] and sums[1] by toggling b so a count rate can be calculated
	while(true)
	{
		epicsThreadSleep(delay);
		i1 = (b == true ? 0 : 1);
		i2 = (b == true ? 1 : 0);
		epicsGuard<isisdaeDriver> _lock(*this);
		getIntegerParam(P_diagEnable, &diag_enable);
		if (diag_enable != 1)
			continue;
		try
		{
			epicsGuardRelease<isisdaeDriver> _unlock(_lock); // read without lock in case icp busy
			frames[i1] = m_iface->getGoodFrames(); // read prior to lock in case ICP busy
		}
		catch(const std::exception& ex)
		{
			std::cerr << "isisdaeDriver::pollerThread3(detector diagnostics): " << ex.what() << std::endl;
			frames[i1] = 0;
			continue;
		}
		catch(...) // catch windows Structured Exceptions
		{
			std::cerr << "isisdaeDriver::pollerThread3(detector diagnostics) generic exception" << std::endl;
			frames[i1] = 0;
			continue;
		}
		fdiff = frames[i1] - frames[i2];
		if (fdiff < 0) { // likely means new run started
			frames[i2] = 0;
			std::fill(sums[i2].begin(), sums[i2].end(), 0);
			fdiff = frames[i1];
		}
		getIntegerParam(P_diagMinFrames, &fdiff_min);
		if (fdiff < fdiff_min)
			continue;
		getIntegerParam(P_diagSpecShow, &spec_type);
		getIntegerParam(P_diagSpecStart, &first_spec);
		getIntegerParam(P_diagSpecNum, &num_spec);
		getIntegerParam(P_diagPeriod, &period);
		getDoubleParam(P_diagSpecIntLow, &time_low);
		getDoubleParam(P_diagSpecIntHigh, &time_high);
		
		try
		{
			epicsGuardRelease<isisdaeDriver> _unlock(_lock); // getSpectraSum may take a while so release asyn lock
			m_iface->getSpectraSum(period, first_spec, num_spec, spec_type,
				time_low, time_high, sums[i1], max_vals, spec_nums);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "isisdaeDriver::pollerThread3(detector diagnostics): " << ex.what() << std::endl;
			continue;
		}
		catch(...) // catch windows Structured Exceptions
		{
			std::cerr << "isisdaeDriver::pollerThread3(detector diagnostics) generic exception" << std::endl;
			continue;
		}
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
		// spec_array is padded with -1 at end if less than requested matched
		nmatch = std::count_if(spec_nums.begin(), spec_nums.end(), std::bind2nd(std::greater_equal<int>(),0));
		setIntegerParam(P_diagSpecMatch, nmatch);
        callParamCallbacks();
		b = !b;
    }
}

void isisdaeDriver::pollerThread4()
{
    static const char* functionName = "isisdaePoller4";
	int acquiring, enable, data_mode;
	int all_acquiring, all_enable;
    int status = asynSuccess;
    int imageCounter;
    int numImages, numImagesCounter;
    int imageMode;
    int arrayCallbacks;
    NDArray *pImage;
    double acquireTime, acquirePeriod, delay, updateTime;
    epicsTimeStamp startTime, endTime;
    double elapsedTime, maxval;
    std::vector<int> old_acquiring(maxAddr, 0);
	std::vector<epicsTimeStamp> last_update(maxAddr);
	long totalCntsDiff, maxSpecCntsDiff;

	registerStructuredExceptionHandler();
	memset(&(last_update[0]), 0, maxAddr * sizeof(epicsTimeStamp));	
	while(true)
	{
		all_acquiring = all_enable = 0;
		for(int i=0; i<maxAddr; ++i)
		{
		    epicsGuard<isisdaeDriver> _lock(*this);
			try 
			{
				acquiring = enable = 0;
				getIntegerParam(i, ADAcquire, &acquiring);
				getIntegerParam(i, P_integralsEnable, &enable);
				getIntegerParam(i, P_integralsDataMode, &data_mode);
				getDoubleParam(i, ADAcquirePeriod, &acquirePeriod);
				
				all_acquiring |= acquiring;
				all_enable |= enable;
				if (acquiring == 0 || enable == 0)
				{
					old_acquiring[i] = acquiring;
	//				epicsThreadSleep( acquirePeriod );
					continue;
				}
				if (old_acquiring[i] == 0)
				{
					setIntegerParam(i, ADNumImagesCounter, 0);
					old_acquiring[i] = acquiring;
				}
				setIntegerParam(i, ADStatus, ADStatusAcquire); 
				epicsTimeGetCurrent(&startTime);
				getIntegerParam(i, ADImageMode, &imageMode);

				/* Get the exposure parameters */
				getDoubleParam(i, ADAcquireTime, &acquireTime);  // not really used

				setShutter(i, ADShutterOpen);
				callParamCallbacks(i, i);
				
				/* Update the image */
				status = computeImage(i, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode);

	//            if (status) continue;

				// could sleep to make up to acquireTime
			
				/* Close the shutter */
				setShutter(i, ADShutterClosed);
			
				setIntegerParam(i, ADStatus, ADStatusReadout);
				/* Call the callbacks to update any changes */
				callParamCallbacks(i, i);

				pImage = this->pArrays[i];
				if (pImage == NULL)
				{
					continue;
				}

				/* Get the current parameters */
				getIntegerParam(i, NDArrayCounter, &imageCounter);
				getIntegerParam(i, ADNumImages, &numImages);
				getIntegerParam(i, ADNumImagesCounter, &numImagesCounter);
				getIntegerParam(i, NDArrayCallbacks, &arrayCallbacks);
				++imageCounter;
				++numImagesCounter;
				setIntegerParam(i, NDArrayCounter, imageCounter);
				setIntegerParam(i, ADNumImagesCounter, numImagesCounter);

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
				  epicsGuardRelease<isisdaeDriver> _unlock(_lock);
				  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
						"%s:%s: calling imageData callback addr %d\n", driverName, functionName, i);
				  doCallbacksGenericPointer(pImage, NDArrayData, i);
				}
				epicsTimeGetCurrent(&endTime);
				elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
				updateTime = epicsTimeDiffInSeconds(&endTime, &(last_update[i]));
				if (updateTime > 0.0)
				{
					setDoubleParam(i, P_integralsUpdateRate, 1.0 / updateTime);
					setDoubleParam(i, P_integralsCountRate, totalCntsDiff / updateTime);
					setDoubleParam(i, P_integralsSpecCountRate, maxSpecCntsDiff / updateTime);
				}
				else
				{
					setDoubleParam(i, P_integralsUpdateRate, 0.0);
					setDoubleParam(i, P_integralsCountRate, 0.0);
					setDoubleParam(i, P_integralsSpecCountRate, 0.0);
				}
				setDoubleParam(i, P_integralsSpecMax, maxval);
				last_update[i] = endTime;
				/* Call the callbacks to update any changes */
				callParamCallbacks(i, i);
				/* sleep for the acquire period minus elapsed time. */
				delay = acquirePeriod - elapsedTime;
				asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
						"%s:%s: delay=%f\n",
						driverName, functionName, delay);
				if (delay >= 0.0) {
					/* We set the status to waiting to indicate we are in the period delay */
					setIntegerParam(i, ADStatus, ADStatusWaiting);
					callParamCallbacks(i, i);
					{
						epicsGuardRelease<isisdaeDriver> _unlock(_lock);
						epicsThreadSleep(delay);
					}
					setIntegerParam(i, ADStatus, ADStatusIdle);
					callParamCallbacks(i, i);  
				}
			}
			catch(...)
			{
				std::cerr << "Exception in pollerThread4" << std::endl;
			}
        }
		if (all_enable == 0 || all_acquiring == 0)
		{
			epicsThreadSleep(1.0);
		}
		else
		{
			epicsThreadSleep(1.0);
		}
	}
}

/** Computes the new image data */
int isisdaeDriver::computeImage(int addr, double& maxval, long& totalCntsDiff, long& maxSpecCntsDiff, int data_mode)
{
    int status = asynSuccess;
    NDDataType_t dataType;
    int itemp, period;
    int binX, binY, minX, minY, sizeX, sizeY, reverseX, reverseY;
    int xDim=0, yDim=1, colorDim=-1;
    int resetImage;
    int spec_start, trans_mode, maxSizeX, maxSizeY;
    int colorMode;
    int ndims=0;
    NDDimension_t dimsOut[3];
    size_t dims[3];
    NDArrayInfo_t arrayInfo;
    NDArray *pImage;
    const char* functionName = "computeImage";

    /* NOTE: The caller of this function must have taken the mutex */

    status |= getIntegerParam(addr, ADBinX,         &binX);
    status |= getIntegerParam(addr, ADBinY,         &binY);
    status |= getIntegerParam(addr, ADMinX,         &minX);
    status |= getIntegerParam(addr, ADMinY,         &minY);
    status |= getIntegerParam(addr, ADSizeX,        &sizeX);
    status |= getIntegerParam(addr, ADSizeY,        &sizeY);
    status |= getIntegerParam(addr, ADReverseX,     &reverseX);
    status |= getIntegerParam(addr, ADReverseY,     &reverseY);
    status |= getIntegerParam(addr, ADMaxSizeX,     &maxSizeX);
    status |= getIntegerParam(addr, ADMaxSizeY,     &maxSizeY);
    status |= getIntegerParam(addr, NDColorMode,    &colorMode);
    status |= getIntegerParam(addr, NDDataType,     &itemp); 
    status |= getIntegerParam(addr, P_integralsSpecStart, &spec_start); 
    status |= getIntegerParam(addr, P_integralsTransformMode, &trans_mode); 
    status |= getIntegerParam(addr, P_integralsPeriod, &period); 
	dataType = (NDDataType_t)itemp;
	if (status)
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
			"%s:%s: error getting parameters\n",
			driverName, functionName);
		return (status);
	}

    // this is for TOFChannel mode, x axis is time of flight
    // minX == 0 and binX == 1 anyway as we use ROI on GUI 
    if (data_mode == 1)
    {
        int ntc = 0;
        status |= getIntegerParam(0, P_NumTimeChannels, &ntc);
        if (maxSizeX != ntc + 1)
        {
            maxSizeX = ntc + 1;
            status |= setIntegerParam(addr, ADMaxSizeX, maxSizeX);
        }
        if (sizeX != ntc + 1)
        {
            sizeX = ntc + 1;
            status |= setIntegerParam(addr, ADSizeX, sizeX);
        }
    }

    /* Make sure parameters are consistent, fix them if they are not */
    if (binX < 1) {
        binX = 1;
        status |= setIntegerParam(addr, ADBinX, binX);
    }
    if (binY < 1) {
        binY = 1;
        status |= setIntegerParam(addr, ADBinY, binY);
    }
    if (minX < 0) {
        minX = 0;
        status |= setIntegerParam(addr, ADMinX, minX);
    }
    if (minY < 0) {
        minY = 0;
        status |= setIntegerParam(addr, ADMinY, minY);
    }
    if (minX > maxSizeX-1) {
        minX = maxSizeX-1;
        status |= setIntegerParam(addr, ADMinX, minX);
    }
    if (minY > maxSizeY-1) {
        minY = maxSizeY-1;
        status |= setIntegerParam(addr, ADMinY, minY);
    }
    if (minX+sizeX > maxSizeX) {
        sizeX = maxSizeX-minX;
        status |= setIntegerParam(addr, ADSizeX, sizeX);
    }
    if (minY+sizeY > maxSizeY) {
        sizeY = maxSizeY-minY;
        status |= setIntegerParam(addr, ADSizeY, sizeY);
    }

	if (status)
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
			"%s:%s: error setting parameters\n",
			driverName, functionName);
		return (status);
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
            status |= computeArray<epicsInt8>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDUInt8:
            status |= computeArray<epicsUInt8>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDInt16:
            status |= computeArray<epicsInt16>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDUInt16:
            status |= computeArray<epicsUInt16>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDInt32:
            status |= computeArray<epicsInt32>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDUInt32:
            status |= computeArray<epicsUInt32>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDInt64:
            status |= computeArray<epicsInt64>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDUInt64:
            status |= computeArray<epicsUInt64>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDFloat32:
            status |= computeArray<epicsFloat32>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
            break;
        case NDFloat64:
            status |= computeArray<epicsFloat64>(addr, spec_start, trans_mode, maxSizeX, maxSizeY, maxval, totalCntsDiff, maxSpecCntsDiff, data_mode, period);
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
    if (this->pArrays[addr]) this->pArrays[addr]->release();
    status = this->pNDArrayPool->convert(m_pRaw,
                                         &this->pArrays[addr],
                                         dataType,
                                         dimsOut);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating buffer in convert()\n",
                    driverName, functionName);
        return(status);
    }
    pImage = this->pArrays[addr];
    pImage->getInfo(&arrayInfo);
    status = asynSuccess;
    status |= setIntegerParam(addr, NDArraySize,  (int)arrayInfo.totalBytes);
    status |= setIntegerParam(addr, NDArraySizeX, (int)pImage->dims[xDim].size);
    status |= setIntegerParam(addr, NDArraySizeY, (int)pImage->dims[yDim].size);
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error setting parameters\n",
                    driverName, functionName);
    return(status);
}

template <typename epicsType> 
void isisdaeDriver::computeColour(double value, double maxval, double& scaled_maxval, epicsType& mono)
{
	epicsType limit = std::numeric_limits<epicsType>::max();
	if (maxval > (double)limit)
	{
		mono = static_cast<epicsType>(value / maxval * (double)limit);
	}
	else
	{
		mono = static_cast<epicsType>(value);
	}
	if (mono > scaled_maxval)
	{
		scaled_maxval = mono;
	}
}

static double myround(double d)
{
    return (d < 0.0) ? ceil(d - 0.5) : floor(d + 0.5);
}

template <typename epicsType> 
void isisdaeDriver::computeColour(double value, double maxval, epicsType& red, epicsType& green, epicsType& blue)
{
	int i;
	epicsType limit = std::numeric_limits<epicsType>::max();
	if (maxval > 0.0)
	{
		i = (int)myround(255.0 * value / maxval);
	}
	else
	{
		i = 0;
	}
	red = static_cast<epicsType>((double)RainbowColorR[i] / 255.0 * (double)limit);
	green = static_cast<epicsType>((double)RainbowColorG[i] / 255.0 * (double)limit);
	blue = static_cast<epicsType>((double)RainbowColorB[i] / 255.0 * (double)limit);
}

#if 0
void transpose(float *src, float *dst, const int N, const int M) {
    int i, j, n;
    for(n = 0; n<N*M; n++) {
        i = n/N;
        j = n%N;
        dst[n] = src[M*j + i];
    }
}
#endif

template <typename epicsType> 
int isisdaeDriver::computeArray(int addr, int spec_start, int trans_mode, int maxSizeX, int maxSizeY, double& maxval, long& totalCntsDiff, long& maxSpecCntsDiff, int data_mode, int period)
{
    epicsType *pMono=NULL, *pRed=NULL, *pGreen=NULL, *pBlue=NULL;
    int columnStep=0, rowStep=0, colorMode, numSpec;
    int status = asynSuccess;
    double exposureTime;
    int i, j, k, integMode, integSpecMode, numPeriods;
	long cntDiff;
    static const int INIT_NDET = 12;
	uint64_t cntSum, oldCntSum; 
	struct Point
	{
		short x;
		short y;
	    Point() : x(-1), y(-1) { }
	};
	double intgTMax(0.0), intgTMin(0.0);
	std::string spec_map_file;

	static std::vector<uint32_t*> old_integrals(INIT_NDET, NULL);
	static std::vector<uint32_t*> new_integrals(INIT_NDET, NULL);
	static std::vector<int> old_nspec(INIT_NDET, 0);
	static std::vector<int> old_map_start(INIT_NDET, 0);
	static std::vector<std::string> old_spec_map_file(INIT_NDET, "");
	static std::vector< std::vector<Point> > spec_map(INIT_NDET);
	static std::vector<int> old_period(INIT_NDET, 0);    
    if (addr >= old_integrals.size()) {
	    old_integrals.resize(addr + 1, NULL);
	    new_integrals.resize(addr + 1, NULL);
	    old_nspec.resize(addr + 1, 0);
	    old_map_start.resize(addr + 1, 0);
	    old_spec_map_file.resize(addr + 1, "");
	    spec_map.resize(addr + 1);
	    old_period.resize(addr + 1, 0);
    }

	maxval = 0.0;
	totalCntsDiff =  maxSpecCntsDiff = 0;
    
    status = getIntegerParam(addr, NDColorMode,   &colorMode);
    status = getDoubleParam (addr, ADAcquireTime, &exposureTime);
	status = getIntegerParam(addr, P_integralsMode,  &integMode);
	status = getIntegerParam(addr, P_integralsSpecMode, &integSpecMode);
    status = getDoubleParam(addr, P_integralsTMax, &intgTMax);
	status = getDoubleParam(addr, P_integralsTMin, &intgTMin);
	status = getStringParam(addr, P_integralsSpecMap, spec_map_file);
	status = getIntegerParam(0, P_NumSpectra,  &numSpec);
	status = getIntegerParam(0, P_NumPeriods, &numPeriods);
	if (period <= 0 || period > numPeriods)
	{
	    status = getIntegerParam(0, P_Period, &period);
	}
    if (old_spec_map_file[addr] != spec_map_file)
	{
		old_spec_map_file[addr] = spec_map_file;
		old_map_start[addr] = 0;
		std::fstream fs;
		fs.open(spec_map_file);
		if (fs.good())
		{
			int map_start, map_end;
			fs >> map_start >> map_end;
			int n = map_end - map_start + 1;
			spec_map[addr].resize(n);
			old_map_start[addr] = map_start;
			for(int i=0; i<n; ++i)
			{
				spec_map[addr][i].x = -1;
				spec_map[addr][i].y = -1;
			}
			while(fs.good())
			{
				int spec;
				short ix, iy;
				fs >> spec >> ix >> iy;
				if (fs.good())
				{
				    spec -= map_start;
				    if (spec >= 0 && spec < n)
				    {
				        spec_map[addr][spec].x = ix;
				        spec_map[addr][spec].y = iy;
				    }
				    else
				    {
					    std::cerr << "map out of range" << std::endl;
				    }
				}
			}
		}
	}
	
	// in data_mode == 1 nspec here will be time channels * real_spectra as X is time channels number and Y is spectrum number
	int nspec = maxSizeX * maxSizeY;
	if (spec_map_file.size() > 0)
	{
		spec_start = old_map_start[addr];
		nspec = spec_map[addr].size();
	}
	// adjust start spectrum for period
	// periods start at 1 in user world, also numSpec+1 as we have spectra from 0 to numSpec in each period
	spec_start += (period - 1) * (numSpec + 1);

	// if data_mode == 1 spec_start needs adjusting for time channel X axis
	if (data_mode == 1)
	{
	    spec_start *= maxSizeX;
	}

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
            rowStep = 2 * maxSizeX;
            pRed   = (epicsType *)m_pRaw->pData;
            pGreen = (epicsType *)m_pRaw->pData + maxSizeX;
            pBlue  = (epicsType *)m_pRaw->pData + 2*maxSizeX;
            break;
        case NDColorModeRGB3:
            columnStep = 1;
            rowStep = 0;
            pRed   = (epicsType *)m_pRaw->pData;
            pGreen = (epicsType *)m_pRaw->pData + maxSizeX*maxSizeY;
            pBlue  = (epicsType *)m_pRaw->pData + 2*maxSizeX*maxSizeY;
            break;
    }
    m_pRaw->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

	const uint32_t* integrals = NULL;
	int max_spec_int_size = 0;
	if ( (data_mode == 0) && ((spec_start + nspec) > (numSpec + 1) * numPeriods) )
	{
		nspec = 0;
	}
	if (integSpecMode == 1)
	{
		try {
			max_spec_int_size = m_iface->getEventSpecIntegralsSize(); // in case numPeriods or numSpec changes just use same default as event mode
			if (new_integrals[addr] == NULL)
			{
				new_integrals[addr] = new uint32_t[max_spec_int_size];
			}
	        if ( (spec_start + nspec) > max_spec_int_size )
	        {
		        nspec = 0;
	        }
			m_iface->updateCRPTSpectra(1, spec_start, nspec);
			std::vector<long> counts;
			memset(new_integrals[addr] + spec_start, 0, nspec * sizeof(uint32_t));
			m_iface->getSpectrumIntegral2(spec_start, nspec, 1, intgTMin, intgTMax, counts);
			if ( (sizeof(long) == sizeof(uint32_t)) && (counts.size() > 0) )
			{
				memcpy(new_integrals[addr] + spec_start, &(counts[0]), counts.size() * sizeof(long));
			}
			integrals = new_integrals[addr];
		}
		catch(...) {
			return status;
		}
	}
	else
	{
		try {
			if (data_mode == 0)
			{
				integrals = m_iface->getEventSpecIntegrals();
				max_spec_int_size = m_iface->getEventSpecIntegralsSize();
			}
			else if (data_mode == 1)
			{
				integrals = m_iface->getData();
				max_spec_int_size = m_iface->getDataSize();
			}
		}
		catch(...) {
			return status;
		}
	}
	if ( (spec_start + nspec) > max_spec_int_size )
	{
		nspec = 0;
	}
	if (integrals == NULL || nspec <= 0 || spec_start < 0)
	{
		return status;
	}
	if (old_nspec[addr] != nspec)
	{
		if (old_integrals[addr] != NULL)
		{
			delete []old_integrals[addr];
		}
		old_integrals[addr] = new uint32_t[nspec];
		old_nspec[addr] = nspec;
		memcpy(old_integrals[addr], integrals + spec_start, nspec * sizeof(uint32_t));
	}
	if (old_period[addr] != period) // this is so we don't get an incorrect count rate on period change as old_integrals is for wrong period
	{
		old_period[addr] = period;
		memcpy(old_integrals[addr], integrals + spec_start, nspec * sizeof(uint32_t));
	}
	double* dintegrals = new double[maxSizeX * maxSizeY];
	for(i=0; i < (maxSizeX * maxSizeY); ++i)
	{
		dintegrals[i] = 0.0;		
	}
	for(i=0; i<nspec; ++i)
	{
		int k;
		// use int64_t so can both be signed and also hold uint32_t fully
		int64_t integ = (integMode == 0 ? integrals[i + spec_start] : integrals[i + spec_start] -  old_integrals[addr][i]);
		if (integ < 0)
		{
			integ = 0;
		}
	    if (spec_map_file.size() > 0)
		{
		    if ( (i >= spec_map[addr].size()) || (spec_map[addr][i].x == -1) )
				continue;
			k = spec_map[addr][i].x + spec_map[addr][i].y * maxSizeX;
		}
		else
		{
			k = i;
		}
		if (k >= maxSizeX * maxSizeY)
		{
			continue;
		}
	    if (trans_mode == 0)
		{
		    dintegrals[k] = static_cast<double>(integ);
	    }
	    else if (trans_mode == 1)
		{
		    dintegrals[k] = sqrt(static_cast<double>(integ));
	    }
	    else if (trans_mode == 2)
	    {
		    dintegrals[k] = log(static_cast<double>(1 + integ));
	    }
		if (dintegrals[k] > maxval)
		{
			maxval = dintegrals[k];
		}
	}
	cntSum = oldCntSum = 0; 
	for (i=0; i<nspec; ++i) 
	{
		if ( (cntDiff = integrals[i + spec_start] - old_integrals[addr][i]) > maxSpecCntsDiff )
		{
			maxSpecCntsDiff = cntDiff;
		}
		cntSum += integrals[i + spec_start];
		oldCntSum += old_integrals[addr][i];
	}
	totalCntsDiff = (cntSum > oldCntSum ? cntSum - oldCntSum : 0);
	memcpy(old_integrals[addr], integrals + spec_start, nspec * sizeof(uint32_t));
	double scaled_maxval = 0.0;
    k = 0;
	for (i=0; i<maxSizeY; i++) {
		switch (colorMode) {
			case NDColorModeMono:
				for (j=0; j<maxSizeX; j++) {
					computeColour(dintegrals[k], maxval, scaled_maxval, *pMono);
					++pMono;
					++k;
				}
				break;
			case NDColorModeRGB1:
			case NDColorModeRGB2:
			case NDColorModeRGB3:
				for (j=0; j<maxSizeX; j++) {
					computeColour(dintegrals[k], maxval, *pRed, *pGreen, *pBlue);
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
	if (scaled_maxval != 0.0)
	{
		maxval = scaled_maxval;
	}
	delete []dintegrals;
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
void isisdaeDriver::setShutter(int addr, int open)
{
    int shutterMode;

    getIntegerParam(addr, ADShutterMode, &shutterMode);
    if (shutterMode == ADShutterModeDetector) {
        /* Simulate a shutter by just changing the status readback */
        setIntegerParam(addr, ADShutterStatus, open);
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

static exServer *pCAS = NULL;
static unsigned fdManagerProcCount = 0;

static void daeCASThread(void* arg)
{
    unsigned    debugLevel = 0u;
    double      executionTime = 0.0;
    double      asyncDelay = 0.1;
    const char*        pvPrefix;
    unsigned    aliasCount = 1u;
    unsigned    scanOn = true;
    unsigned    syncScan = false;
    unsigned    maxSimultAsyncIO = 1000u;

	isisdaeDriver::waitForIOCRunning();

	isisdaeInterface* iface = static_cast<isisdaeInterface*>(arg);
    printf("CAS: starting cas server\n");
	registerStructuredExceptionHandler();
	pvPrefix = macEnvExpand("$(MYPVPREFIX)DAE:");

    int dae_type = -1;
    while(dae_type == -1) {
        try {
	        dae_type = iface->getDAEType();
        }
        catch(...) {
            dae_type = -1;
            std::cerr << "CAS: waiting for DAE" << std::endl;
            epicsThreadSleep(30);
        }
    }
    for(bool done = false; !done; ) {
        try {
            pCAS = new exServer ( pvPrefix, aliasCount, 
                scanOn != 0, syncScan == 0, asyncDelay,
                maxSimultAsyncIO, iface, dae_type);
            done = true;
        }
        catch(const std::exception& ex) {
            errlogSevPrintf (errlogMajor, "CAS: Server initialization error %s\n", ex.what());
            errlogFlush ();
            epicsThreadSleep(30);
        }
        catch(...) {
            errlogSevPrintf (errlogMajor, "CAS: Server initialization error\n" );
            errlogFlush ();
            epicsThreadSleep(30);
        }
    }
    
    pCAS->setDebugLevel(debugLevel);
    while (true) 
    {
	    try {
            fileDescriptorManager.process(0.1);
		}
        catch(const std::exception& ex) {
            std::cerr << "CAS: daeCASThread exception: " << ex.what() << std::endl;
			epicsThreadSleep(5.0);
		}
        catch(...) {
            std::cerr << "CAS: daeCASThread exception" << std::endl;
			epicsThreadSleep(5.0);
		}
		++fdManagerProcCount;
    }
    errlogSevPrintf (errlogMajor, "CAS: daeCASThread exiting\n" );
    //pCAS->show(2u);
    delete pCAS;
	pCAS = NULL;
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

void isisdaeShowPCAS(int level)
{
	if (pCAS != NULL)
	{
		std::cerr << "CAS: fdManagerProcCount = " << fdManagerProcCount << std::endl;
	    pCAS->show(level);
	}
	else
	{
		std::cerr << "CAS: channel access server is not running" << std::endl;
	}
}

/// EPICS iocsh callable function to call constructor of lvDCOMInterface().
/// \param[in] portName @copydoc initArg0
/// \param[in] configSection @copydoc initArg1
/// \param[in] configFile @copydoc initArg2
/// \param[in] host @copydoc initArg3
/// \param[in] options @copydoc initArg4
/// \param[in] progid @copydoc initArg5
/// \param[in] username @copydoc initArg6
/// \param[in] password @copydoc initArg7
int isisdaeConfigure(const char *portName, int options, const char *host, const char* username, const char* password, int ndet)
{
	registerStructuredExceptionHandler();
	try
	{
		isisdaeInterface* iface = new isisdaeInterface(host, options, username, password);
		if (iface != NULL)
		{
			isisdaeDriver* driver = new isisdaeDriver(iface, portName, ndet);
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

volatile bool isisdaeDriver::daeIOCisRunning = false;

int isisdaePCASDebug = 0;

void isisdaeDriver::waitForIOCRunning()
{
	while (!daeIOCisRunning)
	{
		epicsThreadSleep(1.0);
	}
}

/*
 * INITHOOKS
 *
 * called by iocInit at various points during initialization
 *
 */

 /* If this function (initHooks) is loaded, iocInit calls this function
  * at certain defined points during IOC initialization */
static void daeInitHooks(initHookState state)
{
	switch (state) {
	case initHookAtIocBuild:
		break;
	case initHookAtBeginning:
		break;
	case initHookAfterCallbackInit:
		break;
	case initHookAfterCaLinkInit:
		break;
	case initHookAfterInitDrvSup:
		break;
	case initHookAfterInitRecSup:
		break;
	case initHookAfterInitDevSup: // Autosave pass 0 uses this
		break;
	case initHookAfterInitDatabase: // Autosave pass 1 uses this
		break;
	case initHookAfterFinishDevSup:
		break;
	case initHookAfterScanInit:
		break;
	case initHookAfterInitialProcess: // PINI processing happens just before this
		break;
	case initHookAfterCaServerInit:
		break;
	case initHookAfterIocBuilt:
		break;
	case initHookAtIocRun:
		break;
	case initHookAfterDatabaseRunning:
		break;
	case initHookAfterCaServerRunning:
		break;
	case initHookAfterIocRunning:
		isisdaeDriver::daeIOCisRunning = true;
		break;
	default:
		break;
	}
	return;
}

// EPICS iocsh shell commands 

// isisdaeConfigure

static const iocshArg initArg0 = { "portName", iocshArgString};			///< The name of the asyn driver port we will create
static const iocshArg initArg1 = { "options", iocshArgInt};			    ///< options as per #lvDCOMOptions enum
static const iocshArg initArg2 = { "host", iocshArgString};				///< host name where LabVIEW is running ("" for localhost) 
static const iocshArg initArg3 = { "username", iocshArgString};			///< (optional) remote username for host #initArg3
static const iocshArg initArg4 = { "password", iocshArgString};			///< (optional) remote password for username #initArg6 on host #initArg3
static const iocshArg initArg5 = { "ndet", iocshArgInt};			    ///< options as per #lvDCOMOptions enum

static const iocshArg * const initArgs[] = { &initArg0,
                                             &initArg1,
                                             &initArg2,
                                             &initArg3,
                                             &initArg4,
                                             &initArg5 };

static const iocshFuncDef initFuncDef = {"isisdaeConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    isisdaeConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].sval, args[4].sval, args[5].ival);
}

// isisdaeShowPCAS

static const iocshArg initArg0PC = { "level", iocshArgInt };

static const iocshArg * const initArgsPC[] = { &initArg0PC };

static const iocshFuncDef initFuncDefPC = { "isisdaeShowPCAS", sizeof(initArgsPC) / sizeof(iocshArg*), initArgsPC };

static void initCallFuncPC(const iocshArgBuf *args)
{
	isisdaeShowPCAS(args[0].ival);
}

static void isisdaeRegister(void)
{
	initHookRegister(daeInitHooks);
	iocshRegister(&initFuncDef, initCallFunc);
	iocshRegister(&initFuncDefPC, initCallFuncPC);
}

epicsExportRegistrar(isisdaeRegister);
epicsExportAddress(int, isisdaePCASDebug);

#include <registryFunction.h>
#include <aSubRecord.h>
#include <menuFtype.h>

static long parseSettingsXML(aSubRecord *prec)
{
	const char* xml_in = (const char*)(prec->a); /* waveform CHAR data */
	epicsOldString* tag_in = (epicsOldString*)(prec->b); /* string */
	epicsOldString* type_in = (epicsOldString*)(prec->c); /* string */
    char* value_out_wf = (char*)(prec->vala);
    epicsOldString* value_out_str = (epicsOldString*)(prec->valb);
    
    if (!((prec->fta == menuFtypeCHAR || prec->fta == menuFtypeUCHAR) && prec->ftb == menuFtypeSTRING && prec->ftc == menuFtypeSTRING && 
        prec->ftva == menuFtypeCHAR && prec->ftvb == menuFtypeSTRING))
	{
         errlogPrintf("%s incorrect input data type.\n", prec->name);
		 return -1;
	}
    // calculate number of elements, but do not assume NULL termination
    int nelements;
    for(nelements=0; nelements<prec->noa && xml_in[nelements] != '\0'; ++nelements)
        ;
    if (nelements == 0) {
         errlogPrintf("%s no input XML - OK if this only happens once on IOC startup.\n", prec->name);
		 return -1;
    }
	std::string val, xml_in_str(xml_in, nelements);
    char xpath[256];
    prec->nevb = 1;
    try {
        epicsSnprintf(xpath, sizeof(xpath), "/Cluster/%s[Name='%s']/Val", *type_in, *tag_in);
        if (xml_in_str.find("<Cluster>") != std::string::npos) {
	        isisdaeDriver::getDAEXML(xml_in_str, xpath, val);
        } else {
            std::string uncomp_xml;
		    if (uncompressString(xml_in_str, uncomp_xml) == 0)
			{
                size_t found = uncomp_xml.find_last_of(">");  // in cased junk on end
                isisdaeDriver::getDAEXML(uncomp_xml.substr(0,found+1), xpath, val);
			}
        }
        strncpy(value_out_wf, val.c_str(), prec->nova);
        prec->neva = (val.size() > prec->nova ? prec->nova : val.size());
        strncpy(*value_out_str, val.c_str(), sizeof(epicsOldString));
    }
    catch(const std::exception& ex)
    {
        memset(value_out_wf, 0, prec->nova);
        memset(*value_out_str, 0, sizeof(epicsOldString));
        prec->neva = 0;
        errlogPrintf("%s exception in XML for tag %s type %s: %s.\n", prec->name, *tag_in, *type_in, ex.what());
        return -1;
    }
    return 0; /* process output links */
}

epicsRegisterFunction(parseSettingsXML);

}
