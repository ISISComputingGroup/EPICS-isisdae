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

static const int ISISDAE_MAX_NTC_DEFAULT = 8000;

exServer::exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOnIn, 
        bool asyncScan, double asyncDelayIn,
        unsigned maxSimultAsyncIOIn, isisdaeInterface* iface, int dae_type ) :
        pTimerQueue ( 0 ), simultAsychIOCount ( 0u ), 
        _maxSimultAsyncIO ( maxSimultAsyncIOIn ),
        asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn ), m_iface ( iface ), m_pvPrefix(pvPrefix),
		m_ntc(ISISDAE_MAX_NTC_DEFAULT)
{
    if ( getenv("EPICS_CA_MAX_ARRAY_BYTES") != NULL )
	{
	    long max_array_bytes = atol(getenv("EPICS_CA_MAX_ARRAY_BYTES"));
		if (max_array_bytes > 0)
		{
		    // m_ntc = max_array_bytes / sizeof(float);
			// above is too big
			m_ntc = ISISDAE_MAX_NTC_DEFAULT;
		}
	}
	if (dae_type == 2 || dae_type == 4) {
	    m_tof_units = "ns";
	} else {
	    m_tof_units = "us";
    }
    if ( getenv("ISISDAE_MAX_NTC") != NULL )
	{
		m_ntc = atol(getenv("ISISDAE_MAX_NTC"));
		if (m_ntc == 0)
		{
			m_ntc = ISISDAE_MAX_NTC_DEFAULT;			
		}
	}
	std::cerr << "CAS: Spectrum array max size set to " << m_ntc << std::endl;
	
    exPV::initFT();
	setDebugLevel(5);

    if ( asyncScan ) {
        unsigned timerPriotity;
        int timer_priority = atoi(getenv("ISISDAE_TIMER_PRIORITY") != NULL ?  getenv("ISISDAE_TIMER_PRIORITY") : "1");
        epicsThreadBooleanStatus etbs = epicsThreadBooleanStatusSuccess;
        if (timer_priority > 0) {
            etbs = epicsThreadLowestPriorityLevelAbove (
                epicsThreadGetPrioritySelf (), & timerPriotity );
        } else if (timer_priority < 0) {
            etbs = epicsThreadHighestPriorityLevelBelow (
                epicsThreadGetPrioritySelf (), & timerPriotity );
        } else {
            timerPriotity = epicsThreadGetPrioritySelf ();
        }
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

	for (std::map<std::string, pvInfo*>::const_iterator it = m_pvList.begin(); it != m_pvList.end(); ++it)
	{
		std::cerr << "PV: \"" << it->second->getName() << "\"" << std::endl;
		it->second->getPV()->show(level);
#if 0 /* untested, but probably not now needed */
        std::string prefixedPV = m_pvPrefix  + it->second->getName();
        stringId id (prefixedPV.c_str(), stringId::refString);
        if (this->stringResTbl.lookup(id) == NULL) {
            std::cerr << "ERROR: PV is not in string lookup table" << std::endl;
        }
#endif
	}
	//
    // print information about ca server libarary
    // internals
    //
    this->caServer::show(level);
}


void exServer::createAxisPVs(bool use_crpt, bool is_monitor, int id, int period, const char* axis, const std::string& units)
{
	char buffer[256], pvAlias[256];
    const char* prefix;
    if (use_crpt) {
        prefix = (is_monitor ? "CMON" : "CSPEC");
    } else {
        prefix = (is_monitor ? "MON" : "SPEC");
    }
    sprintf(buffer, "%s:%d:%d:%s", prefix, period, id, axis);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 0.0f, 0.0f, units.c_str(), aitEnumFloat32, m_ntc);
    m_pvList[buffer] = pPVI;
	SpectrumPV* pSPV = (is_monitor ? new MonitorSpectrumPV(*this, *pPVI, true, scanOn, axis, id, period, use_crpt) :
                                     new SpectrumPV(*this, *pPVI, true, scanOn, axis, id, period, use_crpt));
    pPVI->setPV(pSPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
    }
	
    sprintf(buffer, "%s:%d:%d:%s.NORD", prefix, period, id, axis);
    bool is_edges = (strcmp(axis, "XE") == 0);
    pPVI = new pvInfo (0.5, buffer, static_cast<float>(m_ntc), 1.0f, "", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
	exPV* pPV = (is_monitor ? static_cast<NORDPV<int>*>(new NORDMONPV<int>(*this, *pPVI, true, scanOn, pSPV->getNORD(), id, is_edges)) :
                              static_cast<NORDPV<int>*>(new NORDSPECPV<int>(*this, *pPVI, true, scanOn, pSPV->getNORD(), id, is_edges)));
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
    // if creating PVs for period 1, as period 1 is the default also now
    // create pv so if :spec: is specified it translates to :1:spec:
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.NORD", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:%s:MAX", prefix, period, id, axis);
    pPVI = new pvInfo (0.5, buffer, 0.0f, 1.0f, "", aitEnumFloat32, 1);
    m_pvList[buffer] = pPVI;
	pPV = new NORDPV<float>(*this, *pPVI, true, scanOn, pSPV->getMAXVAL());
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s:MAX", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}

    sprintf(buffer, "%s:%d:%d:%s.NELM", prefix, period, id, axis);
	pPVI = createFixedPV(buffer, m_ntc, "", aitEnumInt32);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.NELM", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    createStandardPVs(prefix, period, id, axis, units, is_monitor);
}

void exServer::createStandardPVs(const char* prefix, int period, int id, const char* axis, const std::string& units, bool is_monitor)
{
	char buffer[256], pvAlias[256];
    sprintf(buffer, "%s:%d:%d:%s.DESC", prefix, period, id, axis);
	std::ostringstream desc;
	desc << axis << " (" << (is_monitor ? "mon=" : "spec=") << id << ",period=" << period << ")";
	pvInfo* pPVI = createFixedPV(buffer, desc.str(), "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.DESC", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.EGU", prefix, period, id, axis);
	pPVI = createFixedPV(buffer, units, "", aitEnumString);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.EGU", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.UDF", prefix, period, id, axis);
	pPVI = createFixedPV(buffer, 0, "", aitEnumInt8);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.UDF", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.STAT", prefix, period, id, axis);
	pPVI = createNoAlarmPV(buffer);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.STAT", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.SEVR", prefix, period, id, axis);
	pPVI = createNoAlarmPV(buffer);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.SEVR", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.HOPR", prefix, period, id, axis);
	pPVI = createFixedPV(buffer, 0.0, "", aitEnumFloat64);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.HOPR", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    sprintf(buffer, "%s:%d:%d:%s.LOPR", prefix, period, id, axis);
	pPVI = createFixedPV(buffer, 0.0, "", aitEnumFloat64);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:%s.LOPR", prefix, id, axis);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
}

template <typename T>
pvInfo* exServer::createFixedPV(const std::string& pvStr, const T& value, const char* units, aitEnum ait_type)
{
	char pvAlias[256];
    pvInfo* pPVI = new pvInfo (5.0, pvStr.c_str(), 0.0f, 0.0f, units, ait_type, 1);   /// @todo would be nice to use arithmetic value, but need to check for strings
    m_pvList[pvStr.c_str()] = pPVI;
	exPV* pPV = new FixedValuePV<T>(*this, *pPVI, true, scanOn, value);
    pPVI->setPV(pPV);
	epicsSnprintf(pvAlias, sizeof(pvAlias), "%s%s", m_pvPrefix.c_str(), pvStr.c_str());
    this->installAliasName(*pPVI, pvAlias);
	return pPVI;
}

pvInfo* exServer::createNoAlarmPV(const std::string& pvStr)
{
	char pvAlias[256];
    pvInfo* pPVI = new pvInfo (5.0, pvStr.c_str(), 0.0f, 0.0f, "", aitEnumEnum16, 1);   /// @todo would be nice to use arithmetic value, but need to check for strings
    m_pvList[pvStr.c_str()] = pPVI;
	exPV* pPV = new NoAlarmPV(*this, *pPVI, true, scanOn);
    pPVI->setPV(pPV);
	epicsSnprintf(pvAlias, sizeof(pvAlias), "%s%s", m_pvPrefix.c_str(), pvStr.c_str());
    this->installAliasName(*pPVI, pvAlias);
	return pPVI;
}

// id is spec or mon number
void exServer::createCountsPV(bool use_crpt, bool is_monitor, int id, int period)
{
	char buffer[256], pvAlias[256];
    const char* prefix;
    if (use_crpt) {
        prefix = (is_monitor ? "CMON" : "CSPEC");
    } else {
        prefix = (is_monitor ? "MON" : "SPEC");
    }
    sprintf(buffer, "%s:%d:%d:C", prefix, period, id);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 0.0f, 0.0f, "cnt", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
    exPV* pPV = (is_monitor ? new MonitorCountsPV(*this, *pPVI, true, scanOn, id, period, use_crpt) :
                              new CountsPV(*this, *pPVI, true, scanOn, id, period, use_crpt));
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%s:%d:C", prefix, id);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
    createStandardPVs(prefix, period, id, "C", "cnt", is_monitor);
}


bool exServer::createSpecPVs(const std::string& pvStr)
{
    int spec, period;
    bool use_crpt = false;
	std::string axis, field;
	char buffer[256];
    if (!parseSpecPV(pvStr, spec, period, axis, field, use_crpt))
	{
	    return false;
	}
    sprintf(buffer, "%sSPEC:%d:%d:X", (use_crpt ? "C" : ""), period, spec);
    if (m_pvList.find(buffer) != m_pvList.end())
	{
	    return false;
	}

	createAxisPVs(use_crpt, false, spec, period, "X", m_tof_units);
	createAxisPVs(use_crpt, false, spec, period, "XE", m_tof_units);
	createAxisPVs(use_crpt, false, spec, period, "Y", std::string("cnt /") + m_tof_units); // currently MAX_UNIT_SIZE = 8 for CTRL_DOUBLE calls
	createAxisPVs(use_crpt, false, spec, period, "YC", "cnt");
	createCountsPV(use_crpt, false, spec, period);

    return true;
}

bool exServer::createMonitorPVs(const std::string& pvStr)
{
    int mon, period;
    bool use_crpt = false;
	std::string axis, field;
	char buffer[256], pvAlias[256];
    if (!parseMonitorPV(pvStr, mon, period, axis, field, use_crpt))
	{
	    return false;
	}
    sprintf(buffer, "%sMON:%d:%d:X", (use_crpt ? "C" : ""), period, mon);
    if (m_pvList.find(buffer) != m_pvList.end())
	{
	    return false;
	}

	createAxisPVs(use_crpt, true, mon, period, "X", m_tof_units);
	createAxisPVs(use_crpt, true, mon, period, "XE", m_tof_units);
	createAxisPVs(use_crpt, true, mon, period, "Y", std::string("cnt /") + m_tof_units);  // currently MAX_UNIT_SIZE = 8 for CTRL_DOUBLE calls
	createAxisPVs(use_crpt, true, mon, period, "YC", "cnt");
	createCountsPV(use_crpt, true, mon, period);

    sprintf(buffer, "%sMON:%d:%d:S", (use_crpt ? "C" : ""), period, mon);
    pvInfo* pPVI = new pvInfo (0.5, buffer, 0.0f, 0.0f, "", aitEnumInt32, 1);
    m_pvList[buffer] = pPVI;
	exPV* pPV = new MonLookupPV(*this, *pPVI, true, scanOn, mon);
    pPVI->setPV(pPV);
	sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
    this->installAliasName(*pPVI, pvAlias);
	if (period == 1)
	{
        sprintf(buffer, "%sMON:%d:S", (use_crpt ? "C" : ""), mon);
	    sprintf(pvAlias, "%s%s", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	    sprintf(pvAlias, "%s%s.VAL", m_pvPrefix.c_str(), buffer);
        this->installAliasName(*pPVI, pvAlias);
	}
	return true;
}

bool parseSpecPV(const std::string& pvStr, int& spec, int& period, std::string& axis, std::string& field, bool& use_crpt)
{
    //Assumes period then spectrum
    pcrecpp::RE spec_per_re("(C)?SPEC:(\\d+):(\\d+):(XE|X|YC|Y|C)([.].*)?");
    pcrecpp::RE spec_re("(C)?SPEC:(\\d+):(XE|X|YC|Y|C)([.].*)?");
    
    std::string crpt;
    if (!spec_per_re.FullMatch(pvStr, &crpt, &period, &spec, &axis, &field))
    {
        if (!spec_re.FullMatch(pvStr, &crpt, &spec, &axis, &field))
        {
            return false;
        }
        //If not specified assume the period is 1
        period = 1;
    }
    use_crpt = (crpt == "C" ? true : false);
	return true;
}

bool parseMonitorPV(const std::string& pvStr, int& mon, int& period, std::string& axis, std::string& field, bool& use_crpt)
{
    //Assumes period then monitor
    pcrecpp::RE monitor_per_re("(C)?MON:(\\d+):(\\d+):(XE|X|YC|Y|C|S)([.].*)?");
    pcrecpp::RE monitor_re("(C)?MON:(\\d+):(XE|X|YC|Y|C|S)([.].*)?");

    std::string crpt;
	if (!monitor_per_re.FullMatch(pvStr, &crpt, &period, &mon, &axis, &field))
	{
        if (!monitor_re.FullMatch(pvStr, &crpt, &mon, &axis, &field))
        {
            return false;
        }
        //If not specified assume the period is 1
        period = 1;
	}
    use_crpt = (crpt == "C" ? true : false);
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
