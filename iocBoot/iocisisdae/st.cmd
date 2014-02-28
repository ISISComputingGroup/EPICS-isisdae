#!../../bin/windows-x64/isisdae

## You may have to change isisdae to something else
## everywhere it appears in this file

< envPaths

epicsEnvSet "IOCNAME" "${IOC}"

epicsEnvSet "IOCSTATS_DB" "$(DEVIOCSTATS)/db/iocAdminSoft.db"

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/isisdae.dbd"
isisdae_registerRecordDeviceDriver pdbbase

## local dae, no dcom/labview
isisdaeConfigure("icp")
## pass 1 as second arg to signify DCOM to either local or remote dae
#isisdaeConfigure("icp", 1, "localhost")
#isisdaeConfigure("icp", 1, "ndxchipir", "spudulike", "reliablebeam")

## Load record instances
dbLoadRecords("$(TOP)/db/isisdae.db","P=$(MYPVPREFIX)DAE")
dbLoadRecords("$(IOCSTATS_DB)","IOC=$(MYPVPREFIX)DAE")

cd ${TOP}/iocBoot/${IOC}
iocInit

