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
// Example EPICS CA server
//
#include <iostream>
#include "exServer.h"
#include "gddApps.h"

//
// static data for exPV
//
char exPV::hasBeenInitialized = 0;
gddAppFuncTable<exPV> exPV::ft;
epicsTime exPV::currentTime;


//
// exPV::exPV()
//
exPV::exPV ( exServer & casIn, pvInfo & setup, 
            bool preCreateFlag, bool scanOnIn, bool asyncIO ) : 
    cas ( casIn ),
    timer ( cas.createTimer() ),
    info ( setup ),
    interest ( false ),
    preCreate ( preCreateFlag ),
    scanOn ( scanOnIn ),
	m_asyncIO(asyncIO)
{
    //
    // no dataless PV allowed
    //
    assert (this->info.getElementCount()>=1u);

	memset(&(this->lastScan), 0, sizeof(this->lastScan));

    // we no longer start a very slow background scan here
}

//
// exPV::~exPV()
//
exPV::~exPV() 
{
    this->timer.destroy ();
    this->info.unlinkPV();
}

//
// exPV::destroy()
//
// this is replaced by a noop since we are 
// pre-creating most of the PVs during init in this simple server
//
void exPV::destroy()
{
    if ( ! this->preCreate ) {
        delete this;
    }
}

//
// exPV::update()
//
caStatus exPV::update ( const gdd & valueIn )
{
#   if DEBUG
        printf("Setting %s too:\n", this->info.getName().string());
        valueIn.dump();
#   endif

    caStatus status = this->updateValue ( valueIn );
    if ( status || ( ! this->pValue.valid() ) ) {
        return status;
    }

    //
    // post a value change event
    //
    caServer * pCAS = this->getCAS();
    if ( this->interest == true && pCAS != NULL ) {
        casEventMask select ( pCAS->valueEventMask() | pCAS->logEventMask() );
        this->postEvent ( select, *this->pValue );
    }

    return S_casApp_success;
}

//
// exScanTimer::expire ()
//
epicsTimerNotify::expireStatus
exPV::expire ( const epicsTime & /*currentTime*/ ) // X aCC 361
{
    static const double sleep_delay = atof(getenv("ISISDAE_TIMER_SLEEP") != NULL ? getenv("ISISDAE_TIMER_SLEEP") : ".001");
    // only periodic scan if somebody is interested in us
    if (isisdaePCASDebug > 0) {
        std::cerr << "CAS: exPV::expire() timer expired \"" << getName() << "\"" << std::endl;
    }
    if (this->interest) {
        doScan();
    }
    this->timerDone.signal();
    epicsThreadSleep(sleep_delay); // yield thread, this is in case we have a big timer queue and start to starve DAE access
    if ( this->interest && this->scanOn && this->getScanPeriod() > 0.0 ) {
        return expireStatus ( restart, this->getScanPeriod() );
    }
    else {
        return noRestart;
    }
}

void exPV::doScan()
{
    if (isisdaePCASDebug > 0) {
        std::cerr << "CAS: exPV::doScan() updating data for \"" << getName() << "\"" << std::endl;
    }
	try 
	{
        this->scan();
	    epicsGuard<epicsMutex> _lock(timerLock);
	    epicsTimeGetCurrent(&(this->lastScan));	
	}
	catch(const std::exception& ex)
	{
		errlogSevPrintf(errlogMajor, "CAS: exPV::expire: Scan failed: %s", ex.what());
	}
	catch(...)
	{
		errlogSevPrintf(errlogMajor, "CAS: exPV::expire: Scan failed");
	}
}


// exPV::bestExternalType()
//
aitEnum exPV::bestExternalType () const
{
    return this->info.getType ();
}

//
// exPV::interestRegister()
//
caStatus exPV::interestRegister ()
{
    if ( ! this->getCAS() ) {
        return S_casApp_success;
    }

    if (isisdaePCASDebug > 0) {
        std::cerr << "CAS: exPV::interestRegister() in PV \"" << getName() << "\"" << std::endl;
    }
    if (this->interest == false) {
        this->interest = true;
        if ( this->scanOn && this->getScanPeriod() > 0.0 ) {
            this->timer.start ( *this, this->getScanPeriod() );
        }
    }

    return S_casApp_success;
}

//
// exPV::interestDelete()
//
void exPV::interestDelete()
{
    if (isisdaePCASDebug > 0) {
        std::cerr << "CAS: exPV::interestDelete() in PV \"" << getName() << "\"" << std::endl;
    }
	this->interest = false;
    // do not try and call this->timer.cancel() as this can deadlock. We have the pv lock via  casPVI::removeMonitor
    // but if the timer is running it may try and call casPVI::getExtServer (via getCAS()) causing a deadlock
}

//
// exPV::show()
//
void exPV::show ( unsigned level ) const
{
    if (level>1u) {
        if ( this->pValue.valid () ) {
            printf ( "exPV: cond=%d\n", this->pValue->getStat () );
            printf ( "exPV: sevr=%d\n", this->pValue->getSevr () );
            printf ( "exPV: value=%f\n", static_cast < double > ( * this->pValue ) );
			printf ( "exPV: scanOn=%d scanPeriod=%f\n", (this->scanOn?1:0), this->getScanPeriod() );
        }
        printf ( "exPV: interest=%d\n", (this->interest?1:0) );
        this->timer.show ( level - 1u );
    }
}

//
// exPV::initFT()
//
void exPV::initFT ()
{
    if ( exPV::hasBeenInitialized ) {
            return;
    }

    //
    // time stamp, status, and severity are extracted from the
    // GDD associated with the "value" application type.
    //
    exPV::ft.installReadFunc ("value", &exPV::getValue);
    exPV::ft.installReadFunc ("precision", &exPV::getPrecision);
    exPV::ft.installReadFunc ("graphicHigh", &exPV::getHighLimit);
    exPV::ft.installReadFunc ("graphicLow", &exPV::getLowLimit);
    exPV::ft.installReadFunc ("controlHigh", &exPV::getHighLimit);
    exPV::ft.installReadFunc ("controlLow", &exPV::getLowLimit);
    exPV::ft.installReadFunc ("alarmHigh", &exPV::getHighLimit);
    exPV::ft.installReadFunc ("alarmLow", &exPV::getLowLimit);
    exPV::ft.installReadFunc ("alarmHighWarning", &exPV::getHighLimit);
    exPV::ft.installReadFunc ("alarmLowWarning", &exPV::getLowLimit);
    exPV::ft.installReadFunc ("units", &exPV::getUnits);
    exPV::ft.installReadFunc ("enums", &exPV::getEnums);

    exPV::hasBeenInitialized = 1;
}

//
// exPV::getPrecision()
//
caStatus exPV::getPrecision ( gdd & prec )
{
    prec.put(4u);
    return S_cas_success;
}

//
// exPV::getHighLimit()
//
caStatus exPV::getHighLimit ( gdd & value )
{
    value.put(info.getHopr());
    return S_cas_success;
}

//
// exPV::getLowLimit()
//
caStatus exPV::getLowLimit ( gdd & value )
{
    value.put(info.getLopr());
    return S_cas_success;
}

//
// exPV::getUnits()
//
caStatus exPV::getUnits( gdd & units )
{
    aitString str(info.getUnits(), aitStrRefConst);
    units.put(str);
    return S_cas_success;
}

//
// exPV::getEnumsImpl()
//
// returns the eneumerated state strings
// for a discrete channel
//
// The PVs in this example are purely analog,
// and therefore this isnt appropriate in an
// analog context ...
//
caStatus exPV::getEnumsImpl ( gdd & enumsIn )
{
    if ( this->info.getType () == aitEnumEnum16 ) {
        static const unsigned nStr = 2;
        aitFixedString *str;
        exFixedStringDestructor *pDes;

        str = new aitFixedString[nStr];
        if (!str) {
            return S_casApp_noMemory;
        }

        pDes = new exFixedStringDestructor;
        if (!pDes) {
            delete [] str;
            return S_casApp_noMemory;
        }

        strncpy (str[0].fixed_string, "off", 
            sizeof(str[0].fixed_string));
        strncpy (str[1].fixed_string, "on", 
            sizeof(str[1].fixed_string));

        enumsIn.setDimension(1);
        enumsIn.setBound (0,0,nStr);
        enumsIn.putRef (str, pDes);

        return S_cas_success;
    }

    return S_cas_success;
}

caStatus exPV::getEnums ( gdd & enumsIn )
{
    return getEnumsImpl(enumsIn);
}

//
// exPV::getValue()
//
caStatus exPV::getValue ( gdd & value )
{
    caStatus status;

    if ( this->pValue.valid () ) {
        gddStatus gdds;

        gdds = gddApplicationTypeTable::
            app_table.smartCopy ( &value, & (*this->pValue) );
        if (gdds) {
            status = S_cas_noConvert;   
        }
        else {
            status = S_cas_success;
        }
    }
    else {
        status = S_casApp_undefined;
    }
    return status;
}

//
// exPV::write()
// (synchronous default)
//
caStatus exPV::write ( const casCtx &, const gdd & valueIn )
{
    return this->update ( valueIn );
}
 
//
// exPV::read()
// (synchronous default)
//
caStatus exPV::read ( const casCtx & ctx, gdd & protoIn )
{
	static const double cache_time = 0.5;   ///< time to consider a value valid for 
	// we will always be scanning values, but at a slower rate if (interest == false)
	// only forcing an update on (interest == false) means reads and monitors will always get the same value
    epicsTimeStamp now;
	epicsTimeGetCurrent(&now);
	double tdiff;
    {
		epicsGuard<epicsMutex> _lock(timerLock);
		tdiff = epicsTimeDiffInSeconds(&now, &(this->lastScan));
    }
	if ( (this->interest && this->scanOn && this->getScanPeriod() > 0.0) || (tdiff < cache_time) )
	{
        return this->ft.read ( *this, protoIn );
	}
	if (m_asyncIO)
	{
		myAsyncReadIO *pIO = new myAsyncReadIO(ctx, protoIn, *this);  // will delete itself on IO completion
        epicsThreadCreate("myAsyncReadIO", epicsThreadPriorityMedium, epicsThreadStackMedium, myAsyncReadIO::readThreadC, pIO);
		return S_casApp_asyncCompletion;
	}
	else
	{
		return doRead(ctx, protoIn);
	}
}

caStatus exPV::doRead ( const casCtx & ctx, gdd & protoIn )
{
	this->doScan();
    return this->ft.read(*this, protoIn);	
}

//
// exPV::createChannel()
//
// for access control - optional
//
casChannel *exPV::createChannel ( const casCtx &ctx,
        const char * const /* pUserName */, 
        const char * const /* pHostName */ )
{
    return new exChannel ( ctx );
}

//
// exFixedStringDestructor::run()
//
// special gddDestructor guarantees same form of new and delete
//
void exFixedStringDestructor::run ( void * pUntyped )
{
    aitFixedString *ps = (aitFixedString *) pUntyped;
    delete [] ps;
}

