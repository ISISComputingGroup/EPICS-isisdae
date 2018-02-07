#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

CountsPV::CountsPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int spec, int period ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn), m_spec(spec), m_period(period)
{

}

/// update counts value if it has changed, otherwise return false 
bool CountsPV::getNewValue(smartGDDPointer& pDD)
{
    long counts = 0;
	try {
        cas.iface()->getSpectrumIntegral(m_spec, m_period, 0.0, -1.0, counts);
	}
	catch(...) {
		std::cerr << "Exception in CountsPV::getNewValue" << std::endl;
		return false;
	}
	if ( this->pValue.valid() && (static_cast<int>(* this->pValue) == counts) )
	{
		return false;
	}
    *pDD = counts;
    return true;
}

