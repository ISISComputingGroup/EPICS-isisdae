TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#ICPROOT := c:/development/InstrumentControl/ICP/labview modules/dae

#ifeq ($(findstring debug,$(EPICS_HOST_ARCH)),)
#ICPCONFIG := Release
#endif
#ifneq ($(findstring debug,$(EPICS_HOST_ARCH)),)
#ICPCONFIG := Debug
#endif
#ICPSERVICE := $(ICPROOT)/service/x64/$(ICPCONFIG)

#BIN_INSTALLS_WIN32 += isisicpint.dll 

#isisicpint.dll : 
#	@(CP) $(ICPSERVICE)/isisicpint.dll $<

#	$(wildcard lib/*.dll)
#BIN_INSTALLS_WIN32 += $(ICPSERVICE)/isisicpint.dll

# Build the IOC support library

USR_INCLUDES += -I"$(TOP)/../../libraries/boost/include" -I"$(AREA_DETECTOR)"

## We use -EHa so windows structured exceptions are catachable with C++ catch(...) 
USR_CXXFLAGS += -EHa -DNOMINMAX

LIBRARY_IOC_WIN32 += isisdae
isisdae_SRCS += convertToString.cpp variant_utils.cpp
isisdae_SRCS += SpectrumPV.cc NORDPV.cc MonLookupPV.cc CountsPV.cc FixedValuePV.cc
isisdae_SRCS += exChannel.cc exPV.cc exScalarPV.cc exVectorPV.cc exServer.cc
isisdae_SRCS += isisdaeDriver.cpp isisdaeInterface.cpp CRPTMapping.cpp
isisdae_SRCS += MonitorSpectrumPV.cpp MonitorCountsPV.cpp dae_isisicpint_dummy.cpp
isisdae_SRCS += NoAlarmPV.cpp isisdaePvaServices.cpp

isisdae_LIBS += webget tidy asyn oncrpc utilities zlib pcrecpp pcre pugixml
isisdae_LIBS += ADnEDSupport
isisdae_LIBS += cas gdd

## ffmpegserver
#isisdae_LIBS += ffmpegServer
#isisdae_LIBS += avdevice
#isisdae_LIBS += avformat
#isisdae_LIBS += avcodec
#isisdae_LIBS += avutil
#isisdae_LIBS += swscale

isisdae_LIBS += pvxsIoc pvxs

isisdae_LIBS += $(EPICS_BASE_IOC_LIBS)

#ifneq ($(findstring debug,$(EPICS_HOST_ARCH)),)
#isisicpint_DIR = ../lib/windows-x64-debug
#else
#isisicpint_DIR = ../lib/windows-x64
#endif
#isisdae_LIBS_WIN32 += isisicpint
isisdae_SYS_LIBS_WIN32 += psapi

# isisdae.dbd will be created and installed
# see isisdaeInclude.dbd
DBD += isisdae.dbd

DATA += xml/cluster.xml xml/dae_settings_cluster.xml xml/hardware_periods_cluster.xml xml/monitoring_settings_cluster.xml
DATA += xml/run_data_cluster.xml xml/tcb_settings_cluster.xml xml/update_settings_cluster.xml

include $(ADCORE)/ADApp/commonLibraryMakefile

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
