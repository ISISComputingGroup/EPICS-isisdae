#include "exServer.h"
#include "gddApps.h"

#include "isisdaeInterface.h"

MonitorSpectrumPV::MonitorSpectrumPV(exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, const std::string& axis, int mon, int period, bool use_crpt) :
        SpectrumPV(cas, setup, preCreateFlag, scanOnIn, axis, 0/*spec*/, period, use_crpt), m_monitor(mon)
{
    updateSpectrum();
}

bool MonitorSpectrumPV::getNewValue(smartGDDPointer& pDD)
{
    if (updateSpectrum()) {
        return SpectrumPV::getNewValue(pDD);
    } else {
        return false;
    }
}

bool MonitorSpectrumPV::updateSpectrum()
{
    try {
        m_spec = cas.iface()->getSpectrumNumberForMonitor(m_monitor);
    }
    catch(const std::exception& ex) {
        m_spec = 0;
        std::cerr << "CAS: Exception in MonitorSpectrumPV::updateSpectrum(): " << ex.what() << std::endl;
        return false;
    }
    catch(...) {
        m_spec = 0;
        std::cerr << "CAS: Exception in MonitorSpectrumPV::updateSpectrum()" << std::endl;
        return false;
    }
    return true;
}