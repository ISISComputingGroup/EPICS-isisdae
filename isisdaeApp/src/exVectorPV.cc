/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "exServer.h"
#include "gddApps.h"

#include "pcrecpp.h"
#include "isisdaeInterface.h"

#define myPI 3.14159265358979323846

//
// SUN C++ does not have RAND_MAX yet
//
#if ! defined(RAND_MAX)
//
// Apparently SUN C++ is using the SYSV version of rand
//
#   if 0
#       define RAND_MAX INT_MAX
#   else
#       define RAND_MAX SHRT_MAX
#   endif
#endif

//
// special gddDestructor guarantees same form of new and delete
//
class exVecDestructor: public gddDestructor {
    virtual void run (void *);
};

//
// exVectorPV::maxDimension()
//
unsigned exVectorPV::maxDimension() const
{
    return 1u;
}

//
// exVectorPV::maxBound()
//
aitIndex exVectorPV::maxBound (unsigned dimension) const // X aCC 361
{
    if (dimension==0u) {
        return m_size;
    }
    else {
        return 0u;
    }
}

//
// exVectorPV::scan
//
void exVectorPV::scan()
{
    caStatus            status;
    smartGDDPointer     pDD;
    aitFloat32         *pX, *pY;
    float               limit;
    exVecDestructor     *pDest;
    int                 gddStatus;

    //
    // update current time (so we are not required to do
    // this every time that we write the PV which impacts
    // throughput under sunos4 because gettimeofday() is
    // slow)
    //
    this->currentTime = epicsTime::getCurrent();
 
    //
    // allocate array buffer
    //
    pX = new aitFloat32 [this->info.getElementCount()];
    if (!pX) {
        return;
    }
    pY = new aitFloat32 [this->info.getElementCount()];
    if (!pY) {
        delete[] pX;
        return;
    }

    pDest = new exVecDestructor;
    if (!pDest) {
        delete[] pX;
		delete[] pY;
        return;
    }

	std::string pvName(getName());
	int spec, mon, period;
	char axis;
	if (parseSpecPV(pvName, spec, period, axis) == 0x0)
	{
		if (parseMonitorPV(pvName, mon, period, axis) == 0x0)
		{
			return;
		}
		spec = 1;
        period = 1;
	}
	int n = 0;
	if (axis == 'Y')
	{
        n = cas.iface()->getSpectrum(spec, period, pX, pY, this->info.getElementCount());
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, n);
        if ( ! pDD.valid () ) {
            return;
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pY, pDest);
		delete[] pX;
	}
	else if (axis == 'X')
	{
        n = cas.iface()->getSpectrum(spec, period, pX, pY, this->info.getElementCount());
        pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 1u, n);
        if ( ! pDD.valid () ) {
            return;
        }
        //
        // install the buffer into the DD
        // (do this before we increment pF)
        //
        pDD->putRef(pX, pDest);
		delete[] pY;
	}
    //
    // smart pointer class manages reference count after this point
    //
    gddStatus = pDD->unreference();
    assert (!gddStatus);
	
    aitTimeStamp gddts = this->currentTime;
    pDD->setTimeStamp ( & gddts );

    status = this->update ( *pDD );
    if ( status != S_casApp_success ) {
        errMessage (status, "vector scan update failed\n");
        std::cerr << "num points was " << n << std::endl;
    }
}

//
// exVectorPV::updateValue ()
//
// NOTES:
// 1) This should have a test which verifies that the
// incoming value in all of its various data types can
// be translated into a real number?
// 2) We prefer to unreference the old PV value here and
// reference the incomming value because this will
// result in value change events each retaining an
// independent value on the event queue. With large arrays
// this may result in too much memory consumtion on
// the event queue.
//
caStatus exVectorPV::updateValue ( const gdd & value )
{

    //
    // Check bounds of incoming request
    // (and see if we are replacing all elements -
    // replaceOk==true)
    //
    // Perhaps much of this is unnecessary since the
    // server lib checks the bounds of all requests
    //
    const gddBounds* pb = value.getBounds ();
    if ( value.isAtomic()) {
        if ( value.dimension() != 1u ) {
            return S_casApp_badDimension;
        }
        
        if ( pb[0u].first() != 0u ) {
            return S_casApp_outOfBounds;
        }
        else if ( pb[0u].size() > this->info.getElementCount() ) {
            return S_casApp_outOfBounds;
        }
    }
    else if ( ! value.isScalar() ) {
        //
        // no containers
        //
        return S_casApp_outOfBounds;
    }
        
    //
    // Create a new array data descriptor
    // (so that old values that may be referenced on the
    // event queue are not replaced)
    //
    smartGDDPointer pNewValue ( new gddAtomic ( gddAppType_value, aitEnumFloat32, 
        1u, pb[0u].size() ) );
    if ( ! pNewValue.valid() ) {
        return S_casApp_noMemory;
    }

    //
    // smart pointer class takes care of the reference count
    // from here down
    //
    gddStatus gdds = pNewValue->unreference( );
    assert ( ! gdds );
    
    //
    // allocate array buffer
    //
    aitFloat32 * pF = new aitFloat32 [pb[0u].size()];
    if (!pF) {
        return S_casApp_noMemory;
    }
    
    //
    // Install (and initialize) array buffer
    // if no old values exist
    //
    unsigned count = pb[0u].size();
    for ( unsigned i = 0u; i < count; i++ ) {
        pF[i] = 0.0f;
    }

    exVecDestructor * pDest = new exVecDestructor;
    if (!pDest) {
        delete [] pF;
        return S_casApp_noMemory;
    }

    //
    // install the buffer into the DD
    // (do this before we increment pF)
    //
    pNewValue->putRef ( pF, pDest );
    
    //
    // copy in the values that they are writing
    //
    gdds = pNewValue->put( & value );
    if ( gdds ) {
        return S_cas_noConvert;
    }
    
    this->pValue = pNewValue;
    m_size = pb[0u].size();
    
    return S_casApp_success;
}

//
// exVecDestructor::run()
//
// special gddDestructor guarantees same form of new and delete
//
void exVecDestructor::run ( void *pUntyped )
{
    aitFloat32 * pf = reinterpret_cast < aitFloat32 * > ( pUntyped );
    delete [] pf;
}
