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

USR_INCLUDES += -I"$(TOP)/../../libraries/boost/include"
USR_CXXFLAGS += /EHa /DNOMINMAX

LIBRARY_IOC += isisdae
isisdae_SRCS += convertToString.cpp variant_utils.cpp
isisdae_SRCS += exAsyncPV.cc exChannel.cc exPV.cc exScalarPV.cc exServer.cc exVectorPV.cc exAsyncVectorPV.cc
isisdae_SRCS += isisdaeDriver.cpp isisdaeInterface.cpp 

isisdae_LIBS += webget htmltidy asyn oncrpc utilities zlib pcrecpp pcre
isisdae_LIBS += cas gdd
isisdae_LIBS += $(EPICS_BASE_IOC_LIBS)

isisicpint_DIR = ../lib/$(EPICS_HOST_ARCH) 
isisdae_LIBS_WIN32 += isisicpint

# isisdae.dbd will be created and installed
# see isisdaeInclude.dbd
DBD += isisdae.dbd

DATA += xml/cluster.xml xml/dae_settings_cluster.xml xml/hardware_periods_cluster.xml xml/monitoring_settings_cluster.xml
DATA += xml/run_data_cluster.xml xml/tcb_settings_cluster.xml xml/update_settings_cluster.xml


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

