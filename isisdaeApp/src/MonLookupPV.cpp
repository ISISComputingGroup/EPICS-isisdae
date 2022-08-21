#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

/// class for a PV that converts a monitor number into a spectrum number

MonLookupPV::MonLookupPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int monitor ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn, true), m_monitor(monitor)
{

}

bool MonLookupPV::getNewValue(smartGDDPointer& pDD)
{
    try {
        int spec = cas.iface()->getSpectrumNumberForMonitor(m_monitor);
        if ( this->pValue.valid() && (static_cast<int>(* this->pValue) == spec) )
        {
            return false;
        }
        *pDD = spec;
        return true;
    }
    catch(const std::exception& ex) {
        std::cerr << "CAS: Exception in MonLookupPV::getNewValue(): " << ex.what() << std::endl;
        return false;
    }
    catch(...) {
        std::cerr << "CAS: Exception in MonLookupPV::getNewValue()" << std::endl;
        return false;
    }
}
