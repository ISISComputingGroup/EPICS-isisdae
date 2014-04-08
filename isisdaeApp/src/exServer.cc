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
	if (pvtype == -1)
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
		if (pvtype == 0)
		{
		    pPVI = new pvInfo (5.0, pvStr.c_str(), 10.0f, -10.0f, aitEnumInt32, excasIoSync, 1);
		}
		else
		{
		    int ntc = 8000; //m_iface->getNumTimeChannels(spec);
		    pPVI = new pvInfo (5.0, pvStr.c_str(), 10.0f, -10.0f, aitEnumFloat32, excasIoSync, ntc);
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


// -1 on error, 0 for scalar, 1 for array
int parseSpecPV(const std::string& pvStr, int& spec, int& period, char& axis)
{
    //Assumes period then spectrum
    pcrecpp::RE spec_per_re("SPEC:(\\d+):(\\d+):([XYC])");
    pcrecpp::RE spec_re("SPEC:(\\d+):([XYC])");
    
    if (!spec_per_re.FullMatch(pvStr, &period, &spec, &axis))
    {
        if (!spec_re.FullMatch(pvStr, &spec, &axis))
        {
            return -1;
        }
        //If not specified assume the period is 1
        period = 1;
    }
	if (axis == 'C')
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

// -1 on error, 0 for scalar, 1 for array
int parseMonitorPV(const std::string& pvStr, int& mon, char& axis)
{
    pcrecpp::RE monitor_re("MON:(\\d+):([XYC])");
	if (!monitor_re.FullMatch(pvStr, &mon, &axis))
	{
        return -1;
	}
	if (axis == 'C')
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

// -1 on error, 0 for scalar, 1 for array
int getPVType(const std::string& pvStr)
{
	int i, pvtype, p;
	char c;
    if ( (pvtype = parseSpecPV(pvStr, i, p, c)) != -1 )
	{
		return pvtype;
	}
    if ( (pvtype = parseMonitorPV(pvStr, i, c)) != -1 )
	{
		return pvtype;
	}
	return -1;
}