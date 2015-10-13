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
#include "epicsStdio.h"

//
// exServer::exServer()
//

// allow use of .VAL in a name
static std::string getPVBaseName(const std::string& pvStr)
{
	std::string pvName;
	int n = pvStr.find_last_of(".");
	if (n != std::string::npos)
	{
	    pvName = pvStr.substr(0, n);
	}
	else
	{
	    pvName = pvStr;
	}
	return pvName;
}

static std::string getPVNoValName(const std::string& pvStr)
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

exServer::exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOnIn, 
        bool asyncScan, double asyncDelayIn,
        unsigned maxSimultAsyncIOIn, isisdaeInterface* iface) : 
        pTimerQueue ( 0 ), simultAsychIOCount ( 0u ), 
        _maxSimultAsyncIO ( maxSimultAsyncIOIn ),
        asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn ), m_iface ( iface ), m_pvPrefix(pvPrefix),
		m_ntc(8000)
{
    if ( getenv("EPICS_CA_MAX_ARRAY_BYTES") != NULL )
	{
	    long max_array_bytes = atol(getenv("EPICS_CA_MAX_ARRAY_BYTES"));
		if (max_array_bytes > 0)
		{
		    // m_ntc = max_array_bytes / sizeof(float);
			// above is too big
			m_ntc = 8000;
		}
	}
	std::cerr << "Spectrum array max size set to " << m_ntc << std::endl;
	
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
	std::string pvStr(pPVName);
    if (pvStr.compare(0, m_pvPrefix.size(), m_pvPrefix) != 0)
	{
        return pverDoesNotExistHere;
	}
	pvStr.erase(0, m_pvPrefix.size());

	
	// as we create the PV on the fly, we need to protect against multiple access (and hence multiple create)
	epicsGuard<epicsMutex> _guard (m_lock);
    //
    // Look in hash table for PV name (or PV alias name)
    //
    pPVE = this->stringResTbl.lookup ( id );
    if ( ! pPVE ) {
        createSpecPVs(pvStr);
		createMonitorPVs(pvStr);
		pPVE = this->stringResTbl.lookup ( id );
	}

    if ( ! pPVE ) {
        return pverDoesNotExistHere;
	}
//    pvInfo & pvi = pPVE->getInfo();

    return pverExistsHere;
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

    pPV = pvi.getPV();
    if (pPV) {
        return *pPV;
    }
    else {
        return S_casApp_noMemory;
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
// pvInfo::setPV()
//
void pvInfo::setPV ( exPV *pNewPV)
{

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

}

exPV* pvInfo::getPV() 
{ 
    return pPV; 
}

exPV* exServer::getPV(const std::string& pvName)
{ 
    if ( m_pvList.find(pvName) != m_pvList.end() )
	{
	    return m_pvList[pvName]->getPV(); 
	}
	else
	{
	    return NULL;
	}
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


void exServer::createAxisPVs(const char* prefix, int spec, int period, char axis, const char* units)
{
	char buffer[256], pvAlias[256];
    sprintf(buffer, "%s:%d:%d:%c", prefix, period, spec, axis);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 1.0e9f, 0.0f, units, aitEnumFloat32, m_ntc);
    m_pvList[buffer] = pPVI;
	SpectrumPV* pSPV = new SpectrumPV(*this, *pPVI, true, scanOn, axis, spec, period);
    pPVI->setPV(pSPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%c", prefix, spec, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
    }
	
    sprintf(buffer, "%s:%d:%d:%c.NORD", prefix, period, spec, axis);
    pPVI = new pvInfo (0.5, buffer, static_cast<float>(m_ntc), 1.0f, "", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
	exPV* pPV = new NORDPV(*this, *pPVI, true, scanOn, pSPV->getNORD());
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%c.NORD", prefix, spec, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:%c.NELM", prefix, period, spec, axis);
	pPVI = createFixedPV(buffer, m_ntc, "", aitEnumInt32);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%c.NELM", prefix, spec, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:%c.DESC", prefix, period, spec, axis);
	std::ostringstream desc;
	desc << axis << " (spec=" << spec << ",period=" << period << ")";
	pPVI = createFixedPV(buffer, desc.str(), "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%c.DESC", prefix, spec, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:%c.EGU", prefix, period, spec, axis);
	pPVI = createFixedPV(buffer, std::string(units), "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%c.EGU", prefix, spec, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
}

template <typename T>
pvInfo* exServer::createFixedPV(const std::string& pvStr, const T& value, const char* units, aitEnum ait_type)
{
	char pvAlias[256];
    pvInfo* pPVI = new pvInfo (0.5, pvStr.c_str(), 0.0f, 0.0f, units, ait_type, 1);   /// @todo would be nice to use arithmetic value, but need to check for strings
    m_pvList[pvStr.c_str()] = pPVI;
	exPV* pPV = new FixedValuePV<T>(*this, *pPVI, true, scanOn, value);
    pPVI->setPV(pPV);
	epicsSnprintf(pvAlias, sizeof(pvAlias), "%s%s", m_pvPrefix.c_str(), pvStr.c_str());
    this->installAliasName(*pPVI, pvAlias);
	return pPVI;
}

void exServer::createCountsPV(const char* prefix, int spec, int period)
{
	char buffer[256], pvAlias[256];
    sprintf(buffer, "%s:%d:%d:C", prefix, period, spec);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 1.0e9f, 0.0f, "count", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
	exPV* pPV = new CountsPV(*this, *pPVI, true, scanOn, spec, period);
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:C", prefix, spec);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:C.DESC", prefix, period, spec);
	std::ostringstream desc;
	desc << "Integral (Spec=" << spec << ",period=" << period << ")";
	pPVI = createFixedPV(buffer, desc.str(), "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:C.DESC", prefix, spec);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:C.EGU", prefix, period, spec);
	pPVI = createFixedPV(buffer, std::string("count"), "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:C.EGU", prefix, spec);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
}

bool exServer::createSpecPVs(const std::string& pvStr)
{
    int spec, period;
	char axis;
	std::string field;
	char buffer[256];
    if (!parseSpecPV(pvStr, spec, period, axis, field))
	{
	    return false;
	}
    sprintf(buffer, "SPEC:%d:%d:X", period, spec);
    if (m_pvList.find(buffer) != m_pvList.end())
	{
	    return false;
	}

	createAxisPVs("SPEC", spec, period, 'X', "us");
	createAxisPVs("SPEC", spec, period, 'Y', "cnt /us"); // currently MAX_UNIT_SIZE = 8 for CTRL_DOUBLE calls
	createCountsPV("SPEC", spec, period);

    return true;
}

bool exServer::createMonitorPVs(const std::string& pvStr)
{
    int mon, period;
	char axis;
	std::string field;
	char buffer[256], pvAlias[256];
    if (!parseMonitorPV(pvStr, mon, period, axis, field))
	{
	    return false;
	}
    sprintf(buffer, "MON:%d:%d:X", period, mon);
    if (m_pvList.find(buffer) != m_pvList.end())
	{
	    return false;
	}

	createAxisPVs("MON", mon, period, 'X', "us");
	createAxisPVs("MON", mon, period, 'Y', "cnt /us");
	createCountsPV("MON", mon, period);

    sprintf(buffer, "MON:%d:%d:S", period, mon);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 1.0e9f, 0.0f, "", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
	exPV* pPV = new MonLookupPV(*this, *pPVI, true, scanOn, mon);
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "MON:%d:S", mon);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
	return true;
}

bool parseSpecPV(const std::string& pvStr, int& spec, int& period, char& axis, std::string& field)
{
    //Assumes period then spectrum
    pcrecpp::RE spec_per_re("SPEC:(\\d+):(\\d+):([XYC])([.].*)?");
    pcrecpp::RE spec_re("SPEC:(\\d+):([XYC])([.].*)?");
    
    if (!spec_per_re.FullMatch(pvStr, &period, &spec, &axis, &field))
    {
        if (!spec_re.FullMatch(pvStr, &spec, &axis, &field))
        {
            return false;
        }
        //If not specified assume the period is 1
        period = 1;
    }
	return true;
}

bool parseMonitorPV(const std::string& pvStr, int& mon, int& period, char& axis, std::string& field)
{
    //Assumes period then monitor
    pcrecpp::RE monitor_per_re("MON:(\\d+):(\\d+):([XYCS])([.].*)?");
    pcrecpp::RE monitor_re("MON:(\\d+):([XYCS])([.].*)?");
	if (!monitor_per_re.FullMatch(pvStr, &period, &mon, &axis, &field))
	{
        if (!monitor_re.FullMatch(pvStr, &mon, &axis, &field))
        {
            return false;
        }
        //If not specified assume the period is 1
        period = 1;
	}
	return true;
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
