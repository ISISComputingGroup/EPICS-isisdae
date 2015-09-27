
#include "exServer.h"
#include "gddApps.h"

#include "pcrecpp.h"
#include "isisdaeInterface.h"

SpectrumPV::SpectrumPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn, char axis, int spec, int period )
	   : exVectorPV(cas, setup, preCreateFlag, scanOnIn), m_axis(axis), m_spec(spec), m_period(period), m_nord(0)
{

}

bool SpectrumPV::getNewValue(smartGDDPointer& pDD)
{
    aitFloat32         *pX = NULL, *pY = NULL, *old_pX = NULL, *old_pY = NULL;
    exVecDestructor     *pDest;
 
    //
    // allocate array buffer
    //
	int nmax = this->info.getElementCount();
    pX = new aitFloat32 [nmax];
    if (!pX) {
        return false;
    }
    pY = new aitFloat32 [nmax];
    if (!pY) {
        delete[] pX;
        return false;
    }
	pDest = new exVecDestructor;
    if (!pDest) {
        delete[] pX;
		delete[] pY;
        return false;
    }

    int n = cas.iface()->getSpectrum(m_spec, m_period, pX, pY, nmax);
	m_nord = n;
	
	if (m_axis == 'Y')
	{
		delete[] pX;
		if ( this->pValue.valid() )
		{
		    this->pValue->getRef(old_pY);
			if ( old_pY != NULL && memcmp(pY, old_pY, n*sizeof(aitFloat32)) == 0 )
			{
				delete[] pY;
			    return false;
			}
		}
		for(int i=n; i<nmax; ++i)
		{
		    pY[i] = 0.0;
		}
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, nmax);
        if ( ! pDD.valid () ) {
			delete[] pY;
			delete pDest;
            return false;
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pY, pDest);
		return true;
    }
	else if (m_axis == 'X')
	{
		delete[] pY;
		// check to see if value has actually changed since last time
		if ( this->pValue.valid()  )
		{
		    this->pValue->getRef(old_pX);
			if ( old_pX != NULL && memcmp(pX, old_pX, n*sizeof(aitFloat32)) == 0 )
			{
				delete[] pX;
				delete pDest;
			    return false;
			}
		}
		for(int i=n; i<nmax; ++i)
		{
		    pX[i] = pX[n-1];  // repeat last x element
		}
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, nmax);
        if ( ! pDD.valid () ) {
			delete[] pX;
			delete pDest;
            return false;
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pX, pDest);
		return true;
	}
	return false;
}

