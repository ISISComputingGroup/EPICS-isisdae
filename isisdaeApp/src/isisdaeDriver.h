#ifndef ISISDAEDRIVER_H
#define ISISDAEDRIVER_H
 
#include "ADDriver.h"

class isisdaeInterface;

class isisdaeDriver : public ADDriver 
{
public:
    isisdaeDriver(isisdaeInterface* iface, const char *portName, int ndet);
 	static void pollerThreadC1(void* arg);
 	static void pollerThreadC2(void* arg);
 	static void pollerThreadC3(void* arg);
 	static void pollerThreadC4(void* arg);
    enum RunState { RS_PROCESSING=0,RS_SETUP=1,RS_RUNNING=2,RS_PAUSED=3,RS_WAITING=4,RS_VETOING=5,RS_ENDING=6,RS_SAVING=7, RS_RESUMING=8,RS_PAUSING=9,RS_BEGINNING=10,RS_ABORTING=11,RS_UPDATING=12,RS_STORING=13,RS_CHANGING=14 };
	static const char* RunStateNames[];
	static volatile bool daeIOCisRunning;
	static void waitForIOCRunning();

    // These are the methods that we override from asynPortDriver
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
	virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
	virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);
	virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
	
	void beginStateTransition(int state);
	void endStateTransition();
    virtual void report(FILE *fp, int details);
    virtual void setShutter(int addr, int open);
	void setRunState(RunState state) { m_RunStatus = state; setIntegerParam(P_RunStatus, state); }
    
    enum DAEType { UnknownDAE=0, NeutronDAE2=1, MuonDAE2=2, NeutronDAE3=3, MuonDAE3=4 };

private:

	int P_GoodUAH; // double
	#define FIRST_ISISDAE_PARAM P_GoodUAH
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
    int P_SnapshotCRPT; 
    int P_SEWait; // int
	int P_RunStatus; // int
    int P_TotalCounts; // long
    int P_RunTitle; //char*
    int P_RunTitleSP; //char*
    int P_RBNumber; //char*
    int P_UserName; //char*
    int P_InstName; //char*
    int P_RunNumber; //char*
    int P_DAEType; // int
    int P_IsMuonDAE; // int
    int P_IRunNumber; //int
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
    int P_MEvents; // double
    int P_CountRate; // double
    int P_CountRateFrame; // double
    int P_EventModeFraction; // double
	int P_EventModeBufferUsedFraction; // double
	int P_EventModeFileMB; //double
	int P_EventModeDataRate; //double
    int P_GoodFramesTotal; //long
    int P_RawFramesTotal; //long
    int P_GoodFramesPeriod; //long
    int P_RawFramesPeriod; //long
    int P_SamplePar; //string
    int P_BeamlinePar; //string
	int P_StateTrans; //long
	int P_wiringTableFile; // string
	int P_detectorTableFile; // string
	int P_spectraTableFile; // string
	int P_tcbFile; // string
	int P_periodsFile; // string
	int P_inChangingState; // int
	
	int P_diagTableSum; // int array
	int P_diagTableMax; // int array
	int P_diagTableSpec; // int array
	int P_diagTableCntRate; // float array
	int P_diagFrames; // int
	int P_diagMinFrames; // int
	int P_diagEnable;  // int 
	int P_diagPeriod; // int			
	int P_diagSpecStart; // int		
	int P_diagSpecNum; // int		
	int P_diagSpecShow; // int	
	int P_diagSpecMatch; // int			
    int P_diagSpecIntLow; //float				
    int P_diagSpecIntHigh; // float				
	int P_diagSum; // int
	int P_simulationMode; // int
	// create area detector views of intregrals of spectra in event mode
	int P_integralsSpecStart; // int
	int P_integralsSpecMode; // int
	int P_integralsTransformMode; // int
	int P_integralsEnable; // int
	int P_integralsMode; // int
	int P_integralsUpdateRate; // float
	int P_integralsCountRate; // float
	int P_integralsSpecCountRate; // float
	int P_integralsSpecMax; // float
	int P_integralsDataMode; // int
	int P_integralsTMin; // float
	int P_integralsTMax; // float
	int P_integralsPeriod; // int
	int P_integralsSpecMap; // string
	
	int P_vetoEnable;   // string
	int P_vetoDisable;   // string
	
	int P_vetoFramesExt0;   // int
	int P_vetoFramesExt1;   // int
	int P_vetoFramesExt2;   // int
	int P_vetoFramesExt3;   // int
	int P_vetoNameExt0;   // string
	int P_vetoNameExt1;   // string
	int P_vetoNameExt2;   // string
	int P_vetoNameExt3;   // string
	int P_vetoPCExt0;   // double
	int P_vetoPCExt1;   // double
	int P_vetoPCExt2;   // double
	int P_vetoPCExt3;   // double

    int P_AllMsgs; // char
    int P_ErrMsgs; // char
	#define LAST_ISISDAE_PARAM P_ErrMsgs

	isisdaeInterface* m_iface;
    int m_RunStatus;  // cached value used in poller thread
    bool m_inStateTrans; 
    float m_vetopc; // only made it a float as 32bit size is guaranteeded to be atomic on both 32 and 64bit windows   
    NDArray* m_pRaw;

	/// mapping of run state to disallowed asyn commands when in that state
	std::map< int, std::vector<int> > m_disallowedStateCommand;
	
	void pollerThread1();
	void pollerThread2();
	void pollerThread3();
	void pollerThread4();
    void zeroRunCounters();	
    void updateRunStatus();
	void reportErrors(const char* exc_text);
	void reportMessages();
	void setADAcquire(int acquire);
	int computeImage(int addr, double& maxval, long& totalCntsDiff, long& maxSpecCntsDiff, int data_mode);
    template <typename epicsType> 
	  void computeColour(double value, double maxval, double& scaled_maxval, epicsType& mono);
    template <typename epicsType> 
      void computeColour(double value, double maxval, epicsType& red, epicsType& green, epicsType& blue);
	template <typename epicsType> 
	  int computeArray(int addr, int spec_start, int trans_mode, int maxSizeX, int maxSizeY, double& maxval, long& totalCntsDiff, long& maxSpecCntsDiff, int data_mode, int period);
	
	void getDAEXML(const std::string& xmlstr, const std::string& path, std::string& value);
	static void translateBeamlineType(std::string& str);
	template<typename T> asynStatus writeValue(asynUser *pasynUser, const char* functionName, T value);
    template<typename T> asynStatus readValue(asynUser *pasynUser, const char* functionName, T* value);
    template<typename T> asynStatus readArray(asynUser *pasynUser, const char* functionName, T *value, size_t nElements, size_t *nIn);

    void settingsOP(int (isisdaeInterface::*func)(const std::string&), const std::string& value, const char* err_msg);
  
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
#define P_SnapshotCRPTString "SNAPSHOTCRPT"
#define P_SEWaitString	    "SEWAIT"
#define P_RunStatusString	"RUNSTATUS"
#define P_DAETypeString     "DAETYPE"
#define P_IsMuonDAEString         "ISMUONDAE"

#define P_TotalCountsString	"TOTALCOUNTS"
#define P_RunTitleString	"RUNTITLE"
#define P_RunTitleSPString	"RUNTITLESP"
#define P_RBNumberString	"RBNUMBER"
#define P_UserNameString	"USERNAME"
#define P_InstNameString	"INSTNAME"
#define P_RunNumberString	"RUNNUMBER"
#define P_IRunNumberString	"IRUNNUMBER"
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
#define P_MEventsString	"MEVENTS"
#define P_CountRateString	"COUNTRATE"
#define P_CountRateFrameString	"COUNTRATEFRAME"
#define P_EventModeFractionString	"EVENTMODEFRACTION"
#define P_EventModeBufferUsedFractionString "EVENTMODEBUFFERUSED"
#define P_EventModeFileMBString "EVENTMODEFILEMB"
#define P_EventModeDataRateString "EVENTMODEDATARATE"
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
#define	P_wiringTableFileString "WIRINGFILE"
#define P_detectorTableFileString "DETFILE"
#define P_spectraTableFileString "SPECFILE"
#define P_tcbFileString           "TCBFILE"
#define P_periodsFileString      "PERIODSFILE"
#define P_inChangingStateString      "INCHANGINGSTATE"

#define P_diagTableSumString		"DIAG_TABLE_SUM"
#define P_diagTableMaxString		"DIAG_TABLE_MAX"
#define P_diagTableCntRateString	"DIAG_TABLE_CNTRATE"
#define P_diagTableSpecString		"DIAG_TABLE_SPEC"
#define P_diagFramesString			"DIAG_FRAMES"
#define P_diagMinFramesString		"DIAG_MIN_FRAMES"
#define P_diagEnableString			"DIAG_ENABLE"
#define P_diagPeriodString			"DIAG_PERIOD"
#define P_diagSpecStartString		"DIAG_SPEC_START"
#define P_diagSpecNumString			"DIAG_SPEC_NUM"
#define P_diagSpecShowString		"DIAG_SPEC_SHOW"
#define P_diagSumString				"DIAG_SUM"
#define P_diagSpecMatchString				"DIAG_SPEC_MATCH"
#define P_diagSpecIntLowString				"DIAG_SPEC_INTLOW"
#define P_diagSpecIntHighString				"DIAG_SPEC_INTHIGH"

#define P_integralsSpecStartString				"INTG_SPEC_START"
#define P_integralsSpecModeString				"INTG_SPEC_MODE"
#define	P_integralsTransformModeString 			"INTG_TRANS_MODE"
#define P_integralsEnableString					"INTG_ENABLE"
#define P_integralsModeString					"INTG_MODE"
#define P_integralsUpdateRateString				"INTG_UPD_RATE"
#define P_integralsCountRateString				"INTG_RATE"
#define P_integralsSpecCountRateString			"INTG_SPEC_RATE"
#define P_integralsSpecMaxString                "INTG_SPEC_MAX"
#define P_integralsDataModeString               "INTG_DATA_MODE"
#define P_integralsTMinString               	"INTG_TMIN"
#define P_integralsTMaxString               	"INTG_TMAX"
#define P_integralsPeriodString               	"INTG_PERIOD"
#define P_integralsSpecMapString               	"INTG_SPEC_MAP"

#define P_simulationModeString					"SIM_MODE"

#define P_vetoEnableString					"VETO_ENABLE"
#define P_vetoDisableString					"VETO_DISABLE"

#define P_vetoFramesExt0String					"VETO_FRAMES_EXT0"
#define P_vetoFramesExt1String					"VETO_FRAMES_EXT1"
#define P_vetoFramesExt2String					"VETO_FRAMES_EXT2"
#define P_vetoFramesExt3String					"VETO_FRAMES_EXT3"
#define P_vetoNameExt0String					"VETO_NAME_EXT0"
#define P_vetoNameExt1String					"VETO_NAME_EXT1"
#define P_vetoNameExt2String					"VETO_NAME_EXT2"
#define P_vetoNameExt3String					"VETO_NAME_EXT3"
#define P_vetoPCExt0String					"VETO_PC_EXT0"
#define P_vetoPCExt1String					"VETO_PC_EXT1"
#define P_vetoPCExt2String					"VETO_PC_EXT2"
#define P_vetoPCExt3String					"VETO_PC_EXT3"

#define P_AllMsgsString	"ALLMSGS"
#define P_ErrMsgsString	"ERRMSGS"

#endif /* ISISDAEDRIVER_H */
