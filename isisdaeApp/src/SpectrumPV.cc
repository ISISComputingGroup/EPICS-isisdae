
#include "exServer.h"
#include "gddApps.h"

#include "pcrecpp.h"
#include "isisdaeInterface.h"

/// class for a spectrum. This is a fixed size channel access array, but its current size is stored in m_nord and exported as a .NORD field via  
/// the #NORDPV class. It thus looks a bit like an EPICS Waveform record and, like waveform, export an NELM field too.   

SpectrumPV::SpectrumPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, const std::string& axis, int spec, int period )
	   : exVectorPV(cas, setup, preCreateFlag, scanOnIn, true), m_axis(axis), m_spec(spec), m_period(period), m_nord(0), m_minval(0), m_maxval(0)
{

}

bool SpectrumPV::getNewValue(smartGDDPointer& pDD)
{
    aitFloat32         *pX = NULL, *pY = NULL, *old_pX = NULL, *old_pY = NULL;
 
    //
    // allocate array buffer
    //
	int nmax = this->info.getElementCount();  // fixed size of array
    pX = new aitFloat32 [nmax];
    if (!pX) {
        return false;
    }
    pY = new aitFloat32 [nmax];
    if (!pY) {
        delete[] pX;
        return false;
    }
    int n = 0;
	try {
	    if (m_axis == "YC")
		{
	        n = cas.iface()->getSpectrum(m_spec, m_period, pX, pY, nmax, false);
		}
		else
		{
	        n = cas.iface()->getSpectrum(m_spec, m_period, pX, pY, nmax, true);
		}
	}
	catch(const std::exception& ex) {
		std::cerr << "CAS: Exception in SpectrumPV::getNewValue(): " << ex.what() << std::endl;
        delete[] pX;
        delete[] pY;
		return false;
	}
	catch(...) {
		std::cerr << "CAS: Exception in SpectrumPV::getNewValue()" << std::endl;
        delete[] pX;
        delete[] pY;
		return false;
	}
	m_nord = n;  // number of elements used
	if (m_axis == "Y" || m_axis == "YC")
	{
		delete[] pX;
		if ( this->pValue.valid() )
		{
		    this->pValue->getRef(old_pY);
			if ( old_pY != NULL && memcmp(pY, old_pY, n*sizeof(aitFloat32)) == 0 )
			{
				delete[] pY;
			    return false; // no change to value
			}
		}
        const auto minmax = std::minmax_element(pY, pY + n);
		for(int i=n; i<nmax; ++i)
		{
		    pY[i] = 0.0;
		}
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, nmax);
        if ( ! pDD.valid () ) {
			delete[] pY;
            return false; // no change to value
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pY, new exVecDestructor);
        m_minval = *(minmax.first);
        m_maxval = *(minmax.second);
		return true;
    }
	else if (m_axis == "X")
	{
		delete[] pY;
		// check to see if value has actually changed since last time
		if ( this->pValue.valid()  )
		{
		    this->pValue->getRef(old_pX);
			if ( old_pX != NULL && memcmp(pX, old_pX, n*sizeof(aitFloat32)) == 0 )
			{
				delete[] pX;
			    return false; // no change to value
			}
		}
        const auto minmax = std::minmax_element(pX, pX + n);
		for(int i=n; i<nmax; ++i)
		{
		    pX[i] = pX[n-1];  // repeat last x element
		}
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, nmax);
        if ( ! pDD.valid () ) {
			delete[] pX;
            return false; // no change to value
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pX, new exVecDestructor);
        m_minval = *(minmax.first);
        m_maxval = *(minmax.second);
		return true; // value changed, calling function will send monitors
	}
    else
    {
        delete[] pX;
        delete[] pY;
	    return false;
    }
}
