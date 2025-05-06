#include "exServer.h"
#include "gddApps.h"

#include "isisdaeInterface.h"

MonitorCountsPV::MonitorCountsPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int mon, int period, bool use_crpt) :
            CountsPV(cas, setup, preCreateFlag, scanOnIn, 0/*spec*/, period, use_crpt), m_monitor(mon)
{
    updateSpectrum();
}

/// update counts value if it has changed, otherwise return false 
bool MonitorCountsPV::getNewValue(smartGDDPointer& pDD)
{
    if (updateSpectrum()) {
        return CountsPV::getNewValue(pDD);
    } else {
        return false;
    }
}

bool MonitorCountsPV::updateSpectrum()
{
    try {
        m_spec = cas.iface()->getSpectrumNumberForMonitor(m_monitor);
    }
    catch(const std::exception& ex) {
        m_spec = 0;
        std::cerr << "CAS: Exception in MonitorCountsPV::updateSpectrum(): " << ex.what() << std::endl;
        return false;
    }
    catch(...) {
        m_spec = 0;
        std::cerr << "CAS: Exception in MonitorCountsPV::updateSpectrum()" << std::endl;
        return false;
    }
    return true;
}