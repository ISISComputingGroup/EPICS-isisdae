#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

/// class for a PV that returns the current size of an array, this field is called NORD in an EPICS waveform record hence the name of this class.
/// it is initialised with a reference to the array size to monitor, which is usually from something like a spectrumPV class   

NORDPV::NORDPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int& nord ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn, false), m_nord(nord)
{

}

bool NORDPV::getNewValue(smartGDDPointer& pDD)
{
	if ( this->pValue.valid() && (static_cast<int>(* this->pValue) == m_nord) )
	{
		return false;
	}
    *pDD = m_nord;
	return true;
}
