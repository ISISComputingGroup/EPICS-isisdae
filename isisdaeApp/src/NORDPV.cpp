#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

NORDPV::NORDPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, int& nord ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn), m_nord(nord)
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
