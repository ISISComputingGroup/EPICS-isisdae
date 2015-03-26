#ifndef ISISDAEDRIVER_H
#define ISISDAEDRIVER_H
 
#include "asynPortDriver.h"

class isisdaeInterface;

class isisdaeDriver : public asynPortDriver 
{
public:
    isisdaeDriver(isisdaeInterface* iface, const char *portName);
 	static void pollerThreadC1(void* arg);
 	static void pollerThreadC2(void* arg);
    enum { RS_PROCESSING=0,RS_SETUP=1,RS_RUNNING=2,RS_PAUSED=3,RS_WAITING=4,RS_VETOING=5,RS_ENDING=6,RS_SAVING=7,
	        RS_RESUMING=8,RS_PAUSING=9,RS_BEGINNING=10,RS_ABORTING=11,RS_UPDATING=12,RS_STORING=13 };
                
    // These are the methods that we override from asynPortDriver
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
//	virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
//  virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
	virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);
	virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
	
	void beginStateTransition(int state);
	void endStateTransition();

private:

	int P_GoodUAH; // double
	int P_GoodUAHPeriod; // double
	int P_BeginRun; // int
    int P_BeginRunEx; // int
	int P_AbortRun; // int
    int P_PauseRun; // int
    int P_ResumeRun; // int
    int P_EndRun; // int
    int P_RecoverRun; // int
    int P_SaveRun; // int
    int P_UpdateRun; // int
    int P_StoreRun; // int
    int P_StartSEWait; // int
    int P_EndSEWait; // int
	int P_RunStatus; // int
    int P_TotalCounts; // long
    int P_RunTitle; //char*
    int P_RBNumber; //char*
    int P_UserName; //char*
    int P_InstName; //char*
    int P_RunNumber; //char*
    int P_UserTelephone; //char*
    int P_StartTime; //char*
    int P_NPRatio; //double
    int P_ISISCycle; //char*
    int P_DAETimingSource; //char*
    int P_PeriodType; //char*
    int P_DAESettings; //char*
    int P_TCBSettings; //char*
    int P_HardwarePeriodsSettings; //char*
    int P_VetoStatus; //char*
    int P_VetoPC; //double
    int P_UpdateSettings; //char*
    int P_RunDurationTotal; //long
    int P_RunDurationPeriod; //long
    int P_NumTimeChannels; //long
    int P_DAEMemoryUsed; //long
    int P_NumPeriods; // long
    int P_Period; // long
    int P_MonitorSpectrum; // long
    int P_NumSpectra; // long
    int P_MonitorCounts; // long
    int P_PeriodSequence; // long
    int P_BeamCurrent; // double
    int P_TotalUAmps; // double
    int P_MonitorFrom; // double
    int P_MonitorTo; // double
    int P_TotalDaeCounts; // double
    int P_CountRate; // double
    int P_EventModeFraction; // double
    int P_GoodFramesTotal; //long
    int P_RawFramesTotal; //long
    int P_GoodFramesPeriod; //long
    int P_RawFramesPeriod; //long
    int P_SamplePar; //string
    int P_BeamlinePar; //string
	int P_StateTrans; //long
    int P_AllMsgs; // char
    int P_ErrMsgs; // char
	#define FIRST_ISISDAE_PARAM P_GoodUAH
	#define LAST_ISISDAE_PARAM P_ErrMsgs

	isisdaeInterface* m_iface;
    int m_RunStatus;  // cached value used in poller thread
    float m_vetopc; // only made it a float as 32bit size is guaranteeded to be atomic on both 32 and 64bit windows   

	
	void pollerThread1();
	void pollerThread2();
    void zeroRunCounters();	
    void updateRunStatus();
	void reportErrors(const char* exc_text);
	void reportMessages();
	static void translateBeamlineType(std::string& str);
	template<typename T> asynStatus writeValue(asynUser *pasynUser, const char* functionName, T value);
    template<typename T> asynStatus readValue(asynUser *pasynUser, const char* functionName, T* value);
    template<typename T> asynStatus readArray(asynUser *pasynUser, const char* functionName, T *value, size_t nElements, size_t *nIn);
    
};

#define NUM_ISISDAE_PARAMS (&LAST_ISISDAE_PARAM - &FIRST_ISISDAE_PARAM + 1)

#define P_GoodUAHString	"GOODUAH"
#define P_GoodUAHPeriodString	"GOODUAHPD"
#define P_BeginRunString	"BEGINRUN"
#define P_BeginRunExString	"BEGINRUNEX"
#define P_AbortRunString	"ABORTRUN"
#define P_PauseRunString	"PAUSERUN"
#define P_ResumeRunString	"RESUMERUN"
#define P_EndRunString	"ENDRUN"
#define P_RecoverRunString	"RECOVERRUN"
#define P_SaveRunString	"SAVERUN"
#define P_UpdateRunString	"UPDATERUN"
#define P_StoreRunString	"STORERUN"
#define P_StartSEWaitString	"STARTSEWAIT"
#define P_EndSEWaitString	"ENDSEWAIT"
#define P_RunStatusString	"RUNSTATUS"

#define P_TotalCountsString	"TOTALCOUNTS"
#define P_RunTitleString	"RUNTITLE"
#define P_RBNumberString	"RBNUMBER"
#define P_UserNameString	"USERNAME"
#define P_InstNameString	"INSTNAME"
#define P_RunNumberString	"RUNNUMBER"
#define P_UserTelephoneString	"USERTELEPHONE"
#define P_StartTimeString	"STARTTIME"
#define P_NPRatioString	"NPRATIO"
#define P_ISISCycleString	"ISISCYCLE"
#define P_GoodFramesTotalString	"GOODFRAMES"
#define P_RawFramesTotalString	"RAWFRAMES"
#define P_GoodFramesPeriodString	"GOODFRAMESPD"
#define P_RawFramesPeriodString	"RAWFRAMESPD"
#define P_RunDurationTotalString	"RUNDURATION"
#define P_RunDurationPeriodString	"RUNDURATIONPD"
#define P_NumTimeChannelsString	"NUMTIMECHANNELS"
#define P_DAEMemoryUsedString	"DAEMEMORYUSED"
#define P_NumPeriodsString	"NUMPERIODS"
#define P_PeriodString	"PERIOD"
#define P_NumSpectraString	"NUMSPECTRA"
#define P_MonitorCountsString	"MONITORCOUNTS"
#define P_PeriodSequenceString	"PERIODSEQ"
#define P_BeamCurrentString	"BEAMCURRENT"
#define P_TotalUAmpsString	"TOTALUAMPS"
#define P_MonitorFromString	"MONITORFROM"
#define P_MonitorToString	"MONITORTO"
#define P_TotalDaeCountsString	"TOTALDAECOUNTS"
#define P_CountRateString	"COUNTRATE"
#define P_EventModeFractionString	"EVENTMODEFRACTION"
#define P_MonitorSpectrumString	"MONITORSPECTRUM"
#define P_DAETimingSourceString	"DAETIMINGSOURCE"
#define P_PeriodTypeString	"PERIODTYPE"
#define P_DAESettingsString	"DAESETTINGS"
#define P_TCBSettingsString	"TCBSETTINGS"
#define P_HardwarePeriodsSettingsString	"HARDWAREPERIODSSETTINGS"
#define P_UpdateSettingsString	"UPDATESETTINGS"
#define P_VetoStatusString	"VETOSTATUS"
#define P_VetoPCString	"VETOPC"
#define P_SampleParString	"SAMPLEPAR"
#define P_BeamlineParString	"BEAMLINEPAR"
#define P_StateTransString	"STATETRANS"
#define P_AllMsgsString	"ALLMSGS"
#define P_ErrMsgsString	"ERRMSGS"

#endif /* ISISDAEDRIVER_H */
