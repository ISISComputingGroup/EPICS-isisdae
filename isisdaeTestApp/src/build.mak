TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

### NOTE: there should only be one build.mak for a given IOC family and this should be located in the ###-IOC-01 directory

#=============================
# Build the IOC application isisdaeTest
# We actually use $(APPNAME) below so this file can be included by multiple IOCs

PROD_IOC_WIN32 = $(APPNAME)
# isisdaeTest.dbd will be created and installed
DBD += $(APPNAME).dbd

PROD_NAME = $(APPNAME)
include $(ADCORE)/ADApp/commonDriverMakefile

# isisdaeTest.dbd will be made up from these files:
# we get base, asyn + areadetetor standard plugins as part of commonDriverMakefile include
## ISIS standard dbd ##
$(APPNAME)_DBD += icpconfig.dbd
$(APPNAME)_DBD += pvdump.dbd
$(APPNAME)_DBD += caPutLog.dbd
$(APPNAME)_DBD += utilities.dbd
$(APPNAME)_DBD += asubFunctions.dbd 
## add other dbd here ##
$(APPNAME)_DBD += isisdae.dbd
$(APPNAME)_DBD += webget.dbd
$(APPNAME)_DBD += FileList.dbd
$(APPNAME)_DBD += ADnEDSupport.dbd
#$(APPNAME)_DBD += ffmpegServer.dbd
$(APPNAME)_DBD += pvxsIoc.dbd

# Add all the support libraries needed by this IOC
## ISIS standard libraries ##
$(APPNAME)_LIBS += webget tidy
$(APPNAME)_LIBS += seq pv
$(APPNAME)_LIBS += devIocStats 
$(APPNAME)_LIBS += pvdump $(MYSQLLIB) easySQLite sqlite 
$(APPNAME)_LIBS += caPutLog
$(APPNAME)_LIBS += icpconfig
$(APPNAME)_LIBS += autosave
$(APPNAME)_LIBS += utilities
$(APPNAME)_LIBS += asubFunctions
## Add other libraries here ##
#$(APPNAME)_LIBS += xxx
$(APPNAME)_LIBS += isisdae FileList asyn oncrpc zlib efsw libjson pcrecpp pcre pugixml
$(APPNAME)_LIBS += pvxsIoc pvxs
$(APPNAME)_LIBS += cas gdd 

## ffmpegserver
#$(APPNAME)_LIBS += ffmpegServer
#$(APPNAME)_LIBS += avdevice
#$(APPNAME)_LIBS += avformat
#$(APPNAME)_LIBS += avcodec
#$(APPNAME)_LIBS += avutil
#$(APPNAME)_LIBS += swscale

$(APPNAME)_LIBS += ADnEDSupport
$(APPNAME)_LIBS += ADnEDTransform

$(APPNAME)_LIBS_WIN32 += libcurl
$(APPNAME)_SYS_LIBS_Linux += curl

$(APPNAME)_LIBS += libssl libcrypto

#ifneq ($(findstring debug,$(EPICS_HOST_ARCH)),)
#isisicpint_DIR = $(TOP)/isisdaeApp/src/lib/windows-x64-debug
#else
#isisicpint_DIR = $(TOP)/isisdaeApp/src/lib/windows-x64
#endif
#$(APPNAME)_LIBS_WIN32 += isisicpint

# isisdaeTest_registerRecordDeviceDriver.cpp derives from isisdaeTest.dbd
$(APPNAME)_SRCS += $(APPNAME)_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
$(APPNAME)_SRCS_DEFAULT += $(APPNAME)Main.cpp
$(APPNAME)_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#$(APPNAME)_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

$(APPNAME)_SYS_LIBS_WIN32 += psapi wldap32 ws2_32 crypt32 Normaliz # advapi32 user32 msxml2

# Finally link to the EPICS Base libraries
$(APPNAME)_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
#=============================

