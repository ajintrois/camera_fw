#********************************************************************************************/
#*		Project	   	:	Jetson ORIN Camera/NVR			       */
#*		Author/Modified By 	:	Maheen Rasheed				       */
#*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
#********************************************************************************************/
# LIVE_DIR = ../live_DM1
LIVE_DIR = /home/trois/FWCV-248.10-12.11.0/third_party/live

################################################################
###### Start of Version ########################################
# The name of the header file we will generate
# 1. Define the version file names at the top
VERSION_H := version.h
VERSION_TXT := version.txt
# The app version (major.minor)
VERSION_MAJOR = 2
VERSION_MINOR = 1

# Versioning logic files
ALL_FILES    = $(wildcard *.c *.cpp *.h *.py)
HASH_TARGETS = $(filter-out $(VERSION_H) $(VERSION_TXT) .source_hash .new_hash, $(ALL_FILES))

#-----------------------------------------------------------
INCLUDES = -I$(LIVE_DIR)/UsageEnvironment/include -I$(LIVE_DIR)/groupsock/include -I$(LIVE_DIR)/liveMedia/include -I$(LIVE_DIR)/BasicUsageEnvironment/include -I/usr/include/opencv4
# Default library filename suffixes for each library that we link with.  The "config.*" file might redefine these later.
libliveMedia_LIB_SUFFIX = $(LIB_SUFFIX)
libBasicUsageEnvironment_LIB_SUFFIX = $(LIB_SUFFIX)
libUsageEnvironment_LIB_SUFFIX = $(LIB_SUFFIX)
libgroupsock_LIB_SUFFIX = $(LIB_SUFFIX)

##### Change the following for your environment:
CROSS_COMPILE?=	
COMPILE_OPTS =		$(INCLUDES) -I. -O2 -DSOCKLEN_T=socklen_t -DNO_SSTREAM=1 -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -DSO_REUSEPORT=1 -DREUSE_FOR_TCP=1 -D_FORTIFY_SOURCE=2 
C =			c
C_COMPILER =		gcc
C_FLAGS =		$(COMPILE_OPTS)
CPP =			cpp
CPLUSPLUS_COMPILER =	g++
CPLUSPLUS_FLAGS =	$(COMPILE_OPTS) -Wall -DBSD=1 
OBJ =			o
LINK =			g++ -o 
LINK_OPTS =		-L. -s
CONSOLE_LINK_OPTS =	$(LINK_OPTS) 
LIBRARY_LINK =		ld -o
LIBRARY_LINK_OPTS =	$(LINK_OPTS) -r -Bstatic
LIB_SUFFIX =		a
LIBS_FOR_CONSOLE_APPLICATION =
LIBS_FOR_GUI_APPLICATION =
EXE =
##### End of variables to change

USAGE_ENVIRONMENT_DIR = $(LIVE_DIR)/UsageEnvironment
USAGE_ENVIRONMENT_LIB = $(USAGE_ENVIRONMENT_DIR)/libUsageEnvironment.$(libUsageEnvironment_LIB_SUFFIX)
BASIC_USAGE_ENVIRONMENT_DIR = $(LIVE_DIR)/BasicUsageEnvironment
BASIC_USAGE_ENVIRONMENT_LIB = $(BASIC_USAGE_ENVIRONMENT_DIR)/libBasicUsageEnvironment.$(libBasicUsageEnvironment_LIB_SUFFIX)
LIVEMEDIA_DIR = $(LIVE_DIR)/liveMedia
LIVEMEDIA_LIB = $(LIVEMEDIA_DIR)/libliveMedia.$(libliveMedia_LIB_SUFFIX)
GROUPSOCK_DIR = $(LIVE_DIR)/groupsock
GROUPSOCK_LIB = $(GROUPSOCK_DIR)/libgroupsock.$(libgroupsock_LIB_SUFFIX)
LOCAL_LIBS =	$(LIVEMEDIA_LIB) $(GROUPSOCK_LIB) \
		$(BASIC_USAGE_ENVIRONMENT_LIB) $(USAGE_ENVIRONMENT_LIB)
LIBS =		$(LOCAL_LIBS) $(LIBS_FOR_CONSOLE_APPLICATION) -lssl -lcrypto -lrt -lopencv_core -lopencv_imgproc -lopencv_imgcodecs


MTX_APPS = H264RTSP$(EXE)
H264_RTSP_OBJS = MtxH264Streamer.$(OBJ) MtxH264VideoLiveFramedSource.$(OBJ) MtxH264VideoLiveServerMediaSubsession.$(OBJ) SharedMemory.$(OBJ)

# 1. Tell Make to ALWAYS run the version generation logic
.PHONY: all clean build_revision 

all: build_revision $(MTX_APPS)

# @echo "<<<< Cleaning Files>>>>"
# rm -f *.$(OBJ) $(MTX_APPS) *~ 

$(MTX_APPS): $(VERSION_H)
	@echo "LIBRARY_LINK_OPTS: $(LIBRARY_LINK_OPTS)"
	@echo "CPLUSPLUS_FLAGS: $(CPLUSPLUS_FLAGS)"
	@echo "LIBS: $(LIBS)"

	@echo "<<<< Starting Compilation >>>>"
	$(CPLUSPLUS_COMPILER) -c *.cpp $(CPLUSPLUS_FLAGS) $<
	$(LINK) $(MTX_APPS) $(CONSOLE_LINK_OPTS) $(H264_RTSP_OBJS) $(LIBS)
	@echo Copying H264-RTSPClient 
	@cp $(MTX_APPS) ../../build
	@cp version.txt ../../build/$(MTX_APPS)-version.txt
	
	@sync	
clean:
	@echo "<<<<< Cleaning Files >>>>"
	rm -f *.$(OBJ) $(MTX_APPS) $(VERSION_H) *~ 

##### Any additional, platform-specific rules come here:

# 4. Version generation logic
build_revision:
	@md5sum $(HASH_TARGETS) | md5sum > .new_hash
	@if [ ! -f $(VERSION_TXT) ] || [ ! -f $(VERSION_H) ] || ! cmp -s .new_hash .source_hash; then \
		echo "Version files missing or changes detected. Generating version..."; \
		read -r ma mi bu < $(VERSION_TXT) 2>/dev/null || { ma=1; mi=0; bu=0; }; \
		if [ -f $(VERSION_TXT) ] && [ -f $(VERSION_H) ]; then \
			bu=$$((bu + 1)); \
			if [ $$bu -gt 999 ]; then bu=0; mi=$$((mi + 1)); fi; \
			if [ $$mi -gt 999 ]; then mi=0; ma=$$((ma + 1)); fi; \
		fi; \
		echo "$$ma $$mi $$bu" > $(VERSION_TXT); \
		echo "// Generated file. Do not edit." > $(VERSION_H); \
		echo "#define APP_VERSION \"$$ma.$$mi.$$bu\"" >> $(VERSION_H); \
		echo "#define BUILD_TIMESTAMP \"`date '+%A %d %B %Y %I:%M:%S %p %Z'`\"" >> $(VERSION_H); \
		mv .new_hash .source_hash; \
		echo "App Version: $$ma.$$mi.$$bu"; \
	else \
		echo "No changes detected. Current Version: $$(awk '{print $$1"."$$2"."$$3}' $(VERSION_TXT))"; \
		rm -f .new_hash; \
	fi
####### End of version ########################################
###############################################################
