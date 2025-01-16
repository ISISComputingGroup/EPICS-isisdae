#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

/// class for a PV that returns the current size of an array, this field is called NORD in an EPICS waveform record hence the name of this class.
/// it is initialised with a reference to the array size to monitor, which is usually from something like a spectrumPV class   

/// this class is actually more general and now not just use for NORD, it can be used fore any integer PV in the spectrum class
/// and is now used for min and max values as well as NORD

template <typename T>
NORDPV<T>::NORDPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, T& nord ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn, false), m_nord(nord)
{

}

template <typename T>
bool NORDPV<T>::getNewValue(smartGDDPointer& pDD)
{
	if ( this->pValue.valid() && (static_cast<T>(* this->pValue) == m_nord) )
	{
		return false;
	}
    *pDD = m_nord;
	return true;
}

template <typename T>
NORDSPECPV<T>::NORDSPECPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, T& nord, int spec, bool is_edges) : NORDPV<T>(cas, setup, preCreateFlag, scanOnIn, nord), m_is_edges(is_edges), m_spec(spec)
{

}

template <typename T>
bool NORDSPECPV<T>::getNewValue(smartGDDPointer& pDD)
{
    int nmax = static_cast<int>(this->info.getHopr());
    try {
        int n = cas.iface()->getSpectrumSize(m_spec) + (m_is_edges ? 1 : 0);
        n = (n > nmax ? nmax : n); // we may not have allocated enough space so truncate - isisicp may have been reconfigured with a larger size
	    setNORD(n);
	}
	catch(const std::exception& ex) {
		std::cerr << "CAS: Exception in NORDSPECPV::getNewValue(): " << ex.what() << std::endl;
		return false;
	}
	catch(...) {
		std::cerr << "CAS: Exception in NORDSPECPV::getNewValue()" << std::endl;
		return false;
	}
    return NORDPV<T>::getNewValue(pDD);
}

template <typename T>
NORDMONPV<T>::NORDMONPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, T& nord, int mon, bool is_edges) : NORDPV<T>(cas, setup, preCreateFlag, scanOnIn, nord), m_is_edges(is_edges), m_mon(mon)
{

}

template <typename T>
bool NORDMONPV<T>::getNewValue(smartGDDPointer& pDD)
{
    int nmax = static_cast<int>(this->info.getHopr());
    try {
        int spec = cas.iface()->getSpectrumNumberForMonitor(m_mon);
        int n = cas.iface()->getSpectrumSize(spec) + (m_is_edges ? 1 : 0);
        n = (n > nmax ? nmax : n); // we may not have allocated enough space so truncate - isisicp may have been reconfigured with a larger size
	    setNORD(n);
	}
	catch(const std::exception& ex) {
		std::cerr << "CAS: Exception in NORDMONPV::getNewValue(): " << ex.what() << std::endl;
		return false;
	}
	catch(...) {
		std::cerr << "CAS: Exception in NORDMONPV::getNewValue()" << std::endl;
		return false;
	}
    return NORDPV<T>::getNewValue(pDD);
}

template class NORDPV<int>;
template class NORDPV<float>;

template class NORDSPECPV<int>;
template class NORDMONPV<int>;
