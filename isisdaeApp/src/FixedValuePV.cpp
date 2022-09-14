#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"
#include "isisdaeInterface.h"

/// class for a PV that has a fixed, unchaning value

template <typename T>
FixedValuePV<T>::FixedValuePV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, const T& value ) : exScalarPV(cas, setup, preCreateFlag, scanOnIn, false), m_value(value), m_first_call(true)
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

template class FixedValuePV<char>;
template class FixedValuePV<aitEnum16>;
template class FixedValuePV<int>;
template class FixedValuePV<float>;
template class FixedValuePV<double>;
template class FixedValuePV<std::string>;

