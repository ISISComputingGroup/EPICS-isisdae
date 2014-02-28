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

LIBRARY_IOC += isisdaeSupport
isisdaeSupport_SRCS += convertToString.cpp variant_utils.cpp
isisdaeSupport_SRCS += exAsyncPV.cc exChannel.cc exPV.cc exScalarPV.cc exServer.cc exVectorPV.cc exAsyncVectorPV.cc
isisdaeSupport_SRCS += isisdaeDriver.cpp isisdaeInterface.cpp 
isisdaeSupport_LIBS += asyn oncrpc utilities zlib pcrecpp pcre
isisdaeSupport_LIBS += cas gdd
isisdaeSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

isisicpint_DIR = ../lib/$(EPICS_HOST_ARCH) 
isisdaeSupport_LIBS_WIN32 += isisicpint

#=============================
# Build the IOC application

PROD_IOC = isisdae
# isisdae.dbd will be created and installed
# see isisdaeInclude.dbd
DBD += isisdae.dbd

# Add all the support libraries needed by this IOC
## ISIS standard libraries ##
isisdae_LIBS += seqDev seq pv
isisdae_LIBS += devIocStats 
isisdae_LIBS += pvdump easySQLite sqlite 
isisdae_LIBS += caPutLog
isisdae_LIBS += icpconfig
isisdae_LIBS += autosave
isisdae_LIBS += utilities
isisdae_LIBS += isisdaeSupport asyn oncrpc zlib pcrecpp pcre
isisdae_LIBS += cas gdd

# isisdae_registerRecordDeviceDriver.cpp derives from isisdae.dbd
isisdae_SRCS += isisdae_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
isisdae_SRCS_DEFAULT += isisdaeMain.cpp
isisdae_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#isisdae_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#isisdae_SYS_LIBS_WIN32 += msxml2
isisdae_SYS_LIBS_WIN32 += ws2_32 # advapi32 user32

# Finally link to the EPICS Base libraries
isisdae_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

