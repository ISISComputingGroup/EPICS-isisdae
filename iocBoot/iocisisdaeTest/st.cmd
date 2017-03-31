#!../../bin/windows-x64-debug/isisdaeTest

## You may have to change isisdaeTest to something else
## everywhere it appears in this file

< envPaths

epicsEnvSet "WIRING_DIR" "$(ICPCONFIGROOT)/tables"
epicsEnvSet "WIRING_PATTERN" ".*wiring.*"
epicsEnvSet "DETECTOR_DIR" "$(ICPCONFIGROOT)/tables"
epicsEnvSet "DETECTOR_PATTERN" ".*det.*"
epicsEnvSet "SPECTRA_DIR" "$(ICPCONFIGROOT)/tables"
epicsEnvSet "SPECTRA_PATTERN" ".*spec.*"
epicsEnvSet "PERIOD_DIR" "$(ICPCONFIGROOT)/tables"
epicsEnvSet "PERIOD_PATTERN" ".*period.*"
epicsEnvSet "TCB_DIR" "$(ICPCONFIGROOT)/tcb"
epicsEnvSet "TCB_PATTERN" ".*tcb.*"

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/isisdaeTest.dbd"
isisdaeTest_registerRecordDeviceDriver pdbbase

##ISIS## Run IOC initialisation 
< $(IOCSTARTUP)/init.cmd

## used for restarting EPICS archiver via web URL
webgetConfigure("web")

## local dae, no dcom/labview
isisdaeConfigure("icp", 1)
## pass 1 as second arg to signify DCOM to either local or remote dae
#isisdaeConfigure("icp", 1, "localhost")
#isisdaeConfigure("icp", 1, "ndxchipir", "spudulike", "reliablebeam")

## Load the FileLists
FileListConfigure("WLIST", "$(WIRING_DIR)", "$(WIRING_PATTERN)", 1)
FileListConfigure("DLIST", "$(DETECTOR_DIR)", "$(DETECTOR_PATTERN)", 1)
FileListConfigure("SLIST", "$(SPECTRA_DIR)", "$(SPECTRA_PATTERN)", 1)
FileListConfigure("PLIST", "$(PERIOD_DIR)", "$(PERIOD_PATTERN)", 1)
FileListConfigure("TLIST", "$(TCB_DIR)", "$(TCB_PATTERN)", 1)

## Load record instances

##ISIS## Load common DB records 
< $(IOCSTARTUP)/dbload.cmd

dbLoadRecords("$(TOP)/db/isisdae.db","S=$(MYPVPREFIX), P=$(MYPVPREFIX),Q=DAE:, WIRINGLIST=WLIST, DETECTORLIST=DLIST, SPECTRALIST=SLIST, PERIODLIST=PLIST, TCBLIST=TLIST")
dbLoadRecords("$(TOP)/db/dae_diag.db","P=$(MYPVPREFIX),Q=DAE:")

##ISIS## Stuff that needs to be done after all records are loaded but before iocInit is called 
< $(IOCSTARTUP)/preiocinit.cmd

cd ${TOP}/iocBoot/${IOC}
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

##ISIS## Stuff that needs to be done after iocInit is called e.g. sequence programs 
< $(IOCSTARTUP)/postiocinit.cmd
