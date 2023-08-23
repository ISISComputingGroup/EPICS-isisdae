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

template class NORDPV<int>;
template class NORDPV<float>;
