#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

MonLookupPV::MonLookupPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int monitor ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn), m_monitor(monitor)
{

}

/// @todo this needs to actually look up monitor 
bool MonLookupPV::getNewValue(smartGDDPointer& pDD)
{
    int spec = m_monitor;
	if ( this->pValue.valid() && (static_cast<int>(* this->pValue) == spec) )
	{
		return false;
	}
    *pDD = spec;
    return true;
}
