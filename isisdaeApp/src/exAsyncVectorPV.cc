/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// Example EPICS CA server
// (asynchrronous process variable)
//

#include "exServer.h"

exAsyncVectorPV::exAsyncVectorPV ( exServer & cas, pvInfo & setup, 
                             bool preCreateFlag, bool scanOnIn,
                             double asyncDelayIn ) :
    exVectorPV ( cas, setup, preCreateFlag, scanOnIn ),
    asyncDelay ( asyncDelayIn ),
    simultAsychReadIOCount ( 0u ),
    simultAsychWriteIOCount ( 0u )
{
}

//
// exAsyncPV::read()
//
caStatus exAsyncVectorPV::read (const casCtx &ctx, gdd &valueIn)
{
	exAsyncReadIO<exAsyncVectorPV>	*pIO;
	
	if ( this->simultAsychReadIOCount >= this->cas.maxSimultAsyncIO () ) {
		return S_casApp_postponeAsyncIO;
	}

	pIO = new exAsyncReadIO<exAsyncVectorPV> ( this->cas, ctx, 
	                *this, valueIn, this->asyncDelay );
	if ( ! pIO ) {
        if ( this->simultAsychReadIOCount > 0 ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
		    return S_casApp_noMemory;
        }
	}
	this->simultAsychReadIOCount++;
    return S_casApp_asyncCompletion;
}

//
// exAsyncPV::writeNotify()
//
caStatus exAsyncVectorPV::writeNotify ( const casCtx &ctx, const gdd &valueIn )
{	
	if ( this->simultAsychWriteIOCount >= this->cas.maxSimultAsyncIO() ) {
		return S_casApp_postponeAsyncIO;
	}

	exAsyncWriteIO<exAsyncVectorPV> * pIO = new 
        exAsyncWriteIO<exAsyncVectorPV> ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
        if ( this->simultAsychReadIOCount > 0 ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
		    return S_casApp_noMemory;
        }
    }
	this->simultAsychWriteIOCount++;
	return S_casApp_asyncCompletion;
}

//
// exAsyncPV::write()
//
caStatus exAsyncVectorPV::write ( const casCtx &ctx, const gdd &valueIn )
{
	// implement the discard intermediate values, but last value
    // sent always applied behavior that IOCs provide excepting
    // that we will alow N requests to pend instead of a limit
    // of only one imposed in the IOC
	if ( this->simultAsychWriteIOCount >= this->cas.maxSimultAsyncIO() ) {
        pStandbyValue.set ( & valueIn );
		return S_casApp_success;
	}

	exAsyncWriteIO<exAsyncVectorPV> * pIO = new 
        exAsyncWriteIO<exAsyncVectorPV> ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
        pStandbyValue.set ( & valueIn );
		return S_casApp_success;
    }
	this->simultAsychWriteIOCount++;
	return S_casApp_asyncCompletion;
}

// Implementing a specialized update for exAsyncPV
// allows standby value to update when we update 
// the PV from an asynchronous write timer expiration
// which is a better time compared to removeIO below
// which, if used, gets the reads and writes out of
// order. This type of reordering can cause the 
// regression tests to fail.
caStatus exAsyncVectorPV :: updateFromAsyncWrite ( const gdd & src )
{
    caStatus stat = this->update ( src );
    if ( this->simultAsychWriteIOCount <=1 && 
            pStandbyValue.valid () ) {
//printf("updateFromAsyncWrite: write standby\n");
        stat = this->update ( *this->pStandbyValue );
        this->pStandbyValue.set ( 0 );
    }
    return stat;
}

void exAsyncVectorPV::removeReadIO ()
{
    if ( this->simultAsychReadIOCount > 0u ) {
        this->simultAsychReadIOCount--;
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychReadIOCount?\n" );
    }
}

void exAsyncVectorPV::removeWriteIO ()
{
    if ( this->simultAsychWriteIOCount > 0u ) {
        this->simultAsychWriteIOCount--;
        if ( this->simultAsychWriteIOCount == 0 && 
            pStandbyValue.valid () ) {
//printf("removeIO: write standby\n");
            this->update ( *this->pStandbyValue );
            this->pStandbyValue.set ( 0 );
        }
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychWriteIOCount?\n" );
    }
}

//template class exAsyncReadIO<exAsyncVectorPV>;
//template class exAsyncWriteIO<exAsyncVectorPV>;
