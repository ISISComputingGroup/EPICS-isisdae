/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
//  Example EPICS CA server
//
//
//  caServer
//  |
//  exServer
//
//  casPV
//  |
//  exPV-----------
//  |             |
//  exScalarPV    exVectorPV
//  |
//  exAsyncPV
//
//  casChannel
//  |
//  exChannel
//


//
// ANSI C
//
#include <string.h>
#include <stdio.h>

// C++
#include <list>
#include <map>
#include <iostream>
#include <atomic>

//
// EPICS
//
#include "gddAppFuncTable.h"
#include "smartGDDPointer.h"
#include "epicsTimer.h"
#include "casdef.h"
#include "epicsAssert.h"
#include "resourceLib.h"
#include "tsMinMax.h"

#include "boost/function.hpp"
#include "boost/bind.hpp"


#ifndef NELEMENTS
#   define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
bool parseSpecPV(const std::string& pvStr, int& spec, int& period, std::string& axis, std::string& field);
bool parseMonitorPV(const std::string& pvStr, int& mon, int& period, std::string& axis, std::string& field);
//int parseSamplePV(const std::string& pvStr, std::string& param); 
//int parseBeamlinePV(const std::string& pvStr, std::string& param); 

//
// info about all pv in this server
//
//enum excasIoType { excasIoSync, excasIoAsync };

class exPV;
class exServer;
class isisdaeInterface;

//
// pvInfo 
// 
class pvInfo {
public: 
        
    pvInfo ( double scanPeriodIn, const  char * pNameIn, 
        aitFloat32  hoprIn, aitFloat32  loprIn,  const char* unitsIn, aitEnum typeIn,  
        unsigned  countIn);
    pvInfo (  const pvInfo  & copyIn  );
    ~pvInfo ();
    double getScanPeriod () const; 
    const  char * getName () const; 
	double getHopr () const; 
    double getLopr () const;
    const char* getUnits() const;	
    aitEnum getType () const; 
//    excasIoType  getIOType () const; 
    unsigned getElementCount () const; 
    void unlinkPV (); 
    void deletePV ();
    exPV *getPV();
	void setPV(exPV *pNewPV);
	
private:
    const double scanPeriod;
    std::string pName;
    const double hopr;
    const double lopr;
	const std::string units;
    aitEnum type;
//    const excasIoType ioType;
    const unsigned elementCount;
    exPV * pPV;
    pvInfo & operator = ( const pvInfo & );
};

//
// pvEntry 
//
// o entry in the string hash table for the pvInfo
// o Since there may be aliases then we may end up
// with several of this class all referencing 
// the same pv info class (justification
// for this breaking out into a seperate class
// from pvInfo)
//
class pvEntry // X aCC 655
    : public stringId, public tsSLNode < pvEntry > {
public:
    pvEntry ( pvInfo  &infoIn, exServer & casIn, 
            const char * pAliasName );
    ~pvEntry();
    pvInfo & getInfo() const { return this->info; }
    void destroy ();

private:
    pvInfo & info;
    exServer & cas;
    pvEntry & operator = ( const pvEntry & );
    pvEntry ( const pvEntry & );
};

//
// special gddDestructor guarantees same form of new and delete
//
class exFixedStringDestructor: public gddDestructor {
    virtual void run (void *);
};


//
// exPV
//
class exPV : public casPV, public epicsTimerNotify, 
    public tsSLNode < exPV > {
public:
    exPV ( exServer & cas, pvInfo & setup, 
        bool preCreateFlag, bool scanOn, bool asyncIO );
    virtual ~exPV();

    void show ( unsigned level ) const;

    //
    // Called by the server libary each time that it wishes to
    // subscribe for PV the server tool via postEvent() below.
    //
    caStatus interestRegister ();

    //
    // called by the server library each time that it wishes to
    // remove its subscription for PV value change events
    // from the server tool via caServerPostEvents()
    //
    void interestDelete ();

    aitEnum bestExternalType () const;

    //
    // chCreate() is called each time that a PV is attached to
    // by a client. The server tool must create a casChannel object
    // (or a derived class) each time that this routine is called
    //
    // If the operation must complete asynchronously then return
    // the status code S_casApp_asyncCompletion and then
    // create the casChannel object at some time in the future
    //
    //casChannel *createChannel ();

    //
    // This gets called when the pv gets a new value
    //
    caStatus update ( const gdd & );

    //
    // Gets called when we add noise to the current value
    //
    virtual void scan () = 0;
    
    //
    // If no one is watching scan the PV with 10.0
    // times the specified period
    //
    double getScanPeriod () const;

    caStatus read ( const casCtx &, gdd & protoIn );
    caStatus doRead ( const casCtx &, gdd & protoIn );

    caStatus readNoCtx ( smartGDDPointer pProtoIn );

    caStatus write ( const casCtx &, const gdd & value );

    void destroy ();

    const pvInfo & getPVInfo ();

    const char * getName() const;

    static void initFT();

    casChannel * createChannel ( const casCtx &ctx,
        const char * const pUserName, 
        const char * const pHostName );

protected:
	epicsMutex scanLock;
	epicsMutex timerLock;
    smartGDDPointer pValue;
    exServer & cas;
    epicsTimer & timer;
    pvInfo & info; 
    std::atomic<bool> interest;
    bool preCreate;
    bool scanOn;
	bool m_asyncIO;
    static epicsTime currentTime;

    virtual caStatus updateValue ( const gdd & ) = 0;
    virtual gddAppFuncTableStatus getEnumsImpl(gdd &value);

private:

    //
    // scan timer expire
    //
    expireStatus expire ( const epicsTime & currentTime );
    void doScan();

	epicsEvent timerDone; // a timer has fired and completed a scan
	epicsTimeStamp lastScan; // last time we ran ran a scan
	
    //
    // Std PV Attribute fetch support
    //
    gddAppFuncTableStatus getPrecision(gdd &value);
    gddAppFuncTableStatus getHighLimit(gdd &value);
    gddAppFuncTableStatus getLowLimit(gdd &value);
    gddAppFuncTableStatus getUnits(gdd &value);
    gddAppFuncTableStatus getValue(gdd &value);
    gddAppFuncTableStatus getEnums(gdd &value);

    exPV & operator = ( const exPV & );
    exPV ( const exPV & );

    //
    // static
    //
    static gddAppFuncTable<exPV> ft;
    static char hasBeenInitialized;
};

//
// exScalarPV
//
class exScalarPV : public exPV {
public:
    exScalarPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, bool asyncIO ) :
        exPV ( cas, setup, 
            preCreateFlag, scanOnIn, asyncIO) {}
    void scan();
	virtual bool getNewValue(smartGDDPointer& pDD) = 0;  // return true if value changed and monitors shoul;d be signalled
private:
    caStatus updateValue ( const gdd & );
    exScalarPV & operator = ( const exScalarPV & );
    exScalarPV ( const exScalarPV & );
};

// number of array elements PV
class NORDPV : public exScalarPV {
public:
    NORDPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, int& nord );
	virtual bool getNewValue(smartGDDPointer& pDD);
private:
    int& m_nord;
    NORDPV & operator = ( const NORDPV & );
    NORDPV ( const NORDPV & );
};

class CountsPV : public exScalarPV {
public:
    CountsPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, int spec, int period );
	virtual bool getNewValue(smartGDDPointer& pDD);
protected:
    int m_spec;  // so can be updated by MonitorCountsPV subclass 
private:
	int m_period;
    CountsPV & operator = ( const CountsPV & );
    CountsPV ( const CountsPV & );
};

class MonitorCountsPV : public CountsPV {
public:
    MonitorCountsPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, int mon, int period );
	virtual bool getNewValue(smartGDDPointer& pDD);
private:
	int m_monitor;
    MonitorCountsPV & operator = ( const MonitorCountsPV & );
    MonitorCountsPV ( const MonitorCountsPV & );
    bool updateSpectrum();
};

template <typename T>
class FixedValuePV : public exScalarPV {
public:
    FixedValuePV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, const T& value);
	virtual bool getNewValue(smartGDDPointer& pDD);
private:
    const T m_value;
	bool m_first_call;
    FixedValuePV & operator = ( const FixedValuePV & );
    FixedValuePV ( const FixedValuePV & );
};

class NoAlarmPV : public FixedValuePV<aitEnum16> {
public:
    NoAlarmPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn);
    virtual caStatus getEnumsImpl ( gdd & enumsIn );
private:
    NoAlarmPV & operator = ( const NoAlarmPV & );
    NoAlarmPV ( const NoAlarmPV & );
};
    
class MonLookupPV : public exScalarPV {
public:
    MonLookupPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, int monitor );
	virtual bool getNewValue(smartGDDPointer& pDD);
	static MonLookupPV* create(exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, int monitor) { return new MonLookupPV(cas, setup, preCreateFlag, scanOnIn, monitor); }
private:
    int m_monitor;
    MonLookupPV & operator = ( const MonLookupPV & );
    MonLookupPV ( const MonLookupPV & );
};

//
// exVectorPV
//
class exVectorPV : public exPV {
public:
    exVectorPV ( exServer & cas, pvInfo &setup, 
        bool preCreateFlag, bool scanOnIn, bool asyncIO ) :
        exPV ( cas, setup, 
            preCreateFlag, scanOnIn, asyncIO) {  m_size = this->info.getElementCount(); }
    void scan();
	virtual bool getNewValue(smartGDDPointer& pDD) = 0;  // return true if value changed and monitors shoul;d be signalled


    unsigned maxDimension() const;
    aitIndex maxBound (unsigned dimension) const;

private:
    caStatus updateValue ( const gdd & );
    exVectorPV & operator = ( const exVectorPV & );
    exVectorPV ( const exVectorPV & );
	int m_size;
};

//
// special gddDestructor guarantees same form of new and delete
//
class exVecDestructor: public gddDestructor {
    virtual void run (void *);
};


class SpectrumPV : public exVectorPV {
public:
    SpectrumPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, const std::string& axis, int spec, int period);
	virtual bool getNewValue(smartGDDPointer& pDD);
    int& getNORD() { return m_nord; }
    int& getMAXVAL() { return m_maxval; }
    int& getMINVAL() { return m_minval; }
protected:
    int m_spec; // so can be updated by MonitorSpectrumPV subclass  
private:
	std::string m_axis;
	int m_period;
	int m_nord;
	int m_maxval;
	int m_minval;
    SpectrumPV & operator = ( const SpectrumPV & );
    SpectrumPV ( const SpectrumPV & );
};

class MonitorSpectrumPV : public SpectrumPV {
public:
    MonitorSpectrumPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, const std::string& axis, int mon, int period);
	virtual bool getNewValue(smartGDDPointer& pDD);
private:
    int m_monitor;
    MonitorSpectrumPV & operator = ( const MonitorSpectrumPV & );
    MonitorSpectrumPV ( const MonitorSpectrumPV & );
    bool updateSpectrum();
};
    
class myAsyncReadIO : public casAsyncReadIO
{
	exPV& m_pv;
	gdd* m_value;
	const casCtx & m_ctx;

	public:

    myAsyncReadIO(const casCtx & ctx, gdd & protoIn, exPV& pv) : casAsyncReadIO(ctx), m_pv(pv), m_ctx(ctx)
	{
		m_value = &protoIn;
        // keep variable alive for use in readThread()
        // will unreference in destructor
		m_value->reference();
	}
		
    static void readThreadC(void* arg)
    {
		myAsyncReadIO* aio = (myAsyncReadIO*)arg;
		aio->readThread();
	}
	
    void readThread()
	{
        caStatus status, status1;
        status = m_pv.doRead(m_ctx, *m_value);
		status1 = postIOCompletion(status, *m_value);
        // at this point we will have had our destructor called for us
        // so do not reference any variables in "this"
        if (status1 != S_casApp_success)
           std::cerr << "CAS: Error returned by postIOCompletion" << std::endl;
	}
    ~myAsyncReadIO()
    {
        m_value->unreference();
        m_value = NULL;
    }


};

//
// exServer
//
class exServer : private caServer {
public:
    exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOn,
        bool asyncScan, double asyncDelay,
        unsigned maxSimultAsyncIO, isisdaeInterface* iface );
    ~exServer ();
    void show ( unsigned level ) const;
    void removeIO ();
    void removeAliasName ( pvEntry & entry );

    class epicsTimer & createTimer ();
	void setDebugLevel ( unsigned level );

    void destroyAllPV ();
    
    unsigned maxSimultAsyncIO () const;
	
	isisdaeInterface* iface() { return m_iface; }
	
	exPV* getPV(const std::string& pvName);
	
	bool createSpecPVs(const std::string& pvStr);
	bool createMonitorPVs(const std::string& pvStr);
	void createAxisPVs(bool is_monitor, int id, int period, const char* axis, const std::string& units);
	void createCountsPV(bool is_monitor, int id, int period);
    template <typename T> pvInfo* createFixedPV(const std::string& pvStr, const T& value, const char* units, aitEnum ait_type);
    pvInfo* createNoAlarmPV(const std::string& pvStr);
private:
    resTable < pvEntry, stringId > stringResTbl;
    epicsTimerQueueActive * pTimerQueue;
    unsigned simultAsychIOCount;
    const unsigned _maxSimultAsyncIO;
    double asyncDelay;
    bool scanOn;
	int m_ntc;

    void installAliasName ( pvInfo & info, const char * pAliasName );
    pvExistReturn pvExistTest ( const casCtx &, 
        const caNetAddr &, const char * pPVName );
    pvExistReturn pvExistTest ( const casCtx &, 
        const char * pPVName );
    pvAttachReturn pvAttach ( const casCtx &, 
        const char * pPVName );
        
    void createStandardPVs(const char* prefix, int period, int id, const char* axis, const std::string& units, bool is_monitor);


    exServer & operator = ( const exServer & );
    exServer ( const exServer & );

    //
    // list of PVs
    //
    std::map<std::string,pvInfo*> m_pvList;
	isisdaeInterface* m_iface;
    epicsMutex m_lock;
	std::string m_pvPrefix;
	std::string m_tof_units;
    
};

//
// exChannel
//
class exChannel : public casChannel{
public:
    exChannel ( const casCtx & ctxIn );
    void setOwner ( const char * const pUserName, 
        const char * const pHostName );
    bool readAccess () const;
    bool writeAccess () const;
private:
    exChannel & operator = ( const exChannel & );
    exChannel ( const exChannel & );
};



inline pvInfo::pvInfo ( double scanPeriodIn, const char *pNameIn, 
    aitFloat32 hoprIn, aitFloat32 loprIn, const char* unitsIn,
    aitEnum typeIn,  
    unsigned countIn) :

    scanPeriod ( scanPeriodIn ), pName ( pNameIn ), 
    hopr ( hoprIn ), lopr ( loprIn ), units ( unitsIn ), type ( typeIn ),
    elementCount ( countIn ), 
    pPV ( 0 )
{
}

//
// for use when MSVC++ will not build a default copy constructor 
// for this class
//
inline pvInfo::pvInfo ( const pvInfo & copyIn ) :

    scanPeriod ( copyIn.scanPeriod ), pName ( copyIn.pName ), 
    hopr ( copyIn.hopr ), lopr ( copyIn.lopr ), units ( copyIn.units), type ( copyIn.type ),
    elementCount ( copyIn.elementCount ),
    pPV ( copyIn.pPV )
{
}

inline pvInfo::~pvInfo ()
{
    //
    // GDD cleanup gets rid of GDD's that are in use 
    // by the PV before the file scope destructer for 
    // this class runs here so this does not seem to 
    // be a good idea
    //
    //if ( this->pPV != NULL ) {
    //   delete this->pPV;
    //}
}

inline void pvInfo::deletePV ()
{
    if ( this->pPV != NULL ) {
        delete this->pPV;
    }
}

inline double pvInfo::getScanPeriod () const 
{ 
    return this->scanPeriod; 
}

inline const char *pvInfo::getName () const 
{ 
    return this->pName.c_str(); 
}

inline double pvInfo::getHopr () const 
{ 
    return this->hopr; 
}

inline double pvInfo::getLopr () const 
{ 
    return this->lopr; 
}

inline const char* pvInfo::getUnits () const 
{ 
    return this->units.c_str(); 
}

inline aitEnum pvInfo::getType () const 
{ 
    return this->type;
}

inline unsigned pvInfo::getElementCount () const 
{ 
    return this->elementCount; 
}


inline void pvInfo::unlinkPV () 
{ 
    this->pPV = NULL; 
}

inline pvEntry::pvEntry ( pvInfo  & infoIn, exServer & casIn, 
        const char * pAliasName ) : 
    stringId ( pAliasName ), info ( infoIn ), cas ( casIn ) 
{
    assert ( this->stringId::resourceName() != NULL );
}

inline pvEntry::~pvEntry ()
{
    this->cas.removeAliasName ( *this );
}

inline void pvEntry::destroy ()
{
    delete this;
}

inline void exServer::removeAliasName ( pvEntry & entry )
{
    pvEntry * pE;
    pE = this->stringResTbl.remove ( entry );
    assert ( pE == &entry );
}

inline double exPV::getScanPeriod () const
{ 
    double scanPeriod = this->info.getScanPeriod ();
    if ( ! this->interest ) {
        scanPeriod *= 100.0L;
    }
	return scanPeriod;
}

inline caStatus exPV::readNoCtx ( smartGDDPointer pProtoIn )
{
    return this->ft.read ( *this, *pProtoIn );
}

inline const pvInfo & exPV::getPVInfo ()
{
    return this->info;
}

inline const char * exPV::getName () const
{
    return this->info.getName();
}

inline exChannel::exChannel ( const casCtx & ctxIn ) : 
    casChannel(ctxIn) 
{
}

extern "C" int isisdaePCASDebug;
