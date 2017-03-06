#!../../bin/windows-x64-debug/isisdaeTest

## You may have to change isisdaeTest to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/isisdaeTest.dbd"
isisdaeTest_registerRecordDeviceDriver pdbbase

##ISIS## Run IOC initialisation 
< $(IOCSTARTUP)/init.cmd

# Prefix for all records
#epicsEnvSet("PREFIX", "$(MYPVPREFIX)")
# The port name for the detector
#epicsEnvSet("PORT",   "icpad")
# The queue size for all plugins
#epicsEnvSet("QSIZE",  "20")
# The maximim image width; used to set the maximum size for this driver and for row profiles in the NDPluginStats plugin
#epicsEnvSet("XSIZE",  "10")
# The maximim image height; used to set the maximum size for this driver and for column profiles in the NDPluginStats plugin
#epicsEnvSet("YSIZE",  "10")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

asynSetMinTimerPeriod(0.001)

## local dae, no dcom/labview
#isisdaeConfigure("icp")
## pass 1 as second arg to signify DCOM to either local or remote dae
#isisdaeConfigure("icp", 1, "localhost")
#isisdaeConfigure("icp", 1, "ndxchipir", "spudulike", "reliablebeam")

## make sure xand y sizes are each a multiple of 16
isisdaeADConfigure("icpad", 16, 16, 1, 0, 0)

## Load record instances

##ISIS## Load common DB records 
< $(IOCSTARTUP)/dbload.cmd

#dbLoadRecords("$(TOP)/db/isisdae.db","P=$(MYPVPREFIX),Q=DAE:")

dbLoadRecords("$(ADSIMDETECTOR)/db/simDetector.template","P=$(MYPVPREFIX),R=cam1:,PORT=icpad,ADDR=0,TIMEOUT=1")

NDStdArraysConfigure("Image1", 3, 0, "icpad", 0, 0)

# This waveform allows transporting 8-bit images
dbLoadRecords("NDStdArrays.template", "P=$(MYPVPREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icpad,TYPE=Int8,FTVL=UCHAR,NELEMENTS=256,ENABLED=1")
# This waveform allows transporting 32-bit images
#dbLoadRecords("NDStdArrays.template", "P=$(MYPVPREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icpad,TYPE=Int32,FTVL=LONG,NELEMENTS=12000000,ENABLED=1")

## Create an FFT plugin
#NDFFTConfigure("FFT1", 3, 0, "icpad", 0)
#dbLoadRecords("NDFFT.template", "P=$(PREFIX),R=FFT1:,PORT=FFT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NAME=FFT1,NCHANS=2048")

ffmpegServerConfigure(8081)
## ffmpegStreamConfigure(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, maxMemory)
ffmpegStreamConfigure("C1.MJPG", 2, 0, "icpad", "0")
dbLoadRecords("$(FFMPEGSERVER)/db/ffmpegStream.template", "P=FFMPEG:,R=Stream:,PORT=C1.MJPG,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icpad,ENABLED=1")

## ffmpegFileConfigure(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr)
ffmpegFileConfigure("C1.FILE", 16, 0, "icpad", 0)
dbLoadRecords("$(FFMPEGSERVER)/db/ffmpegFile.template", "P=FFMPEG:,R=File:,PORT=C1.FILE,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icpad,ENABLED=1")

NDPvaConfigure("PVA", 3, 0, "icpad", 0, "v4pvname")
dbLoadRecords("NDPva.template", "P=$(MYPVPREFIX),R=V4:,PORT=PVA,ADDR=0,TIMEOUT=1,NDARRAY_PORT=icpad,ENABLED=1")

##ISIS## Stuff that needs to be done after all records are loaded but before iocInit is called 
< $(IOCSTARTUP)/preiocinit.cmd

## 0=none,0x1=err,0x2=IO_device,0x4=IO_filter,0x8=IO_driver,0x10=flow,0x20=warning
#asynSetTraceMask("icpad", -1, 0x11)
#asynSetTraceMask("Image1", -1, 0x11)

cd ${TOP}/iocBoot/${IOC}
iocInit

## Start any sequence programs
#seq sncxxx,"user=faa59Host"

##ISIS## Stuff that needs to be done after iocInit is called e.g. sequence programs 
< $(IOCSTARTUP)/postiocinit.cmd


