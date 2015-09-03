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
// fileDescriptorManager.process(delay);
// (the name of the global symbol has leaked in here)
//

//
// Example EPICS CA server
//
#include "exServer.h"
#include "isisdaeInterface.h"
#include "pcrecpp.h"
//
// exServer::exServer()
//
exServer::exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOnIn, 
        bool asyncScan, double asyncDelayIn,
        unsigned maxSimultAsyncIOIn, isisdaeInterface* iface) : 
        pTimerQueue ( 0 ), simultAsychIOCount ( 0u ), 
        _maxSimultAsyncIO ( maxSimultAsyncIOIn ),
        asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn ), m_iface ( iface ), m_pvPrefix(pvPrefix)
{
    unsigned i;

    exPV::initFT();
	setDebugLevel(5);

    if ( asyncScan ) {
        unsigned timerPriotity;
        epicsThreadBooleanStatus etbs = epicsThreadLowestPriorityLevelAbove (
                epicsThreadGetPrioritySelf (), & timerPriotity );
        if ( etbs != epicsThreadBooleanStatusSuccess ) {
            timerPriotity = epicsThreadGetPrioritySelf ();
        }
        this->pTimerQueue = & epicsTimerQueueActive::allocate ( false, timerPriotity );
    }

}

//
// exServer::~exServer()
//
exServer::~exServer()
{
    this->destroyAllPV ();   
    this->stringResTbl.traverse ( &pvEntry::destroy );
}

void exServer::destroyAllPV ()
{
    for (std::map<std::string,pvInfo*>::iterator it = m_pvList.begin(); it != m_pvList.end(); ++it)
	{	
        it->second->deletePV ();
    }
}

//
// exServer::installAliasName()
//
void exServer::installAliasName(pvInfo &info, const char *pAliasName)
{
    pvEntry *pEntry;

    pEntry = new pvEntry(info, *this, pAliasName);
    if (pEntry) {
        int resLibStatus;
        resLibStatus = this->stringResTbl.add(*pEntry);
        if (resLibStatus==0) {
            return;
        }
        else {
            delete pEntry;
        }
    }
    fprintf ( stderr, 
"Unable to enter PV=\"%s\" Alias=\"%s\" in PV name alias hash table\n",
        info.getName(), pAliasName );
}

//
// More advanced pvExistTest() isnt needed so we forward to
// original version. This avoids sun pro warnings and speeds 
// up execution.
//
pvExistReturn exServer::pvExistTest
	( const casCtx & ctx, const caNetAddr &, const char * pPVName )
{
	return this->pvExistTest ( ctx, pPVName );
}

//
// exServer::pvExistTest()
//
pvExistReturn exServer::pvExistTest // X aCC 361
    ( const casCtx& ctxIn, const char * pPVName )
{
    //
    // lifetime of id is shorter than lifetime of pName
    //
    stringId id ( pPVName, stringId::refString );
    pvEntry *pPVE;
    char pvAlias[256];
	std::string pvStr(pPVName);
    if (pvStr.compare(0, m_pvPrefix.size(), m_pvPrefix) != 0)
	{
        return pverDoesNotExistHere;
	}
	pvStr.erase(0, m_pvPrefix.size());

	int pvtype = getPVType(pvStr);
	if (pvtype == 0x0)
	{
        return pverDoesNotExistHere;
	}
	
	// as we create the PV on the fly, we need to protect against multiple access (and hence multiple create)
	epicsGuard<epicsMutex> _guard (m_lock);
    //
    // Look in hash table for PV name (or PV alias name)
    //
    pPVE = this->stringResTbl.lookup ( id );
    if ( ! pPVE ) {
		pvInfo *pPVI = NULL;
		int ntc = 8000; //m_iface->getNumTimeChannels(spec);
		// pvInfo is given the "interested" scan period (i.e. with monitors) - this is multipled by 10 for the "uninterested case"
        switch(pvtype & 0xff)
        {
            case 0x1:
                pPVI = new pvInfo (0.5, pvStr.c_str(), 10.0f, -10.0f, aitEnumInt32, excasIoSync, (pvtype & 0x100 ? ntc : 1));
                break;
            case 0x2:
                pPVI = new pvInfo (0.5, pvStr.c_str(), 10.0f, -10.0f, aitEnumFloat32, excasIoSync, (pvtype & 0x100 ? ntc : 1));
                break;
            case 0x4:
                pPVI = new pvInfo (0.5, pvStr.c_str(), 10.0f, -10.0f, aitEnumString, excasIoSync, (pvtype & 0x100 ? ntc : 1));
                break;
        }
        m_pvList[pvStr] = pPVI;
        exPV *pPV = pPVI->createPV (*this, true, scanOn,  this->asyncDelay );
        if (!pPV) {
            fprintf(stderr, "Unable to create new PV \"%s\"\n",
                pPVI->getName());
        }
        //
        // Install canonical (root) name
        //
        sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), pPVI->getName());
        this->installAliasName(*pPVI, pvAlias);
		pPVE = this->stringResTbl.lookup ( id );
	}

    pvInfo & pvi = pPVE->getInfo();

    //
    // Initiate async IO if this is an async PV
    //
    if ( pvi.getIOType() == excasIoSync ) {
        return pverExistsHere;
    }
    else {
        if ( this->simultAsychIOCount >= this->_maxSimultAsyncIO ) {
            return pverDoesNotExistHere;
        }

        this->simultAsychIOCount++;

        exAsyncExistIO * pIO = 
            new exAsyncExistIO ( pvi, ctxIn, *this );
        if ( pIO ) {
            return pverAsyncCompletion;
        }
        else {
            this->simultAsychIOCount--;
            return pverDoesNotExistHere;
        }
    }
}

//
// exServer::pvAttach()
//
pvAttachReturn exServer::pvAttach // X aCC 361
    (const casCtx &ctx, const char *pName)
{
    //
    // lifetime of id is shorter than lifetime of pName
    //
    stringId id(pName, stringId::refString); 
    exPV *pPV;
    pvEntry *pPVE;

    pPVE = this->stringResTbl.lookup(id);
    if (!pPVE) {
        return S_casApp_pvNotFound;
    }

    pvInfo &pvi = pPVE->getInfo();

    //
    // If this is a synchronous PV create the PV now 
    //
    if (pvi.getIOType() == excasIoSync) {
        pPV = pvi.createPV(*this, false, this->scanOn, this->asyncDelay );
        if (pPV) {
            return *pPV;
        }
        else {
            return S_casApp_noMemory;
        }
    }
    //
    // Initiate async IO if this is an async PV
    //
    else {
        if (this->simultAsychIOCount>=this->_maxSimultAsyncIO) {
            return S_casApp_postponeAsyncIO;
        }

        this->simultAsychIOCount++;

        exAsyncCreateIO *pIO = 
            new exAsyncCreateIO ( pvi, *this, ctx, 
                this->scanOn, this->asyncDelay );
        if (pIO) {
            return S_casApp_asyncCompletion;
        }
        else {
            this->simultAsychIOCount--;
            return S_casApp_noMemory;
        }
    }
}

//
// exServer::setDebugLevel ()
//
void exServer::setDebugLevel ( unsigned level )
{
    this->caServer::setDebugLevel ( level );
}

//
// exServer::createTimer ()
//
class epicsTimer & exServer::createTimer ()
{
    if ( this->pTimerQueue ) {
        return this->pTimerQueue->createTimer ();
    }
    else {
        return this->caServer::createTimer ();
    }
}

//
// pvInfo::createPV()
//
exPV *pvInfo::createPV ( exServer & cas, bool preCreateFlag, 
                    bool scanOn, double asyncDelay )
{
    if (this->pPV) {
        return this->pPV;
    }

    exPV *pNewPV;

    //
    // create an instance of the appropriate class
    // depending on the io type and the number
    // of elements
    //
    if (this->elementCount==1u) {
        switch (this->ioType){
        case excasIoSync:
            pNewPV = new exScalarPV ( cas, *this, preCreateFlag, scanOn );
            break;
        case excasIoAsync:
            pNewPV = new exAsyncPV ( cas, *this, 
                preCreateFlag, scanOn, asyncDelay );
            break;
        default:
            pNewPV = NULL;
            break;
        }
    }
    else {
        switch (this->ioType){
        case excasIoSync:
            pNewPV = new exVectorPV ( cas, *this, preCreateFlag, scanOn );
            break;
        case excasIoAsync:
            pNewPV = new exAsyncVectorPV ( cas, *this, preCreateFlag, scanOn, asyncDelay );
            break;
        default:
            pNewPV = NULL;
            break;
		}
	}
    
    //
    // load initial value (this is not done in
    // the constructor because the base class's
    // pure virtual function would be called)
    //
    // We always perform this step even if
    // scanning is disable so that there will
    // always be an initial value
    //
    if (pNewPV) {
        this->pPV = pNewPV;
        pNewPV->scan();
    }

    return pNewPV;
}

//
// exServer::show() 
//
void exServer::show (unsigned level) const
{
    //
    // server tool specific show code goes here
    //
    this->stringResTbl.show(level);

    //
    // print information about ca server libarary
    // internals
    //
    this->caServer::show(level);
}

//
// exAsyncExistIO::exAsyncExistIO()
//
exAsyncExistIO::exAsyncExistIO ( const pvInfo &pviIn, const casCtx &ctxIn,
        exServer &casIn ) :
    casAsyncPVExistIO ( ctxIn ), pvi ( pviIn ), 
        timer ( casIn.createTimer () ), cas ( casIn ) 
{
    this->timer.start ( *this, 0.00001 );
}

//
// exAsyncExistIO::~exAsyncExistIO()
//
exAsyncExistIO::~exAsyncExistIO()
{
    this->cas.removeIO ();
    this->timer.destroy ();
}

//
// exAsyncExistIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncExistIO::expire ( const epicsTime & /*currentTime*/ )
{
    //
    // post IO completion
    //
    this->postIOCompletion ( pvExistReturn(pverExistsHere) );
    return noRestart;
}


//
// exAsyncCreateIO::exAsyncCreateIO()
//
exAsyncCreateIO :: 
    exAsyncCreateIO ( pvInfo &pviIn, exServer &casIn, 
    const casCtx &ctxIn, bool scanOnIn, double asyncDelayIn ) :
    casAsyncPVAttachIO ( ctxIn ), pvi ( pviIn ), 
        timer ( casIn.createTimer () ), 
        cas ( casIn ), asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn )
{
    this->timer.start ( *this, 0.00001 );
}

//
// exAsyncCreateIO::~exAsyncCreateIO()
//
exAsyncCreateIO::~exAsyncCreateIO()
{
    this->cas.removeIO ();
    this->timer.destroy ();
}

//
// exAsyncCreateIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncCreateIO::expire ( const epicsTime & /*currentTime*/ )
{
    exPV * pPV = this->pvi.createPV ( this->cas, false, 
                            this->scanOn, this->asyncDelay );
    if ( pPV ) {
        this->postIOCompletion ( pvAttachReturn ( *pPV ) );
    }
    else {
        this->postIOCompletion ( pvAttachReturn ( S_casApp_noMemory ) );
    }
    return noRestart;
}

// allow use of .VAL in a name
static std::string getPVBaseName(const std::string& pvStr)
{
	std::string pvName;
	int n = pvStr.size();
	if ( n >= 4 && pvStr.substr(n - 4) == ".VAL" )
	{
	    pvName = pvStr.substr(0, n - 4);
	}
	else
	{
	    pvName = pvStr;
	}
	return pvName;
}

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
int parseSpecPV(const std::string& pvStr, int& spec, int& period, char& axis)
{
    //Assumes period then spectrum
	std::string pvName = getPVBaseName(pvStr);
    pcrecpp::RE spec_per_re("SPEC:(\\d+):(\\d+):([XYCS])");
    pcrecpp::RE spec_re("SPEC:(\\d+):([XYCS])");
    
    if (!spec_per_re.FullMatch(pvName, &period, &spec, &axis))
    {
        if (!spec_re.FullMatch(pvName, &spec, &axis))
        {
            return 0x0;
        }
        //If not specified assume the period is 1
        period = 1;
    }
	if (axis == 'C' || axis == 'S')
	{
		return 0x1;
	}
	else
	{
		return 0x2 | 0x100;
	}
}

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
int parseMonitorPV(const std::string& pvStr, int& mon, int& period, char& axis)
{
	std::string pvName = getPVBaseName(pvStr);
    pcrecpp::RE monitor_re("MON:(\\d+):([XYCS])");
    pcrecpp::RE monitor_per_re("MON:(\\d+):(\\d+):([XYCS])");
	if (!monitor_re.FullMatch(pvName, &period, &mon, &axis))
	{
        if (!monitor_re.FullMatch(pvName, &mon, &axis))
        {
            return 0x0;
        }
        //If not specified assume the period is 1
        period = 1;
	}
	if (axis == 'C' || axis == 'S')
	{
		return 0x1;
	}
	else
	{
		return 0x2 | 0x100;
	}
}

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
int parseBeamlinePV(const std::string& pvStr, std::string& param)
{
    pcrecpp::RE beamline_re("BLPAR:([^:]+).*");
	if (!beamline_re.FullMatch(pvStr, &param))
	{
        return 0x0;
	}
	return 0x4;
}

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
int parseSamplePV(const std::string& pvStr, std::string& param)
{
    pcrecpp::RE sample_re("SMPLPAR:([^:]+).*");
	if (!sample_re.FullMatch(pvStr, &param))
	{
        return 0x0;
	}
	return 0x4;
}

// 0x0 on error, 0x1 for scalar int, 0x2 for scalar float, 0x4 for string, ored with 0x100 if array
int getPVType(const std::string& pvStr)
{
	int i, pvtype, p;
	char c;
    std::string s;
    if ( (pvtype = parseSpecPV(pvStr, i, p, c)) != 0x0 )
	{
		return pvtype;
	}
    if ( (pvtype = parseMonitorPV(pvStr, i, p, c)) != 0x0 )
	{
		return pvtype;
	}
    if ( (pvtype = parseSamplePV(pvStr, s)) != 0x0 )
	{
		return pvtype;
	}
    if ( (pvtype = parseBeamlinePV(pvStr, s)) != 0x0 )
	{
		return pvtype;
	}
	return 0x0;
}