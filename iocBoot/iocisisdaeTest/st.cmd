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

# Prefix for all records
#epicsEnvSet("PREFIX", "$(MYPVPREFIX)")
# The port name for the detector
#epicsEnvSet("PORT",   "icp")
# The queue size for all plugins
#epicsEnvSet("QSIZE",  "20")
# The maximim image width; used to set the maximum size for this driver and for row profiles in the NDPluginStats plugin
#epicsEnvSet("XSIZE",  "10")
# The maximim image height; used to set the maximum size for this driver and for column profiles in the NDPluginStats plugin
#epicsEnvSet("YSIZE",  "10")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

asynSetMinTimerPeriod(0.001)

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

dbLoadRecords("$(TOP)/db/ADisisdae.template","P=$(MYPVPREFIX),R=DAE:,PORT=icp,ADDR=0,TIMEOUT=1")

NDStdArraysConfigure("Image1", 3, 0, "icp", 0, 0)

# This waveform allows transporting 8-bit images
# needs to fit in EPICS_CA_MAX_ARRAY_BYTES
dbLoadRecords("NDStdArrays.template", "P=$(MYPVPREFIX),R=DAE:image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icp,TYPE=Int8,FTVL=UCHAR,NELEMENTS=150000,ENABLED=1")
# This waveform allows transporting 32-bit images
#dbLoadRecords("NDStdArrays.template", "P=$(MYPVPREFIX),R=DAE:image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icp,TYPE=Int32,FTVL=LONG,NELEMENTS=12000000,ENABLED=1")

## Create an FFT plugin
#NDFFTConfigure("FFT1", 3, 0, "icp", 0)
#dbLoadRecords("NDFFT.template", "P=$(PREFIX),R=DAE:FFT1:,PORT=FFT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NAME=FFT1,NCHANS=2048")

ffmpegServerConfigure(8081)
## ffmpegStreamConfigure(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, maxMemory)
ffmpegStreamConfigure("C1.MJPG", 2, 0, "icp", "0")
dbLoadRecords("$(FFMPEGSERVER)/db/ffmpegStream.template", "P=$(MYPVPREFIX),R=DAE:Stream:,PORT=C1.MJPG,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icp,ENABLED=1")

## ffmpegFileConfigure(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr)
ffmpegFileConfigure("C1.FILE", 16, 0, "icp", 0)
dbLoadRecords("$(FFMPEGSERVER)/db/ffmpegFile.template", "P=$(MYPVPREFIX),R=DAE:File:,PORT=C1.FILE,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icp,ENABLED=1")

NDPvaConfigure("PVA", 3, 0, "icp", 0, "v4pvname")
dbLoadRecords("NDPva.template", "P=$(MYPVPREFIX),R=DAE:V4:,PORT=PVA,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icp,ENABLED=1")

##ISIS## Stuff that needs to be done after all records are loaded but before iocInit is called 
< $(IOCSTARTUP)/preiocinit.cmd

## 0=none,0x1=err,0x2=IO_device,0x4=IO_filter,0x8=IO_driver,0x10=flow,0x20=warning
#asynSetTraceMask("icp", -1, 0x11)
#asynSetTraceMask("Image1", -1, 0x11)

cd ${TOP}/iocBoot/${IOC}
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

##ISIS## Stuff that needs to be done after iocInit is called e.g. sequence programs 
< $(IOCSTARTUP)/postiocinit.cmd
