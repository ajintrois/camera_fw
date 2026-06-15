###############################################################################
#
# Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################
#********************************************************************************************/
#*		Project	   	:	Jetson ORIN Camera/NVR			       */
#*		Author/Modified By 	:	Maheen Rasheed				       */
#*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
#********************************************************************************************/

include Rules.mk

APP := jpeg_decode

SRCS := \
	jpeg_decode_csvparser.cpp \
	jpeg_decode_main.cpp \
	$(wildcard $(CLASS_DIR)/*.cpp)

OBJS := $(SRCS:.cpp=.o)

all: 
	@echo $(OPENCV_MSG)
	@$(MAKE) $(APP) --no-print-directory

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $< -o $@
# $(CPP) $(CPPFLAGS) -c $< -O2 -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath,

# @echo "version: $(shell cat version.txt)" >> ../../../flash/jpeg_decode.version
$(APP): $(OBJS)
	@echo "Linking: $@"
	$(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS) -lm -lrt
	@echo "Hardening: Stripping symbols..."
	strip --strip-unneeded $(APP)
	objcopy --strip-unneeded $(APP)
	@cp $(APP) ../../build/.
	@# Generate version file
	@echo "name: jpeg_decode" > ../../build/jpeg_decode.version
	@echo -n "version: " >> ../../build/$(APP).version
	@./$(APP) --version | awk '{print $$2}' >> ../../build/$(APP).version
	@echo "vendor: Mediatronix" >> ../../build/jpeg_decode.version
	@echo "description: Mediatronix JPEG -> YUV Image Data Decoder" >> ../../build/jpeg_decode.version	
clean:
	@echo "Cleaning project..."
	@if [ -f $(APP) ]; then rm $(APP); else echo cleaned; fi

