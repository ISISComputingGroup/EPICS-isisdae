#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"

NoAlarmPV::NoAlarmPV ( exServer & cas, pvInfo &setup, bool preCreateFlag, bool scanOnIn) : FixedValuePV<aitEnum16>(cas, setup, preCreateFlag, scanOnIn, 0)
{
    assert(this->info.getType () == aitEnumEnum16);
}

caStatus NoAlarmPV::getEnumsImpl ( gdd & enumsIn )
{
        static const unsigned nStr = 1;
        aitFixedString *str;
        exFixedStringDestructor *pDes;

        str = new aitFixedString[nStr];
        if (!str) {
            return S_casApp_noMemory;
        }

        pDes = new exFixedStringDestructor;
        if (!pDes) {
            delete [] str;
            return S_casApp_noMemory;
        }

        strncpy (str[0].fixed_string, "NO_ALARM", 
            sizeof(str[0].fixed_string));

        enumsIn.setDimension(1);
        enumsIn.setBound (0,0,nStr);
        enumsIn.putRef (str, pDes);

        return S_cas_success;
}

