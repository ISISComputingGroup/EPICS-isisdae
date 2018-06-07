#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

/// class for a PV that has a fixed, unchanging value

template <typename T>
FixedValuePV<T>::FixedValuePV ( exServer & cas, pvInfo &setup, bool preCreateFlag, const T& value ) : exScalarPV(cas, setup, preCreateFlag, false, false), m_value(value), m_first_call(true)
{

}

template <typename T>
bool FixedValuePV<T>::getNewValue(smartGDDPointer& pDD)
{
    if ( this->pValue.valid() && !m_first_call )
	{
	    return false; 
	}
	*pDD = m_value;
	m_first_call = false;
	return true;
}

// specialisation for std::string as we need to call c_str()
template <>
bool FixedValuePV<std::string>::getNewValue(smartGDDPointer& pDD)
{
    if ( this->pValue.valid() && !m_first_call )
	{
	    return false; 
	}
	*pDD = m_value.c_str();
	m_first_call = false;
	return true;
}

template class FixedValuePV<int>;
template class FixedValuePV<float>;
template class FixedValuePV<std::string>;

