/// @file variant_utils.cpp 
/// @author Freddie Akeroyd, STFC ISIS Facility, GB
/// Copyright &copy; 2008-9 ISIS Rutherford Appleton Laboratory & NScD Oak Ridge National Laboratory

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <atlbase.h>
#include <atlcom.h>
#include <atlconv.h>

#include <comdef.h>

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <sstream>

#include "variant_utils.h"


std::string COMexception::com_message(const std::string& message, HRESULT hr)
{
	_com_error ce(hr);
	std::ostringstream oss;
	oss << message << ": " << ce.ErrorMessage();
	return oss.str();
}

// 0 on success, -1 on error

int allocateArrayVariant(VARIANT* v, VARTYPE v_type, int* dims_array, int ndims)
{
			int i;
			V_VT(v) = VT_ARRAY | v_type;
			SAFEARRAYBOUND* sab = new SAFEARRAYBOUND[ndims];
			if (sab == NULL)
			{
				return -1;
			}
			// safe arrays are index other way round to C
			for(i=0; i<ndims; i++)
			{
				sab[i].lLbound = 1;
				sab[i].cElements = dims_array[ndims-i-1];
			}
		    V_UNION(v,parray) = SafeArrayCreate(v_type, ndims, sab);
			delete []sab;
			if (V_UNION(v,parray) == NULL)
			{
				return -1;
			}
			return 0;
}


static int accessArrayVariant(VARIANT* v, void** values, VARTYPE vt)
{
			VARTYPE vtt;
			HRESULT hr;
			*values = NULL;
			hr = SafeArrayGetVartype(V_UNION(v,parray), &vtt);
			if (!(vtt & vt))
			{
				return -1;
			}
			hr = SafeArrayAccessData(V_UNION(v,parray), values);
			if ( FAILED(hr) || (*values == NULL) )
			{
				*values = NULL;
				return -1;
			}
			return 0;
}

int accessArrayVariant(VARIANT* v, float** values)
{
	return accessArrayVariant(v, (void**)values, VT_R4);
}

int accessArrayVariant(VARIANT* v, double** values)
{
	return accessArrayVariant(v, (void**)values, VT_R8);
}

int accessArrayVariant(VARIANT* v, long** values)
{
	return accessArrayVariant(v, (void**)values, VT_I4);
}

int accessArrayVariant(VARIANT* v, BSTR** values)
{
	return accessArrayVariant(v, (void**)values, VT_BSTR);
}

int accessArrayVariant(VARIANT* v, VARIANT** values)
{
	return accessArrayVariant(v, (void**)values, VT_VARIANT);
}

int unaccessArrayVariant(VARIANT* v)
{
	HRESULT hr = SafeArrayUnaccessData(V_UNION(v,parray));
	if (FAILED(hr))
	{
		return -1;
	}
	return 0;
}


int arrayVariantLength(VARIANT* v)
{
	if (!(V_VT(v) & VT_ARRAY))
	{
		return 0;
	}
	SAFEARRAY* psa = V_UNION(v,parray);
	if (psa == NULL)
	{
		return 0;
	}
    int ndims = SafeArrayGetDim(psa);
	if (ndims <= 0)
	{
		return 0;
	}
	long lbounds, ubounds;
	int len = 1;
	int i;
	for(i=0; i<ndims; i++)
	{
		lbounds = ubounds = 0;
		SafeArrayGetLBound(psa, i+1, &lbounds);
		SafeArrayGetUBound(psa, i+1, &ubounds);
		len *= (ubounds - lbounds + 1);
	}
	return len;
}

int arrayVariantDimensions(VARIANT* v, int dims_array[], int& ndims)
{
	ndims = 0;
	dims_array[0] = 0;
	if (!(V_VT(v) & VT_ARRAY))
	{
		return -1;
	}
	SAFEARRAY* psa = V_UNION(v,parray);
	if (psa == NULL)
	{
		return -1;
	}
    ndims = SafeArrayGetDim(psa);
	long lbounds, ubounds;
	for(int i=0; i<ndims; i++)
	{
		lbounds = ubounds = 0;
		SafeArrayGetLBound(psa, i+1, &lbounds);
		SafeArrayGetUBound(psa, i+1, &ubounds);
		dims_array[i] = ubounds - lbounds + 1;
	}
	return 0;
}

template <typename T> 
int makeVariantFromArray(VARIANT* v, const std::vector<T>& the_array)
{
	return makeVariantFromArray(v, &(the_array[0]), static_cast<int>(the_array.size()));
}

template <> 
int makeVariantFromArray(VARIANT* v, const std::vector<std::string>& the_array)
{
	int n = static_cast<int>(the_array.size());
	allocateArrayVariant(v, VT_BSTR, &n, 1);
	BSTR* v_array = NULL;
	accessArrayVariant(v, &v_array);
	for(int i=0; i<n; ++i)
	{
		CComBSTR bstr_wrapper(the_array[i].c_str());
		v_array[i] = bstr_wrapper.Detach();
	}
	unaccessArrayVariant(v);
	return 0;
}


template <const VARTYPE vt>
struct CPPType
{
	typedef int type;
};

template <>
struct CPPType<VT_I4>
{
	typedef int type;
};

template <>
struct CPPType<VT_R4>
{
	typedef float type;
};

template <typename T> 
int makeVariantFromArray(VARIANT* v, const T* the_array, int n)
{
	VARTYPE vt_type = CVarTypeInfo<T>::VT;
	allocateArrayVariant(v, vt_type, &n, 1);
	T* v_array = NULL;
	accessArrayVariant(v, &v_array);
	for(int i=0; i<n; ++i)
	{
		v_array[i] = the_array[i];
	}
	unaccessArrayVariant(v);
	return 0;
}

template <> 
int makeVariantFromArray(VARIANT* v, const char* the_array, int n)
{
	return -1;
}

template int makeVariantFromArray(VARIANT* v, const std::vector<float>& the_array);

